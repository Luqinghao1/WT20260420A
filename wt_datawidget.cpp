/**
 * @file wt_datawidget.cpp
 * @brief 数据编辑器主窗口实现文件
 * @details
 * 【本代码文件的作用与功能】
 * 1. 管理 QTabWidget，支持多标签页显示多份数据文件。
 * 2. 实现了多文件同时打开、解析与展示的功能。
 * 3. 采用 Standard_ToolButton 重新装载了 9 大原生功能按键。
 * 4. 完美复原了原生高辨识度的彩色 QPainter 图标。
 * 5. 全面保留原有的滤波取样、单位转换、数据落盘等底层业务逻辑。
 */

#include "wt_datawidget.h"
#include "ui_wt_datawidget.h"
#include "modelparameter.h"
#include "dataimportdialog.h"
#include "datasamplingdialog.h"
#include "dataunitdialog.h"
#include "dataunitmanager.h"
#include "standard_toolbutton.h"   // 引入标准按钮
#include "standard_messagebox.h"   // 引入标准弹窗

#include <QToolButton>
#include <QPainter>
#include <QPixmap>
#include <QPolygon>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QTableView>
#include <QHeaderView>
#include <QScrollBar>
#include <QFileDialog>
#include <QDebug>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QDateTime>

#include "xlsxdocument.h"
#include "xlsxformat.h"

/**
 * @brief 辅助函数：统一应用数据对话框的样式
 */
static void applyDataDialogStyle(QWidget* dialog) {
    if (!dialog) return;
    QString qss = "QWidget { color: black; background-color: white; font-family: 'Microsoft YaHei'; }"
                  "QPushButton { background-color: #f0f0f0; color: black; border: 1px solid #bfbfbf; border-radius: 3px; padding: 5px 15px; min-width: 70px; }"
                  "QPushButton:hover { background-color: #e0e0e0; }"
                  "QPushButton:pressed { background-color: #d0d0d0; }";
    dialog->setStyleSheet(qss);
}

/**
 * @brief 辅助函数：格式化双精度浮点数，杜绝科学计数法
 */
static QString formatToNormalNumber(double val) {
    QString res = QString::number(val, 'f', 8);
    if (res.contains('.')) {
        while (res.endsWith('0')) res.chop(1);
        if (res.endsWith('.')) res.chop(1);
    }
    return res;
}

// =========================================================================
// =                           构造与初始化模块                               =
// =========================================================================

/**
 * @brief 构造函数
 */
WT_DataWidget::WT_DataWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WT_DataWidget),
    m_samplingWidget(nullptr),
    m_isSamplingMaximized(false),
    m_floatingWidth(-1)
{
    ui->setupUi(this);
    initUI();
    setupConnections();
}

/**
 * @brief 析构函数
 */
WT_DataWidget::~WT_DataWidget()
{
    delete ui;
}

/**
 * @brief 纯代码绘制原生高辨识度彩色矢量图标 (100% 复原)
 * @param iconType 功能标识名
 * @return QIcon 绘制好的图标
 */
