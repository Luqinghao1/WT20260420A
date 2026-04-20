/*
 * settingswidget.cpp
 * 文件作用：系统设置窗口的具体实现
 * 功能描述：
 * 1. 构造函数初始化 UI、连接信号槽、加载 QSettings 配置
 * 2. 实现工作区/启动、路径等设置模块的加载与保存逻辑
 * 3. 实现左右区域颜色的下拉框选择与实时预览
 */

#include "settingswidget.h"
#include "ui_settingswidget.h"
#include "settingpaths.h"
#include "standard_messagebox.h"
#include <QDebug>
#include <QDate>
#include <QColor>
#include <QComboBox>

// 默认常量定义
const int SettingsWidget::DEFAULT_AUTO_SAVE = 10;
const int SettingsWidget::DEFAULT_MAX_BACKUPS = 10;

static const QColor DEFAULT_NAV_COLOR("#FFE0B2");
static const QColor DEFAULT_CONTENT_COLOR("#E1F5FE");

// 预设颜色列表
struct ColorPreset { QString name; QColor color; };

static const QList<ColorPreset> NAV_PRESETS = {
    {"浅橙色", QColor("#FFE0B2")},
    {"浅黄色", QColor("#FFF3CD")},
    {"浅灰色", QColor("#ECEFF1")},
    {"浅绿色", QColor("#E8F5E9")},
    {"浅粉色", QColor("#FCE4EC")},
    {"浅蓝色", QColor("#BBDEFB")},
    {"浅青色", QColor("#E0F7FA")},
    {"浅紫色", QColor("#EDE7F6")},
    {"浅薄荷", QColor("#E0F2F1")},
    {"浅米色", QColor("#FDF5E6")},
    {"浅棕色", QColor("#EFEBE9")},
    {"浅红色", QColor("#FFEBEE")},
    {"白色",   QColor("#FFFFFF")},
};

static const QList<ColorPreset> CONTENT_PRESETS = {
    {"浅蓝色", QColor("#E1F5FE")},
    {"米白色", QColor("#FFF8E1")},
    {"浅灰蓝", QColor("#EAF2F8")},
    {"浅青色", QColor("#E0F7FA")},
    {"浅紫色", QColor("#F3E5F5")},
    {"浅灰色", QColor("#F5F5F5")},
    {"浅绿色", QColor("#E8F5E9")},
    {"浅橙色", QColor("#FFF3E0")},
    {"浅粉色", QColor("#FCE4EC")},
    {"浅黄色", QColor("#FFFDE7")},
    {"浅棕色", QColor("#EFEBE9")},
    {"浅红色", QColor("#FFEBEE")},
    {"白色",   QColor("#FFFFFF")},
};

// 工具函数：为下拉框条目添加颜色图标
static void populateColorCombo(QComboBox* combo, const QList<ColorPreset>& presets)
{
    if (!combo) return;
    combo->clear();
    for (const auto& p : presets) {
        QPixmap px(16, 16);
        px.fill(p.color);
        combo->addItem(QIcon(px), p.name, p.color.name());
    }
}

static int findColorIndex(const QList<ColorPreset>& presets, const QColor& color)
{
    for (int i = 0; i < presets.size(); ++i) {
        if (presets[i].color == color) return i;
    }
    return 0;
}

SettingsWidget::SettingsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SettingsWidget),
    m_settings(nullptr),
    m_isModified(false),
    m_navColor(DEFAULT_NAV_COLOR),
    m_contentColor(DEFAULT_CONTENT_COLOR)
{
    ui->setupUi(this);

    // 初始化 QSettings (组织名, 应用名)
    m_settings = new QSettings("WellTestPro", "WellTestAnalysis", this);

    // 初始化界面控件内容
    initInterface();

    // 加载已保存的设置
    loadSettings();

    // 连接颜色下拉框：选择后实时应用
    if (auto navCombo = findChild<QComboBox*>(QStringLiteral("cmbNavColor"))) {
        connect(navCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (index >= 0 && index < NAV_PRESETS.size()) {
                m_navColor = NAV_PRESETS[index].color;
                m_settings->setValue("appearance/navBackground", m_navColor.name());
                m_settings->sync();
                emitAppearanceChanged();
                m_isModified = true;
            }
        });
    }
    if (auto contentCombo = findChild<QComboBox*>(QStringLiteral("cmbContentColor"))) {
        connect(contentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (index >= 0 && index < CONTENT_PRESETS.size()) {
                m_contentColor = CONTENT_PRESETS[index].color;
                m_settings->setValue("appearance/contentBackground", m_contentColor.name());
                m_settings->sync();
                emitAppearanceChanged();
                m_isModified = true;
            }
        });
    }

    // 连接通用修改信号
    QList<QWidget*> widgets = this->findChildren<QWidget*>();
    for(auto w : widgets) {
        if(qobject_cast<QLineEdit*>(w))
            connect(qobject_cast<QLineEdit*>(w), &QLineEdit::textChanged, this, &SettingsWidget::onSettingModified);
        else if(qobject_cast<QSpinBox*>(w))
            connect(qobject_cast<QSpinBox*>(w), QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsWidget::onSettingModified);
        else if(qobject_cast<QCheckBox*>(w))
            connect(qobject_cast<QCheckBox*>(w), &QCheckBox::toggled, this, &SettingsWidget::onSettingModified);
    }
}

