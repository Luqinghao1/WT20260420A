/*
 * 文件名: wt_modelwidget.cpp
 * 作用与功能:
 * 1. 构建试井图版正演计算界面，全面接管 324 种模型的界面交互与计算调度。
 * 2. 实例化并调度 modelsolver07、08、09 求解器 (处理 Model 217~324)。
 * 3. 支持裂缝位置 (xfEdit) 的带括号数组解析，并对数组长度与 nf 数量进行强制校验。
 * 4. 优化了 UI 默认参数加载逻辑，非等长/非均布模型在重置时，会自动将后台的分布参数拼接为 "(x, y, z)" 的格式填入输入框。
 * 5. 彻底解决编译报错与多重嵌套缩进警告。
 */

#include "wt_modelwidget.h"
#include "ui_wt_modelwidget.h"
#include "modelmanager.h"
#include "modelparameter.h"
#include "modelselect.h"

#include <QDebug>
#include "standard_messagebox.h"
#include <QFileDialog>
#include <QTextStream>
#include <QCoreApplication>
#include <QSplitter>

/**
 * @brief 构造函数：实现控件分配、算法加载
 */
WT_ModelWidget::WT_ModelWidget(
    ModelType type, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::WT_ModelWidget)
    , m_type(type)
    , m_solver1(nullptr)
    , m_solver2(nullptr)
    , m_solver3(nullptr)
    , m_solver4(nullptr)
    , m_solver5(nullptr)
    , m_solver6(nullptr)
    , m_solver7(nullptr)
    , m_solver8(nullptr)
    , m_solver9(nullptr)
    , m_highPrecision(true)
{
    ui->setupUi(this);

    // 动态挂载数学引擎，全面覆盖 324 个物理模型
    int id = (int)m_type;
    if (id >= 0 && id <= 35) {
        m_solver1 = new ModelSolver01((ModelSolver01::ModelType)id);
    } else if (id >= 36 && id <= 71) {
        m_solver2 = new ModelSolver02((ModelSolver02::ModelType)(id - 36));
    } else if (id >= 72 && id <= 107) {
        m_solver3 = new ModelSolver03((ModelSolver03::ModelType)(id - 72));
    } else if (id >= 108 && id <= 143) {
        m_solver4 = new ModelSolver04((ModelSolver04::ModelType)(id - 108));
    } else if (id >= 144 && id <= 179) {
        m_solver5 = new ModelSolver05((ModelSolver05::ModelType)(id - 144));
    } else if (id >= 180 && id <= 215) {
        m_solver6 = new ModelSolver06((ModelSolver06::ModelType)(id - 180));
    } else if (id >= 216 && id <= 251) {
        m_solver7 = new ModelSolver07((ModelSolver07::ModelType)(id - 216));
    } else if (id >= 252 && id <= 287) {
        m_solver8 = new ModelSolver08((ModelSolver08::ModelType)(id - 252));
    } else if (id >= 288 && id <= 323) {
        m_solver9 = new ModelSolver09((ModelSolver09::ModelType)(id - 288));
    }

    // 敏感性分析色板
    m_colorList = {
        Qt::red, Qt::blue,
        QColor(0,180,0), Qt::magenta,
        QColor(255,140,0), Qt::cyan
    };

    // 锁定参数和绘图面板比例
    QList<int> sizes;
    sizes << 260 << 940;
    ui->splitter->setSizes(sizes);
    ui->splitter->setCollapsible(0, false);

    // 统一配置动态短名按钮展示
    ui->btnSelectModel->setText(ModelSelect::getShortName(id + 1));

    initUi();
    initChart();
    setupConnections();
    onResetParameters();
}

/**
 * @brief 析构函数
 */
WT_ModelWidget::~WT_ModelWidget()
{
    if(m_solver1) delete m_solver1;
    if(m_solver2) delete m_solver2;
    if(m_solver3) delete m_solver3;
    if(m_solver4) delete m_solver4;
    if(m_solver5) delete m_solver5;
    if(m_solver6) delete m_solver6;
    if(m_solver7) delete m_solver7;
    if(m_solver8) delete m_solver8;
    if(m_solver9) delete m_solver9;
    delete ui;
}

