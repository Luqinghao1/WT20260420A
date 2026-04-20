/*
 * 文件名: wt_modelwidget.h
 * 作用与功能:
 * 1. 试井理论图版分析界面的核心 UI 控制类。
 * 2. 负责管理用户界面的参数输入，支持逗号分隔多维敏感性参数。
 * 3. 动态加载九组数学求解引擎 (ModelSolver01~09)，
 * 全面支持 324 种物理模型的理论曲线计算。
 * 4. 提供界面状态记忆机制 (getUiTexts/setUiTexts)。
 */

#ifndef WT_MODELWIDGET_H
#define WT_MODELWIDGET_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QColor>
#include <tuple>
#include "chartwidget.h"

// 引入全部九大组求解引擎
#include "modelsolver01.h"
#include "modelsolver02.h"
#include "modelsolver03.h"
#include "modelsolver04.h"
#include "modelsolver05.h"
#include "modelsolver06.h"
#include "modelsolver07.h"
#include "modelsolver08.h"
#include "modelsolver09.h"

namespace Ui {
class WT_ModelWidget;
}

class WT_ModelWidget : public QWidget
{
    Q_OBJECT

public:
    // 类型别名定义
    using ModelType = int;
    using ModelCurveData = ::ModelCurveData;

    /**
     * @brief 构造函数，初始化模型界面并分配数学求解器
     * @param type 初始化的模型枚举 ID (范围 0-323)
     * @param parent 父窗口指针
     */
    explicit WT_ModelWidget(
        ModelType type,
        QWidget *parent = nullptr);

    /**
     * @brief 析构函数，负责安全释放界面与求解器内存
     */
    ~WT_ModelWidget();

    /**
     * @brief 统一设置是否采用高精度数值反演模式
     * @param high true 为高精度，false 为普通精度
     */
    void setHighPrecision(bool high);

    /**
     * @brief 绕过界面直接调用求解器计算理论图版
     * @param params 模型的输入参数字典
     * @param providedTime 预先指定的时间序列
     * @return <时间, 压力降, 压力导数> 元组数据
     */
    ModelCurveData calculateTheoreticalCurve(
        const QMap<QString, double>& params,
        const QVector<double>& providedTime = QVector<double>());

    /**
     * @brief 获取当前挂载模型的完整中文名称描述
     */
    QString getModelName() const;

    /**
     * @brief 提取界面中所有 QLineEdit 的当前文本
     */
    QMap<QString, QString> getUiTexts() const;

    /**
     * @brief 将外部文本状态批量安全注入回界面的输入框
     */
    void setUiTexts(const QMap<QString, QString>& texts);

signals:
    /**
     * @brief 当计算且图版绘制完成时发射
     */
    void calculationCompleted(
        const QString& modelType,
        const QMap<QString, double>& params);

    /**
     * @brief 请求弹窗更换模型
     */
    void requestModelSelection();

public slots:
    /**
     * @brief 响应“开始计算”按钮
     */
    void onCalculateClicked();

    /**
     * @brief 响应“重置参数”按钮，请求默认参数覆写
     */
    void onResetParameters();

    /**
     * @brief 关联参数联动变化预留
     */
    void onDependentParamsChanged();

    /**
     * @brief 响应“显示数据点”复选框
     */
    void onShowPointsToggled(bool checked);

    /**
     * @brief 导出 CSV 数据文件
     */
    void onExportData();

private:
    /**
     * @brief 依据 ModelType 动态隐蔽无关物理参数输入框
     */
    void initUi();

    /**
     * @brief 初始化 QCustomPlot 图表框架
     */
    void initChart();

    /**
     * @brief 建立按钮与槽函数的连接
     */
    void setupConnections();

    /**
     * @brief 执行计算的核心控制流程
     */
    void runCalculation();

    /**
     * @brief 解析输入框文本，支持逗号分隔与安全去除中文括号
     */
    QVector<double> parseInput(const QString& text);

    /**
     * @brief 安全向 QLineEdit 设置浮点数
     */
    void setInputText(QLineEdit* edit, double value);

    /**
     * @brief 将底层数据绘制到图表控件
     */
    void plotCurve(
        const ModelCurveData& data,
        const QString& name,
        QColor color,
        bool isSensitivity);

private:
    Ui::WT_ModelWidget *ui;        // 关联的 UI 界面
    ModelType m_type;              // 当前绑定的模型 ID

    ModelSolver01* m_solver1;      // 处理模型 1-36
    ModelSolver02* m_solver2;      // 处理模型 37-72
    ModelSolver03* m_solver3;      // 处理模型 73-108
    ModelSolver04* m_solver4;      // 处理模型 109-144
    ModelSolver05* m_solver5;      // 处理模型 145-180
    ModelSolver06* m_solver6;      // 处理模型 181-216
    ModelSolver07* m_solver7;      // 处理模型 217-252 (新增非均布位置)
    ModelSolver08* m_solver8;      // 处理模型 253-288 (新增非均布位置)
    ModelSolver09* m_solver9;      // 处理模型 289-324 (新增非均布位置)

    bool m_highPrecision;          // 高精度计算标志
    QList<QColor> m_colorList;     // 敏感性绘图颜色库

    QVector<double> res_tD;        // 缓存的时间序列
    QVector<double> res_pD;        // 缓存的压力降
    QVector<double> res_dpD;       // 缓存的压力导数
};

#endif // WT_MODELWIDGET_H
