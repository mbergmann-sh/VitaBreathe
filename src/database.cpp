// SPDX-License-Identifier: MIT
#include "database.h"
#include "qtcompat.h"

#include "sqlite3mc.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

QString vbVersion()
{
#ifdef VITABREATHE_VERSION
    return QStringLiteral(VITABREATHE_VERSION);
#else
    return QStringLiteral("0.0");
#endif
}

namespace Weekdays {
QString text(int mask)
{
    mask &= kAll;
    if (mask == kAll) return QCoreApplication::translate("Weekdays", "täglich");
    if (mask == kWorkdays) return QCoreApplication::translate("Weekdays", "werktags");
    if (mask == 0x60) return QCoreApplication::translate("Weekdays", "am Wochenende");
    if (mask == 0) return QCoreApplication::translate("Weekdays", "nie");
    static const char *names[] = { QT_TRANSLATE_NOOP("Weekdays", "Mo"), QT_TRANSLATE_NOOP("Weekdays", "Di"),
                                   QT_TRANSLATE_NOOP("Weekdays", "Mi"), QT_TRANSLATE_NOOP("Weekdays", "Do"),
                                   QT_TRANSLATE_NOOP("Weekdays", "Fr"), QT_TRANSLATE_NOOP("Weekdays", "Sa"),
                                   QT_TRANSLATE_NOOP("Weekdays", "So") };
    QStringList l;
    for (int i = 0; i < 7; ++i)
        if (mask & (1 << i)) l << QCoreApplication::translate("Weekdays", names[i]);
    return l.join(QStringLiteral(", "));
}
}

namespace {
bool fail(const SqlQuery &q, QString *error)
{
    if (error) *error = q.lastError().text();
    return false;
}
QString iso(const QDateTime &dt) { return dt.toString(Qt::ISODate); }
QDateTime fromIso(const QVariant &v) { return QDateTime::fromString(v.toString(), Qt::ISODate); }
QVariant opt(int v) { return v >= 0 ? QVariant(v) : QVariant(); }
int optInt(const QVariant &v) { return vpIsNull(v) ? kNoValue : v.toInt(); }
QString syncNow() { return QStringLiteral("strftime('%Y-%m-%dT%H:%M:%f','now')"); }
} // namespace

Database &Database::instance()
{
    static Database *inst = new Database;
    return *inst;
}

QString Database::defaultPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/vitabreathe.sqlite");
}

bool Database::fileHasContent(const QString &path)
{
    return QFileInfo(path).exists() && QFileInfo(path).size() > 0;
}

Database::OpenResult Database::open(const QString &path, const QString &password, QString *error)
{
    close();
    sqlite3 *h = nullptr;
    int rc = sqlite3_open_v2(QFile::encodeName(path).constData(), &h,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        if (error) *error = h ? QString::fromUtf8(sqlite3_errmsg(h)) : tr("Datei kann nicht geöffnet werden.");
        sqlite3_close(h);
        return OpenResult::Error;
    }
    sqlite3_busy_timeout(h, 5000);
    const QByteArray key = password.toUtf8();
    sqlite3_key_v2(h, "main", key.constData(), int(key.size()));
    rc = sqlite3_exec(h, "SELECT count(*) FROM sqlite_master", nullptr, nullptr, nullptr);
    if (rc == SQLITE_NOTADB) {
        sqlite3_close(h);
        if (error) *error = tr("Falsches Master-Passwort oder keine VitaBreathe-Datenbank.");
        return OpenResult::WrongPassword;
    }
    if (rc != SQLITE_OK) {
        if (error) *error = QString::fromUtf8(sqlite3_errmsg(h));
        sqlite3_close(h);
        return OpenResult::Error;
    }
    m_db = h;
    m_path = path;
    m_password = password;
    SqlDb d = db();
    d.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    d.exec(QStringLiteral("PRAGMA journal_mode = DELETE"));   // rekey braucht Rollback-Journal
    d.exec(QStringLiteral("PRAGMA secure_delete = ON"));
    d.exec(QStringLiteral("PRAGMA temp_store = MEMORY"));
    if (!createSchema(error)) {
        close();
        return OpenResult::Error;
    }
    return OpenResult::Ok;
}

