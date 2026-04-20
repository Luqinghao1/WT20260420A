/*
 * 文件名: fittingcore.cpp
 * 文件作用: 试井拟合核心算法实现
 * 修复记录:
 * 1. [修复致命死锁] 将 computeJacobian 中的 QtConcurrent::blockingMapped 改为串行 for 循环。
 * 2. [算法优化] 将 LM 算法内循环寻找下降方向的 tryIter 从 5 次提升至 20 次，避免过早陷入局部极小值宣告失败。
 * 3. [功能扩展] 集成了 useLimits 参数，根据用户勾选情况自动拦截越界变动。
 * 4. [算法优化] 针对裂缝条数(nf)实现了伪连续-整数混合差分优化，完美支持将其作为正整数纳入非线性连续下降寻优，不会导致梯度丢失。
 */

#include "fittingcore.h"
#include "modelparameter.h" // 引入模型参数单例
#include <QtConcurrent>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <Eigen/Dense>

FittingCore::FittingCore(QObject *parent)
    : QObject(parent), m_modelManager(nullptr), m_isCustomSamplingEnabled(false), m_stopRequested(false)
{
    // 监听异步任务完成
    connect(&m_watcher, &QFutureWatcher<void>::finished, this, &FittingCore::sigFitFinished);
}

void FittingCore::setModelManager(ModelManager *m) {
    m_modelManager = m;
}

void FittingCore::setObservedData(const QVector<double> &t, const QVector<double> &p, const QVector<double> &d) {
    m_obsTime = t;
    m_obsDeltaP = p;
    m_obsDerivative = d;
}

void FittingCore::setSamplingSettings(const QList<SamplingInterval> &intervals, bool enabled) {
    m_customIntervals = intervals;
    m_isCustomSamplingEnabled = enabled;
}

void FittingCore::startFit(ModelManager::ModelType modelType, const QList<FitParameter> &params, double weight, bool useLimits, bool useMultiStart) {
    if (m_watcher.isRunning()) return;

    m_stopRequested = false;
    // 启动异步线程执行拟合
    m_watcher.setFuture(QtConcurrent::run([this, modelType, params, weight, useLimits, useMultiStart](){
        runOptimizationTask(modelType, params, weight, useLimits, useMultiStart);
    }));
}

void FittingCore::stopFit() {
    m_stopRequested = true;
}

