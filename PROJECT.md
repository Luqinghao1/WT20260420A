# PWT - 页岩油多段压裂水平井试井解释软件

## 项目概述

**PWT** (WellTest) 是一个基于 **Qt6 C++** 的桌面试井解释软件，专门用于页岩油多段压裂水平井的压力数据分析。该软件实现了从数据导入、图表绘制、理论模型生成到曲线拟合反演的完整试井解释工作流。

## 技术栈

| 组件 | 版本/说明 |
|------|----------|
| Qt | 6.x (core, gui, widgets, axcontainer, svg, printsupport, core5compat, concurrent) |
| C++ | C++17, -O3 优化 |
| Eigen | 3.3.8 (矩阵/线性代数) |
| Boost | 1.89.0 (数学函数) |
| QXlsx | Excel 读写 |
| QCustomPlot | 内置 (~1.3MB) |
| 构建系统 | qmake (.pro) |

## 核心功能

- **数据导入**: 通过 QXlsx + ActiveX 从 Excel 导入压力/产量数据
- **压力导数计算**: Bourdet 算法 + 平滑处理
- **理论曲线模型**: 108 种模型，使用 Stehfest 数值拉普拉斯反演
- **曲线拟合**: Levenberg-Marquardt 非线性回归优化
  - 安全残差机制（log-floor 策略，防止理论曲线消失时梯度归零）
  - 多收敛判据（绝对阈值 + 相对变化 + 停滞检测 + 连续失败容忍）
  - 多起点预扫描（5 个随机扰动起点中选最佳后进入正式优化）
  - 数据驱动智能初值估算（从导数平台、单位斜率段、凹陷特征自动推导 kf/C/S/ω/λ）
  - 拟合质量评价（实时迭代趋势显示 + 拟合完成后 R²/MSE/质量评级报告）
- **多数据联合拟合**: 多组试井数据（如多次恢复试井）独立残差拼接法同时拟合
  - 各组数据独立采样后拼接，不做破坏性平均
  - 导数曲线完整参与拟合，压力/导数权重可调
  - 分组误差报告（每组独立 MSE/R²/质量评级）
- **图表绘制**: Log-log、半对数、笛卡尔坐标图 (QCustomPlot)
- **正演验证**: 理论曲线无效点比例超 30% 时自动警告

## 项目架构

```
MainWindow (QMainWindow) ── 7 页堆叠布局 + 侧边栏导航
├── Page 0: WT_ProjectWidget    (项目管理 - 创建/打开/关闭项目)
├── Page 1: WT_DataWidget       (数据编辑 - 导入/编辑/采样/单位转换)
├── Page 2: WT_PlottingWidget   (图表分析 - 试井分析曲线)
├── Page 3: ModelManager        (模型管理 - 108种理论曲线模型)
│   ├── WT_ModelWidget[108]     (每种模型的UI)
│   ├── ModelSolver01[36]       (模型 1-36 求解器)
│   ├── ModelSolver02[36]       (模型 37-72 求解器)
│   └── ModelSolver03[36]       (模型 73-108 求解器)
├── Page 4: FittingPage         (拟合分析 - LM 优化 + 3图布局)
│   ├── FittingWidget[]         (单个拟合分析)
│   │   ├── FittingCore         (Levenberg-Marquardt 优化引擎)
│   │   ├── FittingChart        (图表管理: log-log/半对数/笛卡尔)
│   │   └── FittingParameterChart (参数表)
│   └── FittingMultiplesWidget  (多数据集对比)
├── Page 5: multidatafittingpage (多数据拟合)
└── Page 6: SettingsWidget      (系统设置)
```

## 模块详解

### 1. 项目管理 (Page 0)
- `wt_projectwidget.cpp/.h/.ui` - 项目管理主页面
- `newprojectdialog.cpp/.h/.ui` - 新建项目向导 (3步: 基本信息/井参数/PVT参数)