bool Database::changePassword(const QString &newPassword, QString *error)
{
    if (!m_db) return false;
    const QByteArray key = newPassword.toUtf8();
    const int rc = sqlite3_rekey_v2(m_db, "main", key.constData(), int(key.size()));
    if (rc != SQLITE_OK) {
        if (error) *error = QString::fromUtf8(sqlite3_errmsg(m_db));
        return false;
    }
    m_password = newPassword;
    return true;
}

void Database::close()
{
    if (!m_db) return;
    sqlite3_close_v2(m_db);
    m_db = nullptr;
    m_password.fill(QLatin1Char('\0'));
    m_password.clear();
}

bool Database::backupTo(const QString &target, QString *error)
{
    if (!m_db) return false;
    if (QFile::exists(target) && !QFile::remove(target)) {
        if (error) *error = tr("Zieldatei kann nicht überschrieben werden.");
        return false;
    }
    sqlite3 *t = nullptr;
    if (sqlite3_open_v2(QFile::encodeName(target).constData(), &t,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        if (error) *error = t ? QString::fromUtf8(sqlite3_errmsg(t)) : tr("Zieldatei kann nicht angelegt werden.");
        sqlite3_close(t);
        return false;
    }
    const QByteArray key = m_password.toUtf8();
    sqlite3_key_v2(t, "main", key.constData(), int(key.size()));
    sqlite3_backup *b = sqlite3_backup_init(t, "main", m_db, "main");
    int rc = SQLITE_ERROR;
    if (b) {
        rc = sqlite3_backup_step(b, -1);
        sqlite3_backup_finish(b);
    }
    const bool ok = (rc == SQLITE_DONE) && sqlite3_errcode(t) == SQLITE_OK;
    if (!ok && error) *error = QString::fromUtf8(sqlite3_errmsg(t));
    sqlite3_close(t);
    return ok;
}

int Database::schemaVersionOf(SqlDb &d, const QString &schema)
{
    SqlQuery q(d);
    if (q.exec(QStringLiteral("SELECT value FROM %1.meta WHERE key = 'schema'").arg(schema)) && q.next())
        return q.value(0).toInt();
    return 0;
}

// ---------------------------------------------------------------- Schema

bool Database::createSchema(QString *error)
{
    SqlQuery q(db());
    const QStringList ddl = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS device (key TEXT PRIMARY KEY, value TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS users ("
                       " id INTEGER PRIMARY KEY, name TEXT NOT NULL, created TEXT,"
                       " uid TEXT, sync_at TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS user_settings ("
                       " id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
                       " key TEXT NOT NULL, value TEXT, uid TEXT, sync_at TEXT, UNIQUE (user_id, key))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS sessions ("
                       " id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
                       " module TEXT NOT NULL, event TEXT, started TEXT NOT NULL,"
                       " planned_sec INTEGER, actual_sec INTEGER, cycles INTEGER, completed INTEGER, pattern TEXT,"
                       " spo2_before INTEGER, spo2_after INTEGER, pf_before INTEGER, pf_after INTEGER,"
                       " pulse_before INTEGER, pulse_after INTEGER, feeling_before INTEGER, feeling_after INTEGER,"
                       " note TEXT, uid TEXT, sync_at TEXT)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id, started)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS events ("
                       " id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
                       " name TEXT NOT NULL, module TEXT, time TEXT, weekdays INTEGER, enabled INTEGER, sound INTEGER,"
                       " note TEXT, uid TEXT, sync_at TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS sleep_plans ("
                       " id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
                       " label TEXT, time TEXT, weekdays INTEGER, scheduled INTEGER, auto_start INTEGER, playlist TEXT,"
                       " shuffle INTEGER, volume INTEGER, minutes INTEGER, fade_out INTEGER, module TEXT, uid TEXT, sync_at TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS alarms ("
                       " id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
                       " label TEXT, time TEXT, weekdays INTEGER, enabled INTEGER, playlist TEXT, shuffle INTEGER,"
                       " volume INTEGER, fade_in INTEGER, snooze INTEGER, module TEXT, uid TEXT, sync_at TEXT)"),
    };
    for (const QString &s : ddl)
        if (!q.exec(s)) return fail(q, error);
    SqlDb handle = db();
    const int current = schemaVersionOf(handle, QStringLiteral("main"));
    if (current > kSchemaVersion) {
        if (error) *error = tr("Die Datenbank stammt von einer neueren VitaBreathe-Version (Format %1). "
                               "Bitte VitaBreathe aktualisieren.").arg(current);
        return false;
    }
    // Schema 3: Ergebnisse des Atemtrainers (vor den Abgleich-Triggern, die alle Spalten beobachten)
    if (!ensureColumn(QStringLiteral("sessions"), QStringLiteral("device_balls"), QStringLiteral("INTEGER"), error)
        || !ensureColumn(QStringLiteral("sessions"), QStringLiteral("device_hold"), QStringLiteral("INTEGER"), error))
        return false;
    if (!ensureSyncSchema(error)) return false;
    if (current < 2) migrateSleepSettings();
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO meta (key, value) VALUES ('schema', ?)"));
    q.addBindValue(kSchemaVersion);
    if (!q.exec()) return fail(q, error);
    return true;
}

bool Database::ensureColumn(const QString &table, const QString &column, const QString &type, QString *error)
{
    SqlQuery q(db());
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) return fail(q, error);
    while (q.next())
        if (q.value(1).toString() == column) return true;
    q.finish();
    if (!q.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, type))) return fail(q, error);
    return true;
}

