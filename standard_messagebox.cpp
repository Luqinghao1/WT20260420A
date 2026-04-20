/**
 * @file standard_messagebox.cpp
 * @brief 全局统一标准消息弹窗实现文件
 * @details
 * 【本代码文件的作用与功能】
 * 1. 实现了所有静态调用接口的底层逻辑，以及 1~3 个按钮的动态排版装配。
 * 2. 使用纯 C++ 代码 (QPainter) 绘制四种状态的高清矢量图标（蓝 i、绿 ?、黄 !、红 X）。
 * 3. 实现了完全阻断外部干扰的 QSS 样式表注入，确保跨平台视觉 100% 统一。
 */

#include "standard_messagebox.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>

// ====================================================================
// 静态快捷调用接口实现
// ====================================================================

void Standard_MessageBox::info(QWidget *parent, const QString &title, const QString &text) {
    Standard_MessageBox msg(Info, title, text, parent);
    msg.setupButtons(1, "确定");
    msg.exec();
}

void Standard_MessageBox::warning(QWidget *parent, const QString &title, const QString &text) {
    Standard_MessageBox msg(Warning, title, text, parent);
    msg.setupButtons(1, "确定");
    msg.exec();
}

void Standard_MessageBox::error(QWidget *parent, const QString &title, const QString &text) {
    Standard_MessageBox msg(Error, title, text, parent);
    msg.setupButtons(1, "确定");
    msg.exec();
}

bool Standard_MessageBox::question(QWidget *parent, const QString &title, const QString &text) {
    Standard_MessageBox msg(Question, title, text, parent);
    msg.setupButtons(2, "确定", "", "取消");
    return msg.exec() == 0; // 返回值为 0 代表点击了确定
}

int Standard_MessageBox::question3(QWidget *parent, const QString &title, const QString &text,
                                   const QString &btn1, const QString &btn2, const QString &btn3) {
    Standard_MessageBox msg(Question, title, text, parent);
    msg.setupButtons(3, btn1, btn2, btn3);
    return msg.exec(); // 返回具体点击了哪个按钮 (0, 1, 2)
}

QString Standard_MessageBox::getText(QWidget *parent, const QString &title, const QString &label,
                                     const QString &defaultText, bool *ok) {
    // 使用 Question (绿色) 风格表示"请求输入"；带输入框模式
    Standard_MessageBox msg(Question, title, label, parent, true);
    msg.m_inputEdit->setText(defaultText);
    msg.m_inputEdit->selectAll();
    msg.m_inputEdit->setFocus();
    // 回车键直接确认（绑定到主按钮的 done(0)）
    QObject::connect(msg.m_inputEdit, &QLineEdit::returnPressed, &msg, [&msg]() { msg.done(0); });
    msg.setupButtons(2, "确定", "", "取消");

    bool accepted = (msg.exec() == 0);
    if (ok) *ok = accepted;
    return accepted ? msg.m_inputEdit->text() : QString();
}

// ====================================================================
// UI 初始化与底层事件逻辑
// ====================================================================

Standard_MessageBox::Standard_MessageBox(MsgType type, const QString &title, const QString &text,
                                         QWidget *parent, bool hasInput)
    : QDialog(parent), m_type(type), m_title(title), m_text(text),
      m_hasInput(hasInput), m_inputEdit(nullptr)
{
    // 隐藏系统标题栏，并允许背景透明以便于绘制圆角和阴影
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    initUI();
    applyStyles();
}

Standard_MessageBox::~Standard_MessageBox() {}

