// Abgleich-Test: zwei Geräte (A, B) gleichen über den lokalen Server ab
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "database.h"
#include "dbsync.h"
#include <QApplication>
#include <QDir>
#include <QTemporaryDir>
#include <cstdio>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { ++fails; std::printf("FEHLER: %s\n", msg); } else std::printf("ok: %s\n", msg); } while (0)

static void configure(const QString &url, const QString &key)
{
    DbSync::Config c; c.enabled = true; c.autoSync = false; c.url = url; c.uploadKey = key;
    DbSync::saveConfig(c);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString url = QString::fromLocal8Bit(qgetenv("VB_URL"));
    const QString key = QStringLiteral("testkey-0123456789abcdef");
    QTemporaryDir tmp;
    const QString a = tmp.filePath("a.sqlite"), b = tmp.filePath("b.sqlite"), c = tmp.filePath("c.sqlite");
    Database &db = Database::instance();
    QString msg;

    // Gerät A
    CHECK(db.open(a, "Geheim123!") == Database::OpenResult::Ok, "A öffnen");
    const qint64 ua = db.addUser("Anna");
    SessionRecord s; s.userId = ua; s.moduleId = "box"; s.started = QDateTime::currentDateTime(); s.actualSec = 300; s.pattern = "4-4-4-4";
    db.saveSession(s);
    db.setUserSetting(ua, "module/box", "{\"total\":420}");
    configure(url, key);
    CHECK(DbSync::instance().syncBlocking(&msg), qPrintable("A erster Abgleich: " + msg));
    db.close();

    // Gerät B: per downloadInitial einrichten
    CHECK(DbSync::downloadInitial(url, key, b, &msg), qPrintable("B vom Server laden " + msg));
    CHECK(db.open(b, "Geheim123!") == Database::OpenResult::Ok, "B öffnen");
    DbSync::adoptInitialState(b);
    CHECK(db.users().size() == 1 && db.sessions(db.users().first().id).size() == 1, "B hat Anna mit 1 Einheit");
    const qint64 ub = db.users().first().id;
    SessionRecord s2 = s; s2.id = 0; s2.userId = ub; s2.moduleId = "four_six"; s2.started = s.started.addSecs(3600);
    db.saveSession(s2);
    db.setUserSetting(ub, "module/box", "{\"total\":600}");
    const qint64 ubert = db.addUser("Bert");
    BreathEvent e; e.userId = ubert; e.name = "Einschlafübung"; e.moduleId = "four_six"; db.saveEvent(e);
    SleepPlan sp; sp.userId = ub; sp.label = "Wochentags"; sp.scheduled = true; sp.time = QTime(22, 15);
    sp.weekdays = 0x1F; sp.playlist = QStringList{ "C:/Musik/Regen.mp3" }; sp.moduleId = "four_six"; db.saveSleepPlan(sp);
    CHECK(DbSync::instance().syncBlocking(&msg), qPrintable("B Abgleich: " + msg));
    db.close();

    // Gerät A erneut: übernimmt B's Änderungen, löscht dann die erste Einheit
    CHECK(db.open(a, "Geheim123!") == Database::OpenResult::Ok, "A wieder öffnen");
    CHECK(DbSync::instance().syncBlocking(&msg), qPrintable("A Abgleich: " + msg));
    CHECK(db.users().size() == 2, "A hat 2 Benutzer");
    qint64 annaA = 0; for (const User &u : db.users()) if (u.name == "Anna") annaA = u.id;
    CHECK(db.sessions(annaA).size() == 2, "A: Anna hat 2 Einheiten");
    CHECK(db.userSetting(annaA, "module/box") == "{\"total\":600}", "A: neuere Einstellung von B gewinnt");
    CHECK(db.events().size() == 1, "A: Ereignis von B übernommen");
    CHECK(db.sleepPlans(annaA).size() == 1 && db.sleepPlans(annaA).first().scheduled
          && db.sleepPlans(annaA).first().time == QTime(22, 15) && db.sleepPlans(annaA).first().playlist.size() == 1,
          "A: geplanter Einschlaf-Eintrag von B übernommen");
    for (const SessionRecord &x : db.sessions(annaA)) if (x.moduleId == "box") db.deleteSession(x.id);
    CHECK(DbSync::instance().syncBlocking(&msg), qPrintable("A Abgleich nach Löschen: " + msg));
    db.close();

    CHECK(db.open(b, "Geheim123!") == Database::OpenResult::Ok, "B wieder öffnen");
    CHECK(DbSync::instance().syncBlocking(&msg), qPrintable("B Abgleich: " + msg));
    CHECK(db.sessions(ub).size() == 1 && db.sessions(ub).first().moduleId == "four_six", "B: Löschung von A übernommen");
    // Passwort falsch?
    db.close();
    CHECK(db.open(b, "falsch") == Database::OpenResult::WrongPassword, "falsches Passwort erkannt");
    // Passwort ändern und wieder öffnen
    CHECK(db.open(b, "Geheim123!") == Database::OpenResult::Ok, "B öffnen");
    CHECK(db.changePassword("Neu456!xyz"), "Passwort ändern");
    db.close();
    CHECK(db.open(b, "Neu456!xyz") == Database::OpenResult::Ok, "mit neuem Passwort öffnen");
    db.addUser("Clara");
    CHECK(DbSync::instance().syncBlocking(&msg), qPrintable("B lädt mit neuem Passwort hoch: " + msg));
    db.close();
    CHECK(db.open(a, "Geheim123!") == Database::OpenResult::Ok, "A (altes Passwort) öffnen");
    QString err;
    CHECK(!DbSync::instance().syncBlocking(&err), qPrintable("A: Abgleich mit anderem Passwort abgelehnt: " + err));
    db.close();
    std::printf("\n%s (%d Fehler)\n", fails ? "FEHLGESCHLAGEN" : "ALLES OK", fails);
    return fails ? 1 : 0;
}
