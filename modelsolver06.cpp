/*
 * 文件名: modelsolver06.cpp
 * 作用:
 * 1. 混积型储层试井解释模型计算核心。
 * 2. 非等长裂缝 + 三重孔隙介质/混积型复合模型。
 * 3. 内区三重介质，外区夹层/页岩/均质。
 * 4. 包含页岩瞬态窜流解析函数。
 * 5. 覆盖 Model 181-216 (Group 6)。
 */

#include "modelsolver06.h"
#include "pressurederivativecalculator.h"
#include <Eigen/Dense>
#include <boost/math/special_functions/bessel.hpp>
#include <boost/math/special_functions/erf.hpp>
#include <boost/math/quadrature/gauss_kronrod.hpp>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <QtConcurrent>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 安全bessel方法
double ModelSolver06::safe_bessel_k(
    int v, double x)
{
    if (x < 1e-15) x = 1e-15;
    try {
        return boost::math::
            cyl_bessel_k(v, x);
    } catch (...) { return 0.0; }
}

// 缩放K函数
double ModelSolver06::
    safe_bessel_k_sc(int v, double x)
{
    if (x < 1e-15) x = 1e-15;
    if (x > 600.0)
        return std::sqrt(
            M_PI / (2.0 * x));
    try {
        return boost::math::
               cyl_bessel_k(v, x) * std::exp(x);
    } catch (...) { return 0.0; }
}

// 缩放I函数
double ModelSolver06::
    safe_bessel_i_sc(int v, double x)
{
    if (x < 0) x = -x;
    if (x > 600.0)
        return 1.0 / std::sqrt(
                   2.0 * M_PI * x);
    try {
        return boost::math::
               cyl_bessel_i(v, x) * std::exp(-x);
    } catch (...) { return 0.0; }
}

// 构造函数
ModelSolver06::ModelSolver06(
    ModelType type) :
    m_type(type),
    m_highPrecision(true),
    m_currentN(0)
{
    precomputeStehfestCoeffs(8);
}

ModelSolver06::~ModelSolver06() {}

// 设精度
void ModelSolver06::
    setHighPrecision(bool h) {
    m_highPrecision = h;
}

// 获取模型名
QString ModelSolver06::
    getModelName(ModelType t, bool v) {
    int id = (int)t + 1;
    QString bN = QString(
                     "混积型储层试井解释模型%1").arg(id);
    int rT = ((id - 1) / 12) + 1;
    QString sT;
    if (rT == 1)
        sT = "三重孔隙介质+夹层型";
    else if (rT == 2)
        sT = "三重孔隙介质+页岩型";
    else
        sT = "三重孔隙介质+均质";

    if (!v) return bN;
    int r4 = (id - 1) % 4;
    QString sS;
    if (r4 == 0) sS = "定井储";
    else if (r4 == 1) sS = "线源解";
    else if (r4 == 2) sS = "Fair";
    else sS = "Hegeman";

    int gI = (id - 1) % 12;
    QString sB;
    if (gI < 4) sB = "无限大边界";
    else if (gI < 8) sB = "封闭边界";
    else sB = "定压边界";

    return QString("%1\n(%2,%3,%4)")
        .arg(bN).arg(sS)
        .arg(sB).arg(sT);
}

// 生成对数时间
QVector<double> ModelSolver06::
    generateLogTimeSteps(
        int c, double s, double e)
{
    QVector<double> t;
    if (c <= 0) return t;
    t.reserve(c);
    for (int i = 0; i < c; ++i) {
        t.append(std::pow(
            10.0, s + (e - s) * i / (c - 1)));
    }
    return t;
}

