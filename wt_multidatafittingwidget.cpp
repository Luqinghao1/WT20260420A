/**
 * @file wt_multidatafittingwidget.cpp
 * @brief 多数据试井拟合分析核心界面实现文件
 *
 * 本代码文件的作用与功能：
 * 1. 提供多数据试井拟合的具体管理逻辑及图表可视化显示。
 * 2. 【核心重构】配合动态标量展开方案，彻底删除了所有带括号字符串数组的解析与校验逻辑！
 * 3. 移除了数组数量与 nf 的校验代码，参数表格现已自动生成独立的 Lf_i 与 xf_i 标量，直接传递给核心引擎即可。
 * 4. 接入 ModelSelect::getShortName 统一规范界面文案显示，全面支持 324 种模型。
 * 5. 参数处理核心平滑应用了 k2 (外区渗透率) 和 M12 的互相转化机制。
 */

#include "wt_multidatafittingwidget.h"
#include "ui_wt_multidatafittingwidget.h"
#include "fittingdatadialog.h"
#include "fittingsamplingdialog.h"
#include "modelselect.h"
#include "paramselectdialog.h"
#include "pressurederivativecalculator.h"
#include "pressurederivativecalculator1.h"
#include "modelparameter.h"

#include "standard_messagebox.h"
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <QHeaderView>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QColorDialog>
#include <QMenu>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QApplication>
#include <QSlider>
#include <QLabel>
#include <QPushButton>