SettingsWidget::~SettingsWidget()
{
    delete ui;
}

void SettingsWidget::initInterface()
{
    ui->navTupleList->setCurrentRow(0);

    // 填充颜色下拉框
    if (auto navCombo = findChild<QComboBox*>(QStringLiteral("cmbNavColor")))
        populateColorCombo(navCombo, NAV_PRESETS);
    if (auto contentCombo = findChild<QComboBox*>(QStringLiteral("cmbContentColor")))
        populateColorCombo(contentCombo, CONTENT_PRESETS);
}

void SettingsWidget::loadSettings()
{
    // --- 1. 界面颜色 ---
    QColor savedNav(m_settings->value("appearance/navBackground", DEFAULT_NAV_COLOR.name()).toString());
    QColor savedContent(m_settings->value("appearance/contentBackground", DEFAULT_CONTENT_COLOR.name()).toString());
    m_navColor = savedNav.isValid() ? savedNav : DEFAULT_NAV_COLOR;
    m_contentColor = savedContent.isValid() ? savedContent : DEFAULT_CONTENT_COLOR;

    if (auto navCombo = findChild<QComboBox*>(QStringLiteral("cmbNavColor"))) {
        navCombo->blockSignals(true);
        navCombo->setCurrentIndex(findColorIndex(NAV_PRESETS, m_navColor));
        navCombo->blockSignals(false);
    }
    if (auto contentCombo = findChild<QComboBox*>(QStringLiteral("cmbContentColor"))) {
        contentCombo->blockSignals(true);
        contentCombo->setCurrentIndex(findColorIndex(CONTENT_PRESETS, m_contentColor));
        contentCombo->blockSignals(false);
    }

    // --- 2. 启动行为 ---
    ui->chkStartFullScreen->setChecked(m_settings->value("general/fullScreen", false).toBool());
    ui->chkAutoLoadLastProject->setChecked(false);
    ui->chkAutoLoadLastProject->setEnabled(false);

    // --- 3. 工作区保护 ---
    ui->spinAutoSave->setValue(m_settings->value("system/autoSaveInterval", DEFAULT_AUTO_SAVE).toInt());
    ui->chkEnableBackup->setChecked(m_settings->value("system/backupEnabled", true).toBool());
    ui->spinMaxBackups->setValue(m_settings->value("system/maxBackups", DEFAULT_MAX_BACKUPS).toInt());

    // --- 4. 路径设置 ---
    ui->lineDataPath->setText(SettingPaths::dataPath());
    ui->lineReportPath->setText(SettingPaths::reportPath());
    ui->lineBackupPath->setText(SettingPaths::backupPath());

    // --- 5. 维护与日志 ---
    ui->cmbLogLevel->setCurrentIndex(m_settings->value("system/logLevel", 2).toInt());
    ui->chkCleanupLogs->setChecked(m_settings->value("system/cleanupLogs", true).toBool());
    ui->spinLogDays->setValue(m_settings->value("system/logRetention", 30).toInt());

    m_isModified = false;
}

void SettingsWidget::applySettings()
{
    if (!validatePaths()) {
        Standard_MessageBox::warning(this, "路径错误", "配置的路径不能为空且必须具有读写权限！");
        return;
    }

    // 保存颜色
    m_settings->setValue("appearance/navBackground", m_navColor.name());
    m_settings->setValue("appearance/contentBackground", m_contentColor.name());

    // 保存启动行为
    m_settings->setValue("general/fullScreen", ui->chkStartFullScreen->isChecked());

    // 保存工作区保护
    m_settings->setValue("system/autoSaveInterval", ui->spinAutoSave->value());
    m_settings->setValue("system/backupEnabled", ui->chkEnableBackup->isChecked());
    m_settings->setValue("system/maxBackups", ui->spinMaxBackups->value());

    // 保存路径
    m_settings->setValue("paths/data", ui->lineDataPath->text());
    m_settings->setValue("paths/report", ui->lineReportPath->text());
    m_settings->setValue("paths/backup", ui->lineBackupPath->text());

    // 保存维护
    m_settings->setValue("system/logLevel", ui->cmbLogLevel->currentIndex());
    m_settings->setValue("system/cleanupLogs", ui->chkCleanupLogs->isChecked());
    m_settings->setValue("system/logRetention", ui->spinLogDays->value());

    m_settings->sync();

    emit settingsChanged();

    Standard_MessageBox::info(this, "系统设置", "设置已保存并生效！");
    m_isModified = false;

    ensureDirExists(ui->lineDataPath->text());
    ensureDirExists(ui->lineReportPath->text());
    ensureDirExists(ui->lineBackupPath->text());
}

