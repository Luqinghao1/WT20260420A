/*
 * 文件名: modelparameter.cpp
 * 作用与功能:
 * 1. 充当试井软件的【第一级全局基础参数单例管理器】的核心实现部分。
 * 2. 在程序启动或新建项目时，负责实例化全局基础客观物理属性（如孔隙度、厚度、粘度等），并提供默认兜底参数。
 * 3. 负责与项目工程文件 (.pwt) 及其附属的 JSON 数据文件（_chart.json, _date.json）进行硬盘间的序列化与反序列化读取与保存。
 * 4. 在界面切换时，保证客观基础物性参数全局绝对一致，实现“一处设置，全局通用”。
 * 5. 存储和管理多维度的业务扩展数据，包括自动拟合的历史逼近数据以及图版、数据表等分析结果。
 */

#include "modelparameter.h"
#include <QFile>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDebug>

// 初始化静态单例指针为空
ModelParameter* ModelParameter::m_instance = nullptr;

/**
 * @brief 构造函数：按照 MATLAB 代码中的设定，对全局客观物理属性赋予符合油藏工程理论的初始占位值
 * @param parent 父对象指针
 */
ModelParameter::ModelParameter(QObject* parent) : QObject(parent), m_hasLoaded(false)
{
    // ========================================================================
    // 【修改默认值指引】: 若需修改基础全局物理参数的默认值，请直接修改以下等号右侧的数值
    // ========================================================================
    m_L = 1000.0;   // 设置水平井总长的默认值为 1000 m
    m_nf = 9.0;     // 设置裂缝条数的默认值为 9 条
    m_q = 10.0;     // 设置默认产量的默认值为 10 m³/d
    m_phi = 0.05;   // 设置默认致密孔隙度的默认值为 0.05
    m_h = 10.0;     // 设置默认储层厚度的默认值为 10 m
    m_rw = 0.1;     // 设置默认井筒半径的默认值为 0.1 m
    m_Ct = 0.005;   // 设置默认综合压缩系数的默认值为 5e-3 MPa^-1
    m_mu = 5.0;     // 设置默认粘度的默认值为 5.0 mPa·s
    m_B = 1.2;      // 设置默认体积系数的默认值为 1.2

    // 特殊扩展项设定 (专门针对变井储模型)
    m_alpha = 0.1;    // 设置变井储时间参数 alpha 的默认值
    m_C_phi = 0.0001; // 设置变井储压力参数 C_phi 的默认值
}

/**
 * @brief 懒汉式获取全局唯一实例的指针
 * @return ModelParameter* 唯一的实例指针
 */
ModelParameter* ModelParameter::instance()
{
    // 如果单例尚未创建，则分配内存创建新实例
    if (!m_instance) m_instance = new ModelParameter();
    // 返回单例指针
    return m_instance;
}

/**
 * @brief 在新建项目确认时调用，更新内存中的所有第一级物性参数并构建基础 JSON 对象树格式
 */
void ModelParameter::setParameters(double phi, double h, double mu, double B, double Ct,
                                   double q, double rw, double L, double nf, const QString& path)
{
    // 将传入的物性参数逐一赋值给成员变量
    m_phi = phi; m_h = h; m_mu = mu; m_B = B; m_Ct = Ct; m_q = q; m_rw = rw;
    m_L = L; m_nf = nf;

    // 记录项目工程文件的绝对路径
    m_projectFilePath = path;

    // 解析出工程文件所在的上级目录并存储
    QFileInfo fi(path);
    m_projectPath = fi.isFile() ? fi.absolutePath() : path;
    // 标记系统已成功挂载项目
    m_hasLoaded = true;

    // 如果该项目没有旧数据缓存，则初始化标准的 JSON 存储树结构
    if (m_fullProjectData.isEmpty()) {
        // 构建油藏客观属性 JSON 节点
        QJsonObject reservoir;
        reservoir["porosity"] = m_phi;           // 存入孔隙度
        reservoir["thickness"] = m_h;            // 存入厚度
        reservoir["wellRadius"] = m_rw;          // 存入井筒半径
        reservoir["productionRate"] = m_q;       // 存入产量
        reservoir["horizLength"] = m_L;          // 存入水平井段长度
        reservoir["fracCount"] = m_nf;           // 存入裂缝条数
        reservoir["alpha"] = m_alpha;            // 存入变井储时间参数
        reservoir["C_phi"] = m_C_phi;            // 存入变井储压力参数

        // 构建流体 PVT 属性 JSON 节点
        QJsonObject pvt;
        pvt["viscosity"] = m_mu;                 // 存入粘度
        pvt["volumeFactor"] = m_B;               // 存入体积系数
        pvt["compressibility"] = m_Ct;           // 存入综合压缩系数

        // 将构建好的子节点挂载到总项目数据字典中
        m_fullProjectData["reservoir"] = reservoir;
        m_fullProjectData["pvt"] = pvt;
    }
}