WT_MultidataFittingWidget::WT_MultidataFittingWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WT_MultidataFittingWidget),
    m_modelManager(nullptr),
    m_core(new FittingCore(this)),
    m_chartManager(new FittingChart(this)),
    m_mdiArea(nullptr),
    m_chartLogLog(nullptr),
    m_chartSemiLog(nullptr),
    m_chartCartesian(nullptr),
    m_currentModelType(ModelManager::Model_1),
    m_isFitting(false),
    m_isUpdatingTable(false),
    m_useLimits(false),
    m_useMultiStart(false),
    m_userDefinedTimeMax(-1.0),
    m_initialSSE(0.0),
    m_iterCount(0)
{
    ui->setupUi(this);

    if (ui->plotContainer->layout()) {
        QLayoutItem* item;
        while ((item = ui->plotContainer->layout()->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete ui->plotContainer->layout();
    }

    QVBoxLayout* containerLayout = new QVBoxLayout(ui->plotContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    m_mdiArea = new QMdiArea(this);
    m_mdiArea->setViewMode(QMdiArea::SubWindowView);
    m_mdiArea->setBackground(QBrush(Qt::white));
    m_mdiArea->viewport()->setAutoFillBackground(true);
    m_mdiArea->viewport()->setStyleSheet("background-color: white;");
    containerLayout->addWidget(m_mdiArea);

    m_chartLogLog = new FittingChart1(this);
    m_chartSemiLog = new FittingChart2(this);
    m_chartCartesian = new FittingChart3(this);

    m_plotLogLog = m_chartLogLog->getPlot();
    m_plotSemiLog = m_chartSemiLog->getPlot();
    m_plotCartesian = m_chartCartesian->getPlot();

    m_chartLogLog->setTitle("双对数曲线 (Log-Log)");
    m_chartSemiLog->setTitle("半对数曲线 (Semi-Log)");
    m_chartCartesian->setTitle("历史拟合曲线 (History Plot)");

    m_subWinLogLog = m_mdiArea->addSubWindow(m_chartLogLog);
    m_subWinSemiLog = m_mdiArea->addSubWindow(m_chartSemiLog);
    m_subWinCartesian = m_mdiArea->addSubWindow(m_chartCartesian);

    ui->splitter->setSizes(QList<int>() << 350 << 1000);
    ui->splitter->setCollapsible(0, false);

    m_paramChart = new FittingParameterChart(ui->tableParams, this);
    setupPlot();
    m_chartManager->initializeCharts(m_plotLogLog, m_plotSemiLog, m_plotCartesian);

    ui->tableDataGroups->setColumnCount(5);
    ui->tableDataGroups->setHorizontalHeaderLabels({"数据名称", "颜色", "压差", "压力导数", "权重"});
    ui->tableDataGroups->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tableDataGroups->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableDataGroups->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tableDataGroups->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->tableDataGroups->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->tableDataGroups->setFocusPolicy(Qt::NoFocus);
    ui->tableDataGroups->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tableDataGroups, &QTableWidget::cellChanged, this, &WT_MultidataFittingWidget::onDataWeightChanged);
    connect(ui->tableDataGroups, &QTableWidget::cellClicked, this, &WT_MultidataFittingWidget::onTableCellClicked);
    connect(ui->tableDataGroups, &QTableWidget::customContextMenuRequested, this, &WT_MultidataFittingWidget::showContextMenu);
    connect(m_paramChart, &FittingParameterChart::parameterChangedByWheel, this, &WT_MultidataFittingWidget::onParameterChangedByWheel);
    connect(ui->tableParams, &QTableWidget::itemChanged, this, &WT_MultidataFittingWidget::onParameterTableItemChanged);
    connect(m_core, &FittingCore::sigIterationUpdated, this, &WT_MultidataFittingWidget::onIterationUpdate, Qt::QueuedConnection);
    connect(m_core, &FittingCore::sigProgress, ui->progressBar, &QProgressBar::setValue);
    connect(m_core, &FittingCore::sigFitFinished, this, &WT_MultidataFittingWidget::onFitFinished);
    connect(m_chartLogLog, &FittingChart1::exportDataTriggered, this, &WT_MultidataFittingWidget::onExportCurveData);
    connect(m_chartSemiLog, &FittingChart2::exportDataTriggered, this, &WT_MultidataFittingWidget::onExportCurveData);
    connect(m_chartCartesian, &FittingChart3::exportDataTriggered, this, &WT_MultidataFittingWidget::onExportCurveData);
    connect(m_chartManager, &FittingChart::sigManualPressureUpdated, this, &WT_MultidataFittingWidget::onSemiLogLineMoved);

    // 权重与拟合控制按钮
    QLabel* lblWeightTitle = new QLabel("拟合权重调节:", this);
    QHBoxLayout* weightLayout = new QHBoxLayout();
    QLabel* lblDeriv = new QLabel("导数权重", this);

    QSlider* sliderWeight = new QSlider(Qt::Horizontal, this);
    sliderWeight->setObjectName("sliderWeight");
    sliderWeight->setRange(0, 100);
    sliderWeight->setValue(50);

    QLabel* lblPress = new QLabel("压力权重", this);
    weightLayout->addWidget(lblDeriv);
    weightLayout->addWidget(sliderWeight);
    weightLayout->addWidget(lblPress);

    int comboIdx = -1;
    for (int i = 0; i < ui->verticalLayout_Left->count(); ++i) {
        QWidget* w = ui->verticalLayout_Left->itemAt(i)->widget();
        if (w == ui->comboFitMode) {
            comboIdx = i;
            break;
        }
    }

    if (comboIdx >= 0) {
        ui->verticalLayout_Left->insertWidget(comboIdx, lblWeightTitle);
        ui->verticalLayout_Left->insertLayout(comboIdx + 1, weightLayout);
    }
    ui->comboFitMode->hide();

    QCheckBox* cbMultiStart = new QCheckBox("多起点优化", this);
    cbMultiStart->setToolTip("从多个随机起点中选择最佳出发点，降低局部最优概率");
    cbMultiStart->setChecked(false);
    connect(cbMultiStart, &QCheckBox::toggled, this, [this](bool checked) {
        m_useMultiStart = checked;
    });

    QCheckBox* cbUseLimits = new QCheckBox("参数限幅", this);
    cbUseLimits->setToolTip("在拟合过程中强制参数在上下限范围内");
    cbUseLimits->setChecked(false);
    connect(cbUseLimits, &QCheckBox::toggled, this, [this](bool checked) {
        m_useLimits = checked;
    });

    int runBtnIdx = -1;
    for (int i = 0; i < ui->verticalLayout_Left->count(); ++i) {
        QLayoutItem* item = ui->verticalLayout_Left->itemAt(i);
        if (item->layout() && item->layout()->indexOf(ui->btnRunFit) >= 0) {
            runBtnIdx = i;
            break;
        }
    }

    if (runBtnIdx >= 0) {
        QHBoxLayout* optLayout = new QHBoxLayout();
        optLayout->addWidget(cbMultiStart);
        optLayout->addWidget(cbUseLimits);
        ui->verticalLayout_Left->insertLayout(runBtnIdx, optLayout);
    }
}

WT_MultidataFittingWidget::~WT_MultidataFittingWidget()
{
    delete ui;
}

void WT_MultidataFittingWidget::on_btnSave_clicked()
{
    if (m_dataGroups.isEmpty()) {
        Standard_MessageBox::info(this, "提示", "当前没有分析数据可保存。");
        return;
    }
    m_paramChart->updateParamsFromTable();
    loadProjectParams();
    Standard_MessageBox::info(this, "保存成功", "当前多数据分析与拟合参数状态已保存。");
}

void WT_MultidataFittingWidget::setModelManager(ModelManager *m)
{
    m_modelManager = m;
    m_paramChart->setModelManager(m);
    if (m_core) m_core->setModelManager(m);
    initializeDefaultModel();
}

void WT_MultidataFittingWidget::setProjectDataModels(const QMap<QString, QStandardItemModel *> &models)
{
    m_dataMap = models;
}

void WT_MultidataFittingWidget::setupPlot()
{
    m_plotLogLog->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    m_plotSemiLog->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    m_plotCartesian->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
}

void WT_MultidataFittingWidget::initializeDefaultModel()
{
    if(!m_modelManager) return;
    m_isUpdatingTable = true;
    m_currentModelType = ModelManager::Model_1;

    ui->btn_modelSelect->setText(ModelSelect::getShortName((int)m_currentModelType + 1));

    m_paramChart->resetParams(m_currentModelType, true);
    loadProjectParams();
    hideUnwantedParams();
    m_isUpdatingTable = false;
}

void WT_MultidataFittingWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutCharts();
}

void WT_MultidataFittingWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    layoutCharts();
}

void WT_MultidataFittingWidget::layoutCharts()
{
    if (!m_mdiArea || !m_subWinLogLog || !m_subWinSemiLog || !m_subWinCartesian) return;
    QRect rect = m_mdiArea->contentsRect();
    int w = rect.width();
    int h = rect.height();
    if (w <= 0 || h <= 0) return;

    m_subWinLogLog->setGeometry(0, 0, w / 2, h);
    m_subWinCartesian->setGeometry(w / 2, 0, w - (w / 2), h / 2);
    m_subWinSemiLog->setGeometry(w / 2, h / 2, w - (w / 2), h - (h / 2));
}

void WT_MultidataFittingWidget::onParameterChangedByWheel()
{
    if (m_isUpdatingTable || m_isFitting) return;
    m_paramChart->updateParamsFromTable();
    updateModelCurve(nullptr, false, false);
}

void WT_MultidataFittingWidget::onParameterTableItemChanged(QTableWidgetItem *item)
{
    if (m_isUpdatingTable || m_isFitting || !item) return;
    if (item->column() >= 2 && item->column() <= 4) {
        m_paramChart->updateParamsFromTable();
        updateModelCurve(nullptr, false, false);
    }
}

