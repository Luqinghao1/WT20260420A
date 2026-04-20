/*
 * 文件名: wt_fittingwidget.cpp
 * 作用: 单数据拟合入口界面
 * 【优化内容】：
 * 1. 接入静态短名工具实现标准文案展示，支持高达 324 种物理模型。
 * 2. 【核心重构】配合动态标量展开方案，彻底删除了所有带括号字符串数组的解析逻辑！
 * 3. 移除了数组数量与 nf 的校验代码，因为参数表格现已自动生成独立的 Lf_i 与 xf_i 标量，杜绝了不匹配的可能。
 * 4. 参数处理核心平滑应用了 k2(外区渗透率) 和 M12 的互相转化机制。
 */

#include "wt_fittingwidget.h"
#include "ui_wt_fittingwidget.h"
#include "settingpaths.h"
#include "modelparameter.h"
#include "modelselect.h"
#include "fittingdatadialog.h"
#include "pressurederivativecalculator.h"
#include "pressurederivativecalculator1.h"
#include "paramselectdialog.h"
#include "fittingreport.h"
#include "fittingchart.h"
#include "modelsolver01.h"

#include "standard_messagebox.h"
#include <QDebug>
#include <cmath>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QBuffer>
#include <QFileInfo>
#include <QDateTime>

FittingWidget::FittingWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FittingWidget),
    m_modelManager(nullptr),
    m_core(new FittingCore(this)),
    m_chartManager(new FittingChart(this)),
    m_mdiArea(nullptr),
    m_chartLogLog(nullptr),
    m_chartSemiLog(nullptr),
    m_chartCartesian(nullptr),
    m_subWinLogLog(nullptr),
    m_subWinSemiLog(nullptr),
    m_subWinCartesian(nullptr),
    m_plotLogLog(nullptr),
    m_plotSemiLog(nullptr),
    m_plotCartesian(nullptr),
    m_currentModelType(ModelManager::Model_1),
    m_obsTime(),
    m_obsDeltaP(),
    m_obsDerivative(),
    m_obsRawP(),
    m_isFitting(false),
    m_isCustomSamplingEnabled(false),
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

    m_subWinLogLog->setWindowTitle("双对数图");
    m_subWinSemiLog->setWindowTitle("半对数图");
    m_subWinCartesian->setWindowTitle("标准坐标系");

    connect(m_chartLogLog, &FittingChart1::exportDataTriggered, this, &FittingWidget::onExportCurveData);
    connect(m_chartSemiLog, &FittingChart2::exportDataTriggered, this, &FittingWidget::onExportCurveData);
    connect(m_chartCartesian, &FittingChart3::exportDataTriggered, this, &FittingWidget::onExportCurveData);
    connect(m_chartManager, &FittingChart::sigManualPressureUpdated, this, &FittingWidget::onSemiLogLineMoved);

    ui->splitter->setSizes(QList<int>() << 350 << 650);
    ui->splitter->setCollapsible(0, false);

    m_paramChart = new FittingParameterChart(ui->tableParams, this);
    connect(m_paramChart, &FittingParameterChart::parameterChangedByWheel, this, [this](){
        updateModelCurve(nullptr, false, false);
    });

    setupPlot();
    m_chartManager->initializeCharts(m_plotLogLog, m_plotSemiLog, m_plotCartesian);
    qRegisterMetaType<QMap<QString,double>>("QMap<QString,double>");
    qRegisterMetaType<ModelManager::ModelType>("ModelManager::ModelType");
    qRegisterMetaType<QVector<double>>("QVector<double>");

    connect(m_core, &FittingCore::sigIterationUpdated, this, &FittingWidget::onIterationUpdate, Qt::QueuedConnection);
    connect(m_core, &FittingCore::sigProgress, ui->progressBar, &QProgressBar::setValue);
    connect(m_core, &FittingCore::sigFitFinished, this, &FittingWidget::onFitFinished);
    connect(ui->sliderWeight, &QSlider::valueChanged, this, &FittingWidget::onSliderWeightChanged);
    connect(ui->btnSamplingSettings, &QPushButton::clicked, this, &FittingWidget::onOpenSamplingSettings);

    ui->sliderWeight->setRange(0, 100);
    ui->sliderWeight->setValue(50);
    onSliderWeightChanged(50);

    QCheckBox* chkMultiStart = new QCheckBox("多起点优化", this);
    chkMultiStart->setToolTip("从多个随机初值出发预扫描，选取最优起点再进行完整拟合，降低陷入局部最优的概率");
    chkMultiStart->setChecked(false);

    connect(chkMultiStart, &QCheckBox::toggled, this, [this](bool checked) {
        m_useMultiStart = checked;
    });

    QLayout* parentLayout = ui->progressBar->parentWidget()->layout();
    if (parentLayout) {
        int progIdx = parentLayout->indexOf(ui->progressBar);
        if (progIdx >= 0) {
            static_cast<QVBoxLayout*>(parentLayout)->insertWidget(progIdx, chkMultiStart);
        }
    }
}

FittingWidget::~FittingWidget() {
    delete ui;
}

void FittingWidget::onSemiLogLineMoved(double k, double b) {
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
        m_paramChart->setParameters(params);
    }
}

void FittingWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    layoutCharts();
}

void FittingWidget::setModelManager(ModelManager *m) {
    m_modelManager = m;
    m_paramChart->setModelManager(m);
    if (m_core) m_core->setModelManager(m);
    initializeDefaultModel();
}