QIcon WT_DataWidget::createCustomIcon(const QString& iconType)
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    if (iconType == "open") {
        painter.setBrush(QColor("#FFCA28")); painter.setPen(Qt::NoPen);
        painter.drawRect(8, 24, 48, 32);
        painter.drawPolygon(QPolygon({QPoint(8, 24), QPoint(20, 12), QPoint(32, 12), QPoint(44, 24)}));
    } else if (iconType == "save") {
        painter.setBrush(QColor("#4CAF50")); painter.setPen(Qt::NoPen);
        painter.drawRect(12, 12, 40, 40);
        painter.setBrush(Qt::white);
        painter.drawRect(20, 12, 24, 16); painter.drawRect(24, 36, 16, 16);
    } else if (iconType == "export") {
        painter.setBrush(QColor("#43A047")); painter.setPen(Qt::NoPen);
        painter.drawRect(10, 10, 44, 44);
        painter.setPen(QPen(Qt::white, 4));
        painter.drawLine(10, 26, 54, 26); painter.drawLine(32, 10, 32, 54);
    } else if (iconType == "unit") {
        painter.setBrush(QColor("#00ACC1")); painter.setPen(Qt::NoPen);
        painter.drawRect(8, 26, 48, 14);
        painter.setPen(QPen(Qt::white, 2));
        for (int i = 12; i <= 52; i += 8) { painter.drawLine(i, 26, i, (i % 16 == 12) ? 34 : 31); }
    } else if (iconType == "time") {
        painter.setBrush(QColor("#FF9800")); painter.setPen(Qt::NoPen);
        painter.drawEllipse(12, 12, 40, 40);
        painter.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(32, 32, 32, 20); painter.drawLine(32, 32, 42, 32);
    } else if (iconType == "pressuredrop") {
        painter.setBrush(QColor("#E53935")); painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygon({QPoint(24, 10), QPoint(40, 10), QPoint(40, 34), QPoint(50, 34), QPoint(32, 54), QPoint(14, 34), QPoint(24, 34)}));
    } else if (iconType == "pwf") {
        painter.setBrush(QColor("#3949AB")); painter.setPen(Qt::NoPen);
        painter.drawRect(24, 8, 16, 48);
        painter.setBrush(QColor("#29B6F6")); painter.drawRect(24, 36, 16, 20);
    } else if (iconType == "filter") {
        painter.setBrush(QColor("#8E24AA")); painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygon({QPoint(10, 12), QPoint(54, 12), QPoint(36, 32), QPoint(36, 52), QPoint(28, 52), QPoint(28, 32)}));
    } else if (iconType == "error") {
        painter.setBrush(QColor("#F4511E")); painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygon({QPoint(32, 10), QPoint(56, 52), QPoint(8, 52)}));
        painter.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(32, 24, 32, 38); painter.drawPoint(32, 46);
    }
    return QIcon(pixmap);
}

/**
 * @brief 界面初始化：使用 Standard_ToolButton 替换掉 UI 原生指针
 */
void WT_DataWidget::initUI()
{
    // 清空现有布局
    QLayoutItem *child;
    while ((child = ui->horizontalLayout_Toolbar->takeAt(0)) != nullptr) {
        if(child->widget()) delete child->widget();
        delete child;
    }

    // 重新实例化指针为标准组件，完美承接底层状态逻辑
    ui->btnOpenFile = new Standard_ToolButton("打开数据", createCustomIcon("open"), this);
    ui->btnSave = new Standard_ToolButton("同步保存", createCustomIcon("save"), this);
    ui->btnExport = new Standard_ToolButton("导出 Excel", createCustomIcon("export"), this);
    ui->btnUnitManager = new Standard_ToolButton("单位管理", createCustomIcon("unit"), this);
    ui->btnTimeConvert = new Standard_ToolButton("时间转换", createCustomIcon("time"), this);
    ui->btnPressureDropCalc = new Standard_ToolButton("计算压降", createCustomIcon("pressuredrop"), this);
    ui->btnCalcPwf = new Standard_ToolButton("套压转流压", createCustomIcon("pwf"), this);
    ui->btnFilterSample = new Standard_ToolButton("滤波取样", createCustomIcon("filter"), this);
    ui->btnErrorCheck = new Standard_ToolButton("异常检查", createCustomIcon("error"), this);

    // 按布局添加
    ui->horizontalLayout_Toolbar->addWidget(ui->btnOpenFile);
    ui->horizontalLayout_Toolbar->addWidget(ui->btnSave);
    ui->horizontalLayout_Toolbar->addWidget(ui->btnExport);

    QFrame* line1 = new QFrame(); line1->setFrameShape(QFrame::VLine); line1->setStyleSheet("border:1px solid #E0E0E0;");
    ui->horizontalLayout_Toolbar->addWidget(line1);

    ui->horizontalLayout_Toolbar->addWidget(ui->btnUnitManager);
    ui->horizontalLayout_Toolbar->addWidget(ui->btnTimeConvert);
    ui->horizontalLayout_Toolbar->addWidget(ui->btnPressureDropCalc);
    ui->horizontalLayout_Toolbar->addWidget(ui->btnCalcPwf);

    QFrame* line2 = new QFrame(); line2->setFrameShape(QFrame::VLine); line2->setStyleSheet("border:1px solid #E0E0E0;");
    ui->horizontalLayout_Toolbar->addWidget(line2);

    ui->horizontalLayout_Toolbar->addWidget(ui->btnFilterSample);
    ui->horizontalLayout_Toolbar->addWidget(ui->btnErrorCheck);
    ui->horizontalLayout_Toolbar->addStretch();

    updateButtonsState();
}

