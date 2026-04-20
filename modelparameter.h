/*
 * 文件名: modelparameter.h
 * 文件作用与功能:
 * 1. 本代码文件作为软件系统的【第一级参数存储中心】（全局基础物性单例类）。
 * 2. 负责接收来自“新建项目向导” (NewProjectDialog) 录入的客观地质、流体物性和井身结构等基础参数。
 * 3. 保证这些基础参数在“理论模型界面”和“自动拟合界面”之间切换时绝对不变，实现“一处设置，全局通用”。
 * 4. 负责项目工程文件 (.pwt) 以及附带分析数据 (JSON 图表数据、拟合历史数据) 的完整加载与持久化保存。
 */

#ifndef MODELPARAMETER_H
#define MODELPARAMETER_H

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>

class ModelParameter : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取全局参数单例对象的唯一入口
     * @return ModelParameter* 唯一的实例指针
     */
    static ModelParameter* instance();

    // ========================================================================
    // 项目工程文件与路径管理
    // ========================================================================

    /**
     * @brief 加载项目工程文件 (.pwt) 到内存中
     * @param filePath 工程文件的绝对路径
     * @return 成功加载并解析返回 true，否则返回 false
     */
    bool loadProject(const QString& filePath);

    /**
     * @brief 将内存中所有的参数状态与分析数据保存至当前挂载的 .pwt 工程文件中
     * @return 成功写入磁盘返回 true，否则返回 false
     */
    bool saveProject();

    /**
     * @brief 关闭当前项目，释放相关内存并重置所有变量为出厂默认值
     */
    void closeProject();

    /**
     * @brief 获取当前打开的项目工程文件(.pwt)的全路径
     */
    QString getProjectFilePath() const { return m_projectFilePath; }

    /**
     * @brief 获取当前打开项目的所在目录路径
     */
    QString getProjectPath() const { return m_projectPath; }

    /**
     * @brief 检查当前系统是否已经挂载了有效的工程项目
     */
    bool hasLoadedProject() const { return m_hasLoaded; }

    // ========================================================================
    // 第一级全局物理参数管理 (Global Properties)
    // ========================================================================

    /**
     * @brief 批量设置主要物理与流体参数 (由新建项目向导 NewProjectDialog 调用)
     * @param phi 孔隙度
     * @param h 有效厚度 (m)
     * @param mu 流体粘度 (mPa·s)
     * @param B 体积系数
     * @param Ct 综合压缩系数 (MPa^-1)
     * @param q 测试产量 (m^3/d)
     * @param rw 井筒半径 (m)
     * @param L 水平井长度 (m)
     * @param nf 裂缝条数
     * @param path 项目工程文件的保存路径
     */
    void setParameters(double phi, double h, double mu, double B, double Ct,
                       double q, double rw, double L, double nf, const QString& path);

    /**
     * @brief 单独设置变井储时间参数 (Fair/Hegeman模型专用)
     * @param v 变井储时间参数 alpha (h)
     */
    void setAlpha(double v);

    /**
     * @brief 单独设置变井储压力参数 (Fair/Hegeman模型专用)
     * @param v 变井储压力参数 C_phi (MPa)
     */
    void setCPhi(double v);

    // --- 全局基础物理参数获取接口 ---
    double getPhi() const { return m_phi; }   // 获取孔隙度
    double getH() const { return m_h; }       // 获取有效厚度
    double getMu() const { return m_mu; }     // 获取流体粘度
    double getB() const { return m_B; }       // 获取体积系数
    double getCt() const { return m_Ct; }     // 获取综合压缩系数
    double getQ() const { return m_q; }       // 获取测试产量
    double getRw() const { return m_rw; }     // 获取井筒半径

    double getL() const { return m_L; }       // 获取水平井总长
    double getNf() const { return m_nf; }     // 获取裂缝条数

    double getAlpha() const { return m_alpha; } // 获取变井储时间参数
    double getCPhi() const { return m_C_phi; }  // 获取变井储压力参数

    // ========================================================================
    // 扩展分析数据读写管理 (拟合、图表、数据表)
    // ========================================================================

    /**
     * @brief 将自动拟合的历史数据存储至工程内存缓存
     * @param fittingData JSON 格式的拟合数据字典
     */
    void saveFittingResult(const QJsonObject& fittingData);

    /**
     * @brief 提取当前工程中保存的拟合历史数据
     */
    QJsonObject getFittingResult() const;

    /**
     * @brief 将图版分析界面保存的分析图表数据落盘为 JSON 文件
     * @param plots 包含了绘图点集的 JSON 数组
     */
    void savePlottingData(const QJsonArray& plots);

    /**
     * @brief 提取落盘保存的图表 JSON 数据
     */
    QJsonArray getPlottingData() const;

    /**
     * @brief 将数据分析表落盘为 JSON 文件
     * @param tableData 包含表格矩阵信息的 JSON 数组
     */
    void saveTableData(const QJsonArray& tableData);

    /**
     * @brief 提取落盘保存的表格 JSON 数据
     */
    QJsonArray getTableData() const;

    /**
     * @brief 重置整个工程的参数和状态到代码出厂默认值（并清空挂载状态）
     */
    void resetAllData();

private:
    /**
     * @brief 私有构造函数，阻断外部随意实例化，保障单例模式
     */
    explicit ModelParameter(QObject* parent = nullptr);

    static ModelParameter* m_instance; // 静态实例指针

    bool m_hasLoaded;             // 是否成功挂载了项目标志位
    QString m_projectPath;        // 项目所在文件夹路径
    QString m_projectFilePath;    // 工程文件 .pwt 的绝对路径
    QJsonObject m_fullProjectData;// 缓存完整的 JSON 序列化对象，用于维护不需要经常变动的原始节点

    // --- 第一级基础物理参数存储变量 ---
    double m_phi;   // 孔隙度
    double m_h;     // 有效厚度
    double m_mu;    // 粘度
    double m_B;     // 体积系数
    double m_Ct;    // 综合压缩系数
    double m_q;     // 测试产量
    double m_rw;    // 井筒半径
    double m_L;     // 水平井长度
    double m_nf;    // 裂缝条数
    double m_alpha; // 变井储时间参数
    double m_C_phi; // 变井储压力参数

    // --- 内部辅助路径获取器 ---
    /**
     * @brief 获取绘图数据 JSON 文件的绝对路径
     */
    QString getPlottingDataFilePath() const;

    /**
     * @brief 获取表格数据 JSON 文件的绝对路径
     */
    QString getTableDataFilePath() const;
};

#endif // MODELPARAMETER_H
