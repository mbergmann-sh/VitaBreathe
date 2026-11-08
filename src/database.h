// VitaBreathe – Datenhaltung (SQLite3 Multiple Ciphers, mit dem Master-Passwort verschlüsselt)
//
// Die gesamte Datenbank wird mit dem Master-Passwort verschlüsselt (ChaCha20-Poly1305,
// Schlüsselableitung durch SQLite3 Multiple Ciphers). Ohne das Passwort ist sie unlesbar.
// Es gibt bewusst keine Hintertür: Ein vergessenes Master-Passwort lässt sich nicht
// wiederherstellen – Sicherungen sind mit demselben Passwort verschlüsselt.
//
// Abgleich zwischen Geräten (siehe dbsync.h): Jede abgeglichene Zeile trägt eine
// geräteübergreifende Kennung (uid) und den Zeitpunkt der letzten Änderung (sync_at, UTC);
// Löschungen hinterlassen Löschvermerke (sync_deleted). Gepflegt wird das von Triggern.
// SPDX-License-Identifier: MIT
#pragma once

#include "datatypes.h"
#include "sqlquery.h"

#include <QJsonObject>
#include <QObject>
#include <QVariant>

#include <optional>

struct sqlite3;

class Database : public QObject
{
    Q_OBJECT
public:
    static Database &instance();
    static constexpr int kSchemaVersion = 3;   // 2: Einschlaf-Einträge (sleep_plans), 3: Atemtrainer-Ergebnisse

    enum class OpenResult { Ok, WrongPassword, Error };
    OpenResult open(const QString &path, const QString &password, QString *error = nullptr);
    bool changePassword(const QString &newPassword, QString *error = nullptr);
    bool verifyPassword(const QString &password) const { return password == m_password; }
    void close();
    bool isOpen() const { return m_db != nullptr; }
    QString path() const { return m_path; }
    QString password() const { return m_password; }
    static bool fileHasContent(const QString &path);
    static QString defaultPath();

    // Sicherung als konsistente, ebenso verschlüsselte Kopie (SQLite-Backup-API)
    bool backupTo(const QString &target, QString *error = nullptr);

    // Benutzer
    QList<User> users();
    std::optional<User> user(qint64 id);
    qint64 addUser(const QString &name, QString *error = nullptr);
    bool renameUser(qint64 id, const QString &name, QString *error = nullptr);
    bool deleteUser(qint64 id, QString *error = nullptr);   // samt aller Daten

    // Einstellungen je Benutzer (abgeglichen) – Werte als Text oder JSON
    QString userSetting(qint64 userId, const QString &key, const QString &def = {});
    void setUserSetting(qint64 userId, const QString &key, const QString &value);
    QJsonObject userJson(qint64 userId, const QString &key);
    void setUserJson(qint64 userId, const QString &key, const QJsonObject &o);
    void removeUserSetting(qint64 userId, const QString &key);

    // Einstellungen dieses Geräts (nicht abgeglichen, aber verschlüsselt)
    QString deviceSetting(const QString &key, const QString &def = {});
    void setDeviceSetting(const QString &key, const QString &value);

    // Übungseinheiten (Tagebuch)
    QList<SessionRecord> sessions(qint64 userId, const QDateTime &from = {}, const QDateTime &to = {});
    bool saveSession(SessionRecord &s, QString *error = nullptr);
    bool deleteSession(qint64 id, QString *error = nullptr);
    QStringList eventNames(qint64 userId);   // bisher verwendete Anlässe + Vorschläge

    // Ereignisse mit Erinnerung (userId = 0: alle Benutzer)
    QList<BreathEvent> events(qint64 userId = 0);
    bool saveEvent(BreathEvent &e, QString *error = nullptr);
    bool deleteEvent(qint64 id, QString *error = nullptr);

    // Wecker (userId = 0: alle Benutzer)
    QList<Alarm> alarms(qint64 userId = 0);
    bool saveAlarm(Alarm &a, QString *error = nullptr);
    bool deleteAlarm(qint64 id, QString *error = nullptr);

    // Einschlaffunktion (userId = 0: alle Benutzer)
    QList<SleepPlan> sleepPlans(qint64 userId = 0);
    bool saveSleepPlan(SleepPlan &p, QString *error = nullptr);
    bool deleteSleepPlan(qint64 id, QString *error = nullptr);

    // Abgleich
    struct SyncTable { const char *name; const char *parentColumn; };
    static const QList<SyncTable> &syncTables();
    qint64 syncChangeCounter();
    SqlDb handleDb() const { return SqlDb(m_db); }
    static int schemaVersionOf(SqlDb &d, const QString &schema);

signals:
    void changed();            // Daten wurden geändert (UI aktualisieren)
    void usersChanged();

private:
    Database() = default;
    SqlDb db() const { return SqlDb(m_db); }
    bool createSchema(QString *error);
    bool ensureSyncSchema(QString *error);
    bool ensureColumn(const QString &table, const QString &column, const QString &type, QString *error);
    void migrateSleepSettings();     // Schema 1 → 2: Einstellung "sleep" → erster Eintrag

    sqlite3 *m_db = nullptr;
    QString  m_path;
    QString  m_password;
};

QString vbVersion();
