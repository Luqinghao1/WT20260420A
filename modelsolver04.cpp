/*
 * 文件名: modelsolver04.cpp
 * 文件作用与功能描述:
 * 1. 压裂水平井复合模型 Group 4 (Model 109-144) 的核心计算算法实现。
 * 2. 与 ModelSolver01 (等长裂缝) 对应，本求解器支持非等长裂缝 (每条裂缝可独立设定半长)。
 * 3. 实现了压降有因次化系数(1.842)、时间无因次化系数(3.6)的修正转换。
 * 4. 修复了导压系数比与流度比物理逻辑(eta12 = M12)。
 * 5. 【奇点分离优化】采用无限导流裂缝等效点 (x_obs = 0.732*LfD[i]) 计算裂缝自影响项，
 *    通过切割积分区间 [-LfD[i], x_obs] 和 [x_obs, LfD[i]]，利用自适应高斯积分完美越过对数奇点。
 * 6. 包含了泰勒级数平滑外推以镇压压敏系数过大导致的奇异振荡现象。
 * 7. 【降噪优化】接入 Boost Gauss-Kronrod 工业级积分器彻底抹平积分噪声，并将 Stehfest 阶数降为 8 阶。
 * 8. 涵盖三种储层组合:
 *    - 夹层型 + 夹层型 (Model 109-120)
 *    - 夹层型 + 均质   (Model 121-132)
 *    - 径向复合 (均质+均质) (Model 133-144)
 */

#include "modelsolver04.h"
#include "pressurederivativecalculator.h"

#include <Eigen/Dense>
#include <boost/math/special_functions/bessel.hpp>
#include <boost/math/special_functions/erf.hpp>
#include <boost/math/quadrature/gauss_kronrod.hpp>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <QDebug>
#include <QtConcurrent>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 功能：安全调用第二类修正贝塞尔函数 K_v，防止自变量为0引发崩溃
double ModelSolver04::safe_bessel_k(int v, double x) {
    if (x < 1e-15) x = 1e-15;
    try { return boost::math::cyl_bessel_k(v, x); } catch (...) { return 0.0; }
}

// 功能：计算带有指数缩放的 K_v 函数 (K_v(x) * e^x)，避免大自变量时的数值下溢
double ModelSolver04::safe_bessel_k_scaled(int v, double x) {
    if (x < 1e-15) x = 1e-15;
    if (x > 600.0) return std::sqrt(M_PI / (2.0 * x)); // 渐近展开，防止溢出
    try { return boost::math::cyl_bessel_k(v, x) * std::exp(x); } catch (...) { return 0.0; }
}

// 功能：计算带有指数缩放的 I_v 函数 (I_v(x) * e^-x)，避免大自变量时的数值上溢
double ModelSolver04::safe_bessel_i_scaled(int v, double x) {
    if (x < 0) x = -x;
    if (x > 600.0) return 1.0 / std::sqrt(2.0 * M_PI * x); // 渐近展开，防止溢出
    try { return boost::math::cyl_bessel_i(v, x) * std::exp(-x); } catch (...) { return 0.0; }
}

// 构造函数：初始化当前模型类型及默认精度配置
ModelSolver04::ModelSolver04(ModelType type)
    : m_type(type), m_highPrecision(true), m_currentN(0) {
    precomputeStehfestCoeffs(8); // 预计算默认 8 阶的 Stehfest 反演系数，压制高阶震荡
}

// 析构函数
ModelSolver04::~ModelSolver04() {}

// 功能：设置求解器是否开启高精度模式
void ModelSolver04::setHighPrecision(bool high) { m_highPrecision = high; }

