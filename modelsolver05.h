/*
 * 文件名: ModelSolver05.h
 * 文件作用与功能描述:
 * 1. 本代码文件为压裂水平井页岩型复合模型 Group 5 (Model 145-180) 的核心计算类头文件。
 * 2. 负责计算软件定义的 36 个以页岩型介质为主、非等长裂缝的试井模型理论图版数据。
 * 3. 涵盖三种储层组合的数学求解:
 *    - 页岩型 + 页岩型   (Model 145-156 / 内部编号 1-12)
 *    - 页岩型 + 均质     (Model 157-168 / 内部编号 13-24)
 *    - 页岩型 + 双重孔隙 (Model 169-180 / 内部编号 25-36)
 * 4. 实现了用于描述页岩型介质的瞬态平板模型 (Transient Slab Matrix) 函数的 Laplace 形式。
 * 5. 基于与 MATLAB (Number5_fitfun_V2) 一致的 nf 节点积分方程组算法，
 *    支持非等长裂缝 (各缝独立半长 LfD 向量) 与非均匀裂缝位置的严格解析求解。
 * 6. 与 ModelSolver02 (等长裂缝) 的关键差异:
 *    - PWD_composite 接收 QVector<double> LfD 向量而非标量
 *    - 各裂缝积分区间独立: [-LfD[j], +LfD[j]]
 *    - 自影响等效观测点: 0.732 * LfD[i] (逐缝独立)
 *    - 归一化因子: 2*LfD[j] (逐列独立)
 */

#ifndef ModelSolver05_H
#define ModelSolver05_H

#include <QMap>
#include <QVector>
#include <QString>
#include <tuple>
#include <functional>
#include <QtConcurrent>

using ModelCurveData = std::tuple<QVector<double>, QVector<double>, QVector<double>>;

class ModelSolver05
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

    /**
     * @brief 构造函数，初始化特定类型的求解器
     */
    explicit ModelSolver05(ModelType type);

    virtual ~ModelSolver05();

    /**
     * @brief 设置高精度计算模式
     */
    void setHighPrecision(bool high);

    /**
     * @brief 计算理论曲线的核心接口
     * @param params 模型参数集合 (物理约束同MATLAB要求)
     */
    ModelCurveData calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    static QString getModelName(ModelType type, bool verbose = true);
    static QVector<double> generateLogTimeSteps(int count, double startExp, double endExp);

private:
    void calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                             std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                             QVector<double>& outPD, QVector<double>& outDeriv);

    double flaplace_composite(double z, const QMap<QString, double>& p);

    /**
     * @brief 核心边界元方法求解函数 (非等长裂缝版本)
     *        每条裂缝拥有独立的半长 LfD[j]，积分区间和归一化因子逐缝独立
     */
    double PWD_composite(double z, double fs1, double fs2, double M12, const QVector<double>& LfD, double rmD, double reD,
                         int n_fracs, const QVector<double>& xwD, ModelType type);

    // --- 介质函数 ---
    double calc_fs_dual(double u, double omega, double lambda);
    double calc_fs_shale(double u, double omega, double lambda);

    // --- 数学辅助 ---
    static double safe_bessel_i_scaled(int v, double x);
    static double safe_bessel_k_scaled(int v, double x);
    static double safe_bessel_k(int v, double x);

    double gauss15(std::function<double(double)> f, double a, double b);
    double adaptiveGauss(std::function<double(double)> f, double a, double b, double eps, int depth, int maxDepth);

    double getStehfestCoeff(int i, int N);
    void precomputeStehfestCoeffs(int N);
    double factorial(int n);

private:
    ModelType m_type;
    bool m_highPrecision;
    QVector<double> m_stehfestCoeffs;
    int m_currentN;
};

#endif // ModelSolver05_H
