/*
 * 文件名: datasamplingdialog.cpp
 * 作用: 数据滤波与取样悬浮窗口实现文件
 * 功能描述:
 * 1. 提供用户交互界面，设置数据的多阶段（早期、中期、晚期）滤波与抽样参数。
 * 2. 【悬浮窗特性】开启鼠标事件追踪，识别左侧 6 像素宽度的边缘，支持用户按住左键手动向左拉伸窗口宽度。
 * 3. 包含中值滤波、对数抽样、等间隔抽样等核心数学算法并展示处理前后对比图。
 * 修改记录:
 * - 优化了 "分阶段处理策略" 布局，改为 QGridLayout，左侧增加共用的内容提示标签。
 * - 放大了下方四个核心功能按钮，并为他们添加了直观的绿/红色背景以便区分。
 * - 右上角的取消/关闭按钮更改为醒目的红色背景，使用白色矢量叉号绘制图标。
 */

#include "datasamplingdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

// =========================================================================
// =                           构造与析构模块                                 =
// =========================================================================

DataSamplingWidget::DataSamplingWidget(QWidget *parent)
    : QWidget(parent),
    m_model(nullptr),
    m_dragLineIndex(0),
    m_isResizing(false),
    m_dragStartX(0)
{
    // 【关键】必须开启鼠标追踪，才能在不按下的情况下识别边缘并改变鼠标指针形态为拉伸箭头
    setMouseTracking(true);

    initUI();
    setupConnections();
    applyStyle();
}

DataSamplingWidget::~DataSamplingWidget() {}

// =========================================================================
// =                           UI 初始化与绘制模块                            =
// =========================================================================

/**
 * @brief 纯代码绘制标题栏的三个控制图标
 * @param type 0:恢复默认(双矩形) 1:最大化(单矩形) 2:关闭(交叉X)
 * @return 绘制完成的 QIcon
 */
QIcon DataSamplingWidget::createCtrlIcon(int type) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing); // 开启抗锯齿

    // 根据图标类型选择合适的画笔颜色，关闭按钮由于背景是红色，所以这里画白线
    if (type == 2) {
        p.setPen(QPen(Qt::white, 1.5)); // 白色叉号
    } else {
        p.setPen(QPen(QColor(80, 80, 80), 1.5)); // 图标线条颜色与粗细
    }

    if (type == 0) {
        // 绘制恢复默认大小图标 (两个重叠的矩形)
        p.drawRect(5, 9, 10, 10);
        p.drawLine(9, 9, 9, 5);
        p.drawLine(9, 5, 19, 5);
        p.drawLine(19, 5, 19, 15);
        p.drawLine(19, 15, 15, 15);
    } else if (type == 1) {
        // 绘制最大化图标 (单矩形)
        p.drawRect(6, 6, 12, 12);
    } else if (type == 2) {
        // 绘制关闭图标 (交叉X)
        p.drawLine(7, 7, 17, 17);
        p.drawLine(17, 7, 7, 17);
    }
    return QIcon(pixmap);
}

/**
 * @brief 界面所有控件的生成与布局规划
 */
