/*
 * 文件名: datasinglesheet.cpp
 * 文件作用: 单个数据表页签类实现文件
 * 功能描述:
 * 1. 管理数据表格的核心逻辑，包括界面初始化、模型(Model)设置。
 * 2. 实现多种格式数据的加载功能 (loadExcelFile / loadTextFile)。
 * 3. 实现表格的交互功能：右键菜单操作、Ctrl+滚轮缩放。
 * 4. 集成数据处理与计算接口：时间转换、压降计算、井底流压计算等。
 * 5. 实现数据的导出 (Excel) 和序列化保存 (JSON)。
 * 6. [新增] 禁用点击表头排序功能，防止数据序列被打乱。
 * 7. [新增] 强制使用常规数字表示法，杜绝科学计数法在界面中的显示。
 */

#include "datasinglesheet.h"
#include "ui_datasinglesheet.h"
#include "datacalculate.h"
#include "dataimportdialog.h"
#include "settingpaths.h"

// 引入 QXlsx 头文件
#include "xlsxdocument.h"
#include "xlsxchartsheet.h"
#include "xlsxcellrange.h"
#include "xlsxformat.h"

#include <QFileDialog>
#include "standard_messagebox.h"
#include <QTextStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QTextCodec>
#include <QLineEdit>
#include <QEvent>
#include <QAxObject>
#include <QDir>
#include <QDateTime>
#include <QRadioButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QWheelEvent>
#include <QHeaderView> // 引入表头控制
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>

// ============================================================================
// [辅助函数] 强制应用“灰底黑字”的按钮样式
// ============================================================================
static void applySheetDialogStyle(QWidget* dialog) {
    if (!dialog) return;
    QString qss = "QWidget { color: black; background-color: white; font-family: 'Microsoft YaHei'; }"
                  "QPushButton { "
                  "   background-color: #f0f0f0; "
                  "   color: black; "
                  "   border: 1px solid #bfbfbf; "
                  "   border-radius: 3px; "
                  "   padding: 5px 15px; "
                  "   min-width: 70px; "
                  "}"
                  "QPushButton:hover { background-color: #e0e0e0; }"
                  "QPushButton:pressed { background-color: #d0d0d0; }"
                  "QLabel { color: black; }"
                  "QLineEdit { color: black; background-color: white; border: 1px solid #ccc; }"
                  "QGroupBox { color: black; border: 1px solid #ccc; margin-top: 20px; }"
                  "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 3px; }";
    dialog->setStyleSheet(qss);
}

// [辅助函数] 显示带统一样式的消息提示框
static void showStyledMessage(QWidget* parent, Standard_MessageBox::MsgType type, const QString& title, const QString& text) {
    switch (type) {
    case Standard_MessageBox::Info:
        Standard_MessageBox::info(parent, title, text);
        break;
    case Standard_MessageBox::Warning:
        Standard_MessageBox::warning(parent, title, text);
        break;
    case Standard_MessageBox::Error:
        Standard_MessageBox::error(parent, title, text);
        break;
    default:
        Standard_MessageBox::info(parent, title, text);
        break;
    }
}

// ============================================================================
// [辅助函数] 数值格式化：强制取消科学计数法，转为常规表示并去除多余尾数 0
// ============================================================================
static QString formatStringIfNumber(const QString& str) {
    bool ok;
    double val = str.toDouble(&ok);
    // 如果可以转换为数字，并且不是空字符
    if (ok && !str.trimmed().isEmpty()) {
        // 使用 'f' 强制为常规浮点数，保留最高 8 位小数避免科学计数法
        QString res = QString::number(val, 'f', 8);
        if (res.contains('.')) {
            while (res.endsWith('0')) res.chop(1); // 砍掉尾部多余的 0
            if (res.endsWith('.')) res.chop(1);    // 如果最后是小数点也砍掉
        }
        return res;
    }
    return str; // 如果不是纯数字，则原样返回
}

// ============================================================================
// [内部类] InternalSplitDialog
// ============================================================================
class InternalSplitDialog : public QDialog
{
public:
    explicit InternalSplitDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("数据分列");
        resize(300, 200);
        applySheetDialogStyle(this);

        QVBoxLayout* layout = new QVBoxLayout(this);
        QGroupBox* group = new QGroupBox("选择分隔符");
        QVBoxLayout* gLayout = new QVBoxLayout(group);

        btnGroup = new QButtonGroup(this);

        radioSpace = new QRadioButton("空格 (Space)"); radioSpace->setChecked(true);
        radioTab = new QRadioButton("制表符 (Tab)");
        radioT = new QRadioButton("字母 'T' (日期时间)");
        radioCustom = new QRadioButton("自定义:");
        editCustom = new QLineEdit(); editCustom->setEnabled(false);

        btnGroup->addButton(radioSpace);
        btnGroup->addButton(radioTab);
        btnGroup->addButton(radioT);
        btnGroup->addButton(radioCustom);

        gLayout->addWidget(radioSpace);
        gLayout->addWidget(radioTab);
        gLayout->addWidget(radioT);

        QHBoxLayout* hLayout = new QHBoxLayout;
        hLayout->addWidget(radioCustom);
        hLayout->addWidget(editCustom);
        gLayout->addLayout(hLayout);

        layout->addWidget(group);

        QHBoxLayout* btnLayout = new QHBoxLayout;
        QPushButton* btnOk = new QPushButton("确定");
        QPushButton* btnCancel = new QPushButton("取消");
        btnLayout->addStretch();
        btnLayout->addWidget(btnOk);
        btnLayout->addWidget(btnCancel);
        layout->addLayout(btnLayout);

        connect(radioCustom, &QRadioButton::toggled, editCustom, &QLineEdit::setEnabled);
        connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    QString getSeparator() const {
        if (radioSpace->isChecked()) return " ";
        if (radioTab->isChecked()) return "\t";
        if (radioT->isChecked()) return "T";
        if (radioCustom->isChecked()) return editCustom->text();
        return " ";
    }

private:
    QButtonGroup* btnGroup;
    QRadioButton *radioSpace, *radioTab, *radioT, *radioCustom;
    QLineEdit *editCustom;
};

// ============================================================================
// [内部类] NoContextMenuDelegate & EditorEventFilter
// ============================================================================
class EditorEventFilter : public QObject {
public:
    EditorEventFilter(QObject *parent) : QObject(parent) {}
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::ContextMenu) return true;
        return QObject::eventFilter(obj, event);
    }
};

class CellEditCommand : public QUndoCommand
{
public:
    CellEditCommand(QStandardItemModel* model, int row, int column, const QString& oldValue, const QString& newValue, bool* guard, QUndoCommand* parent = nullptr)
        : QUndoCommand(parent), m_model(model), m_row(row), m_column(column), m_oldValue(oldValue), m_newValue(newValue), m_guard(guard) {}

    void undo() override {
        if (!m_model) return;
        *m_guard = true;
        QStandardItem* item = m_model->item(m_row, m_column);
        if (!item) {
            item = new QStandardItem;
            m_model->setItem(m_row, m_column, item);
        }
        item->setText(m_oldValue);
        item->setData(m_oldValue, Qt::UserRole + 1);
        *m_guard = false;
    }

    void redo() override {
        if (!m_model) return;
        *m_guard = true;
        QStandardItem* item = m_model->item(m_row, m_column);
        if (!item) {
            item = new QStandardItem;
            m_model->setItem(m_row, m_column, item);
        }
        item->setText(m_newValue);
        item->setData(m_newValue, Qt::UserRole + 1);
        *m_guard = false;
    }

private:
    QStandardItemModel* m_model;
    int m_row;
    int m_column;
    QString m_oldValue;
    QString m_newValue;
    bool* m_guard;
};

