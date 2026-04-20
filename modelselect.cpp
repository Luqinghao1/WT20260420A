/*
 * 文件名: modelselect.cpp
 * 作用与功能描述:
 * 1. 实现模型选择界面的全部交互与逻辑处理。
 * 2. 映射从储层类型、内外区、边界条件、井储类型到最终 1~324 编号的算法逻辑。
 * 3. 【解除拦截】正式开放非均布非等长模型选项，并映射至 217-324 号底层引擎。
 * 4. 优化状态回显逻辑，确保三大裂缝形态均可完美跨界面保持与记忆。
 */

#include "modelselect.h"
#include "ui_modelselect.h"
#include "standard_messagebox.h"
#include <QDialogButtonBox>
#include <QPushButton>

/**
 * @brief 构造函数，完成 UI 搭建与信号槽全量绑定
 */
ModelSelect::ModelSelect(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ModelSelect),
    m_isInitializing(false)
{
    ui->setupUi(this);
    // 设置全局字体与颜色，确保显示清晰
    this->setStyleSheet("QWidget { color: black; font-family: Arial; }");

    // 加载全部模型组合的下拉框选项
    initOptions();

    // === 绑定级联更新信号槽 ===
    // 储层大类变化时，级联更新内外区下拉框的内容
    connect(ui->comboReservoirModel, SIGNAL(currentIndexChanged(int)), this, SLOT(updateInnerOuterOptions()));

    // 任意配置项变化时，触发模型 ID 的重新计算
    connect(ui->comboWellModel, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboReservoirModel, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboBoundary, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboStorage, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboInnerOuter, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));
    connect(ui->comboFracMorphology, SIGNAL(currentIndexChanged(int)), this, SLOT(onSelectionChanged()));

    // 断开原生 accept()，绑定到带有校验的 onAccepted() 自定义槽函数
    disconnect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ModelSelect::onAccepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 主动调用一次，初始化计算标签状态
    onSelectionChanged();
}

/**
 * @brief 析构函数，释放 UI 内存
 */
ModelSelect::~ModelSelect()
{
    delete ui;
}

/**
 * @brief 初始化所有的下拉框配置选项与底层数据绑定
 */
void ModelSelect::initOptions()
{
    m_isInitializing = true; // 上锁，防止添加选项时触发 currentIndexChanged 信号

    ui->comboWellModel->clear();
    ui->comboReservoirModel->clear();
    ui->comboBoundary->clear();
    ui->comboStorage->clear();
    ui->comboInnerOuter->clear();

    // 1. 井型配置
    ui->comboWellModel->addItem("压裂水平井", "FracHorizontal");

    // 2. 储层大类配置
    ui->comboReservoirModel->addItem("径向复合模型", "RadialComposite");
    ui->comboReservoirModel->addItem("夹层型径向复合模型", "InterlayerComposite");
    ui->comboReservoirModel->addItem("页岩型径向复合模型", "ShaleComposite");
    ui->comboReservoirModel->addItem("混积型径向复合模型", "MixedComposite");

    // 3. 边界条件配置
    ui->comboBoundary->addItem("无限大外边界", "Infinite");
    ui->comboBoundary->addItem("封闭边界", "Closed");
    ui->comboBoundary->addItem("定压边界", "ConstantPressure");

    // 4. 井筒储集与表皮模型配置
    ui->comboStorage->addItem("定井储", "Constant");
    ui->comboStorage->addItem("线源解", "LineSource");
    ui->comboStorage->addItem("Fair模型", "Fair");
    ui->comboStorage->addItem("Hegeman模型", "Hegeman");

    // 5. 裂缝形态配置 (已全量开放)
    ui->comboFracMorphology->addItem("均布等长", "EqualUniform");
    ui->comboFracMorphology->addItem("均布非等长", "UnequalUniform");
    ui->comboFracMorphology->addItem("非均布非等长", "UnequalNonUniform");

    // 将所有组合重置到首选项
    ui->comboWellModel->setCurrentIndex(0);
    ui->comboReservoirModel->setCurrentIndex(0);
    ui->comboBoundary->setCurrentIndex(0);
    ui->comboStorage->setCurrentIndex(0);

    m_isInitializing = false; // 解锁

    // 初始化时手动构建一次内外区匹配列表
    updateInnerOuterOptions();
}

