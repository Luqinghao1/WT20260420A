/*
 * 文件名: modelsolver09.cpp
 * 作用:
 * 1. 混积型储层试井解释模型计算核心 (非均布位置 + 非等长)。
 * 2. 【核心更新】采用 xf 为距离水平井起点的绝对坐标 [0, L]，并映射为无因次位置 xwD。
 * 3. 严格规范代码布局，全面屏蔽了 -Wmisleading-indentation 等误导性缩进警告。
 */

#include "modelsolver09.h"
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

double ModelSolver09::safe_bessel_k(int v, double x) {
    if (x < 1e-15) x = 1e-15;
    try {
        return boost::math::cyl_bessel_k(v, x);
    } catch (...) {
        return 0.0;
    }
}

double ModelSolver09::safe_bessel_k_scaled(int v, double x) {
    if (x < 1e-15) x = 1e-15;
    if (x > 600.0) return std::sqrt(M_PI / (2.0 * x));
    try {
        return boost::math::cyl_bessel_k(v, x) * std::exp(x);
    } catch (...) {
        return 0.0;
    }
}

double ModelSolver09::safe_bessel_i_scaled(int v, double x) {
    if (x < 0) x = -x;
    if (x > 600.0) return 1.0 / std::sqrt(2.0 * M_PI * x);
    try {
        return boost::math::cyl_bessel_i(v, x) * std::exp(-x);
    } catch (...) {
        return 0.0;
    }
}

ModelSolver09::ModelSolver09(ModelType type)
    : m_type(type),
    m_highPrecision(true),
    m_currentN(0)
{
    // N=8 阶运算配置
    precomputeStehfestCoeffs(8);
}

ModelSolver09::~ModelSolver09() {}

void ModelSolver09::setHighPrecision(bool h) {
    m_highPrecision = h;
}

QString ModelSolver09::getModelName(ModelType t, bool v) {
    int id = (int)t + 1;
    QString bN = QString("混积型储层试井解释模型%1").arg(id);

    int rT = ((id - 1) / 12) + 1;
    QString sT;
    if (rT == 1) sT = "三重孔隙介质+夹层型";
    else if (rT == 2) sT = "三重孔隙介质+页岩型";
    else sT = "三重孔隙介质+均质";

    if (!v) return bN;

    int r4 = (id - 1) % 4;
    QString sS;
    if (r4 == 0) sS = "定井储";
    else if (r4 == 1) sS = "线源解";
    else if (r4 == 2) sS = "Fair模型";
    else sS = "Hegeman模型";

    int gI = (id - 1) % 12;
    QString sB;
    if (gI < 4) sB = "无限大外边界";
    else if (gI < 8) sB = "封闭边界";
    else sB = "定压边界";

    return QString("%1\n(%2、%3、%4)").arg(bN).arg(sS).arg(sB).arg(sT);
}

QVector<double> ModelSolver09::generateLogTimeSteps(int c, double s, double e) {
    QVector<double> t;
    if (c <= 0) return t;
    for (int i = 0; i < c; ++i) {
        t.append(std::pow(10.0, s + (e - s) * i / (c - 1)));
    }
    return t;
}

ModelCurveData ModelSolver09::calculateTheoreticalCurve(const QMap<QString, double>& p, const QVector<double>& pt) {
    QVector<double> tP = pt.isEmpty() ? generateLogTimeSteps(100, -3.0, 4.0) : pt;

    double phi = p.value("phi", 0.05);
    double mu = p.value("mu", 5.0);
    double B = p.value("B", 1.2);
    double Ct = p.value("Ct", 5e-3);
    double q = p.value("q", 10.0);
    double h = p.value("h", 10.0);
    double kf = p.value("kf", 10.0);
    double L_total = p.value("L", 1000.0);
    double L_half = L_total / 2.0;

    if (L_half < 1e-9) L_half = 500.0;

    if (phi < 1e-12 || mu < 1e-12 || Ct < 1e-12 || kf < 1e-12) {
        return std::make_tuple(tP, QVector<double>(tP.size(), 0.0), QVector<double>(tP.size(), 0.0));
    }

    double td_c = 3.6 * kf / (phi * mu * Ct * std::pow(L_half, 2.0));
    QVector<double> tD_v;
    for(double t : tP) {
        tD_v.append(td_c * t);
    }

    QMap<QString, double> cP = p;
    int N = (int)cP.value("N", 8);
    if (N < 4 || N > 18 || N % 2 != 0) {
        N = 8;
    }
    cP["N"] = N;
    precomputeStehfestCoeffs(N);

    if (!cP.contains("nf") || cP["nf"] < 1) {
        cP["nf"] = 10;
    }
    cP["L_total"] = L_total;
    cP["L_half"] = L_half;

    QVector<double> PD_v, Der_v;
    auto func = std::bind(&ModelSolver09::flaplace_composite, this, std::placeholders::_1, std::placeholders::_2);
    calculatePDandDeriv(tD_v, cP, func, PD_v, Der_v);

    double p_c = 1.842 * q * mu * B / (kf * h);
    QVector<double> fP(tP.size()), fDP(tP.size());

    for(int i = 0; i < tP.size(); ++i) {
        fP[i] = p_c * PD_v[i];
    }

    if (tP.size() > 2) {
        fDP = PressureDerivativeCalculator::calculateBourdetDerivative(tP, fP, 0.2);
    } else {
        fDP.fill(0.0);
    }

    return std::make_tuple(tP, fP, fDP);
}