void WT_MultidataFittingWidget::on_btnAddData_clicked()
{
    if (m_dataMap.isEmpty()) {
        Standard_MessageBox::warning(this, "提示", "未检测到项目数据源，请确保是从已加载项目的状态进入。");
    }

    FittingDataDialog dlg(m_dataMap, this);
    if (dlg.exec() != QDialog::Accepted) return;

    FittingDataSettings settings = dlg.getSettings();
    QStandardItemModel* sourceModel = dlg.getPreviewModel();
    if (!sourceModel || sourceModel->rowCount() == 0) return;

    QVector<double> rawTime, rawPressureData, finalDeriv;
    int skip = settings.skipRows;

    for (int i = skip; i < sourceModel->rowCount(); ++i) {
        QStandardItem* itemT = sourceModel->item(i, settings.timeColIndex);
        QStandardItem* itemP = sourceModel->item(i, settings.pressureColIndex);

        if (itemT && itemP) {
            double t = itemT->text().toDouble();
            double p = itemP->text().toDouble();

            if (t > 0) {
                rawTime.append(t);
                rawPressureData.append(p);
                if (settings.derivColIndex >= 0) {
                    QStandardItem* itemD = sourceModel->item(i, settings.derivColIndex);
                    finalDeriv.append(itemD ? itemD->text().toDouble() : 0.0);
                }
            }
        }
    }

    QVector<double> finalDeltaP;
    double p_shutin = rawPressureData.first();

    for (double p : rawPressureData) {
        if (settings.testType == Test_Drawdown) {
            finalDeltaP.append(std::abs(settings.initialPressure - p));
        } else {
            finalDeltaP.append(std::abs(p - p_shutin));
        }
    }

    if (settings.derivColIndex == -1) {
        finalDeriv = PressureDerivativeCalculator::calculateBourdetDerivative(rawTime, finalDeltaP, settings.lSpacing);
    }

    if (settings.enableSmoothing) {
        finalDeriv = PressureDerivativeCalculator1::smoothData(finalDeriv, settings.smoothingSpan);
    }

    QString defaultName = settings.isFromProject ? QFileInfo(settings.projectFileName).completeBaseName() : QFileInfo(settings.filePath).completeBaseName();
    if (defaultName.isEmpty()) {
        defaultName = QString("数据组 %1").arg(m_dataGroups.size() + 1);
    }

    DataGroup group;
    group.groupName = defaultName;
    group.color = getColor(m_dataGroups.size());
    group.weight = 1.0;
    group.showDeltaP = true;
    group.showDerivative = true;
    group.time = rawTime;
    group.deltaP = finalDeltaP;
    group.derivative = finalDeriv;
    group.origTime = rawTime;
    group.origDeltaP = finalDeltaP;
    group.origDerivative = finalDeriv;
    group.rawP = rawPressureData;

    m_dataGroups.append(group);

    for (int i = 0; i < m_dataGroups.size(); ++i) {
        m_dataGroups[i].weight = 1.0 / m_dataGroups.size();
    }

    refreshDataTable();
    updateModelCurve(nullptr, true, false);
}

void WT_MultidataFittingWidget::refreshDataTable()
{
    ui->tableDataGroups->blockSignals(true);
    ui->tableDataGroups->clearContents();
    ui->tableDataGroups->setRowCount(m_dataGroups.size());

    for(int i = 0; i < m_dataGroups.size(); ++i) {
        const DataGroup& g = m_dataGroups[i];
        ui->tableDataGroups->setItem(i, 0, new QTableWidgetItem(g.groupName));

        QTableWidgetItem* colorItem = new QTableWidgetItem();
        colorItem->setBackground(QBrush(g.color));
        colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
        ui->tableDataGroups->setItem(i, 1, colorItem);

        QWidget* pWidgetP = new QWidget();
        pWidgetP->setStyleSheet("background: transparent; border: none;");
        QHBoxLayout* pLayoutP = new QHBoxLayout(pWidgetP);
        pLayoutP->setContentsMargins(0, 0, 0, 0);

        QCheckBox* cbDeltaP = new QCheckBox();
        cbDeltaP->setStyleSheet("background: transparent; border: none;");
        cbDeltaP->setChecked(g.showDeltaP);
        pLayoutP->addWidget(cbDeltaP);
        pLayoutP->setAlignment(Qt::AlignCenter);
        ui->tableDataGroups->setCellWidget(i, 2, pWidgetP);

        connect(cbDeltaP, &QCheckBox::checkStateChanged, this, [this, cbDeltaP](Qt::CheckState state) {
            for(int r = 0; r < ui->tableDataGroups->rowCount(); ++r) {
                QWidget* w = ui->tableDataGroups->cellWidget(r, 2);
                if (w && w->isAncestorOf(cbDeltaP)) {
                    m_dataGroups[r].showDeltaP = (state == Qt::Checked);
                    updateModelCurve(nullptr, false, false);
                    break;
                }
            }
        });

        QWidget* pWidgetD = new QWidget();
        pWidgetD->setStyleSheet("background: transparent; border: none;");
        QHBoxLayout* pLayoutD = new QHBoxLayout(pWidgetD);
        pLayoutD->setContentsMargins(0, 0, 0, 0);

        QCheckBox* cbDeriv = new QCheckBox();
        cbDeriv->setStyleSheet("background: transparent; border: none;");
        cbDeriv->setChecked(g.showDerivative);
        pLayoutD->addWidget(cbDeriv);
        pLayoutD->setAlignment(Qt::AlignCenter);
        ui->tableDataGroups->setCellWidget(i, 3, pWidgetD);

        connect(cbDeriv, &QCheckBox::checkStateChanged, this, [this, cbDeriv](Qt::CheckState state) {
            for(int r = 0; r < ui->tableDataGroups->rowCount(); ++r) {
                QWidget* w = ui->tableDataGroups->cellWidget(r, 3);
                if (w && w->isAncestorOf(cbDeriv)) {
                    m_dataGroups[r].showDerivative = (state == Qt::Checked);
                    updateModelCurve(nullptr, false, false);
                    break;
                }
            }
        });

        ui->tableDataGroups->setItem(i, 4, new QTableWidgetItem(QString::number(g.weight, 'f', 2)));
    }
    ui->tableDataGroups->blockSignals(false);
}