/**
 * @brief 动态更新内外区组合配置下拉框
 */
void ModelSelect::updateInnerOuterOptions()
{
    // 屏蔽信号防止误触触发 onSelectionChanged
    bool oS = ui->comboInnerOuter->blockSignals(true);
    ui->comboInnerOuter->clear();

    QString cR = ui->comboReservoirModel->currentData().toString();

    // 依据当前的储层大类型，装载对应的特定子区组合
    if (cR == "RadialComposite") {
        ui->comboInnerOuter->addItem("均质+均质", "Homo_Homo");
    } else if (cR == "InterlayerComposite") {
        ui->comboInnerOuter->addItem("夹层型+夹层型", "Interlayer_Interlayer");
        ui->comboInnerOuter->addItem("夹层型+均质", "Interlayer_Homo");
    } else if (cR == "ShaleComposite") {
        ui->comboInnerOuter->addItem("页岩型+页岩型", "Shale_Shale");
        ui->comboInnerOuter->addItem("页岩型+均质", "Shale_Homo");
        ui->comboInnerOuter->addItem("页岩型+夹层型", "Shale_Interlayer");
    } else if (cR == "MixedComposite") {
        ui->comboInnerOuter->addItem("三重孔隙介质+夹层型", "Triple_Interlayer");
        ui->comboInnerOuter->addItem("三重孔隙介质+页岩型", "Triple_Shale");
        ui->comboInnerOuter->addItem("三重孔隙介质+均质", "Triple_Homo");
    }

    // 设置默认停留在第一项
    if (ui->comboInnerOuter->count() > 0) {
        ui->comboInnerOuter->setCurrentIndex(0);
    }

    ui->comboInnerOuter->blockSignals(oS);
    ui->label_InnerOuter->setVisible(true);
    ui->comboInnerOuter->setVisible(true);
}

/**
 * @brief 回显设置，打开弹窗时基于现有代码将所有下拉框定位到之前选过的参数状态
 * @param c 目标模型代码 (如 "modelwidget217")
 */
void ModelSelect::setCurrentModelCode(const QString& c)
{
    m_isInitializing = true;
    QString nS = c;
    nS.remove("modelwidget");
    int id = nS.toInt();

    // 支持全矩阵 1~324 范围内的模型反演
    if (id >= 1 && id <= 324) {
        // 固定为压裂水平井
        int iW = ui->comboWellModel->findData("FracHorizontal");
        if (iW >= 0) ui->comboWellModel->setCurrentIndex(iW);

        // 【重构寻址逻辑】计算基础 108 ID，无缝映射三大裂缝形态
        int baseId = (id - 1) % 108 + 1;

        int iFrac = -1;
        if (id > 216) {
            iFrac = ui->comboFracMorphology->findData("UnequalNonUniform");
        } else if (id > 108) {
            iFrac = ui->comboFracMorphology->findData("UnequalUniform");
        } else {
            iFrac = ui->comboFracMorphology->findData("EqualUniform");
        }
        if (iFrac >= 0) {
            ui->comboFracMorphology->setCurrentIndex(iFrac);
        }

        // 依据 BaseID 反向推导模型大类和内外区组合
        QString rD, iD;
        if (baseId <= 12) { rD = "InterlayerComposite"; iD = "Interlayer_Interlayer"; }
        else if (baseId <= 24) { rD = "InterlayerComposite"; iD = "Interlayer_Homo"; }
        else if (baseId <= 36) { rD = "RadialComposite"; iD = "Homo_Homo"; }
        else if (baseId <= 48) { rD = "ShaleComposite"; iD = "Shale_Shale"; }
        else if (baseId <= 60) { rD = "ShaleComposite"; iD = "Shale_Homo"; }
        else if (baseId <= 72) { rD = "ShaleComposite"; iD = "Shale_Interlayer"; }
        else if (baseId <= 84) { rD = "MixedComposite"; iD = "Triple_Interlayer"; }
        else if (baseId <= 96) { rD = "MixedComposite"; iD = "Triple_Shale"; }
        else if (baseId <= 108){ rD = "MixedComposite"; iD = "Triple_Homo"; }

        int iR = ui->comboReservoirModel->findData(rD);
        if (iR >= 0) {
            ui->comboReservoirModel->setCurrentIndex(iR);
            updateInnerOuterOptions(); // 大类定下后主动更新二级菜单
        }

        // 推导边界条件组合 (12 个一循环)
        int gO = (baseId - 1) % 12;
        QString bD;
        if (gO < 4) bD = "Infinite";
        else if (gO < 8) bD = "Closed";
        else bD = "ConstantPressure";

        int iB = ui->comboBoundary->findData(bD);
        if (iB >= 0) ui->comboBoundary->setCurrentIndex(iB);

        // 推导井筒存储与表皮模型类型 (4 个一循环)
        int sO = (baseId - 1) % 4;
        QString sD;
        if (sO == 0) sD = "Constant";
        else if (sO == 1) sD = "LineSource";
        else if (sO == 2) sD = "Fair";
        else sD = "Hegeman";

        int iS = ui->comboStorage->findData(sD);
        if (iS >= 0) ui->comboStorage->setCurrentIndex(iS);

        // 应用推导出的内外区配置
        int iIo = ui->comboInnerOuter->findData(iD);
        if (iIo >= 0) ui->comboInnerOuter->setCurrentIndex(iIo);
    }

    m_isInitializing = false;
    onSelectionChanged(); // 应用回显并刷新 UI 显示面板
}