/**
 * @brief 绑定交互信号
 */
void WT_DataWidget::setupConnections()
{
    connect(ui->btnOpenFile, &QToolButton::clicked, this, &WT_DataWidget::onOpenFile);
    connect(ui->btnSave, &QToolButton::clicked, this, &WT_DataWidget::onSave);
    connect(ui->btnExport, &QToolButton::clicked, this, &WT_DataWidget::onExportExcel);
    connect(ui->btnUnitManager, &QToolButton::clicked, this, &WT_DataWidget::onUnitManager);
    connect(ui->btnTimeConvert, &QToolButton::clicked, this, &WT_DataWidget::onTimeConvert);
    connect(ui->btnPressureDropCalc, &QToolButton::clicked, this, &WT_DataWidget::onPressureDropCalc);
    connect(ui->btnCalcPwf, &QToolButton::clicked, this, &WT_DataWidget::onCalcPwf);
    connect(ui->btnFilterSample, &QToolButton::clicked, this, &WT_DataWidget::onFilterSample);
    connect(ui->btnErrorCheck, &QToolButton::clicked, this, &WT_DataWidget::onHighlightErrors);

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &WT_DataWidget::onTabChanged);
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &WT_DataWidget::onTabCloseRequested);
}

/**
 * @brief 更新按钮状态，无数据时置灰
 */
void WT_DataWidget::updateButtonsState()
{
    bool hasSheet = (ui->tabWidget->count() > 0);
    ui->btnSave->setEnabled(hasSheet);
    ui->btnExport->setEnabled(hasSheet);
    ui->btnUnitManager->setEnabled(hasSheet);
    ui->btnTimeConvert->setEnabled(hasSheet);
    ui->btnPressureDropCalc->setEnabled(hasSheet);
    ui->btnCalcPwf->setEnabled(hasSheet);
    ui->btnFilterSample->setEnabled(hasSheet);
    ui->btnErrorCheck->setEnabled(hasSheet);

    if (auto sheet = currentSheet()) {
        ui->filePathLabel->setText(sheet->getFilePath());
    } else {
        ui->filePathLabel->setText("未加载文件");
    }
}

// =========================================================================
// =                         数据获取与状态代理模块                           =
// =========================================================================

DataSingleSheet* WT_DataWidget::currentSheet() const {
    return qobject_cast<DataSingleSheet*>(ui->tabWidget->currentWidget());
}

QStandardItemModel* WT_DataWidget::getDataModel() const {
    if (auto sheet = currentSheet()) return sheet->getDataModel();
    return nullptr;
}

QMap<QString, QStandardItemModel*> WT_DataWidget::getAllDataModels() const {
    QMap<QString, QStandardItemModel*> map;
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        DataSingleSheet* sheet = qobject_cast<DataSingleSheet*>(ui->tabWidget->widget(i));
        if (sheet) {
            QString key = sheet->getFilePath();
            if (key.isEmpty()) key = ui->tabWidget->tabText(i);
            map.insert(key, sheet->getDataModel());
        }
    }
    return map;
}

QString WT_DataWidget::getCurrentFileName() const {
    if (auto sheet = currentSheet()) return sheet->getFilePath();
    return QString();
}

bool WT_DataWidget::hasData() const {
    return ui->tabWidget->count() > 0;
}

// =========================================================================
// =                            文件 I/O 模块                                =
// =========================================================================

/**
 * @brief 点击打开文件，拉起导入向导
 */
void WT_DataWidget::onOpenFile()
{
    QString filter = "所有支持文件 (*.csv *.txt *.xlsx *.xls);;Excel (*.xlsx *.xls);;CSV 文件 (*.csv);;文本文件 (*.txt)";
    QStringList paths = QFileDialog::getOpenFileNames(this, "打开数据文件", "", filter);
    if (paths.isEmpty()) return;

    for (const QString& path : paths) {
        if (path.endsWith(".json", Qt::CaseInsensitive)) { loadData(path, "json"); return; }

        DataImportDialog dlg(path, this);
        applyDataDialogStyle(&dlg);
        if (dlg.exec() == QDialog::Accepted) {
            createNewTab(path, dlg.getSettings());
        }
    }
}