// 功能：获取当前求解器对应的模型中文描述名称
QString ModelSolver04::getModelName(ModelType type, bool verbose)
{
    int id = (int)type + 1;
    QString baseName;
    QString subType;

    if (id <= 12) {
        baseName = QString("夹层型储层试井解释模型%1").arg(id);
        subType = "夹层型+夹层型";
    } else if (id <= 24) {
        baseName = QString("夹层型储层试井解释模型%1").arg(id);
        subType = "夹层型+均质";
    } else {
        baseName = QString("径向复合模型%1").arg(id - 24);
        subType = "均质+均质";
    }

    if (!verbose) return baseName;

    int rem4 = (id - 1) % 4;
    QString strStorage;
    if (rem4 == 0) strStorage = "定井储";
    else if (rem4 == 1) strStorage = "线源解";
    else if (rem4 == 2) strStorage = "Fair模型";
    else strStorage = "Hegeman模型";

    int groupIdx = (id - 1) % 12;
    QString strBoundary;
    if (groupIdx < 4) strBoundary = "无限大外边界";
    else if (groupIdx < 8) strBoundary = "封闭边界";
    else strBoundary = "定压边界";

    return QString("%1\n(%2、%3、%4)").arg(baseName).arg(strStorage).arg(strBoundary).arg(subType);
}

// 功能：按对数间距生成模拟运算所需的时间离散向量
QVector<double> ModelSolver04::generateLogTimeSteps(int count, double startExp, double endExp)
{
    QVector<double> t;
    if (count <= 0) return t;
    t.reserve(count);
    for (int i = 0; i < count; ++i) {
        double exponent = startExp + (endExp - startExp) * i / (count - 1);
        t.append(std::pow(10.0, exponent));
    }
    return t;
}

// 功能：核心计算总入口，根据输入参数和时间序列计算理论压力与压力导数曲线
ModelCurveData ModelSolver04::calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime)
{
    QVector<double> tPoints = providedTime;
    if (tPoints.isEmpty()) tPoints = generateLogTimeSteps(100, -3.0, 4.0);

    double phi = params.value("phi", 0.05);
    double mu = params.value("mu", 0.5);
    double B = params.value("B", 1.2);
    double Ct = params.value("Ct", 5e-4);
    double q = params.value("q", 50.0);
    double h = params.value("h", 20.0);
    double kf = params.value("kf", 50.0);

    double L_total = params.value("L", 1000.0);
    double L = L_total / 2.0; // 提取半长，用于内部统一体系计算

    if (L < 1e-9) L = 500.0;
    if (phi < 1e-12 || mu < 1e-12 || Ct < 1e-12 || kf < 1e-12) {
        return std::make_tuple(tPoints, QVector<double>(tPoints.size(), 0.0), QVector<double>(tPoints.size(), 0.0));
    }

    // 无因次时间转换系数，基于半长 L_ref，数值修正为 3.6
    double td_coeff = 3.6 * kf / (phi * mu * Ct * std::pow(L, 2.0));
    QVector<double> tD_vec;
    tD_vec.reserve(tPoints.size());
    for(double t : tPoints) tD_vec.append(td_coeff * t);

    QMap<QString, double> calcParams = params;
    int N = (int)calcParams.value("N", 8); // 默认 Stehfest 阶数降为 8
    if (N < 4 || N > 18 || N % 2 != 0) N = 8;
    calcParams["N"] = N;
    precomputeStehfestCoeffs(N);

    if (!calcParams.contains("nf") || calcParams["nf"] < 1) calcParams["nf"] = 9;
    calcParams["L_half"] = L;

    QVector<double> PD_vec, Deriv_vec;
    auto func = std::bind(&ModelSolver04::flaplace_composite, this, std::placeholders::_1, std::placeholders::_2);

    // 多线程并发进行 Stehfest 数值反演
    calculatePDandDeriv(tD_vec, calcParams, func, PD_vec, Deriv_vec);

    // 公制单位压降还原系数修正为 1.842
    double p_coeff = 1.842 * q * mu * B / (kf * h);
    QVector<double> finalP(tPoints.size()), finalDP(tPoints.size());

    for(int i = 0; i < tPoints.size(); ++i) {
        finalP[i] = p_coeff * PD_vec[i];
    }

    // 生成 Bourdet 压力导数
    if (tPoints.size() > 2) {
        finalDP = PressureDerivativeCalculator::calculateBourdetDerivative(tPoints, finalP, 0.2);
    } else {
        finalDP.fill(0.0);
    }

    return std::make_tuple(tPoints, finalP, finalDP);
}

