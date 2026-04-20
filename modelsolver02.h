/*
 * 文件名: ModelSolver02.h
 * 文件作用与功能描述:
 * 1. 压裂水平井页岩型复合模型 Group 2 (Model 37-72) 核心计算类头文件。
 * 2. 彻底移除了废弃的手写高斯积分声明，全面拥抱 Boost 标准库。
 */

#ifndef MODELSOLVER02_H
#define MODELSOLVER02_H

#include <QMap>
#include <QVector>
#include <QString>
#include <tuple>
#include <functional>
#include <QtConcurrent>

using ModelCurveData = std::tuple<QVector<double>, QVector<double>, QVector<double>>;

class ModelSolver02
{
public:
    enum ModelType {
        Model_1 = 0, Model_2, Model_3, Model_4,
        Model_5, Model_6, Model_7, Model_8,
        Model_9, Model_10, Model_11, Model_12,
        Model_13, Model_14, Model_15, Model_16,
        Model_17, Model_18, Model_19, Model_20,
        Model_21, Model_22, Model_23, Model_24,
        Model_25, Model_26, Model_27, Model_28,
        Model_29, Model_30, Model_31, Model_32,
        Model_33, Model_34, Model_35, Model_36
    };

    explicit ModelSolver02(ModelType type);
    virtual ~ModelSolver02();

    void setHighPrecision(bool high);

    ModelCurveData calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    static QString getModelName(ModelType type, bool verbose = true);
    static QVector<double> generateLogTimeSteps(int count, double startExp, double endExp);

private:
    void calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                             std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                             QVector<double>& outPD, QVector<double>& outDeriv);

    double flaplace_composite(double z, const QMap<QString, double>& p);

    double PWD_composite(double z, double fs1, double fs2, double M12, double LfD, double rmD, double reD,
                         int n_fracs, ModelType type);

    double calc_fs_dual(double u, double omega, double lambda);
    double calc_fs_shale(double u, double omega, double lambda);

    static double safe_bessel_i_scaled(int v, double x);
    static double safe_bessel_k_scaled(int v, double x);
    static double safe_bessel_k(int v, double x);

    double getStehfestCoeff(int i, int N);
    void precomputeStehfestCoeffs(int N);
    double factorial(int n);

private:
    ModelType m_type;
    bool m_highPrecision;
    QVector<double> m_stehfestCoeffs;
    int m_currentN;
};

#endif // MODELSOLVER02_H
