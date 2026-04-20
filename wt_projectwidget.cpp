/**
 * @file wt_projectwidget.cpp
 * @brief 试井软件 - 项目管理核心界面实现文件
 * @details
 * 【本代码文件的作用与功能】
 * 1. 负责软件工程项目的全局管理（新建、打开、保存、关闭项目）。
 * 2. 渲染顶部 4 个全局标准工具按钮，通过引入 Standard_ToolButton 实现统一。
 * 3. 动态管理“最近打开项目”列表，支持双击加载。
 * 4. 绘制并管理右侧动态表单，用于输入油藏与井筒参数。
 * 5. 采用 Standard_MessageBox 实现现代化弹窗，并精简了关闭逻辑。
 */

#include "wt_projectwidget.h"
#include "ui_wt_projectwidget.h"
#include "modelparameter.h"
#include "standard_messagebox.h"
#include "standard_toolbutton.h"

#include <QDebug>
#include <QFileDialog>
#include <QApplication>
#include <QFileInfo>
#include <QDateTime>
#include <QSettings>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleValidator>
#include <QListView>
#include <QCalendarWidget>
#include <QPainter>
#include <QIcon>
#include <QPixmap>
#include <QString>

/**
 * @brief 纯代码绘制项目界面的 4 个彩色矢量图标
 */