void DataSamplingWidget::initUI()
{
    // 打破内部组件对尺寸的强制霸占，允许该窗口被主界面的自适应算法强行压窄
    this->setMinimumSize(450, 400); // 稍微增大一点最小宽度以适应左侧提示标签

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ================== 顶部：标题栏与控制按钮 ==================
    QWidget* titleBar = new QWidget(this);
    titleBar->setStyleSheet("background-color: #F0F2F5; border-bottom: 1px solid #D9D9D9;");
    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(15, 6, 10, 6);

    QLabel* lblTitle = new QLabel("数据滤波与多阶段抽样");
    lblTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #333333; border: none;");

    // 初始化右上角图标按钮
    btnRestoreWindow = new QPushButton();
    btnRestoreWindow->setIcon(createCtrlIcon(0));
    btnRestoreWindow->setToolTip("对齐到表格空白区");

    btnMaximizeWindow = new QPushButton();
    btnMaximizeWindow->setIcon(createCtrlIcon(1));
    btnMaximizeWindow->setToolTip("覆盖整个数据区");

    btnCloseWindow = new QPushButton();
    btnCloseWindow->setIcon(createCtrlIcon(2));
    btnCloseWindow->setToolTip("关闭功能面板");

    // 配置顶部普通控制按钮的样式
    QString ctrlStyle = "QPushButton { border: none; background: transparent; min-width: 28px; height: 28px; border-radius: 4px; }"
                        "QPushButton:hover { background-color: #E0E0E0; }"
                        "QPushButton:pressed { background-color: #D0D0D0; }";
    btnRestoreWindow->setStyleSheet(ctrlStyle);
    btnMaximizeWindow->setStyleSheet(ctrlStyle);

    // 【修改】给关闭按钮单独设定红底白叉的样式
    QString closeBtnStyle = "QPushButton { background-color: #FF4D4F; border: none; border-radius: 4px; min-width: 28px; height: 28px; }"
                            "QPushButton:hover { background-color: #FF7875; }"
                            "QPushButton:pressed { background-color: #D9363E; }";
    btnCloseWindow->setStyleSheet(closeBtnStyle);

    titleLayout->addWidget(lblTitle);
    titleLayout->addStretch();
    titleLayout->addWidget(btnRestoreWindow);
    titleLayout->addWidget(btnMaximizeWindow);
    titleLayout->addWidget(btnCloseWindow);

    rootLayout->addWidget(titleBar);

    // ================== 主体：内部参数及图表区 ==================
    QWidget* contentWidget = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(contentWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // ---------------- 左侧：参数设置区 ----------------
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(10);

    // 1. 数据源与坐标系组
    QGroupBox* groupData = new QGroupBox("1. 数据源与坐标系");
    QVBoxLayout* vData = new QVBoxLayout(groupData);
    QHBoxLayout* hX = new QHBoxLayout();
    hX->addWidget(new QLabel("X 轴数据:"));
    comboX = new QComboBox();
    chkLogX = new QCheckBox("X轴对数");
    hX->addWidget(comboX, 1);
    hX->addWidget(chkLogX);

    QHBoxLayout* hY = new QHBoxLayout();
    hY->addWidget(new QLabel("Y 轴数据:"));
    comboY = new QComboBox();
    chkLogY = new QCheckBox("Y轴对数");
    hY->addWidget(comboY, 1);
    hY->addWidget(chkLogY);

    vData->addLayout(hX);
    vData->addLayout(hY);
    leftLayout->addWidget(groupData);

    // 2. 全局异常点组 (基于 Y 轴上下限)
    QGroupBox* groupGlobal = new QGroupBox("2. 全局异常点剔除 (Y轴)");
    QVBoxLayout* vGlobal = new QVBoxLayout(groupGlobal);
    QHBoxLayout* hMin = new QHBoxLayout();
    hMin->addWidget(new QLabel("下限:"));
    spinMinY = new QDoubleSpinBox();
    spinMinY->setRange(-1e9, 1e9);
    spinMinY->setValue(0.0);
    hMin->addWidget(spinMinY);

    QHBoxLayout* hMax = new QHBoxLayout();
    hMax->addWidget(new QLabel("上限:"));
    spinMaxY = new QDoubleSpinBox();
    spinMaxY->setRange(-1e9, 1e9);
    spinMaxY->setValue(100.0);
    hMax->addWidget(spinMaxY);

    vGlobal->addLayout(hMin);
    vGlobal->addLayout(hMax);
    leftLayout->addWidget(groupGlobal);

    // 3. 区间划分边界设置
    QGroupBox* groupSplit = new QGroupBox("3. 区间划分 (支持图上拖拽)");
    QVBoxLayout* vSplit = new QVBoxLayout(groupSplit);
    QHBoxLayout* hS1 = new QHBoxLayout();
    hS1->addWidget(new QLabel("早-中边界:"));
    spinSplit1 = new QDoubleSpinBox();
    spinSplit1->setRange(-1e9, 1e9);
    spinSplit1->setDecimals(4);
    hS1->addWidget(spinSplit1);

    QHBoxLayout* hS2 = new QHBoxLayout();
    hS2->addWidget(new QLabel("中-晚边界:"));
    spinSplit2 = new QDoubleSpinBox();
    spinSplit2->setRange(-1e9, 1e9);
    spinSplit2->setDecimals(4);
    hS2->addWidget(spinSplit2);

    vSplit->addLayout(hS1);
    vSplit->addLayout(hS2);
    leftLayout->addWidget(groupSplit);

    // 4. 分阶段处理策略（早中晚三阶段）- 【已按照要求优化布局并添加提示】
    QGroupBox* groupStage = new QGroupBox("4. 分阶段处理策略");
    QGridLayout* gridStage = new QGridLayout(groupStage);
    gridStage->setVerticalSpacing(8);
    gridStage->setHorizontalSpacing(5);

    // 第一列：增加共用的提示内容标签
    QLabel* lblFilterAlg = new QLabel("滤波算法:");
    QLabel* lblFilterWin = new QLabel("滤波窗口(点):");
    QLabel* lblSampleAlg = new QLabel("抽样算法:");
    QLabel* lblSampleParam = new QLabel("抽样参数:");

    // 配置提示标签样式并放入第 0 列
    lblFilterAlg->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lblFilterWin->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lblSampleAlg->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lblSampleParam->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    gridStage->addWidget(lblFilterAlg, 1, 0);
    gridStage->addWidget(lblFilterWin, 2, 0);
    gridStage->addWidget(lblSampleAlg, 3, 0);
    gridStage->addWidget(lblSampleParam, 4, 0);

    QStringList stageNames = {"早期", "中期", "晚期"};
    for(int i = 0; i < 3; ++i) {
        // 添加顶部的阶段标题 (第0行)
        QLabel* titleLabel = new QLabel(stageNames[i]);
        titleLabel->setStyleSheet("font-weight: bold; color: #555555;");
        gridStage->addWidget(titleLabel, 0, i + 1, Qt::AlignCenter);

        // 滤波算法下拉框 (第1行)
        m_stages[i].comboFilter = new QComboBox();
        m_stages[i].comboFilter->addItems({"无滤波", "中值滤波"});
        m_stages[i].comboFilter->setCurrentIndex(0);
        gridStage->addWidget(m_stages[i].comboFilter, 1, i + 1);

        // 滤波窗口参数 (第2行)
        m_stages[i].spinFilterWin = new QSpinBox();
        m_stages[i].spinFilterWin->setRange(3, 101);
        m_stages[i].spinFilterWin->setSingleStep(2);
        m_stages[i].spinFilterWin->setValue(5);
        m_stages[i].spinFilterWin->setEnabled(false);
        gridStage->addWidget(m_stages[i].spinFilterWin, 2, i + 1);

        // 抽样算法下拉框 (第3行)
        m_stages[i].comboSample = new QComboBox();
        m_stages[i].comboSample->addItems({"对数", "间隔", "总点数"});
        m_stages[i].comboSample->setCurrentIndex(0);
        gridStage->addWidget(m_stages[i].comboSample, 3, i + 1);

        // 抽样参数 (第4行)
        m_stages[i].spinSampleVal = new QSpinBox();
        m_stages[i].spinSampleVal->setRange(1, 100000);
        m_stages[i].spinSampleVal->setValue(20);
        gridStage->addWidget(m_stages[i].spinSampleVal, 4, i + 1);

        // 联动逻辑：仅当选择了中值滤波时，才允许调整参考点数窗口大小
        connect(m_stages[i].comboFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, i](int index){ m_stages[i].spinFilterWin->setEnabled(index == 1); });
    }
    // 使得第0列具有合适自适应的宽度
    gridStage->setColumnStretch(0, 1);
    gridStage->setColumnStretch(1, 2);
    gridStage->setColumnStretch(2, 2);
    gridStage->setColumnStretch(3, 2);
    leftLayout->addWidget(groupStage);

    leftLayout->addStretch();

    // =========================================================================
    // =   按钮调整部分：应用大尺寸，及应用预览/确定(绿)、重置/取消(红)样式      =
    // =========================================================================

    // 定义放大的按钮样式表，包含背景颜色设定
    QString btnGreenStyle = "QPushButton { background-color: #52C41A; color: white; border: none; border-radius: 4px; padding: 8px 20px; min-width: 90px; font-weight: bold; font-size: 13px; }"
                            "QPushButton:hover { background-color: #73D13D; }"
                            "QPushButton:pressed { background-color: #389E0D; }";

    QString btnRedStyle = "QPushButton { background-color: #FF4D4F; color: white; border: none; border-radius: 4px; padding: 8px 20px; min-width: 90px; font-weight: bold; font-size: 13px; }"
                          "QPushButton:hover { background-color: #FF7875; }"
                          "QPushButton:pressed { background-color: #D9363E; }";

    // 预览与重置控制按钮布局
    QHBoxLayout* opLayout = new QHBoxLayout();
    btnPreview = new QPushButton("应用预览");
    btnReset = new QPushButton("重置数据");

    // 应用特定样式
    btnPreview->setStyleSheet(btnGreenStyle);
    btnReset->setStyleSheet(btnRedStyle);

    opLayout->addWidget(btnPreview);
    opLayout->addWidget(btnReset);
    leftLayout->addLayout(opLayout);

    // ---------------- 右侧：双图表展示区 ----------------
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(10);

    customPlotRaw = new QCustomPlot();
    customPlotRaw->plotLayout()->insertRow(0);
    customPlotRaw->plotLayout()->addElement(0, 0, new QCPTextElement(customPlotRaw, "抽样前 (原始数据与阶段划分)", QFont("Microsoft YaHei", 12, QFont::Bold)));

    customPlotProcessed = new QCustomPlot();
    customPlotProcessed->plotLayout()->insertRow(0);
    customPlotProcessed->plotLayout()->addElement(0, 0, new QCPTextElement(customPlotProcessed, "抽样后 (处理后数据)", QFont("Microsoft YaHei", 12, QFont::Bold)));

    rightLayout->addWidget(customPlotRaw, 1);
    rightLayout->addWidget(customPlotProcessed, 1);

    // ---------------- 底部：确认返回按钮 ----------------
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    btnOk = new QPushButton("确定");
    btnCancel = new QPushButton("取消");

    // 应用特定样式
    btnOk->setStyleSheet(btnGreenStyle);
    btnCancel->setStyleSheet(btnRedStyle);

    bottomLayout->addWidget(btnOk);
    bottomLayout->addWidget(btnCancel);
    rightLayout->addLayout(bottomLayout);

    // 组合左右布局
    mainLayout->addLayout(leftLayout, 4);
    mainLayout->addLayout(rightLayout, 5);
    rootLayout->addWidget(contentWidget, 1);

    // ---------------- 图表阶段分割线初始化 ----------------
    vLine1 = new QCPItemStraightLine(customPlotRaw);
    vLine2 = new QCPItemStraightLine(customPlotRaw);
    vLine1->setPen(QPen(QColor(24, 144, 255), 2, Qt::DashLine));
    vLine2->setPen(QPen(QColor(82, 196, 26), 2, Qt::DashLine));

    vLine1_proc = new QCPItemStraightLine(customPlotProcessed);
    vLine2_proc = new QCPItemStraightLine(customPlotProcessed);
    vLine1_proc->setPen(QPen(QColor(24, 144, 255), 2, Qt::DotLine));
    vLine2_proc->setPen(QPen(QColor(82, 196, 26), 2, Qt::DotLine));
}