void ModelSolver09::calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& p,
                                        std::function<double(double, const QMap<QString, double>&)> f, QVector<double>& oPD, QVector<double>& oDer)
{
    int nP = tD.size();
    oPD.resize(nP);
    oDer.resize(nP);

    int N = (int)p.value("N", 8);
    double ln2 = 0.6931471805599453;
    double gamaD = p.value("gamaD", 0.02);

    QVector<int> idxs(nP);
    std::iota(idxs.begin(), idxs.end(), 0);

    auto calcSingle = [&](int k) {
        double t = tD[k];
        if (t <= 1e-10) {
            oPD[k] = 0.0;
            return;
        }

        double pd_v = 0.0;
        for (int m = 1; m <= N; ++m) {
            double z = m * ln2 / t;
            double pf = f(z, p);
            if (std::isnan(pf) || std::isinf(pf)) {
                pf = 0.0;
            }
            pd_v += getStehfestCoeff(m, N) * pf;
        }

        double pd_r = pd_v * ln2 / t;

        if (gamaD > 1e-9) {
            double arg = 1.0 - gamaD * pd_r;
            double a_m = 1e-3;
            if (arg >= a_m) {
                pd_r = -1.0 / gamaD * std::log(arg);
            } else {
                pd_r = -1.0 / gamaD * std::log(a_m) - 1.0 / (gamaD * a_m) * (arg - a_m);
            }
        }

        if (pd_r <= 1e-15) {
            pd_r = 1e-15;
        }
        oPD[k] = pd_r;
    };

    QtConcurrent::blockingMap(idxs, calcSingle);
}

double ModelSolver09::calc_fs_dual(double u, double omega, double lambda) {
    double one_m = 1.0 - omega;
    double den = one_m * u + lambda;
    if (std::abs(den) < 1e-20) return 0.0;
    return (omega * one_m * u + lambda) / den;
}

double ModelSolver09::calc_fs_shale(double u, double omega, double lambda) {
    if (u < 1e-15) return 1.0;
    double one_m = 1.0 - omega;
    if (one_m < 1e-9) one_m = 1e-9;
    if (lambda < 1e-15) lambda = 1e-15;

    double isqrt = 3.0 * one_m * u / lambda;
    double arg_t = std::sqrt(isqrt);
    double fsqrt = std::sqrt( (lambda * one_m) / (3.0 * u) );
    return omega + fsqrt * std::tanh(arg_t);
}