/**
 * @brief 根据向导创建新表
 */
void WT_DataWidget::createNewTab(const QString& filePath, const DataImportSettings& settings) {
    DataSingleSheet* sheet = new DataSingleSheet(this);
    if (sheet->loadData(filePath, settings)) {
        QFileInfo fi(filePath);
        ui->tabWidget->addTab(sheet, fi.fileName());
        ui->tabWidget->setCurrentWidget(sheet);
        connect(sheet, &DataSingleSheet::dataChanged, this, &WT_DataWidget::onSheetDataChanged);
        updateButtonsState();
        emit fileChanged(filePath, "text");
        emit dataChanged();
    } else {
        delete sheet;
        ui->statusLabel->setText("加载文件失败: " + filePath);
    }
}

void WT_DataWidget::loadData(const QString& filePath, const QString& fileType) {
    if (fileType == "json") return;
    DataImportDialog dlg(filePath, this);
    applyDataDialogStyle(&dlg);
    if (dlg.exec() == QDialog::Accepted) {
        createNewTab(filePath, dlg.getSettings());
    }
}

/**
 * @brief 同步保存：提取表单转化为 JSON 落盘
 */
void WT_DataWidget::onSave() {
    QJsonArray allData;
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        DataSingleSheet* sheet = qobject_cast<DataSingleSheet*>(ui->tabWidget->widget(i));
        if (sheet) allData.append(sheet->saveToJson());
    }
    ModelParameter::instance()->saveTableData(allData);
    ModelParameter::instance()->saveProject();

    Standard_MessageBox::info(this, "保存成功", "所有标签页数据已同步保存到项目文件。");
}

void WT_DataWidget::loadFromProjectData() {
    clearAllData();
    QJsonArray dataArray = ModelParameter::instance()->getTableData();
    if (dataArray.isEmpty()) { ui->statusLabel->setText("无数据"); return; }

    bool isNewFormat = false;
    if (!dataArray.isEmpty()) {
        QJsonValue first = dataArray.first();
        if (first.isObject() && first.toObject().contains("filePath") && first.toObject().contains("data")) isNewFormat = true;
    }

    if (isNewFormat) {
        for (auto val : dataArray) {
            DataSingleSheet* sheet = new DataSingleSheet(this);
            sheet->loadFromJson(val.toObject());
            QFileInfo fi(sheet->getFilePath());
            ui->tabWidget->addTab(sheet, fi.fileName().isEmpty() ? "恢复数据" : fi.fileName());
            connect(sheet, &DataSingleSheet::dataChanged, this, &WT_DataWidget::onSheetDataChanged);
        }
    } else {
        DataSingleSheet* sheet = new DataSingleSheet(this);
        QJsonObject sheetObj; sheetObj["filePath"] = "Restored Data";
        if (!dataArray.isEmpty() && dataArray.first().toObject().contains("headers")) sheetObj["headers"] = dataArray.first().toObject()["headers"];
        QJsonArray rows;
        for (int i = 1; i < dataArray.size(); ++i) {
            QJsonObject rowObj = dataArray[i].toObject();
            if (rowObj.contains("row_data")) rows.append(rowObj["row_data"]);
        }
        sheetObj["data"] = rows;
        sheet->loadFromJson(sheetObj);
        ui->tabWidget->addTab(sheet, "恢复数据");
        connect(sheet, &DataSingleSheet::dataChanged, this, &WT_DataWidget::onSheetDataChanged);
    }
    updateButtonsState();
    ui->statusLabel->setText("数据已恢复");
}

void WT_DataWidget::clearAllData() {
    ui->tabWidget->clear();
    ui->filePathLabel->setText("未加载文件");
    ui->statusLabel->setText("无数据");
    updateButtonsState();
    emit dataChanged();
}

// =========================================================================
// =                            功能计算代理模块                             =
// =========================================================================