/**
 * @brief 绑定交互信号
 */
void DataSamplingWidget::setupConnections()
{
    // 下拉框数据源变更事件
    connect(comboX, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataSamplingWidget::onDataColumnChanged);
    connect(comboY, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataSamplingWidget::onDataColumnChanged);

    // 坐标轴线型/对数选项变更事件
    connect(chkLogX, &QCheckBox::stateChanged, this, &DataSamplingWidget::onAxisScaleChanged);
    connect(chkLogY, &QCheckBox::stateChanged, this, &DataSamplingWidget::onAxisScaleChanged);

    // 区间划分微调框变化事件
    connect(spinSplit1, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &DataSamplingWidget::onSplitBoxValueChanged);
    connect(spinSplit2, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &DataSamplingWidget::onSplitBoxValueChanged);

    // 数据操作按钮信号
    connect(btnPreview, &QPushButton::clicked, this, &DataSamplingWidget::onPreviewClicked);
    connect(btnReset, &QPushButton::clicked, this, &DataSamplingWidget::onResetClicked);

    // 将处理好的数据打包抛给主窗口进行替换
    connect(btnOk, &QPushButton::clicked, this, [this]() {
        emit processingFinished(getHeaders(), getProcessedTable());
        emit requestClose();
    });
    connect(btnCancel, &QPushButton::clicked, this, &DataSamplingWidget::requestClose);

    // 窗口控制信号外抛
    connect(btnMaximizeWindow, &QPushButton::clicked, this, &DataSamplingWidget::requestMaximize);
    connect(btnRestoreWindow, &QPushButton::clicked, this, &DataSamplingWidget::requestRestore);
    connect(btnCloseWindow, &QPushButton::clicked, this, &DataSamplingWidget::requestClose);

    // 绑定鼠标在图表上的交互，实现边界线的直观拖拽
    connect(customPlotRaw, &QCustomPlot::mousePress, this, &DataSamplingWidget::onRawPlotMousePress);
    connect(customPlotRaw, &QCustomPlot::mouseMove, this, &DataSamplingWidget::onRawPlotMouseMove);
    connect(customPlotRaw, &QCustomPlot::mouseRelease, this, &DataSamplingWidget::onRawPlotMouseRelease);
}