void FittingWidget::setProjectDataModels(const QMap<QString, QStandardItemModel *> &models) {
    m_dataMap = models;
}

void FittingWidget::setObservedData(const QVector<double>& t, const QVector<double>& deltaP, const QVector<double>& d) {
    setObservedData(t, deltaP, d, QVector<double>());
}

void FittingWidget::setObservedData(const QVector<double>& t, const QVector<double>& deltaP, const QVector<double>& d, const QVector<double>& rawP) {
    m_obsTime = t;
    m_obsDeltaP = deltaP;
    m_obsDerivative = d;
    m_obsRawP = rawP;

    if (m_core) m_core->setObservedData(t, deltaP, d);
    if (m_chartManager) m_chartManager->setObservedData(t, deltaP, d, rawP);

    updateModelCurve(nullptr, true);
}

void FittingWidget::updateBasicParameters() {}

void FittingWidget::initializeDefaultModel() {
    if(!m_modelManager) return;
    m_currentModelType = ModelManager::Model_1;

    // 初始化时按钮赋值采用最新通用规范命名
    ui->btn_modelSelect->setText(ModelSelect::getShortName((int)m_currentModelType + 1));

    m_paramChart->resetParams(m_currentModelType, true);
    loadProjectParams();
    hideUnwantedParams();
    updateModelCurve(nullptr, true, true);
}

void FittingWidget::setupPlot() {
    if(m_plotLogLog) m_plotLogLog->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    if(m_plotSemiLog) m_plotSemiLog->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    if(m_plotCartesian) m_plotCartesian->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
}

void FittingWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    layoutCharts();
}

void FittingWidget::layoutCharts() {
    if (!m_mdiArea || !m_subWinLogLog || !m_subWinSemiLog || !m_subWinCartesian) return;
    QRect rect = m_mdiArea->contentsRect();
    int w = rect.width();
    int h = rect.height();
    if (w <= 0 || h <= 0) return;

    m_subWinLogLog->setGeometry(0, 0, w / 2, h);
    m_subWinCartesian->setGeometry(w / 2, 0, w - (w / 2), h / 2);
    m_subWinSemiLog->setGeometry(w / 2, h / 2, w - (w / 2), h - (h / 2));

    if (m_subWinLogLog->isMinimized()) m_subWinLogLog->showNormal();
    if (m_subWinCartesian->isMinimized()) m_subWinCartesian->showNormal();
    if (m_subWinSemiLog->isMinimized()) m_subWinSemiLog->showNormal();
}

void FittingWidget::on_btnLoadData_clicked() {
    FittingDataDialog dlg(m_dataMap, this);
    if (dlg.exec() != QDialog::Accepted) return;

    FittingDataSettings settings = dlg.getSettings();
    QStandardItemModel* sourceModel = dlg.getPreviewModel();
    if (!sourceModel || sourceModel->rowCount() == 0) {
        Standard_MessageBox::warning(this, "警告", "所选数据源为空，无法加载！");
        return;
    }

    QVector<double> rawTime, rawPressureData, finalDeriv;
    int skip = settings.skipRows;
    int rows = sourceModel->rowCount();

    for (int i = skip; i < rows; ++i) {
        QStandardItem* itemT = sourceModel->item(i, settings.timeColIndex);
        QStandardItem* itemP = sourceModel->item(i, settings.pressureColIndex);

        if (itemT && itemP) {
            bool okT, okP;
            double t = itemT->text().toDouble(&okT);
            double p = itemP->text().toDouble(&okP);
            if (okT && okP && t > 0) {
                rawTime.append(t);
                rawPressureData.append(p);
                if (settings.derivColIndex >= 0) {
                    QStandardItem* itemD = sourceModel->item(i, settings.derivColIndex);
                    if (itemD) finalDeriv.append(itemD->text().toDouble());
                    else finalDeriv.append(0.0);
                }
            }
        }
    }

    if (rawTime.isEmpty()) {
        Standard_MessageBox::warning(this, "警告", "未能提取到有效数据。");
        return;
    }

    QVector<double> finalDeltaP;
    double p_shutin = rawPressureData.first();
    for (double p : rawPressureData) {
        double deltaP = 0.0;
        if (settings.testType == Test_Drawdown) {
            deltaP = std::abs(settings.initialPressure - p);
        } else {
            deltaP = std::abs(p - p_shutin);
        }
        finalDeltaP.append(deltaP);
    }

    if (settings.derivColIndex == -1) {
        finalDeriv = PressureDerivativeCalculator::calculateBourdetDerivative(rawTime, finalDeltaP, settings.lSpacing);
        if (settings.enableSmoothing) {
            finalDeriv = PressureDerivativeCalculator1::smoothData(finalDeriv, settings.smoothingSpan);
        }
    } else {
        if (settings.enableSmoothing) {
            finalDeriv = PressureDerivativeCalculator1::smoothData(finalDeriv, settings.smoothingSpan);
        }
        if (finalDeriv.size() != rawTime.size()) {
            finalDeriv.resize(rawTime.size());
        }
    }

    ModelParameter* mp = ModelParameter::instance();
    if (mp) {
        settings.porosity = mp->getPhi();
        settings.thickness = mp->getH();
        settings.wellRadius = mp->getRw();
        settings.viscosity = mp->getMu();
        settings.ct = mp->getCt();
        settings.fvf = mp->getB();
        settings.rate = mp->getQ();
    }

    m_chartManager->setSettings(settings);
    setObservedData(rawTime, finalDeltaP, finalDeriv, rawPressureData);

    if (!rawTime.isEmpty()) {
        m_userDefinedTimeMax = rawTime.last();
    }

    Standard_MessageBox::info(this, "成功", "观测数据已成功加载。");
}