static QIcon createProjectVectorIcon(int type) {
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    if (type == 0) {
        p.setPen(QPen(QColor("#1C3C6A"), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(QColor("#FFFFFF"));
        QPolygon doc; doc << QPoint(14, 8) << QPoint(36, 8) << QPoint(46, 18) << QPoint(46, 52) << QPoint(14, 52); p.drawPolygon(doc);
        p.drawLine(36, 8, 36, 18); p.drawLine(36, 18, 46, 18);
        p.setPen(Qt::NoPen); p.setBrush(QColor("#52C41A")); p.drawEllipse(30, 34, 22, 22);
        p.setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap)); p.drawLine(41, 39, 41, 51); p.drawLine(35, 45, 47, 45);
    } else if (type == 1) {
        p.setPen(QPen(QColor("#D48806"), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(QColor("#FFD666"));
        QPolygon backFolder; backFolder << QPoint(10, 16) << QPoint(26, 16) << QPoint(32, 22) << QPoint(54, 22) << QPoint(54, 50) << QPoint(10, 50); p.drawPolygon(backFolder);
        p.setBrush(QColor("#FFEC3D"));
        QPolygon frontFolder; frontFolder << QPoint(8, 50) << QPoint(16, 28) << QPoint(56, 28) << QPoint(48, 50); p.drawPolygon(frontFolder);
        p.setPen(QPen(QColor("#1890FF"), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(32, 42, 32, 20); p.drawLine(24, 28, 32, 20); p.drawLine(40, 28, 32, 20);
    } else if (type == 2) {
        p.setPen(QPen(QColor("#1C3C6A"), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(QColor("#FFFFFF"));
        QPolygon doc; doc << QPoint(14, 8) << QPoint(36, 8) << QPoint(46, 18) << QPoint(46, 52) << QPoint(14, 52); p.drawPolygon(doc);
        p.drawLine(36, 8, 36, 18); p.drawLine(36, 18, 46, 18);
        p.setPen(Qt::NoPen); p.setBrush(QColor("#FF4D4F")); p.drawEllipse(30, 34, 22, 22);
        p.setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap)); p.drawLine(36, 40, 46, 50); p.drawLine(46, 40, 36, 50);
    } else if (type == 3) {
        p.setPen(QPen(QColor("#FF4D4F"), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawArc(16, 16, 32, 32, 115 * 16, 310 * 16); p.drawLine(32, 10, 32, 28);
    }
    return QIcon(pixmap);
}

// ==============================================================================
// 构造、初始化与 UI 排版
// ==============================================================================

/**
 * @brief 构造函数
 */
WT_ProjectWidget::WT_ProjectWidget(QWidget *parent) :
    QWidget(parent), ui(new Ui::WT_ProjectWidget), m_isProjectOpen(false), m_currentMode(Mode_None)
{
    ui->setupUi(this);
    init();
}

/**
 * @brief 析构函数
 */
WT_ProjectWidget::~WT_ProjectWidget() {
    delete ui;
}

/**
 * @brief 核心初始化函数
 * @details 采用 Standard_ToolButton 重新装载按钮并绑定交互。
 */
void WT_ProjectWidget::init()
{
    this->setStyleSheet("background-color: transparent;");

    // 清除原有旧按钮
    QLayoutItem *child;
    while ((child = ui->horizontalLayout_buttons->takeAt(0)) != nullptr) {
        if(child->widget()) delete child->widget();
        delete child;
    }

    // 重新实例化为标准组件
    ui->btnNew  = new Standard_ToolButton("新建项目", createProjectVectorIcon(0), this);
    ui->btnOpen = new Standard_ToolButton("打开项目", createProjectVectorIcon(1), this);
    ui->btnClose= new Standard_ToolButton("关闭项目", createProjectVectorIcon(2), this);
    ui->btnExit = new Standard_ToolButton("退出系统", createProjectVectorIcon(3), this);

    ui->horizontalLayout_buttons->addWidget(ui->btnNew);
    ui->horizontalLayout_buttons->addWidget(ui->btnOpen);
    ui->horizontalLayout_buttons->addWidget(ui->btnClose);
    ui->horizontalLayout_buttons->addWidget(ui->btnExit);
    ui->horizontalLayout_buttons->addStretch();

    connect(ui->btnNew,   &Standard_ToolButton::clicked, this, &WT_ProjectWidget::onNewProjectClicked);
    connect(ui->btnOpen,  &Standard_ToolButton::clicked, this, &WT_ProjectWidget::onOpenProjectClicked);
    connect(ui->btnClose, &Standard_ToolButton::clicked, this, &WT_ProjectWidget::onCloseProjectClicked);
    connect(ui->btnExit,  &Standard_ToolButton::clicked, this, &WT_ProjectWidget::onExitClicked);

    ui->label_recent->setStyleSheet("color: #333333; font-size: 16px; font-weight: bold; margin-top: 15px; margin-bottom: 8px;");
    ui->listWidget_recent->setStyleSheet(
        "QListWidget { background-color: #ffffff; border: 1px solid #D0D0D0; border-radius: 5px; outline: none; color: #333333; }"
        "QListWidget::item { padding: 12px; border-bottom: 1px solid #F0F0F0; color: #333333; line-height: 1.5; }"
        "QListWidget::item:hover { background-color: #E6F7FF; color: #1C3C6A; }"
        "QListWidget::item:selected { background-color: #BAE7FF; color: #000000; font-weight: bold; }"
        );

    connect(ui->listWidget_recent, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onRecentProjectClicked(QListWidgetItem*)));
    connect(ui->listWidget_recent, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onRecentProjectDoubleClicked(QListWidgetItem*)));

    initRightPanel();
    loadRecentProjects();
}

/**
 * @brief 初始化右侧的动态面板区域
 */
void WT_ProjectWidget::initRightPanel()
{
    m_rightStackedWidget = new QStackedWidget(ui->widgetRight);
    QVBoxLayout* mainRightLayout = new QVBoxLayout(ui->widgetRight);
    mainRightLayout->setContentsMargins(0, 0, 0, 0); mainRightLayout->addWidget(m_rightStackedWidget);
    m_workflowPage = new QWidget(); m_formPage = new QWidget();
    m_rightStackedWidget->addWidget(m_workflowPage); m_rightStackedWidget->addWidget(m_formPage);

    // ================== 页面 0: 静态引导页 ==================
    QVBoxLayout* workflowLayout = new QVBoxLayout(m_workflowPage); workflowLayout->setContentsMargins(50, 40, 50, 40); workflowLayout->setSpacing(15);
    QLabel* titleLabel = new QLabel("标准事件分析工作流"); titleLabel->setStyleSheet("font-size: 22px; font-weight: bold; color: #1C3C6A;"); titleLabel->setAlignment(Qt::AlignCenter); workflowLayout->addWidget(titleLabel);

    struct FlowStep { QString title; QString desc; };
    QList<FlowStep> steps = {
        {"1. 项目创建与加载", "在“项目”页面新建工程或打开历史 .pwt 项目。"},
        {"2. 数据导入与质量整理", "进入“数据”页面导入文件，并完成单位处理、滤波取样与异常检查。"},
        {"3. 图表构建与诊断查看", "进入“图表”页面绘制压力/导数等曲线并检查关键特征。"},
        {"4. 模型选择与参数计算", "进入“模型”页面选择理论模型并执行参数求解。"},
        {"5. 历史拟合与多数据对比", "在“拟合”和“多数据”页面完成单井拟合与多组数据对比分析。"},
        {"6. 结果导出与报告归档", "在拟合页面导出曲线数据与试井解释报告，完成成果归档。"}
    };
    for (int i = 0; i < steps.size(); ++i) {
        QFrame* stepFrame = new QFrame(); stepFrame->setStyleSheet("QFrame { background-color: #FFFFFF; border: 1px solid #E0E0E0; border-radius: 8px; } QFrame:hover { border: 1px solid #5C9DFF; background-color: #F8FBFF; }");
        QVBoxLayout* frameLayout = new QVBoxLayout(stepFrame);
        QLabel* stepTitle = new QLabel(steps[i].title); stepTitle->setStyleSheet("font-size: 15px; font-weight: bold; color: #333333; border: none; background: transparent;");
        QLabel* stepDesc = new QLabel(steps[i].desc); stepDesc->setStyleSheet("font-size: 12px; color: #555555; border: none; background: transparent;");
        frameLayout->addWidget(stepTitle); frameLayout->addWidget(stepDesc); workflowLayout->addWidget(stepFrame);
    }
    workflowLayout->addStretch();

    // ================== 页面 1: 动态表单页 ==================
    QVBoxLayout* formLayout = new QVBoxLayout(m_formPage); formLayout->setContentsMargins(40, 20, 40, 20); formLayout->setSpacing(15);
    m_formTitleLabel = new QLabel("新建项目"); m_formTitleLabel->setStyleSheet("font-size: 22px; font-weight: bold; color: #1C3C6A; margin-bottom: 5px;"); m_formTitleLabel->setAlignment(Qt::AlignCenter); formLayout->addWidget(m_formTitleLabel);

    QFrame* contentFrame = new QFrame(); contentFrame->setObjectName("contentFrame");
    contentFrame->setStyleSheet("QFrame#contentFrame { border: 1px solid #D0D0D0; border-radius: 6px; background-color: #FFFFFF; } QLineEdit { border: 1px solid #B0B0B0; border-radius: 4px; padding: 4px; font-size: 13px; color: #333333; } QLineEdit:focus { border: 1px solid #5C9DFF; }");
    QHBoxLayout* hLayout = new QHBoxLayout(contentFrame); hLayout->setContentsMargins(20, 20, 20, 20); hLayout->setSpacing(30);

    auto createLabel = [](const QString& text) { QLabel* l = new QLabel(text); l->setStyleSheet("font-weight: bold; color: #555555; font-size: 13px; border: none;"); return l; };
    auto createTitle = [](const QString& text) { QLabel* l = new QLabel(text); l->setStyleSheet("font-weight: bold; color: #1C3C6A; font-size: 15px; border: none; padding-top: 10px; padding-bottom: 5px;"); return l; };
    QDoubleValidator* doubleVal = new QDoubleValidator(this);

    // 左侧基础配置
    QWidget* leftWidget = new QWidget(); QGridLayout* leftGrid = new QGridLayout(leftWidget); leftGrid->setContentsMargins(0, 0, 0, 0); leftGrid->setVerticalSpacing(18);
    m_editProjName = new QLineEdit(); m_editPath = new QLineEdit(); m_editPath->setReadOnly(true); m_editPath->setStyleSheet("background:#F5F5F5; color:#666;");
    m_btnBrowse = new QPushButton("..."); m_btnBrowse->setFixedWidth(35); m_btnBrowse->setStyleSheet("border: 1px solid #B0B0B0; border-radius: 4px; background: #E0E0E0;");
    connect(m_btnBrowse, &QPushButton::clicked, this, &WT_ProjectWidget::onBrowsePathClicked);
    QHBoxLayout* pathLayout = new QHBoxLayout(); pathLayout->setContentsMargins(0,0,0,0); pathLayout->addWidget(m_editPath); pathLayout->addWidget(m_btnBrowse);

    m_editOilField = new QLineEdit(); m_editWell = new QLineEdit(); m_editEngineer = new QLineEdit();
    m_dateEdit = new QDateEdit(QDate::currentDate()); m_dateEdit->setDisplayFormat("yyyy-MM-dd"); m_dateEdit->setCalendarPopup(true); m_dateEdit->setStyleSheet("QDateEdit { border: 1px solid #B0B0B0; border-radius: 4px; padding: 4px; font-size: 13px; }");
    QCalendarWidget* calendar = new QCalendarWidget(this); calendar->setStyleSheet("QCalendarWidget QWidget { background-color: #FFFFFF; color: #333333; font-size: 12px; } QCalendarWidget QAbstractItemView:enabled { selection-background-color: #5C9DFF; selection-color: #FFFFFF; } QCalendarWidget QToolButton { color: #333333; background-color: transparent; border: none; font-weight: bold; } "); m_dateEdit->setCalendarWidget(calendar);
    m_comboUnit = new QComboBox(); m_comboUnit->addItems({"公制 (Metric / SI)", "英制 (Field Unit)"}); m_comboUnit->setStyleSheet("QComboBox { border: 1px solid #B0B0B0; border-radius: 4px; padding: 4px; font-size: 13px; }");
    QListView* comboListView = new QListView(m_comboUnit); comboListView->setStyleSheet("QListView { background-color: #FFFFFF; color: #333333; font-size: 13px; border: 1px solid #B0B0B0; } QListView::item { padding: 6px; } QListView::item:hover { background-color: #E6F7FF; color: #1C3C6A; } QListView::item:selected { background-color: #5C9DFF; color: #FFFFFF; }"); m_comboUnit->setView(comboListView);
    m_editComments = new QLineEdit();

    leftGrid->addWidget(createTitle("基础信息配置"), 0, 0, 1, 2); leftGrid->addWidget(createLabel("项目名称:"), 1, 0); leftGrid->addWidget(m_editProjName, 1, 1);
    leftGrid->addWidget(createLabel("存放路径:"), 2, 0); leftGrid->addLayout(pathLayout, 2, 1); leftGrid->addWidget(createLabel("油田名称:"), 3, 0); leftGrid->addWidget(m_editOilField, 3, 1);
    leftGrid->addWidget(createLabel("井号:"), 4, 0); leftGrid->addWidget(m_editWell, 4, 1); leftGrid->addWidget(createLabel("测试日期:"), 5, 0); leftGrid->addWidget(m_dateEdit, 5, 1);
    leftGrid->addWidget(createLabel("工程师:"), 6, 0); leftGrid->addWidget(m_editEngineer, 6, 1); leftGrid->addWidget(createLabel("单位系统:"), 7, 0); leftGrid->addWidget(m_comboUnit, 7, 1);
    leftGrid->addWidget(createLabel("备注说明:"), 8, 0); leftGrid->addWidget(m_editComments, 8, 1); leftGrid->setRowStretch(9, 1);

    QFrame* vLine = new QFrame(); vLine->setFrameShape(QFrame::VLine); vLine->setStyleSheet("border: 1px solid #E0E0E0;");

    // 右侧物理参数
    QWidget* rightWidget = new QWidget(); QGridLayout* rightGrid = new QGridLayout(rightWidget); rightGrid->setContentsMargins(0, 0, 0, 0); rightGrid->setVerticalSpacing(18);
    m_editRate = new QLineEdit(); m_editRate->setValidator(doubleVal); m_editThickness = new QLineEdit(); m_editThickness->setValidator(doubleVal); m_editPorosity = new QLineEdit();  m_editPorosity->setValidator(doubleVal);
    m_editRw = new QLineEdit(); m_editRw->setValidator(doubleVal); m_editL = new QLineEdit(); m_editL->setValidator(doubleVal); m_editNf = new QLineEdit(); m_editNf->setValidator(doubleVal);
    m_editCt = new QLineEdit(); m_editCt->setValidator(doubleVal); m_editMu = new QLineEdit(); m_editMu->setValidator(doubleVal); m_editB = new QLineEdit(); m_editB->setValidator(doubleVal);

    int rRow = 0; rightGrid->addWidget(createTitle("油藏与井筒参数"), rRow++, 0, 1, 2);
    rightGrid->addWidget(createLabel("产/注量 (q):"), rRow, 0); rightGrid->addWidget(m_editRate, rRow++, 1); rightGrid->addWidget(createLabel("有效厚度 (h):"), rRow, 0);  rightGrid->addWidget(m_editThickness, rRow++, 1);
    rightGrid->addWidget(createLabel("孔隙度 (φ):"), rRow, 0); rightGrid->addWidget(m_editPorosity, rRow++, 1); rightGrid->addWidget(createLabel("井筒半径 (rw):"), rRow, 0); rightGrid->addWidget(m_editRw, rRow++, 1);
    rightGrid->addWidget(createLabel("水平井长 (L):"), rRow, 0); rightGrid->addWidget(m_editL, rRow++, 1); rightGrid->addWidget(createLabel("裂缝条数 (nf):"), rRow, 0); rightGrid->addWidget(m_editNf, rRow++, 1);
    rightGrid->addWidget(createTitle("流体 PVT 参数"), rRow++, 0, 1, 2);
    rightGrid->addWidget(createLabel("综合压缩系数:"), rRow, 0); rightGrid->addWidget(m_editCt, rRow++, 1); rightGrid->addWidget(createLabel("流体粘度:"), rRow, 0); rightGrid->addWidget(m_editMu, rRow++, 1);
    rightGrid->addWidget(createLabel("体积系数:"), rRow, 0); rightGrid->addWidget(m_editB, rRow++, 1); rightGrid->setRowStretch(rRow, 1);

    hLayout->addWidget(leftWidget, 1); hLayout->addWidget(vLine); hLayout->addWidget(rightWidget, 1); formLayout->addWidget(contentFrame);

    m_btnAction = new QPushButton("✔ 执行操作");
    m_btnAction->setStyleSheet("QPushButton { background-color: #1C3C6A; color: #FFF; font-size: 16px; font-weight: bold; border-radius: 6px; padding: 12px; margin-top: 10px;} QPushButton:hover { background-color: #2A5A9A; } QPushButton:pressed { background-color: #142B4C; }");
    connect(m_btnAction, &QPushButton::clicked, this, &WT_ProjectWidget::onActionButtonClicked);
    formLayout->addWidget(m_btnAction);

    m_rightStackedWidget->setCurrentWidget(m_workflowPage);
}

// ==============================================================================
// 业务逻辑：表单交互与工程存储
// ==============================================================================

/**
 * @brief 设置表单当前的显示与交互模式
 */
void WT_ProjectWidget::setFormMode(FormMode mode, const QString& filePath) {
    m_currentMode = mode; m_previewFilePath = filePath; m_rightStackedWidget->setCurrentWidget(m_formPage);
    if (mode == Mode_New) {
        m_formTitleLabel->setText("新建项目"); m_btnAction->setText("✔ 创建并进入项目"); m_btnBrowse->setEnabled(true); m_editProjName->setReadOnly(false); clearFormFields();
    } else if (mode == Mode_Preview) {
        m_formTitleLabel->setText("项目预览 (未加载)"); m_btnAction->setText("📂 加载此项目至引擎"); m_btnBrowse->setEnabled(false); m_editProjName->setReadOnly(true); loadFormFromJson(filePath);
    } else if (mode == Mode_Opened) {
        m_formTitleLabel->setText("项目属性设置 (运行中)"); m_btnAction->setText("💾 保存属性更改"); m_btnBrowse->setEnabled(false); m_editProjName->setReadOnly(true);
    }
}

/**
 * @brief 清空表单数据
 */
void WT_ProjectWidget::clearFormFields() {
    m_editProjName->clear(); m_editPath->setText(QCoreApplication::applicationDirPath() + "/Projects"); m_editOilField->clear(); m_editWell->clear(); m_editEngineer->clear(); m_editComments->clear();
    m_dateEdit->setDate(QDate::currentDate()); m_comboUnit->setCurrentIndex(0);
    m_editRate->setText("10.0"); m_editThickness->setText("10.0"); m_editPorosity->setText("0.2"); m_editRw->setText("0.1"); m_editL->setText("1000.0"); m_editNf->setText("10"); m_editCt->setText("0.001"); m_editMu->setText("1.0"); m_editB->setText("1.0");
}

/**
 * @brief 从项目工程 JSON 中加载表单
 */
void WT_ProjectWidget::loadFormFromJson(const QString& filePath) {
    QFile file(filePath); if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll()); file.close(); if (!doc.isObject()) return;
    QJsonObject root = doc.object();
    auto rs = [&root](const QString& k1, const QString& k2) { return root.contains(k1) ? root.value(k1).toString() : root.value(k2).toString(); };
    auto rd = [&root](const QString& k1, const QString& k2) { return root.contains(k1) ? root.value(k1).toDouble() : root.value(k2).toDouble(); };

    m_editProjName->setText(QFileInfo(filePath).baseName()); m_editPath->setText(QFileInfo(filePath).absolutePath()); m_editOilField->setText(rs("oilFieldName", "OilFieldName")); m_editWell->setText(rs("wellName", "WellName")); m_editEngineer->setText(rs("engineer", "Engineer")); m_editComments->setText(rs("comments", "Comments"));
    QString dateStr = rs("testDate", "TestDate"); if(!dateStr.isEmpty()) m_dateEdit->setDate(QDate::fromString(dateStr.left(10), "yyyy-MM-dd"));
    m_comboUnit->setCurrentIndex(root.contains("currentUnitSystem") ? root.value("currentUnitSystem").toInt() : root.value("CurrentUnitSystem").toInt());
    m_editRate->setText(QString::number(rd("productionRate", "ProductionRate"))); m_editThickness->setText(QString::number(rd("thickness", "Thickness"))); m_editPorosity->setText(QString::number(rd("porosity", "Porosity"))); m_editRw->setText(QString::number(rd("wellRadius", "WellRadius"))); m_editL->setText(QString::number(rd("horizLength", "HorizLength"))); m_editNf->setText(QString::number(rd("fracCount", "FracCount"))); m_editCt->setText(QString::number(rd("compressibility", "Compressibility"))); m_editMu->setText(QString::number(rd("viscosity", "Viscosity"))); m_editB->setText(QString::number(rd("volumeFactor", "VolumeFactor")));
}

/**
 * @brief 将表单状态保存回 JSON 文件
 */
bool WT_ProjectWidget::saveFormToJson(const QString& filePath) {
    QJsonObject root;
    root["projectName"] = m_editProjName->text(); root["oilFieldName"] = m_editOilField->text(); root["wellName"] = m_editWell->text(); root["engineer"] = m_editEngineer->text(); root["testDate"] = m_dateEdit->date().toString("yyyy-MM-dd"); root["comments"] = m_editComments->text(); root["currentUnitSystem"] = m_comboUnit->currentIndex();
    root["productionRate"] = m_editRate->text().toDouble(); root["thickness"] = m_editThickness->text().toDouble(); root["porosity"] = m_editPorosity->text().toDouble(); root["wellRadius"] = m_editRw->text().toDouble(); root["horizLength"] = m_editL->text().toDouble(); root["fracCount"] = m_editNf->text().toDouble(); root["compressibility"] = m_editCt->text().toDouble(); root["viscosity"] = m_editMu->text().toDouble(); root["volumeFactor"] = m_editB->text().toDouble(); root["testType"] = 0;

    QJsonDocument doc(root); QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Standard_MessageBox::error(this, "文件保存错误", "无法写入工程文件至指定路径: \n" + filePath);
        return false;
    }
    file.write(doc.toJson()); file.close(); return true;
}

// ==============================================================================
// 槽函数交互逻辑
// ==============================================================================

/**
 * @brief 浏览目录按钮被点击
 */
void WT_ProjectWidget::onBrowsePathClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择工程保存目录", m_editPath->text());
    if(!dir.isEmpty()) m_editPath->setText(dir);
}

/**
 * @brief 表单底部的核心执行操作
 */
void WT_ProjectWidget::onActionButtonClicked() {
    if (m_currentMode == Mode_New) {
        if(m_editProjName->text().trimmed().isEmpty()) {
            Standard_MessageBox::warning(this, "必填项缺失", "创建项目失败，请输入项目名称。");
            return;
        }
        QString fullPath = m_editPath->text() + "/" + m_editProjName->text().trimmed() + ".pwt";
        if (saveFormToJson(fullPath)) {
            ModelParameter::instance()->setParameters(m_editPorosity->text().toDouble(), m_editThickness->text().toDouble(), m_editMu->text().toDouble(), m_editB->text().toDouble(), m_editCt->text().toDouble(), m_editRate->text().toDouble(), m_editRw->text().toDouble(), m_editL->text().toDouble(), m_editNf->text().toDouble(), fullPath);
            setProjectState(true, fullPath); updateRecentProjectsList(fullPath); setFormMode(Mode_Opened, fullPath); emit projectOpened(true);
        }
    } else if (m_currentMode == Mode_Preview) {
        if (ModelParameter::instance()->loadProject(m_previewFilePath)) {
            setProjectState(true, m_previewFilePath); updateRecentProjectsList(m_previewFilePath); setFormMode(Mode_Opened, m_previewFilePath); emit projectOpened(false);
        } else {
            Standard_MessageBox::error(this, "项目加载失败", "该项目文件可能已损坏或格式不兼容。");
        }
    } else if (m_currentMode == Mode_Opened) {
        if (saveFormToJson(m_currentProjectFilePath)) {
            ModelParameter::instance()->setParameters(m_editPorosity->text().toDouble(), m_editThickness->text().toDouble(), m_editMu->text().toDouble(), m_editB->text().toDouble(), m_editCt->text().toDouble(), m_editRate->text().toDouble(), m_editRw->text().toDouble(), m_editL->text().toDouble(), m_editNf->text().toDouble(), m_currentProjectFilePath);
            Standard_MessageBox::info(this, "操作成功", "项目的属性参数更改已安全保存至本地。");
        }
    }
}

/**
 * @brief 顶部“新建项目”被点击
 */
void WT_ProjectWidget::onNewProjectClicked() {
    if (m_isProjectOpen) { Standard_MessageBox::warning(this, "操作受限", "请先关闭当前正在运行的项目，再执行新建操作。"); return; }
    setFormMode(Mode_New);
}

/**
 * @brief 顶部“打开项目”被点击
 */
void WT_ProjectWidget::onOpenProjectClicked() {
    if (m_isProjectOpen) { Standard_MessageBox::warning(this, "操作受限", "为了保证内存安全，本软件不支持同时打开多个项目。\n请先点击“关闭项目”。"); return; }
    QString filePath = QFileDialog::getOpenFileName(this, "选择项目", "", "WellTest Project (*.pwt)");
    if (filePath.isEmpty()) return;
    if (ModelParameter::instance()->loadProject(filePath)) {
        setProjectState(true, filePath); updateRecentProjectsList(filePath); setFormMode(Mode_Opened, filePath); emit projectOpened(false);
    } else {
        Standard_MessageBox::error(this, "解析异常", "所选文件无法完成加载。");
    }
}

/**
 * @brief 顶部“关闭项目”被点击，应用三按钮交互
 */
void WT_ProjectWidget::onCloseProjectClicked() {
    if (!m_isProjectOpen) { Standard_MessageBox::info(this, "提示", "目前系统处于空闲状态，没有需要关闭的活跃项目。"); return; }

    QString projName = QFileInfo(m_currentProjectFilePath).fileName();
    QString text = QString("您即将关闭项目 [%1]。\n为了防止进度丢失，建议在关闭前保存当前进度。").arg(projName);

    int res = Standard_MessageBox::question3(this, "确认关闭项目", text, "保存并关闭", "直接关闭", "取消");
    if (res == 0) { saveFormToJson(m_currentProjectFilePath); saveCurrentProject(); closeProjectInternal(); }
    else if (res == 1) { closeProjectInternal(); }
}

/**
 * @brief 执行项目内存清空
 */
void WT_ProjectWidget::closeProjectInternal() {
    setProjectState(false, "");
    ModelParameter::instance()->closeProject();
    m_currentMode = Mode_None; m_rightStackedWidget->setCurrentWidget(m_workflowPage);
    emit projectClosed();
}

/**
 * @brief 近期记录点击预览
 */
void WT_ProjectWidget::onRecentProjectClicked(QListWidgetItem *item) {
    if (!item || m_isProjectOpen) return;
    QString filePath = item->data(Qt::UserRole).toString();
    if(QFileInfo::exists(filePath)) setFormMode(Mode_Preview, filePath);
}

/**
 * @brief 近期记录双击装载
 */
void WT_ProjectWidget::onRecentProjectDoubleClicked(QListWidgetItem *item) {
    if (m_isProjectOpen) { Standard_MessageBox::warning(this, "操作受限", "请先关闭当前项目后再载入历史工程。"); return; }
    QString filePath = item->data(Qt::UserRole).toString();
    if (ModelParameter::instance()->loadProject(filePath)) {
        setProjectState(true, filePath); updateRecentProjectsList(filePath); setFormMode(Mode_Opened, filePath); emit projectOpened(false);
    } else {
        Standard_MessageBox::error(this, "文件错误", "指定的工程文件已被移除或损坏，已清理该条无效记录。");
        m_recentProjects.removeAll(filePath); loadRecentProjects(); m_rightStackedWidget->setCurrentWidget(m_workflowPage);
    }
}

/**
 * @brief 获取与更新历史记录
 */
void WT_ProjectWidget::loadRecentProjects() {
    QSettings settings("WellTestPro", "WellTestAnalysis");
    m_recentProjects = settings.value("RecentProjects").toStringList();
    ui->listWidget_recent->clear();

    for (const QString& path : m_recentProjects) {
        QFileInfo fileInfo(path);
        if (!fileInfo.exists()) {
            continue;
        }

        QString oilFieldName;
        QString wellName;
        QString modifiedTime = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm");

        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                oilFieldName = root.value("oilFieldName").toString(root.value("OilFieldName").toString());
                wellName = root.value("wellName").toString(root.value("WellName").toString());
            }
        }

        QString metaLine;
        if (!wellName.isEmpty() && !oilFieldName.isEmpty()) {
            metaLine = QString("井名: %1 | 油田: %2").arg(wellName, oilFieldName);
        } else if (!wellName.isEmpty()) {
            metaLine = QString("井名: %1").arg(wellName);
        } else if (!oilFieldName.isEmpty()) {
            metaLine = QString("油田: %1").arg(oilFieldName);
        } else {
            metaLine = "井名/油田信息未填写";
        }

        QString displayText = QString("%1\n%2\n最近修改: %3\n%4")
                                  .arg(fileInfo.fileName(), metaLine, modifiedTime, path);
        QListWidgetItem* item = new QListWidgetItem(displayText, ui->listWidget_recent);
        item->setData(Qt::UserRole, path);
    }
}