double ModelSolver09::flaplace_composite(double z, const QMap<QString, double>& p) {
    double kf = p.value("kf", 10.0);
    double k2 = p.value("k2", 10.0);
    if (k2 < 1e-12) k2 = 1e-12;

    double M12 = kf / k2;
    double eta12 = M12;

    double L_total = p.value("L_total", 1000.0);
    double L_half = p.value("L_half", 500.0);
    double rm = p.value("rm", 1500.0);
    double re = p.value("re", 20000.0);
    int nf = (int)p.value("nf", 10);

    QVector<double> LfD_arr(nf);
    QVector<double> xwD(nf);

    for (int i = 0; i < nf; ++i) {
        QString key_lf_0 = QString("Lf_%1").arg(i);
        QString key_lf_1 = QString("Lf_%1").arg(i + 1);
        if (p.contains(key_lf_0)) {
            LfD_arr[i] = (L_half > 1e-9) ? p.value(key_lf_0) / L_half : 0.05;
        } else {
            LfD_arr[i] = (L_half > 1e-9) ? p.value(key_lf_1, 50.0) / L_half : 0.05;
        }

        // 【核心映射】：使用绝对距离 xf 计算无因次位置
        QString key_xf_0 = QString("xf_%1").arg(i);
        QString key_xf_1 = QString("xf_%1").arg(i + 1);
        double xf_val = 0.0;

        if (p.contains(key_xf_0)) {
            xf_val = p.value(key_xf_0);
        } else if (p.contains(key_xf_1)) {
            xf_val = p.value(key_xf_1);
        } else {
            xf_val = 0.05 * L_total + 0.9 * L_total * (double)i / std::max(1, nf - 1);
        }

        xwD[i] = (L_total > 1e-9) ? ((2.0 * xf_val / L_total) - 1.0) : 0.0;
    }

    double rmD = (L_half > 1e-9) ? rm / L_half : 1.25;
    double reD = (L_half > 1e-9) ? re / L_half : 25.0;

    double omega_f1 = p.value("omega_f1", 0.02);
    double omega_v1 = p.value("omega_v1", 0.01);
    double lambda_m1 = p.value("lambda_m1", 4e-4);
    double lambda_v1 = p.value("lambda_v1", 1e-4);

    double omega_m1 = 1.0 - omega_f1 - omega_v1;
    if (omega_m1 < 1e-9) omega_m1 = 1e-9;

    double dm1 = omega_m1 * z + lambda_m1;
    double dv1 = omega_v1 * z + lambda_v1;
    double fs1 = omega_f1 + (lambda_m1 * omega_m1) / dm1 + (lambda_v1 * omega_v1) / dv1;

    double z_outer = eta12 * z;
    double fs2 = 1.0;
    int rT = (((int)m_type) / 12) + 1;

    if (rT == 1) {
        double omga2 = p.value("omega_f2", 0.008);
        double remda2 = p.value("lambda_m2", 1e-7);
        fs2 = eta12 * calc_fs_dual(z_outer, omga2, remda2);
    } else if (rT == 2) {
        double omga2 = p.value("omega_f2", 0.008);
        double remda2 = p.value("lambda_m2", 1e-7);
        fs2 = eta12 * calc_fs_shale(z_outer, omga2, remda2);
    } else {
        fs2 = eta12;
    }

    double pf_base = PWD_composite(z, fs1, fs2, M12, LfD_arr, rmD, reD, nf, xwD, m_type);

    int sT = (int)m_type % 4;
    if (sT == 1) return pf_base;

    double CD = p.value("cD", 0.1);
    double S = p.value("S", 1.0);
    if (S < 0.0) S = 0.0;

    double alpha = p.value("alpha", 1e-1);
    double C_phi = p.value("C_phi", 1e-4);
    double P_phiD = 0.0;

    if (sT == 2) {
        if (std::abs(alpha) > 1e-12) {
            P_phiD = C_phi / (z * (1.0 + alpha * z));
        }
    } else if (sT == 3) {
        if (std::abs(alpha) > 1e-12) {
            double xx = z * alpha / 2.0;
            double exp_erfc = (xx < 10.0) ? (std::exp(xx * xx) * std::erfc(xx)) : (1.0 / (std::sqrt(M_PI) * xx));
            P_phiD = (C_phi / z) * exp_erfc;
        }
    }

    double pu = z * pf_base + S;
    double pf = 0.0;

    if (sT == 0) {
        pf = pu / (z * (1.0 + CD * z * pu));
    } else if (sT == 2 || sT == 3) {
        double num = pu * (1.0 + CD * z * z * P_phiD);
        double den = z * (1.0 + CD * z * pu);
        if (std::abs(den) > 1e-100) {
            pf = num / den;
        } else {
            pf = pf_base;
        }
    }
    return pf;
}