/**
 * @brief 应用全局的扁平化控件样式（去除系统默认强烈的违和风格，保留底色统一）
 * 注意：由于前面在 initUI 里已经单独给四个按钮做了覆盖，这部分的 QPushButton 主要作用于可能的其他新增按钮
 */
void DataSamplingWidget::applyStyle()
{
    QString qss = "QWidget { color: #333333; font-family: 'Microsoft YaHei'; font-size: 12px; }"
                  "QGroupBox { border: 1px solid #E5E5E5; border-radius: 4px; margin-top: 10px; padding-top: 15px; }"
                  "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 3px; font-weight: bold; color: #1890FF; }"
                  "QComboBox, QSpinBox, QDoubleSpinBox { border: 1px solid #D9D9D9; border-radius: 3px; padding: 2px; min-height: 22px; }"
                  "QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: #1890FF; }"
                  "QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { background-color: #F5F5F5; color: #BFBFBF; }";
    this->setStyleSheet(qss);
}

// =========================================================================
// =                           悬浮窗边缘拖拽伸缩模块                          =
// =========================================================================

void DataSamplingWidget::mousePressEvent(QMouseEvent *event) {
    // 鼠标在左侧 6 像素边缘按下时，启动拉伸模式并记录起始坐标
    if (event->button() == Qt::LeftButton && event->pos().x() <= 6) {
        m_isResizing = true;
        m_dragStartX = event->globalPos().x();
    }
    QWidget::mousePressEvent(event);
}

void DataSamplingWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_isResizing) {
        // 计算鼠标水平移动了多少像素
        int dx = event->globalPos().x() - m_dragStartX;
        m_dragStartX = event->globalPos().x();
        // 将位移增量发送给主窗口，由主窗口调用 setGeometry 进行安全限制和绝对坐标刷新
        emit requestResize(dx);
    } else {
        // 只有开启了 setMouseTracking(true) 才能走到这里。悬停在左侧边缘时改变鼠标形状为缩放箭头
        if (event->pos().x() <= 6) setCursor(Qt::SizeHorCursor);
        else unsetCursor();
    }
    QWidget::mouseMoveEvent(event);
}

void DataSamplingWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isResizing = false; // 结束拖拽
    }
    QWidget::mouseReleaseEvent(event);
}

// =========================================================================
// =                             核心数据读取模块                             =
// =========================================================================

/**
 * @brief 将主窗口传入的 QStandardItemModel 绑定到当前组件，读取原始数据并缓存
 */
void DataSamplingWidget::setModel(QStandardItemModel* model)
{
    m_model = model;
    comboX->clear();
    comboY->clear();
    m_headers.clear();

    if (m_model && m_model->columnCount() > 0) {
        // 加载表头，供用户选择作为 X 轴或 Y 轴的数据列
        for(int i = 0; i < m_model->columnCount(); ++i) {
            QString name = m_model->headerData(i, Qt::Horizontal).toString();
            if(name.isEmpty()) name = QString("列 %1").arg(i+1);
            m_headers.append(name);
            comboX->addItem(name, i);
            comboY->addItem(name, i);
        }
        // 默认将第一列设为 X 轴，第二列设为 Y 轴
        if (m_model->columnCount() >= 2) {
            comboX->setCurrentIndex(0);
            comboY->setCurrentIndex(1);
        }
        loadRawData();
        updateSplitLines();
    }
}

/**
 * @brief 从绑定的模型中提取二维表及用户选择列对应的浮点数坐标数组
 */
void DataSamplingWidget::loadRawData()
{
    if (!m_model) return;

    m_rawTable.clear(); m_rawX.clear(); m_rawY.clear();
    int colX = comboX->currentData().toInt();
    int colY = comboY->currentData().toInt();
    m_xName = comboX->currentText();
    m_yName = comboY->currentText();

    double maxY = -1e9;

    for(int r = 0; r < m_model->rowCount(); ++r) {
        QStringList rowData;
        for(int c = 0; c < m_model->columnCount(); ++c) {
            rowData.append(m_model->index(r, c).data().toString());
        }

        bool okX, okY;
        double x = m_model->index(r, colX).data().toDouble(&okX);
        double y = m_model->index(r, colY).data().toDouble(&okY);

        // 如果该行选择列数据均可成功转换为 double 则认为该行有效加入缓存
        if(okX && okY) {
            m_rawTable.append(rowData);
            m_rawX.append(x);
            m_rawY.append(y);
            if (y > maxY) maxY = y;
        }
    }

    // 动态更新异常点剔除的上限默认值为全区最大值
    if (maxY > -1e8) {
        spinMaxY->blockSignals(true);
        spinMaxY->setValue(maxY);
        spinMaxY->blockSignals(false);
    }

    // 初始状态下处理后数据等同于未处理的原始数据
    m_processedTable = m_rawTable;
    m_processedX = m_rawX;
    m_processedY = m_rawY;

    // 触发图表重新绘制
    drawRawPlot();
    drawProcessedPlot();
}

/**
 * @brief 用户变更数据源下拉框后重新读取数据并刷新图表
 */
void DataSamplingWidget::onDataColumnChanged() {
    loadRawData();
    updateSplitLines();
}

/**
 * @brief 内部辅助函数：计算对数坐标系下的完美范围以避开负值崩溃或极端数值拉扯
 */
static void adjustLogAxisRange(QCPAxis* axis, const QVector<double>& data) {
    double minPos = 1e-3, maxPos = 1e-3;
    bool found = false;
    for (double v : data) {
        if (v > 0) {
            if (!found) { minPos = v; maxPos = v; found = true; }
            else { if (v < minPos) minPos = v; if (v > maxPos) maxPos = v; }
        }
    }

    if (found && maxPos > minPos) {
        double minLog = std::log10(minPos);
        double maxLog = std::log10(maxPos);
        double lowerLog = std::floor(minLog);
        double upperLog = std::ceil(maxLog);

        if (minLog - lowerLog < 0.15) lowerLog -= 1.0;
        if (upperLog - maxLog < 0.15) upperLog += 1.0;

        if (upperLog <= lowerLog + 1.0) {
            lowerLog -= 1.0;
            upperLog += 1.0;
        }
        axis->setRange(std::pow(10, lowerLog), std::pow(10, upperLog));
    } else if (found && maxPos == minPos) {
        double logV = std::floor(std::log10(minPos));
        axis->setRange(std::pow(10, logV - 1), std::pow(10, logV + 1));
    } else {
        axis->setRange(1e-3, 1e3); // 无有效正数则设为默认占位区间
    }
}

