/*
 * 文件名: modelmanager.cpp
 * 作用与功能:
 * 1. 拦截并管理软件内全部 324 个试井模型界面的懒加载与生命周期堆栈。
 * 2. 作为中枢神经，负责将通用物理参数、界面默认参数统一派发给各个正演界面。
 * 3. 将字典下发底座中的 M12 全局剔除，替换为统一的外部渗透率 k2 (默认值 10.0)。
 * 4. 全面装载 modelsolver07、08、09 求解器，提供 xf 数组的默认生成与管理。
 * 5. 彻底规范了代码排版，消除了所有的 -Wmisleading-indentation 编译警告。
 */

#include "modelmanager.h"
#include "modelselect.h"
#include "modelparameter.h"
#include "wt_modelwidget.h"

// 引入九大物理求解引擎
#include "modelsolver01.h"
#include "modelsolver02.h"
#include "modelsolver03.h"
#include "modelsolver04.h"
#include "modelsolver05.h"
#include "modelsolver06.h"
#include "modelsolver07.h"
#include "modelsolver08.h"
#include "modelsolver09.h"

#include <QVBoxLayout>

ModelManager::ModelManager(QWidget* parent)
    : QObject(parent),
    m_mainWidget(nullptr),
    m_modelStack(nullptr),
    m_currentModelType(Model_1)
{
}

ModelManager::~ModelManager()
{
    // 安全释放所有已初始化的底层求解器内存，严谨的代码展开以避免警告
    for(auto* s : m_solversGroup1) {
        if(s) delete s;
    }
    for(auto* s : m_solversGroup2) {
        if(s) delete s;
    }
    for(auto* s : m_solversGroup3) {
        if(s) delete s;
    }
    for(auto* s : m_solversGroup4) {
        if(s) delete s;
    }
    for(auto* s : m_solversGroup5) {
        if(s) delete s;
    }
    for(auto* s : m_solversGroup6) {
        if(s) delete s;
    }
    for(auto* s : m_solversGroup7) {
        if(s) delete s;
    }
    for(auto* s : m_solversGroup8) {
        if(s) delete s;
    }
    for(auto* s : m_solversGroup9) {
        if(s) delete s;
    }
}

void ModelManager::initializeModels(QWidget* parentWidget)
{
    if (!parentWidget) return;

    createMainWidget();
    m_modelStack = new QStackedWidget(m_mainWidget);

    // 初始化 324 个 UI 界面缓存槽位
    m_modelWidgets.resize(324);
    m_modelWidgets.fill(nullptr);

    // 初始化 9 大求解器组的底层算法缓存槽位，各 36 个变体
    m_solversGroup1.resize(36); m_solversGroup1.fill(nullptr);
    m_solversGroup2.resize(36); m_solversGroup2.fill(nullptr);
    m_solversGroup3.resize(36); m_solversGroup3.fill(nullptr);
    m_solversGroup4.resize(36); m_solversGroup4.fill(nullptr);
    m_solversGroup5.resize(36); m_solversGroup5.fill(nullptr);
    m_solversGroup6.resize(36); m_solversGroup6.fill(nullptr);
    m_solversGroup7.resize(36); m_solversGroup7.fill(nullptr);
    m_solversGroup8.resize(36); m_solversGroup8.fill(nullptr);
    m_solversGroup9.resize(36); m_solversGroup9.fill(nullptr);

    m_mainWidget->layout()->addWidget(m_modelStack);

    // 默认展示第一个模型
    switchToModel(Model_1);

    // 挂载到父级面板
    if (parentWidget->layout()) {
        parentWidget->layout()->addWidget(m_mainWidget);
    } else {
        QVBoxLayout* lay = new QVBoxLayout(parentWidget);
        lay->addWidget(m_mainWidget);
        parentWidget->setLayout(lay);
    }
}

void ModelManager::createMainWidget()
{
    m_mainWidget = new QWidget();
    QVBoxLayout* ml = new QVBoxLayout(m_mainWidget);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(0);
    m_mainWidget->setLayout(ml);
}

