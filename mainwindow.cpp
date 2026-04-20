/**
 * @file mainwindow.cpp
 * @brief 试井解释软件的主窗口类实现文件
 * @details
 * 【本文件功能与作用】
 * 1. 负责全局的 UI 加载、页面布局和各个子页面的实例化连接。
 * 2. 控制左侧的 NavBtn 侧边栏，实现页面间的流畅切换。
 * 3. 全局应用 Standard_MessageBox 进行各类操作的退出拦截与安全警告。
 * 4. 修复了构造函数初始化列表的顺序警告 (-Wreorder)。
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingbackuplog.h"
#include "settingpaths.h"
#include "settingstartup.h"
#include "navbtn.h"
#include "wt_projectwidget.h"
#include "wt_datawidget.h"
#include "modelmanager.h"
#include "modelparameter.h"
#include "wt_plottingwidget.h"
#include "fittingpage.h"
#include "settingswidget.h"
#include "pressurederivativecalculator.h"
#include "multidatafittingpage.h"
#include "standard_messagebox.h" // 引入全新的全局标准弹窗系统

#include <QDateTime>
#include <QDebug>
#include <QStandardItemModel>
#include <QTimer>
#include <QSpacerItem>
#include <QStackedWidget>
#include <cmath>
#include <QStatusBar>
#include <QShortcut>
#include <QColor>

// ==============================================================================
// 构造与拦截事件
// ==============================================================================

/**
 * @brief 构造函数
 * @details 注意：此处的初始化列表顺序 m_hasValidData, m_isProjectLoaded
 * 必须与 mainwindow.h 中声明的顺序绝对一致，以消除 -Wreorder 警告。
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_hasValidData(false),
    m_isProjectLoaded(false)
{
    ui->setupUi(this);
    this->setWindowTitle("PWT页岩油多段压裂水平井试井解释软件");
    this->setMinimumSize(1024, 768);
    applyMainWindowBackgrounds();
    init();
}

MainWindow::~MainWindow() {
    delete ui;
}

/**
 * @brief 全局退出拦截：采用全新的三按钮 Standard_MessageBox
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_isProjectLoaded) {
        event->accept();
        return;
    }

    // 调用 Standard_MessageBox 的 3 按钮模式
    int res = Standard_MessageBox::question3(this, "退出系统",
                                             "当前有项目正在运行，确定要退出吗？\n建议在退出前保存当前进度。",
                                             "保存并退出", "直接退出", "取消");

    if (res == 0) { // 保存并退出
        ModelParameter::instance()->saveProject();
        event->accept();
    } else if (res == 1) { // 直接退出
        event->accept();
    } else { // 取消
        event->ignore();
    }
}

// ==============================================================================
// 系统初始化
// ==============================================================================

void MainWindow::applyMainWindowBackgrounds()
{
    const QString textStyle = QStringLiteral("color: #333333; font: 75 8pt \"微软雅黑\";");
    const QColor navColor = readColorSetting(QStringLiteral("appearance/navBackground"), QColor(QStringLiteral("#FFE0B2")));
    const QColor contentColor = readColorSetting(QStringLiteral("appearance/contentBackground"), QColor(QStringLiteral("#E1F5FE")));
    ui->centralwidget->setStyleSheet(textStyle);
    ui->NavigationBar->setStyleSheet(QStringLiteral("background-color: %1; %2").arg(navColor.name(), textStyle));
    ui->stackedWidget->setStyleSheet(QStringLiteral("QStackedWidget#stackedWidget { background-color: %1; }").arg(contentColor.name()));
}

QColor MainWindow::readColorSetting(const QString& key, const QColor& fallback) const
{
    QSettings settings(QStringLiteral("WellTestPro"), QStringLiteral("WellTestAnalysis"));
    const QColor color(settings.value(key, fallback.name()).toString());
    return color.isValid() ? color : fallback;
}

void MainWindow::init()
{
    QStringList names = {tr("项目"), tr("数据"), tr("图表"), tr("模型"), tr("拟合"), tr("多数据"), tr("设置")};
    QStringList icons = { ":/new/prefix1/Resource/X0.png", ":/new/prefix1/Resource/X1.png", ":/new/prefix1/Resource/X2.png", ":/new/prefix1/Resource/X3.png", ":/new/prefix1/Resource/X4.png", ":/new/prefix1/Resource/X5.png", ":/new/prefix1/Resource/X6.png" };

    for(int i = 0 ; i < 7; i++) {
        NavBtn* btn = new NavBtn(ui->widgetNav);
        btn->setPicName(icons[i], names[i]);
        btn->setIndex(i);
        if (i == 0) { btn->setClickedStyle(); ui->stackedWidget->setCurrentIndex(0); } else { btn->setNormalStyle(); }

        m_NavBtnMap.insert(btn->getName(), btn);
        ui->verticalLayoutNav->addWidget(btn);

        connect(btn, &NavBtn::sigClicked, this, [=](QString name) {
            int targetIndex = m_NavBtnMap.value(name)->getIndex();
            if ((targetIndex >= 1 && targetIndex <= 5) && !m_isProjectLoaded) {
                Standard_MessageBox::warning(this, "提示", "当前无活动项目，请先在“项目”界面新建或打开一个项目！");
                m_NavBtnMap.value(name)->setNormalStyle();
                updateNavigationState();
                return;
            }
            QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
            while (item != m_NavBtnMap.end()) { if(item.key() != name) ((NavBtn*)(item.value()))->setNormalStyle(); item++; }
            ui->stackedWidget->setCurrentIndex(targetIndex);
            if (name == tr("图表")) onTransferDataToPlotting();
        });
    }

    QSpacerItem* verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    ui->verticalLayoutNav->addSpacerItem(verticalSpacer);

    ui->labelTime->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss").replace(" ","\n"));
    connect(&m_timer, &QTimer::timeout, [=] { ui->labelTime->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").replace(" ","\n")); ui->labelTime->setStyleSheet("color: black; font-weight: bold;"); });
    m_timer.start(1000);

    // 子页面装载
    m_ProjectWidget = new WT_ProjectWidget(ui->pageMonitor); ui->verticalLayoutMonitor->addWidget(m_ProjectWidget);
    connect(m_ProjectWidget, &WT_ProjectWidget::projectOpened, this, &MainWindow::onProjectOpened);
    connect(m_ProjectWidget, &WT_ProjectWidget::projectClosed, this, &MainWindow::onProjectClosed);
    connect(m_ProjectWidget, &WT_ProjectWidget::fileLoaded, this, &MainWindow::onFileLoaded);

    m_DataEditorWidget = new WT_DataWidget(ui->pageHand); ui->verticalLayoutHandle->addWidget(m_DataEditorWidget);
    connect(m_DataEditorWidget, &WT_DataWidget::fileChanged, this, &MainWindow::onFileLoaded);
    connect(m_DataEditorWidget, &WT_DataWidget::dataChanged, this, &MainWindow::onDataEditorDataChanged);

    m_PlottingWidget = new WT_PlottingWidget(ui->pageData); ui->verticalLayout_2->addWidget(m_PlottingWidget);
    connect(m_PlottingWidget, &WT_PlottingWidget::viewExportedFile, this, &MainWindow::onViewExportedFile);

    m_ModelManager = new ModelManager(this); m_ModelManager->initializeModels(ui->pageParamter);
    connect(m_ModelManager, &ModelManager::calculationCompleted, this, &MainWindow::onModelCalculationCompleted);

    if (ui->pageFitting && ui->verticalLayoutFitting) { m_FittingPage = new FittingPage(ui->pageFitting); ui->verticalLayoutFitting->addWidget(m_FittingPage); m_FittingPage->setModelManager(m_ModelManager); }
    if (ui->pagePrediction && ui->verticalLayoutPrediction) { multidatafittingpage* multiPage = new multidatafittingpage(ui->pagePrediction); multiPage->setObjectName("MultiDataFittingPage"); ui->verticalLayoutPrediction->addWidget(multiPage); multiPage->setModelManager(m_ModelManager); }

    m_SettingsWidget = new SettingsWidget(ui->pageAlarm); ui->verticalLayout_3->addWidget(m_SettingsWidget);
    connect(m_SettingsWidget, &SettingsWidget::settingsChanged, this, &MainWindow::onSystemSettingsChanged);
    connect(m_SettingsWidget, &SettingsWidget::appearanceChanged, this, [this](const QColor&, const QColor&) {
        applyMainWindowBackgrounds();
    });

    connect(&m_autoSaveTimer, &QTimer::timeout, this, [this]() {
        if (m_isProjectLoaded) {
            ModelParameter::instance()->saveProject();

            if (SettingBackupLog::backupEnabled()) {
                const QString projectFile = ModelParameter::instance()->getProjectFilePath();
                if (!projectFile.isEmpty() && QFileInfo::exists(projectFile)) {
                    const QString backupDir = SettingPaths::backupPath();
                    QDir().mkpath(backupDir);
                    QFileInfo info(projectFile);
                    const QString backupFile = backupDir + "/" + info.completeBaseName() + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".pwt";
                    QFile::copy(projectFile, backupFile);

                    QDir dir(backupDir);
                    QStringList filters; filters << info.completeBaseName() + "_*.pwt";
                    QFileInfoList backups = dir.entryInfoList(filters, QDir::Files, QDir::Time);
                    for (int i = SettingBackupLog::maxBackups(); i < backups.size(); ++i) {
                        QFile::remove(backups[i].absoluteFilePath());
                    }
                }
            }

            if (SettingBackupLog::cleanupLogs()) {
                const QString reportDir = SettingPaths::reportPath();
                QDir dir(reportDir);
                QFileInfoList logs = dir.entryInfoList(QStringList() << "*.log", QDir::Files, QDir::Time);
                const QDateTime cutoff = QDateTime::currentDateTime().addDays(-SettingBackupLog::logRetentionDays());
                for (const QFileInfo& logInfo : logs) {
                    if (logInfo.lastModified() < cutoff) {
                        QFile::remove(logInfo.absoluteFilePath());
                    }
                }
            }
        }
    });

    onSystemSettingsChanged();

    auto switchToPage = [this](int index) {
        if (index < 0 || index >= ui->stackedWidget->count()) {
            return;
        }
        if ((index >= 1 && index <= 5) && !m_isProjectLoaded) {
            return;
        }
        ui->stackedWidget->setCurrentIndex(index);
        updateNavigationState();
        if (index == 2) {
            onTransferDataToPlotting();
        }
    };

    new QShortcut(QKeySequence::Save, this, [this]() {
        if (m_isProjectLoaded) {
            ModelParameter::instance()->saveProject();
        }
    });

    new QShortcut(QKeySequence::Open, this, [switchToPage]() {
        switchToPage(0);
    });

    new QShortcut(QKeySequence::New, this, [switchToPage]() {
        switchToPage(0);
    });

    for (int i = 0; i < 7; ++i) {
        new QShortcut(QKeySequence(QString("Ctrl+%1").arg(i + 1)), this, [switchToPage, i]() {
            switchToPage(i);
        });
    }
}

// 预留接口
void MainWindow::initProjectForm() {} void MainWindow::initDataEditorForm() {} void MainWindow::initModelForm() {} void MainWindow::initPlottingForm() {} void MainWindow::initFittingForm() {} void MainWindow::initPredictionForm() {}

// ==============================================================================
// 数据流转与业务操作
// ==============================================================================

void MainWindow::onProjectOpened(bool isNew)
{
    m_isProjectLoaded = true;
    if (m_ModelManager) m_ModelManager->updateAllModelsBasicParameters();

    if (m_DataEditorWidget) {
        if (!isNew) m_DataEditorWidget->loadFromProjectData();
        if (m_FittingPage) m_FittingPage->setProjectDataModels(m_DataEditorWidget->getAllDataModels());
        multidatafittingpage* multiPage = ui->pagePrediction->findChild<multidatafittingpage*>("MultiDataFittingPage");
        if (multiPage) multiPage->setProjectDataModels(m_DataEditorWidget->getAllDataModels());
    }

    if (m_FittingPage) { m_FittingPage->updateBasicParameters(); m_FittingPage->loadAllFittingStates(); }
    if (m_PlottingWidget) m_PlottingWidget->loadProjectData();
    updateNavigationState();

    QString title = isNew ? "新建项目成功" : "加载项目成功";
    QString text = isNew ? "新项目已成功创建并载入引擎。\n您可以开始进行数据录入或模型计算。"
                         : "项目文件加载完成。\n历史参数、数据及图表分析状态已完整恢复。";
    Standard_MessageBox::info(this, title, text);
}

void MainWindow::onProjectClosed()
{
    m_isProjectLoaded = false; m_hasValidData = false;
    if (m_DataEditorWidget) m_DataEditorWidget->clearAllData();
    if (m_PlottingWidget) m_PlottingWidget->clearAllPlots();
    if (m_FittingPage) m_FittingPage->resetAnalysis();
    if (m_ModelManager) m_ModelManager->clearCache();

    ModelParameter::instance()->resetAllData();
    multidatafittingpage* multiPage = ui->pagePrediction->findChild<multidatafittingpage*>("MultiDataFittingPage");
    if (multiPage) multiPage->setProjectDataModels(QMap<QString, QStandardItemModel*>());

    ui->stackedWidget->setCurrentIndex(0);
    updateNavigationState();
}

void MainWindow::onFileLoaded(const QString& filePath, const QString& fileType)
{
    if (!m_isProjectLoaded) {
        Standard_MessageBox::warning(this, "警告", "请先创建或打开项目！");
        return;
    }
    ui->stackedWidget->setCurrentIndex(1);
    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
    while (item != m_NavBtnMap.end()) {
        ((NavBtn*)(item.value()))->setNormalStyle();
        if(item.key() == tr("数据")) ((NavBtn*)(item.value()))->setClickedStyle();
        item++;
    }
    if (m_DataEditorWidget && sender() != m_DataEditorWidget) { m_DataEditorWidget->loadData(filePath, fileType); }
    if (m_DataEditorWidget) {
        if (m_FittingPage) m_FittingPage->setProjectDataModels(m_DataEditorWidget->getAllDataModels());
        multidatafittingpage* multiPage = ui->pagePrediction->findChild<multidatafittingpage*>("MultiDataFittingPage");
        if (multiPage) multiPage->setProjectDataModels(m_DataEditorWidget->getAllDataModels());
    }
    m_hasValidData = true; QTimer::singleShot(1000, this, &MainWindow::onDataReadyForPlotting);
}

void MainWindow::onViewExportedFile(const QString& filePath)
{
    ui->stackedWidget->setCurrentIndex(1);
    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
    while (item != m_NavBtnMap.end()) {
        ((NavBtn*)(item.value()))->setNormalStyle();
        if(item.key() == tr("数据")) ((NavBtn*)(item.value()))->setClickedStyle();
        item++;
    }
    if (m_DataEditorWidget) m_DataEditorWidget->loadData(filePath, "auto");
}

void MainWindow::onPlotAnalysisCompleted(const QString &, const QMap<QString, double> &) { }
void MainWindow::onDataReadyForPlotting() { transferDataFromEditorToPlotting(); }
void MainWindow::onTransferDataToPlotting() { if (hasDataLoaded()) transferDataFromEditorToPlotting(); }
void MainWindow::onDataEditorDataChanged() { if (ui->stackedWidget->currentIndex() == 2) { transferDataFromEditorToPlotting(); } m_hasValidData = hasDataLoaded(); }
void MainWindow::onModelCalculationCompleted(const QString &, const QMap<QString, double> &) { }

void MainWindow::transferDataToFitting() {
    if (!m_FittingPage || !m_DataEditorWidget) return;
    QStandardItemModel* model = m_DataEditorWidget->getDataModel(); if (!model || model->rowCount() == 0) return;
    QVector<double> tVec, pVec, dVec; double p_initial = 0.0;
    for(int r = 0; r < model->rowCount(); ++r) { double p = model->index(r, 1).data().toDouble(); if (std::abs(p) > 1e-6) { p_initial = p; break; } }
    for(int r = 0; r < model->rowCount(); ++r) { double t = model->index(r, 0).data().toDouble(); double p_raw = model->index(r, 1).data().toDouble(); if (t > 0) { tVec.append(t); pVec.append(std::abs(p_raw - p_initial)); } }
    if (tVec.size() > 2) { dVec = PressureDerivativeCalculator::calculateBourdetDerivative(tVec, pVec, 0.1); } else { dVec.resize(tVec.size()); dVec.fill(0.0); }
    m_FittingPage->setObservedDataToCurrent(tVec, pVec, dVec);
}

void MainWindow::onFittingProgressChanged(int progress) {
    if (this->statusBar()) { this->statusBar()->showMessage(QString("正在拟合... %1%").arg(progress)); if(progress >= 100) this->statusBar()->showMessage("拟合完成", 5000); }
}

void MainWindow::onSystemSettingsChanged()
{
    if (!m_SettingsWidget) {
        m_autoSaveTimer.stop();
        return;
    }

    applyMainWindowBackgrounds();

    int autoSaveMinutes = m_SettingsWidget->getAutoSaveInterval();
    if (autoSaveMinutes <= 0) {
        m_autoSaveTimer.stop();
    } else {
        m_autoSaveTimer.start(autoSaveMinutes * 60 * 1000);
    }

    if (m_PlottingWidget && ui->stackedWidget->currentIndex() == 2) {
        QList<ChartWidget*> chartWidgets = m_PlottingWidget->findChildren<ChartWidget*>();
        for (ChartWidget* chartWidget : chartWidgets) {
            chartWidget->applyGlobalPlotDefaults();
        }
    }

    if (this->statusBar()) {
        this->statusBar()->showMessage(QStringLiteral("设置已更新"), 3000);
    }
}
void MainWindow::onPerformanceSettingsChanged() {}

QStandardItemModel* MainWindow::getDataEditorModel() const { return m_DataEditorWidget ? m_DataEditorWidget->getDataModel() : nullptr; }
QString MainWindow::getCurrentFileName() const { return m_DataEditorWidget ? m_DataEditorWidget->getCurrentFileName() : QString(); }
bool MainWindow::hasDataLoaded() { return m_DataEditorWidget ? m_DataEditorWidget->hasData() : false; }

void MainWindow::transferDataFromEditorToPlotting() {
    if (!m_DataEditorWidget || !m_PlottingWidget) return;
    QMap<QString, QStandardItemModel*> models = m_DataEditorWidget->getAllDataModels();
    m_PlottingWidget->setDataModels(models);
    if (!models.isEmpty()) m_hasValidData = true;
}

void MainWindow::updateNavigationState() {
    int currentIndex = ui->stackedWidget->currentIndex();
    QMap<QString,NavBtn*>::Iterator item = m_NavBtnMap.begin();
    while (item != m_NavBtnMap.end()) {
        if(((NavBtn*)(item.value()))->getIndex() == currentIndex) { ((NavBtn*)(item.value()))->setClickedStyle(); } else { ((NavBtn*)(item.value()))->setNormalStyle(); } item++;
    }
}