void WT_DataWidget::onExportExcel() { if (auto s = currentSheet()) s->onExportExcel(); }
void WT_DataWidget::onTimeConvert() { if (auto s = currentSheet()) s->onTimeConvert(); }
void WT_DataWidget::onPressureDropCalc() { if (auto s = currentSheet()) s->onPressureDropCalc(); }
void WT_DataWidget::onCalcPwf() { if (auto s = currentSheet()) s->onCalcPwf(); }
void WT_DataWidget::onHighlightErrors() { if (auto s = currentSheet()) s->onHighlightErrors(); }

/**
 * @brief 点击单位管理，触发转换
 */
void WT_DataWidget::onUnitManager()
{
    QStandardItemModel* model = getDataModel();
    if (!model || model->rowCount() == 0) { Standard_MessageBox::warning(this, "提示", "当前无数据可操作！"); return; }

    DataUnitDialog dialog(model, this);
    if (dialog.exec() != QDialog::Accepted) return;

    auto tasks = dialog.getConversionTasks();
    if (tasks.isEmpty()) return;

    bool appendMode = dialog.isAppendMode();
    bool saveToFile = dialog.isSaveToFile();
    int rowCount = model->rowCount();
    int colCount = model->columnCount();

    if (!saveToFile) {
        // 在原表修改
        if (appendMode) {
            int insertPos = colCount;
            for (const auto& task : tasks) {
                model->insertColumn(insertPos);
                model->setHeaderData(insertPos, Qt::Horizontal, task.newHeaderName);
                for (int r = 0; r < rowCount; ++r) {
                    QString origText = model->item(r, task.colIndex)->text();
                    if (task.needsMathConversion) {
                        bool ok; double val = origText.toDouble(&ok);
                        if (ok) {
                            double newVal = DataUnitManager::instance()->convert(val, task.quantityType, task.fromUnit, task.toUnit);
                            model->setItem(r, insertPos, new QStandardItem(formatToNormalNumber(newVal)));
                        } else model->setItem(r, insertPos, new QStandardItem(""));
                    } else model->setItem(r, insertPos, new QStandardItem(origText));
                }
                insertPos++;
            }
        } else {
            for (const auto& task : tasks) {
                model->setHeaderData(task.colIndex, Qt::Horizontal, task.newHeaderName);
                if (task.needsMathConversion) {
                    for (int r = 0; r < rowCount; ++r) {
                        QString origText = model->item(r, task.colIndex)->text();
                        bool ok; double val = origText.toDouble(&ok);
                        if (ok) {
                            double newVal = DataUnitManager::instance()->convert(val, task.quantityType, task.fromUnit, task.toUnit);
                            model->item(r, task.colIndex)->setText(formatToNormalNumber(newVal));
                        }
                    }
                }
            }
        }
        emit dataChanged();
    } else {
        // 存为新文件
        QVector<QStringList> fullTable;
        QStringList headers;
        for (int c = 0; c < colCount; ++c) headers.append(model->horizontalHeaderItem(c) ? model->horizontalHeaderItem(c)->text() : "");
        if (appendMode) { for (const auto& task : tasks) headers.append(task.newHeaderName); }
        else { for (const auto& task : tasks) headers[task.colIndex] = task.newHeaderName; }

        for (int r = 0; r < rowCount; ++r) {
            QStringList rowData;
            for (int c = 0; c < colCount; ++c) rowData.append(model->item(r, c)->text());
            if (appendMode) {
                for (const auto& task : tasks) {
                    if (task.needsMathConversion) {
                        bool ok; double val = rowData[task.colIndex].toDouble(&ok);
                        if (ok) {
                            double nV = DataUnitManager::instance()->convert(val, task.quantityType, task.fromUnit, task.toUnit);
                            rowData.append(formatToNormalNumber(nV));
                        } else rowData.append("");
                    } else rowData.append(rowData[task.colIndex]);
                }
            } else {
                for (const auto& task : tasks) {
                    if (task.needsMathConversion) {
                        bool ok; double val = rowData[task.colIndex].toDouble(&ok);
                        if (ok) rowData[task.colIndex] = formatToNormalNumber(DataUnitManager::instance()->convert(val, task.quantityType, task.fromUnit, task.toUnit));
                    }
                }
            }
            fullTable.append(rowData);
        }
        saveAndLoadNewData(getCurrentFileName(), headers, fullTable);
    }
}

// =========================================================================
// =                   悬浮窗模式的滤波与取样控制                             =
// =========================================================================

