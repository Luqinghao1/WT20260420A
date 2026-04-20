/*
 * 文件名: wt_datawidget.h
 * 作用: 数据编辑器主窗口头文件
 * 更新: 新增了精确对齐所需的数据表宽度获取支持，以及对悬浮窗自定义宽度的记录属性。
 */

#ifndef WT_DATAWIDGET_H
#define WT_DATAWIDGET_H

#include <QWidget>
#include <QStandardItemModel>
#include <QJsonArray>
#include <QMap>
#include <QVector>
#include <QStringList>
#include <QIcon>
#include "datasinglesheet.h"
#include "datasamplingdialog.h"

namespace Ui {
class WT_DataWidget;
}

class WT_DataWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WT_DataWidget(QWidget *parent = nullptr);
    ~WT_DataWidget();

    void clearAllData();
    void loadFromProjectData();
    QStandardItemModel* getDataModel() const;
    QMap<QString, QStandardItemModel*> getAllDataModels() const;
    void loadData(const QString& filePath, const QString& fileType = "auto");
    QString getCurrentFileName() const;
    bool hasData() const;

signals:
    void dataChanged();
    void fileChanged(const QString& filePath, const QString& fileType);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onOpenFile();
    void onSave();
    void onExportExcel();
    void onUnitManager();
    void onTimeConvert();
    void onPressureDropCalc();
    void onCalcPwf();
    void onFilterSample();
    void onHighlightErrors();

    // 控制面板响应槽
    void onSamplingMaximize();
    void onSamplingRestore();
    void onSamplingClose();
    void onSamplingResized(int dx); // 【新增】处理面板左边缘被手动拉伸的宽度重算
    void onSamplingFinished(const QStringList& headers, const QVector<QStringList>& processedTable);

    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void onSheetDataChanged();

private:
    Ui::WT_DataWidget *ui;

    DataSamplingWidget* m_samplingWidget;
    bool m_isSamplingMaximized;
    int m_floatingWidth; // 【新增】记录悬浮窗的有效宽度（-1表示随表格空白区自动吸附）

    void initUI();
    void setupConnections();
    void updateButtonsState();
    void createNewTab(const QString& filePath, const DataImportSettings& settings);
    DataSingleSheet* currentSheet() const;

    // 核心计算函数：根据当前表格真实宽度或用户自定义宽度更新面板坐标
    void updateSamplingWidgetGeometry();

    void saveAndLoadNewData(const QString& oldFilePath, const QStringList& headers, const QVector<QStringList>& fullTable);
    QIcon createCustomIcon(const QString& iconType);
};

#endif // WT_DATAWIDGET_H