// 功能：通过多线程映射加速执行 Stehfest 拉普拉斯反演计算与压敏外推
void ModelSolver04::calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                                           std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                                           QVector<double>& outPD, QVector<double>& outDeriv)
{
    int numPoints = tD.size();
    outPD.resize(numPoints);
    outDeriv.resize(numPoints);

    int N = (int)params.value("N", 8);
    double ln2 = 0.6931471805599453;
    double gamaD = params.value("gamaD", 0.02);

    QVector<int> indexes(numPoints);
    std::iota(indexes.begin(), indexes.end(), 0);

    // 定义单点的计算流程
    auto calculateSinglePoint = [&](int k) {
        double t = tD[k];
        if (t <= 1e-10) { outPD[k] = 0.0; return; }

        double pd_val = 0.0;
        for (int m = 1; m <= N; ++m) {
            double z = m * ln2 / t;
            double pf = laplaceFunc(z, params);
            if (std::isnan(pf) || std::isinf(pf)) pf = 0.0;
            pd_val += getStehfestCoeff(m, N) * pf;
        }

        double pd_real = pd_val * ln2 / t;

        // 压敏修正算法 (摄动反推)
        if (gamaD > 1e-9) {
            double arg = 1.0 - gamaD * pd_real;
            double arg_min = 1e-3; // 防止极度压敏导致对数域崩盘的截断点

            if (arg >= arg_min) {
                // 正常的指数摄动转换
                pd_real = -1.0 / gamaD * std::log(arg);
            } else {
                // 超越截断点后，使用泰勒一阶展开相切外推，确保导数平滑不断崖
                double val_at_min = -1.0 / gamaD * std::log(arg_min);
                double slope_at_min = -1.0 / (gamaD * arg_min);
                pd_real = val_at_min + slope_at_min * (arg - arg_min);
            }
        }

        if (pd_real <= 1e-15) pd_real = 1e-15;
        outPD[k] = pd_real;
    };

    QtConcurrent::blockingMap(indexes, calculateSinglePoint);
}