void FittingWidget::onSliderWeightChanged(int value) {
    double wPressure = value / 100.0;
    double wDerivative = 1.0 - wPressure;
    ui->label_ValDerivative->setText(QString("导数权重: %1").arg(wDerivative, 0, 'f', 2));
    ui->label_ValPressure->setText(QString("压差权重: %1").arg(wPressure, 0, 'f', 2));
}

void FittingWidget::on_btnSelectParams_clicked() {
    m_paramChart->updateParamsFromTable();
    QList<FitParameter> currentParams = m_paramChart->getParameters();
    double currentTime = m_userDefinedTimeMax;

    if (currentTime <= 0 && !m_obsTime.isEmpty()) {
        currentTime = m_obsTime.last();
    } else if (currentTime <= 0) {
        currentTime = 10000.0;
    }

    ParamSelectDialog dlg(currentParams, m_currentModelType, currentTime, m_useLimits, this);
    connect(&dlg, &ParamSelectDialog::estimateInitialParamsRequested, this, [this, &dlg]() {
        onEstimateInitialParams();
        dlg.collectData();
    });

    if(dlg.exec() == QDialog::Accepted) {
        QList<FitParameter> updatedParams = dlg.getUpdatedParams();
        for(auto& p : updatedParams) {
            if(p.name == "LfD") p.isFit = false;
        }
        m_paramChart->setParameters(updatedParams);
        m_userDefinedTimeMax = dlg.getFittingTime();
        m_useLimits = dlg.getUseLimits();
        hideUnwantedParams();
        updateModelCurve(nullptr, false);
    }
}

void FittingWidget::hideUnwantedParams() {
    for(int i = 0; i < ui->tableParams->rowCount(); ++i) {
        QTableWidgetItem* item = ui->tableParams->item(i, 1);
        if(item) {
            QString name = item->data(Qt::UserRole).toString();
            if(name == "LfD") {
                ui->tableParams->setRowHidden(i, true);
            }
        }
    }
}

void FittingWidget::onOpenSamplingSettings() {
    if (m_obsTime.isEmpty()) {
        Standard_MessageBox::warning(this, "提示", "请先加载观测数据，以便确定时间范围。");
        return;
    }

    double tMin = m_obsTime.first();
    double tMax = m_obsTime.last();
    SamplingSettingsDialog dlg(m_customIntervals, m_isCustomSamplingEnabled, tMin, tMax, this);

    if (dlg.exec() == QDialog::Accepted) {
        m_customIntervals = dlg.getIntervals();
        m_isCustomSamplingEnabled = dlg.isCustomSamplingEnabled();
        if(m_core) m_core->setSamplingSettings(m_customIntervals, m_isCustomSamplingEnabled);
        updateModelCurve(nullptr, false);
    }
}

void FittingWidget::on_btnRunFit_clicked() {
    if(m_isFitting) return;
    if(m_obsTime.isEmpty()) {
        Standard_MessageBox::warning(this,"错误","请先加载观测数据。");
        return;
    }

    // 【核心精简】所有的数组校验与剥离代码已被彻底删除！
    // 因为 UI 表格现在直接生成展开后的纯标量参数 (Lf_1, xf_1 等)，
    // 这本身就杜绝了长度不匹配的问题。
    m_paramChart->updateParamsFromTable();

    m_isFitting = true;
    m_iterCount = 0;
    m_initialSSE = 0.0;
    ui->btnRunFit->setEnabled(false);
    ui->btnSelectParams->setEnabled(false);

    ModelManager::ModelType modelType = m_currentModelType;
    QList<FitParameter> paramsCopy = m_paramChart->getParameters();
    double w = ui->sliderWeight->value() / 100.0;

    if(m_core) {
        m_core->startFit(modelType, paramsCopy, w, m_useLimits, m_useMultiStart);
    }
}

void FittingWidget::on_btnStop_clicked() {
    if(m_core) m_core->stopFit();
}

void FittingWidget::on_btnImportModel_clicked() {
    updateModelCurve(nullptr, false, false);
}

void FittingWidget::on_btn_modelSelect_clicked() {
    ModelSelect dlg(this);
    int currentId = (int)m_currentModelType + 1;
    dlg.setCurrentModelCode(QString("modelwidget%1").arg(currentId));

    if (dlg.exec() == QDialog::Accepted) {
        QString code = dlg.getSelectedModelCode();
        QString numStr = code;
        numStr.remove("modelwidget");
        bool ok;
        int modelId = numStr.toInt(&ok);

        if (ok && modelId >= 1 && modelId <= 324) {
            ModelManager::ModelType newType = (ModelManager::ModelType)(modelId - 1);
            m_paramChart->switchModel(newType);
            m_currentModelType = newType;

            // 采用最新通用规范命名回显
            ui->btn_modelSelect->setText(ModelSelect::getShortName(modelId));

            hideUnwantedParams();
            updateModelCurve(nullptr, true);
        } else {
            Standard_MessageBox::warning(this, "提示", "所选分析组合暂无对应的算法模型关联。\nCode: " + code);
        }
    }
}