void WT_MultidataFittingWidget::onTableCellClicked(int row, int col)
{
    if (col == 1) {
        QColor newColor = QColorDialog::getColor(m_dataGroups[row].color, this, "选择数据曲线颜色");
        if (newColor.isValid()) {
            m_dataGroups[row].color = newColor;
            ui->tableDataGroups->item(row, col)->setBackground(QBrush(newColor));
            updateModelCurve(nullptr, false, false);
        }
    }
}

void WT_MultidataFittingWidget::onDataWeightChanged(int row, int col)
{
    if (col == 0) {
        m_dataGroups[row].groupName = ui->tableDataGroups->item(row, col)->text();
    } else if (col == 4) {
        bool ok;
        double newWeight = ui->tableDataGroups->item(row, col)->text().toDouble(&ok);
        if(ok && newWeight >= 0 && newWeight <= 1.0) {
            m_dataGroups[row].weight = newWeight;
        } else {
            ui->tableDataGroups->item(row, col)->setText(QString::number(m_dataGroups[row].weight, 'f', 2));
            Standard_MessageBox::warning(this, "权重输入错误", "权重必须在0到1之间！");
        }
    }
}

void WT_MultidataFittingWidget::showContextMenu(const QPoint &pos)
{
    int row = ui->tableDataGroups->rowAt(pos.y());
    if (row >= 0 && row < m_dataGroups.size()) {
        QMenu menu(this);
        menu.setStyleSheet("QMenu { background-color: white; color: #333333; border: 1px solid gray; } QMenu::item:selected { background-color: #e0e0e0; }");
        QAction* delAct = menu.addAction("删除本行数据");

        if (menu.exec(ui->tableDataGroups->viewport()->mapToGlobal(pos)) == delAct) {
            if (Standard_MessageBox::question(this, "确认删除", QString("确定要移除【%1】吗？").arg(m_dataGroups[row].groupName))) {
                m_dataGroups.removeAt(row);
                if (!m_dataGroups.isEmpty()) {
                    for(auto& g : m_dataGroups) {
                        g.weight = 1.0 / m_dataGroups.size();
                    }
                }
                refreshDataTable();
                updateModelCurve(nullptr, true, false);
            }
        }
    }
}

bool WT_MultidataFittingWidget::validateDataWeights()
{
    double sum = 0.0;
    for(const auto& g : m_dataGroups) {
        sum += g.weight;
    }

    if(std::abs(sum - 1.0) > 1e-4) {
        Standard_MessageBox::warning(this, "权重校验失败", QString("当前各组数据权重之和为 %1，必须恰好等于 1.0！").arg(sum));
        return false;
    }
    return true;
}

void WT_MultidataFittingWidget::on_btn_modelSelect_clicked()
{
    ModelSelect dlg(this);
    dlg.setCurrentModelCode(QString("modelwidget%1").arg((int)m_currentModelType + 1));

    if (dlg.exec() == QDialog::Accepted) {
        QString code = dlg.getSelectedModelCode();
        code.remove("modelwidget");

        m_isUpdatingTable = true;
        m_currentModelType = (ModelManager::ModelType)(code.toInt() - 1);
        m_paramChart->switchModel(m_currentModelType);

        ui->btn_modelSelect->setText(ModelSelect::getShortName((int)m_currentModelType + 1));

        loadProjectParams();
        hideUnwantedParams();
        m_isUpdatingTable = false;
        updateModelCurve(nullptr, true);
    }
}

void WT_MultidataFittingWidget::on_btnSelectParams_clicked()
{
    m_paramChart->updateParamsFromTable();
    double currentTime = m_userDefinedTimeMax > 0 ? m_userDefinedTimeMax : 10000.0;

    ParamSelectDialog dlg(m_paramChart->getParameters(), m_currentModelType, currentTime, m_useLimits, this);
    connect(&dlg, &ParamSelectDialog::estimateInitialParamsRequested, this, [this, &dlg]() {
        onEstimateInitialParams();
        dlg.collectData();
    });

    if(dlg.exec() == QDialog::Accepted) {
        auto params = dlg.getUpdatedParams();
        for(auto& p : params) {
            if(p.name == "LfD") p.isFit = false;
        }

        m_isUpdatingTable = true;
        m_paramChart->setParameters(params);
        m_userDefinedTimeMax = dlg.getFittingTime();
        m_useLimits = dlg.getUseLimits();
        hideUnwantedParams();
        m_isUpdatingTable = false;

        updateModelCurve(nullptr, false);
    }
}

