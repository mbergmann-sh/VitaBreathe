// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "dbsync.h"
#include "database.h"
#include "sqlquery.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>

// Übersetzungskontext "DbSync" (für lupdate erkennbar)
struct SyncTr { Q_DECLARE_TR_FUNCTIONS(DbSync) };

namespace {

QStringList columns(const SqlDb &d, const QString &schema, const QString &table)
{
    QStringList l;
    SqlQuery q(d);
    if (q.exec(QStringLiteral("PRAGMA %1.table_info(%2)").arg(schema, table)))
        while (q.next()) l << q.value(1).toString();
    return l;
}

qint64 scalar(const SqlDb &d, const QString &sql)
{
    SqlQuery q(d);
    if (q.exec(sql) && q.next()) return q.value(0).toLongLong();
    return -1;
}

const char *kEnabled = "sync/enabled", *kAuto = "sync/auto", *kUrl = "sync/url", *kKey = "sync/key",
           *kRev = "sync/rev", *kBaseline = "sync/baseline", *kLast = "sync/last", *kResult = "sync/result";
QString s(const char *k) { return QString::fromLatin1(k); }
} // namespace

// ====================================================================== Zusammenführen

namespace DbMerge {

bool mergeFrom(const QString &remotePath, const QString &password, Stats *stats, QString *error)
{
    Database &db = Database::instance();
    SqlDb d = db.handleDb();
    Stats st;
    auto failed = [&](const QString &msg) {
        if (error) *error = msg;
        return false;
    };
    {
        SqlQuery a(d);
        a.prepare(QStringLiteral("ATTACH DATABASE ? AS r KEY ?"));
        a.addBindValue(remotePath);
        a.addBindValue(password);
        if (!a.exec()) {
            if (a.lastError().text().contains(QLatin1String("not a database")))
                return failed(SyncTr::tr("Die Datenbank auf dem Server ist mit einem anderen Master-Passwort verschlüsselt. Bitte auf "
                                 "allen Geräten dasselbe Master-Passwort verwenden (Extras → Einstellungen → Master-Passwort)."));
            return failed(SyncTr::tr("Die Datenbank des Servers lässt sich nicht öffnen: %1").arg(a.lastError().text()));
        }
    }
    auto detach = [&] { d.exec(QStringLiteral("DETACH DATABASE r")); };
    if (scalar(d, QStringLiteral("SELECT count(*) FROM r.sqlite_master")) < 0) {
        detach();
        return failed(SyncTr::tr("Die Datenbank auf dem Server hat ein anderes Master-Passwort. Bitte auf allen Geräten "
                         "dasselbe Master-Passwort verwenden."));
    }
    if (const int remote = Database::schemaVersionOf(d, QStringLiteral("r")); remote > Database::kSchemaVersion) {
        detach();
        return failed(SyncTr::tr("Ein anderes Gerät verwendet eine neuere ViviaBreathe-Version (Datenbankformat %1, hier %2). "
                         "Bitte ViviaBreathe auf diesem Gerät aktualisieren.").arg(remote).arg(Database::kSchemaVersion));
    }
    if (!d.transaction()) { detach(); return failed(d.lastError().text()); }
    SqlQuery q(d);
    auto run = [&](const QString &sql) {
        if (q.exec(sql)) return true;
        if (error) *error = q.lastError().text() + QStringLiteral("\n") + sql;
        return false;
    };
    bool ok = run(QStringLiteral("UPDATE sync_state SET suspend = 1 WHERE id = 1"))
           && run(QStringLiteral("CREATE TEMP TABLE IF NOT EXISTS _chg (tbl TEXT, uid TEXT, PRIMARY KEY (tbl, uid))"))
           && run(QStringLiteral("DELETE FROM temp._chg"));

    auto mapExpr = [](const QString &remoteValue) {
        return QStringLiteral("(SELECT lp.id FROM main.users lp JOIN r.users rp ON rp.uid = lp.uid WHERE rp.id = %1)")
            .arg(remoteValue);
    };
    for (const Database::SyncTable &t : Database::syncTables()) {
        if (!ok) break;
        const QString tbl = QString::fromLatin1(t.name);
        const QString fk = t.parentColumn ? QString::fromLatin1(t.parentColumn) : QString();
        const QStringList remoteCols = columns(d, QStringLiteral("r"), tbl);
        if (remoteCols.isEmpty()) continue;
        QStringList cols;
        for (const QString &c : columns(d, QStringLiteral("main"), tbl))
            if (remoteCols.contains(c) && c != QLatin1String("id") && c != QLatin1String("uid")
                && c != QLatin1String("sync_at") && c != fk)
                cols << c;

        // 1. Löschvermerke der Gegenseite, die jünger sind als die lokale Änderung
        const qint64 before = scalar(d, QStringLiteral("SELECT count(*) FROM main.%1").arg(tbl));
        ok = run(QStringLiteral("DELETE FROM main.%1 WHERE EXISTS (SELECT 1 FROM r.sync_deleted rd"
                                " WHERE rd.uid = main.%1.uid AND rd.at >= main.%1.sync_at)").arg(tbl));
        if (!ok) break;
        st.deleted += int(before - scalar(d, QStringLiteral("SELECT count(*) FROM main.%1").arg(tbl)));

        // 2. Zeilen, bei denen die Gegenseite gewinnt
        ok = run(QStringLiteral("INSERT INTO temp._chg (tbl, uid) SELECT '%1', rr.uid FROM r.%1 rr"
                                " LEFT JOIN main.%1 l ON l.uid = rr.uid"
                                " WHERE rr.uid IS NOT NULL AND (l.uid IS NULL OR rr.sync_at > l.sync_at)"
                                " AND NOT EXISTS (SELECT 1 FROM main.sync_deleted ld WHERE ld.uid = rr.uid AND ld.at >= rr.sync_at)")
                     .arg(tbl));
        if (!ok) break;

        // 3. Vorhandene Zeilen aktualisieren
        QStringList setCols = cols, selCols;
        for (const QString &c : cols) selCols << QStringLiteral("rr.") + c;
        setCols << QStringLiteral("sync_at");
        selCols << QStringLiteral("rr.sync_at");
        const qint64 updates = scalar(d, QStringLiteral("SELECT count(*) FROM temp._chg c JOIN main.%1 l ON l.uid = c.uid"
                                                        " WHERE c.tbl = '%1'").arg(tbl));
        if (!cols.isEmpty()) {
            ok = run(QStringLiteral("UPDATE main.%1 SET (%2) = (SELECT %3 FROM r.%1 rr WHERE rr.uid = main.%1.uid)"
                                    " WHERE uid IN (SELECT uid FROM temp._chg WHERE tbl = '%1')")
                         .arg(tbl, setCols.join(QLatin1Char(',')), selCols.join(QLatin1Char(','))));
            if (!ok) break;
        }
        st.updated += int(updates);

        // 4. Neue Zeilen einfügen (Benutzer muss lokal vorhanden sein)
        QStringList insCols = QStringList{ QStringLiteral("uid"), QStringLiteral("sync_at") } + cols;
        QStringList insSel = QStringList{ QStringLiteral("rr.uid"), QStringLiteral("rr.sync_at") };
        for (const QString &c : cols) insSel << QStringLiteral("rr.") + c;
        QString parentOk;
        if (!fk.isEmpty()) {
            insCols << fk;
            insSel << mapExpr(QStringLiteral("rr.") + fk);
            parentOk = QStringLiteral(" AND %1 IS NOT NULL").arg(mapExpr(QStringLiteral("rr.") + fk));
        }
        const qint64 n0 = scalar(d, QStringLiteral("SELECT count(*) FROM main.%1").arg(tbl));
        // OR REPLACE: gleiche Einstellung (Benutzer, Schlüssel) unter anderer Kennung
        ok = run(QStringLiteral("INSERT OR REPLACE INTO main.%1 (%2) SELECT %3 FROM r.%1 rr"
                                " WHERE rr.uid IN (SELECT uid FROM temp._chg WHERE tbl = '%1')"
                                " AND rr.uid NOT IN (SELECT uid FROM main.%1 WHERE uid IS NOT NULL)%4")
                     .arg(tbl, insCols.join(QLatin1Char(',')), insSel.join(QLatin1Char(',')), parentOk));
        if (!ok) break;
        st.inserted += int(scalar(d, QStringLiteral("SELECT count(*) FROM main.%1").arg(tbl)) - n0);

        // 5. Löschvermerke übernehmen
        ok = run(QStringLiteral("INSERT OR REPLACE INTO main.sync_deleted (uid, tbl, at)"
                                " SELECT rd.uid, rd.tbl, rd.at FROM r.sync_deleted rd WHERE rd.tbl = '%1'"
                                " AND NOT EXISTS (SELECT 1 FROM main.%1 l WHERE l.uid = rd.uid)"
                                " AND rd.at > coalesce((SELECT ld.at FROM main.sync_deleted ld WHERE ld.uid = rd.uid), '')")
                     .arg(tbl));
    }
    if (ok) ok = run(QStringLiteral("UPDATE sync_state SET suspend = 0 WHERE id = 1"));
    if (ok) ok = d.commit();
    if (!ok) {
        d.rollback();
        d.exec(QStringLiteral("UPDATE sync_state SET suspend = 0 WHERE id = 1"));
    }
    q.finish();
    d.exec(QStringLiteral("DROP TABLE IF EXISTS temp._chg"));
    detach();
    if (stats) *stats = st;
    return ok;
}

} // namespace DbMerge