void FittingWidget::loadProjectParams() {
    ModelParameter* mp = ModelParameter::instance();
    QList<FitParameter> params = m_paramChart->getParameters();
    bool changed = false;

    for(auto& p : params) {
        if(p.name == "phi") { p.value = mp->getPhi(); changed = true; }
        else if(p.name == "h") { p.value = mp->getH(); changed = true; }
        else if(p.name == "rw") { p.value = mp->getRw(); changed = true; }
        else if(p.name == "mu") { p.value = mp->getMu(); changed = true; }
        else if(p.name == "Ct") { p.value = mp->getCt(); changed = true; }
        else if(p.name == "B") { p.value = mp->getB(); changed = true; }
        else if(p.name == "q") { p.value = mp->getQ(); changed = true; }
    }

    if(changed) m_paramChart->setParameters(params);
}

void FittingWidget::updateModelCurve(const QMap<QString, double>* explicitParams, bool autoScale, bool calcError) {
    if(!m_modelManager) {
        Standard_MessageBox::error(this, "错误", "ModelManager 未初始化！");
        return;
    }
    if(m_obsTime.isEmpty() && !explicitParams && m_userDefinedTimeMax <= 0) {
        m_chartLogLog->clearGraphs();
        m_chartSemiLog->clearGraphs();
        m_chartCartesian->clearGraphs();
        return;
    }

    ui->tableParams->clearFocus();
    QMap<QString, double> rawParams;

    // 【核心精简】由于我们已彻底拥抱全标量展开阵列
    // 参数表内不存在带括号的数组，直接抓取即可！
    if (explicitParams) {
        rawParams = *explicitParams;
    } else {
        QList<FitParameter> allParams = m_paramChart->getParameters();
        for(const auto& p : allParams) {
            rawParams.insert(p.name, p.value);
        }
    }

    // 无因次参数别名防呆兼容
    if (rawParams.contains("C_D")) rawParams.insert("cD", rawParams["C_D"]);
    if (rawParams.contains("cD")) rawParams.insert("C_D", rawParams["cD"]);
    if (rawParams.contains("C_phi")) rawParams.insert("cPhi", rawParams["C_phi"]);
    if (rawParams.contains("cPhi")) rawParams.insert("C_phi", rawParams["cPhi"]);

    // k2 与 M12 双向自动补全计算机制
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
    QVector<double> targetT;

    double tMax = 10000.0;
    if (m_userDefinedTimeMax > 0) {
        tMax = m_userDefinedTimeMax;
    } else if (!m_obsTime.isEmpty()) {
        tMax = m_obsTime.last();
    }

    double tMin = (!m_obsTime.isEmpty()) ? std::max(1e-5, m_obsTime.first()) : 1e-4;
    if (tMax < tMin) tMax = tMin * 10.0;
    targetT = ModelManager::generateLogTimeSteps(300, log10(tMin), log10(tMax));

    ui->btnRunFit->setEnabled(true);

    ModelCurveData res = m_modelManager->calculateTheoreticalCurve(m_currentModelType, solverParams, targetT);
    const QVector<double>& theoP = std::get<1>(res);
    int invalidCount = 0;

    for (double v : theoP) {
        if (std::isnan(v) || std::isinf(v) || v <= 1e-15) invalidCount++;
    }

    if (theoP.size() > 0 && invalidCount > theoP.size() * 3 / 10) {
        ui->label_Error->setText(QString("警告: %1%的理论曲线点无效，请检查参数合理性").arg(100 * invalidCount / theoP.size()));
        ui->label_Error->setStyleSheet("QLabel { color: #D4380D; font-weight: bold; }");
    } else {
        ui->label_Error->setStyleSheet("");
    }

    m_chartManager->plotAll(std::get<0>(res), std::get<1>(res), std::get<2>(res), true, autoScale);

    if (!m_obsTime.isEmpty() && m_core && calcError) {
        QVector<double> sampleT, sampleP, sampleD;
        m_core->getLogSampledData(m_obsTime, m_obsDeltaP, m_obsDerivative, sampleT, sampleP, sampleD);
        QVector<double> residuals = m_core->calculateResiduals(rawParams, m_currentModelType, ui->sliderWeight->value()/100.0, sampleT, sampleP, sampleD);
        double sse = m_core->calculateSumSquaredError(residuals);
        ui->label_Error->setText(QString("误差(MSE): %1").arg(sse/residuals.size(), 0, 'e', 3));

        if (m_isCustomSamplingEnabled) {
            m_chartManager->plotSampledPoints(sampleT, sampleP, sampleD);
        }
    }

    m_plotLogLog->replot();
    m_plotSemiLog->replot();
    m_plotCartesian->replot();
}

