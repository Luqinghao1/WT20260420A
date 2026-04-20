/*
 * settingswidget.h
 * 文件作用：系统设置窗口的头文件
 * 功能描述：
 * 1. 定义 SettingsWidget 类，继承自 QWidget
 * 2. 声明工作区/启动、路径等设置模块的 UI 交互与数据持久化方法
 * 3. 提供左侧导航区与右侧内容区颜色的实时配置接口
 */

#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QWidget>
#include <QSettings>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QColor>

namespace Ui {
class SettingsWidget;
}

class SettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWidget(QWidget *parent = nullptr);
    ~SettingsWidget();

    // --- 公共接口：获取当前配置值 ---

    // 路径配置
    QString getDataPath() const;
    QString getReportPath() const;
    QString getBackupPath() const;

    // 系统配置
    int getAutoSaveInterval() const;
    bool isBackupEnabled() const;

    // 界面配色
    QColor getNavBackgroundColor() const;
    QColor getContentBackgroundColor() const;

signals:
    // 配置变更信号
    void settingsChanged();                             // 通用变更信号
    void appearanceChanged(const QColor& navColor,
                           const QColor& contentColor); // 颜色实时变更

private slots:
    // 侧边导航栏切换
    void on_navTupleList_currentRowChanged(int currentRow);

    // 路径浏览按钮
    void on_btnBrowseData_clicked();
    void on_btnBrowseReport_clicked();
    void on_btnBrowseBackup_clicked();

    // 底部操作按钮
    void on_btnRestoreDefaults_clicked(); // 恢复默认
    void on_btnApply_clicked();           // 应用保存
    void on_btnCancel_clicked();          // 取消/关闭

    // 监听部分控件变化（用于激活"应用"按钮状态等）
    void onSettingModified();

    // 颜色选择入口
    void onPickNavColor();
    void onPickContentColor();

private:
    Ui::SettingsWidget *ui;
    QSettings *m_settings;
    bool m_isModified; // 记录是否有未保存的修改
    QColor m_navColor;
    QColor m_contentColor;

    // --- 核心逻辑方法 ---

    // 初始化界面控件（设置下拉框选项、默认值等）
    void initInterface();

    // 加载所有设置到 UI
    void loadSettings();

    // 保存 UI 设置到配置文件
    void applySettings();

    // 恢复所有设置到出厂默认值
    void restoreDefaults();

    // 路径有效性验证
    bool validatePaths();

    // 辅助：获取默认路径
    QString getDefaultPath(const QString &subDir);

    // 辅助：创建目录
    void ensureDirExists(const QString &path);

    // 颜色按钮外观同步
    void updateColorButton(QPushButton* button, const QColor& color);

    // 广播颜色变更
    void emitAppearanceChanged();

    // 常量定义
    static const int DEFAULT_AUTO_SAVE;
    static const int DEFAULT_MAX_BACKUPS;
};

#endif // SETTINGSWIDGET_H