WT_ModelWidget* ModelManager::ensureWidget(ModelType type)
{
    int i = (int)type;
    if (i < 0 || i >= 324) return nullptr;

    // 懒加载：如果界面还未创建，则动态生成并压入堆栈
    if (!m_modelWidgets[i]) {
        WT_ModelWidget* w = new WT_ModelWidget(type, m_modelStack);
        m_modelWidgets[i] = w;
        m_modelStack->addWidget(w);

        connect(w, &WT_ModelWidget::requestModelSelection, this, &ModelManager::onSelectModelClicked);
        connect(w, &WT_ModelWidget::calculationCompleted, this, &ModelManager::onWidgetCalculationCompleted);
    }
    return m_modelWidgets[i];
}

// === 底层算法实例的懒加载工厂 ===
ModelSolver01* ModelManager::ensureSolverGroup1(int idx) {
    if (!m_solversGroup1[idx]) {
        m_solversGroup1[idx] = new ModelSolver01((ModelSolver01::ModelType)idx);
    }
    return m_solversGroup1[idx];
}

ModelSolver02* ModelManager::ensureSolverGroup2(int idx) {
    if (!m_solversGroup2[idx]) {
        m_solversGroup2[idx] = new ModelSolver02((ModelSolver02::ModelType)idx);
    }
    return m_solversGroup2[idx];
}

ModelSolver03* ModelManager::ensureSolverGroup3(int idx) {
    if (!m_solversGroup3[idx]) {
        m_solversGroup3[idx] = new ModelSolver03((ModelSolver03::ModelType)idx);
    }
    return m_solversGroup3[idx];
}

ModelSolver04* ModelManager::ensureSolverGroup4(int idx) {
    if (!m_solversGroup4[idx]) {
        m_solversGroup4[idx] = new ModelSolver04((ModelSolver04::ModelType)idx);
    }
    return m_solversGroup4[idx];
}

ModelSolver05* ModelManager::ensureSolverGroup5(int idx) {
    if (!m_solversGroup5[idx]) {
        m_solversGroup5[idx] = new ModelSolver05((ModelSolver05::ModelType)idx);
    }
    return m_solversGroup5[idx];
}

ModelSolver06* ModelManager::ensureSolverGroup6(int idx) {
    if (!m_solversGroup6[idx]) {
        m_solversGroup6[idx] = new ModelSolver06((ModelSolver06::ModelType)idx);
    }
    return m_solversGroup6[idx];
}

ModelSolver07* ModelManager::ensureSolverGroup7(int idx) {
    if (!m_solversGroup7[idx]) {
        m_solversGroup7[idx] = new ModelSolver07((ModelSolver07::ModelType)idx);
    }
    return m_solversGroup7[idx];
}

ModelSolver08* ModelManager::ensureSolverGroup8(int idx) {
    if (!m_solversGroup8[idx]) {
        m_solversGroup8[idx] = new ModelSolver08((ModelSolver08::ModelType)idx);
    }
    return m_solversGroup8[idx];
}

ModelSolver09* ModelManager::ensureSolverGroup9(int idx) {
    if (!m_solversGroup9[idx]) {
        m_solversGroup9[idx] = new ModelSolver09((ModelSolver09::ModelType)idx);
    }
    return m_solversGroup9[idx];
}

void ModelManager::switchToModel(ModelType modelType)
{
    if (!m_modelStack) return;

    ModelType old = m_currentModelType;
    QMap<QString, QString> curT;

    // 记忆当前界面的输入状态
    if (m_modelWidgets[old]) {
        curT = m_modelWidgets[old]->getUiTexts();
    }

    m_currentModelType = modelType;
    WT_ModelWidget* w = ensureWidget(modelType);

    if (w) {
        if (!curT.isEmpty()) {
            w->setUiTexts(curT);
        }
        m_modelStack->setCurrentWidget(w);
    }

    emit modelSwitched(modelType, old);
}