// 功能：Laplace拉氏域综合耦合中心，将不同流动机制的特征函数耦合在一起 (非等长裂缝版)
double ModelSolver04::flaplace_composite(double z, const QMap<QString, double>& p) {
    double kf = p.value("kf", 50.0);
    double k2 = p.value("k2", 10.0);
    double M12 = kf / k2;
    // 导压系数比等于流度比
    double eta12 = M12;

    double L = p.value("L_half", 500.0);
    double rm = p.value("rm", 1500.0);
    double re = p.value("re", 20000.0);

    int n_fracs = (int)p.value("nf", 9);

    // 读取非等长裂缝半长数组 Lf_1 ~ Lf_nf
    QVector<double> LfD_arr(n_fracs);
    for (int i = 0; i < n_fracs; ++i) {
        QString key = QString("Lf_%1").arg(i + 1);
        double Lf_i = p.value(key, 50.0);
        LfD_arr[i] = (L > 1e-9) ? Lf_i / L : 0.05;
    }

    // 读取裂缝位置 xf_1 ~ xf_nf，或默认 linspace(-0.9, 0.9, nf)
    QVector<double> xwD(n_fracs);
    bool hasPos = p.contains("xf_1");
    if (hasPos) {
        for (int i = 0; i < n_fracs; ++i) {
            QString key = QString("xf_%1").arg(i + 1);
            xwD[i] = p.value(key, 0.0);
        }
    } else {
        if (n_fracs == 1) {
            xwD[0] = 0.0;
        } else {
            for (int k = 0; k < n_fracs; ++k)
                xwD[k] = -0.9 + 1.8 * (double)k / (double)(n_fracs - 1);
        }
    }

    double rmD = (L > 1e-9) ? rm / L : 1.25;
    double reD = (L > 1e-9) ? re / L : 25.0;

    double fs1 = 1.0, fs2 = 1.0;
    int id = (int)m_type + 1;

    // 内区介质函数
    if (id <= 24) {
        double omga1 = p.value("omega1", 0.4);
        double remda1 = p.value("lambda1", 1e-3);
        double one_minus = 1.0 - omga1;
        fs1 = (omga1 * one_minus * z + remda1) / (one_minus * z + remda1);
    }
    // 外区介质函数
    if (id <= 12) {
        double omga2 = p.value("omega2", 0.08);
        double remda2 = p.value("lambda2", 1e-4);
        double one_minus = 1.0 - omga2;
        fs2 = eta12 * (omga2 * one_minus * eta12 * z + remda2) / (one_minus * eta12 * z + remda2);
    } else {
        fs2 = eta12;
    }

    // 呼叫底层复合响应 (非等长裂缝版本，传入 LfD_arr 和 xwD)
    double pf_base = PWD_composite(z, fs1, fs2, M12, LfD_arr, rmD, reD, n_fracs, xwD, m_type);

    int storageType = (int)m_type % 4;
    if (storageType == 1) return pf_base; // 纯线源解，直接返回

    double CD = p.value("cD", 0.1);

    double S = p.value("S", 1.0);
    if (S < 0.0) S = 0.0;

    double alpha = p.value("alpha", 1e-1);
    double C_phi = p.value("C_phi", 1e-4);

    double P_phiD = 0.0;
    // 井筒相重分布附加响应拉氏方程
    if (storageType == 2) {
        if (std::abs(alpha) > 1e-12) {
            P_phiD = C_phi / (z * (1.0 + alpha * z));
        }
    } else if (storageType == 3) {
        if (std::abs(alpha) > 1e-12) {
            double xx = z * alpha / 2.0;
            double exp_erfc = (xx < 10.0) ? (std::exp(xx * xx) * std::erfc(xx)) : (1.0 / (std::sqrt(M_PI) * xx));
            P_phiD = (C_phi / z) * exp_erfc;
        }
    }

    double pu = z * pf_base + S;
    double pf = 0.0;

    // Duhamel 叠加法则
    if (storageType == 0) {
        pf = pu / (z * (1.0 + CD * z * pu));
    } else if (storageType == 2 || storageType == 3) {
        double num = pu * (1.0 + CD * z * z * P_phiD);
        double den = z * (1.0 + CD * z * pu);
        if (std::abs(den) > 1e-100) pf = num / den;
        else pf = pf_base;
    }

    return pf;
}