class InsertRowCommand : public QUndoCommand
{
public:
    InsertRowCommand(QStandardItemModel* model, int row, int columnCount, bool* guard, QUndoCommand* parent = nullptr)
        : QUndoCommand(parent), m_model(model), m_row(row), m_columnCount(columnCount), m_guard(guard) {}

    void undo() override {
        if (!m_model) return;
        *m_guard = true;
        m_model->removeRow(m_row);
        *m_guard = false;
    }

    void redo() override {
        if (!m_model) return;
        *m_guard = true;
        QList<QStandardItem*> items;
        for (int i = 0; i < m_columnCount; ++i) {
            QStandardItem* item = new QStandardItem("");
            item->setData(QString(), Qt::UserRole + 1);
            items << item;
        }
        m_model->insertRow(m_row, items);
        *m_guard = false;
    }

private:
    QStandardItemModel* m_model;
    int m_row;
    int m_columnCount;
    bool* m_guard;
};

class RemoveRowCommand : public QUndoCommand
{
public:
    RemoveRowCommand(QStandardItemModel* model, int row, bool* guard, QUndoCommand* parent = nullptr)
        : QUndoCommand(parent), m_model(model), m_row(row), m_guard(guard) {
        if (!m_model) return;
        for (int c = 0; c < m_model->columnCount(); ++c) {
            QStandardItem* item = m_model->item(row, c);
            m_values << (item ? item->text() : QString());
        }
    }

    void undo() override {
        if (!m_model) return;
        *m_guard = true;
        QList<QStandardItem*> items;
        for (const QString& value : m_values) {
            QStandardItem* item = new QStandardItem(value);
            item->setData(value, Qt::UserRole + 1);
            items << item;
        }
        m_model->insertRow(m_row, items);
        *m_guard = false;
    }

    void redo() override {
        if (!m_model) return;
        *m_guard = true;
        m_model->removeRow(m_row);
        *m_guard = false;
    }

private:
    QStandardItemModel* m_model;
    int m_row;
    QStringList m_values;
    bool* m_guard;
};

class InsertColumnCommand : public QUndoCommand
{
public:
    InsertColumnCommand(QStandardItemModel* model, int column, bool* guard, QList<ColumnDefinition>* definitions, QUndoCommand* parent = nullptr)
        : QUndoCommand(parent), m_model(model), m_column(column), m_guard(guard), m_definitions(definitions) {}

    void undo() override {
        if (!m_model) return;
        *m_guard = true;
        m_model->removeColumn(m_column);
        if (m_definitions && m_column < m_definitions->size())
            m_definitions->removeAt(m_column);
        *m_guard = false;
    }

    void redo() override {
        if (!m_model) return;
        *m_guard = true;
        m_model->insertColumn(m_column);
        ColumnDefinition d; d.name = "新列";
        if (m_definitions) {
            if (m_column < m_definitions->size()) m_definitions->insert(m_column, d);
            else m_definitions->append(d);
        }
        m_model->setHeaderData(m_column, Qt::Horizontal, "新列");
        for (int r = 0; r < m_model->rowCount(); ++r) {
            QStandardItem* item = m_model->item(r, m_column);
            if (!item) {
                item = new QStandardItem("");
                item->setData(QString(), Qt::UserRole + 1);
                m_model->setItem(r, m_column, item);
            }
        }
        *m_guard = false;
    }

private:
    QStandardItemModel* m_model;
    int m_column;
    bool* m_guard;
    QList<ColumnDefinition>* m_definitions;
};

class RemoveColumnCommand : public QUndoCommand
{
public:
    RemoveColumnCommand(QStandardItemModel* model, int column, bool* guard, QList<ColumnDefinition>* definitions, QUndoCommand* parent = nullptr)
        : QUndoCommand(parent), m_model(model), m_column(column), m_guard(guard), m_definitions(definitions) {
        if (!m_model) return;
        m_header = m_model->headerData(column, Qt::Horizontal).toString();
        if (m_definitions && column < m_definitions->size())
            m_definition = m_definitions->at(column);
        for (int r = 0; r < m_model->rowCount(); ++r) {
            QStandardItem* item = m_model->item(r, column);
            m_values << (item ? item->text() : QString());
        }
    }

    void undo() override {
        if (!m_model) return;
        *m_guard = true;
        m_model->insertColumn(m_column);
        m_model->setHeaderData(m_column, Qt::Horizontal, m_header);
        if (m_definitions) {
            if (m_column < m_definitions->size()) m_definitions->insert(m_column, m_definition);
            else m_definitions->append(m_definition);
        }
        for (int r = 0; r < m_values.size(); ++r) {
            QStandardItem* item = new QStandardItem(m_values[r]);
            item->setData(m_values[r], Qt::UserRole + 1);
            m_model->setItem(r, m_column, item);
        }
        *m_guard = false;
    }

    void redo() override {
        if (!m_model) return;
        *m_guard = true;
        m_model->removeColumn(m_column);
        if (m_definitions && m_column < m_definitions->size())
            m_definitions->removeAt(m_column);
        *m_guard = false;
    }

private:
    QStandardItemModel* m_model;
    int m_column;
    QString m_header;
    ColumnDefinition m_definition;
    QStringList m_values;
    bool* m_guard;
    QList<ColumnDefinition>* m_definitions;
};

class TableSnapshotCommand : public QUndoCommand
{
public:
    TableSnapshotCommand(QStandardItemModel* model,
                         QList<ColumnDefinition>* definitions,
                         const QStringList& oldHeaders,
                         const QVector<QStringList>& oldRows,
                         const QList<ColumnDefinition>& oldDefinitions,
                         const QStringList& newHeaders,
                         const QVector<QStringList>& newRows,
                         const QList<ColumnDefinition>& newDefinitions,
                         bool* guard,
                         QUndoCommand* parent = nullptr)
        : QUndoCommand(parent),
          m_model(model),
          m_definitions(definitions),
          m_oldHeaders(oldHeaders),
          m_oldRows(oldRows),
          m_oldDefinitions(oldDefinitions),
          m_newHeaders(newHeaders),
          m_newRows(newRows),
          m_newDefinitions(newDefinitions),
          m_guard(guard) {}

    void undo() override { applySnapshot(m_oldHeaders, m_oldRows, m_oldDefinitions); }
    void redo() override { applySnapshot(m_newHeaders, m_newRows, m_newDefinitions); }

private:
    void applySnapshot(const QStringList& headers, const QVector<QStringList>& rows, const QList<ColumnDefinition>& definitions)
    {
        if (!m_model) return;
        *m_guard = true;
        m_model->clear();
        m_model->setColumnCount(headers.size());
        m_model->setHorizontalHeaderLabels(headers);
        for (const QStringList& row : rows) {
            QList<QStandardItem*> items;
            for (const QString& value : row) {
                QStandardItem* item = new QStandardItem(value);
                item->setData(value, Qt::UserRole + 1);
                items << item;
            }
            m_model->appendRow(items);
        }
        if (m_definitions) {
            *m_definitions = definitions;
        }
        *m_guard = false;
    }

    QStandardItemModel* m_model;
    QList<ColumnDefinition>* m_definitions;
    QStringList m_oldHeaders;
    QVector<QStringList> m_oldRows;
    QList<ColumnDefinition> m_oldDefinitions;
    QStringList m_newHeaders;
    QVector<QStringList> m_newRows;
    QList<ColumnDefinition> m_newDefinitions;
    bool* m_guard;
};

class ViewStateCommand : public QUndoCommand
{
public:
    struct SpanState {
        int row;
        int column;
        int rowSpan;
        int columnSpan;
    };