ModelCurveData ModelManager::calculateTheoreticalCurve(ModelType type, const QMap<QString, double>& p, const QVector<double>& t)
{
    int id = (int)type;

    // 根据系统 ID 将任务路由分配给正确的底层模块进行并行运算
    if (id <= 35) {
        return ensureSolverGroup1(id)->calculateTheoreticalCurve(p, t);
    } else if (id <= 71) {
        return ensureSolverGroup2(id - 36)->calculateTheoreticalCurve(p, t);
    } else if (id <= 107) {
        return ensureSolverGroup3(id - 72)->calculateTheoreticalCurve(p, t);
    } else if (id <= 143) {
        return ensureSolverGroup4(id - 108)->calculateTheoreticalCurve(p, t);
    } else if (id <= 179) {
        return ensureSolverGroup5(id - 144)->calculateTheoreticalCurve(p, t);
    } else if (id <= 215) {
        return ensureSolverGroup6(id - 180)->calculateTheoreticalCurve(p, t);
    } else if (id <= 251) {
        return ensureSolverGroup7(id - 216)->calculateTheoreticalCurve(p, t);
    } else if (id <= 287) {
        return ensureSolverGroup8(id - 252)->calculateTheoreticalCurve(p, t);
    } else {
        return ensureSolverGroup9(id - 288)->calculateTheoreticalCurve(p, t);
    }
}

QString ModelManager::getModelTypeName(ModelType type)
{
    int id = (int)type;

    if (id <= 35) {
        return ModelSolver01::getModelName((ModelSolver01::ModelType)id);
    } else if (id <= 71) {
        return ModelSolver02::getModelName((ModelSolver02::ModelType)(id - 36));
    } else if (id <= 107) {
        return ModelSolver03::getModelName((ModelSolver03::ModelType)(id - 72));
    } else if (id <= 143) {
        return ModelSolver04::getModelName((ModelSolver04::ModelType)(id - 108));
    } else if (id <= 179) {
        return ModelSolver05::getModelName((ModelSolver05::ModelType)(id - 144));
    } else if (id <= 215) {
        return ModelSolver06::getModelName((ModelSolver06::ModelType)(id - 180));
    } else if (id <= 251) {
        return ModelSolver07::getModelName((ModelSolver07::ModelType)(id - 216));
    } else if (id <= 287) {
        return ModelSolver08::getModelName((ModelSolver08::ModelType)(id - 252));
    } else {
        return ModelSolver09::getModelName((ModelSolver09::ModelType)(id - 288));
    }
}

void ModelManager::onSelectModelClicked()
{
    ModelSelect dlg(m_mainWidget);
    dlg.setCurrentModelCode(QString("modelwidget%1").arg((int)m_currentModelType + 1));

    if (dlg.exec() == QDialog::Accepted) {
        QString numS = dlg.getSelectedModelCode();
        numS.remove("modelwidget");
        int mId = numS.toInt();

        if (mId >= 1 && mId <= 324) {
            switchToModel((ModelType)(mId - 1));
        }
    }
}

