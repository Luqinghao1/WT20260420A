/*
 * 文件名: modelsolver03.h
 * 作用:
 * 1. 混积型复合模型核心头文件。
 * 2. 覆盖 Model 73-108 的运算。
 * 3. 彻底移除了废弃的手写高斯积分声明，全面拥抱 Boost 标准库。
 */

#ifndef MODELSOLVER03_H
#define MODELSOLVER03_H

#include <QMap>
#include <QVector>
#include <QString>
#include <tuple>
#include <functional>
#include <QtConcurrent>

using ModelCurveData = std::tuple<QVector<double>, QVector<double>, QVector<double>>;

class ModelSolver03
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

    explicit ModelSolver03(ModelType type);
    virtual ~ModelSolver03();

    void setHighPrecision(bool h);

    ModelCurveData calculateTheoreticalCurve(const QMap<QString, double>& p, const QVector<double>& t = QVector<double>());

    static QString getModelName(ModelType type, bool v = true);
    static QVector<double> generateLogTimeSteps(int c, double s, double e);

private:
    void calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& p,
                             std::function<double(double, const QMap<QString, double>&)> f,
                             QVector<double>& oPD, QVector<double>& oDeriv);

    double flaplace_composite(double z, const QMap<QString, double>& p);

    double PWD_composite(double z, double fs1, double fs2, double M12,
                         double LfD, double rmD, double reD, int nF, ModelType type);

    static double safe_bessel_i_sc(int v, double x);
    static double safe_bessel_k_sc(int v, double x);
    static double safe_bessel_k(int v, double x);

    double calc_fs_shale(double u, double o, double l);

    double getStehfestCoeff(int i, int N);
    void precomputeStehfestCoeffs(int N);
    double factorial(int n);

private:
    ModelType m_type;
    bool m_highPrecision;
    QVector<double> m_stehfestCoeffs;
    int m_currentN;
};

#endif // MODELSOLVER03_H