    ViewStateCommand(QTableView* tableView,
                     const QList<int>& oldHiddenRows,
                     const QList<int>& oldHiddenCols,
                     const QList<SpanState>& oldSpans,
                     const QList<int>& newHiddenRows,
                     const QList<int>& newHiddenCols,
                     const QList<SpanState>& newSpans,
                     QUndoCommand* parent = nullptr)
        : QUndoCommand(parent),
          m_tableView(tableView),
          m_oldHiddenRows(oldHiddenRows),
          m_oldHiddenCols(oldHiddenCols),
          m_oldSpans(oldSpans),
          m_newHiddenRows(newHiddenRows),
          m_newHiddenCols(newHiddenCols),
          m_newSpans(newSpans) {}

    void undo() override { applyState(m_oldHiddenRows, m_oldHiddenCols, m_oldSpans); }
    void redo() override { applyState(m_newHiddenRows, m_newHiddenCols, m_newSpans); }

private:
    void applyState(const QList<int>& hiddenRows, const QList<int>& hiddenCols, const QList<SpanState>& spans)
    {
        if (!m_tableView || !m_tableView->model()) return;
        for (int r = 0; r < m_tableView->model()->rowCount(); ++r) {
            m_tableView->setRowHidden(r, hiddenRows.contains(r));
            for (int c = 0; c < m_tableView->model()->columnCount(); ++c)
                m_tableView->setSpan(r, c, 1, 1);
        }
        for (int c = 0; c < m_tableView->model()->columnCount(); ++c)
            m_tableView->setColumnHidden(c, hiddenCols.contains(c));
        for (const SpanState& span : spans)
            m_tableView->setSpan(span.row, span.column, span.rowSpan, span.columnSpan);
    }

    QTableView* m_tableView;
    QList<int> m_oldHiddenRows;
    QList<int> m_oldHiddenCols;
    QList<SpanState> m_oldSpans;
    QList<int> m_newHiddenRows;
    QList<int> m_newHiddenCols;
    QList<SpanState> m_newSpans;
};

QWidget *NoContextMenuDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                             const QModelIndex &index) const
{
    QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
    if (editor) editor->installEventFilter(new EditorEventFilter(editor));
    return editor;
}

// ============================================================================
// [类实现] DataSingleSheet
// ============================================================================

DataSingleSheet::DataSingleSheet(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataSingleSheet),
    m_dataModel(new QStandardItemModel(this)),
    m_proxyModel(new QSortFilterProxyModel(this)),
    m_undoStack(new QUndoStack(this))
{
    ui->setupUi(this);
    initUI();
    setupModel();

    connect(ui->dataTableView, &QTableView::customContextMenuRequested, this, &DataSingleSheet::onCustomContextMenu);
    connect(m_dataModel, &QStandardItemModel::itemChanged, this, &DataSingleSheet::onModelDataChanged);

    ui->dataTableView->viewport()->installEventFilter(this);

    auto undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, m_undoStack, &QUndoStack::undo);
    auto redoShortcut = new QShortcut(QKeySequence::Redo, this);
    connect(redoShortcut, &QShortcut::activated, m_undoStack, &QUndoStack::redo);
}

DataSingleSheet::~DataSingleSheet()
{
    delete ui;
}

void DataSingleSheet::initUI()
{
    ui->dataTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->dataTableView->setItemDelegate(new NoContextMenuDelegate(this));
}

