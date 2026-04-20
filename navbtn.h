/**
 * @file navbtn.h
 * @brief 左侧导航栏按钮控件头文件 (优化版)
 * @details 移除了悬停动画，严格控制选中白字、未选中黑字，字号缩小，取消图标阴影。
 */

#ifndef NAVBTN_H
#define NAVBTN_H

#include <QWidget>
#include <QPropertyAnimation>
#include <QPixmap>

class NavBtn : public QWidget
{
    Q_OBJECT
    // 仅保留点击时的微缩放反馈，移除悬停动画属性
    Q_PROPERTY(qreal pressValue READ pressValue WRITE setPressValue)

public:
    explicit NavBtn(QWidget *parent = nullptr);
    ~NavBtn();

    // 设置原有图片和名称
    void setPicName(const QString& picPath, const QString& name);

    QString getName() const;

    void setIndex(int index);
    int getIndex() const;

    // 状态切换：控制选中背景与文字颜色
    void setNormalStyle();
    void setClickedStyle();

signals:
    void sigClicked(QString name);

protected:
    // 重载核心事件
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int m_index;
    QString m_name;
    QPixmap m_iconPixmap;
    bool m_isChecked;

    qreal m_pressValue;
    QPropertyAnimation *m_pressAnim;

    qreal pressValue() const { return m_pressValue; }
    void setPressValue(qreal val) { m_pressValue = val; update(); }
};

#endif // NAVBTN_H