// ====================================================================== Netzwerk

namespace {
struct Http {
    int status = 0;
    QByteArray body;
    QString error;
    QJsonObject json() const { return QJsonDocument::fromJson(body).object(); }
};

QNetworkRequest request(const QString &baseUrl, const QString &key, const QUrlQuery &query = {})
{
    QUrl u(DbSync::normalizedUrl(baseUrl) + QStringLiteral("api/sync.php"));
    u.setQuery(query);
    QNetworkRequest r(u);
    r.setRawHeader("X-ViviaBreathe-Key", key.toUtf8());
    r.setRawHeader("Accept-Language", QLocale().bcp47Name().toUtf8());
    r.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ViviaBreathe/%1").arg(vbVersion()));
    r.setTransferTimeout(120000);
    return r;
}

Http finish(QNetworkReply *reply, QIODevice *sink = nullptr)
{
    QEventLoop loop;
    if (sink)
        QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply, sink] { sink->write(reply->readAll()); });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    Http h;
    h.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (sink) sink->write(reply->readAll());
    else h.body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) h.error = reply->errorString();
    reply->deleteLater();
    return h;
}

QString httpMessage(const Http &h)
{
    const QString e = h.json().value(QStringLiteral("error")).toString();
    if (!e.isEmpty()) return e;
    if (h.status == 404) return SyncTr::tr("Auf dem Server fehlt api/sync.php (HTTP 404).");
    if (!h.error.isEmpty()) return h.error;
    return SyncTr::tr("Unerwartete Antwort des Servers (HTTP %1).").arg(h.status);
}