/**
 * @brief 坐标系切换（线型/对数）时的图表设置更新逻辑
 */
void DataSamplingWidget::onAxisScaleChanged() {
    // 处理 X 轴对数切换
    if (chkLogX->isChecked()) {
        customPlotRaw->xAxis->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTickerX1(new QCPAxisTickerLog);
        customPlotRaw->xAxis->setTicker(logTickerX1);

        customPlotProcessed->xAxis->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTickerX2(new QCPAxisTickerLog);
        customPlotProcessed->xAxis->setTicker(logTickerX2);
    } else {
        customPlotRaw->xAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTickerX1(new QCPAxisTicker);
        customPlotRaw->xAxis->setTicker(linTickerX1);

        customPlotProcessed->xAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTickerX2(new QCPAxisTicker);
        customPlotProcessed->xAxis->setTicker(linTickerX2);
    }

    // 处理 Y 轴对数切换
    if (chkLogY->isChecked()) {
        customPlotRaw->yAxis->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTickerY1(new QCPAxisTickerLog);
        customPlotRaw->yAxis->setTicker(logTickerY1);

        customPlotProcessed->yAxis->setScaleType(QCPAxis::stLogarithmic);
        QSharedPointer<QCPAxisTickerLog> logTickerY2(new QCPAxisTickerLog);
        customPlotProcessed->yAxis->setTicker(logTickerY2);
    } else {
        customPlotRaw->yAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTickerY1(new QCPAxisTicker);
        customPlotRaw->yAxis->setTicker(linTickerY1);

        customPlotProcessed->yAxis->setScaleType(QCPAxis::stLinear);
        QSharedPointer<QCPAxisTicker> linTickerY2(new QCPAxisTicker);
        customPlotProcessed->yAxis->setTicker(linTickerY2);
    }

    drawRawPlot();
    drawProcessedPlot();
}

// =========================================================================
// =                             边界线交互与绘制模块                          =
// =========================================================================

/**
 * @brief 自动计算边界线初始位置，将其按默认比例设置在数据的 10% 和 50% 处
 */
void DataSamplingWidget::updateSplitLines() {
    if(m_rawX.isEmpty()) return;
    double minX = *std::min_element(m_rawX.begin(), m_rawX.end());
    double maxX = *std::max_element(m_rawX.begin(), m_rawX.end());
    if (minX == maxX) maxX += 1.0;

    double s1 = minX + (maxX - minX) * 0.1;
    double s2 = minX + (maxX - minX) * 0.5;

    spinSplit1->blockSignals(true); spinSplit2->blockSignals(true);
    spinSplit1->setValue(s1);       spinSplit2->setValue(s2);
    spinSplit1->blockSignals(false);spinSplit2->blockSignals(false);

    onSplitBoxValueChanged();
}

/**
 * @brief 将数值框中的值同步更新到图表上的阶段划分直线位置
 */
void DataSamplingWidget::onSplitBoxValueChanged() {
    double s1 = spinSplit1->value();
    double s2 = spinSplit2->value();
    // 防错控制：中晚期边界永远不应小于早期边界
    if (s1 > s2) { s1 = s2 - 1e-4; spinSplit1->setValue(s1); }

    vLine1->point1->setCoords(s1, 0); vLine1->point2->setCoords(s1, 1);
    vLine2->point1->setCoords(s2, 0); vLine2->point2->setCoords(s2, 1);

    vLine1_proc->point1->setCoords(s1, 0); vLine1_proc->point2->setCoords(s1, 1);
    vLine2_proc->point1->setCoords(s2, 0); vLine2_proc->point2->setCoords(s2, 1);

    customPlotRaw->replot();
    customPlotProcessed->replot();
}

/** @brief 捕获鼠标点击了图表上哪根边界线以便于准备拖拽 */
void DataSamplingWidget::onRawPlotMousePress(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        double x = customPlotRaw->xAxis->pixelToCoord(event->pos().x());
        double s1 = vLine1->point1->key();
        double s2 = vLine2->point1->key();
        double range = customPlotRaw->xAxis->range().size();
        double tol = range * 0.05; // 点击允许的距离容差

        if (std::abs(x - s1) < tol && std::abs(x - s1) <= std::abs(x - s2)) m_dragLineIndex = 1;
        else if (std::abs(x - s2) < tol) m_dragLineIndex = 2;
        else m_dragLineIndex = 0;
    }
}

/** @brief 在图表上拖动鼠标时动态更新被选中边界线的 X 坐标 */
void DataSamplingWidget::onRawPlotMouseMove(QMouseEvent *event) {
    if (m_dragLineIndex != 0) {
        double x = customPlotRaw->xAxis->pixelToCoord(event->pos().x());
        if (m_dragLineIndex == 1) {
            double s2 = spinSplit2->value();
            if (x >= s2) x = s2 - 1e-4; // 限制早中边界不能越过中晚边界
            spinSplit1->setValue(x);
        } else if (m_dragLineIndex == 2) {
            double s1 = spinSplit1->value();
            if (x <= s1) x = s1 + 1e-4;
            spinSplit2->setValue(x);
        }
    }
}

