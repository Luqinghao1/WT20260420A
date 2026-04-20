/*
 * 文件名: newprojectdialog.h
 * 文件作用与功能:
 * 1. 本代码文件定义了用于创建新建项目的交互界面（NewProjectDialog）及内部传递数据结构。
 * 2. 负责拦截、校验用户录入的初始井况与流体物性数据，并执行英/公制单位切换。
 * 3. 负责向硬盘写入项目引导基础 .pwt JSON 文件并生成目录结构。
 * 4. 【核心架构】：收集的所有全局数据在建立完成时强制打入全局 ModelParameter 单例，奠定 Level 1 的不可变初值底座。
 */

#ifndef NEWPROJECTDIALOG_H
#define NEWPROJECTDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QDateTime>

namespace Ui {
class NewProjectDialog;
}

// 项目单位制枚举
enum class ProjectUnitType {
    Metric_SI = 0, // 公制
    Field_Unit = 1 // 英制/矿场单位
};

// 项目数据结构体，用于封装页面采集到的所有输入项
struct ProjectData {
    // --- 基本信息 ---
    QString projectName;    // 项目名
    QString oilFieldName;   // 油田名
    QString wellName;       // 井名
    QString engineer;       // 工程师
    QString comments;       // 备注
    QString projectPath;    // 项目保存的根目录
    QString fullFilePath;   // 生成的具体项目文件全路径 (.pwt)

    // --- 第二页：井参数 / 油藏参数 ---
    int testType;           // 0: 压力降落, 1: 压力恢复

    double horizLength;     // 水平井长度 L (m/ft)
    double fracCount;       // 裂缝条数 nf

    double productionRate;  // 测试产量 q
    double porosity;        // 孔隙度 phi
    double thickness;       // 有效厚度 h
    double wellRadius;      // 井筒半径 rw

    // --- 第三页：PVT 参数 ---
    QDateTime testDate;     // 测试日期
    double compressibility; // 综合压缩系数 Ct
    double viscosity;       // 粘度 mu
    double volumeFactor;    // 体积系数 B

    // 界面选择的单位制
    ProjectUnitType currentUnitSystem;
};

class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数，建立UI层、装填预置选项等
     * @param parent 父窗口指针
     */
    explicit NewProjectDialog(QWidget *parent = nullptr);

    /**
     * @brief 析构释放内存
     */
    ~NewProjectDialog();

    /**
     * @brief 返还组装成型的工程结构数据，供主页面读取
     * @return ProjectData 项目数据结构体
     */
    ProjectData getProjectData() const;

private slots:
    /**
     * @brief 响应用户点击路径浏览按钮并弹出目录选择器
     */
    void on_btnBrowse_clicked();

    /**
     * @brief 响应用户拉取单位制下拉框改变单位动作
     * @param index 当前下拉框选中的索引
     */
    void on_comboUnits_currentIndexChanged(int index);

    /**
     * @brief 响应确认建项动作，实现数据打包验证及落盘
     */
    void on_btnOk_clicked();

    /**
     * @brief 响应取消动作，关闭对话框
     */
    void on_btnCancel_clicked();

private:
    Ui::NewProjectDialog *ui;
    ProjectData m_projectData;

    /**
     * @brief 强制分配默认工程空白模板初值，全面对齐底层算法默认值
     */
    void initDefaultValues();

    /**
     * @brief 更迭界面 UI 上的带单位标签文字
     * @param unit 目标单位制
     */
    void updateUnitLabels(ProjectUnitType unit);

    /**
     * @brief 执行底层数值本身的公/英制公式汇率转换
     * @param from 转换前单位制
     * @param to 转换后单位制
     */
    void convertValues(ProjectUnitType from, ProjectUnitType to);

    /**
     * @brief 利用用户填写信息，建立文件夹并组装出底层数据承载包，注入至全局内存中
     * @return bool 创建成功返回 true，否则返回 false
     */
    bool createProjectStructure();

    /**
     * @brief 加载美化样式表
     */
    void loadModernStyle();

    /**
     * @brief 创建基础属性并形成 json 后落盘写入硬盘
     * @param filePath 写入的绝对路径
     * @param data 要写入的项目数据结构
     */
    void saveProjectFile(const QString &filePath, const ProjectData &data);
};

#endif // NEWPROJECTDIALOG_H