double ModelSolver09::PWD_composite(double z, double fs1, double fs2, double M12, const QVector<double>& LfD, double rmD, double reD, int nf, const QVector<double>& xwD, ModelType t) {
    int gI = ((int)t) % 12;
    bool isInf = (gI < 4);
    bool isCls = (gI >= 4 && gI < 8);
    bool isCst = (gI >= 8);

    double gama1 = std::sqrt(z * fs1);
    double gama2 = std::sqrt(z * fs2);
    double a1 = gama1 * rmD;
    double a2 = gama2 * rmD;

    double k0_1 = safe_bessel_k_scaled(0, a1);
    double k1_1 = safe_bessel_k_scaled(1, a1);
    double i0_1 = safe_bessel_i_scaled(0, a1);
    double i1_1 = safe_bessel_i_scaled(1, a1);

    double k0_2 = safe_bessel_k_scaled(0, a2);
    double k1_2 = safe_bessel_k_scaled(1, a2);
    double i0_2 = safe_bessel_i_scaled(0, a2);
    double i1_2 = safe_bessel_i_scaled(1, a2);

    double T1p = k0_2;
    double T2p = -k1_2;

    if (!isInf && reD > 1e-5) {
        double a_re = gama2 * reD;
        double k0_re = safe_bessel_k_scaled(0, a_re);
        double k1_re = safe_bessel_k_scaled(1, a_re);
        double i0_re = safe_bessel_i_scaled(0, a_re);
        double i1_re = safe_bessel_i_scaled(1, a_re);
        double ef = std::exp(2.0 * gama2 * (rmD - reD));

        if (isCls) {
            double ratio = k1_re / std::max(i1_re, 1e-100);
            T1p = ratio * i0_2 * ef + k0_2;
            T2p = ratio * i1_2 * ef - k1_2;
        } else if (isCst) {
            double ratio = -k0_re / std::max(i0_re, 1e-100);
            T1p = ratio * i0_2 * ef + k0_2;
            T2p = ratio * i1_2 * ef - k1_2;
        }
    }

    double Acu = M12 * gama1 * k1_1 * T1p + gama2 * k0_1 * T2p;
    double Acd = M12 * gama1 * i1_1 * T1p - gama2 * i0_1 * T2p;

    if (std::abs(Acd) < 1e-100) {
        Acd = (Acd >= 0) ? 1e-100 : -1e-100;
    }
    double Ac = Acu / Acd;

    int sz = nf + 1;
    Eigen::MatrixXd A(sz, sz);
    Eigen::VectorXd b(sz);
    b.setZero();

    for (int i = 0; i < nf; ++i) {
        for (int j = 0; j < nf; ++j) {
            double dx = std::abs(xwD[i] - xwD[j]);
            double val = 0.0;

            if (i == j) {
                double xo = 0.732 * LfD[i];
                auto ig_self = [&](double a) -> double {
                    double dst = std::abs(xo - a);
                    if (dst < 1e-15) return 0.0;
                    double ad = gama1 * dst;
                    return safe_bessel_k(0, ad) + Ac * safe_bessel_i_scaled(0, ad) * std::exp(ad - 2.0 * a1);
                };

                double err;
                double Il = boost::math::quadrature::gauss_kronrod<double, 15>::integrate(ig_self, -LfD[i], xo, 15, 1e-6, &err);
                double Ir = boost::math::quadrature::gauss_kronrod<double, 15>::integrate(ig_self, xo, LfD[i], 15, 1e-6, &err);
                val = (Il + Ir) / (z * 2.0 * LfD[i]);
            } else {
                auto ig = [&](double a) -> double {
                    double dst = std::sqrt(dx * dx + a * a);
                    double ad = gama1 * dst;
                    return safe_bessel_k(0, ad) + Ac * safe_bessel_i_scaled(0, ad) * std::exp(ad - 2.0 * a1);
                };

                double err;
                double I = boost::math::quadrature::gauss_kronrod<double, 15>::integrate(ig, -LfD[j], LfD[j], 15, 1e-6, &err);
                val = I / (z * 2.0 * LfD[j]);
            }
            A(i, j) = z * val;
        }
    }

    for (int i = 0; i < nf; ++i) {
        A(i, nf) = -1.0;
        A(nf, i) = z;
    }

    A(nf, nf) = 0.0;
    b(nf) = 1.0;

    Eigen::VectorXd x = A.fullPivLu().solve(b);
    return x(nf);
}

void ModelSolver09::precomputeStehfestCoeffs(int N) {
    if (m_currentN == N && !m_stehfestCoeffs.isEmpty()) return;
    m_currentN = N;
    m_stehfestCoeffs.resize(N + 1);

    for (int i = 1; i <= N; ++i) {
        double s = 0.0;
        int k1 = (i + 1) / 2;
        int k2 = std::min(i, N / 2);

        for (int k = k1; k <= k2; ++k) {
            double num = std::pow((double)k, N / 2.0) * factorial(2 * k);
            double den = factorial(N / 2 - k) * factorial(k) * factorial(k - 1) * factorial(i - k) * factorial(2 * k - i);
            if (den != 0) {
                s += num / den;
            }
        }
        double sign = ((i + N / 2) % 2 == 0) ? 1.0 : -1.0;
        m_stehfestCoeffs[i] = sign * s;
    }
}

double ModelSolver09::getStehfestCoeff(int i, int N) {
    if (m_currentN != N || i < 1 || i > N) return 0.0;
    return m_stehfestCoeffs[i];
}

double ModelSolver09::factorial(int n) {
    if(n <= 1) {
        return 1.0;
    }

    double r = 1.0;
    for(int i = 2; i <= n; ++i) {
        r *= i;
    }

    return r;
}