// 功能：通过半解析边界元计算 DFN (离散裂缝网络) 的基础无因次压力 (非等长裂缝版本)
double ModelSolver04::PWD_composite(double z, double fs1, double fs2, double M12,
                                       const QVector<double>& LfD, double rmD, double reD,
                                       int n_fracs, const QVector<double>& xwD, ModelType type) {
    int id = (int)type + 1;
    int groupIdx = (id - 1) % 12;
    bool isInfinite = (groupIdx < 4);
    bool isClosed = (groupIdx >= 4 && groupIdx < 8);
    bool isConstP = (groupIdx >= 8);

    double gama1 = std::sqrt(z * fs1);
    double gama2 = std::sqrt(z * fs2);
    double arg_g1_rm = gama1 * rmD;
    double arg_g2_rm = gama2 * rmD;

    // 预计算边界条件所需的贝塞尔族群
    double k0_g1 = safe_bessel_k_scaled(0, arg_g1_rm);
    double k1_g1 = safe_bessel_k_scaled(1, arg_g1_rm);
    double i0_g1 = safe_bessel_i_scaled(0, arg_g1_rm);
    double i1_g1 = safe_bessel_i_scaled(1, arg_g1_rm);

    double k0_g2 = safe_bessel_k_scaled(0, arg_g2_rm);
    double k1_g2 = safe_bessel_k_scaled(1, arg_g2_rm);
    double i0_g2 = safe_bessel_i_scaled(0, arg_g2_rm);
    double i1_g2 = safe_bessel_i_scaled(1, arg_g2_rm);

    double T1_prime = k0_g2;
    double T2_prime = -k1_g2;

    // 融入物理边界镜像系数
    if (!isInfinite && reD > 1e-5) {
        double arg_re = gama2 * reD;
        double k0_re = safe_bessel_k_scaled(0, arg_re);
        double k1_re = safe_bessel_k_scaled(1, arg_re);
        double i0_re = safe_bessel_i_scaled(0, arg_re);
        double i1_re = safe_bessel_i_scaled(1, arg_re);

        double exp_factor = std::exp(2.0 * gama2 * (rmD - reD));

        if (isClosed) {
            double ratio = k1_re / std::max(i1_re, 1e-100);
            T1_prime = ratio * i0_g2 * exp_factor + k0_g2;
            T2_prime = ratio * i1_g2 * exp_factor - k1_g2;
        } else if (isConstP) {
            double ratio = -k0_re / std::max(i0_re, 1e-100);
            T1_prime = ratio * i0_g2 * exp_factor + k0_g2;
            T2_prime = ratio * i1_g2 * exp_factor - k1_g2;
        }
    }

    // 求出复合内区控制传导特征系数 Ac
    double Acup_prime   = M12 * gama1 * k1_g1 * T1_prime + gama2 * k0_g1 * T2_prime;
    double Acdown_prime = M12 * gama1 * i1_g1 * T1_prime - gama2 * i0_g1 * T2_prime;
    if (std::abs(Acdown_prime) < 1e-100) Acdown_prime = (Acdown_prime >= 0) ? 1e-100 : -1e-100;

    double Ac_core = Acup_prime / Acdown_prime;

    int size = n_fracs + 1;
    Eigen::MatrixXd A_mat(size, size);
    Eigen::VectorXd b_vec(size);
    b_vec.setZero();

    // 构建半解析干扰系数矩阵 (非等长裂缝: 每条裂缝使用独立的 LfD[i]/LfD[j])
    for (int i = 0; i < n_fracs; ++i) {
        for (int j = 0; j < n_fracs; ++j) {
            double dx = std::abs(xwD[i] - xwD[j]);
            double val = 0.0;

            if (i == j) {
                // 引入无限导流裂缝等效观测点 (0.732 * LfD[i])
                double x_obs = 0.732 * LfD[i];

                auto integrand_self = [&](double a) -> double {
                    double dist = std::abs(x_obs - a);
                    if (dist < 1e-15) return 0.0; // 越过绝对零点防护
                    double arg_dist = gama1 * dist;
                    return safe_bessel_k(0, arg_dist) + Ac_core * safe_bessel_i_scaled(0, arg_dist) * std::exp(arg_dist - 2.0 * arg_g1_rm);
                };

                // 使用 Boost.Math Gauss-Kronrod 积分器
                unsigned max_depth = 15;
                double tol = 1e-6;
                double error_est;

                double I_left = boost::math::quadrature::gauss_kronrod<double, 15>::integrate(integrand_self, -LfD[i], x_obs, max_depth, tol, &error_est);
                double I_right = boost::math::quadrature::gauss_kronrod<double, 15>::integrate(integrand_self, x_obs, LfD[i], max_depth, tol, &error_est);
                val = (I_left + I_right) / (z * 2.0 * LfD[i]);

            } else {
                // 异缝平滑影响，积分从 -LfD[j] 到 +LfD[j]
                auto integrand = [&](double a) -> double {
                    double dist_val = std::sqrt(dx * dx + a * a);
                    double arg_dist = gama1 * dist_val;
                    return safe_bessel_k(0, arg_dist) + Ac_core * safe_bessel_i_scaled(0, arg_dist) * std::exp(arg_dist - 2.0 * arg_g1_rm);
                };

                // 使用 Boost.Math Gauss-Kronrod 积分器
                unsigned max_depth = 15;
                double tol = 1e-6;
                double error_est;
                double I_full = boost::math::quadrature::gauss_kronrod<double, 15>::integrate(integrand, -LfD[j], LfD[j], max_depth, tol, &error_est);
                val = I_full / (z * 2.0 * LfD[j]);
            }
            A_mat(i, j) = z * val;
        }
    }

    // 约束方程：并联总产量等于1，端部压力全等
    for (int i = 0; i < n_fracs; ++i) {
        A_mat(i, n_fracs) = -1.0;
        A_mat(n_fracs, i) = z;
    }
    A_mat(n_fracs, n_fracs) = 0.0;
    b_vec(n_fracs) = 1.0;

    // 稠密矩阵全主元LU分解求逆提取总系统响应
    Eigen::VectorXd x_sol = A_mat.fullPivLu().solve(b_vec);
    return x_sol(n_fracs);
}