/**
 * @brief 每当用户修改下拉框选项时，重新生成排列组合结果并映射 ID
 */
void ModelSelect::onSelectionChanged()
{
    if (m_isInitializing) return; // 初始化防抖

    // 抓取所有当前的下拉框配置
    QString well = ui->comboWellModel->currentData().toString();
    QString res = ui->comboReservoirModel->currentData().toString();
    QString bnd = ui->comboBoundary->currentData().toString();
    QString store = ui->comboStorage->currentData().toString();
    QString io = ui->comboInnerOuter->currentData().toString();
    QString fracType = ui->comboFracMorphology->currentData().toString();

    m_selectedModelCode = "";
    m_selectedModelName = "";
    m_fracMorphology = fracType;

    // 辅助闭包：用于叠加边界和井储的微小偏移量
    auto calcID = [&](int sId, QString bT, QString sT) -> int {
        int oB = 0;
        if (bT == "Closed") oB = 4;
        else if (bT == "ConstantPressure") oB = 8;

        int oS = 0;
        if (sT == "LineSource") oS = 1;
        else if (sT == "Fair") oS = 2;
        else if (sT == "Hegeman") oS = 3;

        return sId + oB + oS;
    };

    int bSId = 0;
    QString bNCn = "";
    int solverGroup = 0;

    // 根据主大类匹配核心基底编号(间隔距均为12)
    if (well == "FracHorizontal") {
        if (res == "InterlayerComposite") {
            if (io == "Interlayer_Interlayer") { bSId = 1; bNCn = "夹层型储层试井解释模型"; solverGroup = 1; }
            else if (io == "Interlayer_Homo") { bSId = 13; bNCn = "夹层型储层试井解释模型"; solverGroup = 1; }
        } else if (res == "RadialComposite") {
            if (io == "Homo_Homo") { bSId = 25; bNCn = "径向复合模型"; solverGroup = 1; }
        } else if (res == "ShaleComposite") {
            if (io == "Shale_Shale") { bSId = 37; bNCn = "页岩型储层试井解释模型"; solverGroup = 2; }
            else if (io == "Shale_Homo") { bSId = 49; bNCn = "页岩型储层试井解释模型"; solverGroup = 2; }
            else if (io == "Shale_Interlayer") { bSId = 61; bNCn = "页岩型储层试井解释模型"; solverGroup = 2; }
        } else if (res == "MixedComposite") {
            if (io == "Triple_Interlayer") { bSId = 73; bNCn = "混积型储层试井解释模型"; solverGroup = 3; }
            else if (io == "Triple_Shale") { bSId = 85; bNCn = "混积型储层试井解释模型"; solverGroup = 3; }
            else if (io == "Triple_Homo") { bSId = 97; bNCn = "混积型储层试井解释模型"; solverGroup = 3; }
        }
    }

    if (bSId > 0) {
        int fId = calcID(bSId, bnd, store);
        int globalId = fId;
        int displayGroup = solverGroup;

        // 【扩展逻辑】依据裂缝形态计算全局 ID 和显示分组
        if (fracType == "UnequalNonUniform") {
            globalId = fId + 216;
            displayGroup = solverGroup + 6;
        } else if (fracType == "UnequalUniform") {
            globalId = fId + 108;
            displayGroup = solverGroup + 3;
        }

        m_selectedModelCode = QString("modelwidget%1").arg(globalId);

        int localId = ((fId - 1) % 36) + 1;
        QString seqNo = QString("model%1-%2").arg(displayGroup).arg(localId);

        // 拼接成详细的中文大标题显示
        m_selectedModelName = QString("%1 %2").arg(seqNo).arg(bNCn);
    }

    bool isV = !m_selectedModelCode.isEmpty();

    // 更新弹窗底部的状态提示与防呆逻辑
    if (isV) {
        ui->label_ModelName->setText(m_selectedModelName);
        ui->label_ModelName->setStyleSheet("color: black; font-weight: bold; font-size: 14px;");
    } else {
        ui->label_ModelName->setText("该组合暂无已实现模型");
        ui->label_ModelName->setStyleSheet("color: red; font-weight: normal;");
        Standard_MessageBox::warning(this, "模型组合无效", "当前所选的模型组合暂无已实现的模型，请重新选择。");
    }

    // 禁用或启用底部的 OK 按钮
    if(QPushButton* okB = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        okB->setEnabled(isV);
    }
}