void FittingCore::getLogSampledData(const QVector<double>& srcT, const QVector<double>& srcP, const QVector<double>& srcD,
                                    QVector<double>& outT, QVector<double>& outP, QVector<double>& outD)
{
    outT.clear(); outP.clear(); outD.clear();
    if (srcT.isEmpty()) return;

    // 辅助结构体用于排序去重
    struct DataPoint {
        double t, p, d;
        bool operator<(const DataPoint& other) const { return t < other.t; }
        bool operator==(const DataPoint& other) const { return std::abs(t - other.t) < 1e-9; }
    };
    QVector<DataPoint> points;

    // 模式1：默认策略
    if (!m_isCustomSamplingEnabled) {
        int targetCount = 80; // [性能优化] 200 → 80，大幅减少每次正演的计算量
        if (srcT.size() <= targetCount) {
            outT = srcT; outP = srcP; outD = srcD;
            return;
        }

        double tMin = srcT.first() <= 1e-10 ? 1e-4 : srcT.first();
        double tMax = srcT.last();
        double logMin = log10(tMin);
        double logMax = log10(tMax);
        double step = (logMax - logMin) / (targetCount - 1);

        int currentIndex = 0;
        for (int i = 0; i < targetCount; ++i) {
            double targetT = pow(10, logMin + i * step);
            double minDiff = 1e30;
            int bestIdx = currentIndex;
            while (currentIndex < srcT.size()) {
                double diff = std::abs(srcT[currentIndex] - targetT);
                if (diff < minDiff) { minDiff = diff; bestIdx = currentIndex; }
                else break;
                currentIndex++;
            }
            currentIndex = bestIdx;
            points.append({srcT[bestIdx],
                           (bestIdx<srcP.size()?srcP[bestIdx]:0.0),
                           (bestIdx<srcD.size()?srcD[bestIdx]:0.0)});
        }
    }
    // 模式2：自定义区间策略
    else {
        if (m_customIntervals.isEmpty()) {
            outT = srcT; outP = srcP; outD = srcD;
            return;
        }
        for (const auto& interval : m_customIntervals) {
            double tStart = interval.tStart;
            double tEnd = interval.tEnd;
            int count = interval.count;
            if (count <= 0) continue;

            auto itStart = std::lower_bound(srcT.begin(), srcT.end(), tStart);
            auto itEnd = std::upper_bound(srcT.begin(), srcT.end(), tEnd);
            int idxStart = std::distance(srcT.begin(), itStart);
            int idxEnd = std::distance(srcT.begin(), itEnd);

            if (idxStart >= srcT.size() || idxStart >= idxEnd) continue;

            double subMin = srcT[idxStart];
            double subMax = srcT[idxEnd - 1];
            if (subMin <= 1e-10) subMin = 1e-4;

            double logMin = log10(subMin);
            double logMax = log10(subMax);
            double step = (count > 1) ? (logMax - logMin) / (count - 1) : 0;

            int subCurrentIdx = idxStart;
            for (int i = 0; i < count; ++i) {
                double targetT = (count == 1) ? subMin : pow(10, logMin + i * step);
                double minDiff = 1e30;
                int bestIdx = subCurrentIdx;
                while (subCurrentIdx < idxEnd) {
                    double diff = std::abs(srcT[subCurrentIdx] - targetT);
                    if (diff < minDiff) { minDiff = diff; bestIdx = subCurrentIdx; }
                    else break;
                    subCurrentIdx++;
                }
                subCurrentIdx = bestIdx;
                if (bestIdx < srcT.size()) {
                    points.append({srcT[bestIdx],
                                   (bestIdx<srcP.size()?srcP[bestIdx]:0.0),
                                   (bestIdx<srcD.size()?srcD[bestIdx]:0.0)});
                }
            }
        }
    }

    std::sort(points.begin(), points.end());
    auto last = std::unique(points.begin(), points.end());
    points.erase(last, points.end());

    for (const auto& p : points) {
        outT.append(p.t);
        outP.append(p.p);
        outD.append(p.d);
    }
}

QMap<QString, double> FittingCore::preprocessParams(const QMap<QString, double>& rawParams, ModelManager::ModelType type)
{
    QMap<QString, double> processed = rawParams;
    ModelParameter* mp = ModelParameter::instance();

    auto getSafeParam = [&](const QString& key, double mpVal, double defaultVal) {
        if (rawParams.contains(key)) return rawParams[key];
        if (std::abs(mpVal) > 1e-15) return mpVal;
        return defaultVal;
    };

    double phi = getSafeParam("phi", mp->getPhi(), 0.05);
    double h   = getSafeParam("h",   mp->getH(),   20.0);
    double Ct  = getSafeParam("Ct",  mp->getCt(),  5e-4);
    double mu  = getSafeParam("mu",  mp->getMu(),  0.5);
    double B   = getSafeParam("B",   mp->getB(),   1.05);
    double q   = getSafeParam("q",   mp->getQ(),   5.0);
    double rw  = getSafeParam("rw",  mp->getRw(),  0.1);

    // [算法核心] 提取 nf 时进行拦截，强转为正整数代入物理模型计算，使得拟合计算全过程对齐物理意义
    double nf  = getSafeParam("nf",  mp->getNf(),  9.0);

    processed["phi"] = phi;
    processed["h"] = h;
    processed["Ct"] = Ct;
    processed["mu"] = mu;
    processed["B"] = B;
    processed["q"] = q;
    processed["rw"] = rw;
    processed["nf"] = std::max(1.0, std::round(nf)); // 强制四舍五入为正整数

    double L = processed.value("L", 0.0);
    if (L < 1e-9) {
        L = 1000.0;
        processed["L"] = L;
    }

    int typeId = (int)type;
    bool isNonEqual = (typeId >= 108);

    if (!isNonEqual) {
        if (processed.contains("Lf")) {
            processed["LfD"] = processed["Lf"] / L;
        } else {
            processed["LfD"] = 0.0;
        }
    }

    if (!processed.contains("M12") && processed.contains("km")) {
        processed["M12"] = processed["km"];
    }

    int tBase = isNonEqual ? typeId - 108 : typeId;
    int storageType = tBase % 4;
    bool hasStorage = (storageType != 1);
    if (hasStorage) {
        if (!processed.contains("cD")) processed["cD"] = 0.1;
    } else {
        processed["cD"] = 0.0;
        processed["S"] = 0.0;
    }

    int gI = tBase % 12;
    bool isInfinite = (gI < 4);
    if (isInfinite) {
        if (!processed.contains("re")) processed["re"] = 20000.0;
    }

    return processed;
}

