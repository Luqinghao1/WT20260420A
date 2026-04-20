#ifndef SETTINGPATHS_H
#define SETTINGPATHS_H

#include <QSettings>
#include <QStandardPaths>
#include <QString>

class SettingPaths
{
public:
    static QString dataPath()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        QString docPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        return settings.value("paths/data", docPath + "/WellTestPro/Data").toString();
    }

    static QString reportPath()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        QString docPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        return settings.value("paths/report", docPath + "/WellTestPro/Reports").toString();
    }

    static QString backupPath()
    {
        QSettings settings("WellTestPro", "WellTestAnalysis");
        QString docPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        return settings.value("paths/backup", docPath + "/WellTestPro/Backups").toString();
    }
};

#endif // SETTINGPATHS_H