void WT_MultidataFittingWidget::on_btnSampling_clicked()
{
    if (m_dataGroups.isEmpty()) return;

    double globalMinT = 1e9;
    double globalMaxT = -1e9;

    for (const auto& g : m_dataGroups) {
        if (g.origTime.isEmpty()) continue;
        if (g.origTime.first() < globalMinT) globalMinT = g.origTime.first();
        if (g.origTime.last() > globalMaxT) globalMaxT = g.origTime.last();
    }

    if (globalMinT > globalMaxT || globalMinT <= 0) {
        globalMinT = 0.01;
        globalMaxT = 1000.0;
    }

    QList<SamplingInterval> intervals;
    SamplingSettingsDialog dlg(intervals, true, globalMinT, globalMaxT, this);

    if(dlg.exec() == QDialog::Accepted) {
        if (!dlg.isCustomSamplingEnabled()) {
            for (auto& g : m_dataGroups) {
                g.time = g.origTime;
                g.deltaP = g.origDeltaP;
                g.derivative = g.origDerivative;
            }
            updateModelCurve(nullptr, true, false);
            return;
        }

        intervals = dlg.getIntervals();
        QVector<double> masterTimeGrid;

        for (const auto& interval : intervals) {
            if (interval.count <= 0 || interval.tStart >= interval.tEnd || interval.tStart <= 0) continue;

            if (interval.count == 1) {
                masterTimeGrid.append(interval.tStart);
            } else {
                double logStart = std::log10(interval.tStart);
                double logEnd = std::log10(interval.tEnd);
                double step = (logEnd - logStart) / (interval.count - 1);

                for (int i = 0; i < interval.count; ++i) {
                    masterTimeGrid.append(std::pow(10, logStart + i * step));
                }
            }
        }

        std::sort(masterTimeGrid.begin(), masterTimeGrid.end());
        auto last = std::unique(masterTimeGrid.begin(), masterTimeGrid.end(), [](double a, double b) {
            return std::abs(a - b) < 1e-6;
        });
        masterTimeGrid.erase(last, masterTimeGrid.end());

        if (masterTimeGrid.isEmpty()) return;

        for (auto& g : m_dataGroups) {
            if (g.origTime.isEmpty()) continue;
            g.time = masterTimeGrid;
            g.deltaP = interpolate(g.origTime, g.origDeltaP, masterTimeGrid);
            g.derivative = interpolate(g.origTime, g.origDerivative, masterTimeGrid);
        }

        updateModelCurve(nullptr, true, false);
    }
}

void WT_MultidataFittingWidget::on_comboFitMode_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    updateModelCurve(nullptr, true, false);
}

QVector<double> WT_MultidataFittingWidget::interpolate(const QVector<double>& srcX, const QVector<double>& srcY, const QVector<double>& targetX)
{
    QVector<double> result;
    int n = srcX.size();
    if(n == 0) return result;

    for(double x : targetX) {
        if(x <= srcX.first()) {
            result.append(srcY.first());
            continue;
        }
        if(x >= srcX.last()) {
            result.append(srcY.last());
            continue;
        }

        int l = 0, r = n - 1;
        while(l < r - 1) {
            int mid = l + (r - l) / 2;
            if(srcX[mid] < x) {
                l = mid;
            } else {
                r = mid;
            }
        }

        double weight = (x - srcX[l]) / (srcX[r] - srcX[l]);
        result.append(srcY[l] + weight * (srcY[r] - srcY[l]));
    }
    return result;
}

QVector<double> WT_MultidataFittingWidget::generateCommonTimeGrid()
{
    QVector<double> commonT;
    double minT = 1e9, maxT = -1e9;

    for(const auto& g : m_dataGroups) {
        if(g.time.isEmpty()) continue;
        if(g.time.first() < minT) minT = g.time.first();
        if(g.time.last() > maxT) maxT = g.time.last();
    }

    if(minT > maxT) return commonT;

    int N = 300;
    double logMin = std::log10(minT);
    double logMax = std::log10(maxT);
    double step = (logMax - logMin) / (N - 1);

    for(int i = 0; i < N; ++i) {
        commonT.append(std::pow(10, logMin + i * step));
    }

    return commonT;
}

void WT_MultidataFittingWidget::on_btnRunFit_clicked()
{
    if(m_isFitting) return;

    if(m_dataGroups.isEmpty()) {
        Standard_MessageBox::warning(this, "错误", "请先导入至少一组观测数据才能进行计算。");
        return;
    }

    // 【核心精简】所有的数组校验与剥离代码已被彻底删除！
    m_paramChart->updateParamsFromTable();

    if(!validateDataWeights()) return;

    m_isFitting = true;
    m_iterCount = 0;
    m_initialSSE = 0.0;
    ui->btnRunFit->setEnabled(false);

    QVector<double> fitT, fitP, fitD;
    int totalGroups = 0;

    for (const auto& g : m_dataGroups) {
        if (g.weight > 0 && !g.time.isEmpty()) {
            totalGroups++;
        }
    }

    int basePoints = 40;

    for (const auto& g : m_dataGroups) {
        if (g.weight <= 0 || g.time.isEmpty()) continue;

        int sampleCount = qMax(15, (int)(basePoints * totalGroups * g.weight));
        QVector<double> sampT, sampP, sampD;

        logSampleGroup(g.time, g.deltaP, g.derivative, sampleCount, sampT, sampP, sampD);
        fitT.append(sampT);
        fitP.append(sampP);
        fitD.append(sampD);
    }

    sortConcatenated(fitT, fitP, fitD);

    QSlider* slider = findChild<QSlider*>("sliderWeight");
    double w = slider ? slider->value() / 100.0 : 0.5;

    m_core->setObservedData(fitT, fitP, fitD);
    m_core->startFit(m_currentModelType, m_paramChart->getParameters(), w, m_useLimits, m_useMultiStart);
}

void WT_MultidataFittingWidget::on_btnStop_clicked()
{
    if(m_core) m_core->stopFit();
}

void WT_MultidataFittingWidget::on_btnImportModel_clicked()
{
    m_paramChart->updateParamsFromTable();
    updateModelCurve(nullptr, false, false);
}

