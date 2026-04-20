/*
 * 文件名: datasamplingdialog.h
 * 作用: 数据滤波与取样悬浮窗口头文件
 * 功能描述:
 * 1. 声明数据滤波与多阶段抽样的核心数据结构及界面类。
 * 2. 支持拖拽窗口边缘调整大小。
 * 3. 负责向主界面抛出处理完成后的数据信号。
 * 修改记录:
 * - 优化了界面的控件布局结构，以支持分阶段策略的共用提示标签。
 * - 预留了样式美化相关的声明。
 */

#ifndef DATASAMPLINGDIALOG_H
#define DATASAMPLINGDIALOG_H

#include <QWidget>
#include <QVector>
#include <QStringList>
#include <QStandardItemModel>
#include <QMouseEvent>
#include <QIcon>
#include "qcustomplot.h"

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;

class DataSamplingWidget : public QWidget
{
    Q_OBJECT

public:
    // 构造与析构函数
    explicit DataSamplingWidget(QWidget *parent = nullptr);
    ~DataSamplingWidget();

    // 绑定外部的数据模型
    void setModel(QStandardItemModel* model);

    // 获取处理后的结果表格与表头
    QVector<QStringList> getProcessedTable() const;
    QStringList getHeaders() const;

signals:
    void requestMaximize();     // 信号: 请求最大化
    void requestRestore();      // 信号: 请求恢复默认对齐大小
    void requestClose();        // 信号: 请求关闭窗口
    void requestResize(int dx); // 信号: 请求主界面配合进行水平宽度拉伸
    void processingFinished(const QStringList& headers, const QVector<QStringList>& processedTable); // 处理完成的数据传出信号

protected:
    // 重载鼠标事件以支持悬浮窗边缘的手动拖拽拉伸
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    // 控件交互槽函数
    void onPreviewClicked();            // 预览按钮点击
    void onResetClicked();              // 重置按钮点击
    void onDataColumnChanged();         // 数据列改变
    void onAxisScaleChanged();          // 坐标系(对数/线性)切换
    void onSplitBoxValueChanged();      // 区间划分数值改变

    // 图表鼠标交互槽函数
    void onRawPlotMousePress(QMouseEvent *event);
    void onRawPlotMouseMove(QMouseEvent *event);
    void onRawPlotMouseRelease(QMouseEvent *event);

private:
    // 初始化相关私有函数
    void initUI();              // 初始化界面控件及布局
    void setupConnections();    // 绑定信号与槽
    void applyStyle();          // 全局应用扁平化样式

    // 纯代码绘制控制台矢量图标 (0:恢复 1:最大化 2:关闭)
    QIcon createCtrlIcon(int type);

    // 核心数据处理流
    void loadRawData();         // 加载初始原始数据
    void updateSplitLines();    // 更新图表分割线位置
    void drawRawPlot();         // 绘制原始数据图表
    void drawProcessedPlot();   // 绘制处理后数据图表
    void processData();         // 执行核心的数据抽样及滤波算法

    // 算法辅助函数
    void applyMedianFilter(QVector<double>& y, int windowSize);                             // 中值滤波算法
    QVector<int> getLogSamplingIndices(const QVector<double>& x, int pointsPerDecade);      // 对数抽样算法
    QVector<int> getIntervalSamplingIndices(int totalCount, int interval);                  // 间隔抽样算法
    QVector<int> getTotalPointsSamplingIndices(int totalCount, int targetPoints);           // 总点数抽样算法

    // 核心数据存储
    QStandardItemModel* m_model;
    QStringList m_headers;
    QVector<QStringList> m_rawTable;
    QVector<QStringList> m_processedTable;

    QVector<double> m_rawX, m_rawY;
    QVector<double> m_processedX, m_processedY;
    QString m_xName, m_yName;

    // 左侧设置区 UI 控件
    QComboBox* comboX;
    QComboBox* comboY;
    QCheckBox* chkLogX;
    QCheckBox* chkLogY;
    QDoubleSpinBox* spinMinY;
    QDoubleSpinBox* spinMaxY;
    QDoubleSpinBox* spinSplit1;
    QDoubleSpinBox* spinSplit2;

    // 分阶段 UI 控件聚合结构
    struct StageUI {
        QComboBox* comboFilter;
        QSpinBox* spinFilterWin;
        QComboBox* comboSample;
        QSpinBox* spinSampleVal;
    };
    StageUI m_stages[3];

    // 底部操作按钮
    QPushButton* btnPreview;
    QPushButton* btnReset;
    QPushButton* btnOk;
    QPushButton* btnCancel;

    // 标题栏控制按钮
    QPushButton* btnMaximizeWindow;
    QPushButton* btnRestoreWindow;
    QPushButton* btnCloseWindow;

    // 右侧双图表控件
    QCustomPlot* customPlotRaw;
    QCustomPlot* customPlotProcessed;
    QCPItemStraightLine *vLine1, *vLine2;
    QCPItemStraightLine *vLine1_proc, *vLine2_proc;

    int m_dragLineIndex;        // 当前拖拽的边界线索引(0无, 1早中边界, 2中晚边界)

    // 窗口拉伸的状态标记
    bool m_isResizing;
    int m_dragStartX;
};

#endif // DATASAMPLINGDIALOG_H