bool download(QNetworkAccessManager &nam, const QString &url, const QString &key, const QString &target, QString *error)
{
    QFile f(target);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = f.errorString();
        return false;
    }
    const Http h = finish(nam.get(request(url, key, QUrlQuery{ { QStringLiteral("get"), QStringLiteral("db") } })), &f);
    f.close();
    if (h.status != 200 || !h.error.isEmpty()) {
        QFile::remove(target);
        if (error) *error = h.error.isEmpty() ? SyncTr::tr("Download fehlgeschlagen (HTTP %1).").arg(h.status) : h.error;
        return false;
    }
    return true;
}

QJsonObject serverState(QNetworkAccessManager &nam, const QString &url, const QString &key, QString *error)
{
    const Http h = finish(nam.get(request(url, key, QUrlQuery{ { QStringLiteral("state"), QStringLiteral("1") } })));
    if (h.status != 200 || !h.json().value(QStringLiteral("ok")).toBool()) {
        if (error) *error = httpMessage(h);
        return {};
    }
    return h.json();
}
} // namespace

// ====================================================================== Abgleich

DbSync &DbSync::instance()
{
    static DbSync *inst = new DbSync;
    return *inst;
}

DbSync::DbSync()
{
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(60 * 1000);
    connect(m_debounce, &QTimer::timeout, this, [this] {
        const Config c = config();
        if (c.autoSync && isReady(c) && !m_busy) sync();
    });
}