const QList<Database::SyncTable> &Database::syncTables()
{
    static const QList<SyncTable> t = {
        { "users",         nullptr },
        { "user_settings", "user_id" },
        { "sessions",      "user_id" },
        { "events",        "user_id" },
        { "alarms",        "user_id" },
        { "sleep_plans",   "user_id" },
    };
    return t;
}

bool Database::ensureSyncSchema(QString *error)
{
    SqlQuery q(db());
    const QString now = syncNow();
    if (!q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS sync_deleted (uid TEXT PRIMARY KEY, tbl TEXT NOT NULL, at TEXT NOT NULL)"))
        || !q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS sync_state (id INTEGER PRIMARY KEY CHECK (id = 1),"
                                  " changes INTEGER NOT NULL DEFAULT 0, suspend INTEGER NOT NULL DEFAULT 0)"))
        || !q.exec(QStringLiteral("INSERT OR IGNORE INTO sync_state (id, changes, suspend) VALUES (1, 0, 0)")))
        return fail(q, error);
    const QString bump = QStringLiteral(" UPDATE sync_state SET changes = changes + 1 WHERE id = 1;");
    for (const SyncTable &t : syncTables()) {
        const QString tbl = QString::fromLatin1(t.name);
        // Einstellungen: feste Kennung (Benutzer#Schlüssel), damit beide Geräte dieselbe Zeile meinen
        QString newUid = QStringLiteral("lower(hex(randomblob(16)))");
        if (tbl == QLatin1String("user_settings"))
            newUid = QStringLiteral("(SELECT u.uid FROM users u WHERE u.id = NEW.user_id) || '#' || NEW.key");
        if (!q.exec(QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_%1_uid ON %1(uid)").arg(tbl)))
            return fail(q, error);
        QStringList cols;
        {
            SqlQuery c(db());
            c.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tbl));
            while (c.next()) {
                const QString n = c.value(1).toString();
                if (n == QLatin1String("id") || n == QLatin1String("uid") || n == QLatin1String("sync_at")) continue;
                cols << QStringLiteral("NEW.%1 IS NOT OLD.%1").arg(n);
            }
        }
        const QStringList trig = {
            QStringLiteral("DROP TRIGGER IF EXISTS vb_sync_ins_%1").arg(tbl),
            QStringLiteral("DROP TRIGGER IF EXISTS vb_sync_upd_%1").arg(tbl),
            QStringLiteral("DROP TRIGGER IF EXISTS vb_sync_del_%1").arg(tbl),
            QStringLiteral("CREATE TRIGGER vb_sync_ins_%1 AFTER INSERT ON %1 BEGIN"
                           " UPDATE %1 SET uid = coalesce(NEW.uid, %2), sync_at = coalesce(NEW.sync_at, %3)"
                           " WHERE rowid = NEW.rowid;"
                           " DELETE FROM sync_deleted WHERE uid = (SELECT uid FROM %1 WHERE rowid = NEW.rowid);%4 END")
                .arg(tbl, newUid, now, bump),
            QStringLiteral("CREATE TRIGGER vb_sync_upd_%1 AFTER UPDATE ON %1 WHEN NEW.sync_at IS OLD.sync_at AND (%2)"
                           " AND (SELECT suspend FROM sync_state WHERE id = 1) = 0 BEGIN"
                           " UPDATE %1 SET sync_at = %3 WHERE rowid = NEW.rowid;%4 END")
                .arg(tbl, cols.join(QStringLiteral(" OR ")), now, bump),
            QStringLiteral("CREATE TRIGGER vb_sync_del_%1 AFTER DELETE ON %1 WHEN OLD.uid IS NOT NULL BEGIN"
                           " INSERT OR REPLACE INTO sync_deleted (uid, tbl, at) VALUES (OLD.uid, '%1', %2);%3 END")
                .arg(tbl, now, bump),
        };
        for (const QString &s : trig)
            if (!q.exec(s)) return fail(q, error);
    }
    return true;
}