void FittingWidget::onIterationUpdate(double err, const QMap<QString,double>& p, const QVector<double>& t, const QVector<double>& p_curve, const QVector<double>& d_curve) {
    m_iterCount++;
    if (m_initialSSE <= 0.0) m_initialSSE = err;

    double improvePct = (m_initialSSE > 1e-20) ? (1.0 - err / m_initialSSE) * 100.0 : 0.0;
    ui->label_Error->setText(QString("迭代 %1 | MSE: %2 | 改善: %3%").arg(m_iterCount).arg(err, 0, 'e', 3).arg(improvePct, 0, 'f', 1));
    ui->tableParams->blockSignals(true);

    for(int i=0; i<ui->tableParams->rowCount(); ++i) {
        QString key = ui->tableParams->item(i, 1)->data(Qt::UserRole).toString();
        if(p.contains(key)) {
            ui->tableParams->item(i, 2)->setText(QString::number(p[key], 'g', 5));
        }
    }

    ui->tableParams->blockSignals(false);
    m_chartManager->plotAll(t, p_curve, d_curve, true, false);

    if (m_isCustomSamplingEnabled && m_core) {
        QVector<double> sampleT, sampleP, sampleD;
        m_core->getLogSampledData(m_obsTime, m_obsDeltaP, m_obsDerivative, sampleT, sampleP, sampleD);
        m_chartManager->plotSampledPoints(sampleT, sampleP, sampleD);
    }

    if(m_plotLogLog) m_plotLogLog->replot();
    if(m_plotSemiLog) m_plotSemiLog->replot();
    if(m_plotCartesian) m_plotCartesian->replot();
}

void FittingWidget::onEstimateInitialParams() {
    if (m_obsTime.isEmpty()) {
        Standard_MessageBox::warning(this, "提示", "请先加载观测数据！");
        return;
    }

    QMap<QString, double> estimated = FittingCore::estimateInitialParams(m_obsTime, m_obsDeltaP, m_obsDerivative, m_currentModelType);
    if (estimated.isEmpty()) {
        Standard_MessageBox::warning(this, "提示", "数据点不足，无法自动估算初值。");
        return;
    }

    QList<FitParameter> params = m_paramChart->getParameters();
    int updatedCount = 0;

    for (auto& p : params) {
        if (estimated.contains(p.name) && p.isFit) {
            p.value = estimated[p.name];
            updatedCount++;
        }
    }

    if (updatedCount > 0) {
        m_paramChart->setParameters(params);
        m_paramChart->autoAdjustLimits();
        hideUnwantedParams();
        updateModelCurve(nullptr, true);
        Standard_MessageBox::info(this, "智能初值", QString("已根据观测数据自动估算 %1 个参数的初始值。").arg(updatedCount));
    } else {
        Standard_MessageBox::info(this, "智能初值", "没有需要估算的拟合参数。请检查参数配置中的拟合勾选。");
    }
}

void FittingWidget::onFitFinished() {
    m_isFitting = false;
    ui->btnRunFit->setEnabled(true);
    ui->btnSelectParams->setEnabled(true);

    double finalMSE = 0.0;
    double rSquared = 0.0;

    if (m_core && !m_obsTime.isEmpty()) {
        m_paramChart->updateParamsFromTable();
        QList<FitParameter> allParams = m_paramChart->getParameters();
        QMap<QString, double> rawParams;

        for (const auto& p : allParams) {
            rawParams.insert(p.name, p.value);
        }

        QVector<double> sampleT, sampleP, sampleD;
        m_core->getLogSampledData(m_obsTime, m_obsDeltaP, m_obsDerivative, sampleT, sampleP, sampleD);
        QVector<double> residuals = m_core->calculateResiduals(rawParams, m_currentModelType, ui->sliderWeight->value() / 100.0, sampleT, sampleP, sampleD);
        double sse = m_core->calculateSumSquaredError(residuals);
        finalMSE = residuals.isEmpty() ? 0.0 : sse / residuals.size();

        if (!sampleP.isEmpty()) {
            double meanLogP = 0.0;
            int validCount = 0;

            for (double v : sampleP) {
                if (v > 1e-10) {
                    meanLogP += std::log(v);
                    validCount++;
                }
            }

            if (validCount > 0) {
                meanLogP /= validCount;
                double sst = 0.0;
                for (double v : sampleP) {
                    if (v > 1e-10) {
                        double d = std::log(v) - meanLogP;
                        sst += d * d;
                    }
                }
                if (sst > 1e-20) {
                    rSquared = 1.0 - sse / sst;
                }
            }
        }
    }

    double improvePct = (m_initialSSE > 1e-20) ? (1.0 - finalMSE / m_initialSSE) * 100.0 : 0.0;
    QString quality;
    if (rSquared > 0.95) quality = "优秀";
    else if (rSquared > 0.85) quality = "良好";
    else if (rSquared > 0.70) quality = "一般";
    else quality = "较差";

    ui->label_Error->setText(QString("MSE: %1 | R²: %2 | %3").arg(finalMSE, 0, 'e', 3).arg(rSquared, 0, 'f', 4).arg(quality));
    Standard_MessageBox::info(this, "拟合完成", QString("共迭代 %1 次\n最终 MSE: %2\n拟合优度 R²: %3 (%4)\n误差改善: %5%").arg(m_iterCount).arg(finalMSE, 0, 'e', 3).arg(rSquared, 0, 'f', 4).arg(quality).arg(improvePct, 0, 'f', 1));
}