void Standard_MessageBox::initUI()
{
    // 约束弹窗尺寸，保证排版不会过窄或拉伸过长
    this->setMinimumWidth(380);
    this->setMaximumWidth(600);

    // 根布局
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 28, 24, 24); // 顶部边距稍大，为彩色指示条留出空间
    rootLayout->setSpacing(20);

    // ------ 顶部内容区 (图标 + 文字) ------
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(16);

    // 左侧图标
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(40, 40);
    m_iconLabel->setPixmap(generateIcon(m_type));

    QVBoxLayout *iconLayout = new QVBoxLayout();
    iconLayout->addWidget(m_iconLabel);
    iconLayout->addStretch();
    contentLayout->addLayout(iconLayout);

    // 右侧文本区 (支持换行)
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(8);

    m_titleLabel = new QLabel(m_title, this);
    m_textLabel = new QLabel(m_text, this);
    m_textLabel->setWordWrap(true);

    textLayout->addWidget(m_titleLabel);
    textLayout->addWidget(m_textLabel);

    // 仅在 getText 模式下创建并挂载输入框
    if (m_hasInput) {
        m_inputEdit = new QLineEdit(this);
        m_inputEdit->setClearButtonEnabled(true);
        textLayout->addWidget(m_inputEdit);
        // 输入模式下放宽最小宽度，提高可用性
        this->setMinimumWidth(420);
    }

    textLayout->addStretch();

    contentLayout->addLayout(textLayout, 1);
    rootLayout->addLayout(contentLayout);

    // ------ 底部操作区 (按钮) ------
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(12);
    btnLayout->addStretch(); // 弹簧将按钮顶向右侧

    // 实例化全部三个按钮
    m_btnCancel = new QPushButton("取消", this);
    m_btnExtra = new QPushButton("备用", this);
    m_btnConfirm = new QPushButton("确定", this);

    // 绑定返回码 (0:主操作, 1:副操作, 2:取消操作)
    connect(m_btnConfirm, &QPushButton::clicked, this, [this](){ this->done(0); });
    connect(m_btnExtra, &QPushButton::clicked, this, [this](){ this->done(1); });
    connect(m_btnCancel, &QPushButton::clicked, this, [this](){ this->done(2); });

    btnLayout->addWidget(m_btnCancel);
    btnLayout->addWidget(m_btnExtra);
    btnLayout->addWidget(m_btnConfirm);

    rootLayout->addLayout(btnLayout);
}

/**
 * @brief 根据调用场景动态装配按钮的显示数量和文本
 */
void Standard_MessageBox::setupButtons(int count, const QString& b1, const QString& b2, const QString& b3) {
    if (count == 1) {
        m_btnCancel->hide();
        m_btnExtra->hide();
        if(!b1.isEmpty()) m_btnConfirm->setText(b1);
    } else if (count == 2) {
        m_btnExtra->hide();
        if(!b1.isEmpty()) m_btnConfirm->setText(b1);
        if(!b3.isEmpty()) m_btnCancel->setText(b3);
    } else if (count == 3) {
        m_btnConfirm->setText(b1);
        m_btnExtra->setText(b2);
        m_btnCancel->setText(b3);
    }
}

// ====================================================================
// 纯 QSS 样式控制 (完全摒弃 QFont 绝对控制物理像素)
// ====================================================================

QString Standard_MessageBox::getPrimaryColor() const {
    switch (m_type) {
    case Info: return "#1890FF"; case Question: return "#10B981";
    case Warning: return "#FA8C16"; case Error: return "#EF4444";
    default: return "#1890FF";
    }
}

QString Standard_MessageBox::getHoverColor() const {
    switch (m_type) {
    case Info: return "#40A9FF"; case Question: return "#34D399";
    case Warning: return "#FFA940"; case Error: return "#F87171";
    default: return "#40A9FF";
    }
}

QString Standard_MessageBox::getPressedColor() const {
    switch (m_type) {
    case Info: return "#096DD9"; case Question: return "#059669";
    case Warning: return "#D46B08"; case Error: return "#DC2626";
    default: return "#096DD9";
    }
}