void DataSingleSheet::setupModel()
{
    m_proxyModel->setSourceModel(m_dataModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    ui->dataTableView->setModel(m_proxyModel);
    ui->dataTableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->dataTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // 【修改点】：彻底关闭点击表头自动排序功能，并锁定表头点击交互
    ui->dataTableView->setSortingEnabled(false);
    ui->dataTableView->horizontalHeader()->setSectionsClickable(false);
}

bool DataSingleSheet::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->dataTableView->viewport() && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            int delta = wheelEvent->angleDelta().y();
            if (delta == 0) return false;

            QFont font = ui->dataTableView->font();
            int fontSize = font.pointSize();

            if (delta > 0) fontSize += 1;
            else fontSize -= 1;

            if (fontSize < 5) fontSize = 5;
            if (fontSize > 30) fontSize = 30;

            font.setPointSize(fontSize);
            ui->dataTableView->setFont(font);
            ui->dataTableView->resizeRowsToContents();

            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DataSingleSheet::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Paste)) {
        QModelIndex currentIndex = ui->dataTableView->currentIndex();
        if (!currentIndex.isValid()) {
            QWidget::keyPressEvent(event);
            return;
        }

        QString clipboardText = QApplication::clipboard()->text();
        if (clipboardText.isEmpty()) {
            QWidget::keyPressEvent(event);
            return;
        }

        QStringList rows = clipboardText.split(QRegularExpression("\r\n|\n|\r"), Qt::SkipEmptyParts);
        int startRow = m_proxyModel->mapToSource(currentIndex).row();
        int startCol = m_proxyModel->mapToSource(currentIndex).column();

        for (int rowOffset = 0; rowOffset < rows.size(); ++rowOffset) {
            QStringList cols = rows[rowOffset].split('\t', Qt::KeepEmptyParts);
            int targetRow = startRow + rowOffset;
            while (m_dataModel->rowCount() <= targetRow) {
                m_dataModel->insertRow(m_dataModel->rowCount());
            }

            for (int colOffset = 0; colOffset < cols.size(); ++colOffset) {
                int targetCol = startCol + colOffset;
                while (m_dataModel->columnCount() <= targetCol) {
                    m_dataModel->insertColumn(m_dataModel->columnCount());
                    if (m_dataModel->headerData(targetCol, Qt::Horizontal).toString().isEmpty()) {
                        m_dataModel->setHeaderData(targetCol, Qt::Horizontal, QString("Col %1").arg(targetCol + 1));
                    }
                }

                QString value = formatStringIfNumber(cols[colOffset].trimmed());
                QStandardItem* item = m_dataModel->item(targetRow, targetCol);
                QString oldValue;
                if (!item) {
                    item = new QStandardItem;
                    m_dataModel->setItem(targetRow, targetCol, item);
                } else {
                    oldValue = item->text();
                }

                if (oldValue != value) {
                    m_undoStack->push(new CellEditCommand(m_dataModel, targetRow, targetCol, oldValue, value, &m_isApplyingUndoRedo));
                }
            }
        }

        emit dataChanged();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void DataSingleSheet::setFilterText(const QString& text)
{
    m_proxyModel->setFilterWildcard(text);
}

bool DataSingleSheet::loadData(const QString& filePath, const DataImportSettings& settings)
{
    m_filePath = filePath;
    m_dataModel->clear();
    m_columnDefinitions.clear();

    if (settings.isExcel) return loadExcelFile(filePath, settings);
    else return loadTextFile(filePath, settings);
}

bool DataSingleSheet::loadExcelFile(const QString& path, const DataImportSettings& settings)
{
    if(path.endsWith(".xlsx", Qt::CaseInsensitive)) {
        QXlsx::Document xlsx(path);
        if(!xlsx.load()) {
            showStyledMessage(this, Standard_MessageBox::Error, "错误", "无法加载 .xlsx 文件");
            return false;
        }

        if(xlsx.currentWorksheet()==nullptr && !xlsx.sheetNames().isEmpty())
            xlsx.selectSheet(xlsx.sheetNames().first());

        int maxRow = xlsx.dimension().lastRow();
        int maxCol = xlsx.dimension().lastColumn();
        if(maxRow < 1 || maxCol < 1) return true;

        for(int r = 1; r <= maxRow; ++r) {
            if(r < settings.startRow && !(settings.useHeader && r == settings.headerRow)) continue;

            QStringList fields;
            for(int c = 1; c <= maxCol; ++c) {
                auto cell = xlsx.cellAt(r, c);
                if(cell) {
                    if(cell->isDateTime())
                        fields.append(cell->readValue().toDateTime().toString("yyyy-MM-dd hh:mm:ss"));
                    else
                        // 【修改点】：利用辅助函数格式化取消科学计数法
                        fields.append(formatStringIfNumber(cell->value().toString()));
                } else {
                    fields.append("");
                }
            }

            if(settings.useHeader && r == settings.headerRow) {
                m_dataModel->setHorizontalHeaderLabels(fields);
                for(auto h : fields) {
                    ColumnDefinition d; d.name = h; m_columnDefinitions.append(d);
                }
            } else if(r >= settings.startRow) {
                QList<QStandardItem*> items;
                for(auto f : fields) items.append(new QStandardItem(f));
                m_dataModel->appendRow(items);
            }
        }
        for (int r = 0; r < m_dataModel->rowCount(); ++r)
            for (int c = 0; c < m_dataModel->columnCount(); ++c)
                if (auto item = m_dataModel->item(r, c))
                    item->setData(item->text(), Qt::UserRole + 1);
        return true;
    } else {
        QAxObject excel("Excel.Application");
        if(excel.isNull()) return false;

        excel.setProperty("Visible", false);
        excel.setProperty("DisplayAlerts", false);

        QAxObject* workbooks = excel.querySubObject("Workbooks");
        if (!workbooks) return false;

        QAxObject* wb = workbooks->querySubObject("Open(const QString&)", QDir::toNativeSeparators(path));
        if(!wb) { excel.dynamicCall("Quit()"); return false; }

        QAxObject* sheets = wb->querySubObject("Worksheets");
        QAxObject* sheet = sheets->querySubObject("Item(int)", 1);

        if(sheet) {
            QAxObject* ur = sheet->querySubObject("UsedRange");
            if(ur) {
                QVariant val = ur->dynamicCall("Value()");
                QList<QList<QVariant>> data;

                if(val.typeId() == QMetaType::QVariantList) {
                    for(auto r : val.toList()) {
                        if(r.typeId() == QMetaType::QVariantList)
                            data.append(r.toList());
                    }
                }

                for(int i = 0; i < data.size(); ++i) {
                    int currentRow = i + 1;
                    if(currentRow < settings.startRow && !(settings.useHeader && currentRow == settings.headerRow)) continue;

                    QStringList fields;
                    for(auto c : data[i]) {
                        if(c.typeId() == QMetaType::QDateTime)
                            fields.append(c.toDateTime().toString("yyyy-MM-dd hh:mm:ss"));
                        else if(c.typeId() == QMetaType::QDate)
                            fields.append(c.toDate().toString("yyyy-MM-dd"));
                        else
                            // 【修改点】：利用辅助函数格式化取消科学计数法
                            fields.append(formatStringIfNumber(c.toString()));
                    }

                    if(settings.useHeader && currentRow == settings.headerRow) {
                        m_dataModel->setHorizontalHeaderLabels(fields);
                        for(auto h : fields) {
                            ColumnDefinition d; d.name = h; m_columnDefinitions.append(d);
                        }
                    } else if(currentRow >= settings.startRow) {
                        QList<QStandardItem*> items;
                        for(auto f : fields) items.append(new QStandardItem(f));
                        m_dataModel->appendRow(items);
                    }
                }
                delete ur;
            }
            delete sheet;
        }
        wb->dynamicCall("Close()"); delete wb; delete workbooks; excel.dynamicCall("Quit()");
        for (int r = 0; r < m_dataModel->rowCount(); ++r)
            for (int c = 0; c < m_dataModel->columnCount(); ++c)
                if (auto item = m_dataModel->item(r, c))
                    item->setData(item->text(), Qt::UserRole + 1);
        return true;
    }
}

bool DataSingleSheet::loadTextFile(const QString& path, const DataImportSettings& settings)
{
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QTextStream in(&f);

    if(settings.encoding.startsWith("GBK")) in.setEncoding(QStringConverter::System);
    else if (settings.encoding.startsWith("ISO")) in.setEncoding(QStringConverter::Latin1);
    else in.setEncoding(QStringConverter::Utf8);

    QChar separator = ',';
    if (settings.separator.contains("Tab")) separator = '\t';
    else if (settings.separator.contains("Space")) separator = ' ';
    else if (settings.separator.contains("Semicolon")) separator = ';';
    else if (settings.separator.contains("Comma")) separator = ',';
    else if (settings.separator.contains("Auto")) {
        qint64 originalPos = in.pos();
        QString firstLine = in.readLine();
        if (firstLine.count('\t') > firstLine.count(',')) separator = '\t';
        in.seek(originalPos);
    }

    int lineIdx = 0;
    while(!in.atEnd()) {
        QString line = in.readLine();
        lineIdx++;

        bool isHeader = (settings.useHeader && lineIdx == settings.headerRow);
        bool isData = (lineIdx >= settings.startRow);

        if (!isHeader && !isData) continue;

        QStringList parts = line.split(separator);
        for (int i = 0; i < parts.size(); ++i) {
            QString p = parts[i].trimmed();
            if (p.startsWith('"') && p.endsWith('"') && p.length() >= 2) {
                p = p.mid(1, p.length() - 2);
            }
            parts[i] = p;
        }

        if (isHeader) {
            m_dataModel->setHorizontalHeaderLabels(parts);
            m_columnDefinitions.clear();
            for (const QString& h : parts) {
                ColumnDefinition d; d.name = h; m_columnDefinitions.append(d);
            }
        } else if (isData) {
            QList<QStandardItem*> items;
            // 【修改点】：文本直接转常规表示的字符串
            for(const QString& p : parts) items.append(new QStandardItem(formatStringIfNumber(p.trimmed())));
            m_dataModel->appendRow(items);
        }
    }

    f.close();
    for (int r = 0; r < m_dataModel->rowCount(); ++r)
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            if (auto item = m_dataModel->item(r, c))
                item->setData(item->text(), Qt::UserRole + 1);
    return true;
}

void DataSingleSheet::onExportExcel()
{
    QString path = QFileDialog::getSaveFileName(this, "导出 Excel", SettingPaths::reportPath() + "/table_export.xlsx", "Excel 文件 (*.xlsx)");
    if (path.isEmpty()) return;

    QXlsx::Document xlsx;
    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    headerFormat.setFillPattern(QXlsx::Format::PatternSolid);
    headerFormat.setPatternBackgroundColor(QColor(240, 240, 240));
    headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    headerFormat.setBorderStyle(QXlsx::Format::BorderThin);

    int colCount = m_dataModel->columnCount();
    int rowCount = m_dataModel->rowCount();

    for (int col = 0; col < colCount; ++col) {
        QString header = m_dataModel->headerData(col, Qt::Horizontal).toString();
        xlsx.write(1, col + 1, header, headerFormat);
        if (ui->dataTableView->isColumnHidden(col)) xlsx.setColumnHidden(col + 1, true);
    }

    for (int row = 0; row < rowCount; ++row) {
        if (ui->dataTableView->isRowHidden(row)) xlsx.setRowHidden(row + 2, true);

        for (int col = 0; col < colCount; ++col) {
            QStandardItem* item = m_dataModel->item(row, col);
            if (!item) continue;

            QVariant value = item->data(Qt::DisplayRole);
            QString strVal = value.toString();
            QXlsx::Format cellFormat;

            if (strVal.startsWith("=")) xlsx.write(row + 2, col + 1, strVal, cellFormat);
            else {
                bool ok; double dVal = value.toDouble(&ok);
                if (ok && !strVal.isEmpty()) xlsx.write(row + 2, col + 1, dVal, cellFormat);
                else xlsx.write(row + 2, col + 1, strVal, cellFormat);
            }
        }
    }

    if (xlsx.saveAs(path)) showStyledMessage(this, Standard_MessageBox::Info, "成功", "数据已成功导出！");
    else showStyledMessage(this, Standard_MessageBox::Warning, "失败", "导出失败，请检查文件是否被占用。");
}

void DataSingleSheet::onCustomContextMenu(const QPoint& pos) {
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #FFFFFF; border: 1px solid #CCCCCC; padding: 4px; } "
                       "QMenu::item { padding: 6px 24px; color: #333333; } "
                       "QMenu::item:selected { background-color: #E6F7FF; color: #000000; }");

    QMenu* rowMenu = menu.addMenu("行操作");
    rowMenu->addAction("在上方插入行", [=](){ onAddRow(1); });
    rowMenu->addAction("在下方插入行", [=](){ onAddRow(2); });
    rowMenu->addAction("删除选中行", this, &DataSingleSheet::onDeleteRow);
    rowMenu->addSeparator();
    rowMenu->addAction("隐藏选中行", this, &DataSingleSheet::onHideRow);
    rowMenu->addAction("显示所有行", this, &DataSingleSheet::onShowAllRows);

    QMenu* colMenu = menu.addMenu("列操作");
    colMenu->addAction("在左侧插入列", [=](){ onAddCol(1); });
    colMenu->addAction("在右侧插入列", [=](){ onAddCol(2); });
    colMenu->addAction("删除选中列", this, &DataSingleSheet::onDeleteCol);
    colMenu->addSeparator();
    colMenu->addAction("隐藏选中列", this, &DataSingleSheet::onHideCol);
    colMenu->addAction("显示所有列", this, &DataSingleSheet::onShowAllCols);

    menu.addSeparator();

    QMenu* dataMenu = menu.addMenu("数据处理");
    dataMenu->addAction("升序排列 (A-Z)", this, &DataSingleSheet::onSortAscending);
    dataMenu->addAction("降序排列 (Z-A)", this, &DataSingleSheet::onSortDescending);
    dataMenu->addAction("数据分列...", this, &DataSingleSheet::onSplitColumn);

    if (ui->dataTableView->selectionModel()->selectedIndexes().size() > 1) {
        menu.addSeparator();
        menu.addAction("合并单元格", this, &DataSingleSheet::onMergeCells);
        menu.addAction("取消合并", this, &DataSingleSheet::onUnmergeCells);
    }

    menu.exec(ui->dataTableView->mapToGlobal(pos));
}