void FittingWidget::on_btnExportData_clicked() {
    m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();
    QString defaultDir = SettingPaths::reportPath();
    if(defaultDir.isEmpty()) defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";

    QString fileName = QFileDialog::getSaveFileName(this, "导出拟合参数", defaultDir + "/FittingParameters.csv", "CSV Files (*.csv);;Text Files (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);

    if(fileName.endsWith(".csv", Qt::CaseInsensitive)) {
        file.write("\xEF\xBB\xBF");
        out << QString("参数中文名,参数英文名,拟合值,单位\n");
        for(const auto& param : params) {
            QString htmlSym, uniSym, unitStr, dummyName;
            FittingParameterChart::getParamDisplayInfo(param.name, dummyName, htmlSym, uniSym, unitStr);
            if(unitStr == "无因次" || unitStr == "小数") unitStr = "";
            out << QString("%1,%2,%3,%4\n").arg(param.displayName).arg(uniSym).arg(param.value, 0, 'g', 10).arg(unitStr);
        }
    } else {
        for(const auto& param : params) {
            QString htmlSym, uniSym, unitStr, dummyName;
            FittingParameterChart::getParamDisplayInfo(param.name, dummyName, htmlSym, uniSym, unitStr);
            if(unitStr == "无因次" || unitStr == "小数") unitStr = "";
            out << QString("%1 (%2): %3 %4").arg(param.displayName).arg(uniSym).arg(param.value, 0, 'g', 10).arg(unitStr) << "\n";
        }
    }
    file.close();
    Standard_MessageBox::info(this, "完成", "参数数据已成功导出。");
}

void FittingWidget::onExportCurveData() {
    QString defaultDir = SettingPaths::reportPath();
    if(defaultDir.isEmpty()) defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";
    QString path = QFileDialog::getSaveFileName(this, "导出拟合曲线数据", defaultDir + "/FittingCurves.csv", "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    auto graphObsP = m_plotLogLog->graph(0);
    auto graphObsD = m_plotLogLog->graph(1);
    if (!graphObsP) return;

    QCPGraph *graphModP = (m_plotLogLog->graphCount() > 2) ? m_plotLogLog->graph(2) : nullptr;
    QCPGraph *graphModD = (m_plotLogLog->graphCount() > 3) ? m_plotLogLog->graph(3) : nullptr;
    QFile f(path);

    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "Obs_Time,Obs_DP,Obs_Deriv,Model_Time,Model_DP,Model_Deriv\n";
        auto itObsP = graphObsP->data()->begin();
        auto itObsD = graphObsD->data()->begin();
        auto endObsP = graphObsP->data()->end();

        QCPGraphDataContainer::const_iterator itModP, endModP, itModD;
        bool hasModel = (graphModP != nullptr && graphModD != nullptr);
        if(hasModel) {
            itModP = graphModP->data()->begin();
            endModP = graphModP->data()->end();
            itModD = graphModD->data()->begin();
        }

        while (itObsP != endObsP || (hasModel && itModP != endModP)) {
            QStringList line;
            if (itObsP != endObsP) {
                line << QString::number(itObsP->key, 'g', 10) << QString::number(itObsP->value, 'g', 10);
                if (itObsD != graphObsD->data()->end()) {
                    line << QString::number(itObsD->value, 'g', 10);
                    ++itObsD;
                } else {
                    line << "";
                }
                ++itObsP;
            } else {
                line << "" << "" << "";
            }

            if (hasModel && itModP != endModP) {
                line << QString::number(itModP->key, 'g', 10) << QString::number(itModP->value, 'g', 10);
                if (itModD != graphModD->data()->end()) {
                    line << QString::number(itModD->value, 'g', 10);
                    ++itModD;
                } else {
                    line << "";
                }
                ++itModP;
            } else {
                line << "" << "" << "";
            }
            out << line.join(",") << "\n";
        }
        f.close();
        Standard_MessageBox::info(this, "导出成功", "拟合曲线数据已保存。");
    }
}

void FittingWidget::on_btnExportReport_clicked() {
    QString wellName = "未命名井";
    QString projectFilePath = ModelParameter::instance()->getProjectFilePath();
    QFile pwtFile(projectFilePath);

    if (pwtFile.exists() && pwtFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = pwtFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject root = doc.object();
            if (root.contains("wellName")) {
                wellName = root["wellName"].toString();
            } else if (root.contains("basicParams")) {
                QJsonObject basic = root["basicParams"].toObject();
                if (basic.contains("wellName")) {
                    wellName = basic["wellName"].toString();
                }
            }
        }
        pwtFile.close();
    }

    if (wellName == "未命名井" || wellName.isEmpty()) {
        QFileInfo fi(projectFilePath);
        wellName = fi.completeBaseName();
    }

    FittingReportData reportData;
    reportData.wellName = wellName;
    reportData.modelType = m_currentModelType;
    QString mseText = ui->label_Error->text().remove("误差(MSE): ");
    reportData.mse = mseText.toDouble();
    reportData.t = m_obsTime;
    reportData.p = m_obsDeltaP;
    reportData.d = m_obsDerivative;
    m_paramChart->updateParamsFromTable();
    reportData.params = m_paramChart->getParameters();
    reportData.imgLogLog = getPlotImageBase64(m_plotLogLog);
    reportData.imgSemiLog = getPlotImageBase64(m_plotSemiLog);
    reportData.imgCartesian = getPlotImageBase64(m_plotCartesian);

    QString reportFileName = QString("%1试井解释报告.doc").arg(wellName);
    QString defaultDir = SettingPaths::reportPath();
    if(defaultDir.isEmpty() || defaultDir == ".") defaultDir = QFileInfo(projectFilePath).absolutePath();
    if(defaultDir.isEmpty() || defaultDir == ".") defaultDir = ModelParameter::instance()->getProjectPath();
    if(defaultDir.isEmpty()) defaultDir = ".";

    QString fileName = QFileDialog::getSaveFileName(this, "导出报告", defaultDir + "/" + reportFileName, "Word 文档 (*.doc);;HTML 文件 (*.html)");
    if(fileName.isEmpty()) return;

    QString errorMsg;
    if (FittingReportGenerator::generate(fileName, reportData, &errorMsg)) {
        Standard_MessageBox::info(this, "成功", QString("报告及数据已导出！\n\n文件路径: %1").arg(fileName));
    } else {
        Standard_MessageBox::error(this, "错误", "报告导出失败:\n" + errorMsg);
    }
}

