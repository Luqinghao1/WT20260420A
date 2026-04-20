/*
 * 文件名: mainwindow.h
 * 文件作用: 试井解释软件的主窗口类头文件
 * 功能描述:
 * 1. 定义主窗口界面框架，并声明各个子功能模块（如项目、数据、图表、模型等）的指针。
 * 2. 声明主窗口与各个子模块页面之间的数据交互接口和槽函数。
 * 3. 拦截窗口关闭事件(closeEvent)，实现统一的全局退出确认与数据保存提示逻辑。
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include <QStandardItemModel>
#include <QCloseEvent>
#include <QSettings>
#include <QColor>
#include "modelmanager.h"

// 前置声明各个功能页面的类，避免包含过多头文件导致编译缓慢
class NavBtn;
class WT_ProjectWidget;
class WT_DataWidget;
class WT_PlottingWidget;
class FittingPage;
class SettingsWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // 构造函数，初始化主窗口
    explicit MainWindow(QWidget *parent = nullptr);
    // 析构函数，释放界面资源
    ~MainWindow();

    // 初始化主程序的UI与逻辑
    void init();

    // 以下为各个功能子界面的初始化函数声明
    void initProjectForm();         // 初始化项目界面
    void initDataEditorForm();      // 初始化数据编辑界面
    void initModelForm();           // 初始化模型界面
    void initPlottingForm();        // 初始化图表绘制界面
    void initFittingForm();         // 初始化单图拟合界面
    void initPredictionForm();      // 初始化多数据预测界面

protected:
    /**
     * @brief 拦截主界面的全局关闭事件
     * @param event 关闭事件句柄，用于控制是否真正关闭窗口
     * 功能：弹出保存并退出、直接退出、取消的确认框，防止用户丢失数据
     */
    void closeEvent(QCloseEvent *event) override;

private slots:
    // 当项目被新建或打开时触发的槽函数
    void onProjectOpened(bool isNew);
    // 当项目被关闭时触发的槽函数
    void onProjectClosed();
    // 当有新数据文件被加载时触发的槽函数
    void onFileLoaded(const QString& filePath, const QString& fileType);
    // 图表分析完成时的回调槽函数
    void onPlotAnalysisCompleted(const QString &analysisType, const QMap<QString, double> &results);
    // 当数据准备好进行绘图时触发
    void onDataReadyForPlotting();
    // 手动将数据编辑器中的数据传输到图表界面的槽函数
    void onTransferDataToPlotting();
    // 数据编辑界面的数据发生变动时触发
    void onDataEditorDataChanged();
    // 预览导出的文件时触发
    void onViewExportedFile(const QString& filePath);
    // 系统设置发生更改时触发
    void onSystemSettingsChanged();
    // 性能设置发生更改时触发
    void onPerformanceSettingsChanged();
    // 模型计算完成时的回调槽函数
    void onModelCalculationCompleted(const QString &analysisType, const QMap<QString, double> &results);
    // 拟合进度更新时的槽函数，用于更新状态栏
    void onFittingProgressChanged(int progress);

private:
    Ui::MainWindow *ui; // 界面UI指针

    // 各个子功能页面的实例指针
    WT_ProjectWidget* m_ProjectWidget;      // 项目管理页面
    WT_DataWidget* m_DataEditorWidget;      // 数据处理页面
    ModelManager* m_ModelManager;           // 模型管理模块
    WT_PlottingWidget* m_PlottingWidget;    // 图表绘制页面
    FittingPage* m_FittingPage;             // 数据拟合页面
    SettingsWidget* m_SettingsWidget;       // 系统设置页面

    // 导航按钮映射表，用于快速根据名称查找对应的导航按钮
    QMap<QString, NavBtn*> m_NavBtnMap;
    // 界面右下角显示时间的定时器
    QTimer m_timer;
    QTimer m_autoSaveTimer;

    // 状态标志位
    bool m_hasValidData = false;     // 当前是否包含有效数据
    bool m_isProjectLoaded = false;  // 当前是否已经加载了工程项目

    // 内部私有功能函数
    void transferDataFromEditorToPlotting(); // 核心：将编辑器数据同步至图表区
    void updateNavigationState();            // 更新左侧导航栏的选中状态
    void transferDataToFitting();            // 将数据同步至拟合界面
    QColor readColorSetting(const QString& key, const QColor& fallback) const;
    void applyMainWindowBackgrounds();       // 用代码统一设置主界面背景色

    // 获取当前的数据模型
    QStandardItemModel* getDataEditorModel() const;
    // 获取当前活动的文件名称
    QString getCurrentFileName() const;
    // 判断是否已经加载了数据
    bool hasDataLoaded();
    // 获取全局统一的弹窗样式字符串
    QString getMessageBoxStyle() const;
    int getCurrentThemeIndex() const;
};

#endif // MAINWINDOW_H