void WT_MultidataFittingWidget::updateModelCurve(const QMap<QString, double>* explicitParams, bool autoScale, bool calcError)
{
    if(!m_modelManager) return;

    m_plotLogLog->clearGraphs();
    m_plotSemiLog->clearGraphs();
    m_plotCartesian->clearGraphs();

    for(int i = 0; i < m_dataGroups.size(); ++i) {
        const auto& g = m_dataGroups[i];
        if(g.origTime.isEmpty()) continue;

        bool isSampled = (g.time.size() < g.origTime.size() && g.time.size() > 0);

        if (g.showDeltaP) {
            QCPGraph* graphP_orig = m_plotLogLog->addGraph();
            graphP_orig->setData(g.origTime, g.origDeltaP);
            graphP_orig->setLineStyle(QCPGraph::lsNone);
            graphP_orig->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, g.color, Qt::white, 5));
            graphP_orig->setName(g.groupName + " (原始压差)");

            if (isSampled) {
                QCPGraph* graphP_samp = m_plotLogLog->addGraph();
                graphP_samp->setData(g.time, g.deltaP);
                graphP_samp->setLineStyle(QCPGraph::lsNone);
                graphP_samp->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, g.color, g.color, 5));
                graphP_samp->setName(g.groupName + " (抽样压差)");
            }
        }

        if (g.showDerivative) {
            QCPGraph* graphD_orig = m_plotLogLog->addGraph();
            graphD_orig->setData(g.origTime, g.origDerivative);
            graphD_orig->setLineStyle(QCPGraph::lsNone);
            graphD_orig->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssTriangle, g.color, Qt::white, 5));
            graphD_orig->setName(g.groupName + " (原始导数)");

            if (isSampled) {
                QCPGraph* graphD_samp = m_plotLogLog->addGraph();
                graphD_samp->setData(g.time, g.derivative);
                graphD_samp->setLineStyle(QCPGraph::lsNone);
                graphD_samp->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssTriangle, g.color, g.color, 5));
                graphD_samp->setName(g.groupName + " (抽样导数)");
            }
        }
    }

    QMap<QString, double> rawParams;

    // 【核心精简】剔除全部手动拆分字符串的逻辑
    if (explicitParams) {
        rawParams = *explicitParams;
    } else {
        QList<FitParameter> allParams = m_paramChart->getParameters();
        for(const auto& p : allParams) {
            rawParams.insert(p.name, p.value);
        }
    }

    if (rawParams.contains("C_D")) rawParams.insert("cD", rawParams["C_D"]);
    if (rawParams.contains("cD")) rawParams.insert("C_D", rawParams["cD"]);
    if (rawParams.contains("C_phi")) rawParams.insert("cPhi", rawParams["C_phi"]);
    if (rawParams.contains("cPhi")) rawParams.insert("C_phi", rawParams["cPhi"]);

    if (rawParams.contains("k2") && rawParams.contains("kf")) {
        double kf = rawParams["kf"];
        double k2 = rawParams["k2"];
        rawParams["M12"] = (k2 < 1e-12) ? 1.0 : (kf / k2);
    } else if (rawParams.contains("M12") && rawParams.contains("kf")) {
        double m12 = rawParams["M12"];
        double kf = rawParams["kf"];
        rawParams["k2"] = (m12 < 1e-12) ? kf : (kf / m12);
    }

    QMap<QString, double> solverParams = FittingCore::preprocessParams(rawParams, m_currentModelType);
    QVector<double> targetT = generateCommonTimeGrid();

    if(targetT.isEmpty()) {
        targetT = ModelManager::generateLogTimeSteps(300, -4, 4);
    }

    ModelCurveData res = m_modelManager->calculateTheoreticalCurve(m_currentModelType, solverParams, targetT);
    QVector<double> tCurve = std::get<0>(res);
    QVector<double> pCurve = std::get<1>(res);
    QVector<double> dCurve = std::get<2>(res);

    QCPGraph* gModelP = m_plotLogLog->addGraph();
    gModelP->setData(tCurve, pCurve);
    gModelP->setPen(QPen(Qt::red, 2, Qt::SolidLine));
    gModelP->setName("理论压差");

    QCPGraph* gModelD = m_plotLogLog->addGraph();
    gModelD->setData(tCurve, dCurve);
    gModelD->setPen(QPen(Qt::blue, 2, Qt::SolidLine));
    gModelD->setName("理论导数");

    for (int i = 0; i < m_dataGroups.size(); ++i) {
        const auto& g = m_dataGroups[i];
        if (g.origTime.isEmpty() || !g.showDeltaP) continue;

        QCPGraph* gSemi = m_plotSemiLog->addGraph();
        gSemi->setData(g.origTime, g.origDeltaP);
        gSemi->setLineStyle(QCPGraph::lsNone);
        gSemi->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, g.color, Qt::white, 5));
        gSemi->setName(g.groupName);
    }

    QCPGraph* gSemiModel = m_plotSemiLog->addGraph();
    gSemiModel->setData(tCurve, pCurve);
    gSemiModel->setPen(QPen(Qt::red, 2));
    gSemiModel->setName("理论压差");

    for (int i = 0; i < m_dataGroups.size(); ++i) {
        const auto& g = m_dataGroups[i];
        if (g.origTime.isEmpty() || g.rawP.isEmpty()) continue;

        QCPGraph* gCart = m_plotCartesian->addGraph();
        gCart->setData(g.origTime, g.rawP);
        gCart->setLineStyle(QCPGraph::lsNone);
        gCart->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, g.color, Qt::white, 4));
        gCart->setName(g.groupName);
    }

    if(autoScale) {
        m_plotLogLog->rescaleAxes();
        m_plotSemiLog->rescaleAxes();
        m_plotCartesian->rescaleAxes();
    }

    m_plotLogLog->replot();
    m_plotSemiLog->replot();
    m_plotCartesian->replot();
}