void FittingCore::runOptimizationTask(ModelManager::ModelType modelType, QList<FitParameter> fitParams, double weight, bool useLimits, bool useMultiStart) {
    runLevenbergMarquardtOptimization(modelType, fitParams, weight, useLimits, useMultiStart);
}

void FittingCore::runLevenbergMarquardtOptimization(ModelManager::ModelType modelType, QList<FitParameter> params, double weight, bool useLimits, bool useMultiStart) {
    if(m_modelManager) m_modelManager->setHighPrecision(false);

    QVector<int> fitIndices;
    for(int i=0; i<params.size(); ++i) {
        if(params[i].isFit && params[i].name != "LfD") fitIndices.append(i);
    }
    int nParams = fitIndices.size();

    QMap<QString, double> currentParamMap;
    for(const auto& p : params) currentParamMap.insert(p.name, p.value);

    QVector<double> fitT, fitP, fitD;
    getLogSampledData(m_obsTime, m_obsDeltaP, m_obsDerivative, fitT, fitP, fitD);

    QVector<double> residuals = calculateResiduals(currentParamMap, modelType, weight, fitT, fitP, fitD);
    double currentSSE = calculateSumSquaredError(residuals);

    // [P2新增] 多起点预扫描：在多个随机起点中选取最佳出发点
    if (useMultiStart && nParams > 0 && !m_stopRequested) {
        QMap<QString, double> bestParamMap = currentParamMap;
        double bestSSE = currentSSE;

        std::srand(static_cast<unsigned>(std::time(nullptr)));
        const int numTrials = 5;

        for (int trial = 0; trial < numTrials && !m_stopRequested; ++trial) {
            QMap<QString, double> trialMap = currentParamMap;

            // 对每个拟合参数施加随机对数扰动 (10^[-1, 1] 倍)
            for (int i = 0; i < nParams; ++i) {
                int pIdx = fitIndices[i];
                QString pName = params[pIdx].name;
                double val = currentParamMap[pName];
                double logPert = (double(std::rand()) / RAND_MAX) * 2.0 - 1.0;

                if (pName == "nf") {
                    trialMap[pName] = std::max(1.0, std::round(val + logPert * 5));
                } else if (pName == "S") {
                    trialMap[pName] = val + logPert * 5.0;
                } else if (val > 1e-12) {
                    trialMap[pName] = val * std::pow(10.0, logPert);
                }

                if (useLimits) {
                    trialMap[pName] = qMax(params[pIdx].min, qMin(trialMap[pName], params[pIdx].max));
                }
            }

            QVector<double> trialRes = calculateResiduals(trialMap, modelType, weight, fitT, fitP, fitD);
            double trialSSE = calculateSumSquaredError(trialRes);

            if (trialSSE < bestSSE && !std::isnan(trialSSE)) {
                bestSSE = trialSSE;
                bestParamMap = trialMap;
            }
        }

        // 采纳最佳起点
        currentParamMap = bestParamMap;
        currentSSE = bestSSE;
        residuals = calculateResiduals(currentParamMap, modelType, weight, fitT, fitP, fitD);
    }

    QMap<QString, double> solverParams = preprocessParams(currentParamMap, modelType);
    ModelCurveData curve = m_modelManager->calculateTheoreticalCurve(modelType, solverParams);
    emit sigIterationUpdated(currentSSE/residuals.size(), currentParamMap, std::get<0>(curve), std::get<1>(curve), std::get<2>(curve));

    if(nParams == 0) {
        emit sigFitFinished();
        return;
    }

    double lambda = 0.1; // 初始化阻尼参数
    int maxIter = 80;    // [性能优化] 最大迭代次数 200 → 80，配合更宽松的收敛条件
    double prevSSE = currentSSE;
    int stagnationCount = 0;
    int consecutiveFailures = 0;

    for(int iter = 0; iter < maxIter; ++iter) {
        if(m_stopRequested) break;
        // [性能优化] 绝对阈值放宽: 1e-6 → 1e-4
        if (!residuals.isEmpty() && (currentSSE / residuals.size()) < 1e-4) break;

        emit sigProgress(iter * 100 / maxIter);

        QVector<QVector<double>> J = computeJacobian(currentParamMap, residuals, fitIndices, modelType, params, weight, fitT, fitP, fitD);
        if(m_stopRequested) break;

        int nRes = residuals.size();

        QVector<QVector<double>> H(nParams, QVector<double>(nParams, 0.0));
        QVector<double> g(nParams, 0.0);

        for(int k=0; k<nRes; ++k) {
            for(int i=0; i<nParams; ++i) {
                g[i] += J[k][i] * residuals[k];
                for(int j=0; j<=i; ++j) {
                    H[i][j] += J[k][i] * J[k][j];
                }
            }
        }
        for(int i=0; i<nParams; ++i) {
            for(int j=i+1; j<nParams; ++j) H[i][j] = H[j][i];
        }

        bool stepAccepted = false;
        // 寻优探索尝试
        for(int tryIter=0; tryIter<20; ++tryIter) {
            if(m_stopRequested) break;

            QVector<QVector<double>> H_lm = H;
            for(int i=0; i<nParams; ++i) {
                H_lm[i][i] += lambda * (1.0 + std::abs(H[i][i]));
            }

            QVector<double> negG(nParams);
            for(int i=0;i<nParams;++i) negG[i] = -g[i];

            QVector<double> delta = solveLinearSystem(H_lm, negG);
            if (delta.isEmpty()) {
                lambda *= 10.0;
                continue;
            }

            QMap<QString, double> trialMap = currentParamMap;

            for(int i=0; i<nParams; ++i) {
                int pIdx = fitIndices[i];
                QString pName = params[pIdx].name;
                double oldVal = currentParamMap[pName];
                bool isLog = (oldVal > 1e-12 && pName != "S" && pName != "nf");
                double newVal;
                if(isLog) newVal = pow(10.0, log10(oldVal) + delta[i]);
                else newVal = oldVal + delta[i];

                // 异常数值保护
                if (std::isnan(newVal) || std::isinf(newVal)) newVal = oldVal;

                // 若启用了限幅控制，应用约束；否则任其寻优发展
                if (useLimits) {
                    newVal = qMax(params[pIdx].min, qMin(newVal, params[pIdx].max));
                }
                trialMap[pName] = newVal;
            }

            QVector<double> newRes = calculateResiduals(trialMap, modelType, weight, fitT, fitP, fitD);
            double newSSE = calculateSumSquaredError(newRes);

            // 如果这一步显著减小了残差误差，采纳并退出内部寻优循环
            if(newSSE < currentSSE && !std::isnan(newSSE)) {
                currentSSE = newSSE;
                currentParamMap = trialMap;
                residuals = newRes;
                lambda = std::max(1e-7, lambda / 10.0); // 缩小阻尼，偏向高斯牛顿法
                stepAccepted = true;

                // 相对变化检查：连续 2 次改善极微则认为收敛
                double relChange = (prevSSE > 1e-20) ?
                                   std::abs(prevSSE - currentSSE) / prevSSE : 0.0;
                if (relChange < 1e-4) {
                    stagnationCount++;
                } else {
                    stagnationCount = 0;
                }
                prevSSE = currentSSE;
                consecutiveFailures = 0;

                // 派发迭代更新
                QMap<QString, double> trialSolverParams = preprocessParams(trialMap, modelType);
                ModelCurveData iterCurve = m_modelManager->calculateTheoreticalCurve(modelType, trialSolverParams);
                emit sigIterationUpdated(currentSSE/nRes, currentParamMap, std::get<0>(iterCurve), std::get<1>(iterCurve), std::get<2>(iterCurve));
                break;
            } else {
                lambda *= 10.0; // 加大阻尼，偏向梯度下降
            }
        }

        // [P0优化] 容忍连续失败，不再单次失败即退出
        if(!stepAccepted) {
            consecutiveFailures++;
            if (consecutiveFailures >= 3) {
                break;  // 连续 3 次找不到下降方向，才真正放弃
            }
            // 强制大幅提升 lambda，偏向纯梯度下降以脱离僵局
            lambda = std::min(lambda * 100.0, 1e10);
        }

        // 连续 2 次接受步骤但改善微小 → 认为已收敛
        if (stagnationCount >= 2) break;
    }

    if(m_modelManager) m_modelManager->setHighPrecision(true);

    // [最后环节] 拟合结束后，必须将 nf 严格转换为正整数，以便显示和导出不出错
    if (currentParamMap.contains("nf")) {
        currentParamMap["nf"] = std::max(1.0, std::round(currentParamMap["nf"]));
    }

    QMap<QString, double> finalSolverParams = preprocessParams(currentParamMap, modelType);
    ModelCurveData finalCurve = m_modelManager->calculateTheoreticalCurve(modelType, finalSolverParams);
    emit sigIterationUpdated(currentSSE/residuals.size(), currentParamMap, std::get<0>(finalCurve), std::get<1>(finalCurve), std::get<2>(finalCurve));
}