/**
 * @brief 单独设置并写入变井储时间参数
 */
void ModelParameter::setAlpha(double v) { m_alpha = v; }

/**
 * @brief 单独设置并写入变井储压力参数
 */
void ModelParameter::setCPhi(double v) { m_C_phi = v; }

/**
 * @brief 自动推导并返回绘图数据文件 (_chart.json) 的完整路径
 */
QString ModelParameter::getPlottingDataFilePath() const
{
    // 如果工程路径为空，则返回空字符串
    if (m_projectFilePath.isEmpty()) return QString();
    // 组合当前路径与基础文件名，生成对应的 _chart.json 后缀路径
    QFileInfo fi(m_projectFilePath);
    return fi.absolutePath() + "/" + fi.completeBaseName() + "_chart.json";
}

/**
 * @brief 自动推导并返回数据表格文件 (_date.json) 的完整路径
 */
QString ModelParameter::getTableDataFilePath() const
{
    // 如果工程路径为空，则返回空字符串
    if (m_projectFilePath.isEmpty()) return QString();
    // 组合当前路径与基础文件名，生成对应的 _date.json 后缀路径
    QFileInfo fi(m_projectFilePath);
    return fi.absolutePath() + "/" + fi.completeBaseName() + "_date.json";
}

/**
 * @brief 读取硬盘上的 .pwt 文件并反序列化回程序的各个第一级参数变量中
 */
bool ModelParameter::loadProject(const QString& filePath)
{
    // 实例化文件对象并尝试以只读模式打开
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false; // 打开失败返回 false

    // 读取全部二进制数据并关闭文件句柄
    QByteArray data = file.readAll();
    file.close();

    // 尝试将二进制数据解析为 JSON 文档
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false; // JSON 格式错误返回 false

    // 将解析出的 JSON 对象缓存至内存中
    m_fullProjectData = doc.object();

    // 尝试从 JSON 中解析油藏相关的物理属性并恢复到内存变量中
    if (m_fullProjectData.contains("reservoir")) {
        QJsonObject res = m_fullProjectData["reservoir"].toObject();
        m_q = res["productionRate"].toDouble(10.0);
        m_phi = res["porosity"].toDouble(0.05);
        m_h = res["thickness"].toDouble(10.0);
        m_rw = res["wellRadius"].toDouble(0.1);
        m_L = res["horizLength"].toDouble(1000.0);
        m_nf = res["fracCount"].toDouble(9.0);
        m_alpha = res["alpha"].toDouble(0.1);
        m_C_phi = res["C_phi"].toDouble(0.0001);
    }

    // 尝试从 JSON 中解析 PVT 相关的流体属性并恢复到内存变量中
    if (m_fullProjectData.contains("pvt")) {
        QJsonObject pvt = m_fullProjectData["pvt"].toObject();
        m_Ct = pvt["compressibility"].toDouble(0.005);
        m_mu = pvt["viscosity"].toDouble(5.0);
        m_B = pvt["volumeFactor"].toDouble(1.2);
    }

    // 记录并锁定当前读取成功的文件路径及挂载状态
    m_projectFilePath = filePath;
    m_projectPath = QFileInfo(filePath).absolutePath();
    m_hasLoaded = true;

    // 步骤：静默加载并挂载可能存在的附属绘图分析文件
    QFile chartFile(getPlottingDataFilePath());
    if (chartFile.exists() && chartFile.open(QIODevice::ReadOnly)) {
        QJsonDocument d = QJsonDocument::fromJson(chartFile.readAll());
        if (!d.isNull() && d.isObject() && d.object().contains("plotting_data")) {
            // 将读取到的绘图数据附加进全量字典中
            m_fullProjectData["plotting_data"] = d.object()["plotting_data"];
        }
        chartFile.close();
    }

    // 步骤：静默加载并挂载可能存在的附属分析表格文件
    QFile dateFile(getTableDataFilePath());
    if (dateFile.exists() && dateFile.open(QIODevice::ReadOnly)) {
        QJsonDocument d = QJsonDocument::fromJson(dateFile.readAll());
        if (!d.isNull() && d.isObject() && d.object().contains("table_data")) {
            // 将读取到的表格数据附加进全量字典中
            m_fullProjectData["table_data"] = d.object()["table_data"];
        }
        dateFile.close();
    } else {
        // 若没有找到对应的表格文件，则清除字典中的无效或过期节点
        m_fullProjectData.remove("table_data");
    }

    return true; // 最终成功加载完毕
}

/**
 * @brief 将内存中发生更改的第一级参数组装成 JSON，并向硬盘持久化覆写
 */
