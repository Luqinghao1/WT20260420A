/*
 * 文件名: fittingparameterchart.cpp
 * 作用与功能:
 * 1. 负责参数配置表格的动态渲染与修改监听。
 * 2. 【核心重构】实现了“动态标量展开”逻辑。彻底摒弃了易出错的字符串数组！
 * 3. 通过 syncFractureParams 实现了随着 nf 的增减，表格自动生成 Lf_1, xf_1 等独立行。
 * 每一条裂缝均可单独设置拟合开关与上下限，完美接入 LM 非线性反演引擎。
 * 4. 彻底清除了多重单行缩进，严格遵守 C++ 规范。
 */

#include "fittingparameterchart.h"
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QDebug>
#include <QBrush>
#include <QColor>
#include <QRegularExpression>
#include <QWheelEvent>
#include <cmath>

FittingParameterChart::FittingParameterChart(QTableWidget *parentTable, QObject *parent)
    : QObject(parent),
    m_table(parentTable),
    m_modelManager(nullptr),
    m_modelType(ModelManager::Model_1)
{
    m_wheelTimer = new QTimer(this);
    m_wheelTimer->setSingleShot(true);
    m_wheelTimer->setInterval(200);
    connect(m_wheelTimer, &QTimer::timeout, this, &FittingParameterChart::onWheelDebounceTimeout);

    if(m_table) {
        QStringList headers;
        headers << "序号" << "参数名称" << "数值" << "单位";
        m_table->setColumnCount(headers.size());
        m_table->setHorizontalHeaderLabels(headers);

        m_table->horizontalHeader()->setStyleSheet(
            "QHeaderView::section { background-color: #E0E0E0; color: black; font-weight: bold; border: 1px solid #A0A0A0; }"
            );
        m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_table->horizontalHeader()->setStretchLastSection(true);

        m_table->setColumnWidth(0, 40);
        m_table->setColumnWidth(1, 160);
        m_table->setColumnWidth(2, 80);

        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setAlternatingRowColors(false);
        m_table->verticalHeader()->setVisible(false);

        m_table->viewport()->installEventFilter(this);
        connect(m_table, &QTableWidget::itemChanged, this, &FittingParameterChart::onTableItemChanged);
    }
}

bool FittingParameterChart::eventFilter(QObject *w, QEvent *e)
{
    // 拦截表格的鼠标滚轮事件，实现快捷微调参数
    if (w == m_table->viewport() && e->type() == QEvent::Wheel) {
        QWheelEvent *we = static_cast<QWheelEvent*>(e);
        QTableWidgetItem *item = m_table->itemAt(we->position().toPoint());

        if (item && item->column() == 2) {
            QTableWidgetItem *kItem = m_table->item(item->row(), 1);
            if (!kItem) {
                return false;
            }

            QString pName = kItem->data(Qt::UserRole).toString();
            // 无因次缝长是衍生计算量，禁止滚轮修改
            if (pName == "LfD") {
                return true;
            }

            for (auto &p : m_params) {
                if (p.name == pName) {
                    QString txt = item->text();

                    // 现在的架构已经全部标量化，彻底告别带逗号的数组防护判断，但为了极端防呆，仍保留一层过滤
                    if (txt.contains(',') || txt.contains(QChar(0xFF0C))) {
                        return false;
                    }

                    bool ok;
                    double cur = txt.toDouble(&ok);

                    if (ok) {
                        double nV = cur + (we->angleDelta().y() / 120) * p.step;

                        // 保证裂缝条数为整数且大于等于1
                        if (p.name == "nf") {
                            nV = qMax(1.0, std::round(nV));
                        }

                        // 保证不超过设定的限幅
                        if (p.max > p.min) {
                            nV = qBound(p.min, nV, p.max);
                        }

                        if (p.name == "nf") {
                            item->setText(QString::number(nV));
                        } else {
                            item->setText(QString::number(nV, 'g', 6));
                        }

                        p.value = nV;
                        m_wheelTimer->start();
                        return true;
                    }
                }
            }
        }
    }
    return QObject::eventFilter(w, e);
}

