/**
 * @file standard_toolbutton.h
 * @brief 全软件统一的标准顶部功能按钮组件
 * @details 继承自 QToolButton，内部固化了数据界面的排版、尺寸和全局 QSS 样式。
 * 调用者只需传入 文本 和 图标 即可，无需再重复写任何 UI 设置代码。
 */

#ifndef STANDARD_TOOLBUTTON_H
#define STANDARD_TOOLBUTTON_H

#include <QToolButton>
#include <QString>
#include <QIcon>

class Standard_ToolButton : public QToolButton
{
    Q_OBJECT

public:
    /**
     * @brief 标准按钮构造函数
     * @param text   按钮下方显示的文字
     * @param icon   按钮上方显示的矢量图标
     * @param parent 父级指针
     */
    explicit Standard_ToolButton(const QString &text, const QIcon &icon, QWidget *parent = nullptr);

    /**
     * @brief 纯文字构造函数 (备用，若需后期动态设置图标)
     * @param text   按钮下方显示的文字
     * @param parent 父级指针
     */
    explicit Standard_ToolButton(const QString &text, QWidget *parent = nullptr);

    ~Standard_ToolButton();

private:
    /**
     * @brief 初始化按钮的固化属性与样式
     */
    void initStyle();
};

#endif // STANDARD_TOOLBUTTON_H
