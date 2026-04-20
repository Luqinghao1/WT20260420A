/*
 * 文件名: modelsolver08.h
 * 作用:
 * 1. 定义非均布位置、非等长长度的页岩型储层试井解释模型 (Model 253-288)。
 */

#ifndef MODELSOLVER08_H
#define MODELSOLVER08_H

#include <QVector>
#include <QMap>
#include <QString>
#include <tuple>
#include <functional>

typedef std::tuple<QVector<double>, QVector<double>, QVector<double>> ModelCurveData;

class ModelSolver08
{
public:
    enum ModelType {
        Model_1 = 0, Model_2, Model_3, Model_4, Model_5, Model_6,
        Model_7, Model_8, Model_9, Model_10, Model_11, Model_12,
        Model_13, Model_14, Model_15, Model_16, Model_17, Model_18,
        Model_19, Model_20, Model_21, Model_22, Model_23, Model_24,
        Model_25, Model_26, Model_27, Model_28, Model_29, Model_30,
        Model_31, Model_32, Model_33, Model_34, Model_35, Model_36
    };

    explicit ModelSolver08(ModelType type);
    ~ModelSolver08();

    ModelCurveData calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());
    void setHighPrecision(bool high);
    static QString getModelName(ModelType type, bool verbose = true);
    static QVector<double> generateLogTimeSteps(int count, double startExp, double endExp);

private:
    void calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                             std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                             QVector<double>& outPD, QVector<double>& outDeriv);

    double flaplace_composite(double z, const QMap<QString, double>& p);

    double PWD_composite(double z, double fs1, double fs2, double M12,
                         const QVector<double>& LfD, double rmD, double reD,
                         int n_fracs, const QVector<double>& xwD, ModelType type);

    double calc_fs_dual(double u, double omega, double lambda);
    double calc_fs_shale(double u, double omega, double lambda);

    void precomputeStehfestCoeffs(int N);
    double getStehfestCoeff(int i, int N);
    double factorial(int n);

    double safe_bessel_k(int v, double x);
    double safe_bessel_k_scaled(int v, double x);
    double safe_bessel_i_scaled(int v, double x);

private:
    ModelType m_type;
    bool m_highPrecision;
    int m_currentN;
    QVector<double> m_stehfestCoeffs;
};

#endif // MODELSOLVER08_H