void DataSingleSheet::onHideRow() {
    QList<int> oldHiddenRows, oldHiddenCols, newHiddenRows, newHiddenCols;
    QList<ViewStateCommand::SpanState> oldSpans, newSpans;
    for (int r = 0; r < m_dataModel->rowCount(); ++r) if (ui->dataTableView->isRowHidden(r)) oldHiddenRows << r;
    for (int c = 0; c < m_dataModel->columnCount(); ++c) if (ui->dataTableView->isColumnHidden(c)) oldHiddenCols << c;

    QModelIndexList s = ui->dataTableView->selectionModel()->selectedRows();
    if(s.isEmpty()) {
        QModelIndex i = ui->dataTableView->currentIndex();
        if(i.isValid()) ui->dataTableView->setRowHidden(i.row(),true);
    } else {
        for(auto i : s) ui->dataTableView->setRowHidden(i.row(),true);
    }

    for (int r = 0; r < m_dataModel->rowCount(); ++r) if (ui->dataTableView->isRowHidden(r)) newHiddenRows << r;
    for (int c = 0; c < m_dataModel->columnCount(); ++c) if (ui->dataTableView->isColumnHidden(c)) newHiddenCols << c;
    m_undoStack->push(new ViewStateCommand(ui->dataTableView, oldHiddenRows, oldHiddenCols, oldSpans, newHiddenRows, newHiddenCols, newSpans));
}
void DataSingleSheet::onShowAllRows() {
    QList<int> oldHiddenRows, oldHiddenCols, newHiddenRows, newHiddenCols;
    QList<ViewStateCommand::SpanState> oldSpans, newSpans;
    for (int r = 0; r < m_dataModel->rowCount(); ++r) if (ui->dataTableView->isRowHidden(r)) oldHiddenRows << r;
    for (int c = 0; c < m_dataModel->columnCount(); ++c) if (ui->dataTableView->isColumnHidden(c)) oldHiddenCols << c;
    for(int i=0; i<m_dataModel->rowCount(); ++i) ui->dataTableView->setRowHidden(i, false);
    for (int r = 0; r < m_dataModel->rowCount(); ++r) if (ui->dataTableView->isRowHidden(r)) newHiddenRows << r;
    for (int c = 0; c < m_dataModel->columnCount(); ++c) if (ui->dataTableView->isColumnHidden(c)) newHiddenCols << c;
    m_undoStack->push(new ViewStateCommand(ui->dataTableView, oldHiddenRows, oldHiddenCols, oldSpans, newHiddenRows, newHiddenCols, newSpans));
}
void DataSingleSheet::onHideCol() {
    QList<int> oldHiddenRows, oldHiddenCols, newHiddenRows, newHiddenCols;
    QList<ViewStateCommand::SpanState> oldSpans, newSpans;
    for (int r = 0; r < m_dataModel->rowCount(); ++r) if (ui->dataTableView->isRowHidden(r)) oldHiddenRows << r;
    for (int c = 0; c < m_dataModel->columnCount(); ++c) if (ui->dataTableView->isColumnHidden(c)) oldHiddenCols << c;

    QModelIndexList s = ui->dataTableView->selectionModel()->selectedColumns();
    if(s.isEmpty()) {
        QModelIndex i = ui->dataTableView->currentIndex();
        if(i.isValid()) ui->dataTableView->setColumnHidden(i.column(),true);
    } else {
        for(auto i : s) ui->dataTableView->setColumnHidden(i.column(),true);
    }

    for (int r = 0; r < m_dataModel->rowCount(); ++r) if (ui->dataTableView->isRowHidden(r)) newHiddenRows << r;
    for (int c = 0; c < m_dataModel->columnCount(); ++c) if (ui->dataTableView->isColumnHidden(c)) newHiddenCols << c;
    m_undoStack->push(new ViewStateCommand(ui->dataTableView, oldHiddenRows, oldHiddenCols, oldSpans, newHiddenRows, newHiddenCols, newSpans));
}
void DataSingleSheet::onShowAllCols() {
    QList<int> oldHiddenRows, oldHiddenCols, newHiddenRows, newHiddenCols;
    QList<ViewStateCommand::SpanState> oldSpans, newSpans;
    for (int r = 0; r < m_dataModel->rowCount(); ++r) if (ui->dataTableView->isRowHidden(r)) oldHiddenRows << r;
    for (int c = 0; c < m_dataModel->columnCount(); ++c) if (ui->dataTableView->isColumnHidden(c)) oldHiddenCols << c;
    for(int i=0; i<m_dataModel->columnCount(); ++i) ui->dataTableView->setColumnHidden(i, false);
    for (int r = 0; r < m_dataModel->rowCount(); ++r) if (ui->dataTableView->isRowHidden(r)) newHiddenRows << r;
    for (int c = 0; c < m_dataModel->columnCount(); ++c) if (ui->dataTableView->isColumnHidden(c)) newHiddenCols << c;
    m_undoStack->push(new ViewStateCommand(ui->dataTableView, oldHiddenRows, oldHiddenCols, oldSpans, newHiddenRows, newHiddenCols, newSpans));
}