/** @brief 释放鼠标左键，结束图表上虚线的拖拽状态 */
void DataSamplingWidget::onRawPlotMouseRelease(QMouseEvent *) {
    m_dragLineIndex = 0;
}

/**
 * @brief 在左侧图表绘制原始未抽样、未滤波的完整散点线
 */
void DataSamplingWidget::drawRawPlot() {
    customPlotRaw->clearGraphs();
    customPlotRaw->addGraph();
    customPlotRaw->graph(0)->setData(m_rawX, m_rawY);

    QPen redPen(Qt::red);
    redPen.setWidthF(1.0);
    customPlotRaw->graph(0)->setPen(redPen);
    customPlotRaw->graph(0)->setLineStyle(QCPGraph::lsLine);
    customPlotRaw->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 3));
    customPlotRaw->graph(0)->setName(QString("原始数据 (共 %1 点)").arg(m_rawX.size()));

    customPlotRaw->xAxis->setLabel(m_xName);
    customPlotRaw->yAxis->setLabel(m_yName);

    if (chkLogX->isChecked()) adjustLogAxisRange(customPlotRaw->xAxis, m_rawX);
    else customPlotRaw->xAxis->rescale();

    if (chkLogY->isChecked()) adjustLogAxisRange(customPlotRaw->yAxis, m_rawY);
    else customPlotRaw->yAxis->rescale();

    customPlotRaw->legend->setVisible(true);
    customPlotRaw->replot();
}

/**
 * @brief 在右侧图表绘制经过分阶段抽样和滤波算法处理过后的散点线
 */
void DataSamplingWidget::drawProcessedPlot() {
    customPlotProcessed->clearGraphs();
    customPlotProcessed->addGraph();
    customPlotProcessed->graph(0)->setData(m_processedX, m_processedY);

    QPen darkBluePen(Qt::darkBlue);
    darkBluePen.setWidthF(1.5);
    customPlotProcessed->graph(0)->setPen(darkBluePen);
    customPlotProcessed->graph(0)->setLineStyle(QCPGraph::lsLine);
    customPlotProcessed->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 4));
    customPlotProcessed->graph(0)->setName(QString("处理后数据 (共 %1 点)").arg(m_processedX.size()));

    customPlotProcessed->xAxis->setLabel(m_xName);
    customPlotProcessed->yAxis->setLabel(m_yName);

    if (chkLogX->isChecked()) adjustLogAxisRange(customPlotProcessed->xAxis, m_processedX);
    else customPlotProcessed->xAxis->rescale();

    if (chkLogY->isChecked()) adjustLogAxisRange(customPlotProcessed->yAxis, m_processedY);
    else customPlotProcessed->yAxis->rescale();

    customPlotProcessed->legend->setVisible(true);
    customPlotProcessed->replot();
}

// =========================================================================
// =                             核心算法处理模块                              =
// =========================================================================

/**
 * @brief 用户点击应用预览，运行核心数据处理引擎
 */
void DataSamplingWidget::onPreviewClicked() {
    processData();
    drawProcessedPlot();
}

/**
 * @brief 重置所有处理逻辑，恢复为原始数据状态
 */
void DataSamplingWidget::onResetClicked() {
    m_processedTable = m_rawTable;
    m_processedX = m_rawX;
    m_processedY = m_rawY;
    drawProcessedPlot();
}

/**
 * @brief 执行异常点剔除、阶段划分、阶段滤波、阶段抽样主流程
 */