### 2. 数据编辑 (Page 1)
- `wt_datawidget.cpp/.h/.ui` - 数据编辑主控件
- `datasinglesheet.cpp/.h/.ui` - 单数据表
- `dataimportdialog.cpp/.h/.ui` - 数据导入对话框
- `dataunitdialog.cpp/.h/.ui` - 单位转换对话框
- `dataunitmanager.cpp/.h` - 单位管理逻辑
- `datacalculate.cpp/.h` - 时间转换/压降/Pwf计算
- `pressurederivativecalculator.cpp/.h` - Bourdet 压力导数算法
- `pressurederivativecalculator1.cpp/.h` - 高级压力导数计算器 (平滑处理)
- `datasamplingdialog.cpp/.h` - 数据采样/滤波对话框

### 3. 图表分析 (Page 2)
- `wt_plottingwidget.cpp/.h/.ui` - 图表分析主控件
- `chartwidget.cpp/.h/.ui` - 通用图表控件
- `chartwindow.cpp/.h/.ui` - 图表窗口容器
- `chartsetting1.cpp/.h/.ui` - 图表设置 (坐标轴/网格/外观)
- `chartsetting2.cpp/.h/.ui` - 扩展图表设置
- `plottingdialog1-4.cpp/.h/.ui` - 绘图配置对话框 (4阶段)
- `styleselectordialog.cpp/.h/.ui` - 线条样式/颜色/宽度选择器
- `mousezoom.cpp/.h` - 增强型 QCustomPlot (鼠标缩放)

### 4. 模型管理 (Page 3)
- `modelmanager.cpp/.h` - 模型管理中枢 (108种模型调度)
- `wt_modelwidget.cpp/.h/.ui` - 模型分析UI
- `modelparameter.cpp/.h` - 全局参数单例 (Level-1 存储)
- `modelselect.cpp/.h/.ui` - 模型选择对话框 (108模型网格)
- `modelsolver01.cpp/.h` - 模型 1-36 求解器 (层间/复合)
- `modelsolver02.cpp/.h` - 模型 37-72 求解器
- `modelsolver03.cpp/.h` - 模型 73-108 求解器

### 5. 拟合分析 (Page 4)
- `wt_fittingwidget.cpp/.h/.ui` - 拟合主控件
- `fittingpage.cpp/.h/.ui` - 拟合标签页容器
- `fittingcore.cpp/.h` - Levenberg-Marquardt 优化引擎
  - 安全残差 (log-floor 策略，消除梯度黑洞)
  - 多收敛判据 (绝对阈值 + 相对变化 + 停滞检测 + 连续失败容忍)
  - 多起点预扫描 (5 个随机扰动起点，选最佳后正式优化)
  - 数据驱动智能初值估算 (estimateInitialParams)
- `fittingchart.cpp/.h` - 拟合图表管理器
- `fittingchart1.cpp/.h/.ui` - Log-log 图表
- `fittingchart2.cpp/.h/.ui` - 半对数图表
- `fittingchart3.cpp/.h/.ui` - 笛卡尔图表
- `fittingparameterchart.cpp/.h` - 拟合参数表 (108模型)
- `fittingdatadialog.cpp/.h/.ui` - 拟合数据设置
- `fittingnewdialog.cpp/.h/.ui` - 新建拟合分析
- `fittingmultiples.cpp/.h/.ui` - 多数据集对比
- `fittingpressuredialog.cpp/.h/.ui` - 压力求解器
- `fittingreport.cpp/.h` - 拟合报告生成器
- `fittingsamplingdialog.cpp/.h` - 拟合采样设置
- `paramselectdialog.cpp/.h/.ui` - 参数选择对话框

### 6. 多数据联合拟合 (Page 5)
- `wt_multidatafittingwidget.cpp/.h/.ui` - 多数据拟合核心控件
  - 独立残差拼接法 (各组独立采样拼接，导数完整参与拟合)
  - 压力/导数权重可调，组间权重通过采样点数比例控制
  - 三图完整显示 (log-log/半对数/笛卡尔)
  - 拟合完成后分组误差报告 (每组独立 MSE/R²/质量评级)
- `multidatafittingpage.cpp/.h/.ui` - 多数据拟合页面容器

### 7. 系统设置 (Page 6)
- `settingswidget.cpp/.h/.ui` - 设置页面 (单位/路径/自动保存/绘图样式)