// =========================================================================
// 以下为原自定义高斯积分器，因已替换为 Boost Gauss-Kronrod，故予以保留，
// 避免外部其他文件(.h)中存在声明而导致的链接器 (Linker) 报错。
// =========================================================================

// 功能：15点高斯-勒让德数值求积基础引擎 (已弃用，保留作后备)
double ModelSolver04::gauss15(std::function<double(double)> f, double a, double b) {
    static const double X[] = { 0.0, 0.20119409, 0.39415135, 0.57097217, 0.72441773, 0.84820658, 0.93729853, 0.98799252 };
    static const double W[] = { 0.20257824, 0.19843149, 0.18616100, 0.16626921, 0.13957068, 0.10715922, 0.07036605, 0.03075324 };
    double h = 0.5 * (b - a);
    double c = 0.5 * (a + b);
    double s = W[0] * f(c);
    for (int i = 1; i < 8; ++i) {
        double dx = h * X[i];
        s += W[i] * (f(c - dx) + f(c + dx));
    }
    return s * h;
}

// 功能：自适应多级高斯数值求积框架 (已弃用，替换为 Boost GK)
double ModelSolver04::adaptiveGauss(std::function<double(double)> f, double a, double b, double eps, int depth, int maxDepth) {
    double c = (a + b) / 2.0;
    double v1 = gauss15(f, a, b);
    double v2 = gauss15(f, a, c) + gauss15(f, c, b);
    if (depth >= maxDepth || std::abs(v1 - v2) < eps * (std::abs(v2) + 1.0)) return v2;
    return adaptiveGauss(f, a, c, eps/2, depth+1, maxDepth) + adaptiveGauss(f, c, b, eps/2, depth+1, maxDepth);
}

// 功能：预先建立并存储指定阶数的 Stehfest 矩阵权重系数字典，消除重复算力消耗
void ModelSolver04::precomputeStehfestCoeffs(int N) {
    if (m_currentN == N && !m_stehfestCoeffs.isEmpty()) return;
    m_currentN = N; m_stehfestCoeffs.resize(N + 1);
    for (int i = 1; i <= N; ++i) {
        double s = 0.0;
        int k1 = (i + 1) / 2;
        int k2 = std::min(i, N / 2);
        for (int k = k1; k <= k2; ++k) {
            double num = std::pow((double)k, N / 2.0) * factorial(2 * k);
            double den = factorial(N / 2 - k) * factorial(k) * factorial(k - 1) * factorial(i - k) * factorial(2 * k - i);
            if (den != 0) s += num / den;
        }
        double sign = ((i + N / 2) % 2 == 0) ? 1.0 : -1.0;
        m_stehfestCoeffs[i] = sign * s;
    }
}

// 功能：拉取特定阶数对应的预存反演权重
double ModelSolver04::getStehfestCoeff(int i, int N) {
    if (m_currentN != N) return 0.0;
    if (i < 1 || i > N) return 0.0;
    return m_stehfestCoeffs[i];
}

// 功能：阶乘数学原语
double ModelSolver04::factorial(int n) {
    if(n <= 1) return 1.0;
    double r = 1.0;
    for(int i = 2; i <= n; ++i) r *= i;
    return r;
}