QVector<double> FittingCore::calculateResiduals(const QMap<QString, double>& params, ModelManager::ModelType modelType, double weight,
                                                const QVector<double>& t, const QVector<double>& obsP, const QVector<double>& obsD) {
    if(!m_modelManager || t.isEmpty()) return QVector<double>();

    QMap<QString, double> solverParams = preprocessParams(params, modelType);

    ModelCurveData res = m_modelManager->calculateTheoreticalCurve(modelType, solverParams, t);
    const QVector<double>& pCal = std::get<1>(res);
    const QVector<double>& dpCal = std::get<2>(res);

    QVector<double> r;
    double wp = weight;
    double wd = 1.0 - weight;

    // [P0修复] 安全残差：使用 log-floor 策略保持梯度连续性
    // 当理论值非法 (NaN/Inf/≤0) 时，clamp 到 1e-20 计算对数
    // 这样即使理论值从 1e-15 变到 1e-14，log 差值仍能产生梯度
    // 旧逻辑将残差直接置 0 → 梯度为 0 → LM 无法找到下降方向 → 曲线"消失"且无法恢复
    auto safeResidual = [](double obs, double calc, double w) -> double {
        if (obs <= 1e-10) return 0.0;  // 观测数据无效才跳过
        double calcEff = calc;
        if (std::isnan(calcEff) || std::isinf(calcEff) || calcEff <= 0.0) {
            calcEff = 1e-20;
        } else if (calcEff < 1e-20) {
            calcEff = 1e-20;
        }
        return (std::log(obs) - std::log(calcEff)) * w;
    };

    int count = qMin((int)obsP.size(), (int)pCal.size());
    for(int i=0; i<count; ++i) {
        r.append(safeResidual(obsP[i], pCal[i], wp));
    }
    int dCount = qMin((int)obsD.size(), (int)dpCal.size());
    dCount = qMin(dCount, count);
    for(int i=0; i<dCount; ++i) {
        r.append(safeResidual(obsD[i], dpCal[i], wd));
    }
    return r;
}