/**
 * @brief 获取描述长名
 */
QString WT_ModelWidget::getModelName() const {
    return ModelSelect::getShortName((int)m_type + 1);
}

/**
 * @brief 提取当前界面文本
 */
QMap<QString, QString> WT_ModelWidget::getUiTexts() const {
    QMap<QString, QString> texts;
    QList<QLineEdit*> edits =
        this->findChildren<QLineEdit*>();
    for (QLineEdit* edit : edits) {
        texts[edit->objectName()] = edit->text();
    }
    return texts;
}

/**
 * @brief 恢复历史文本
 */
void WT_ModelWidget::setUiTexts(
    const QMap<QString, QString>& texts)
{
    for (auto it = texts.begin();
         it != texts.end(); ++it)
    {
        if (QLineEdit* edit =
            this->findChild<QLineEdit*>(it.key()))
        {
            edit->setText(it.value());
        }
    }
}

/**
 * @brief 图版计算透传，根据引擎挂载路由
 */
WT_ModelWidget::ModelCurveData
WT_ModelWidget::calculateTheoreticalCurve(
    const QMap<QString, double>& params,
    const QVector<double>& providedTime)
{
    if (m_solver1) return m_solver1->calculateTheoreticalCurve(params, providedTime);
    if (m_solver2) return m_solver2->calculateTheoreticalCurve(params, providedTime);
    if (m_solver3) return m_solver3->calculateTheoreticalCurve(params, providedTime);
    if (m_solver4) return m_solver4->calculateTheoreticalCurve(params, providedTime);
    if (m_solver5) return m_solver5->calculateTheoreticalCurve(params, providedTime);
    if (m_solver6) return m_solver6->calculateTheoreticalCurve(params, providedTime);
    if (m_solver7) return m_solver7->calculateTheoreticalCurve(params, providedTime);
    if (m_solver8) return m_solver8->calculateTheoreticalCurve(params, providedTime);
    if (m_solver9) return m_solver9->calculateTheoreticalCurve(params, providedTime);
    return ModelCurveData();
}

/**
 * @brief 设置反演精度
 */
void WT_ModelWidget::setHighPrecision(bool high) {
    m_highPrecision = high;
    if (m_solver1) m_solver1->setHighPrecision(high);
    if (m_solver2) m_solver2->setHighPrecision(high);
    if (m_solver3) m_solver3->setHighPrecision(high);
    if (m_solver4) m_solver4->setHighPrecision(high);
    if (m_solver5) m_solver5->setHighPrecision(high);
    if (m_solver6) m_solver6->setHighPrecision(high);
    if (m_solver7) m_solver7->setHighPrecision(high);
    if (m_solver8) m_solver8->setHighPrecision(high);
    if (m_solver9) m_solver9->setHighPrecision(high);
}

/**
 * @brief 根据模型掩藏无用控件，新增了针对 xfEdit 裂缝位置的动态显隐
 */
