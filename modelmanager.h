/*
 * 文件名: modelmanager.h
 * 作用与功能描述:
 * 1. 声明模型生命周期管理器类 (ModelManager)。
 * 2. 作为全局中枢神经，维护 324 个试井模型界面的懒加载与内存释放。
 * 3. 声明针对 9 大类基础求解器组 (modelsolver01 ~ 09) 的工厂方法。
 * 4. 提供统一的基础物理字典下发以及理论曲线结果回传路由。
 * 5. 【核心修复】为 calculateTheoreticalCurve 提供缺省时间参数 t = QVector<double>()，彻底解决底层拟合引擎编译时的 no matching function for call 报错。
 */

#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>
#include <QStackedWidget>
#include <tuple>

// 前置声明 UI 界面与各底层引擎
class WT_ModelWidget;
class ModelSolver01;
class ModelSolver02;
class ModelSolver03;
class ModelSolver04;
class ModelSolver05;
class ModelSolver06;
class ModelSolver07;
class ModelSolver08;
class ModelSolver09;

// 理论曲线数据类型: 时间, 压差, 压力导数
typedef std::tuple<QVector<double>, QVector<double>, QVector<double>> ModelCurveData;

class ModelManager : public QObject
{
    Q_OBJECT
public:
    // 采用宽泛枚举绑定内部数组偏移，范围延展至 0 ~ 323 (代表 324 个模型)
    enum ModelType {
        Model_1 = 0
    };

    explicit ModelManager(QWidget* parent = nullptr);
    ~ModelManager();

    // 核心初始化：为父级面板创建容纳 324 个界面卡片的堆栈
    void initializeModels(QWidget* parentWidget);

    // 切换界面路由
    void switchToModel(ModelType modelType);

    // 理论曲线集中路由调度
    // 【核心修复】此处为第三个参数 t 增加了默认值 = QVector<double>()，这样既能接受模型界面的 3 参数调用，也能无缝兼容拟合内核 (fittingcore.cpp) 的 2 参数调用
    ModelCurveData calculateTheoreticalCurve(ModelType type, const QMap<QString, double>& p, const QVector<double>& t = QVector<double>());

    // 静态工具：获取模型名
    static QString getModelTypeName(ModelType type);

    // 静态工具：下发包含了 k2、Lf_i、xf_i 等特化物理量的一级/二级合并默认字典
    static QMap<QString, double> getDefaultParameters(ModelType type);

    // 设置高精度模式并透传给所有已实例化的求解器
    void setHighPrecision(bool h);

    // 通知所有界面更新物理字典
    void updateAllModelsBasicParameters();

    // 观测数据的全局缓存管理
    void setObservedData(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d);
    void getObservedData(QVector<double>& t, QVector<double>& p, QVector<double>& d) const;
    void clearCache();
    bool hasObservedData() const;

    // 工具函数：生成对数分布的时间步长
    static QVector<double> generateLogTimeSteps(int c, double s, double e);

signals:
    void modelSwitched(ModelType newType, ModelType oldType);
    void calculationCompleted(const QString &title, const QMap<QString, double> &results);

private slots:
    void onSelectModelClicked();
    void onWidgetCalculationCompleted(const QString &t, const QMap<QString, double> &r);

private:
    void createMainWidget();
    WT_ModelWidget* ensureWidget(ModelType type);

    // 懒加载实例化工厂，分别为 1~9 号基础求解器分配内存
    ModelSolver01* ensureSolverGroup1(int idx);
    ModelSolver02* ensureSolverGroup2(int idx);
    ModelSolver03* ensureSolverGroup3(int idx);
    ModelSolver04* ensureSolverGroup4(int idx);
    ModelSolver05* ensureSolverGroup5(int idx);
    ModelSolver06* ensureSolverGroup6(int idx);
    ModelSolver07* ensureSolverGroup7(int idx);
    ModelSolver08* ensureSolverGroup8(int idx);
    ModelSolver09* ensureSolverGroup9(int idx);

private:
    QWidget* m_mainWidget;
    QStackedWidget* m_modelStack;
    ModelType m_currentModelType;

    // 界面缓存组 (324 容量)
    QVector<WT_ModelWidget*> m_modelWidgets;

    // 求解器缓存组 (各 36 容量)
    QVector<ModelSolver01*> m_solversGroup1;
    QVector<ModelSolver02*> m_solversGroup2;
    QVector<ModelSolver03*> m_solversGroup3;
    QVector<ModelSolver04*> m_solversGroup4;
    QVector<ModelSolver05*> m_solversGroup5;
    QVector<ModelSolver06*> m_solversGroup6;
    QVector<ModelSolver07*> m_solversGroup7;
    QVector<ModelSolver08*> m_solversGroup8;
    QVector<ModelSolver09*> m_solversGroup9;

    QVector<double> m_cachedObsTime;
    QVector<double> m_cachedObsPressure;
    QVector<double> m_cachedObsDeriv;
};

#endif // MODELMANAGER_H