qint64 Database::syncChangeCounter()
{
    SqlQuery q(db());
    if (q.exec(QStringLiteral("SELECT changes FROM sync_state WHERE id = 1")) && q.next())
        return q.value(0).toLongLong();
    return 0;
}

// ---------------------------------------------------------------- Benutzer

QList<User> Database::users()
{
    QList<User> l;
    SqlQuery q(db());
    if (q.exec(QStringLiteral("SELECT id, uid, name FROM users ORDER BY name COLLATE NOCASE")))
        while (q.next()) l << User{ q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toString() };
    return l;
}

std::optional<User> Database::user(qint64 id)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("SELECT id, uid, name FROM users WHERE id = ?"));
    q.addBindValue(id);
    if (q.exec() && q.next()) return User{ q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toString() };
    return std::nullopt;
}

qint64 Database::addUser(const QString &name, QString *error)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("INSERT INTO users (name, created) VALUES (?, ?)"));
    q.addBindValue(name.trimmed());
    q.addBindValue(iso(QDateTime::currentDateTime()));
    if (!q.exec()) { fail(q, error); return 0; }
    const qint64 id = q.lastInsertId().toLongLong();
    emit usersChanged();
    emit changed();
    return id;
}

bool Database::renameUser(qint64 id, const QString &name, QString *error)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE users SET name = ? WHERE id = ?"));
    q.addBindValue(name.trimmed());
    q.addBindValue(id);
    if (!q.exec()) return fail(q, error);
    emit usersChanged();
    return true;
}

bool Database::deleteUser(qint64 id, QString *error)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM users WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) return fail(q, error);
    emit usersChanged();
    emit changed();
    return true;
}

// ---------------------------------------------------------------- Einstellungen

QString Database::userSetting(qint64 userId, const QString &key, const QString &def)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("SELECT value FROM user_settings WHERE user_id = ? AND key = ?"));
    q.addBindValue(userId);
    q.addBindValue(key);
    if (q.exec() && q.next()) return q.value(0).toString();
    return def;
}

void Database::setUserSetting(qint64 userId, const QString &key, const QString &value)
{
    if (userSetting(userId, key, QStringLiteral("\x01")) == value) return;
    SqlQuery q(db());
    q.prepare(QStringLiteral("INSERT INTO user_settings (user_id, key, value) VALUES (?, ?, ?)"
                             " ON CONFLICT (user_id, key) DO UPDATE SET value = excluded.value"));
    q.addBindValue(userId);
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}

QJsonObject Database::userJson(qint64 userId, const QString &key)
{
    return QJsonDocument::fromJson(userSetting(userId, key).toUtf8()).object();
}

void Database::setUserJson(qint64 userId, const QString &key, const QJsonObject &o)
{
    setUserSetting(userId, key, QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
}

void Database::removeUserSetting(qint64 userId, const QString &key)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM user_settings WHERE user_id = ? AND key = ?"));
    q.addBindValue(userId);
    q.addBindValue(key);
    q.exec();
}

QString Database::deviceSetting(const QString &key, const QString &def)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("SELECT value FROM device WHERE key = ?"));
    q.addBindValue(key);
    if (q.exec() && q.next()) return q.value(0).toString();
    return def;
}

void Database::setDeviceSetting(const QString &key, const QString &value)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO device (key, value) VALUES (?, ?)"));
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}

// ---------------------------------------------------------------- Übungseinheiten