// 核心总包计算
ModelCurveData ModelSolver06::
    calculateTheoreticalCurve(
        const QMap<QString, double>& p,
        const QVector<double>& pt)
{
    QVector<double> tP = pt.isEmpty() ?
                             generateLogTimeSteps(
                                 100, -3.0, 4.0) : pt;

    double phi = p.value("phi", 0.05);
    double mu = p.value("mu", 5.0);
    double B = p.value("B", 1.2);
    double Ct = p.value("Ct", 5e-3);
    double q = p.value("q", 10.0);
    double h = p.value("h", 10.0);
    double kf = p.value("kf", 10.0);
    double L = p.value(
                   "L", 1000.0) / 2.0;

    if (L < 1e-9) L = 500.0;
    if (phi < 1e-12 ||
        mu < 1e-12 ||
        Ct < 1e-12 ||
        kf < 1e-12)
    {
        return std::make_tuple(
            tP,
            QVector<double>(
                tP.size(), 0.0),
            QVector<double>(
                tP.size(), 0.0));
    }

    double td_c = 3.6 * kf /
                  (phi * mu * Ct * std::pow(L, 2.0));

    QVector<double> tD_v;
    tD_v.reserve(tP.size());
    for(double t : tP)
        tD_v.append(td_c * t);

    QMap<QString, double> cP = p;
    int N = (int)cP.value("N", 8);
    if (N<4 || N>18 || N%2!=0) N = 8;
    cP["N"] = N;
    precomputeStehfestCoeffs(N);

    if (!cP.contains("nf") ||
        cP["nf"] < 1)
        cP["nf"] = 10;
    cP["L_half"] = L;

    QVector<double> PD_v, Der_v;
    auto func = std::bind(
        &ModelSolver06::
        flaplace_composite,
        this,
        std::placeholders::_1,
        std::placeholders::_2);

    calculatePDandDeriv(
        tD_v, cP, func,
        PD_v, Der_v);

    double p_c = 1.842 * q * mu * B / (kf * h);

    QVector<double> fP(tP.size());
    QVector<double> fDP(tP.size());

    for(int i=0; i<tP.size(); ++i)
        fP[i] = p_c * PD_v[i];

    if (tP.size() > 2) {
        fDP =
            PressureDerivativeCalculator::
            calculateBourdetDerivative(
                tP, fP, 0.2);
    } else {
        fDP.fill(0.0);
    }

    return std::make_tuple(
        tP, fP, fDP);
}

// 压敏逆变换多线程
void ModelSolver06::
    calculatePDandDeriv(
        const QVector<double>& tD,
        const QMap<QString, double>& p,
        std::function<double(
            double,
            const QMap<QString,
                       double>&)> f,
        QVector<double>& oPD,
        QVector<double>& oDer)
{
    int nP = tD.size();
    oPD.resize(nP);
    oDer.resize(nP);

    int N = (int)p.value("N", 8);
    double ln2 = 0.6931471805599453;
    double gD = p.value("gamaD", 0.006);

    QVector<int> idx(nP);
    std::iota(
        idx.begin(), idx.end(), 0);

    auto calcP = [&](int k) {
        double t = tD[k];
        if (t <= 1e-10) {
            oPD[k] = 0.0; return;
        }

        double pd_val = 0.0;
        for (int m = 1; m <= N; ++m) {
            double pf = f(
                m * ln2 / t, p);
            if (std::isnan(pf) ||
                std::isinf(pf))
                pf = 0.0;
            pd_val +=
                getStehfestCoeff(m, N)
                * pf;
        }

        double pd_r = pd_val * ln2 / t;
        if (gD > 1e-9) {
            double arg = 1.0 - gD*pd_r;
            double a_min = 1e-3;
            pd_r = arg >= a_min ?
                       -1.0 / gD * std::log(arg) :
                       (-1.0 / gD * std::log(a_min)) +
                           (-1.0 / (gD * a_min)) * (arg - a_min);
        }
        if (pd_r <= 1e-15)
            pd_r = 1e-15;
        oPD[k] = pd_r;
    };
    QtConcurrent::blockingMap(
        idx, calcP);
}

// 页岩函数
double ModelSolver06::
    calc_fs_shale(
        double u, double o, double l)
{
    if (u < 1e-15) return 1.0;
    double om = std::max(
        1.0 - o, 1e-9);
    double lbd = std::max(
        l, 1e-15);
    double isq = 3.0 * om * u / lbd;
    return o + std::sqrt(
                   (lbd * om) / (3.0 * u)) * std::tanh(std::sqrt(isq));
}