DbSync::Config DbSync::config()
{
    Database &db = Database::instance();
    Config c;
    c.enabled = db.deviceSetting(s(kEnabled)) == QLatin1String("1");
    c.autoSync = db.deviceSetting(s(kAuto), QStringLiteral("1")) == QLatin1String("1");
    c.url = db.deviceSetting(s(kUrl));
    c.uploadKey = db.deviceSetting(s(kKey));
    return c;
}

void DbSync::saveConfig(const Config &c)
{
    Database &db = Database::instance();
    db.setDeviceSetting(s(kEnabled), c.enabled ? QStringLiteral("1") : QStringLiteral("0"));
    db.setDeviceSetting(s(kAuto), c.autoSync ? QStringLiteral("1") : QStringLiteral("0"));
    if (normalizedUrl(c.url) != normalizedUrl(db.deviceSetting(s(kUrl)))) {
        db.setDeviceSetting(s(kRev), QStringLiteral("0"));      // anderer Server: neu beginnen
        db.setDeviceSetting(s(kBaseline), QStringLiteral("-1"));
    }
    db.setDeviceSetting(s(kUrl), c.url.trimmed());
    db.setDeviceSetting(s(kKey), c.uploadKey.trimmed());
}

QString DbSync::normalizedUrl(const QString &url)
{
    QString u = url.trimmed();
    if (u.isEmpty()) return u;
    if (!u.contains(QStringLiteral("://"))) u.prepend(QStringLiteral("https://"));
    if (!u.endsWith(QLatin1Char('/'))) u += QLatin1Char('/');
    return u;
}

QString DbSync::generateUploadKey()
{
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    QString k;
    for (int i = 0; i < 32; ++i)
        k += QLatin1Char(alphabet[QRandomGenerator::system()->bounded(int(sizeof(alphabet) - 1))]);
    return k;
}

bool DbSync::tlsAvailable(QString *text)
{
    const bool ok = QSslSocket::supportsSsl();
    if (text)
        *text = ok ? QSslSocket::sslLibraryVersionString()
                   : SyncTr::tr("Keine TLS-Bibliothek gefunden (OpenSSL) – verschlüsselte Verbindungen sind nicht möglich.");
    return ok;
}

bool DbSync::isReady(const Config &c, QString *why)
{
    auto no = [why](const QString &m) { if (why) *why = m; return false; };
    if (!c.enabled) return no(SyncTr::tr("Der Abgleich ist nicht eingeschaltet."));
    const QUrl u(normalizedUrl(c.url));
    if (c.url.trimmed().isEmpty() || !u.isValid()) return no(SyncTr::tr("Die Adresse des Servers fehlt oder ist ungültig."));
    const bool local = u.host() == QLatin1String("localhost") || u.host() == QLatin1String("127.0.0.1");
    if (u.scheme() != QLatin1String("https") && !local) return no(SyncTr::tr("Der Server muss über https:// erreichbar sein."));
    if (u.scheme() == QLatin1String("https")) {
        QString tls;
        if (!tlsAvailable(&tls)) return no(tls);
    }
    if (c.uploadKey.size() < 16) return no(SyncTr::tr("Der Zugangsschlüssel fehlt (mindestens 16 Zeichen)."));
    return true;
}

