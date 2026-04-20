/**
 * @file navbtn.cpp
 * @brief 左侧导航栏按钮控件实现文件
 */

#include "navbtn.h"
#include <QPainter>
#include <QFont>
#include <QMouseEvent>

NavBtn::NavBtn(QWidget *parent) :
    QWidget(parent),
    m_index(0),
    m_isChecked(false),
    m_pressValue(0.0)
{
    // 固定高度，维持布局稳定
    this->setFixedHeight(105);
    this->setMinimumWidth(110);

    // 仅保留点击物理回缩动画 (快速反馈 100ms)
    m_pressAnim = new QPropertyAnimation(this, "pressValue", this);
    m_pressAnim->setDuration(100);
}

NavBtn::~NavBtn() {}

/**
 * @brief 接收主界面的路径参数，支持直接解析原有 border-image 字符串格式
 */
void NavBtn::setPicName(const QString& picPath, const QString& name)
{
    m_name = name;

    // 如果传入的是类似 "border-image: url(:/new/prefix1/Resource/X0.png);" 的字符串，提取真实路径
    QString realPath = picPath;
    realPath.replace("border-image: url(", "").replace(");", "");
    realPath = realPath.trimmed();

    m_iconPixmap = QPixmap(realPath);
    update();
}

QString NavBtn::getName() const { return m_name; }
void NavBtn::setIndex(int index) { m_index = index; }
int NavBtn::getIndex() const { return m_index; }

void NavBtn::setNormalStyle()
{
    m_isChecked = false;
    update();
}

void NavBtn::setClickedStyle()
{
    m_isChecked = true;
    update();
}

void NavBtn::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton) {
        m_pressAnim->setDirection(QAbstractAnimation::Forward);
        m_pressAnim->setEndValue(1.0);
        m_pressAnim->start();
    }
    QWidget::mousePressEvent(event);
}

void NavBtn::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton) {
        m_pressAnim->setDirection(QAbstractAnimation::Backward);
        m_pressAnim->start();
        emit sigClicked(m_name);
        setClickedStyle(); // 点亮自己
    }
    QWidget::mouseReleaseEvent(event);
}

void NavBtn::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // 1. 绘制背景：选中时为深蓝色半透明，未选中全透明 (无悬停效果)
    if (m_isChecked) {
        p.fillRect(rect(), QColor(27, 45, 85, 120));
    }

    // 2. 点击缩放动画应用
    qreal scale = 1.0 - (0.05 * m_pressValue);
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(scale, scale);
    p.translate(-width() / 2.0, -height() / 2.0);

    // 3. 绘制原版图片图标 (无任何阴影和滤镜处理)
    int iconSize = 46;
    QRect iconRect(width() / 2 - iconSize / 2, 18, iconSize, iconSize);
    if (!m_iconPixmap.isNull()) {
        p.drawPixmap(iconRect, m_iconPixmap.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    // 4. 绘制下方文字
    // 逻辑：点击(选中)时白色，其余时候纯黑色
    QColor textColor = m_isChecked ? Qt::white : Qt::black;
    QFont font = p.font();
    font.setPixelSize(14);      // 【下调字号】设定为14px，比之前更精巧
    font.setBold(true);         // 保持加粗
    p.setFont(font);
    p.setPen(textColor);

    QRect textRect(0, 68, width(), 35);
    p.drawText(textRect, Qt::AlignCenter, m_name);
}