QList<SessionRecord> Database::sessions(qint64 userId, const QDateTime &from, const QDateTime &to)
{
    QList<SessionRecord> l;
    SqlQuery q(db());
    QString sql = QStringLiteral("SELECT id, user_id, module, event, started, planned_sec, actual_sec, cycles, completed,"
                                 " pattern, spo2_before, spo2_after, pf_before, pf_after, pulse_before, pulse_after,"
                                 " feeling_before, feeling_after, note, device_balls, device_hold FROM sessions WHERE user_id = ?");
    if (from.isValid()) sql += QStringLiteral(" AND started >= ?");
    if (to.isValid()) sql += QStringLiteral(" AND started < ?");
    sql += QStringLiteral(" ORDER BY started DESC");
    q.prepare(sql);
    q.addBindValue(userId);
    if (from.isValid()) q.addBindValue(iso(from));
    if (to.isValid()) q.addBindValue(iso(to));
    if (!q.exec()) return l;
    while (q.next()) {
        SessionRecord s;
        s.id = q.value(0).toLongLong();
        s.userId = q.value(1).toLongLong();
        s.moduleId = q.value(2).toString();
        s.eventName = q.value(3).toString();
        s.started = fromIso(q.value(4));
        s.plannedSec = q.value(5).toInt();
        s.actualSec = q.value(6).toInt();
        s.cycles = q.value(7).toInt();
        s.completed = q.value(8).toInt() != 0;
        s.pattern = q.value(9).toString();
        s.spo2Before = optInt(q.value(10));
        s.spo2After = optInt(q.value(11));
        s.pfBefore = optInt(q.value(12));
        s.pfAfter = optInt(q.value(13));
        s.pulseBefore = optInt(q.value(14));
        s.pulseAfter = optInt(q.value(15));
        s.feelingBefore = optInt(q.value(16));
        s.feelingAfter = optInt(q.value(17));
        s.note = q.value(18).toString();
        s.deviceBalls = optInt(q.value(19));
        s.deviceHoldSec = optInt(q.value(20));
        l << s;
    }
    return l;
}

bool Database::saveSession(SessionRecord &s, QString *error)
{
    SqlQuery q(db());
    const QString cols = QStringLiteral("user_id, module, event, started, planned_sec, actual_sec, cycles, completed, pattern,"
                                        " spo2_before, spo2_after, pf_before, pf_after, pulse_before, pulse_after,"
                                        " feeling_before, feeling_after, note, device_balls, device_hold");
    if (s.id == 0)
        q.prepare(QStringLiteral("INSERT INTO sessions (%1) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)").arg(cols));
    else
        q.prepare(QStringLiteral("UPDATE sessions SET (%1) = (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) WHERE id = ?").arg(cols));
    for (const QVariant &v : QVariantList{ s.userId, s.moduleId, s.eventName, iso(s.started), s.plannedSec, s.actualSec,
                                           s.cycles, s.completed ? 1 : 0, s.pattern, opt(s.spo2Before), opt(s.spo2After),
                                           opt(s.pfBefore), opt(s.pfAfter), opt(s.pulseBefore), opt(s.pulseAfter),
                                           opt(s.feelingBefore), opt(s.feelingAfter), s.note, opt(s.deviceBalls),
                                           opt(s.deviceHoldSec) })
        q.addBindValue(v);
    if (s.id != 0) q.addBindValue(s.id);
    if (!q.exec()) return fail(q, error);
    if (s.id == 0) s.id = q.lastInsertId().toLongLong();
    emit changed();
    return true;
}

bool Database::deleteSession(qint64 id, QString *error)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM sessions WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) return fail(q, error);
    emit changed();
    return true;
}

QStringList Database::eventNames(qint64 userId)
{
    QStringList l = { tr("Einschlafübung"), tr("Stressbewältigung"), tr("Morgenroutine"), tr("Konzentration"),
                      tr("Entspannung"), tr("Atemtraining") };
    SqlQuery q(db());
    q.prepare(QStringLiteral("SELECT DISTINCT event FROM sessions WHERE user_id = ? AND event <> ''"
                             " UNION SELECT DISTINCT name FROM events WHERE user_id = ?"));
    q.addBindValue(userId);
    q.addBindValue(userId);
    if (q.exec())
        while (q.next())
            if (!l.contains(q.value(0).toString())) l << q.value(0).toString();
    return l;
}

// ---------------------------------------------------------------- Ereignisse