QMap<QString, double> ModelManager::getDefaultParameters(ModelType type)
{
    QMap<QString, double> p;
    ModelParameter* mp = ModelParameter::instance();

    // 第一级：基础物理常数
    p.insert("phi", mp->getPhi());
    p.insert("h", mp->getH());
    p.insert("rw", mp->getRw());
    p.insert("mu", mp->getMu());
    p.insert("B", mp->getB());
    p.insert("Ct", mp->getCt());
    p.insert("q", mp->getQ());
    p.insert("L", mp->getL());
    p.insert("nf", mp->getNf());
    p.insert("alpha", mp->getAlpha());
    p.insert("C_phi", mp->getCPhi());

    // 第二级：试井扩展参数
    p.insert("kf", 10.0);
    p.insert("rm", 50000.0);
    p.insert("gamaD", 0.006);

    // 全局插入外区渗透率 k2，代替废弃的 M12
    p.insert("k2", 10.0);

    int t = (int)type;
    int tBase = t % 108; // 基础储层/边界种类编号 (0-107)

    bool isUnequalLength = (t >= 108); // 只要大于等于108，必然是非等长模型
    bool isUnequalPosition = (t >= 216); // 大于等于216，为非均布非等长模型

    int nf = (int)mp->getNf();

    // 动态装载裂缝半长 (Lf)
    if (isUnequalLength) {
        for (int i = 1; i <= nf; ++i) {
            p.insert(QString("Lf_%1").arg(i), 50.0);
        }
    } else {
        p.insert("Lf", 50.0);
    }

    // 动态装载裂缝位置 (xf)
    if (isUnequalPosition) {
        double L_total = mp->getL();
        if (L_total < 1e-9) {
            L_total = 1000.0;
        }

        for (int i = 1; i <= nf; ++i) {
            // 均匀分布作为初始默认位置: 距离起点的 5% 到 95%
            double pos = 0.05 * L_total + 0.9 * L_total * (i - 1) / std::max(1, nf - 1);
            p.insert(QString("xf_%1").arg(i), pos);
        }
    }

    bool isMix = (tBase >= 72 && tBase <= 107);
    int gI = tBase % 12;
    if (gI >= 4) {
        p.insert("re", isMix ? 500000.0 : 20000.0);
    }

    if (isMix) {
        int sub3 = tBase - 72;
        p.insert("omega_f1", 0.02);
        p.insert("omega_v1", 0.01);
        p.insert("lambda_m1", 4e-4);
        p.insert("lambda_v1", 1e-4);
        if (sub3 < 24) {
            p.insert("omega_f2", 0.008);
            p.insert("lambda_m2", 1e-7);
        }
    } else {
        p.insert("omega1", 0.1);
        p.insert("omega2", 0.001);
        p.insert("lambda1", 2e-3);
        p.insert("lambda2", 1e-3);
    }

    int sT = tBase % 4;
    if (sT != 1) {
        p.insert("cD", 0.1);
        p.insert("S", 1.0);
    }

    // 计算属性默认配置
    p.insert("t", 1e10);
    p.insert("points", 100.0);

    return p;
}

void ModelManager::setHighPrecision(bool h)
{
    for(WT_ModelWidget* w : m_modelWidgets) {
        if(w) {
            w->setHighPrecision(h);
        }
    }

    for(ModelSolver01* s : m_solversGroup1) if(s) s->setHighPrecision(h);
    for(ModelSolver02* s : m_solversGroup2) if(s) s->setHighPrecision(h);
    for(ModelSolver03* s : m_solversGroup3) if(s) s->setHighPrecision(h);
    for(ModelSolver04* s : m_solversGroup4) if(s) s->setHighPrecision(h);
    for(ModelSolver05* s : m_solversGroup5) if(s) s->setHighPrecision(h);
    for(ModelSolver06* s : m_solversGroup6) if(s) s->setHighPrecision(h);
    for(ModelSolver07* s : m_solversGroup7) if(s) s->setHighPrecision(h);
    for(ModelSolver08* s : m_solversGroup8) if(s) s->setHighPrecision(h);
    for(ModelSolver09* s : m_solversGroup9) if(s) s->setHighPrecision(h);
}

void ModelManager::updateAllModelsBasicParameters()
{
    for(WT_ModelWidget* w : m_modelWidgets) {
        if(w) {
            QMetaObject::invokeMethod(w, "onResetParameters");
        }
    }
}

void ModelManager::setObservedData(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d)
{
    m_cachedObsTime = t;
    m_cachedObsPressure = p;
    m_cachedObsDeriv = d;
}

void ModelManager::getObservedData(QVector<double>& t, QVector<double>& p, QVector<double>& d) const
{
    t = m_cachedObsTime;
    p = m_cachedObsPressure;
    d = m_cachedObsDeriv;
}

void ModelManager::clearCache()
{
    m_cachedObsTime.clear();
    m_cachedObsPressure.clear();
    m_cachedObsDeriv.clear();
}

bool ModelManager::hasObservedData() const
{
    return !m_cachedObsTime.isEmpty();
}

void ModelManager::onWidgetCalculationCompleted(const QString &t, const QMap<QString, double> &r)
{
    emit calculationCompleted(t, r);
}

QVector<double> ModelManager::generateLogTimeSteps(int c, double s, double e)
{
    return ModelSolver01::generateLogTimeSteps(c, s, e);
}
