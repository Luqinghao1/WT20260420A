/*
 * 文件名: dataunitdialog.cpp
 * 文件作用: 单位与列属性管理对话框实现文件
 * 功能描述:
 * 1. [界面优化] 移除了顶部的单位制选择；整体表格加入斑马纹、弱化网格线，配色简约现代。
 * 2. [界面优化] 按钮配色定制：确认(绿色)、取消(红褐色)、预览(蓝色)。
 * 3. [功能新增] 增加前 15 行的数据实时换算预览功能 (弹出一个轻量级的预览表)。
 */

#include "dataunitdialog.h"
#include "ui_dataunitdialog.h"
#include <QHeaderView>
#include <QTableView>
#include <QVBoxLayout>
#include "standard_messagebox.h"

DataUnitDialog::DataUnitDialog(QStandardItemModel* dataModel, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DataUnitDialog),
    m_model(dataModel)
{
    ui->setupUi(this);
    this->setWindowTitle("列属性与单位管理");
    applyStyle();

    connect(ui->btnOk, &QPushButton::clicked, this, &DataUnitDialog::onBtnOkClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->btnPreview, &QPushButton::clicked, this, &DataUnitDialog::onBtnPreviewClicked);

    initTable();
}

DataUnitDialog::~DataUnitDialog()
{
    delete ui;
}

void DataUnitDialog::applyStyle()
{
    QString svgArrow = "data:image/svg+xml;utf8,%3Csvg%20width%3D%2210%22%20height%3D%228%22%20viewBox%3D%220%200%2010%208%22%20fill%3D%22none%22%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%3E%3Cpath%20d%3D%22M1%201L5%207L9%201Z%22%20fill%3D%22%231890FF%22%2F%3E%3C%2Fsvg%3E";

    QString qss = "QDialog, QWidget { background-color: #F8F9FA; color: #333333; font-family: 'Microsoft YaHei'; }"
                  "QTableWidget { gridline-color: #E9ECEF; border: 1px solid #DEE2E6; border-radius: 4px; background-color: #FFFFFF; alternate-background-color: #F8F9FA; }"
                  "QTableWidget::item { padding: 4px; }"
                  "QHeaderView::section { background-color: #F1F3F5; border: none; border-right: 1px solid #DEE2E6; border-bottom: 1px solid #DEE2E6; padding: 6px; font-weight: bold; color: #495057; }"
                  "QComboBox { border: 1px solid transparent; background: transparent; padding: 2px 4px; color: #333333; }"
                  "QComboBox:focus { border: 1px solid #1890FF; border-radius: 3px; background: #FFFFFF; }"
                  "QComboBox::drop-down { border-left: none; width: 22px; }"
                  "QComboBox::down-arrow { image: url('" + svgArrow + "'); width: 10px; height: 8px; }"
                  "QTableWidget QComboBox { margin: 0px; padding: 0px 4px; }"
                  "QPushButton { border: none; border-radius: 4px; padding: 6px 18px; color: white; font-weight: bold; min-height: 24px; }"
                  "QPushButton#btnOk { background-color: #52C41A; }"
                  "QPushButton#btnOk:hover { background-color: #73D13D; }"
                  "QPushButton#btnOk:pressed { background-color: #389E0D; }"
                  "QPushButton#btnPreview { background-color: #1890FF; }"
                  "QPushButton#btnPreview:hover { background-color: #40A9FF; }"
                  "QPushButton#btnPreview:pressed { background-color: #096DD9; }"
                  "QPushButton#btnCancel { background-color: #A54B4B; }"
                  "QPushButton#btnCancel:hover { background-color: #BD5959; }"
                  "QPushButton#btnCancel:pressed { background-color: #8C3A3A; }"
                  "QRadioButton, QCheckBox { spacing: 6px; color: #495057; font-weight: bold; }";

    this->setStyleSheet(qss);
}