/**
 * @brief 点击 OK 按下时触发的最终校验确认
 */
void ModelSelect::onAccepted() {
    if (!m_selectedModelCode.isEmpty()) {
        QString confirmMsg = QString("确认选择模型：\n\n%1\n\n模型ID: %2").arg(m_selectedModelName).arg(m_selectedModelCode);
        if (Standard_MessageBox::question(this, "确认选择", confirmMsg)) {
            accept(); // 正式下发允许通过
        }
    } else {
        Standard_MessageBox::warning(this, "提示", "请先选择有效模型");
    }
}

QString ModelSelect::getSelectedModelCode() const { return m_selectedModelCode; }
QString ModelSelect::getSelectedModelName() const { return m_selectedModelName; }
QString ModelSelect::getSelectedFracMorphology() const { return m_fracMorphology; }

/**
 * @brief 静态推演方法，基于唯一底层 ID 转换为标准界面展示的短名称
 * @param modelId 系统分配的底层 ID (1~324)
 */
QString ModelSelect::getShortName(int modelId) {
    if (modelId < 1 || modelId > 324) return "未知模型";

    // 【核心重构】映射三阶形态基底 ID (1~108)
    int baseId = (modelId - 1) % 108 + 1;

    int solverGroup = 0;
    QString bNCn;

    if (baseId <= 36) {
        solverGroup = 1;
        if(baseId <= 24) bNCn = "夹层型储层试井解释模型";
        else bNCn = "径向复合模型";
    }
    else if (baseId <= 72) {
        solverGroup = 2;
        bNCn = "页岩型储层试井解释模型";
    }
    else {
        solverGroup = 3;
        bNCn = "混积型储层试井解释模型";
    }

    int displayGroup = solverGroup;
    if (modelId > 216) {
        displayGroup += 6; // 7, 8, 9 系列
    } else if (modelId > 108) {
        displayGroup += 3; // 4, 5, 6 系列
    }

    int localId = ((baseId - 1) % 36) + 1;

    // 返回拼接好的标准化名称，例如 "model7-1 夹层型储层试井解释模型"
    return QString("model%1-%2 %3").arg(displayGroup).arg(localId).arg(bNCn);
}