void FittingParameterChart::onWheelDebounceTimeout()
{
    emit parameterChangedByWheel();
}

void FittingParameterChart::onTableItemChanged(QTableWidgetItem *item)
{
    if (!item || item->column() != 2) return;

    QTableWidgetItem *kItem = m_table->item(item->row(), 1);
    if (!kItem) return;

    QString key = kItem->data(Qt::UserRole).toString();

    // =========================================================================
    // 【核心动态展开监听】：如果用户修改了裂缝条数(nf)
    // 立即触发重构机制，将 Lf_i 与 xf_i 的行数与新设定的 nf 绝对对齐
    // =========================================================================
    if (key == "nf") {
        double val = item->text().toDouble();
        int newNf = qMax(1, (int)std::round(val));

        int oldNf = 1;
        for (const auto& p : m_params) {
            if (p.name == "nf") {
                oldNf = (int)p.value;
                break;
            }
        }

        if (newNf != oldNf) {
            syncFractureParams(newNf);
        }

        if (!m_wheelTimer->isActive()) {
            m_wheelTimer->start();
        }
        return;
    }

    // 同步 LfD 无因次缝长 (仅针对 1~107 等长均质模型有效)
    if (key == "L" || key == "Lf") {
        double valL = 0.0;
        double valLf = 0.0;
        QTableWidgetItem* iLfD = nullptr;

        for(int i = 0; i < m_table->rowCount(); ++i) {
            QTableWidgetItem* k = m_table->item(i, 1);
            QTableWidgetItem* v = m_table->item(i, 2);

            if(k && v) {
                QString cK = k->data(Qt::UserRole).toString();
                if (cK == "L") {
                    valL = v->text().toDouble();
                } else if (cK == "Lf") {
                    valLf = v->text().toDouble();
                } else if (cK == "LfD") {
                    iLfD = v;
                }
            }
        }

        if (valL > 1e-9 && iLfD) {
            double nLfD = valLf / valL;
            m_table->blockSignals(true);
            iLfD->setText(QString::number(nLfD, 'g', 6));
            m_table->blockSignals(false);

            for(auto& p : m_params) {
                if(p.name == "LfD") {
                    p.value = nLfD;
                    break;
                }
            }
        }
        if (!m_wheelTimer->isActive()) {
            m_wheelTimer->start();
        }
    }
}

/**
 * @brief 【核心数据重构函数】：根据新的裂缝条数，动态重构物理参数列表。
 * 添加或剔除对应数量的 Lf_i / xf_i，并保留那些没有被剔除的旧参数用户输入的值！
 */
void FittingParameterChart::syncFractureParams(int newNf)
{
    int t = (int)m_modelType;

    // 如果是 0-107 均布等长模型，不存在多条参数，直接返回
    if (t < 108) {
        return;
    }

    // 1. 缓存旧的参数状态
    QMap<QString, FitParameter> oldParams;
    for (const auto& p : m_params) {
        oldParams[p.name] = p;
    }

    // 2. 利用 overrideNf 获取包含了全新条数阵列的平铺参数模板
    m_params = generateDefaultParams(m_modelType, newNf);

    // 3. 将缓存中的旧数据原封不动还给新列表，确保用户的输入不被刷没
    for (auto& p : m_params) {
        if (oldParams.contains(p.name)) {
            p.value = oldParams[p.name].value;
            p.isFit = oldParams[p.name].isFit;
            p.isVisible = oldParams[p.name].isVisible;
        }
    }

    // 4. 重新计算所有标量的上下限并刷新 UI 表格
    autoAdjustLimits();
    refreshParamTable();
}

void FittingParameterChart::setModelManager(ModelManager *m)
{
    m_modelManager = m;
}