void WT_MultidataFittingWidget::onIterationUpdate(double err, const QMap<QString,double>& p, const QVector<double>& t, const QVector<double>& p_curve, const QVector<double>& d_curve)
{
    m_iterCount++;
    if (m_initialSSE <= 0.0) m_initialSSE = err;

    double improvePct = (m_initialSSE > 1e-20) ? (1.0 - err / m_initialSSE) * 100.0 : 0.0;
    ui->label_Error->setText(QString("迭代 %1 | MSE: %2 | 改善: %3%").arg(m_iterCount).arg(err, 0, 'e', 3).arg(improvePct, 0, 'f', 1));

    m_isUpdatingTable = true;
    ui->tableParams->blockSignals(true);

    for(int i=0; i<ui->tableParams->rowCount(); ++i) {
        QString key = ui->tableParams->item(i, 1)->data(Qt::UserRole).toString();
        if(p.contains(key)) {
            QTableWidgetItem* valItem = ui->tableParams->item(i, 2);
            if(valItem) {
                valItem->setText(QString::number(p[key], 'g', 5));
            }
        }
    }

    ui->tableParams->blockSignals(false);
    m_isUpdatingTable = false;

    updateModelCurve(&p, false, false);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void WT_MultidataFittingWidget::onFitFinished()
{
    m_isFitting = false;
    ui->btnRunFit->setEnabled(true);
    m_paramChart->updateParamsFromTable();

    QMap<QString, double> rawParams;
    for (const auto& p : m_paramChart->getParameters()) {
        rawParams.insert(p.name, p.value);
    }

    QSlider* slider = findChild<QSlider*>("sliderWeight");
    double w = slider ? slider->value() / 100.0 : 0.5;

    QString report = QString("多数据拟合完成（共 %1 组，迭代 %2 次）\n").arg(m_dataGroups.size()).arg(m_iterCount);
    report += QString::fromUtf8("─────────────────────────\n");

    double totalSSE = 0.0;
    int totalPoints = 0;

    for (const auto& g : m_dataGroups) {
        if (g.time.isEmpty() || g.weight <= 0) continue;

        QVector<double> residuals = m_core->calculateResiduals(rawParams, m_currentModelType, w, g.time, g.deltaP, g.derivative);
        double sse = m_core->calculateSumSquaredError(residuals);
        double mse = residuals.isEmpty() ? 0.0 : sse / residuals.size();

        double meanLogP = 0.0;
        int validCount = 0;
        for (double v : g.deltaP) {
            if (v > 1e-10) {
                meanLogP += std::log(v);
                validCount++;
            }
        }

        double rSquared = 0.0;
        if (validCount > 0) {
            meanLogP /= validCount;
            double sst = 0.0;
            for (double v : g.deltaP) {
                if (v > 1e-10) {
                    double d = std::log(v) - meanLogP;
                    sst += d * d;
                }
            }
            if (sst > 1e-20) {
                rSquared = 1.0 - sse / sst;
            }
        }

        QString quality;
        if (rSquared > 0.95) quality = "优秀";
        else if (rSquared > 0.85) quality = "良好";
        else if (rSquared > 0.70) quality = "一般";
        else quality = "较差";

        report += QString("%1: MSE=%2, R²=%3 (%4)\n").arg(g.groupName, -12).arg(mse, 0, 'e', 2).arg(rSquared, 0, 'f', 3).arg(quality);
        totalSSE += sse;
        totalPoints += residuals.size();
    }

    double globalMSE = totalPoints > 0 ? totalSSE / totalPoints : 0.0;
    report += QString::fromUtf8("─────────────────────────\n");
    report += QString("全局 MSE: %1").arg(globalMSE, 0, 'e', 3);

    ui->label_Error->setText(QString("全局 MSE: %1").arg(globalMSE, 0, 'e', 3));
    Standard_MessageBox::info(this, "拟合完成", report);
}

void WT_MultidataFittingWidget::onExportCurveData()
{
    QString defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";

    QString path = QFileDialog::getSaveFileName(this, "导出多数据拟合曲线", defaultDir + "/MultiFittingCurves.csv", "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "DataGroup,Time,DeltaP,Derivative\n";

        for(const auto& g : m_dataGroups) {
            for(int i = 0; i < g.time.size(); ++i) {
                out << g.groupName << "," << g.time[i] << "," << g.deltaP[i] << ",";
                if(i < g.derivative.size()) {
                    out << g.derivative[i] << "\n";
                } else {
                    out << "\n";
                }
            }
        }

        out << "TheoreticalModel,Time,DeltaP,Derivative\n";
        QMap<QString, double> rawParams;
        for(const auto& p : m_paramChart->getParameters()) {
            rawParams.insert(p.name, p.value);
        }

        QMap<QString, double> solverParams = FittingCore::preprocessParams(rawParams, m_currentModelType);
        QVector<double> targetT = generateCommonTimeGrid();

        if(targetT.isEmpty()) {
            targetT = ModelManager::generateLogTimeSteps(300, -4, 4);
        }

        ModelCurveData res = m_modelManager->calculateTheoreticalCurve(m_currentModelType, solverParams, targetT);
        QVector<double> tCurve = std::get<0>(res);
        QVector<double> pCurve = std::get<1>(res);
        QVector<double> dCurve = std::get<2>(res);

        for(int i = 0; i < tCurve.size(); ++i) {
            out << "Model," << tCurve[i] << "," << pCurve[i] << "," << dCurve[i] << "\n";
        }

        f.close();
        Standard_MessageBox::info(this, "导出成功", "多数据拟合曲线数据已保存。");
    }
}

void WT_MultidataFittingWidget::onSemiLogLineMoved(double k, double b)
{
    Q_UNUSED(k);
    QList<FitParameter> params = m_paramChart->getParameters();
    bool updated = false;

    for(auto& p : params) {
        if(p.name == "Pi" || p.name == "p*") {
            p.value = b;
            updated = true;
            break;
        }
    }

    if(updated) {
        m_isUpdatingTable = true;
        m_paramChart->setParameters(params);
        m_isUpdatingTable = false;
        updateModelCurve(nullptr, false, false);
    }
}

void WT_MultidataFittingWidget::hideUnwantedParams()
{
    for(int i = 0; i < ui->tableParams->rowCount(); ++i) {
        QTableWidgetItem* item = ui->tableParams->item(i, 1);
        if(item && item->data(Qt::UserRole).toString() == "LfD") {
            ui->tableParams->setRowHidden(i, true);
        }
    }
}

void WT_MultidataFittingWidget::loadProjectParams()
{
    ModelParameter* mp = ModelParameter::instance();
    QList<FitParameter> params = m_paramChart->getParameters();
    bool changed = false;

    for(auto& p : params) {
        if(p.name == "phi") { p.value = mp->getPhi(); changed = true; }
        else if(p.name == "h")  { p.value = mp->getH();  changed = true; }
        else if(p.name == "rw") { p.value = mp->getRw(); changed = true; }
        else if(p.name == "mu") { p.value = mp->getMu(); changed = true; }
        else if(p.name == "Ct") { p.value = mp->getCt(); changed = true; }
        else if(p.name == "B")  { p.value = mp->getB();  changed = true; }
        else if(p.name == "q")  { p.value = mp->getQ();  changed = true; }
    }

    if(changed) {
        m_isUpdatingTable = true;
        m_paramChart->setParameters(params);
        m_isUpdatingTable = false;
    }
}

QColor WT_MultidataFittingWidget::getColor(int index)
{
    static QList<QColor> colors = {
        QColor("#1f77b4"), QColor("#ff7f0e"), QColor("#2ca02c"), QColor("#d62728"), QColor("#9467bd")
};
return colors[index % colors.size()];
}

void WT_MultidataFittingWidget::logSampleGroup(const QVector<double>& srcT, const QVector<double>& srcP, const QVector<double>& srcD, int count, QVector<double>& outT, QVector<double>& outP, QVector<double>& outD)
{
    outT.clear();
    outP.clear();
    outD.clear();

    if (srcT.isEmpty() || count <= 0) return;

    if (srcT.size() <= count) {
        outT = srcT;
        outP = srcP;
        outD = srcD;
        return;
    }

    double tMin = srcT.first() <= 1e-10 ? 1e-4 : srcT.first();
    double tMax = srcT.last();
    double logMin = log10(tMin);
    double logMax = log10(tMax);
    double step = (logMax - logMin) / (count - 1);

    int currentIndex = 0;

    for (int i = 0; i < count; ++i) {
        double targetT = pow(10, logMin + i * step);
        double minDiff = 1e30;
        int bestIdx = currentIndex;

        while (currentIndex < srcT.size()) {
            double diff = std::abs(srcT[currentIndex] - targetT);
            if (diff < minDiff) {
                minDiff = diff;
                bestIdx = currentIndex;
            } else {
                break;
            }
            currentIndex++;
        }

        currentIndex = bestIdx;
        outT.append(srcT[bestIdx]);
        outP.append(bestIdx < srcP.size() ? srcP[bestIdx] : 0.0);
        outD.append(bestIdx < srcD.size() ? srcD[bestIdx] : 0.0);
    }
}

void WT_MultidataFittingWidget::sortConcatenated(QVector<double>& t, QVector<double>& p, QVector<double>& d)
{
    int n = t.size();
    if (n <= 1) return;

    QVector<std::tuple<double,double,double>> packed(n);
    for (int i = 0; i < n; ++i) {
        packed[i] = std::make_tuple(t[i], p[i], i < d.size() ? d[i] : 0.0);
    }

    std::sort(packed.begin(), packed.end(), [](const auto& a, const auto& b) {
        return std::get<0>(a) < std::get<0>(b);
    });

    t.resize(n);
    p.resize(n);
    d.resize(n);

    for (int i = 0; i < n; ++i) {
        t[i] = std::get<0>(packed[i]);
        p[i] = std::get<1>(packed[i]);
        d[i] = std::get<2>(packed[i]);
    }
}

void WT_MultidataFittingWidget::onEstimateInitialParams()
{
    if (m_dataGroups.isEmpty()) return;

    int bestIdx = 0;
    double maxW = -1;

    for (int i = 0; i < m_dataGroups.size(); ++i) {
        if (m_dataGroups[i].weight > maxW && !m_dataGroups[i].time.isEmpty()) {
            maxW = m_dataGroups[i].weight;
            bestIdx = i;
        }
    }

    const DataGroup& ref = m_dataGroups[bestIdx];
    QMap<QString, double> estimated = FittingCore::estimateInitialParams(ref.time, ref.deltaP, ref.derivative, m_currentModelType);

    if (estimated.isEmpty()) return;

    QList<FitParameter> params = m_paramChart->getParameters();
    int updatedCount = 0;

    for (auto& p : params) {
        if (estimated.contains(p.name) && p.isFit) {
            p.value = estimated[p.name];
            updatedCount++;
        }
    }

    if (updatedCount > 0) {
        m_isUpdatingTable = true;
        m_paramChart->setParameters(params);
        m_paramChart->autoAdjustLimits();
        hideUnwantedParams();
        m_isUpdatingTable = false;

        updateModelCurve(nullptr, true);
        Standard_MessageBox::info(this, "智能初值", QString("已基于【%1】的观测数据自动估算 %2 个参数的初始值。").arg(ref.groupName).arg(updatedCount));
    }
}