void WT_ProjectWidget::updateRecentProjectsList(const QString& newProjectPath) {
    m_recentProjects.removeAll(newProjectPath); m_recentProjects.prepend(newProjectPath);
    while (m_recentProjects.size() > MAX_RECENT_PROJECTS) m_recentProjects.removeLast();
    QSettings settings("WellTestPro", "WellTestAnalysis");
    settings.setValue("RecentProjects", m_recentProjects);
    loadRecentProjects();
}

bool WT_ProjectWidget::tryAutoLoadLastProject() {
    loadRecentProjects();
    if (m_isProjectOpen || m_recentProjects.isEmpty()) {
        return false;
    }

    const QString filePath = m_recentProjects.first();
    if (!QFileInfo::exists(filePath)) {
        m_recentProjects.removeAll(filePath);
        QSettings settings("WellTestPro", "WellTestAnalysis");
        settings.setValue("RecentProjects", m_recentProjects);
        loadRecentProjects();
        return false;
    }

    if (!ModelParameter::instance()->loadProject(filePath)) {
        return false;
    }

    setProjectState(true, filePath);
    updateRecentProjectsList(filePath);
    setFormMode(Mode_Opened, filePath);
    emit projectOpened(false);
    return true;
}

/**
 * @brief 管理工程状态和退出
 */
void WT_ProjectWidget::setProjectState(bool isOpen, const QString& filePath) { m_isProjectOpen = isOpen; m_currentProjectFilePath = filePath; }
bool WT_ProjectWidget::saveCurrentProject() { ModelParameter::instance()->saveProject(); return true; }
void WT_ProjectWidget::onExitClicked() {
    if (!m_isProjectOpen) { QApplication::quit(); } else { this->window()->close(); }
}