void Standard_MessageBox::applyStyles()
{
    // 1. 标题标签：16px，加粗，深墨蓝
    m_titleLabel->setStyleSheet(
        "QLabel {"
        "  font-family: 'Microsoft YaHei', 'PingFang SC', sans-serif;"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  color: #1E293B;"
        "  background: transparent;"
        "}"
        );

    // 2. 正文标签：14px，正常字重，高级灰，提供1.5倍行高
    m_textLabel->setStyleSheet(
        "QLabel {"
        "  font-family: 'Microsoft YaHei', 'PingFang SC', sans-serif;"
        "  font-size: 14px;"
        "  font-weight: normal;"
        "  color: #475569;"
        "  background: transparent;"
        "}"
        );

    // 3. 确定/主操作按钮：13px，加粗，纯白字体，背景跟随主题色
    QString confirmStyle = QString(
                               "QPushButton { "
                               "  background-color: %1; color: #FFFFFF; "
                               "  font-family: 'Microsoft YaHei', 'PingFang SC', sans-serif; "
                               "  font-size: 13px; font-weight: bold; "
                               "  border: none; border-radius: 6px; min-width: 80px; height: 32px; "
                               "}"
                               "QPushButton:hover { background-color: %2; }"
                               "QPushButton:pressed { background-color: %3; }"
                               ).arg(getPrimaryColor(), getHoverColor(), getPressedColor());
    m_btnConfirm->setStyleSheet(confirmStyle);

    // 4. 取消/次级操作按钮：13px，加粗，深灰字体，浅灰背景
    QString secondaryStyle =
        "QPushButton { "
        "  background-color: #F1F5F9; color: #475569; "
        "  font-family: 'Microsoft YaHei', 'PingFang SC', sans-serif; "
        "  font-size: 13px; font-weight: bold; "
        "  border: none; border-radius: 6px; min-width: 80px; height: 32px; "
        "}"
        "QPushButton:hover { background-color: #E2E8F0; color: #1E293B; }"
        "QPushButton:pressed { background-color: #CBD5E1; }";
    m_btnCancel->setStyleSheet(secondaryStyle);
    m_btnExtra->setStyleSheet(secondaryStyle); // 备用按钮 (中间的选项) 也应用次级样式

    // 5. 输入框样式 (仅 getText 模式)：14px，圆角，聚焦时高亮主题色边框
    if (m_inputEdit) {
        QString inputStyle = QString(
            "QLineEdit {"
            "  font-family: 'Microsoft YaHei', 'PingFang SC', sans-serif;"
            "  font-size: 14px;"
            "  color: #1E293B;"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "  min-height: 24px;"
            "}"
            "QLineEdit:focus {"
            "  border: 1.5px solid %1;"
            "}"
        ).arg(getPrimaryColor());
        m_inputEdit->setStyleSheet(inputStyle);
    }
}

// ====================================================================
// 纯代码绘制四大状态的精美矢量图标
// ====================================================================

QPixmap Standard_MessageBox::generateIcon(MsgType type)
{
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    int cx = 20, cy = 20;
    switch (type) {
    case Info:
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#E6F7FF"));
        p.drawEllipse(0, 0, 40, 40);
        p.setPen(QPen(QColor("#1890FF"), 4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(cx, cy - 4, cx, cy + 8);
        p.drawPoint(cx, cy - 10);
        break;

    case Question:
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#D1FAE5"));
        p.drawEllipse(0, 0, 40, 40);
        p.setPen(QPen(QColor("#10B981"), 3, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(13, 10, 14, 14, -30 * 16, 240 * 16);
        p.drawLine(cx, cy + 2, cx, cy + 5);
        p.setPen(QPen(QColor("#10B981"), 4, Qt::SolidLine, Qt::RoundCap));
        p.drawPoint(cx, cy + 10);
        break;

    case Warning:
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#FFF7E6"));
        p.drawEllipse(0, 0, 40, 40);
        p.setPen(QPen(QColor("#FA8C16"), 4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(cx, cy - 8, cx, cy + 2);
        p.drawPoint(cx, cy + 8);
        break;

    case Error:
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#FEF2F2"));
        p.drawEllipse(0, 0, 40, 40);
        p.setPen(QPen(QColor("#EF4444"), 4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(cx - 6, cy - 6, cx + 6, cy + 6);
        p.drawLine(cx + 6, cy - 6, cx - 6, cy + 6);
        break;
    }
    return pixmap;
}

// ====================================================================
// 窗口背景与顶部彩色指示条绘制
// ====================================================================

void Standard_MessageBox::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 绘制白色圆角弹窗主体
    QPainterPath path;
    path.addRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);
    p.setBrush(QColor("#FFFFFF"));
    p.setPen(QPen(QColor("#E2E8F0"), 1));
    p.drawPath(path);

    // 绘制顶部的彩色指示条 (Accent Line)
    QPainterPath topPath;
    topPath.addRoundedRect(0, 0, width() - 1, 6, 8, 8);

    // 利用直角矩形遮盖下方的圆角，实现“仅上方为圆角”的彩色条
    QPainterPath bottomRect;
    bottomRect.addRect(0, 4, width() - 1, 2);

    p.setBrush(QColor(getPrimaryColor()));
    p.setPen(Qt::NoPen);
    p.drawPath(topPath.united(bottomRect));
}

// ====================================================================
// 无边框窗口拖拽实现
// ====================================================================

void Standard_MessageBox::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void Standard_MessageBox::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}