void DataUnitDialog::initTable()
{
    int colCount = m_model->columnCount();
    ui->tableWidget->setRowCount(colCount);
    ui->tableWidget->setColumnCount(4);
    // 表头: 原始列名, 物理量选择, 当前单位, 转换后单位
    ui->tableWidget->setHorizontalHeaderLabels({"原始列名", "物理量选择", "当前单位", "转换后单位"});
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->setAlternatingRowColors(true);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(36);

    QStringList allQtys = DataUnitManager::instance()->getRegisteredQuantities();
    allQtys.prepend("");

    for (int i = 0; i < colCount; ++i) {
        QString fullHeader = m_model->horizontalHeaderItem(i) ? m_model->horizontalHeaderItem(i)->text() : QString("列%1").arg(i+1);
        m_originalHeaders.append(fullHeader);

        QString namePart = fullHeader;
        QString unitPart = "";
        if (fullHeader.contains("\\")) {
            namePart = fullHeader.split("\\").first().trimmed();
            unitPart = fullHeader.split("\\").last().trimmed();
        } else {
            namePart = fullHeader.trimmed();
        }

        QTableWidgetItem* itemHeader = new QTableWidgetItem(fullHeader);
        itemHeader->setFlags(itemHeader->flags() & ~Qt::ItemIsEditable);
        ui->tableWidget->setItem(i, 0, itemHeader);

        QComboBox* qtyCombo = new QComboBox();
        qtyCombo->setEditable(true);
        qtyCombo->addItems(allQtys);
        qtyCombo->setProperty("row", i);
        ui->tableWidget->setCellWidget(i, 1, qtyCombo);
        m_qtyCombos.append(qtyCombo);

        QComboBox* fromCombo = new QComboBox();
        fromCombo->setEditable(true);
        ui->tableWidget->setCellWidget(i, 2, fromCombo);
        m_fromUnitCombos.append(fromCombo);

        QComboBox* toCombo = new QComboBox();
        toCombo->setEditable(true);
        ui->tableWidget->setCellWidget(i, 3, toCombo);
        m_toUnitCombos.append(toCombo);

        connect(qtyCombo, &QComboBox::currentTextChanged, this, &DataUnitDialog::onQuantityChanged);

        qtyCombo->setCurrentText(namePart);
        if (!unitPart.isEmpty()) fromCombo->setCurrentText(unitPart);
        toCombo->setCurrentText("");
    }
}

void DataUnitDialog::onQuantityChanged(const QString& text)
{
    QComboBox* senderCombo = qobject_cast<QComboBox*>(sender());
    if (!senderCombo) return;
    int row = senderCombo->property("row").toInt();

    QString cleanText = text.trimmed();
    QStringList units = DataUnitManager::instance()->getUnitsForQuantity(cleanText);

    if (units.isEmpty() && !cleanText.isEmpty()) {
        units = DataUnitManager::instance()->getAllUniqueUnits();
    }

    units.prepend("");

    m_fromUnitCombos[row]->blockSignals(true);
    m_toUnitCombos[row]->blockSignals(true);

    QString oldFrom = m_fromUnitCombos[row]->currentText();
    QString oldTo = m_toUnitCombos[row]->currentText();

    m_fromUnitCombos[row]->clear();
    m_toUnitCombos[row]->clear();

    m_fromUnitCombos[row]->addItems(units);
    m_toUnitCombos[row]->addItems(units);

    if (m_fromUnitCombos[row]->findText(oldFrom) >= 0) m_fromUnitCombos[row]->setCurrentText(oldFrom);
    if (m_toUnitCombos[row]->findText(oldTo) >= 0) m_toUnitCombos[row]->setCurrentText(oldTo);

    m_fromUnitCombos[row]->blockSignals(false);
    m_toUnitCombos[row]->blockSignals(false);
}

void DataUnitDialog::onBtnOkClicked()
{
    for (int i = 0; i < m_qtyCombos.size(); ++i) {
        QString qty = m_qtyCombos[i]->currentText().trimmed();
        QString fromU = m_fromUnitCombos[i]->currentText().trimmed();
        QString toU = m_toUnitCombos[i]->currentText().trimmed();

        if (qty.isEmpty()) {
            Standard_MessageBox::warning(this, "校验失败",
                QString("第 %1 行的《物理量选择》不能为空！\n请填入或选择一个物理量。").arg(i + 1));
            return;
        }

        if (!toU.isEmpty() && fromU.isEmpty()) {
            Standard_MessageBox::warning(this, "校验失败",
                QString("第 %1 行配置了转换后单位 '%2'，表示需要换算。\n但《当前单位》为空，无法进行换算，请补充当前单位！").arg(i + 1).arg(toU));
            return;
        }
    }
    accept();
}