// 拉氏域复配函数 (非等长裂缝版)
double ModelSolver06::
    flaplace_composite(
        double z,
        const QMap<QString, double>& p)
{
    int r_t = ((int)m_type / 12) + 1;
    double kf = p.value("kf", 10.0);
    double k2 = p.value("k2", 10.0);
    double M12 = kf / k2;
    double eta12 = M12;
    double L = p.value("L_half", 500);
    double rm = p.value("rm", 5000);
    double re = p.value("re", 5e5);

    int nF = (int)p.value("nf", 10);

    // 读取非等长裂缝半长数组 Lf_1 ~ Lf_nf
    QVector<double> LfD_arr(nF);
    for (int i = 0; i < nF; ++i) {
        QString key = QString("Lf_%1")
                          .arg(i + 1);
        double Lf_i = p.value(key, 50.0);
        LfD_arr[i] = (L > 1e-9) ?
                         Lf_i / L : 0.05;
    }

    // 读取裂缝位置 xf_1 ~ xf_nf, 或默认 linspace(-0.9, 0.9, nf)
    QVector<double> xwD(nF);
    bool hasPos = p.contains("xf_1");
    if (hasPos) {
        for (int i = 0; i < nF; ++i) {
            QString key = QString("xf_%1")
                              .arg(i + 1);
            xwD[i] = p.value(key, 0.0);
        }
    } else {
        if (nF == 1) {
            xwD[0] = 0.0;
        } else {
            for (int k = 0; k < nF; ++k)
                xwD[k] = -0.9 + 1.8 *
                          (double)k /
                          (double)(nF - 1);
        }
    }

    double rmD = (L > 1e-9) ?
                     rm / L : 16.0;
    double reD = (L > 1e-9) ?
                     re / L : 1000.0;

    double of1 = p.value("omega_f1", 0.02);
    double ov1 = p.value("omega_v1", 0.01);
    double rm1 = p.value("lambda_m1", 1e-4);
    double rv1 = p.value("lambda_v1", 1e-1);

    double om1 = 1.0 - of1 - ov1;
    double dm1 = om1 * z + rm1;
    double dv1 = ov1 * z + rv1;

    double fs1 = of1 +
                 (dm1 == 0 ? 0 :
                      (rm1 * om1) / dm1) +
                 (dv1 == 0 ? 0 :
                      (rv1 * ov1) / dv1);

    double of2 = p.value(
        "omega_f2", 0.008);
    double rm2 = p.value(
        "lambda_m2", 1e-7);
    double fs2 = eta12;

    if (r_t == 1) { // 夹层
        double om2 = 1.0 - of2;
        double dm2 = om2 * eta12 * z
                     + rm2;
        fs2 = eta12 * (of2 +
                       (dm2 == 0 ? 0 :
                            (rm2 * om2) / dm2));
    } else if (r_t == 2) { // 页岩
        fs2 = eta12 * calc_fs_shale(
                  eta12 * z, of2, rm2);
    } // 3 均质

    double pf_b = PWD_composite(
        z, fs1, fs2, M12, LfD_arr, xwD,
        rmD, reD, nF, m_type);

    int sT = (int)m_type % 4;
    if (sT == 1) return pf_b;

    double CD = p.value("cD", 0.1);

    double S = std::max(
        0.0, p.value("S", 10.0));
    double alpha = p.value(
        "alpha", 0.1);
    double C_p = p.value(
        "C_phi", 1e-4);
    double P_pD = 0.0;

    if (sT == 2 &&
        std::abs(alpha) > 1e-12) {
        P_pD = C_p / (z * (1.0 + alpha * z));
    }
    else if (sT == 3 &&
             std::abs(alpha) > 1e-12) {
        double xx = z * alpha / 2.0;
        P_pD = (C_p / z) * ((xx < 10.0) ?
                                (std::exp(xx * xx) * std::erfc(xx)) :
                                (1.0 / (std::sqrt(M_PI)
                                        * xx)));
    }

    double pu = z * pf_b + S;
    double pf = 0.0;
    if (sT == 0) {
        pf = pu / (z * (1.0 +
                        CD * z * pu));
    }
    else if (sT == 2 || sT == 3) {
        double den = z * (1.0 +
                          CD * z * pu);
        pf = std::abs(den) > 1e-100 ?
                 (pu * (1.0 + CD * z * z
                                  * P_pD)) / den : pf_b;
    }
    return pf;
}