void DataSamplingWidget::processData()
{
    if (m_rawTable.isEmpty()) return;

    double s1 = spinSplit1->value();
    double s2 = spinSplit2->value();
    double minY = spinMinY->value();
    double maxY = spinMaxY->value();

    int colY = comboY->currentData().toInt();

    // 创建分别用于早、中、晚三段的临时数据桶
    QVector<QStringList> tb[3];
    QVector<double> tx[3], ty[3];

    // 第一步：全局数据遍历，执行上下限异常剔除，并基于 X 坐标段分配到不同阶段数组
    for (int i = 0; i < m_rawTable.size(); ++i) {
        double x = m_rawX[i];
        double y = m_rawY[i];
        if (y < minY || y > maxY) continue;

        int st = (x < s1) ? 0 : ((x < s2) ? 1 : 2);
        tb[st].append(m_rawTable[i]);
        tx[st].append(x);
        ty[st].append(y);
    }

    // 清空现有结果数据集，准备拼接新产生的数据
    m_processedTable.clear(); m_processedX.clear(); m_processedY.clear();

    // 第二步：循环分阶段执行各阶段设定的滤波和抽样策略
    for (int i = 0; i < 3; ++i) {
        if (tx[i].isEmpty()) continue;

        // 滤波处理
        if (m_stages[i].comboFilter->currentIndex() == 1) {
            applyMedianFilter(ty[i], m_stages[i].spinFilterWin->value());
            // 将滤波平滑后改变的 Y 值重新写入到用于传给底层的二维表格字面字符串中
            for(int j = 0; j < ty[i].size(); ++j) {
                tb[i][j][colY] = QString::number(ty[i][j], 'f', 6);
            }
        }

        // 抽样处理
        int sType = m_stages[i].comboSample->currentIndex();
        int sVal = m_stages[i].spinSampleVal->value();
        QVector<int> indices;

        // 获取当前阶段根据设定算法应该保留下来的原始索引集合
        if (sType == 0) indices = getLogSamplingIndices(tx[i], sVal);
        else if (sType == 1) indices = getIntervalSamplingIndices(tx[i].size(), sVal);
        else if (sType == 2) indices = getTotalPointsSamplingIndices(tx[i].size(), sVal);

        // 利用索合生成该阶段最终保留的数据点并归拢拼接到全局处理后数组中
        for (int idx : indices) {
            m_processedTable.append(tb[i][idx]);
            m_processedX.append(tx[i][idx]);
            m_processedY.append(ty[i][idx]);
        }
    }
}

/**
 * @brief 一维序列中值滤波算法 (有效消除局部孤立毛刺尖峰)
 * @param y 待滤波序列 (引用传递，原位修改)
 * @param windowSize 滑动窗口大小 (点数)
 */
void DataSamplingWidget::applyMedianFilter(QVector<double>& y, int windowSize) {
    if (y.size() < windowSize) return;
    int halfWin = windowSize / 2;
    QVector<double> y_filtered = y;
    for (int i = halfWin; i < y.size() - halfWin; ++i) {
        QVector<double> winData;
        for (int j = -halfWin; j <= halfWin; ++j) winData.append(y[i + j]);
        std::sort(winData.begin(), winData.end()); // 取窗口内数据的中位数
        y_filtered[i] = winData[winData.size() / 2];
    }
    y = y_filtered;
}

/**
 * @brief 对数阶梯抽样算法（常用于试井等存在明显对数周期规律的数据，实现在对数图上点距均匀）
 * @param x 依据的 X 坐标数组
 * @param pointsPerDecade 每十个数量级计划采集保留的点数
 */
QVector<int> DataSamplingWidget::getLogSamplingIndices(const QVector<double>& x, int pointsPerDecade) {
    QVector<int> indices;
    if (x.isEmpty() || pointsPerDecade <= 0) return indices;
    indices.append(0); // 首点恒保留

    double xMin = x.first() > 0 ? x.first() : 1e-4;
    double logStep = 1.0 / pointsPerDecade; // 计算对数域跨度步长
    double nextTargetLog = std::log10(xMin) + logStep;

    for (int i = 1; i < x.size() - 1; ++i) {
        if (x[i] > 0 && std::log10(x[i]) >= nextTargetLog) {
            indices.append(i);
            nextTargetLog += logStep;
        }
    }
    if (indices.last() != x.size() - 1) indices.append(x.size() - 1); // 强保末尾点不丢失
    return indices;
}

/**
 * @brief 等间隔抽样算法
 * @param totalCount 当前段的总点数
 * @param interval 跳过的点数间隔
 */
QVector<int> DataSamplingWidget::getIntervalSamplingIndices(int totalCount, int interval) {
    QVector<int> indices;
    if (interval <= 1) {
        for(int i=0; i<totalCount; ++i) indices.append(i);
        return indices;
    }
    for (int i = 0; i < totalCount; i += interval) indices.append(i);
    if (indices.last() != totalCount - 1) indices.append(totalCount - 1);
    return indices;
}

/**
 * @brief 总点数强制均分抽样算法
 * @param totalCount 当前段落具有的总点数
 * @param targetPoints 期望该段落最终剩下的特定目标点数
 */
QVector<int> DataSamplingWidget::getTotalPointsSamplingIndices(int totalCount, int targetPoints) {
    QVector<int> indices;
    if (totalCount <= targetPoints || targetPoints <= 2) {
        for(int i=0; i<totalCount; ++i) indices.append(i);
        return indices;
    }
    double step = (double)(totalCount - 1) / (targetPoints - 1);
    for (int i = 0; i < targetPoints; ++i) {
        int idx = qRound(i * step);
        if (idx >= totalCount) idx = totalCount - 1;
        indices.append(idx);
    }
    return indices;
}

/**
 * @brief 对外提供获取当前处理完成后纯文本数据的接口
 */
QVector<QStringList> DataSamplingWidget::getProcessedTable() const {
    return m_processedTable;
}

/**
 * @brief 对外提供获取当前表头的接口
 */
QStringList DataSamplingWidget::getHeaders() const {
    return m_headers;
}