void DataSingleSheet::onMergeCells() {
    auto s = ui->dataTableView->selectionModel()->selectedIndexes();
    if(s.isEmpty()) return;

    QList<int> oldHiddenRows, oldHiddenCols, newHiddenRows, newHiddenCols;
    QList<ViewStateCommand::SpanState> oldSpans, newSpans;
    for (int r = 0; r < m_dataModel->rowCount(); ++r)
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            if (ui->dataTableView->rowSpan(r, c) > 1 || ui->dataTableView->columnSpan(r, c) > 1)
                oldSpans << ViewStateCommand::SpanState{r, c, ui->dataTableView->rowSpan(r, c), ui->dataTableView->columnSpan(r, c)};

    int r1 = 2147483647, r2 = -1, c1 = 2147483647, c2 = -1;
    for(auto i : s){
        r1 = qMin(r1, i.row()); r2 = qMax(r2, i.row());
        c1 = qMin(c1, i.column()); c2 = qMax(c2, i.column());
    }
    ui->dataTableView->setSpan(r1, c1, r2-r1+1, c2-c1+1);

    for (int r = 0; r < m_dataModel->rowCount(); ++r)
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            if (ui->dataTableView->rowSpan(r, c) > 1 || ui->dataTableView->columnSpan(r, c) > 1)
                newSpans << ViewStateCommand::SpanState{r, c, ui->dataTableView->rowSpan(r, c), ui->dataTableView->columnSpan(r, c)};

    m_undoStack->push(new ViewStateCommand(ui->dataTableView, oldHiddenRows, oldHiddenCols, oldSpans, newHiddenRows, newHiddenCols, newSpans));
}

void DataSingleSheet::onUnmergeCells() {
    auto i = ui->dataTableView->currentIndex();
    if(!i.isValid()) return;

    QList<int> oldHiddenRows, oldHiddenCols, newHiddenRows, newHiddenCols;
    QList<ViewStateCommand::SpanState> oldSpans, newSpans;
    for (int r = 0; r < m_dataModel->rowCount(); ++r)
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            if (ui->dataTableView->rowSpan(r, c) > 1 || ui->dataTableView->columnSpan(r, c) > 1)
                oldSpans << ViewStateCommand::SpanState{r, c, ui->dataTableView->rowSpan(r, c), ui->dataTableView->columnSpan(r, c)};

    ui->dataTableView->setSpan(i.row(), i.column(), 1, 1);

    for (int r = 0; r < m_dataModel->rowCount(); ++r)
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            if (ui->dataTableView->rowSpan(r, c) > 1 || ui->dataTableView->columnSpan(r, c) > 1)
                newSpans << ViewStateCommand::SpanState{r, c, ui->dataTableView->rowSpan(r, c), ui->dataTableView->columnSpan(r, c)};

    m_undoStack->push(new ViewStateCommand(ui->dataTableView, oldHiddenRows, oldHiddenCols, oldSpans, newHiddenRows, newHiddenCols, newSpans));
}

void DataSingleSheet::onSortAscending() {
    if(!ui->dataTableView->currentIndex().isValid())
        return;

    QStringList oldHeaders;
    QVector<QStringList> oldRows;
    for (int c = 0; c < m_dataModel->columnCount(); ++c)
        oldHeaders << m_dataModel->headerData(c, Qt::Horizontal).toString();
    for (int r = 0; r < m_dataModel->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            row << (m_dataModel->item(r, c) ? m_dataModel->item(r, c)->text() : QString());
        oldRows << row;
    }
    QList<ColumnDefinition> oldDefinitions = m_columnDefinitions;

    m_proxyModel->sort(ui->dataTableView->currentIndex().column(), Qt::AscendingOrder);

    QVector<QStringList> newRows;
    for (int r = 0; r < m_proxyModel->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < m_proxyModel->columnCount(); ++c)
            row << m_proxyModel->data(m_proxyModel->index(r, c)).toString();
        newRows << row;
    }

    m_undoStack->push(new TableSnapshotCommand(m_dataModel, &m_columnDefinitions, oldHeaders, oldRows, oldDefinitions, oldHeaders, newRows, oldDefinitions, &m_isApplyingUndoRedo));
}
void DataSingleSheet::onSortDescending() {
    if(!ui->dataTableView->currentIndex().isValid())
        return;

    QStringList oldHeaders;
    QVector<QStringList> oldRows;
    for (int c = 0; c < m_dataModel->columnCount(); ++c)
        oldHeaders << m_dataModel->headerData(c, Qt::Horizontal).toString();
    for (int r = 0; r < m_dataModel->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            row << (m_dataModel->item(r, c) ? m_dataModel->item(r, c)->text() : QString());
        oldRows << row;
    }
    QList<ColumnDefinition> oldDefinitions = m_columnDefinitions;

    m_proxyModel->sort(ui->dataTableView->currentIndex().column(), Qt::DescendingOrder);

    QVector<QStringList> newRows;
    for (int r = 0; r < m_proxyModel->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < m_proxyModel->columnCount(); ++c)
            row << m_proxyModel->data(m_proxyModel->index(r, c)).toString();
        newRows << row;
    }

    m_undoStack->push(new TableSnapshotCommand(m_dataModel, &m_columnDefinitions, oldHeaders, oldRows, oldDefinitions, oldHeaders, newRows, oldDefinitions, &m_isApplyingUndoRedo));
}

void DataSingleSheet::onAddRow(int m) {
    int r = m_dataModel->rowCount();
    QModelIndex i = ui->dataTableView->currentIndex();
    if(i.isValid()){
        int sr = m_proxyModel->mapToSource(i).row();
        r = (m == 1) ? sr : sr + 1;
    }
    m_undoStack->push(new InsertRowCommand(m_dataModel, r, m_dataModel->columnCount(), &m_isApplyingUndoRedo));
}

void DataSingleSheet::onDeleteRow() {
    auto s = ui->dataTableView->selectionModel()->selectedRows();
    if(s.isEmpty()){
        auto i = ui->dataTableView->currentIndex();
        if(i.isValid()) m_undoStack->push(new RemoveRowCommand(m_dataModel, m_proxyModel->mapToSource(i).row(), &m_isApplyingUndoRedo));
    } else {
        QList<int> rs;
        for(auto i : s) rs << m_proxyModel->mapToSource(i).row();
        std::sort(rs.begin(), rs.end(), std::greater<int>());
        auto l = std::unique(rs.begin(), rs.end());
        rs.erase(l, rs.end());
        for(int r : rs) m_undoStack->push(new RemoveRowCommand(m_dataModel, r, &m_isApplyingUndoRedo));
    }
}

void DataSingleSheet::onAddCol(int m) {
    int c = m_dataModel->columnCount();
    QModelIndex i = ui->dataTableView->currentIndex();
    if(i.isValid()){
        int sc = m_proxyModel->mapToSource(i).column();
        c = (m == 1) ? sc : sc + 1;
    }
    m_undoStack->push(new InsertColumnCommand(m_dataModel, c, &m_isApplyingUndoRedo, &m_columnDefinitions));
}

void DataSingleSheet::onDeleteCol() {
    auto s = ui->dataTableView->selectionModel()->selectedColumns();
    if(s.isEmpty()){
        auto i = ui->dataTableView->currentIndex();
        if(i.isValid()){
            int c = m_proxyModel->mapToSource(i).column();
            m_undoStack->push(new RemoveColumnCommand(m_dataModel, c, &m_isApplyingUndoRedo, &m_columnDefinitions));
        }
    } else {
        QList<int> cs;
        for(auto i : s) cs << m_proxyModel->mapToSource(i).column();
        std::sort(cs.begin(), cs.end(), std::greater<int>());
        auto l = std::unique(cs.begin(), cs.end());
        cs.erase(l, cs.end());
        for(int c : cs){
            m_undoStack->push(new RemoveColumnCommand(m_dataModel, c, &m_isApplyingUndoRedo, &m_columnDefinitions));
        }
    }
}