### UI 工具组件
- `navbtn.cpp/.h/.ui` - 侧边栏导航按钮
- `monitorbtn.cpp/.h/.ui` - 监控按钮
- `monitostatew.cpp/.h/.ui` - 监控状态控件
- `standard_messagebox.cpp/.h` - 自定义标准消息框
- `standard_toolbutton.cpp/.h` - 自定义标准工具按钮

## 关键数据流

```
NewProjectDialog ──收集井参数──> ModelParameter (单例)
WT_DataWidget ──导入/编辑数据──> MainWindow ──传递──> WT_PlottingWidget + FittingPage
ModelManager ──调度计算──> ModelSolver01/02/03 (Stehfest反演)
FittingCore ──LM优化迭代──> ModelManager::calculateTheoreticalCurve()
ModelParameter ──持久化──> .pwt JSON 项目文件
```

## 文件清单

### 构建文件
- `WellTest.pro` - qmake 主项目文件
- `resource.qrc` - Qt 资源文件

### 入口文件
- `main.cpp` - 应用程序入口
- `mainwindow.cpp/.h/.ui` - 主窗口

### 资源目录
- `Resource/` - 图标和背景图片 (PWT.ico, X0-X6.png, PRO1-4.png, background*.png/svg, jiazai*.png)

### 部署目录
- `FABU/` - 已编译的可执行文件 (~225MB) 和 Qt6 运行时

### 参考代码
- `z_matlab/` - MATLAB 参考脚本 (模型求解的数学基础)

### 遗留文件 (可安全删除)


**Qt Creator 用户配置文件** (本地构建环境相关，不应纳入版本控制)：
- `WellTest.pro.user` - 当前 Qt Creator 用户设置
- `WellTest.pro.user.59494c7` - 历史用户设置备份
- `WellTest.pro.user.7a0c892` - 历史用户设置备份

**根目录编译产物** (应在 build/ 下生成，根目录残留是旧构建方式遗留)：
- `Makefile` / `Makefile.Debug` / `Makefile.Release` - 根目录的 Makefile
- `.qmake.stash` - qmake 缓存
- `WellTest_resource.rc` - 资源编译中间文件

**旧编译输出目录** (新构建已统一到 build/ 下)：
- `release/` - 旧 release 编译产物目录 (含已删除类 ParameterRecommendationDialog 的残留 .o/.cpp)
- `debug/` - 旧 debug 编译产物目录

**Qt 工具缓存**：
- `.qtc_clangd/` - Qt Creator clangd 索引缓存

## 构建说明

```bash
# 使用 qmake 构建
qmake WellTest.pro
make          # 或 nmake (MSVC) / mingw32-make (MinGW)
```

依赖库路径在 `.pro` 文件中指定为 `D:/08YYYXXX/`，需确保:
- `D:/08YYYXXX/Eigen/` - Eigen 3.3.8
- `D:/08YYYXXX/boost_1_89_0/` - Boost 1.89.0
- `D:/08YYYXXX/QXlsx/` - QXlsx 库

## UI 文件列表 (共35个)

`mainwindow.ui`, `wt_projectwidget.ui`, `newprojectdialog.ui`, `wt_datawidget.ui`, `datasinglesheet.ui`, `dataimportdialog.ui`, `dataunitdialog.ui`, `wt_plottingwidget.ui`, `chartwidget.ui`, `chartwindow.ui`, `chartsetting1.ui`, `chartsetting2.ui`, `plottingdialog1.ui`, `plottingdialog2.ui`, `plottingdialog3.ui`, `plottingdialog4.ui`, `styleselectordialog.ui`, `wt_modelwidget.ui`, `modelselect.ui`, `wt_fittingwidget.ui`, `fittingpage.ui`, `fittingchart1.ui`, `fittingchart2.ui`, `fittingchart3.ui`, `fittingdatadialog.ui`, `fittingnewdialog.ui`, `fittingmultiples.ui`, `fittingpressuredialog.ui`, `paramselectdialog.ui`, `multidatafittingpage.ui`, `wt_multidatafittingwidget.ui`, `settingswidget.ui`, `navbtn.ui`, `monitorbtn.ui`, `monitostatew.ui`