/**
 * @brief 生成底层默认参数模板。
 * 【核心演进】将裂缝阵列全部平铺为独立物理标量！
 */
QList<FitParameter> FittingParameterChart::generateDefaultParams(ModelManager::ModelType type, int overrideNf)
{
    QList<FitParameter> params;
    QMap<QString, double> defs = ModelManager::getDefaultParameters(type);

    auto addParam = [&](QString name, bool isFitDefault, double fallbackValue = 0.0) {
        FitParameter p;
        p.name = name;
        p.value = defs.contains(name) ? defs.value(name) : fallbackValue;
        p.isFit = isFitDefault;
        p.isVisible = true;
        p.min = p.max = p.step = 0;

        QString dummy;
        getParamDisplayInfo(p.name, p.displayName, dummy, dummy, dummy);
        params.append(p);
    };

    // 第一级核心物理参数
    addParam("phi", false, 0.05);
    addParam("h", false, 20.0);
    addParam("rw", false, 0.1);
    addParam("mu", false, 0.5);
    addParam("B", false, 1.2);
    addParam("Ct", false, 5e-4);
    addParam("q", false, 50.0);

    int t = static_cast<int>(type);
    int tBase = t % 108;

    addParam("kf", true, 10.0);
    addParam("k2", true, 10.0);
    addParam("L", true, 1000.0);

    // 确认本次构建所依据的裂缝条数
    int nf = (overrideNf > 0) ? overrideNf : (int)defs.value("nf", 9);

    // ============================================
    // 【核心重构：多维阵列平铺化展开】
    // 将缝长与缝位置，逐一以独立 ID 映射入参数表
    // 使得 LM 引擎可以对 Lf_1, Lf_2, xf_1 等参数进行独立拟合
    // ============================================
    if (t >= 108) {
        // 针对非等长模型，展开每条裂缝的独立半长
        for (int i = 1; i <= nf; ++i) {
            addParam(QString("Lf_%1").arg(i), true, 50.0);
        }
    } else {
        // 均长模型只需要一个全局半长参数
        addParam("Lf", true, 50.0);
    }

    if (t >= 216) {
        // 针对非均布模型，展开每条裂缝的独立绝对坐标 xf
        double L_total = defs.value("L", 1000.0);
        for (int i = 1; i <= nf; ++i) {
            // 提供距离起点的均匀分布位置作为默认初值
            double default_pos = 0.05 * L_total + 0.9 * L_total * (i - 1) / std::max(1, nf - 1);
            addParam(QString("xf_%1").arg(i), true, default_pos);
        }
    }

    // 将裂缝条数本身作为一个参数装入，由它统领整体结构
    addParam("nf", false, nf);  // 通常情况下，裂缝条数作为已知前提不参与拟合
    addParam("rm", true, 50000.0);

    if (defs.contains("re")) {
        addParam("re", true, 20000.0);
    }

    if (tBase >= 72 && tBase <= 107) {
        addParam("omega_f1", true, 0.02);
        addParam("omega_v1", true, 0.01);
        addParam("lambda_m1", true, 4e-4);
        addParam("lambda_v1", true, 1e-4);
        if (tBase - 72 < 24) {
            addParam("omega_f2", true, 0.008);
            addParam("lambda_m2", true, 1e-7);
        }
    } else if (!(tBase >= 7 && tBase <= 12)) {
        addParam("omega1", true, 0.1);
        addParam("omega2", true, 0.001);
        addParam("lambda1", true, 2e-3);
        addParam("lambda2", true, 1e-3);
    }

    int sType = tBase % 4;
    if (sType != 1) {
        addParam("cD", true, 0.1);
        addParam("S", true, 1.0);
    }

    if (sType == 2 || sType == 3) {
        addParam("alpha", false, 0.1);
        addParam("C_phi", false, 1e-4);
    }
    addParam("gamaD", false, 0.006);

    // 只有 0-107 模型，才需要保留 LfD 这个显示辅助参数
    if (t < 108) {
        FitParameter lfd;
        lfd.name = "LfD";
        lfd.displayName = "无因次缝长 LfD";
        lfd.value = defs.value("Lf", 50.0) / defs.value("L", 1000.0);
        lfd.isFit = false;
        lfd.isVisible = true;
        lfd.step = 0;
        params.append(lfd);
    }

    return params;
}

