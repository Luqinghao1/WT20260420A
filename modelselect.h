/*
 * 文件名: modelselect.h
 * 作用与功能描述:
 * 1. 声明模型选择对话框 UI 类 (ModelSelect)。
 * 2. 提供获取用户选择的模型代码 (Code)、模型名称 (Name) 和裂缝形态的方法。
 * 3. 【扩展】全面支持高达 324 种模型组合（108等长 + 108非等长 + 108非均布非等长）。
 * 4. 声明静态方法 getShortName，提供全局统一的模型短名称生成能力，用于更新图版和拟合界面的按钮名称。
 */

#ifndef MODELSELECT_H
#define MODELSELECT_H

#include <QDialog>
#include <QString>

namespace Ui {
class ModelSelect;
}

class ModelSelect : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * 初始化模型选择界面及所有下拉框选项
     */
    explicit ModelSelect(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     * 释放界面资源
     */
    ~ModelSelect();

    /**
     * @brief 获取选中模型的底层系统代码
     * @return 返回内部标识符字符串，例如 "modelwidget1" 到 "modelwidget324"
     */
    QString getSelectedModelCode() const;

    /**
     * @brief 获取选中模型的完整中文名称
     * @return 返回带有编号和配置详情的中文字符串
     */
    QString getSelectedModelName() const;

    /**
     * @brief 获取用户选择的裂缝形态内部数据名
     * @return 返回如 "EqualUniform", "UnequalUniform" 或 "UnequalNonUniform"
     */
    QString getSelectedFracMorphology() const;

    /**
     * @brief 设置当前激活的模型代码，用于打开弹窗时的状态记忆与回显
     * @param code 目标模型代码 (如 "modelwidget217")
     */
    void setCurrentModelCode(const QString& code);

    /**
     * @brief 静态方法：根据底层模型 ID 推演标准、简短的模型展示名称
     * @param modelId 底层的数字 ID (1 ~ 324)
     * @return 返回类似 "model7-1 夹层型储层试井解释模型" 的精简名称
     */
    static QString getShortName(int modelId);

private slots:
    /**
     * @brief 任意下拉框选项变动时的槽函数
     * 负责重新计算排列组合，拼接出最终的模型 ID 并更新界面提示
     */
    void onSelectionChanged();

    /**
     * @brief 确认按钮被点击时的槽函数
     * 进行最后的状态校验，如果有效则接受 (Accept) 对话框
     */
    void onAccepted();

    /**
     * @brief 储层类型变动时的级联更新槽函数
     * 依据当前选择的储层大类（如夹层型、页岩型、混积型），动态更新与之匹配的内外区结构子选项
     */
    void updateInnerOuterOptions();

private:
    /**
     * @brief 初始选项加载
     * 挂载所有的界面显示文字及其对应的后台数据字典映射
     */
    void initOptions();

private:
    Ui::ModelSelect *ui;              // 界面指针
    QString m_selectedModelCode;      // 当前缓存的已选模型代码
    QString m_selectedModelName;      // 当前缓存的已选模型完整名称
    QString m_fracMorphology;         // 当前缓存的裂缝形态
    bool m_isInitializing;            // 防止初始化期间触发级联信号的锁标志
};

#endif // MODELSELECT_H
