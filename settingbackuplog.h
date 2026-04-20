#ifndef SETTINGBACKUPLOG_H
#define SETTINGBACKUPLOG_H

#include <QSettings>

class SettingBackupLog
{
public:
    static bool backupEnabled()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("system/backupEnabled", true).toBool();
    }

    static int maxBackups()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("system/maxBackups", 10).toInt();
    }

    static bool cleanupLogs()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("system/cleanupLogs", true).toBool();
    }

    static int logRetentionDays()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("system/logRetention", 30).toInt();
    }

    static int logLevel()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        return settings.value("system/logLevel", 2).toInt();
    }
};

#endif // SETTINGBACKUPLOG_H