void DataSingleSheet::onSplitColumn() {
    QModelIndex idx = ui->dataTableView->currentIndex();
    if (!idx.isValid()) return;
    int col = m_proxyModel->mapToSource(idx).column();

    InternalSplitDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    QString separator = dlg.getSeparator();
    if (separator.isEmpty()) return;

    QStringList oldHeaders;
    QVector<QStringList> oldRows;
    for (int c = 0; c < m_dataModel->columnCount(); ++c)
        oldHeaders << m_dataModel->headerData(c, Qt::Horizontal).toString();
    for (int r = 0; r < m_dataModel->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            row << (m_dataModel->item(r, c) ? m_dataModel->item(r, c)->text() : QString());
        oldRows << row;
    }
    QList<ColumnDefinition> oldDefinitions = m_columnDefinitions;

    int rows = m_dataModel->rowCount();
    m_dataModel->insertColumn(col + 1);

    ColumnDefinition def; def.name = "拆分数据";
    if (col + 1 < m_columnDefinitions.size()) m_columnDefinitions.insert(col + 1, def);
    else m_columnDefinitions.append(def);
    m_dataModel->setHeaderData(col + 1, Qt::Horizontal, "拆分数据");

    for (int i = 0; i < rows; ++i) {
        QStandardItem* item = m_dataModel->item(i, col);
        if (!item) continue;
        QString text = item->text();
        int sepIdx = text.indexOf(separator);
        if (sepIdx != -1) {
            QString left = text.left(sepIdx).trimmed();
            QString right = text.mid(sepIdx + separator.length()).trimmed();
            item->setText(left);
            item->setData(left, Qt::UserRole + 1);
            QStandardItem* newItem = new QStandardItem(right);
            newItem->setData(right, Qt::UserRole + 1);
            m_dataModel->setItem(i, col + 1, newItem);
        } else {
            QStandardItem* newItem = new QStandardItem("");
            newItem->setData(QString(), Qt::UserRole + 1);
            m_dataModel->setItem(i, col + 1, newItem);
        }
    }

    QStringList newHeaders;
    QVector<QStringList> newRows;
    for (int c = 0; c < m_dataModel->columnCount(); ++c)
        newHeaders << m_dataModel->headerData(c, Qt::Horizontal).toString();
    for (int r = 0; r < m_dataModel->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            row << (m_dataModel->item(r, c) ? m_dataModel->item(r, c)->text() : QString());
        newRows << row;
    }
    QList<ColumnDefinition> newDefinitions = m_columnDefinitions;

    m_undoStack->push(new TableSnapshotCommand(m_dataModel, &m_columnDefinitions, oldHeaders, oldRows, oldDefinitions, newHeaders, newRows, newDefinitions, &m_isApplyingUndoRedo));
}

void DataSingleSheet::onTimeConvert() {
    DataCalculate calc;
    QStringList h;
    for(int i=0; i<m_dataModel->columnCount(); ++i)
        h << m_dataModel->headerData(i, Qt::Horizontal).toString();

    TimeConversionDialog d(h, this);
    applySheetDialogStyle(&d);

    if(d.exec() == QDialog::Accepted){
        QStringList oldHeaders;
        QVector<QStringList> oldRows;
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            oldHeaders << m_dataModel->headerData(c, Qt::Horizontal).toString();
        for (int r = 0; r < m_dataModel->rowCount(); ++r) {
            QStringList row;
            for (int c = 0; c < m_dataModel->columnCount(); ++c)
                row << (m_dataModel->item(r, c) ? m_dataModel->item(r, c)->text() : QString());
            oldRows << row;
        }
        QList<ColumnDefinition> oldDefinitions = m_columnDefinitions;

        auto cfg = d.getConversionConfig();
        auto res = calc.convertTimeColumn(m_dataModel, m_columnDefinitions, cfg);
        if(res.success) {
            QStringList newHeaders;
            QVector<QStringList> newRows;
            for (int c = 0; c < m_dataModel->columnCount(); ++c)
                newHeaders << m_dataModel->headerData(c, Qt::Horizontal).toString();
            for (int r = 0; r < m_dataModel->rowCount(); ++r) {
                QStringList row;
                for (int c = 0; c < m_dataModel->columnCount(); ++c) {
                    QString value = m_dataModel->item(r, c) ? m_dataModel->item(r, c)->text() : QString();
                    row << value;
                    if (auto item = m_dataModel->item(r, c))
                        item->setData(value, Qt::UserRole + 1);
                }
                newRows << row;
            }
            QList<ColumnDefinition> newDefinitions = m_columnDefinitions;
            m_undoStack->push(new TableSnapshotCommand(m_dataModel, &m_columnDefinitions, oldHeaders, oldRows, oldDefinitions, newHeaders, newRows, newDefinitions, &m_isApplyingUndoRedo));
            showStyledMessage(this, Standard_MessageBox::Info, "成功", "时间列转换完成");
        }
        else showStyledMessage(this, Standard_MessageBox::Warning, "失败", res.errorMessage);
        emit dataChanged();
    }
}

void DataSingleSheet::onPressureDropCalc() {
    DataCalculate calc;

    QStringList oldHeaders;
    QVector<QStringList> oldRows;
    for (int c = 0; c < m_dataModel->columnCount(); ++c)
        oldHeaders << m_dataModel->headerData(c, Qt::Horizontal).toString();
    for (int r = 0; r < m_dataModel->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            row << (m_dataModel->item(r, c) ? m_dataModel->item(r, c)->text() : QString());
        oldRows << row;
    }
    QList<ColumnDefinition> oldDefinitions = m_columnDefinitions;

    auto res = calc.calculatePressureDrop(m_dataModel, m_columnDefinitions);
    if(res.success) {
        QStringList newHeaders;
        QVector<QStringList> newRows;
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            newHeaders << m_dataModel->headerData(c, Qt::Horizontal).toString();
        for (int r = 0; r < m_dataModel->rowCount(); ++r) {
            QStringList row;
            for (int c = 0; c < m_dataModel->columnCount(); ++c) {
                QString value = m_dataModel->item(r, c) ? m_dataModel->item(r, c)->text() : QString();
                row << value;
                if (auto item = m_dataModel->item(r, c))
                    item->setData(value, Qt::UserRole + 1);
            }
            newRows << row;
        }
        QList<ColumnDefinition> newDefinitions = m_columnDefinitions;
        m_undoStack->push(new TableSnapshotCommand(m_dataModel, &m_columnDefinitions, oldHeaders, oldRows, oldDefinitions, newHeaders, newRows, newDefinitions, &m_isApplyingUndoRedo));
        showStyledMessage(this, Standard_MessageBox::Info, "成功", "压降计算完成");
    }
    else showStyledMessage(this, Standard_MessageBox::Warning, "失败", res.errorMessage);
    emit dataChanged();
}

void DataSingleSheet::onCalcPwf() {
    DataCalculate calc;
    QStringList h;
    for(int i=0; i<m_dataModel->columnCount(); ++i)
        h << m_dataModel->headerData(i, Qt::Horizontal).toString();

    PwfCalculationDialog d(h, this);
    applySheetDialogStyle(&d);

    if(d.exec() == QDialog::Accepted){
        QStringList oldHeaders;
        QVector<QStringList> oldRows;
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            oldHeaders << m_dataModel->headerData(c, Qt::Horizontal).toString();
        for (int r = 0; r < m_dataModel->rowCount(); ++r) {
            QStringList row;
            for (int c = 0; c < m_dataModel->columnCount(); ++c)
                row << (m_dataModel->item(r, c) ? m_dataModel->item(r, c)->text() : QString());
            oldRows << row;
        }
        QList<ColumnDefinition> oldDefinitions = m_columnDefinitions;

        auto cfg = d.getConfig();
        auto res = calc.calculateBottomHolePressure(m_dataModel, m_columnDefinitions, cfg);
        if(res.success) {
            QStringList newHeaders;
            QVector<QStringList> newRows;
            for (int c = 0; c < m_dataModel->columnCount(); ++c)
                newHeaders << m_dataModel->headerData(c, Qt::Horizontal).toString();
            for (int r = 0; r < m_dataModel->rowCount(); ++r) {
                QStringList row;
                for (int c = 0; c < m_dataModel->columnCount(); ++c) {
                    QString value = m_dataModel->item(r, c) ? m_dataModel->item(r, c)->text() : QString();
                    row << value;
                    if (auto item = m_dataModel->item(r, c))
                        item->setData(value, Qt::UserRole + 1);
                }
                newRows << row;
            }
            QList<ColumnDefinition> newDefinitions = m_columnDefinitions;
            m_undoStack->push(new TableSnapshotCommand(m_dataModel, &m_columnDefinitions, oldHeaders, oldRows, oldDefinitions, newHeaders, newRows, newDefinitions, &m_isApplyingUndoRedo));
            showStyledMessage(this, Standard_MessageBox::Info, "成功", "井底流压计算完成");
        }
        else showStyledMessage(this, Standard_MessageBox::Warning, "失败", res.errorMessage);
        emit dataChanged();
    }
}