void WT_ModelWidget::initUi() {
    // 隐藏废弃的 eta12
    if (QWidget* etaEdit =
        this->findChild<QWidget*>("eta12Edit"))
    {
        if (QLineEdit* le = qobject_cast<QLineEdit*>(etaEdit)) {
            le->setReadOnly(true);
            le->setStyleSheet(
                "background-color: #f0f0f0; color: #808080;");
        }
    }

    // 将界面的流度比 M12 描述文字强制修改为 外区渗透率 k2
    if (QLabel* lbl = this->findChild<QLabel*>("label_km")) {
        lbl->setText("外区渗透率 k2");
    }
    if (QLabel* lbl = this->findChild<QLabel*>("label_M12")) {
        lbl->setText("外区渗透率 k2");
    }

    // 动态显隐“裂缝位置 xf”输入框：只有选择了 >=216 的模型（非均布模型）才显示
    bool isUnequalPosition = (m_type >= 216);
    if (QWidget* w = this->findChild<QWidget*>("label_xf")) {
        w->setVisible(isUnequalPosition);
    }
    if (QWidget* w = this->findChild<QWidget*>("xfEdit")) {
        w->setVisible(isUnequalPosition);
    }

    // 通过取余 108 使 1-108, 109-216, 217-324 都能复用基础储层判断逻辑
    int tBase = (int)m_type % 108;

    int groupIdx = tBase % 12;
    bool isInfinite = (groupIdx < 4);
    if(ui->label_reD)
        ui->label_reD->setVisible(!isInfinite);
    if(ui->reDEdit)
        ui->reDEdit->setVisible(!isInfinite);

    int storageType = tBase % 4;
    bool hasBasicStorage = (storageType != 1);
    if(ui->label_cD)
        ui->label_cD->setVisible(hasBasicStorage);
    if(ui->cDEdit)
        ui->cDEdit->setVisible(hasBasicStorage);
    if(ui->label_s)
        ui->label_s->setVisible(hasBasicStorage);
    if(ui->sEdit)
        ui->sEdit->setVisible(hasBasicStorage);

    bool hasVarStorage =
        (storageType == 2 || storageType == 3);
    if(ui->label_alpha)
        ui->label_alpha->setVisible(hasVarStorage);
    if(ui->alphaEdit)
        ui->alphaEdit->setVisible(hasVarStorage);
    if(ui->label_cphi)
        ui->label_cphi->setVisible(hasVarStorage);
    if(ui->cPhiEdit)
        ui->cPhiEdit->setVisible(hasVarStorage);

    // 复杂介质逻辑辨识
    bool hasInD = false, hasOutD = false;
    bool hasInT = false, hasOutTS = false;

    if (tBase <= 35) {
        if (tBase <= 23) hasInD = true;
        if (tBase <= 11) hasOutD = true;
    } else if (tBase <= 71) {
        hasInD = true;
        int sub = tBase - 36;
        if (sub <= 11 || sub >= 24) hasOutD = true;
    } else {
        hasInT = true;
        int sub3 = tBase - 72;
        if (sub3 < 24) hasOutTS = true;
    }

    if(ui->label_omga1)
        ui->label_omga1->setVisible(hasInD);
    if(ui->omga1Edit)
        ui->omga1Edit->setVisible(hasInD);
    if(ui->label_remda1)
        ui->label_remda1->setVisible(hasInD);
    if(ui->remda1Edit)
        ui->remda1Edit->setVisible(hasInD);
    if(ui->label_omga2)
        ui->label_omga2->setVisible(hasOutD);
    if(ui->omga2Edit)
        ui->omga2Edit->setVisible(hasOutD);
    if(ui->label_remda2)
        ui->label_remda2->setVisible(hasOutD);
    if(ui->remda2Edit)
        ui->remda2Edit->setVisible(hasOutD);

    // 动态显隐多孔介质特征控件
    auto setVis = [this](const QString& n, bool v) {
        if (QWidget* w = this->findChild<QWidget*>(n))
            w->setVisible(v);
    };

    setVis("label_omega_f1", hasInT);
    setVis("omega_f1Edit", hasInT);
    setVis("label_omega_v1", hasInT);
    setVis("omega_v1Edit", hasInT);
    setVis("label_lambda_m1", hasInT);
    setVis("lambda_m1Edit", hasInT);
    setVis("label_lambda_v1", hasInT);
    setVis("lambda_v1Edit", hasInT);
    setVis("label_omega_f2", hasOutTS);
    setVis("omega_f2Edit", hasOutTS);
    setVis("label_lambda_m2", hasOutTS);
    setVis("lambda_m2Edit", hasOutTS);
}

/**
 * @brief 初始化图表绘制类
 */
