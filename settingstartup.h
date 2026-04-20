#ifndef SETTINGSTARTUP_H
#define SETTINGSTARTUP_H

#include <QSettings>

class SettingStartup
{
public:
    static int themeIndex()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("general/theme", 0).toInt();
    }

    static bool startFullScreen()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("general/fullScreen", false).toBool();
    }

};

#endif // SETTINGSTARTUP_H