QVector<QVector<double>> FittingCore::computeJacobian(const QMap<QString, double>& params, const QVector<double>& baseResiduals,
                                                      const QVector<int>& fitIndices, ModelManager::ModelType modelType,
                                                      const QList<FitParameter>& currentFitParams, double weight,
                                                      const QVector<double>& t, const QVector<double>& obsP, const QVector<double>& obsD) {
    int nRes = baseResiduals.size();
    int nParams = fitIndices.size();
    QVector<QVector<double>> J(nRes, QVector<double>(nParams, 0.0));

    for(int j = 0; j < nParams; ++j) {
        if (m_stopRequested) break;

        int idx = fitIndices[j];
        QString pName = currentFitParams[idx].name;
        double val = params.value(pName);
        bool isLog = (val > 1e-12 && pName != "S" && pName != "nf");

        double h;
        QMap<QString, double> pPlus = params;
        QMap<QString, double> pMinus = params;

        if (pName == "nf") {
            h = 0.51;
            pPlus[pName] = val + h;
            pMinus[pName] = val - h;
        } else if(isLog) {
            h = 0.01;
            double valLog = log10(val);
            pPlus[pName] = pow(10.0, valLog + h);
            pMinus[pName] = pow(10.0, valLog - h);
        } else {
            h = 1e-4;
            pPlus[pName] = val + h;
            pMinus[pName] = val - h;
        }

        QVector<double> rPlus = this->calculateResiduals(pPlus, modelType, weight, t, obsP, obsD);
        QVector<double> rMinus = this->calculateResiduals(pMinus, modelType, weight, t, obsP, obsD);

        // 中心差分商
        if(rPlus.size() == nRes && rMinus.size() == nRes) {
            for(int i=0; i<nRes; ++i) {
                J[i][j] = (rPlus[i] - rMinus[i]) / (2.0 * h);
            }
        }
    }

    return J;
}

