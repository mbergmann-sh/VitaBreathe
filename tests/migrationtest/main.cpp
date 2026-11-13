// Migrationstest: Eine Datenbank im Format 1 (ViviaBreathe 0.2 – ohne sleep_plans und ohne
// Atemtrainer-Spalten, Einschlaf-Einstellung als Benutzer-JSON "sleep") wird nachgebildet und
// mit der aktuellen Version geöffnet. Erwartet: Daten bleiben erhalten, neue Tabellen/Spalten
// sind vorhanden, die Einschlaf-Einstellung wird zum ersten Einschlaf-Eintrag.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "database.h"
#include "sqlquery.h"
#include "legacyimport.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cstdio>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { ++fails; std::printf("FEHLER: %s\n", msg); } else std::printf("ok: %s\n", msg); } while (0)

static bool hasColumn(const QString &table, const QString &column)
{
    SqlQuery q(Database::instance().handleDb());
    q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table));
    while (q.next())
        if (q.value(1).toString() == column) return true;
    return false;
}

static bool exec(const QString &sql)
{
    SqlQuery q(Database::instance().handleDb());
    const bool ok = q.exec(sql);
    if (!ok) std::printf("  SQL fehlgeschlagen: %s\n", qPrintable(sql));
    return ok;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir tmp;
    const QString path = tmp.filePath("alt.sqlite");
    Database &db = Database::instance();

    // 1) Datenbank anlegen und auf Format 1 zurückbauen
    CHECK(db.open(path, "Geheim123!") == Database::OpenResult::Ok, "neue Datenbank anlegen");
    const qint64 uid = db.addUser("Erika");
    SessionRecord s;
    s.userId = uid; s.moduleId = "four_seven_eight"; s.started = QDateTime::currentDateTime();
    s.actualSec = 300; s.plannedSec = 300; s.pattern = "4-7-8"; s.cycles = 15; s.spo2Before = 94; s.spo2After = 96;
    s.note = "vor dem Umbau";
    CHECK(db.saveSession(s), "Übungseinheit speichern");
    QJsonObject sleep;
    sleep["playlist"] = QJsonArray{ "/musik/meer.mp3", "/musik/regen.mp3" };
    sleep["shuffle"] = true; sleep["volume"] = 35; sleep["minutes"] = 40; sleep["fadeOut"] = 8; sleep["module"] = "four_six";
    db.setUserJson(uid, "sleep", sleep);

    bool ok = exec("DROP TRIGGER IF EXISTS vb_sync_ins_sessions") && exec("DROP TRIGGER IF EXISTS vb_sync_upd_sessions")
              && exec("DROP TRIGGER IF EXISTS vb_sync_del_sessions")
              && exec("ALTER TABLE sessions DROP COLUMN device_balls") && exec("ALTER TABLE sessions DROP COLUMN device_hold")
              && exec("DROP TABLE sleep_plans") && exec("UPDATE meta SET value = '1' WHERE key = 'schema'");
    CHECK(ok, "auf Format 1 (0.2) zurückgebaut");
    CHECK(!hasColumn("sessions", "device_balls"), "Format 1: keine Atemtrainer-Spalte");
    db.close();

    // 2) Mit der aktuellen Version öffnen → Migration
    QString err;
    CHECK(db.open(path, "Geheim123!", &err) == Database::OpenResult::Ok, qPrintable("alte Datenbank öffnen " + err));
    {
        SqlDb h = db.handleDb();
        CHECK(Database::schemaVersionOf(h, "main") == Database::kSchemaVersion, "Formatnummer aktualisiert");
    }
    CHECK(hasColumn("sessions", "device_balls") && hasColumn("sessions", "device_hold"), "Atemtrainer-Spalten ergänzt");
    CHECK(hasColumn("sleep_plans", "auto_start"), "Tabelle sleep_plans angelegt");

    const QList<SessionRecord> list = db.sessions(uid);
    CHECK(list.size() == 1, "Übungseinheit erhalten");
    if (!list.isEmpty()) {
        const SessionRecord &r = list.first();
        CHECK(r.moduleId == "four_seven_eight" && r.cycles == 15 && r.spo2Before == 94 && r.spo2After == 96
                  && r.note == "vor dem Umbau", "Werte der Übungseinheit unverändert");
        CHECK(r.deviceBalls == kNoValue && r.deviceHoldSec == kNoValue, "neue Felder leer");
    }

    const QList<SleepPlan> plans = db.sleepPlans(uid);
    CHECK(plans.size() == 1, "Einschlaf-Einstellung → ein Einschlaf-Eintrag");
    if (!plans.isEmpty()) {
        const SleepPlan &p = plans.first();
        CHECK(p.playlist.size() == 2 && p.shuffle && p.volume == 35 && p.minutes == 40 && p.fadeOutMin == 8
                  && p.moduleId == "four_six" && !p.scheduled, "Einschlaf-Eintrag übernommen (nicht geplant)");
    }
    CHECK(db.userJson(uid, "sleep").isEmpty(), "alte Einstellung entfernt");

    // 3) Nach der Migration: Atemtrainer-Werte speichern und ändern (Abgleich-Trigger beobachten neue Spalten)
    SessionRecord t;
    t.userId = uid; t.moduleId = "triball"; t.started = QDateTime::currentDateTime(); t.actualSec = 230;
    t.pattern = "3-3-3-4-10"; t.cycles = 10; t.deviceBalls = 2; t.deviceHoldSec = 4;
    CHECK(db.saveSession(t), "TriBall-Einheit speichern");
    t.deviceBalls = 3;
    CHECK(db.saveSession(t), "TriBall-Einheit ändern");
    bool found = false;
    for (const SessionRecord &r : db.sessions(uid))
        if (r.moduleId == "triball") found = r.deviceBalls == 3 && r.deviceHoldSec == 4;
    CHECK(found, "Atemtrainer-Werte gelesen");
    {
        SqlQuery q(db.handleDb());
        q.exec("SELECT sql FROM sqlite_master WHERE name = 'vb_sync_upd_sessions'");
        CHECK(q.next() && q.value(0).toString().contains("device_balls"), "Abgleich-Trigger kennt die neuen Spalten");
    }
    db.close();

    // 4) Erneutes Öffnen ist idempotent
    CHECK(db.open(path, "Geheim123!") == Database::OpenResult::Ok, "zweites Öffnen");
    CHECK(db.sleepPlans(uid).size() == 1 && db.sessions(uid).size() == 2, "keine doppelten Einträge");
    db.close();

    // 5) Umbenennung VitaBreathe → ViviaBreathe: Einstellungen und Datenbank übernehmen
    {
        const QString org = QStringLiteral("vbtest-legacy");
        QSettings(org, "Alt").clear();
        QSettings(org, "Neu").clear();
        const QString oldDir = tmp.filePath("alt-daten"), newDir = tmp.filePath("neu-daten");
        QDir().mkpath(oldDir);
        QFile::copy(path, oldDir + "/vitabreathe.sqlite");
        {
            QSettings o(org, "Alt");
            o.setValue("ui/language", "en");
            o.setValue("windows/SessionDialog", QByteArray("geo"));
            o.setValue("sync/url", "https://example.org/");
            o.setValue("db/path", oldDir + "/vitabreathe.sqlite");
            QSettings n(org, "Neu");
            n.setValue("ui/language", "de");   // vom Installer gesetzt
        }
        const QString r = LegacyImport::run(org, "Alt", "Neu", oldDir, newDir, "vitabreathe.sqlite", "viviabreathe.sqlite");
        QSettings n(org, "Neu");
        CHECK(n.value("ui/language").toString() == "de", "Umbenennung: Sprache des Installers bleibt");
        CHECK(n.value("sync/url").toString() == "https://example.org/"
                  && n.value("windows/SessionDialog").toByteArray() == "geo", "Umbenennung: Einstellungen übernommen");
        CHECK(n.value("db/path").toString() == newDir + "/viviabreathe.sqlite"
                  && QFileInfo(newDir + "/viviabreathe.sqlite").size() == QFileInfo(oldDir + "/vitabreathe.sqlite").size(),
              "Umbenennung: Datenbank in den neuen Ordner kopiert");
        CHECK(QFileInfo::exists(oldDir + "/vitabreathe.sqlite"), "Umbenennung: alte Datenbank bleibt als Sicherung");
        CHECK(r.contains("Datenbank"), "Umbenennung: Meldung");
        CHECK(db.open(n.value("db/path").toString(), "Geheim123!") == Database::OpenResult::Ok
                  && db.sessions(uid).size() == 2, "Umbenennung: übernommene Datenbank öffnet mit allen Einträgen");
        db.close();
        n.setValue("sync/url", "https://neu.example.org/");
        CHECK(LegacyImport::run(org, "Alt", "Neu", oldDir, newDir, "vitabreathe.sqlite", "viviabreathe.sqlite").isEmpty()
                  && QSettings(org, "Neu").value("sync/url").toString() == "https://neu.example.org/",
              "Umbenennung: läuft nur einmal");

        // selbst gewählter Datenbankort bleibt
        QSettings(org, "Alt").clear();
        QSettings(org, "Neu2").clear();
        QSettings(org, "Alt").setValue("db/path", path);
        LegacyImport::run(org, "Alt", "Neu2", oldDir, tmp.filePath("neu2"), "vitabreathe.sqlite", "viviabreathe.sqlite");
        CHECK(QSettings(org, "Neu2").value("db/path").toString() == path && !QFileInfo::exists(tmp.filePath("neu2/viviabreathe.sqlite")),
              "Umbenennung: eigener Datenbankort bleibt unverändert");
        QSettings(org, "Alt").clear(); QSettings(org, "Neu").clear(); QSettings(org, "Neu2").clear();
    }

    std::printf("\n%s (%d Fehler)\n", fails ? "FEHLGESCHLAGEN" : "ALLES OK", fails);
    return fails ? 1 : 0;
}
