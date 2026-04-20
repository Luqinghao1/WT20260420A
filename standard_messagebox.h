/**
 * @file standard_messagebox.h
 * @brief 全局统一的现代化标准消息弹窗组件 (无边框 + 全 QSS 渲染设计)
 * @details
 * 【本代码文件的作用与功能】
 * 1. 提供 Info, Warning, Error, Question, Question3 等五种静态快捷调用接口。
 * 2. 彻底剥离 QFont，通过严格的 QSS 像素级控制，确保在任何 DPI 缩放下的字体大小绝对一致。
 * 3. 支持标准的单按钮、双按钮（确定/取消）以及复杂的三按钮（如：保存并关闭/直接关闭/取消）交互。
 * 4. 采用无边框窗口设计，自带鼠标拖拽功能和顶部彩色状态指示条。
 */

#ifndef STANDARD_MESSAGEBOX_H
#define STANDARD_MESSAGEBOX_H

#include <QDialog>
#include <QString>
#include <QPoint>

class QLabel;
class QPushButton;
class QLineEdit;

class Standard_MessageBox : public QDialog
{
    Q_OBJECT

public:
    // 定义弹窗的四种严重等级，决定了图标样式和主色调
    enum MsgType {
        Info,       // 提示 (科技蓝)
        Question,   // 询问 (翡翠绿)
        Warning,    // 警告 (警示橙)
        Error       // 错误 (危险红)
    };

    // ====================================================================
    // 静态快捷调用接口 (无需 new，直接使用)
    // ====================================================================

    // 1. 基础单按钮提示框
    static void info(QWidget *parent, const QString &title, const QString &text);
    static void warning(QWidget *parent, const QString &title, const QString &text);
    static void error(QWidget *parent, const QString &title, const QString &text);

    // 2. 双按钮询问框 (确定/取消) -> 返回 true(确定), false(取消)
    static bool question(QWidget *parent, const QString &title, const QString &text);

    // 3. 三按钮复杂询问框 -> 返回 0(按钮1), 1(按钮2), 2(按钮3)
    static int question3(QWidget *parent, const QString &title, const QString &text,
                         const QString &btn1Text = "保存并关闭",
                         const QString &btn2Text = "直接关闭",
                         const QString &btn3Text = "取消");

    // 4. 带输入框的文本输入对话框 (可替代 QInputDialog::getText)
    //    用户点击"确定"时 *ok=true 并返回输入文本；"取消"时 *ok=false 并返回空串。
    static QString getText(QWidget *parent, const QString &title, const QString &label,
                           const QString &defaultText = QString(), bool *ok = nullptr);

protected:
    // 重载核心事件，支持无边框拖拽和自定义绘制
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    // 构造与析构私有化，强制开发者使用静态接口
    explicit Standard_MessageBox(MsgType type, const QString &title, const QString &text,
                                 QWidget *parent = nullptr, bool hasInput = false);
    ~Standard_MessageBox();

    // 内部初始化与 UI 构建函数
    void initUI();
    void applyStyles(); // 核心：全 QSS 化样式控制
    void setupButtons(int count, const QString& b1 = "", const QString& b2 = "", const QString& b3 = "");

    // 绘图与色彩辅助函数
    QPixmap generateIcon(MsgType type);
    QString getPrimaryColor() const;
    QString getHoverColor() const;
    QString getPressedColor() const;

private:
    MsgType m_type;
    QString m_title;
    QString m_text;
    bool    m_hasInput;        // 是否包含输入框 (true 时显示 QLineEdit)

    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QLabel *m_textLabel;
    QLineEdit *m_inputEdit;    // 可选：文本输入框 (仅 getText 模式使用)

    QPushButton *m_btnConfirm; // 对应返回值 0
    QPushButton *m_btnExtra;   // 对应返回值 1
    QPushButton *m_btnCancel;  // 对应返回值 2

    QPoint m_dragPosition;     // 记录鼠标拖拽时的坐标偏移量
};

#endif // STANDARD_MESSAGEBOX_H