void FittingParameterChart::adjustLimits(QList<FitParameter>& params)
{
    for(auto& p : params) {
        if(p.name == "LfD") continue;

        double val = p.value;
        if (std::abs(val) > 1e-15) {
            p.min = val > 0 ? val * 0.1 : val * 10.0;
            p.max = val > 0 ? val * 10.0 : val * 0.1;
        } else {
            p.min = 0.0;
            p.max = 1.0;
        }

        if (p.name == "phi" || p.name.startsWith("omega")) {
            p.max = qMin(p.max, 1.0);
            p.min = qMax(p.min, 0.0001);
        }

        // 【关键保护】因为我们明确 xf 表示距离起点的绝对坐标，因此严格拦截负数
        if (p.name.startsWith("xf_")) {
            p.min = 0.0;
            p.max = 20000.0;
        }
        else if (p.name == "kf" || p.name == "k2" || p.name == "L" || p.name == "Lf" ||
                 p.name.startsWith("Lf_") || p.name == "rm" || p.name == "re" ||
                 p.name.startsWith("lambda") || p.name == "h" || p.name == "rw" ||
                 p.name == "mu" || p.name == "B" || p.name == "Ct" || p.name == "C" ||
                 p.name == "q" || p.name == "alpha" || p.name == "C_phi")
        {
            p.min = qMax(p.min, qMax(std::abs(val) * 0.01, 1e-6));
        }

        if (p.name == "nf") {
            p.min = qMax(std::ceil(p.min), 1.0);
            p.max = std::floor(p.max);
            p.step = 1.0;
        }

        // 智能步长算法计算
        if (p.max - p.min > 1e-20 && p.name != "nf") {
            double rs = (p.max - p.min) / 20.0;
            double mag = std::pow(10.0, std::floor(std::log10(rs)));
            p.step = qMax(std::round(rs / mag * 10.0) / 10.0, 0.1) * mag;
        } else if (p.name != "nf") {
            p.step = 0.1;
        }
    }
}

void FittingParameterChart::resetParams(ModelManager::ModelType type, bool preserveStates)
{
    m_modelType = type;
    QMap<QString, QPair<bool, bool>> bkp;

    if (preserveStates) {
        for(const auto& p : m_params) {
            bkp[p.name] = {p.isFit, p.isVisible};
        }
    }

    m_params = generateDefaultParams(type);

    if (preserveStates) {
        for(auto& p : m_params) {
            if(bkp.contains(p.name)) {
                p.isFit = (p.name == "LfD") ? false : bkp[p.name].first;
                p.isVisible = bkp[p.name].second;
            }
        }
    }

    autoAdjustLimits();
    refreshParamTable();
}

void FittingParameterChart::autoAdjustLimits()
{
    adjustLimits(m_params);
}

QList<FitParameter> FittingParameterChart::getParameters() const
{
    return m_params;
}

void FittingParameterChart::setParameters(const QList<FitParameter> &p)
{
    m_params = p;
    refreshParamTable();
}