QVector<double> FittingCore::solveLinearSystem(const QVector<QVector<double>>& A, const QVector<double>& b) {
    int n = b.size();
    if (n == 0) return QVector<double>();
    Eigen::MatrixXd matA(n, n);
    Eigen::VectorXd vecB(n);
    for (int i = 0; i < n; ++i) {
        vecB(i) = b[i];
        for (int j = 0; j < n; ++j) matA(i, j) = A[i][j];
    }

    // 使用更稳定的 Cholesky 分解
    Eigen::VectorXd x = matA.ldlt().solve(vecB);
    QVector<double> res(n);
    for (int i = 0; i < n; ++i) {
        res[i] = x(i);
    }
    return res;
}

double FittingCore::calculateSumSquaredError(const QVector<double>& residuals) {
    double sse = 0.0;
    for(double v : residuals) {
        sse += v*v;
    }
    return sse;
}

// ====================================================================
// [P1新增] 基于观测数据自动估算关键拟合参数初值
// 算法原理：利用试井分析诊断图的标准特征段反算物理参数
// - 径向流段导数平台 → 渗透率 kf
// - 早期单位斜率段   → 井储系数 C
// - 导数峰值/平台比  → 表皮系数 S
// - 导数凹陷深度     → 储容比 ω
// - 凹陷时间位置     → 窜流系数 λ
// ====================================================================
QMap<QString, double> FittingCore::estimateInitialParams(
    const QVector<double>& obsTime,
    const QVector<double>& obsDeltaP,
    const QVector<double>& obsDerivative,
    ModelManager::ModelType modelType)
{
    QMap<QString, double> estimated;
    ModelParameter* mp = ModelParameter::instance();

    double q   = mp->getQ();
    double mu  = mp->getMu();
    double B   = mp->getB();
    double h   = mp->getH();
    double phi = mp->getPhi();
    double Ct  = mp->getCt();
    double L   = mp->getL();

    int n = qMin(qMin(obsTime.size(), obsDeltaP.size()), obsDerivative.size());
    if (n < 10) return estimated; // 数据点不足

    // ========== 1. 从导数平台估算 kf (渗透率) ==========
    // 径向流段导数平台值 = 0.921 * q * μ * B / (kf * h)
    // 取中间 30%-70% 时间段的中位数作为平台估计值
    int startIdx = n * 3 / 10;
    int endIdx   = n * 7 / 10;

    QVector<double> plateauDerivs;
    for (int i = startIdx; i < endIdx; ++i) {
        if (obsDerivative[i] > 1e-10) {
            plateauDerivs.append(obsDerivative[i]);
        }
    }

    double derivPlateau = 0.0;
    if (!plateauDerivs.isEmpty()) {
        std::sort(plateauDerivs.begin(), plateauDerivs.end());
        derivPlateau = plateauDerivs[plateauDerivs.size() / 2]; // 中位数
    }

    double kf_est = 10.0;
    if (derivPlateau > 1e-15 && q > 0 && mu > 0 && B > 0 && h > 0) {
        kf_est = 0.921 * q * mu * B / (h * derivPlateau);
        kf_est = qBound(0.01, kf_est, 100000.0);
    }
    estimated["kf"] = kf_est;

    // ========== 2. 从早期单位斜率段估算 cD (无因次井储系数) ==========
    // 早期: ΔP = q·B·t / (24·C) → C = q·B·t / (24·ΔP)
    // cD = 0.159 * C / (phi * h * Ct * L_half^2)
    int storageType = (int)modelType % 4;
    if (storageType != 1) { // 非纯线源解模型
        int earlyEnd = qMin(n / 5, 20);
        double bestC = 10.0;
        double bestSlopeErr = 1e30;

        for (int i = 1; i < earlyEnd; ++i) {
            if (obsTime[i] > 1e-10 && obsDeltaP[i] > 1e-10
                && obsTime[i-1] > 1e-10 && obsDeltaP[i-1] > 1e-10) {
                double slope = (std::log10(obsDeltaP[i]) - std::log10(obsDeltaP[i-1]))
                             / (std::log10(obsTime[i]) - std::log10(obsTime[i-1]));
                double slopeErr = std::abs(slope - 1.0);
                if (slopeErr < bestSlopeErr) {
                    bestSlopeErr = slopeErr;
                    bestC = q * B * obsTime[i] / (24.0 * obsDeltaP[i]);
                }
            }
        }
        double L_half = L / 2.0;
        double denom = phi * h * Ct * L_half * L_half;
        double cD_est = (denom > 1e-20) ? 0.159 * bestC / denom : 0.1;
        estimated["cD"] = qBound(1e-4, cD_est, 10.0);
    }

    // ========== 3. 从导数峰值估算 S (表皮系数) ==========
    if (storageType != 1 && derivPlateau > 1e-15) {
        double maxDeriv = 0.0;
        for (int i = 0; i < n; ++i) {
            if (obsDerivative[i] > maxDeriv) maxDeriv = obsDerivative[i];
        }
        double S_est = 1.0;
        if (maxDeriv > derivPlateau * 1.1) {
            S_est = 0.5 * (maxDeriv / derivPlateau - 1.0);
        }
        estimated["S"] = qBound(-5.0, S_est, 50.0);
    }

    // ========== 4. 从导数凹陷估算 ω (储容比) ==========
    double minDeriv = 1e30;
    int minIdx = -1;
    for (int i = startIdx; i < endIdx; ++i) {
        if (obsDerivative[i] > 1e-10 && obsDerivative[i] < minDeriv) {
            minDeriv = obsDerivative[i];
            minIdx = i;
        }
    }

    int modelId = (int)modelType;
    bool isTriple = (modelId >= 72 && modelId <= 107);

    if (derivPlateau > 1e-15 && minDeriv < derivPlateau * 0.9 && minIdx > 0) {
        double omega_est = minDeriv / derivPlateau;
        omega_est = qBound(0.001, omega_est, 0.5);

        if (isTriple) {
            estimated["omega_f1"] = omega_est;
            estimated["omega_v1"] = omega_est * 0.5;
        } else {
            estimated["omega1"] = omega_est;
        }

        // ========== 5. 从凹陷时间估算 λ (窜流系数) ==========
        if (kf_est > 0 && phi > 0 && mu > 0 && Ct > 0 && L > 0) {
            double tDip = obsTime[minIdx];
            double L_half = L / 2.0;
            double tD_dip = 3.6 * kf_est * tDip / (phi * mu * Ct * L_half * L_half);
            double lambda_est = (tD_dip > 1e-20) ? 1.0 / tD_dip : 1e-3;
            lambda_est = qBound(1e-9, lambda_est, 1.0);

            if (isTriple) {
                estimated["lambda_m1"] = lambda_est;
                estimated["lambda_v1"] = lambda_est * 0.1;
            } else {
                estimated["lambda1"] = lambda_est;
            }
        }
    }

    // ========== 6. 裂缝半长 Lf 启发式估算 ==========
    double Lf_est = 50.0;
    if (L > 0) {
        Lf_est = L / 20.0;
        Lf_est = qBound(10.0, Lf_est, L / 2.0);
    }
    estimated["Lf"] = Lf_est;

    return estimated;
}
