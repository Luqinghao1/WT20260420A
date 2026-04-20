/*
 * 文件名: fittingcore.h
 * 文件作用: 试井拟合核心算法类头文件
 * 功能描述:
 * 1. 封装 Levenberg-Marquardt 优化算法。
 * 2. 处理数据的抽样逻辑 (getLogSampledData)。
 * 3. 管理拟合过程中的数学计算（残差、雅可比矩阵、线性方程组求解）。
 * 4. 提供异步拟合控制接口。
 * 5. [新增] 算法入口增加 useLimits 参数，决定是否在拟合时强加限幅。
 */

#ifndef FITTINGCORE_H
#define FITTINGCORE_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QFutureWatcher>
#include "modelmanager.h"
#include "fittingsamplingdialog.h"
#include "fittingparameterchart.h"

class FittingCore : public QObject
{
    Q_OBJECT
public:
    explicit FittingCore(QObject *parent = nullptr);

    // 设置模型管理器
    void setModelManager(ModelManager* m);

    // 设置观测数据
    void setObservedData(const QVector<double>& t, const QVector<double>& p, const QVector<double>& d);

    // 设置抽样策略
    void setSamplingSettings(const QList<SamplingInterval>& intervals, bool enabled);

    // 开始拟合 (useLimits: 是否限幅, useMultiStart: 是否多起点)
    void startFit(ModelManager::ModelType modelType, const QList<FitParameter>& params, double weight, bool useLimits = false, bool useMultiStart = false);

    // 停止拟合
    void stopFit();

    // 辅助函数：根据当前策略获取抽样数据（可供界面绘图使用）
    void getLogSampledData(const QVector<double>& srcT, const QVector<double>& srcP, const QVector<double>& srcD,
                           QVector<double>& outT, QVector<double>& outP, QVector<double>& outD);

    // 计算残差 (公开以便计算最终误差)
    QVector<double> calculateResiduals(const QMap<QString, double>& params, ModelManager::ModelType modelType, double weight,
                                       const QVector<double>& t, const QVector<double>& obsP, const QVector<double>& obsD);

    // 计算误差平方和
    double calculateSumSquaredError(const QVector<double>& residuals);

    // 静态辅助函数：参数预处理
    static QMap<QString, double> preprocessParams(const QMap<QString, double>& rawParams, ModelManager::ModelType type);

    // [P1新增] 基于观测数据自动估算关键参数初值
    // 返回字典包含可估算的参数键值对 (kf, C, S, omega, lambda, Lf 等)
    static QMap<QString, double> estimateInitialParams(
        const QVector<double>& obsTime,
        const QVector<double>& obsDeltaP,
        const QVector<double>& obsDerivative,
        ModelManager::ModelType modelType);

signals:
    // 迭代更新信号
    void sigIterationUpdated(double error, QMap<QString,double> params, QVector<double> t, QVector<double> p, QVector<double> d);
    // 进度信号
    void sigProgress(int percent);
    // 拟合结束信号
    void sigFitFinished();

private:
    ModelManager* m_modelManager;
    QVector<double> m_obsTime;
    QVector<double> m_obsDeltaP;
    QVector<double> m_obsDerivative;

    bool m_isCustomSamplingEnabled;
    QList<SamplingInterval> m_customIntervals;

    bool m_stopRequested;
    QFutureWatcher<void> m_watcher;

    // 内部运行的优化任务
    void runOptimizationTask(ModelManager::ModelType modelType, QList<FitParameter> fitParams, double weight, bool useLimits, bool useMultiStart);

    // LM算法实现
    void runLevenbergMarquardtOptimization(ModelManager::ModelType modelType, QList<FitParameter> params, double weight, bool useLimits, bool useMultiStart);

    // 计算雅可比矩阵
    QVector<QVector<double>> computeJacobian(const QMap<QString, double>& params, const QVector<double>& baseResiduals,
                                             const QVector<int>& fitIndices, ModelManager::ModelType modelType,
                                             const QList<FitParameter>& currentFitParams, double weight,
                                             const QVector<double>& t, const QVector<double>& obsP, const QVector<double>& obsD);

    // 求解线性方程组
    QVector<double> solveLinearSystem(const QVector<QVector<double>>& A, const QVector<double>& b);
};

#endif // FITTINGCORE_H
