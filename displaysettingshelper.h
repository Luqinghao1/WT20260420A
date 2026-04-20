#ifndef DISPLAYSETTINGSHELPER_H
#define DISPLAYSETTINGSHELPER_H

#include <QString>
#include "settingdisplayunits.h"

class DisplaySettingsHelper
{
public:
    static QString preferredPressureUnit()
    {
        switch (SettingDisplayUnits::pressureUnitIndex()) {
        case 1: return "psi";
        case 2: return "bar";
        default: return "MPa";
        }
    }

    static QString preferredRateUnit()
    {
        switch (SettingDisplayUnits::rateUnitIndex()) {
        case 1: return "bbl/d";
        case 2: return "t/d";
        default: return "m³/d";
        }
    }

    static int preferredPrecision()
    {
        return SettingDisplayUnits::precision();
    }

    static QString formatNumber(double value)
    {
        return QString::number(value, 'f', preferredPrecision());
    }
};

#endif // DISPLAYSETTINGSHELPER_H