void WT_DataWidget::onFilterSample() {
    QStandardItemModel* model = getDataModel();
    if (!model || model->rowCount() == 0) { Standard_MessageBox::warning(this, "提示", "当前无数据可处理！"); return; }

    if (!m_samplingWidget) {
        m_samplingWidget = new DataSamplingWidget(this);
        m_samplingWidget->setObjectName("DataSamplingWidget");
        m_samplingWidget->setStyleSheet("#DataSamplingWidget { border-left: 2px solid #1890FF; border-top: 1px solid #D9D9D9; background-color: #FFFFFF; }");

        connect(m_samplingWidget, &DataSamplingWidget::requestMaximize, this, &WT_DataWidget::onSamplingMaximize);
        connect(m_samplingWidget, &DataSamplingWidget::requestRestore, this, &WT_DataWidget::onSamplingRestore);
        connect(m_samplingWidget, &DataSamplingWidget::requestClose, this, &WT_DataWidget::onSamplingClose);
        connect(m_samplingWidget, &DataSamplingWidget::requestResize, this, &WT_DataWidget::onSamplingResized);
        connect(m_samplingWidget, &DataSamplingWidget::processingFinished, this, &WT_DataWidget::onSamplingFinished);
    }
    m_samplingWidget->setModel(model);
    m_samplingWidget->show();
    m_samplingWidget->raise();
    m_isSamplingMaximized = false;
    m_floatingWidth = -1;
    updateSamplingWidgetGeometry();
}

void WT_DataWidget::onSamplingMaximize() { m_isSamplingMaximized = true; updateSamplingWidgetGeometry(); }
void WT_DataWidget::onSamplingRestore() { m_isSamplingMaximized = false; m_floatingWidth = -1; updateSamplingWidgetGeometry(); }
void WT_DataWidget::onSamplingClose() { if (m_samplingWidget) m_samplingWidget->hide(); }

void WT_DataWidget::onSamplingResized(int dx) {
    if (m_isSamplingMaximized) return;
    QRect geom = m_samplingWidget->geometry();
    int newWidth = geom.width() - dx;
    int newX = geom.x() + dx;
    if (newWidth < 400) { newX -= (400 - newWidth); newWidth = 400; }
    QWidget* currentTab = ui->tabWidget->currentWidget();
    QPoint topLeft = currentTab->mapTo(this, QPoint(0, 0));
    if (newX < topLeft.x()) { newWidth -= (topLeft.x() - newX); newX = topLeft.x(); }
    m_floatingWidth = newWidth;
    m_samplingWidget->setGeometry(newX, geom.y(), newWidth, geom.height());
}

void WT_DataWidget::updateSamplingWidgetGeometry() {
    if (!m_samplingWidget || !m_samplingWidget->isVisible()) return;
    QWidget* currentTab = ui->tabWidget->currentWidget();
    if (!currentTab) { m_samplingWidget->hide(); return; }

    QPoint topLeft = currentTab->mapTo(this, QPoint(0, 0));
    QSize tabSize = currentTab->size();

    if (m_isSamplingMaximized) {
        m_samplingWidget->setGeometry(topLeft.x(), topLeft.y(), tabSize.width(), tabSize.height());
    } else {
        int startX = topLeft.x();
        if (m_floatingWidth > 0) {
            startX = topLeft.x() + tabSize.width() - m_floatingWidth;
        } else {
            QTableView* view = currentTab->findChild<QTableView*>();
            if (view) {
                int cols = view->horizontalHeader()->length();
                int vHead = view->verticalHeader()->isVisible() ? view->verticalHeader()->width() : 0;
                int scroll = view->verticalScrollBar()->isVisible() ? view->verticalScrollBar()->width() : 0;
                startX += cols + vHead + scroll + 2;
            } else startX += tabSize.width() / 2;

            if (startX > topLeft.x() + tabSize.width() - 450) startX = topLeft.x() + tabSize.width() - 450;
            m_floatingWidth = topLeft.x() + tabSize.width() - startX;
        }
        int finalWidth = topLeft.x() + tabSize.width() - startX;
        m_samplingWidget->setGeometry(startX, topLeft.y(), finalWidth, tabSize.height());
    }
    m_samplingWidget->raise();
}

