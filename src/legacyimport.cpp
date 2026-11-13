// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "legacyimport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace LegacyImport {

static const char *kDoneKey = "legacy/vitabreatheImported";

static QString cleanPath(const QString &p)
{
    return QDir::cleanPath(QFileInfo(p).absoluteFilePath());
}

QString run(const QString &organization, const QString &oldApp, const QString &newApp,
            const QString &oldDataDir, const QString &newDataDir,
            const QString &oldDbName, const QString &newDbName)
{
    QSettings now(organization, newApp);
    if (now.value(QLatin1String(kDoneKey)).toBool())
        return QString();
    QSettings old(organization, oldApp);
    const QStringList oldKeys = old.allKeys();
    const QString oldDb = oldDataDir + QLatin1Char('/') + oldDbName;
    const QString newDb = newDataDir + QLatin1Char('/') + newDbName;
    QStringList done;

    // 1) Einstellungen: nur fehlende Schlüssel übernehmen
    int copied = 0;
    for (const QString &k : oldKeys) {
        if (now.contains(k)) continue;
        now.setValue(k, old.value(k));
        ++copied;
    }
    if (copied) done << QStringLiteral("%1 Einstellungen").arg(copied);

    // 2) Datenbank: nur vom alten Standardort in den neuen kopieren
    const QString dbPath = now.value(QStringLiteral("db/path")).toString();
    const bool atOldDefault = dbPath.isEmpty() || cleanPath(dbPath) == cleanPath(oldDb);
    if (atOldDefault && QFileInfo(oldDb).isFile() && QFileInfo(oldDb).size() > 0) {
        if (QFileInfo::exists(newDb)) {
            // Unter dem neuen Namen gibt es schon eine Datenbank – nicht überschreiben
            if (!dbPath.isEmpty()) now.setValue(QStringLiteral("db/path"), newDb);
        } else {
            QDir().mkpath(newDataDir);
            if (QFile::copy(oldDb, newDb)) {
                now.setValue(QStringLiteral("db/path"), newDb);
                done << QStringLiteral("Datenbank");
            } else if (!dbPath.isEmpty()) {
                // Kopieren fehlgeschlagen: an der alten Stelle weiter öffnen
                now.setValue(QStringLiteral("db/path"), oldDb);
            }
        }
    }

    now.setValue(QLatin1String(kDoneKey), true);
    now.sync();
    return done.join(QStringLiteral(", "));
}

QString run()
{
    const QString org = QCoreApplication::organizationName();
    const QString newApp = QCoreApplication::applicationName();
    const QString oldApp = QStringLiteral("VitaBreathe");

    // Datenordner beider Namen ermitteln (plattformgerecht über QStandardPaths)
    const QString newDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QCoreApplication::setApplicationName(oldApp);
    const QString oldDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QCoreApplication::setApplicationName(newApp);

    return run(org, oldApp, newApp, oldDir, newDir,
               QStringLiteral("vitabreathe.sqlite"), QStringLiteral("viviabreathe.sqlite"));
}

} // namespace LegacyImport