DbSync::Status DbSync::status()
{
    Database &db = Database::instance();
    Status st;
    st.revision = db.deviceSetting(s(kRev), QStringLiteral("0")).toLongLong();
    st.last = QDateTime::fromString(db.deviceSetting(s(kLast)), Qt::ISODate);
    st.lastResult = db.deviceSetting(s(kResult));
    st.pending = db.syncChangeCounter() != db.deviceSetting(s(kBaseline), QStringLiteral("-1")).toLongLong();
    return st;
}

void DbSync::watchDatabase()
{
    if (m_watching) return;
    m_watching = true;
    connect(&Database::instance(), &Database::changed, this, [this] {
        const Config c = config();
        if (c.enabled && c.autoSync) m_debounce->start();
    });
}

void DbSync::sync()
{
    if (m_busy) return;
    emit started();
    QTimer::singleShot(0, this, [this] {
        QString msg;
        const bool ok = runSync(&msg);
        emit finished(ok, msg);
    });
}

bool DbSync::syncBlocking(QString *message)
{
    if (m_busy) return false;
    emit started();
    const bool ok = runSync(message);
    emit finished(ok, message ? *message : QString());
    return ok;
}

bool DbSync::runSync(QString *message)
{
    Database &db = Database::instance();
    const Config c = config();
    QString why;
    if (!isReady(c, &why)) {
        if (message) *message = why;
        return false;
    }
    m_busy = true;
    m_debounce->stop();
    QNetworkAccessManager nam;
    QTemporaryDir tmp;
    QString err;
    bool ok = false;
    int mergedChanges = 0;
    auto done = [&](bool result, const QString &msg) {
        m_busy = false;
        db.setDeviceSetting(s(kLast), QDateTime::currentDateTime().toString(Qt::ISODate));
        db.setDeviceSetting(s(kResult), msg);
        if (message) *message = msg;
        return result;
    };

    for (int attempt = 0; attempt < 3; ++attempt) {
        const QJsonObject state = serverState(nam, c.url, c.uploadKey, &err);
        if (state.isEmpty()) return done(false, err);
        const qint64 serverRev = state.value(QStringLiteral("rev")).toVariant().toLongLong();
        const qint64 localRev = db.deviceSetting(s(kRev), QStringLiteral("0")).toLongLong();

        // 1. Stand des Servers einarbeiten
        if (serverRev > 0 && serverRev != localRev) {
            const QString remote = tmp.filePath(QStringLiteral("server.sqlite"));
            if (!download(nam, c.url, c.uploadKey, remote, &err)) return done(false, err);
            DbMerge::Stats st;
            if (!DbMerge::mergeFrom(remote, db.password(), &st, &err)) return done(false, err);
            mergedChanges += st.total();
            QFile::remove(remote);
        }
        // 2. Eigenen Stand hochladen, wenn es lokale Änderungen gibt (oder der Server leer ist)
        const bool localChanges = db.syncChangeCounter() != db.deviceSetting(s(kBaseline), QStringLiteral("-1")).toLongLong();
        if (!localChanges && serverRev > 0) {
            db.setDeviceSetting(s(kRev), QString::number(serverRev));
            ok = true;
            break;
        }
        const QString upload = tmp.filePath(QStringLiteral("upload.sqlite"));
        if (!db.backupTo(upload, &err)) return done(false, err);
        auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        auto field = [multi](const QString &name, const QByteArray &value) {
            QHttpPart p;
            p.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"%1\"").arg(name));
            p.setBody(value);
            multi->append(p);
        };
        field(QStringLiteral("base"), QByteArray::number(serverRev));
        field(QStringLiteral("schema"), QByteArray::number(Database::kSchemaVersion));
        auto *file = new QFile(upload, multi);
        file->open(QIODevice::ReadOnly);
        QHttpPart fp;
        fp.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
        fp.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"db\"; filename=\"db.vbdb\""));
        fp.setBodyDevice(file);
        multi->append(fp);
        QNetworkReply *reply = nam.post(request(c.url, c.uploadKey), multi);
        multi->setParent(reply);
        const Http h = finish(reply);
        if (h.status == 409) continue;            // ein anderes Gerät war schneller → erneut einarbeiten
        if (h.status != 200 || !h.json().value(QStringLiteral("ok")).toBool()) return done(false, httpMessage(h));
        db.setDeviceSetting(s(kRev), QString::number(h.json().value(QStringLiteral("rev")).toVariant().toLongLong()));
        db.setDeviceSetting(s(kBaseline), QString::number(db.syncChangeCounter()));
        ok = true;
        break;
    }
    if (!ok) return done(false, SyncTr::tr("Der Server war zu oft durch andere Geräte belegt – bitte später erneut versuchen."));
    if (mergedChanges > 0) emit Database::instance().changed();
    db.setDeviceSetting(s(kBaseline), QString::number(db.syncChangeCounter()));
    return done(true, mergedChanges > 0 ? SyncTr::tr("Abgeglichen – %1 Änderung(en) von anderen Geräten übernommen.").arg(mergedChanges)
                                        : SyncTr::tr("Abgeglichen."));
}