QString FittingWidget::getPlotImageBase64(MouseZoom* plot) {
    if(!plot) return "";
    QPixmap pixmap = plot->toPixmap(800, 600);
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    return QString::fromLatin1(byteArray.toBase64().data());
}

void FittingWidget::on_btnSaveFit_clicked() {
    emit sigRequestSave();
}

QJsonObject FittingWidget::getJsonState() const {
    const_cast<FittingWidget*>(this)->m_paramChart->updateParamsFromTable();
    QList<FitParameter> params = m_paramChart->getParameters();
    QJsonObject root;
    root["modelType"] = (int)m_currentModelType;
    root["modelName"] = ModelManager::getModelTypeName(m_currentModelType);
    root["fitWeightVal"] = ui->sliderWeight->value();
    root["useLimits"] = m_useLimits;

    QJsonObject plotRange;
    plotRange["xMin"] = m_plotLogLog->xAxis->range().lower;
    plotRange["xMax"] = m_plotLogLog->xAxis->range().upper;
    plotRange["yMin"] = m_plotLogLog->yAxis->range().lower;
    plotRange["yMax"] = m_plotLogLog->yAxis->range().upper;
    root["plotView"] = plotRange;

    QJsonObject semiLogRange;
    semiLogRange["xMin"] = m_plotSemiLog->xAxis->range().lower;
    semiLogRange["xMax"] = m_plotSemiLog->xAxis->range().upper;
    semiLogRange["yMin"] = m_plotSemiLog->yAxis->range().lower;
    semiLogRange["yMax"] = m_plotSemiLog->yAxis->range().upper;
    root["plotViewSemiLog"] = semiLogRange;

    if (m_chartManager) {
        FittingDataSettings settings = m_chartManager->getSettings();
        QJsonObject settingsObj;
        settingsObj["producingTime"] = settings.producingTime;
        settingsObj["initialPressure"] = settings.initialPressure;
        settingsObj["testType"] = (int)settings.testType;
        settingsObj["porosity"] = settings.porosity;
        settingsObj["thickness"] = settings.thickness;
        settingsObj["wellRadius"] = settings.wellRadius;
        settingsObj["viscosity"] = settings.viscosity;
        settingsObj["ct"] = settings.ct;
        settingsObj["fvf"] = settings.fvf;
        settingsObj["rate"] = settings.rate;
        settingsObj["skipRows"] = settings.skipRows;
        settingsObj["timeCol"] = settings.timeColIndex;
        settingsObj["presCol"] = settings.pressureColIndex;
        settingsObj["derivCol"] = settings.derivColIndex;
        settingsObj["lSpacing"] = settings.lSpacing;
        settingsObj["smoothing"] = settings.enableSmoothing;
        settingsObj["span"] = settings.smoothingSpan;
        root["dataSettings"] = settingsObj;
    }

    QJsonArray paramsArray;
    for(const auto& p : params) {
        QJsonObject pObj;
        pObj["name"] = p.name;
        pObj["value"] = p.value;
        pObj["isFit"] = p.isFit;
        pObj["min"] = p.min;
        pObj["max"] = p.max;
        pObj["isVisible"] = p.isVisible;
        pObj["step"] = p.step;
        paramsArray.append(pObj);
    }
    root["parameters"] = paramsArray;

    QJsonArray timeArr, pressArr, derivArr, rawPArr;
    for(double v : m_obsTime) timeArr.append(v);
    for(double v : m_obsDeltaP) pressArr.append(v);
    for(double v : m_obsDerivative) derivArr.append(v);
    for(double v : m_obsRawP) rawPArr.append(v);

    QJsonObject obsData;
    obsData["time"] = timeArr;
    obsData["pressure"] = pressArr;
    obsData["derivative"] = derivArr;
    obsData["rawPressure"] = rawPArr;
    root["observedData"] = obsData;

    root["useCustomSampling"] = m_isCustomSamplingEnabled;
    QJsonArray intervalArr;
    for(const auto& item : m_customIntervals) {
        QJsonObject obj;
        obj["start"] = item.tStart;
        obj["end"] = item.tEnd;
        obj["count"] = item.count;
        intervalArr.append(obj);
    }
    root["customIntervals"] = intervalArr;

    if (m_chartManager) {
        root["manualPressureFitState"] = m_chartManager->getManualPressureState();
    }
    root["fittingTimeMax"] = m_userDefinedTimeMax;
    return root;
}