QList<BreathEvent> Database::events(qint64 userId)
{
    QList<BreathEvent> l;
    SqlQuery q(db());
    q.prepare(QStringLiteral("SELECT id, user_id, name, module, time, weekdays, enabled, sound, note FROM events")
              + (userId ? QStringLiteral(" WHERE user_id = ?") : QString()) + QStringLiteral(" ORDER BY time, name"));
    if (userId) q.addBindValue(userId);
    if (!q.exec()) return l;
    while (q.next()) {
        BreathEvent e;
        e.id = q.value(0).toLongLong();
        e.userId = q.value(1).toLongLong();
        e.name = q.value(2).toString();
        e.moduleId = q.value(3).toString();
        e.time = QTime::fromString(q.value(4).toString(), QStringLiteral("HH:mm"));
        e.weekdays = q.value(5).toInt();
        e.enabled = q.value(6).toInt() != 0;
        e.sound = q.value(7).toInt() != 0;
        e.note = q.value(8).toString();
        l << e;
    }
    return l;
}

bool Database::saveEvent(BreathEvent &e, QString *error)
{
    SqlQuery q(db());
    const QString cols = QStringLiteral("user_id, name, module, time, weekdays, enabled, sound, note");
    if (e.id == 0) q.prepare(QStringLiteral("INSERT INTO events (%1) VALUES (?,?,?,?,?,?,?,?)").arg(cols));
    else q.prepare(QStringLiteral("UPDATE events SET (%1) = (?,?,?,?,?,?,?,?) WHERE id = ?").arg(cols));
    for (const QVariant &v : QVariantList{ e.userId, e.name, e.moduleId, e.time.toString(QStringLiteral("HH:mm")),
                                           e.weekdays, e.enabled ? 1 : 0, e.sound ? 1 : 0, e.note })
        q.addBindValue(v);
    if (e.id) q.addBindValue(e.id);
    if (!q.exec()) return fail(q, error);
    if (!e.id) e.id = q.lastInsertId().toLongLong();
    emit changed();
    return true;
}

bool Database::deleteEvent(qint64 id, QString *error)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM events WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) return fail(q, error);
    emit changed();
    return true;
}

// ---------------------------------------------------------------- Wecker

QList<Alarm> Database::alarms(qint64 userId)
{
    QList<Alarm> l;
    SqlQuery q(db());
    q.prepare(QStringLiteral("SELECT id, user_id, label, time, weekdays, enabled, playlist, shuffle, volume, fade_in,"
                             " snooze, module FROM alarms")
              + (userId ? QStringLiteral(" WHERE user_id = ?") : QString()) + QStringLiteral(" ORDER BY time"));
    if (userId) q.addBindValue(userId);
    if (!q.exec()) return l;
    while (q.next()) {
        Alarm a;
        a.id = q.value(0).toLongLong();
        a.userId = q.value(1).toLongLong();
        a.label = q.value(2).toString();
        a.time = QTime::fromString(q.value(3).toString(), QStringLiteral("HH:mm"));
        a.weekdays = q.value(4).toInt();
        a.enabled = q.value(5).toInt() != 0;
        const QJsonArray pl = QJsonDocument::fromJson(q.value(6).toString().toUtf8()).array();
        for (const QJsonValue &v : pl) a.playlist << v.toString();
        a.shuffle = q.value(7).toInt() != 0;
        a.volume = q.value(8).toInt();
        a.fadeInMin = q.value(9).toInt();
        a.snoozeMin = q.value(10).toInt();
        a.moduleId = q.value(11).toString();
        l << a;
    }
    return l;
}

bool Database::saveAlarm(Alarm &a, QString *error)
{
    SqlQuery q(db());
    const QString cols = QStringLiteral("user_id, label, time, weekdays, enabled, playlist, shuffle, volume, fade_in, snooze, module");
    if (a.id == 0) q.prepare(QStringLiteral("INSERT INTO alarms (%1) VALUES (?,?,?,?,?,?,?,?,?,?,?)").arg(cols));
    else q.prepare(QStringLiteral("UPDATE alarms SET (%1) = (?,?,?,?,?,?,?,?,?,?,?) WHERE id = ?").arg(cols));
    const QString pl = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(a.playlist)).toJson(QJsonDocument::Compact));
    for (const QVariant &v : QVariantList{ a.userId, a.label, a.time.toString(QStringLiteral("HH:mm")), a.weekdays,
                                           a.enabled ? 1 : 0, pl, a.shuffle ? 1 : 0, a.volume, a.fadeInMin, a.snoozeMin,
                                           a.moduleId })
        q.addBindValue(v);
    if (a.id) q.addBindValue(a.id);
    if (!q.exec()) return fail(q, error);
    if (!a.id) a.id = q.lastInsertId().toLongLong();
    emit changed();
    return true;
}

