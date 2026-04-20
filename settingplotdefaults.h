#ifndef SETTINGPLOTDEFAULTS_H
#define SETTINGPLOTDEFAULTS_H

#include <QSettings>

class SettingPlotDefaults
{
public:
    static int backgroundStyle()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("plot/background", 0).toInt();
    }

    static bool showGrid()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("plot/showGrid", true).toBool();
    }

    static bool showTitle()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("plot/showTitle", true).toBool();
    }

    static bool showLegend()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("plot/showLegend", true).toBool();
    }

    static int lineWidth()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("plot/lineWidth", 2).toInt();
    }
};

#endif // SETTINGPLOTDEFAULTS_H