void FittingParameterChart::switchModel(ModelManager::ModelType newType)
{
    m_modelType = newType;
    QMap<QString, double> old;
    for(const auto& p : m_params) {
        old.insert(p.name, p.value);
    }

    resetParams(newType, false);

    for(auto& p : m_params) {
        if(old.contains(p.name)) {
            p.value = old[p.name];
        }
    }

    autoAdjustLimits();

    double cL = 1000.0;
    for(const auto& p : m_params) {
        if(p.name == "L") {
            cL = p.value;
        }
    }

    for(auto& p : m_params) {
        if(p.name == "rm") {
            p.min = qMax(p.min, cL);
            p.value = qMax(p.value, p.min);
        }
        if(p.name == "LfD") {
            double cLf = 20.0;
            for(const auto& pp : m_params) {
                if(pp.name == "Lf") cLf = pp.value;
            }
            if(cL > 1e-9) {
                p.value = cLf / cL;
            }
        }
    }
    refreshParamTable();
}

void FittingParameterChart::updateParamsFromTable()
{
    if(!m_table) return;

    for(int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* iK = m_table->item(i, 1);
        if(!iK) continue;

        QTableWidgetItem* iV = m_table->item(i, 2);
        QString k = iK->data(Qt::UserRole).toString();

        // 【纯净标量处理】：由于阵列已全部平展为 Lf_1, Lf_2 等等标量，我们直接安全的 toDouble() 即可！
        double v = iV->text().toDouble();

        if (k == "nf") {
            v = qMax(1.0, std::round(v));
        }

        for(auto& p : m_params) {
            if(p.name == k) {
                p.value = v;
                break;
            }
        }
    }
}

QMap<QString, QString> FittingParameterChart::getRawParamTexts() const
{
    QMap<QString, QString> res;
    if(!m_table) return res;

    for(int i = 0; i < m_table->rowCount(); ++i) {
        QTableWidgetItem* k = m_table->item(i, 1);
        QTableWidgetItem* v = m_table->item(i, 2);
        if (k && v) {
            res.insert(k->data(Qt::UserRole).toString(), v->text());
        }
    }
    return res;
}

void FittingParameterChart::refreshParamTable()
{
    if(!m_table) return;

    m_table->blockSignals(true);
    m_table->setRowCount(0);

    int no = 1;
    for(const auto& p : m_params) {
        if(p.isVisible && p.isFit) {
            addRowToTable(p, no, true);
        }
    }

    for(const auto& p : m_params) {
        if(p.isVisible && !p.isFit) {
            addRowToTable(p, no, false);
        }
    }

    m_table->blockSignals(false);
}

void FittingParameterChart::addRowToTable(const FitParameter& p, int& serialNo, bool highlight)
{
    int r = m_table->rowCount();
    m_table->insertRow(r);

    QColor bg = (p.name == "LfD") ? QColor(245, 245, 245) : (highlight ? QColor(255, 255, 224) : Qt::white);

    QTableWidgetItem* i0 = new QTableWidgetItem(QString::number(serialNo++));
    i0->setFlags(i0->flags() & ~Qt::ItemIsEditable);
    i0->setTextAlignment(Qt::AlignCenter);
    i0->setBackground(bg);
    m_table->setItem(r, 0, i0);

    QTableWidgetItem* i1 = new QTableWidgetItem(p.displayName);
    i1->setFlags(i1->flags() & ~Qt::ItemIsEditable);
    i1->setData(Qt::UserRole, p.name);
    i1->setBackground(bg);

    if(highlight) {
        QFont f = i1->font();
        f.setBold(true);
        i1->setFont(f);
    }
    m_table->setItem(r, 1, i1);

    QTableWidgetItem* i2 = nullptr;
    if (p.name == "nf") {
        i2 = new QTableWidgetItem(QString::number(qMax(1.0, std::round(p.value))));
    } else {
        i2 = new QTableWidgetItem(QString::number(p.value, 'g', 6));
    }

    i2->setBackground(bg);
    if(highlight) {
        QFont f = i2->font();
        f.setBold(true);
        i2->setFont(f);
    }
    if (p.name == "LfD") {
        i2->setFlags(i2->flags() & ~Qt::ItemIsEditable);
        i2->setForeground(QBrush(Qt::darkGray));
    }
    m_table->setItem(r, 2, i2);

    QString d, u;
    getParamDisplayInfo(p.name, d, d, d, u);

    QTableWidgetItem* i3 = new QTableWidgetItem((u == "无因次" || u == "小数") ? "-" : u);
    i3->setFlags(i3->flags() & ~Qt::ItemIsEditable);
    i3->setBackground(bg);
    m_table->setItem(r, 3, i3);
}