void DataSingleSheet::onHighlightErrors() {
    for (int r = 0; r < m_dataModel->rowCount(); ++r)
        for (int c = 0; c < m_dataModel->columnCount(); ++c)
            if (auto it = m_dataModel->item(r, c)) {
                it->setBackground(Qt::NoBrush);
                it->setToolTip(QString());
            }

    int timeIdx = -1;
    int pressureIdx = -1;
    int derivativeIdx = -1;

    for (int i = 0; i < m_columnDefinitions.size(); ++i) {
        const QString header = m_columnDefinitions[i].name;
        const QString lowerHeader = header.toLower();
        if (timeIdx == -1 && (lowerHeader.contains("time") || header.contains("时间")))
            timeIdx = i;
        if (pressureIdx == -1 && (lowerHeader.contains("pressure") || header.contains("压力") || header.contains("压降")))
            pressureIdx = i;
        if (derivativeIdx == -1 && (lowerHeader.contains("derivative") || header.contains("导数")))
            derivativeIdx = i;
    }

    QVector<double> pressureValues;
    if (pressureIdx != -1) {
        for (int r = 0; r < m_dataModel->rowCount(); ++r) {
            if (auto item = m_dataModel->item(r, pressureIdx)) {
                bool ok = false;
                double value = item->text().toDouble(&ok);
                if (ok)
                    pressureValues.append(value);
            }
        }
    }

    double pressureMean = 0.0;
    double pressureStd = 0.0;
    if (!pressureValues.isEmpty()) {
        for (double value : pressureValues)
            pressureMean += value;
        pressureMean /= pressureValues.size();
        for (double value : pressureValues)
            pressureStd += (value - pressureMean) * (value - pressureMean);
        pressureStd = std::sqrt(pressureStd / pressureValues.size());
    }

    int err = 0;
    double prevTime = -1.0;
    double prevDerivative = 0.0;
    bool hasPrevDerivative = false;

    for (int r = 0; r < m_dataModel->rowCount(); ++r) {
        if (timeIdx != -1) {
            auto item = m_dataModel->item(r, timeIdx);
            if (item) {
                bool ok = false;
                double timeValue = item->text().toDouble(&ok);
                if (!ok || timeValue < 0) {
                    item->setBackground(QColor(255, 200, 200));
                    item->setToolTip("时间值无效或小于0");
                    err++;
                } else if (prevTime >= 0 && timeValue < prevTime) {
                    item->setBackground(QColor(255, 230, 180));
                    item->setToolTip("时间序列不是单调递增");
                    err++;
                }
                if (ok)
                    prevTime = timeValue;
            }
        }

        if (pressureIdx != -1) {
            auto item = m_dataModel->item(r, pressureIdx);
            if (item) {
                bool ok = false;
                double pressureValue = item->text().toDouble(&ok);
                if (!ok || pressureValue < 0) {
                    item->setBackground(QColor(255, 200, 200));
                    item->setToolTip("压力值无效或小于0");
                    err++;
                } else if (pressureStd > 0.0 && std::fabs(pressureValue - pressureMean) > 3.0 * pressureStd) {
                    item->setBackground(QColor(255, 245, 180));
                    item->setToolTip("压力值疑似异常点（超过3σ）");
                    err++;
                }
            }
        }

        if (derivativeIdx != -1) {
            auto item = m_dataModel->item(r, derivativeIdx);
            if (item) {
                bool ok = false;
                double derivativeValue = item->text().toDouble(&ok);
                if (!ok) {
                    item->setBackground(QColor(255, 200, 200));
                    item->setToolTip("导数值无效");
                    err++;
                } else {
                    if (hasPrevDerivative && derivativeValue * prevDerivative < 0) {
                        item->setBackground(QColor(225, 210, 255));
                        item->setToolTip("导数符号发生翻转，请确认是否存在噪声或异常点");
                        err++;
                    }
                    prevDerivative = derivativeValue;
                    hasPrevDerivative = true;
                }
            }
        }
    }

    showStyledMessage(this, Standard_MessageBox::Info, "检查完成", QString("发现 %1 处潜在数据问题。\n红色：明显错误；橙色/黄色/紫色：建议复核。").arg(err));
}

void DataSingleSheet::onModelDataChanged(QStandardItem* item)
{
    if (!item) {
        emit dataChanged();
        return;
    }

    if (m_isApplyingUndoRedo) {
        emit dataChanged();
        return;
    }

    QVariant oldValueData = item->data(Qt::UserRole + 1);
    QString oldValue = oldValueData.toString();
    QString newValue = item->text();

    if (oldValueData.isValid() && oldValue != newValue) {
        m_undoStack->push(new CellEditCommand(m_dataModel, item->row(), item->column(), oldValue, newValue, &m_isApplyingUndoRedo));
        return;
    }

    item->setData(newValue, Qt::UserRole + 1);
    emit dataChanged();
}

QJsonObject DataSingleSheet::saveToJson() const {
    QJsonObject sheetObj;
    sheetObj["filePath"] = m_filePath;

    QJsonArray headers;
    for(int i=0; i<m_dataModel->columnCount(); ++i)
        headers.append(m_dataModel->headerData(i, Qt::Horizontal).toString());
    sheetObj["headers"] = headers;

    sheetObj["data"] = serializeRows();
    return sheetObj;
}

void DataSingleSheet::loadFromJson(const QJsonObject& jsonSheet) {
    m_dataModel->clear();
    m_columnDefinitions.clear();
    m_filePath = jsonSheet["filePath"].toString();

    QJsonArray headers = jsonSheet["headers"].toArray();
    QStringList sl;
    for(auto v: headers) sl << v.toString();
    m_dataModel->setHorizontalHeaderLabels(sl);

    for(auto s : sl) {
        ColumnDefinition d;
        d.name = s;
        m_columnDefinitions.append(d);
    }

    QJsonArray rows = jsonSheet["data"].toArray();
    deserializeRows(rows);
}

QJsonArray DataSingleSheet::serializeRows() const {
    QJsonArray a;
    for(int i=0; i<m_dataModel->rowCount(); ++i) {
        QJsonArray r;
        for(int j=0; j<m_dataModel->columnCount(); ++j) {
            QStandardItem* item = m_dataModel->item(i, j);
            if (item) r.append(item->text());
            else r.append("");
        }
        a.append(r);
    }
    return a;
}

void DataSingleSheet::deserializeRows(const QJsonArray& array) {
    for(auto val : array) {
        QJsonArray r = val.toArray();
        QList<QStandardItem*> l;
        // 【修改点】：从工程文件恢复数据时也强制转为常规数字格式
        for(auto v : r) l.append(new QStandardItem(formatStringIfNumber(v.toString())));
        m_dataModel->appendRow(l);
    }
}