bool Database::deleteAlarm(qint64 id, QString *error)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM alarms WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) return fail(q, error);
    emit changed();
    return true;
}

// ---------------------------------------------------------------- Einschlafen

QList<SleepPlan> Database::sleepPlans(qint64 userId)
{
    QList<SleepPlan> l;
    SqlQuery q(db());
    q.prepare(QStringLiteral("SELECT id, user_id, label, time, weekdays, scheduled, auto_start, playlist, shuffle, volume,"
                             " minutes, fade_out, module FROM sleep_plans")
              + (userId ? QStringLiteral(" WHERE user_id = ?") : QString())
              + QStringLiteral(" ORDER BY scheduled DESC, time, label COLLATE NOCASE"));
    if (userId) q.addBindValue(userId);
    if (!q.exec()) return l;
    while (q.next()) {
        SleepPlan p;
        p.id = q.value(0).toLongLong();
        p.userId = q.value(1).toLongLong();
        p.label = q.value(2).toString();
        p.time = QTime::fromString(q.value(3).toString(), QStringLiteral("HH:mm"));
        p.weekdays = q.value(4).toInt();
        p.scheduled = q.value(5).toInt() != 0;
        p.autoStart = q.value(6).toInt() != 0;
        for (const QJsonValue &v : QJsonDocument::fromJson(q.value(7).toString().toUtf8()).array()) p.playlist << v.toString();
        p.shuffle = q.value(8).toInt() != 0;
        p.volume = q.value(9).toInt();
        p.minutes = q.value(10).toInt();
        p.fadeOutMin = q.value(11).toInt();
        p.moduleId = q.value(12).toString();
        l << p;
    }
    return l;
}

bool Database::saveSleepPlan(SleepPlan &p, QString *error)
{
    SqlQuery q(db());
    const QString cols = QStringLiteral("user_id, label, time, weekdays, scheduled, auto_start, playlist, shuffle, volume,"
                                        " minutes, fade_out, module");
    if (p.id == 0) q.prepare(QStringLiteral("INSERT INTO sleep_plans (%1) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)").arg(cols));
    else q.prepare(QStringLiteral("UPDATE sleep_plans SET (%1) = (?,?,?,?,?,?,?,?,?,?,?,?) WHERE id = ?").arg(cols));
    const QString pl = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(p.playlist)).toJson(QJsonDocument::Compact));
    for (const QVariant &v : QVariantList{ p.userId, p.label, p.time.toString(QStringLiteral("HH:mm")), p.weekdays,
                                           p.scheduled ? 1 : 0, p.autoStart ? 1 : 0, pl, p.shuffle ? 1 : 0, p.volume,
                                           p.minutes, p.fadeOutMin, p.moduleId })
        q.addBindValue(v);
    if (p.id) q.addBindValue(p.id);
    if (!q.exec()) return fail(q, error);
    if (!p.id) p.id = q.lastInsertId().toLongLong();
    emit changed();
    return true;
}

bool Database::deleteSleepPlan(qint64 id, QString *error)
{
    SqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM sleep_plans WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) return fail(q, error);
    emit changed();
    return true;
}

void Database::migrateSleepSettings()
{
    // Version 0.2: eine Einstellung "sleep" je Benutzer → erster gespeicherter Eintrag (nicht geplant)
    for (const User &u : users()) {
        const QJsonObject o = userJson(u.id, QStringLiteral("sleep"));
        if (o.isEmpty() || !sleepPlans(u.id).isEmpty()) continue;
        SleepPlan p;
        p.userId = u.id;
        p.label = tr("Einschlafen");
        for (const QJsonValue &v : o.value(QStringLiteral("playlist")).toArray()) p.playlist << v.toString();
        p.shuffle = o.value(QStringLiteral("shuffle")).toBool(false);
        p.volume = o.value(QStringLiteral("volume")).toInt(50);
        p.minutes = o.value(QStringLiteral("minutes")).toInt(30);
        p.fadeOutMin = o.value(QStringLiteral("fadeOut")).toInt(5);
        p.moduleId = o.value(QStringLiteral("module")).toString();
        saveSleepPlan(p);
        removeUserSetting(u.id, QStringLiteral("sleep"));
    }
}