void DataUnitDialog::onBtnPreviewClicked()
{
    for (int i = 0; i < m_qtyCombos.size(); ++i) {
        QString qty = m_qtyCombos[i]->currentText().trimmed();
        QString fromU = m_fromUnitCombos[i]->currentText().trimmed();
        QString toU = m_toUnitCombos[i]->currentText().trimmed();

        if (qty.isEmpty()) {
            Standard_MessageBox::warning(this, "校验失败",
                QString("第 %1 行的《物理量选择》不能为空！").arg(i + 1));
            return;
        }
        if (!toU.isEmpty() && fromU.isEmpty()) {
            Standard_MessageBox::warning(this, "校验失败",
                QString("第 %1 行配置了转换后单位，但《当前单位》为空！").arg(i + 1));
            return;
        }
    }

    auto tasks = getConversionTasks();
    bool appendMode = isAppendMode();

    int previewRowCount = qMin(15, m_model->rowCount());
    int colCount = m_model->columnCount();

    QStandardItemModel previewModel;

    QStringList headers;
    for (int c = 0; c < colCount; ++c) {
        headers.append(m_model->horizontalHeaderItem(c) ? m_model->horizontalHeaderItem(c)->text() : "");
    }
    if (appendMode) {
        for (const auto& task : tasks) headers.append(task.newHeaderName);
    } else {
        for (const auto& task : tasks) headers[task.colIndex] = task.newHeaderName;
    }
    previewModel.setHorizontalHeaderLabels(headers);

    for (int r = 0; r < previewRowCount; ++r) {
        QList<QStandardItem*> rowItems;
        QStringList rowData;
        for (int c = 0; c < colCount; ++c) {
            rowData.append(m_model->item(r, c)->text());
        }

        if (appendMode) {
            for (const auto& task : tasks) {
                if (task.needsMathConversion) {
                    bool ok; double val = rowData[task.colIndex].toDouble(&ok);
                    if (ok) {
                        double nV = DataUnitManager::instance()->convert(val, task.quantityType, task.fromUnit, task.toUnit);
                        rowData.append(QString::number(nV, 'g', 6));
                    } else rowData.append("");
                } else rowData.append(rowData[task.colIndex]);
            }
        } else {
            for (const auto& task : tasks) {
                if (task.needsMathConversion) {
                    bool ok; double val = rowData[task.colIndex].toDouble(&ok);
                    if (ok) {
                        double nV = DataUnitManager::instance()->convert(val, task.quantityType, task.fromUnit, task.toUnit);
                        rowData[task.colIndex] = QString::number(nV, 'g', 6);
                    }
                }
            }
        }

        for (const QString& text : rowData) {
            rowItems.append(new QStandardItem(text));
        }
        previewModel.appendRow(rowItems);
    }

    QDialog previewDlg(this);
    previewDlg.setWindowTitle("转换效果预览 (取前15行)");
    previewDlg.resize(850, 450);
    previewDlg.setStyleSheet("QDialog { background-color: #F8F9FA; }");

    QVBoxLayout* layout = new QVBoxLayout(&previewDlg);
    QTableView* view = new QTableView(&previewDlg);
    view->setModel(&previewModel);
    view->horizontalHeader()->setStretchLastSection(true);
    view->setAlternatingRowColors(true);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setStyleSheet("QTableView { gridline-color: #E9ECEF; border: 1px solid #DEE2E6; background-color: #FFFFFF; alternate-background-color: #F8F9FA; }"
                        "QHeaderView::section { background-color: #F1F3F5; border: none; border-right: 1px solid #DEE2E6; border-bottom: 1px solid #DEE2E6; padding: 6px; font-weight: bold; color: #495057; }");
    layout->addWidget(view);

    QPushButton* btnClose = new QPushButton("关闭预览", &previewDlg);
    btnClose->setCursor(Qt::PointingHandCursor);
    btnClose->setStyleSheet("QPushButton { background-color: #1890FF; color: white; border: none; border-radius: 4px; padding: 6px 20px; font-weight: bold; } QPushButton:hover { background-color: #40A9FF; }");
    connect(btnClose, &QPushButton::clicked, &previewDlg, &QDialog::accept);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(btnClose);
    layout->addLayout(btnLayout);

    previewDlg.exec();
}

QList<DataUnitDialog::ConversionTask> DataUnitDialog::getConversionTasks() const
{
    QList<ConversionTask> tasks;
    for (int i = 0; i < m_originalHeaders.size(); ++i) {
        QString qty = m_qtyCombos[i]->currentText().trimmed();
        QString fromU = m_fromUnitCombos[i]->currentText().trimmed();
        QString toU = m_toUnitCombos[i]->currentText().trimmed();

        ConversionTask t;
        t.colIndex = i;
        t.quantityType = qty;
        t.fromUnit = fromU;
        t.toUnit = toU;

        t.needsMathConversion = (!toU.isEmpty() && !fromU.isEmpty() && fromU != toU);

        if (toU.isEmpty()) {
            if (fromU.isEmpty()) t.newHeaderName = qty;
            else t.newHeaderName = QString("%1\\%2").arg(qty).arg(fromU);
        } else {
            t.newHeaderName = QString("%1\\%2").arg(qty).arg(toU);
        }
        tasks.append(t);
    }
    return tasks;
}

bool DataUnitDialog::isAppendMode() const { return ui->radioAppend->isChecked(); }
bool DataUnitDialog::isSaveToFile() const { return ui->checkSaveFile->isChecked(); }
