/*
 * 文件名: wt_projectwidget.h
 * 文件作用: 项目管理主界面类头文件
 * 功能升级: 单页双列全参数表单展示
 */

#ifndef WT_PROJECTWIDGET_H
#define WT_PROJECTWIDGET_H

#include <QWidget>
#include <QString>
#include <QStringList>

class QListWidgetItem;
class QStackedWidget;
class QLabel;
class QLineEdit;
class QComboBox;
class QDateEdit;   // 【修改点】：引入 QDateEdit 替代 QDateTimeEdit
class QPushButton;

namespace Ui {
class WT_ProjectWidget;
}

// 表单模式状态机
enum FormMode {
    Mode_None = 0,    // 无状态
    Mode_New,         // 新建模式（空白表单，底端为创建按钮）
    Mode_Preview,     // 预览模式（填充数据，底端为加载按钮）
    Mode_Opened       // 已打开模式（填充数据，底端为保存更改按钮）
};

class WT_ProjectWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WT_ProjectWidget(QWidget *parent = nullptr);
    ~WT_ProjectWidget();
    bool tryAutoLoadLastProject();

signals:
    void projectOpened(bool isNew);
    void projectClosed();
    void fileLoaded(const QString& filePath, const QString& fileType);

private slots:
    void onNewProjectClicked();
    void onOpenProjectClicked();
    void onCloseProjectClicked();
    void onExitClicked();

    void onRecentProjectClicked(QListWidgetItem *item);
    void onRecentProjectDoubleClicked(QListWidgetItem *item);

    // 右侧表单的操作事件
    void onBrowsePathClicked();
    void onActionButtonClicked();

private:
    Ui::WT_ProjectWidget *ui;

    bool m_isProjectOpen;
    QString m_currentProjectFilePath;
    QString m_previewFilePath;
    FormMode m_currentMode;

    QStringList m_recentProjects;
    const int MAX_RECENT_PROJECTS = 10;

    // 右侧动态面板及核心控件
    QStackedWidget* m_rightStackedWidget;
    QWidget* m_workflowPage;     // 静态工作流展示
    QWidget* m_formPage;         // 动态项目表单

    // 表单标题与大操作按钮
    QLabel* m_formTitleLabel;
    QPushButton* m_btnAction;

    // --- 左列：基本信息控件 ---
    QLineEdit* m_editProjName;
    QLineEdit* m_editPath;
    QPushButton* m_btnBrowse;
    QLineEdit* m_editOilField;
    QLineEdit* m_editWell;
    QLineEdit* m_editEngineer;
    QDateEdit* m_dateEdit;       // 【修改点】：日期编辑器
    QComboBox* m_comboUnit;
    QLineEdit* m_editComments;

    // --- 右列：油藏与井参数控件 ---
    QLineEdit* m_editRate;
    QLineEdit* m_editThickness;
    QLineEdit* m_editPorosity;
    QLineEdit* m_editRw;
    QLineEdit* m_editL;
    QLineEdit* m_editNf;

    // --- 右列：PVT 参数控件 ---
    QLineEdit* m_editCt;
    QLineEdit* m_editMu;
    QLineEdit* m_editB;

    // 内部方法
    void init();
    void initRightPanel();
    void setFormMode(FormMode mode, const QString& filePath = "");

    // 表单数据流转
    void clearFormFields();
    void loadFormFromJson(const QString& filePath);
    bool saveFormToJson(const QString& filePath);

    void setProjectState(bool isOpen, const QString& filePath);
    bool saveCurrentProject();
    void closeProjectInternal();

    void loadRecentProjects();
    void updateRecentProjectsList(const QString& newProjectPath);

    QString getMessageBoxStyle() const;
};

#endif // WT_PROJECTWIDGET_H