void SettingsWidget::restoreDefaults()
{
    if(!Standard_MessageBox::question(this, "确认重置", "确定要将所有设置恢复为出厂默认值吗？\n此操作不可撤销。"))
        return;

    const QStringList keys = {
        "appearance/navBackground", "appearance/contentBackground", "general/fullScreen",
        "paths/data", "paths/report", "paths/backup",
        "system/autoSaveInterval", "system/backupEnabled", "system/maxBackups",
        "system/cleanupLogs", "system/logRetention", "system/logLevel"
    };
    for (const QString& key : keys) m_settings->remove(key);
    m_settings->sync();

    loadSettings();
    emitAppearanceChanged();

    Standard_MessageBox::info(this, "系统设置", "已恢复默认设置。");
}

bool SettingsWidget::validatePaths()
{
    const QStringList paths = {
        ui->lineDataPath->text().trimmed(),
        ui->lineReportPath->text().trimmed(),
        ui->lineBackupPath->text().trimmed()
    };
    for (const QString& path : paths) {
        if (path.isEmpty()) return false;
        QDir dir(path);
        if (!dir.exists() && !QDir().mkpath(path)) return false;
    }
    return true;
}

void SettingsWidget::ensureDirExists(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) dir.mkpath(path);
}

// 槽函数：导航切换
void SettingsWidget::on_navTupleList_currentRowChanged(int currentRow)
{
    ui->stackedContent->setCurrentIndex(currentRow);
    QStringList titles = {
        "工作区与启动 - 界面配色与启动行为",
        "路径与维护 - 目录、日志与维护策略"
    };
    if(currentRow >= 0 && currentRow < titles.size())
        ui->lblPageTitle->setText(titles[currentRow]);
}

// 槽函数：浏览按钮
void SettingsWidget::on_btnBrowseData_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择数据存储路径", ui->lineDataPath->text());
    if(!dir.isEmpty()) ui->lineDataPath->setText(dir);
}
void SettingsWidget::on_btnBrowseReport_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择报告输出路径", ui->lineReportPath->text());
    if(!dir.isEmpty()) ui->lineReportPath->setText(dir);
}
void SettingsWidget::on_btnBrowseBackup_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择备份路径", ui->lineBackupPath->text());
    if(!dir.isEmpty()) ui->lineBackupPath->setText(dir);
}

// 槽函数：底部按钮
void SettingsWidget::on_btnRestoreDefaults_clicked() { restoreDefaults(); }
void SettingsWidget::on_btnApply_clicked() { applySettings(); }
void SettingsWidget::on_btnCancel_clicked() {
    if (m_isModified) {
        if (!Standard_MessageBox::question(this, "放弃未保存更改", "当前设置存在未保存修改，确认放弃并关闭吗？"))
            return;
    }
    loadSettings();
    this->close();
}
void SettingsWidget::onSettingModified() { m_isModified = true; }

// 颜色选择（不再需要 QColorDialog 版本）
void SettingsWidget::onPickNavColor() {}
void SettingsWidget::onPickContentColor() {}

void SettingsWidget::updateColorButton(QPushButton*, const QColor&) {}

void SettingsWidget::emitAppearanceChanged()
{
    emit appearanceChanged(m_navColor, m_contentColor);
}

// Getters
QString SettingsWidget::getDataPath() const { return ui->lineDataPath->text(); }
QString SettingsWidget::getReportPath() const { return ui->lineReportPath->text(); }
QString SettingsWidget::getBackupPath() const { return ui->lineBackupPath->text(); }
int SettingsWidget::getAutoSaveInterval() const { return ui->spinAutoSave->value(); }
bool SettingsWidget::isBackupEnabled() const { return ui->chkEnableBackup->isChecked(); }
QColor SettingsWidget::getNavBackgroundColor() const { return m_navColor; }
QColor SettingsWidget::getContentBackgroundColor() const { return m_contentColor; }