bool ModelParameter::saveProject()
{
    // 必须确保系统已经挂载了有效项目
    if (!m_hasLoaded || m_projectFilePath.isEmpty()) return false;

    // 提取当前的 reservoir 节点并全量更新最新值
    QJsonObject reservoir = m_fullProjectData["reservoir"].toObject();
    reservoir["porosity"] = m_phi;
    reservoir["thickness"] = m_h;
    reservoir["wellRadius"] = m_rw;
    reservoir["productionRate"] = m_q;
    reservoir["horizLength"] = m_L;
    reservoir["fracCount"] = m_nf;
    reservoir["alpha"] = m_alpha;
    reservoir["C_phi"] = m_C_phi;
    m_fullProjectData["reservoir"] = reservoir; // 写回字典

    // 提取当前的 pvt 节点并全量更新最新值
    QJsonObject pvt = m_fullProjectData["pvt"].toObject();
    pvt["viscosity"] = m_mu;
    pvt["volumeFactor"] = m_B;
    pvt["compressibility"] = m_Ct;
    m_fullProjectData["pvt"] = pvt; // 写回字典

    // 步骤：分离存储策略，主工程文件不包含过大、非核心的缓存分析数据
    QJsonObject dataToWrite = m_fullProjectData;
    dataToWrite.remove("plotting_data");
    dataToWrite.remove("table_data");

    // 打开当前挂载文件并进行覆盖写入操作
    QFile file(m_projectFilePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(dataToWrite).toJson());
    file.close();

    return true; // 落盘成功
}

/**
 * @brief 关闭当前已加载的工程状态，并执行深度数据清理
 */
void ModelParameter::closeProject() {
    // 调用统一重置函数清除内存
    resetAllData();
}

/**
 * @brief 保存拟合模块传入的最佳逼近反演参数与误差历史
 */
void ModelParameter::saveFittingResult(const QJsonObject& fittingData)
{
    // 安全校验，文件路径不能为空
    if (m_projectFilePath.isEmpty()) return;

    // 将传入的拟合数据写入全局缓存树
    m_fullProjectData["fitting"] = fittingData;

    // 触发局部主文件保存逻辑
    QFile file(m_projectFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject dataToWrite = m_fullProjectData;
        // 剥离巨大的绘图与表格数据，防止主文件过载
        dataToWrite.remove("plotting_data");
        dataToWrite.remove("table_data");
        file.write(QJsonDocument(dataToWrite).toJson());
        file.close();
    }
}

/**
 * @brief 提取当前工程中记录在案的自动拟合结果数据包
 */
QJsonObject ModelParameter::getFittingResult() const {
    // 直接返回 JSON 树中名为 fitting 的子节点
    return m_fullProjectData.value("fitting").toObject();
}

/**
 * @brief 将图版模块生成的曲线集合剥离并保存入专属 JSON 附属文件
 */
void ModelParameter::savePlottingData(const QJsonArray& plots)
{
    // 安全校验，文件路径不能为空
    if (m_projectFilePath.isEmpty()) return;

    // 更新内存中的绘图缓存
    m_fullProjectData["plotting_data"] = plots;

    // 构建独立的图表 JSON 对象
    QJsonObject dataObj;
    dataObj["plotting_data"] = plots;

    // 寻找图表附属文件并执行硬盘写入
    QFile file(getPlottingDataFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(dataObj).toJson());
        file.close();
    }
}

/**
 * @brief 提取当前绑定的附属分析图表数据
 */
QJsonArray ModelParameter::getPlottingData() const {
    // 解析并返回 plotting_data 数组
    return m_fullProjectData.value("plotting_data").toArray();
}

/**
 * @brief 将数据表模块的明细矩阵剥离并保存入专属 JSON 附属文件
 */
void ModelParameter::saveTableData(const QJsonArray& tableData)
{
    // 安全校验，文件路径不能为空
    if (m_projectFilePath.isEmpty()) return;

    // 更新内存中的数据表缓存
    m_fullProjectData["table_data"] = tableData;

    // 构建独立的数据表 JSON 对象
    QJsonObject dataObj;
    dataObj["table_data"] = tableData;

    // 寻找表格附属文件并执行硬盘写入
    QFile file(getTableDataFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(dataObj).toJson());
        file.close();
    }
}

/**
 * @brief 提取当前绑定的附属明细表格数据
 */
QJsonArray ModelParameter::getTableData() const {
    // 解析并返回 table_data 数组
    return m_fullProjectData.value("table_data").toArray();
}

/**
 * @brief 强制所有变量退回默认出厂物理设定，并彻底清除内存中的残留 JSON 树，用于关闭项目或清空操作
 */
void ModelParameter::resetAllData()
{
    // 将所有基础物理参数复原至默认预设值
    m_L = 1000.0; m_nf = 9.0; m_q = 10.0; m_phi = 0.05;
    m_h = 10.0; m_rw = 0.1; m_Ct = 0.005; m_mu = 5.0; m_B = 1.2;
    m_alpha = 0.1; m_C_phi = 0.0001;

    // 抹除项目挂载状态及所有路径缓存
    m_hasLoaded = false;
    m_projectPath.clear();
    m_projectFilePath.clear();

    // 重新实例化一个空的 JSON 树对象，释放旧有数据
    m_fullProjectData = QJsonObject();
}