void WT_ModelWidget::initChart() {
    MouseZoom* plot = ui->chartWidget->getPlot();
    plot->setBackground(Qt::white);
    plot->axisRect()->setBackground(Qt::white);

    QSharedPointer<QCPAxisTickerLog> logTicker(
        new QCPAxisTickerLog);

    plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    plot->xAxis->setTicker(logTicker);
    plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    plot->yAxis->setTicker(logTicker);

    plot->xAxis->setNumberFormat("eb");
    plot->xAxis->setNumberPrecision(0);
    plot->yAxis->setNumberFormat("eb");
    plot->yAxis->setNumberPrecision(0);

    QFont lF("Microsoft YaHei", 10, QFont::Bold);
    QFont tF("Microsoft YaHei", 9);
    plot->xAxis->setLabel("测试时间 t (h)");
    plot->yAxis->setLabel(
        "压力差 \xce\x94p & 压力导数 d(\xce\x94p)/d(ln t) (MPa)");

    plot->xAxis->setLabelFont(lF);
    plot->yAxis->setLabelFont(lF);
    plot->xAxis->setTickLabelFont(tF);
    plot->yAxis->setTickLabelFont(tF);

    plot->xAxis2->setVisible(true);
    plot->yAxis2->setVisible(true);
    plot->xAxis2->setTickLabels(false);
    plot->yAxis2->setTickLabels(false);

    connect(plot->xAxis,
            SIGNAL(rangeChanged(QCPRange)),
            plot->xAxis2,
            SLOT(setRange(QCPRange)));

    connect(plot->yAxis,
            SIGNAL(rangeChanged(QCPRange)),
            plot->yAxis2,
            SLOT(setRange(QCPRange)));

    plot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    plot->yAxis2->setScaleType(QCPAxis::stLogarithmic);
    plot->xAxis2->setTicker(logTicker);
    plot->yAxis2->setTicker(logTicker);

    plot->xAxis->grid()->setVisible(true);
    plot->yAxis->grid()->setVisible(true);
    plot->xAxis->grid()->setSubGridVisible(true);
    plot->yAxis->grid()->setSubGridVisible(true);

    plot->xAxis->grid()->setPen(
        QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    plot->yAxis->grid()->setPen(
        QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    plot->xAxis->grid()->setSubGridPen(
        QPen(QColor(240, 240, 240), 1, Qt::DotLine));
    plot->yAxis->grid()->setSubGridPen(
        QPen(QColor(240, 240, 240), 1, Qt::DotLine));

    plot->xAxis->setRange(1e-3, 1e4);
    plot->yAxis->setRange(1e-3, 1e2);

    plot->legend->setVisible(true);
    plot->legend->setFont(QFont("Microsoft YaHei", 9));
    plot->legend->setBrush(QBrush(QColor(255, 255, 255, 200)));

    ui->chartWidget->setTitle(
        "多段压裂水平井有因次压力双对数响应曲线");
}

/**
 * @brief 事件连接
 */
void WT_ModelWidget::setupConnections() {
    connect(ui->calculateButton,
            &QPushButton::clicked,
            this,
            &WT_ModelWidget::onCalculateClicked);

    connect(ui->resetButton,
            &QPushButton::clicked,
            this,
            &WT_ModelWidget::onResetParameters);

    connect(ui->chartWidget,
            &ChartWidget::exportDataTriggered,
            this,
            &WT_ModelWidget::onExportData);

    connect(ui->btnExportDataTab,
            &QPushButton::clicked,
            this,
            &WT_ModelWidget::onExportData);

    connect(ui->checkShowPoints,
            &QCheckBox::toggled,
            this,
            &WT_ModelWidget::onShowPointsToggled);

    connect(ui->btnSelectModel,
            &QPushButton::clicked,
            this,
            &WT_ModelWidget::requestModelSelection);
}

/**
 * @brief 文本多输入解析
 * 安全处理各种中文、全角字符和空格的嵌套数组格式
 */
QVector<double> WT_ModelWidget::parseInput(const QString& text) {
    QVector<double> vals;
    QString clText = text;
    clText.replace("，", ",");

    // 安全移除圆括号，避免全角字符在常量中的警告
    clText.remove('(');
    clText.remove(')');
    clText.remove(QChar(0xFF08)); // 全角左括号
    clText.remove(QChar(0xFF09)); // 全角右括号

    QStringList parts = clText.split(
        ",", Qt::SkipEmptyParts);

    for(const QString& part : parts) {
        bool ok;
        double v = part.trimmed().toDouble(&ok);
        if(ok) vals.append(v);
    }
    if(vals.isEmpty()) vals.append(0.0);
    return vals;
}

/**
 * @brief 安全赋值
 */
void WT_ModelWidget::setInputText(
    QLineEdit* edit, double value)
{
    if(edit) edit->setText(
            QString::number(value, 'g', 8));
}

/**
 * @brief 向中枢请求混合默认字典注入，智能填充阵列数据 (x, y, z)
 */
void WT_ModelWidget::onResetParameters() {
    QMap<QString, double> defs =
        ModelManager::getDefaultParameters(
            (ModelManager::ModelType)m_type);

    ui->tEdit->setText(
        QString::number(defs.value("t", 1e10), 'g', 8));
    ui->pointsEdit->setText(
        QString::number(defs.value("points", 100), 'g', 8));

    setInputText(ui->phiEdit, defs.value("phi"));
    setInputText(ui->hEdit,   defs.value("h"));
    setInputText(ui->rwEdit,  defs.value("rw"));
    setInputText(ui->muEdit,  defs.value("mu"));
    setInputText(ui->BEdit,   defs.value("B"));
    setInputText(ui->CtEdit,  defs.value("Ct"));
    setInputText(ui->qEdit,   defs.value("q"));

    setInputText(ui->kfEdit,  defs.value("kf"));

    // 读取 k2 默认值，并显示在 kmEdit (被转用的控件)
    setInputText(ui->kmEdit,  defs.value("k2", 10.0));

    setInputText(ui->LEdit,   defs.value("L"));
    setInputText(ui->nfEdit,  defs.value("nf"));

    // 动态填充裂缝半长阵列 (Lf)
    if (m_type >= 108) {
        int nf = (int)defs.value("nf", 9);
        QStringList lfList;
        for(int i = 1; i <= nf; ++i) {
            if (defs.contains(QString("Lf_%1").arg(i))) {
                lfList.append(QString::number(defs.value(QString("Lf_%1").arg(i)), 'g', 6));
            }
        }
        if (!lfList.isEmpty()) {
            ui->LfEdit->setText("(" + lfList.join(", ") + ")");
        }
    } else {
        setInputText(ui->LfEdit,  defs.value("Lf"));
    }

    // 动态填充裂缝位置阵列 (xf)
    if (QLineEdit* xfEdit = this->findChild<QLineEdit*>("xfEdit")) {
        if (m_type >= 216) {
            int nf = (int)defs.value("nf", 9);
            QStringList xfList;
            for(int i = 1; i <= nf; ++i) {
                if (defs.contains(QString("xf_%1").arg(i))) {
                    xfList.append(QString::number(defs.value(QString("xf_%1").arg(i)), 'g', 6));
                }
            }
            if (!xfList.isEmpty()) {
                xfEdit->setText("(" + xfList.join(", ") + ")");
            }
        } else {
            xfEdit->setText("0.0"); // 对非适用模型提供安全重置
        }
    }

    setInputText(ui->rmDEdit, defs.value("rm"));
    setInputText(ui->gamaDEdit, defs.value("gamaD"));

    if (ui->reDEdit)
        setInputText(ui->reDEdit, defs.value("re", 200000.0));
    if (ui->cDEdit)
        setInputText(ui->cDEdit, defs.value("cD", 0.1));
    if (ui->sEdit)
        setInputText(ui->sEdit, defs.value("S", 1.0));
    if (ui->alphaEdit)
        setInputText(ui->alphaEdit, defs.value("alpha", 0.1));
    if (ui->cPhiEdit)
        setInputText(ui->cPhiEdit, defs.value("C_phi", 1e-4));

    if (ui->omga1Edit)
        setInputText(ui->omga1Edit, defs.value("omega1", 0.1));
    if (ui->omga2Edit)
        setInputText(ui->omga2Edit, defs.value("omega2", 0.001));
    if (ui->remda1Edit)
        setInputText(ui->remda1Edit, defs.value("lambda1", 2e-3));
    if (ui->remda2Edit)
        setInputText(ui->remda2Edit, defs.value("lambda2", 1e-3));

    setInputText(
        this->findChild<QLineEdit*>("omega_f1Edit"),
        defs.value("omega_f1", 0.02));
    setInputText(
        this->findChild<QLineEdit*>("omega_v1Edit"),
        defs.value("omega_v1", 0.01));
    setInputText(
        this->findChild<QLineEdit*>("lambda_m1Edit"),
        defs.value("lambda_m1", 1e-4));
    setInputText(
        this->findChild<QLineEdit*>("lambda_v1Edit"),
        defs.value("lambda_v1", 1e-1));
    setInputText(
        this->findChild<QLineEdit*>("omega_f2Edit"),
        defs.value("omega_f2", 0.008));
    setInputText(
        this->findChild<QLineEdit*>("lambda_m2Edit"),
        defs.value("lambda_m2", 1e-7));
}

void WT_ModelWidget::onDependentParamsChanged() {}

/**
 * @brief 散点图开关
 */
void WT_ModelWidget::onShowPointsToggled(bool checked) {
    MouseZoom* plot = ui->chartWidget->getPlot();
    for(int i = 0; i < plot->graphCount(); ++i) {
        if (checked) {
            plot->graph(i)->setScatterStyle(
                QCPScatterStyle(QCPScatterStyle::ssDisc, 5));
        } else {
            plot->graph(i)->setScatterStyle(
                QCPScatterStyle::ssNone);
        }
    }
    plot->replot();
}

/**
 * @brief 计算指令拉起
 */
void WT_ModelWidget::onCalculateClicked() {
    ui->calculateButton->setEnabled(false);
    ui->calculateButton->setText("计算中...");
    QCoreApplication::processEvents();
    runCalculation();
    ui->calculateButton->setEnabled(true);
    ui->calculateButton->setText("开始计算");
}

/**
 * @brief 计算与装配主逻辑
 */
void WT_ModelWidget::runCalculation() {
    MouseZoom* plot = ui->chartWidget->getPlot();
    plot->clearGraphs();

    QMap<QString, QVector<double>> raw;

    auto safeP = [&](QLineEdit* e) {
        return e ? parseInput(e->text()) : QVector<double>{0.0};
    };
    auto safeFP = [&](const QString& n) {
        return safeP(this->findChild<QLineEdit*>(n));
    };

    raw["phi"] = safeP(ui->phiEdit);
    raw["h"]   = safeP(ui->hEdit);
    raw["rw"]  = safeP(ui->rwEdit);
    raw["mu"]  = safeP(ui->muEdit);
    raw["B"]   = safeP(ui->BEdit);
    raw["Ct"]  = safeP(ui->CtEdit);
    raw["q"]   = safeP(ui->qEdit);
    raw["t"]   = safeP(ui->tEdit);

    raw["kf"]  = safeP(ui->kfEdit);

    // 捕获外区渗透率 k2，并自动为底层引擎推算无因次的 M12
    raw["k2"]  = safeP(ui->kmEdit);

    QVector<double> m12_vec;
    for(int i = 0; i < qMax(raw["kf"].size(), raw["k2"].size()); ++i) {
        double kf_v = raw["kf"][i % raw["kf"].size()];
        double k2_v = raw["k2"][i % raw["k2"].size()];
        m12_vec.append(k2_v < 1e-12 ? 1.0 : (kf_v / k2_v));
    }
    raw["M12"] = m12_vec;
    raw["eta12"] = m12_vec;

    raw["L"]     = safeP(ui->LEdit);
    raw["nf"]    = safeP(ui->nfEdit);

    // 安全抓取动态阵列
    raw["Lf"]    = safeP(ui->LfEdit);
    raw["xf"]    = safeFP("xfEdit");

    raw["rm"]    = safeP(ui->rmDEdit);
    raw["gamaD"] = safeP(ui->gamaDEdit);

    raw["omega1"]  = safeP(ui->omga1Edit);
    raw["omega2"]  = safeP(ui->omga2Edit);
    raw["lambda1"] = safeP(ui->remda1Edit);
    raw["lambda2"] = safeP(ui->remda2Edit);

    raw["omega_f1"]  = safeFP("omega_f1Edit");
    raw["omega_v1"]  = safeFP("omega_v1Edit");
    raw["lambda_m1"] = safeFP("lambda_m1Edit");
    raw["lambda_v1"] = safeFP("lambda_v1Edit");
    raw["omega_f2"]  = safeFP("omega_f2Edit");
    raw["lambda_m2"] = safeFP("lambda_m2Edit");

    raw["alpha"] = ui->alphaEdit && ui->alphaEdit->isVisible() ?
                       safeP(ui->alphaEdit) : QVector<double>{0.1};
    raw["C_phi"] = ui->cPhiEdit && ui->cPhiEdit->isVisible() ?
                       safeP(ui->cPhiEdit) : QVector<double>{1e-4};
    raw["re"]    = ui->reDEdit && ui->reDEdit->isVisible() ?
                    safeP(ui->reDEdit) : QVector<double>{20000.0};

    if (ui->cDEdit && ui->cDEdit->isVisible()) {
        raw["cD"] = safeP(ui->cDEdit);
        raw["S"] = safeP(ui->sEdit);
    } else {
        raw["cD"] = {0.0};
        raw["S"] = {0.0};
    }

    // ============================================
    // 【重要逻辑】非等长/非均布模型的裂缝参数数量校验
    // ============================================
    int nf = raw["nf"].isEmpty() ? 1 : raw["nf"].first();

    // 校验 1：如果 >= 108，必须校验 Lf 数量是否匹配
    if (m_type >= 108) {
        QVector<double> lfArr = raw.value("Lf");
        if (!lfArr.isEmpty() && lfArr.size() != nf) {
            Standard_MessageBox::warning(this, "参数错误",
                                         QString("非等长裂缝模型要求输入的裂缝半长数量必须与裂缝条数一致！\n当前裂缝条数: %1，输入半长数量: %2")
                                             .arg(nf).arg(lfArr.size()));
            ui->calculateButton->setEnabled(true);
            ui->calculateButton->setText("开始计算");
            return;
        }
    }

    // 校验 2：如果 >= 216，必须追加校验 xf 数量是否匹配
    if (m_type >= 216) {
        QVector<double> xfArr = raw.value("xf");
        if (!xfArr.isEmpty() && xfArr.size() != nf) {
            Standard_MessageBox::warning(this, "参数错误",
                                         QString("非均布裂缝模型要求输入的裂缝位置数量必须与裂缝条数一致！\n当前裂缝条数: %1，输入位置数量: %2")
                                             .arg(nf).arg(xfArr.size()));
            ui->calculateButton->setEnabled(true);
            ui->calculateButton->setText("开始计算");
            return;
        }
    }

    // 敏感性维度抓取
    QString sensK = "";
    QVector<double> sensV;
    for(auto it = raw.begin(); it != raw.end(); ++it) {
        if(it.key() == "t") continue;

        // 阵列参数排除在敏感性分析循环枚举外，它们是单次传递的集合
        if(m_type >= 108 && (it.key() == "Lf" || it.key() == "LfD")) continue;
        if(m_type >= 216 && it.key() == "xf") continue;

        if(it.value().size() > 1) {
            sensK = it.key();
            sensV = it.value();
            break;
        }
    }
    bool isSens = !sensK.isEmpty();

    QMap<QString, double> bParams;
    for(auto it = raw.begin(); it != raw.end(); ++it) {
        bParams[it.key()] = it.value().isEmpty() ?
                                0.0 : it.value().first();
    }

    int nPts = ui->pointsEdit->text().toInt();
    if(nPts < 5) nPts = 5;
    double maxT = bParams.value("t", 10000.0);
    QVector<double> t =
        ModelManager::generateLogTimeSteps(
            nPts, -3.0, log10(maxT));

    int iter = isSens ? sensV.size() : 1;
    iter = qMin(iter, (int)m_colorList.size());

    QString rHead = QString("计算完成 (%1)\n")
                        .arg(getModelName());
    if(isSens)
        rHead += QString("敏感性分析参数: %1\n").arg(sensK);

    // 分发运算
    for(int i = 0; i < iter; ++i) {
        QMap<QString, double> cParams = bParams;

        // 解析注入独立的裂缝半长组
        if (m_type >= 108) {
            QVector<double> lfArr = raw.value("Lf");
            if (!lfArr.isEmpty()) {
                cParams["Lf"] = lfArr.first();
                cParams["Lf_Count"] = lfArr.size();
                for (int j = 0; j < lfArr.size(); ++j) {
                    cParams[QString("Lf_%1").arg(j)] = lfArr[j];
                }
            }
            QVector<double> lfdArr = raw.value("LfD");
            if (!lfdArr.isEmpty()) {
                cParams["LfD"] = lfdArr.first();
                cParams["LfD_Count"] = lfdArr.size();
                for (int j = 0; j < lfdArr.size(); ++j) {
                    cParams[QString("LfD_%1").arg(j)] = lfdArr[j];
                }
            }
        }

        // 解析注入独立的裂缝绝对位置组
        if (m_type >= 216) {
            QVector<double> xfArr = raw.value("xf");
            if (!xfArr.isEmpty()) {
                cParams["xf"] = xfArr.first();
                cParams["xf_Count"] = xfArr.size();
                for (int j = 0; j < xfArr.size(); ++j) {
                    cParams[QString("xf_%1").arg(j)] = xfArr[j];
                }
            }
        }

        double val = 0;
        if (isSens) {
            val = sensV[i];
            cParams[sensK] = val;
        }

        ModelCurveData res =
            calculateTheoreticalCurve(cParams, t);

        res_tD = std::get<0>(res);
        res_pD = std::get<1>(res);
        res_dpD = std::get<2>(res);

        QColor color = isSens ? m_colorList[i] : Qt::red;
        QString lgName = isSens ?
                             QString("%1 = %2").arg(sensK).arg(val) : "压力差 \xce\x94p";

        plotCurve(res, lgName, color, isSens);
    }

    QString resTxt = rHead;
    resTxt += "t (h)\t\tDp (MPa)\t\tdDp (MPa)\n";
    for(int i = 0; i < res_pD.size(); ++i) {
        double dp = (i < res_dpD.size()) ? res_dpD[i] : 0.0;
        resTxt += QString("%1\t%2\t%3\n")
                      .arg(res_tD[i],0,'e',4)
                      .arg(res_pD[i],0,'e',4)
                      .arg(dp,0,'e',4);
    }
    ui->resultTextEdit->setText(resTxt);

    plot->rescaleAxes();
    plot->replot();

    onShowPointsToggled(ui->checkShowPoints->isChecked());
    emit calculationCompleted(getModelName(), bParams);
}

/**
 * @brief 制图挂载
 */
void WT_ModelWidget::plotCurve(
    const ModelCurveData& data,
    const QString& name,
    QColor color,
    bool isSensitivity)
{
    MouseZoom* plot = ui->chartWidget->getPlot();
    const QVector<double>& t = std::get<0>(data);
    const QVector<double>& p = std::get<1>(data);
    const QVector<double>& d = std::get<2>(data);

    QCPGraph* gP = plot->addGraph();
    gP->setData(t, p);
    gP->setPen(QPen(color, 2, Qt::SolidLine));

    QCPGraph* gD = plot->addGraph();
    gD->setData(t, d);

    if (isSensitivity) {
        gD->setPen(QPen(color, 2, Qt::DashLine));
        gP->setName(name);
        gD->removeFromLegend();
    } else {
        gP->setPen(QPen(color, 2, Qt::SolidLine));
        gP->setName(name);
        gD->setPen(QPen(Qt::blue, 2, Qt::SolidLine));
        gD->setName("压力导数 d(\xce\x94p)/d(ln t)");
    }
}

/**
 * @brief 导出落地
 */
void WT_ModelWidget::onExportData() {
    if (res_tD.isEmpty()) {
        Standard_MessageBox::warning(this, "提示", "无曲线数据！");
        return;
    }

    QString dDir = ModelParameter::instance()->getProjectPath();
    if(dDir.isEmpty()) dDir = ".";

    QString path = QFileDialog::getSaveFileName(
        this, "导出CSV数据",
        dDir + "/CalculatedData.csv", "CSV Files (*.csv)");

    if (path.isEmpty()) return;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "t,Dp,dDp\n";
        for (int i = 0; i < res_tD.size(); ++i) {
            double dp = (i < res_dpD.size()) ? res_dpD[i] : 0.0;
            out << res_tD[i] << "," << res_pD[i] << "," << dp << "\n";
        }
        f.close();
        Standard_MessageBox::info(
            this, "导出成功", "成功保存至:\n" + path);
    }
}