void WT_DataWidget::resizeEvent(QResizeEvent *event) { QWidget::resizeEvent(event); updateSamplingWidgetGeometry(); }

// =========================================================================
// =                           事件回调与存取模块                             =
// =========================================================================

void WT_DataWidget::onSamplingFinished(const QStringList& headers, const QVector<QStringList>& processedTable) {
    if (processedTable.isEmpty()) { Standard_MessageBox::warning(this, "错误", "处理后数据为空，请检查参数设置！"); return; }
    saveAndLoadNewData(getCurrentFileName(), headers, processedTable);
}

void WT_DataWidget::saveAndLoadNewData(const QString& oldFilePath, const QStringList& headers, const QVector<QStringList>& fullTable) {
    QFileInfo fi(oldFilePath);
    QString defaultPath = fi.absolutePath() + "/" + QString("%1_处理后_%2.xlsx").arg(fi.completeBaseName()).arg(QDateTime::currentDateTime().toString("HHmmss"));
    QString savePath = QFileDialog::getSaveFileName(this, "保存处理后的全表数据", defaultPath, "Excel文件 (*.xlsx *.xls);;CSV文件 (*.csv);;文本文件 (*.txt)");
    if (savePath.isEmpty()) return;

    bool isExcel = savePath.endsWith(".xlsx", Qt::CaseInsensitive) || savePath.endsWith(".xls", Qt::CaseInsensitive);

    if (isExcel) {
        QXlsx::Document xlsx; QXlsx::Format infoFormat; infoFormat.setFontColor(Qt::darkGray);
        xlsx.write(1, 1, "// PWT System: Data Processed/Standardized Data", infoFormat);
        xlsx.write(2, 1, "// Original File: " + fi.fileName(), infoFormat);
        QXlsx::Format headerFormat; headerFormat.setFontBold(true); headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter); headerFormat.setPatternBackgroundColor(QColor(240, 240, 240));
        for (int c = 0; c < headers.size(); ++c) xlsx.write(3, c + 1, headers[c], headerFormat);
        for (int r = 0; r < fullTable.size(); ++r) {
            for (int c = 0; c < fullTable[r].size(); ++c) {
                bool ok; double val = fullTable[r][c].toDouble(&ok);
                if (ok) xlsx.write(r + 4, c + 1, val); else xlsx.write(r + 4, c + 1, fullTable[r][c]);
            }
        }
        if (!xlsx.saveAs(savePath)) { Standard_MessageBox::error(this, "错误", "无法保存 Excel 文件，请确保文件未被占用。"); return; }
    } else {
        QFile file(savePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { Standard_MessageBox::error(this, "错误", "无法创建或写入文件！"); return; }
        QTextStream out(&file);
        out << "// PWT System: Data Processed/Standardized Data\n// Original File: " << fi.fileName() << "\n" << headers.join(",") << "\n";
        for (const QStringList& row : fullTable) out << row.join(",") << "\n";
        file.close();
    }

    DataImportSettings settings;
    settings.filePath = savePath; settings.encoding = "UTF-8"; settings.separator = ","; settings.useHeader = true; settings.headerRow = 3; settings.startRow = 4; settings.isExcel = isExcel;
    createNewTab(savePath, settings);
    ui->statusLabel->setText("处理完成，保留有效点数: " + QString::number(fullTable.size()));
}

void WT_DataWidget::onTabChanged(int index) {
    Q_UNUSED(index); updateButtonsState();
    if(m_samplingWidget && m_samplingWidget->isVisible()){
        m_samplingWidget->setModel(getDataModel()); updateSamplingWidgetGeometry();
    }
    emit dataChanged();
}

void WT_DataWidget::onTabCloseRequested(int index) {
    QWidget* widget = ui->tabWidget->widget(index);
    if (widget) { ui->tabWidget->removeTab(index); delete widget; }
    updateButtonsState();
    if (!hasData() && m_samplingWidget) m_samplingWidget->hide();
    else if (hasData() && m_samplingWidget && m_samplingWidget->isVisible()){ m_samplingWidget->setModel(getDataModel()); updateSamplingWidgetGeometry(); }
    emit dataChanged();
}

void WT_DataWidget::onSheetDataChanged() { if (sender() == currentSheet()) emit dataChanged(); }