void FittingParameterChart::getParamDisplayInfo(const QString &n, QString &cN, QString &s, QString &uS, QString &u)
{
    if(n == "kf") { cN = "内区渗透率 kf"; u = "mD"; }
    else if(n == "k2") { cN = "外区渗透率 k₂"; u = "mD"; }
    else if(n == "L") { cN = "水平井长 L"; u = "m"; }
    else if(n == "Lf") { cN = "单段裂缝半长 Lf"; u = "m"; }
    else if(n == "rm") { cN = "复合半径 rm"; u = "m"; }
    else if(n == "omega1") { cN = "内区储容比 ω₁"; u = "无因次"; }
    else if(n == "omega2") { cN = "外区储容比 ω₂"; u = "无因次"; }
    else if(n == "lambda1") { cN = "内区窜流系数 λ₁"; u = "无因次"; }
    else if(n == "lambda2") { cN = "外区窜流系数 λ₂"; u = "无因次"; }
    else if(n == "omega_f1") { cN = "内区裂缝储容比 ωf₁"; u = "无因次"; }
    else if(n == "omega_v1") { cN = "内区溶洞储容比 ωv₁"; u = "无因次"; }
    else if(n == "lambda_m1") { cN = "内区基质窜流系数 λm₁"; u = "无因次"; }
    else if(n == "lambda_v1") { cN = "内区溶洞窜流系数 λv₁"; u = "无因次"; }
    else if(n == "omega_f2") { cN = "外区裂缝储容比 ωf₂"; u = "无因次"; }
    else if(n == "lambda_m2") { cN = "外区基质窜流系数 λm₂"; u = "无因次"; }
    else if(n == "re") { cN = "外区半径 re"; u = "m"; }
    else if(n == "nf") { cN = "裂缝条数 nf"; u = "条"; }

    // 【解析并动态翻译裂缝位置和独立半长为中文直观标签】
    else if(n.startsWith("Lf_")) {
        int idx = n.mid(3).toInt();
        cN = QString("第 %1 段半长 Lf_%2").arg(idx).arg(idx);
        u = "m";
    }
    else if(n.startsWith("xf_")) {
        int idx = n.mid(3).toInt();
        cN = QString("第 %1 段位置 xf_%2").arg(idx).arg(idx);
        u = "m";
    }

    else if(n == "h") { cN = "有效厚度 h"; u = "m"; }
    else if(n == "rw") { cN = "井筒半径 rw"; u = "m"; }
    else if(n == "phi") { cN = "孔隙度 φ"; u = "小数"; }
    else if(n == "mu") { cN = "流体粘度 μ"; u = "mPa·s"; }
    else if(n == "B") { cN = "体积系数 B"; u = "无因次"; }
    else if(n == "Ct") { cN = "综合压缩系数 Ct"; u = "MPa⁻¹"; }
    else if(n == "q") { cN = "测试产量 q"; u = "m³/d"; }
    else if(n == "cD") { cN = "无因次井储 cD"; u = "无因次"; }
    else if(n == "S") { cN = "表皮系数 S"; u = "无因次"; }
    else if(n == "gamaD") { cN = "压敏系数 γD"; u = "无因次"; }
    else if(n == "LfD") { cN = "无因次缝长 LfD"; u = "无因次"; }
    else if(n == "alpha") { cN = "变井储时间参数 α"; u = "h"; }
    else if(n == "C_phi") { cN = "变井储压力参数 Cφ"; u = "MPa"; }
    else { cN = n; u = ""; }

    s = uS = n;
}

