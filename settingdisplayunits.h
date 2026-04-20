#ifndef SETTINGDISPLAYUNITS_H
#define SETTINGDISPLAYUNITS_H

#include <QSettings>

class SettingDisplayUnits
{
public:
    static int pressureUnitIndex()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("units/pressure", 0).toInt();
    }

    static int rateUnitIndex()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("units/rate", 0).toInt();
    }

    static int precision()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("units/precision", 4).toInt();
    }
};

#endif // SETTINGDISPLAYUNITS_H
