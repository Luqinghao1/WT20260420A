/*
 * 文件名: fittingparameterchart.h
 * 文件作用:
 * 1. 声明拟合参数表格的管理器类，负责 UI 表格与底层参数数据(FitParameter)的绑定。
 * 2. 【核心修复】明确定义了 FitParameter 结构体，并移除了 fittingcore.h 的引用，
 * 彻底打破头文件循环依赖，解决 'FitParameter was not declared in this scope' 等数百条连环报错。
 * 3. 声明了动态重构参数阵列的方法 syncFractureParams。
 */

#ifndef FITTINGPARAMETERCHART_H
#define FITTINGPARAMETERCHART_H

#include <QObject>
#include <QTableWidget>
#include <QList>
#include <QMap>
#include <QString>
#include <QTimer>
#include "modelmanager.h" // 仅包含中枢管理器以使用 ModelType 枚举

// =========================================================================
// 【核心修复】全局物理参数结构体定义
// 在此处明确定义，提供给上层 UI 和下层 FittingCore 使用，杜绝未声明报错
// =========================================================================
struct FitParameter {
    QString name;           // 参数内部键名 (如 "kf", "Lf_1", "xf_2")
    QString displayName;    // 参数中文显示名称
    double value;           // 当前设定数值
    double min;             // LM非线性拟合约束下限
    double max;             // LM非线性拟合约束上限
    double step;            // 界面滚轮调节步长
    bool isFit;             // 是否参与拟合运算
    bool isVisible;         // 是否在界面上显示
};

class FittingParameterChart : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数，接管 UI 上的 TableWidget
     */
    explicit FittingParameterChart(QTableWidget *parentTable, QObject *parent = nullptr);

    /**
     * @brief 设置模型管理器中枢，用于获取物理量默认值
     */
    void setModelManager(ModelManager *m);

    /**
     * @brief 切换当前试井模型，保留同名参数的值，并调整新特有参数
     */
    void switchModel(ModelManager::ModelType newType);

    /**
     * @brief 获取当前表格配置的参数列表 (平铺的标量列表，适配 LM 引擎)
     */
    QList<FitParameter> getParameters() const;

    /**
     * @brief 从外部注入并覆盖当前的参数列表，刷新表格
     */
    void setParameters(const QList<FitParameter> &p);

    /**
     * @brief 将当前模型的参数重置为默认值
     * @param preserveStates 是否保留当前的“参与拟合”、“可见性”勾选状态
     */
    void resetParams(ModelManager::ModelType type, bool preserveStates = true);

    /**
     * @brief 提取表格中的原始文本(字符串字典)，用于外部映射校验
     */
    QMap<QString, QString> getRawParamTexts() const;

    /**
     * @brief 将 UI 表格用户的修改，同步提取并保存至内部的 m_params 列表
     */
    void updateParamsFromTable();

    /**
     * @brief 静态工具：基于物理意义自动评估并收紧各参数的拟合上下限及滚轮步长
     */
    static void adjustLimits(QList<FitParameter>& params);

    /**
     * @brief 触发自动限幅
     */
    void autoAdjustLimits();

    /**
     * @brief 静态工具：生成指定模型的默认参数模板
     * @param type 目标模型类型
     * @param overrideNf (可选) 强制指定的裂缝条数，用于动态展开多缝长/多位置
     */
    static QList<FitParameter> generateDefaultParams(ModelManager::ModelType type, int overrideNf = -1);

    /**
     * @brief 静态工具：获取参数的中文名称与单位展示信息
     */
    static void getParamDisplayInfo(const QString &n, QString &cN, QString &s, QString &uS, QString &u);

signals:
    /**
     * @brief 鼠标滚轮修改数值时触发的防抖信号，通知界面刷新图版
     */
    void parameterChangedByWheel();

protected:
    /**
     * @brief 事件过滤器，用于拦截和处理鼠标滚轮修改数值
     */
    bool eventFilter(QObject *w, QEvent *e) override;

private slots:
    void onWheelDebounceTimeout();

    /**
     * @brief 表格项被用户编辑后触发的槽函数
     */
    void onTableItemChanged(QTableWidgetItem *item);

private:
    void refreshParamTable();
    void addRowToTable(const FitParameter& p, int& serialNo, bool highlight);

    /**
     * @brief 【核心新增机制】当裂缝条数 nf 发生改变时，动态伸缩 Lf_i 和 xf_i 的行数
     * @param newNf 新的裂缝条数
     */
    void syncFractureParams(int newNf);

private:
    QTableWidget *m_table;               // 关联的 UI 表格组件
    ModelManager *m_modelManager;        // 中枢管理器指针
    QList<FitParameter> m_params;        // 核心参数列表缓存
    QTimer *m_wheelTimer;                // 滚轮防抖定时器
    ModelManager::ModelType m_modelType; // 当前绑定的模型类型
};

#endif // FITTINGPARAMETERCHART_H
