/*
 * 文件名: paramselectdialog.h
 * 文件作用: 参数选择配置对话框头文件。
 * 功能描述:
 * 1. 声明了参数的显示、选择、限值配置等交互接口。
 * 2. 支持重置默认参数和自动更新限值功能。
 * 3. 包含了表格列重排、高亮行渲染优化（首尾不亮）、且保证高亮不遮挡表格网格线等UI美化相关核心方法的声明。
 * 4. [新增] 支持用户控制拟合过程中是否强制应用上下限约束。
 */

#ifndef PARAMSELECTDIALOG_H
#define PARAMSELECTDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QEvent>
#include "fittingparameterchart.h"

namespace Ui {
class ParamSelectDialog;
}

class ParamSelectDialog : public QDialog
{
    Q_OBJECT

public:
    // 构造函数：初始化参数对话框（新增 useLimits 参数）
    explicit ParamSelectDialog(const QList<FitParameter>& params,
                               ModelManager::ModelType modelType,
                               double fitTime,
                               bool useLimits,
                               QWidget *parent = nullptr);
    // 析构函数：释放 UI 资源
    ~ParamSelectDialog();

    // 获取用户修改后的参数列表，供外部提取
    QList<FitParameter> getUpdatedParams() const;

    // 获取用户设定的最大拟合时间
    double getFittingTime() const;

    // 获取用户是否勾选了“使用上下限控制范围”
    bool getUseLimits() const;

signals:
    void estimateInitialParamsRequested();

protected:
    // 事件过滤器，用于拦截调节框内的鼠标滚轮事件，防止误修改数值
    bool eventFilter(QObject *obj, QEvent *event) override;

    friend class FittingWidget;
    friend class WT_MultidataFittingWidget;

private:
    Ui::ParamSelectDialog *ui;

public:
    void collectData();

private:

    // 当前选择的模型类型
    ModelManager::ModelType m_modelType;

    // 初始化并渲染表格（包括列顺序重组、背景透明化、高度居中等）
    void initTable();

    // 参数列表副本，用于展示和数据修改绑定
    QList<FitParameter> m_params;
    void updateRowAppearance(int row, bool isFit);

private slots:
    // 确认与取消
    void onConfirm();
    void onCancel();

    // 重置参数与自动适配限值
    void onResetParams();
    void onAutoLimits();
    void onEstimateInitialParams();
};

#endif // PARAMSELECTDIALOG_H
