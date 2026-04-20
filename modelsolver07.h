/*
 * 文件名: modelsolver07.h
 * 作用:
 * 1. 定义非均布位置、非等长长度的夹层型/均质复合模型 (Model 217-252)。
 * 2. 声明拉普拉斯域核心解析函数与矩阵求逆器。
 * 3. 使用标准 C++ 面向对象封装，彻底规避全局变量污染。
 */

#ifndef MODELSOLVER07_H
#define MODELSOLVER07_H

#include <QVector>
#include <QMap>
#include <QString>
#include <tuple>
#include <functional>

// 理论曲线数据格式: 时间, 压差, 压力导数
typedef std::tuple<QVector<double>, QVector<double>, QVector<double>> ModelCurveData;

class ModelSolver07
{
public:
    // 模型内部路由枚举 (36种组合)
    enum ModelType {
        Model_1 = 0, Model_2, Model_3, Model_4, Model_5, Model_6,
        Model_7, Model_8, Model_9, Model_10, Model_11, Model_12,
        Model_13, Model_14, Model_15, Model_16, Model_17, Model_18,
        Model_19, Model_20, Model_21, Model_22, Model_23, Model_24,
        Model_25, Model_26, Model_27, Model_28, Model_29, Model_30,
        Model_31, Model_32, Model_33, Model_34, Model_35, Model_36
    };

    explicit ModelSolver07(ModelType type);
    ~ModelSolver07();

    // 核心正演接口
    ModelCurveData calculateTheoreticalCurve(const QMap<QString, double>& params, const QVector<double>& providedTime = QVector<double>());

    // 精度控制开关
    void setHighPrecision(bool high);

    // 获取模型的中文描述名称
    static QString getModelName(ModelType type, bool verbose = true);

    // 生成对数分布的时间步
    static QVector<double> generateLogTimeSteps(int count, double startExp, double endExp);

private:
    // 并行计算时间向量上所有点的无因次压力和导数
    void calculatePDandDeriv(const QVector<double>& tD, const QMap<QString, double>& params,
                             std::function<double(double, const QMap<QString, double>&)> laplaceFunc,
                             QVector<double>& outPD, QVector<double>& outDeriv);

    // 拉普拉斯空间的压力解装配
    double flaplace_composite(double z, const QMap<QString, double>& p);

    // 无因次点源积分与半解析矩阵求解
    double PWD_composite(double z, double fs1, double fs2, double M12,
                         const QVector<double>& LfD, double rmD, double reD,
                         int n_fracs, const QVector<double>& xwD, ModelType type);

    // Stehfest 算法辅助函数
    void precomputeStehfestCoeffs(int N);
    double getStehfestCoeff(int i, int N);
    double factorial(int n);

    // 贝塞尔函数的防溢出安全封装
    double safe_bessel_k(int v, double x);
    double safe_bessel_k_scaled(int v, double x);
    double safe_bessel_i_scaled(int v, double x);

private:
    ModelType m_type;
    bool m_highPrecision;
    int m_currentN;
    QVector<double> m_stehfestCoeffs;
};

#endif // MODELSOLVER07_H