// 矩阵计算 (非等长裂缝版)
double ModelSolver06::
    PWD_composite(
        double z, double fs1,
        double fs2, double M12,
        const QVector<double>& LfD,
        const QVector<double>& xwD,
        double rmD,
        double reD, int n_fracs,
        ModelType type)
{
    int gI = ((int)type) % 12;
    bool isI = (gI < 4);
    bool isC = (gI >= 4 && gI < 8);
    bool isP = (gI >= 8);

    double ga1 = std::sqrt(z * fs1);
    double ga2 = std::sqrt(z * fs2);
    double a_g1_r = ga1 * rmD;
    double a_g2_r = ga2 * rmD;

    double k0g1 = safe_bessel_k_sc(
        0, a_g1_r);
    double k1g1 = safe_bessel_k_sc(
        1, a_g1_r);
    double i0g1 = safe_bessel_i_sc(
        0, a_g1_r);
    double i1g1 = safe_bessel_i_sc(
        1, a_g1_r);

    double k0g2 = safe_bessel_k_sc(
        0, a_g2_r);
    double k1g2 = safe_bessel_k_sc(
        1, a_g2_r);
    double i0g2 = safe_bessel_i_sc(
        0, a_g2_r);
    double i1g2 = safe_bessel_i_sc(
        1, a_g2_r);

    double T1p = k0g2;
    double T2p = -k1g2;

    if (!isI && reD > 1e-5) {
        double a_re = ga2 * reD;
        double k0re = safe_bessel_k_sc(
            0, a_re);
        double k1re = safe_bessel_k_sc(
            1, a_re);
        double i0re = safe_bessel_i_sc(
            0, a_re);
        double i1re = safe_bessel_i_sc(
            1, a_re);
        double ef = std::exp(
            2.0 * ga2 * (rmD - reD));

        if (isC) {
            double r = k1re /
                       std::max(i1re, 1e-100);
            T1p = r * i0g2 * ef + k0g2;
            T2p = r * i1g2 * ef - k1g2;
        }
        else if (isP) {
            double r = -k0re /
                       std::max(i0re, 1e-100);
            T1p = r * i0g2 * ef + k0g2;
            T2p = r * i1g2 * ef - k1g2;
        }
    }

    double Acu = M12 * ga1 * k1g1 * T1p + ga2 * k0g1 * T2p;
    double Acd = M12 * ga1 * i1g1 * T1p - ga2 * i0g1 * T2p;

    if (std::abs(Acd) < 1e-100)
        Acd = (Acd >= 0) ?
                  1e-100 : -1e-100;

    double Ac_c = Acu / Acd;

    Eigen::MatrixXd A_m(
        n_fracs + 1, n_fracs + 1);
    Eigen::VectorXd b_v =
        Eigen::VectorXd::Zero(
            n_fracs + 1);

    for (int i = 0; i < n_fracs; ++i) {
        for (int j=0; j<n_fracs; ++j) {
            double dx = std::abs(
                xwD[i] - xwD[j]);
            if (i == j) {
                // 自干涉: 积分从 -LfD[i] 到 +LfD[i]
                double x_o = 0.732*LfD[i];
                auto fn = [&](double a)
                    -> double {
                    double dt =
                        std::abs(x_o-a);
                    if (dt < 1e-15)
                        return 0.0;
                    double a_d =
                        ga1 * dt;
                    return
                        safe_bessel_k(
                            0, a_d) +
                        Ac_c * safe_bessel_i_sc(
                            0, a_d) * std::exp(a_d -
                                       2.0*a_g1_r);
                };
                double err;
                A_m(i, j) = z * ((
                                     boost::math::
                                     quadrature::
                                     gauss_kronrod<
                                         double, 15>::
                                     integrate(
                                         fn, -LfD[i], x_o,
                                         15, 1e-6, &err)
                                     + boost::math::
                                     quadrature::
                                     gauss_kronrod<
                                         double, 15>::
                                     integrate(
                                         fn, x_o, LfD[i],
                                         15, 1e-6, &err)
                                     ) / (z*2.0*LfD[i]));
            } else {
                // 互干涉: 积分从 -LfD[j] 到 +LfD[j]
                auto fn = [&](double a)
                -> double {
                    double a_d = ga1 * std::sqrt(
                                     dx*dx + a*a);
                    return
                        safe_bessel_k(
                            0, a_d) +
                        Ac_c * safe_bessel_i_sc(
                            0, a_d) * std::exp(a_d -
                                       2.0*a_g1_r);
                };
                double err;
                A_m(i, j) = z * (
                                boost::math::
                                quadrature::
                                gauss_kronrod<
                                    double, 15>::
                                integrate(
                                    fn, -LfD[j], LfD[j],
                                    15, 1e-6, &err)
                                / (z*2.0*LfD[j]));
            }
        }
    }

    for (int i = 0; i < n_fracs; ++i) {
        A_m(i, n_fracs) = -1.0;
        A_m(n_fracs, i) = z;
    }
    A_m(n_fracs, n_fracs) = 0.0;
    b_v(n_fracs) = 1.0;

    return A_m.fullPivLu().solve(b_v)(
        n_fracs);
}

// 权重计算
void ModelSolver06::
    precomputeStehfestCoeffs(int N) {
    if (m_currentN == N &&
        !m_stehfestCoeffs.isEmpty())
        return;
    m_currentN = N;
    m_stehfestCoeffs.resize(N + 1);

    for (int i = 1; i <= N; ++i) {
        double s = 0.0;
        for (int k = (i + 1) / 2;
             k <= std::min(i, N / 2);
             ++k) {

            double den = factorial(
                             N / 2 - k) * factorial(k) * factorial(k - 1) * factorial(i - k) * factorial(2 * k - i);

            if (den != 0) {
                s += (std::pow(
                          (double)k, N/2.0)
                      * factorial(2*k)) /
                     den;
            }
        }
        m_stehfestCoeffs[i] =
            (((i + N / 2) % 2 == 0) ?
                 1.0 : -1.0) * s;
    }
}

// 获取权重
double ModelSolver06::
    getStehfestCoeff(int i, int N) {
    return (m_currentN != N ||
            i < 1 || i > N) ?
               0.0 : m_stehfestCoeffs[i];
}

// 阶乘
double ModelSolver06::
    factorial(int n) {
    double r = 1.0;
    for(int i = 2; i <= n; ++i)
        r *= i;
    return r;
}