void FittingWidget::loadFittingState(const QJsonObject& root) {
    if (root.isEmpty()) return;

    if (root.contains("modelType")) {
        int type = root["modelType"].toInt();
        m_currentModelType = (ModelManager::ModelType)type;
        ui->btn_modelSelect->setText(ModelSelect::getShortName(type + 1));
    }

    if (root.contains("useLimits")) {
        m_useLimits = root["useLimits"].toBool();
    } else {
        m_useLimits = false;
    }

    m_paramChart->resetParams(m_currentModelType);

    if (root.contains("dataSettings") && m_chartManager) {
        QJsonObject sObj = root["dataSettings"].toObject();
        FittingDataSettings settings;
        settings.producingTime = sObj["producingTime"].toDouble();
        settings.initialPressure = sObj["initialPressure"].toDouble();
        settings.testType = (WellTestType)sObj["testType"].toInt();
        settings.porosity = sObj["porosity"].toDouble();
        settings.thickness = sObj["thickness"].toDouble();
        settings.wellRadius = sObj["wellRadius"].toDouble();
        settings.viscosity = sObj["viscosity"].toDouble();
        settings.ct = sObj["ct"].toDouble();
        settings.fvf = sObj["fvf"].toDouble();
        settings.rate = sObj["rate"].toDouble();
        settings.skipRows = sObj["skipRows"].toInt();
        settings.timeColIndex = sObj["timeCol"].toInt();
        settings.pressureColIndex = sObj["presCol"].toInt();
        settings.derivColIndex = sObj["derivCol"].toInt();
        settings.lSpacing = sObj["lSpacing"].toDouble();
        settings.enableSmoothing = sObj["smoothing"].toBool();
        settings.smoothingSpan = sObj["span"].toDouble();
        m_chartManager->setSettings(settings);
    }

    QMap<QString, double> explicitParamsMap;
    if (root.contains("parameters")) {
        QJsonArray arr = root["parameters"].toArray();
        QList<FitParameter> currentParams = m_paramChart->getParameters();
        for(int i=0; i<arr.size(); ++i) {
            QJsonObject pObj = arr[i].toObject();
            QString name = pObj["name"].toString();
            for(auto& p : currentParams) {
                if(p.name == name) {
                    p.value = pObj["value"].toDouble();
                    p.isFit = pObj["isFit"].toBool();
                    p.min = pObj["min"].toDouble();
                    p.max = pObj["max"].toDouble();
                    if(pObj.contains("isVisible")) p.isVisible = pObj["isVisible"].toBool();
                    else p.isVisible = true;
                    if(pObj.contains("step")) p.step = pObj["step"].toDouble();
                    explicitParamsMap.insert(p.name, p.value);
                    break;
                }
            }
        }
        m_paramChart->setParameters(currentParams);
    }

    if (root.contains("fitWeightVal")) {
        ui->sliderWeight->setValue(root["fitWeightVal"].toInt());
    }

    if (root.contains("observedData")) {
        QJsonObject obs = root["observedData"].toObject();
        QJsonArray tArr = obs["time"].toArray();
        QJsonArray pArr = obs["pressure"].toArray();
        QJsonArray dArr = obs["derivative"].toArray();
        QJsonArray rawPArr;

        if (obs.contains("rawPressure")) {
            rawPArr = obs["rawPressure"].toArray();
        }

        QVector<double> t, p, d, rawP;
        for(auto v : tArr) t.append(v.toDouble());
        for(auto v : pArr) p.append(v.toDouble());
        for(auto v : dArr) d.append(v.toDouble());
        for(auto v : rawPArr) rawP.append(v.toDouble());

        setObservedData(t, p, d, rawP);
    }

    if (root.contains("useCustomSampling")) {
        m_isCustomSamplingEnabled = root["useCustomSampling"].toBool();
    }

    if (root.contains("customIntervals")) {
        m_customIntervals.clear();
        QJsonArray arr = root["customIntervals"].toArray();
        for(auto v : arr) {
            QJsonObject obj = v.toObject();
            SamplingInterval item;
            item.tStart = obj["start"].toDouble();
            item.tEnd = obj["end"].toDouble();
            item.count = obj["count"].toInt();
            m_customIntervals.append(item);
        }
        if(m_core) m_core->setSamplingSettings(m_customIntervals, m_isCustomSamplingEnabled);
    }

    if (root.contains("fittingTimeMax")) {
        m_userDefinedTimeMax = root["fittingTimeMax"].toDouble();
    } else {
        m_userDefinedTimeMax = -1.0;
    }

    hideUnwantedParams();
    updateModelCurve(&explicitParamsMap);

    if (root.contains("plotView")) {
        QJsonObject range = root["plotView"].toObject();
        if (range.contains("xMin") && range.contains("xMax")) {
            double xMin = range["xMin"].toDouble();
            double xMax = range["xMax"].toDouble();
            double yMin = range["yMin"].toDouble();
            double yMax = range["yMax"].toDouble();
            if (xMax > xMin && yMax > yMin && xMin > 0 && yMin > 0) {
                m_plotLogLog->xAxis->setRange(xMin, xMax);
                m_plotLogLog->yAxis->setRange(yMin, yMax);
                m_plotLogLog->replot();
            }
        }
    }

    if (root.contains("plotViewSemiLog")) {
        QJsonObject range = root["plotViewSemiLog"].toObject();
        if (range.contains("xMin") && range.contains("xMax")) {
            double xMin = range["xMin"].toDouble();
            double xMax = range["xMax"].toDouble();
            double yMin = range["yMin"].toDouble();
            double yMax = range["yMax"].toDouble();
            if (xMax > xMin || (xMax < xMin)) {
                m_plotSemiLog->xAxis->setRange(xMin, xMax);
                m_plotSemiLog->yAxis->setRange(yMin, yMax);
                m_plotSemiLog->replot();
            }
        }
    }

    if (root.contains("manualPressureFitState") && m_chartManager) {
        m_chartManager->setManualPressureState(root["manualPressureFitState"].toObject());
    }
}
