/**
 * @file standard_toolbutton.cpp
 * @brief 标准功能按钮实现文件
 * @details 彻底贯彻“全 QSS 化”和“样式内聚”，阻断外部 DPI 和全局样式的干扰。
 */

#include "standard_toolbutton.h"

// 带有文字和图标的主构造函数
Standard_ToolButton::Standard_ToolButton(const QString &text, const QIcon &icon, QWidget *parent)
    : QToolButton(parent)
{
    this->setText(text);
    this->setIcon(icon);
    initStyle();
}

// 仅带文字的备用构造函数
Standard_ToolButton::Standard_ToolButton(const QString &text, QWidget *parent)
    : QToolButton(parent)
{
    this->setText(text);
    initStyle();
}

Standard_ToolButton::~Standard_ToolButton()
{
}

/**
 * @brief 核心：将数据界面的经典样式 100% 固化到该组件内部
 */
void Standard_ToolButton::initStyle()
{
    // 1. 固化排版逻辑：强制“上图标，下文字”
    this->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // 2. 固化图标尺寸：强制为 36x36 的精致大小
    this->setIconSize(QSize(36, 36));

    // 3. 固化全 QSS 样式 (背景、圆角、悬停交互、绝对字号字体回退)
    // 彻底摒弃 QFont，将字体家族、字号(13px)、颜色打包进 QSS，阻断外部污染
    QString standardQSS =
        "QToolButton { "
        "  background-color: transparent; "
        "  border-radius: 4px; "
        "  border: 1px solid transparent; "
        "  color: #333333; "
        "  font-family: 'Microsoft YaHei', 'PingFang SC', sans-serif; "
        "  font-size: 13px; "
        "  font-weight: normal; "
        "  padding: 4px; "
        "  min-width: 65px; "
        "  min-height: 70px; "  // 固化物理占用尺寸
        "}"
        "QToolButton:hover { "
        "  background-color: #E6F7FF; "
        "  border: 1px solid #91D5FF; "  // 浅蓝色悬停交互
        "}"
        "QToolButton:pressed { "
        "  background-color: #BAE7FF; "
        "  border: 1px solid #1890FF; "  // 深蓝色按下交互
        "}";

    this->setStyleSheet(standardQSS);

    // 设置光标为手型，增强交互暗示
    this->setCursor(Qt::PointingHandCursor);
}