qint64 DbSync::serverRevision(const QString &url, const QString &uploadKey, QString *message)
{
    QNetworkAccessManager nam;
    QString err;
    const QJsonObject st = serverState(nam, url, uploadKey, &err);
    if (st.isEmpty()) {
        if (message) *message = err;
        return -1;
    }
    const qint64 rev = st.value(QStringLiteral("rev")).toVariant().toLongLong();
    if (message)
        *message = st.value(QStringLiteral("claimed")).toBool()
                       ? SyncTr::tr("Verbindung in Ordnung – Stand auf dem Server: Revision %1.").arg(rev)
                       : SyncTr::tr("Verbindung in Ordnung – der Server ist noch leer; der erste Abgleich richtet ihn ein.");
    return rev;
}

bool DbSync::downloadInitial(const QString &url, const QString &uploadKey, const QString &dbPath, QString *error)
{
    QNetworkAccessManager nam;
    const QJsonObject st = serverState(nam, url, uploadKey, error);
    if (st.isEmpty()) return false;
    const qint64 rev = st.value(QStringLiteral("rev")).toVariant().toLongLong();
    if (rev <= 0) {
        if (error) *error = SyncTr::tr("Auf dem Server liegt noch keine Datenbank.");
        return false;
    }
    const QString tmp = dbPath + QStringLiteral(".download");
    if (!download(nam, url, uploadKey, tmp, error)) return false;
    QFile::remove(dbPath);
    if (!QFile::rename(tmp, dbPath)) {
        if (error) *error = SyncTr::tr("Die Datenbank lässt sich nicht speichern.");
        return false;
    }
    QFile marker(dbPath + QStringLiteral(".syncinit"));
    if (marker.open(QIODevice::WriteOnly))
        marker.write(QJsonDocument(QJsonObject{ { QStringLiteral("url"), url }, { QStringLiteral("key"), uploadKey },
                                                { QStringLiteral("rev"), QString::number(rev) } }).toJson());
    return true;
}

void DbSync::adoptInitialState(const QString &dbPath)
{
    QFile marker(dbPath + QStringLiteral(".syncinit"));
    if (!marker.open(QIODevice::ReadOnly)) return;
    const QJsonObject o = QJsonDocument::fromJson(marker.readAll()).object();
    marker.close();
    marker.remove();
    Database &db = Database::instance();
    Config c;
    c.enabled = true;
    c.autoSync = true;
    c.url = o.value(QStringLiteral("url")).toString();
    c.uploadKey = o.value(QStringLiteral("key")).toString();
    saveConfig(c);
    db.setDeviceSetting(s(kRev), o.value(QStringLiteral("rev")).toString());
    db.setDeviceSetting(s(kBaseline), QString::number(db.syncChangeCounter()));
    db.setDeviceSetting(s(kLast), QDateTime::currentDateTime().toString(Qt::ISODate));
    db.setDeviceSetting(s(kResult), SyncTr::tr("Datenbank vom Server übernommen."));
}
