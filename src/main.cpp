// ViviaBreathe – Programmstart
//
//   ViviaBreathe                 normaler Start (Master-Passwort-Abfrage)
//   ViviaBreathe --demo          Demo mit erfundenen Daten in einer eigenen, temporären Datenbank
//                               (Master-Passwort "demo"), ohne die eigenen Daten zu berühren
//   ViviaBreathe --screenshots <ordner>   (mit --demo) Bildschirmfotos für Handbuch/README erzeugen
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "alarmpage.h"
#include "audio.h"
#include "breathengine.h"
#include "breathmodules.h"
#include "database.h"
#include "dbsync.h"
#include "i18n.h"
#include "legacyimport.h"
#include "icons.h"
#include "logindialog.h"
#include "mainwindow.h"
#include "screenfit.h"
#include "sessiondialog.h"
#include "themes.h"
#include "windowstate.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QGroupBox>
#include <QCommandLineParser>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>

#include "qtcompat.h"

namespace {

void createDemoData()
{
    Database &db = Database::instance();
    const qint64 erika = db.addUser(QObject::tr("Erika Mustermann"));
    const qint64 max = db.addUser(QObject::tr("Max Mustermann"));
    auto *rng = QRandomGenerator::global();
    const QDate today = QDate::currentDate();
    for (int d = 59; d >= 0; --d) {
        const QDate day = today.addDays(-d);
        // abends 4-6 bzw. 4-7-8, gelegentlich morgens Box-Atmen
        if (rng->bounded(10) < 8) {
            SessionRecord s;
            s.userId = erika;
            const bool sleep = rng->bounded(2) == 0;
            s.moduleId = sleep ? QStringLiteral("four_six") : QStringLiteral("four_seven_eight");
            s.eventName = sleep ? QObject::tr("Einschlafübung") : QObject::tr("Entspannung");
            s.pattern = sleep ? QStringLiteral("4-6") : QStringLiteral("4-7-8");
            s.started = QDateTime(day, QTime(21, 10 + rng->bounded(40)));
            s.plannedSec = sleep ? 600 : 300;
            s.actualSec = s.plannedSec + rng->bounded(15);
            s.cycles = s.actualSec / (sleep ? 10 : 19);
            s.completed = rng->bounded(12) != 0;
            if (!s.completed) s.actualSec /= 2;
            if (d % 3 == 0) {     // Trainingsmodus
                s.spo2Before = 93 + rng->bounded(3);
                s.spo2After = qMin(99, s.spo2Before + rng->bounded(3));
                s.pfBefore = 330 + rng->bounded(40) + (59 - d);
                s.pfAfter = s.pfBefore + 10 + rng->bounded(30);
                s.pulseBefore = 74 + rng->bounded(12);
                s.pulseAfter = s.pulseBefore - 4 - rng->bounded(6);
                s.feelingBefore = 4 + rng->bounded(3);
                s.feelingAfter = s.feelingBefore + 1 + rng->bounded(3);
            }
            if (d == 2) s.note = QObject::tr("Sehr ruhig geworden, schnell eingeschlafen.");
            db.saveSession(s);
        }
        if (rng->bounded(10) < 4) {
            SessionRecord s;
            s.userId = erika;
            s.moduleId = QStringLiteral("box");
            s.eventName = QObject::tr("Stressbewältigung");
            s.pattern = QStringLiteral("4-4-4-4");
            s.started = QDateTime(day, QTime(12 + rng->bounded(5), rng->bounded(60)));
            s.plannedSec = 300;
            s.actualSec = 300 + rng->bounded(12);
            s.cycles = s.actualSec / 16;
            s.completed = true;
            db.saveSession(s);
        }
        if (d % 4 == 1) {     // Lippenbremse im Trainingsmodus
            SessionRecord s;
            s.userId = erika;
            s.moduleId = QStringLiteral("pursed_lip");
            s.eventName = QObject::tr("Atemtraining");
            s.pattern = QStringLiteral("2-4");
            s.started = QDateTime(day, QTime(10, rng->bounded(60)));
            s.plannedSec = 300;
            s.actualSec = 300 + rng->bounded(6);
            s.cycles = 50;
            s.completed = true;
            s.spo2Before = 92 + rng->bounded(3);
            s.spo2After = qMin(99, s.spo2Before + 1 + rng->bounded(2));
            s.pfBefore = 320 + rng->bounded(40);
            s.pfAfter = s.pfBefore + rng->bounded(25);
            db.saveSession(s);
        }
        if (d % 2 == 0) {     // TriBall-Training, langsam gesteigert
            SessionRecord s;
            s.userId = erika;
            s.moduleId = QStringLiteral("triball");
            s.eventName = QObject::tr("Atemtrainer");
            s.pattern = QStringLiteral("3-3-3-4-10");
            s.started = QDateTime(day, QTime(16, rng->bounded(60)));
            s.plannedSec = s.actualSec = 230;
            s.cycles = 10;
            s.completed = true;
            s.deviceBalls = d > 40 ? 1 : d > 15 ? 2 : (rng->bounded(3) ? 2 : 3);
            s.deviceHoldSec = 2 + (59 - d) / 12 + rng->bounded(2);
            db.saveSession(s);
        }
        if (rng->bounded(10) < 3) {
            SessionRecord s;
            s.userId = max;
            s.moduleId = QStringLiteral("box");
            s.eventName = QObject::tr("Konzentration");
            s.pattern = QStringLiteral("4-4-4-4");
            s.started = QDateTime(day, QTime(8, rng->bounded(60)));
            s.plannedSec = s.actualSec = 300;
            s.cycles = 19;
            s.completed = true;
            db.saveSession(s);
        }
    }
    BreathEvent e1{ 0, erika, QObject::tr("Einschlafübung"), QStringLiteral("four_six"), QTime(21, 30), Weekdays::kAll, true, true,
                    QObject::tr("Im Bett, Licht aus") };
    BreathEvent e2{ 0, erika, QObject::tr("Stressbewältigung"), QStringLiteral("box"), QTime(13, 0), Weekdays::kWorkdays, true, true, {} };
    BreathEvent e3{ 0, max, QObject::tr("Konzentration"), QStringLiteral("box"), QTime(8, 0), Weekdays::kWorkdays, true, true, {} };
    db.saveEvent(e1);
    db.saveEvent(e2);
    db.saveEvent(e3);
    Alarm a;
    a.userId = erika;
    a.label = QObject::tr("Arbeitstag");
    a.time = QTime(6, 45);
    a.moduleId = QStringLiteral("box");
    db.saveAlarm(a);
    SleepPlan sp;
    sp.userId = erika;
    sp.label = QObject::tr("Wochentags");
    sp.scheduled = true;
    sp.time = QTime(22, 30);
    sp.weekdays = Weekdays::kWorkdays;
    sp.minutes = 30;
    sp.moduleId = QStringLiteral("four_six");
    db.saveSleepPlan(sp);
    SleepPlan we;
    we.userId = erika;
    we.label = QObject::tr("Wochenende – länger");
    we.minutes = 60;
    we.fadeOutMin = 10;
    db.saveSleepPlan(we);
    db.setDeviceSetting(QStringLiteral("ui/lastUser"), QString::number(erika));
}

bool openDatabase(QString path, bool demo)
{
    Database &db = Database::instance();
    if (demo) {
        QFile::remove(path);
        if (db.open(path, QStringLiteral("demo")) != Database::OpenResult::Ok) return false;
        createDemoData();
        return true;
    }
    QString error;
    for (;;) {
        LoginDialog dlg(path);
        if (!error.isEmpty()) dlg.setError(error);
        if (dlg.exec() != QDialog::Accepted) return false;
        path = dlg.databasePath();
        const bool isNew = dlg.isNewDatabase();
        QString err;
        const auto r = db.open(path, dlg.password(), &err);
        if (r == Database::OpenResult::Ok) {
            if (isNew && db.users().isEmpty()) db.addUser(dlg.firstUserName());
            DbSync::adoptInitialState(path);
            return true;
        }
        error = r == Database::OpenResult::WrongPassword ? QObject::tr("Das Master-Passwort ist falsch.") : err;
    }
}

// Bildschirmfotos für Dokumentation (nur mit --demo)
void takeScreenshots(MainWindow *w, const QString &dir)
{
    QDir().mkpath(dir);
    auto shot = [dir](QWidget *wid, const QString &name) {
        QApplication::processEvents();
        wid->grab().save(dir + QLatin1Char('/') + name + QStringLiteral(".png"));
    };
    for (int i = 0; i < w->tabs()->count(); ++i) {
        w->tabs()->setCurrentIndex(i);
        shot(w, QStringLiteral("main_%1").arg(i));
    }
    w->tabs()->setCurrentIndex(0);
    QTimer::singleShot(300, [shot]() {   // Über-Fenster (modal) aufnehmen und schließen
        if (QWidget *about = QApplication::activeModalWidget()) {
            shot(about, QStringLiteral("about"));
            about->close();
        }
    });
    w->showAbout();
    if (!Database::instance().sleepPlans(w->currentUser()).isEmpty()) {
        SleepPlanDialog sd(Database::instance().sleepPlans(w->currentUser()).first(), w);
        sd.show();
        shot(&sd, QStringLiteral("sleep_plan"));
        sd.reject();
    }
    for (const ModuleDef &m : BreathModules::all()) {
        SessionDialog dlg(w->currentUser(), m.id, w);
        dlg.resize(720, 760);
        dlg.show();
        shot(&dlg, QStringLiteral("prep_%1").arg(m.id));
        // Übung starten (wie ein Klick auf „Übung starten“) und Momentaufnahmen machen
        auto *engine = dlg.findChild<BreathEngine *>();
        auto wait = [](int ms) {
            QEventLoop loop;
            QTimer::singleShot(ms, &loop, &QEventLoop::quit);
            loop.exec();
        };
        auto click = [&dlg](const QString &text) {
            for (QAbstractButton *b : dlg.findChildren<QAbstractButton *>())
                if (b->isVisible() && b->text() == text) { b->click(); return true; }
            return false;
        };
        const bool training = m.id == QLatin1String("four_seven_eight");
        if (training)
            for (QGroupBox *g : dlg.findChildren<QGroupBox *>())
                if (g->isCheckable() && g->title() == QCoreApplication::translate("SessionDialog", "Trainingsmodus: Messwerte vor und nach der Übung erfassen")) g->setChecked(true);
        click(QCoreApplication::translate("SessionDialog", "Übung starten"));
        if (training) {
            wait(200);
            shot(&dlg, QStringLiteral("measure_before"));
            click(QCoreApplication::translate("SessionDialog", "Weiter zur Übung"));
        }
        const int waits[] = { 5200, 3300 };
        for (int k = 0; k < 2; ++k) {
            wait(waits[k]);
            shot(&dlg, QStringLiteral("run_%1_%2").arg(m.id).arg(k));
        }
        engine->stop();
        wait(700);
        if (training) {
            shot(&dlg, QStringLiteral("measure_after"));
            click(QCoreApplication::translate("SessionDialog", "Weiter"));
            wait(200);
        }
        shot(&dlg, QStringLiteral("summary_%1").arg(m.id));
        click(QCoreApplication::translate("SessionDialog", "Im Tagebuch speichern"));
        dlg.hide();
    }
}

} // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("mbergmann-sh"));
    QApplication::setApplicationName(QStringLiteral("ViviaBreathe"));
    QApplication::setApplicationVersion(vbVersion());
    QApplication::setWindowIcon(Icons::app());
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("ViviaBreathe – geführte Atemübungen mit Tagebuch"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption demo(QStringLiteral("demo"), QStringLiteral("Demo mit erfundenen Daten (Master-Passwort: demo)"));
    QCommandLineOption shots(QStringLiteral("screenshots"), QStringLiteral("Bildschirmfotos erzeugen (mit --demo)"),
                             QStringLiteral("ordner"));
    QCommandLineOption dbOpt(QStringLiteral("database"), QStringLiteral("Datenbankdatei"), QStringLiteral("datei"));
    parser.addOptions({ demo, shots, dbOpt });
    parser.process(app);
    const bool isDemo = parser.isSet(demo);
    if (isDemo) QApplication::setApplicationName(QStringLiteral("ViviaBreathe-Demo"));   // eigene Einstellungen

    // Bis 0.4.0 hieß das Programm „VitaBreathe“: Einstellungen und Datenbank einmalig übernehmen
    if (!isDemo) LegacyImport::run();

    // Sprache (Vorgabe Deutsch, alternativ Englisch) – vor dem ersten Fenster
    I18n::install(app);

    Themes::init();
    ScreenFit::install(app);
    WindowState::install(app);   // nach ScreenFit: erst wiederherstellen, dann an den Bildschirm anpassen

    QString path = parser.value(dbOpt);
    if (path.isEmpty()) path = QSettings().value(QStringLiteral("db/path"), Database::defaultPath()).toString();
    if (isDemo) path = QDir::temp().filePath(QStringLiteral("viviabreathe-demo.sqlite"));
    if (!openDatabase(path, isDemo)) return 0;
    if (!isDemo) QSettings().setValue(QStringLiteral("db/path"), Database::instance().path());

    MainWindow w;
    w.show();

    if (isDemo && parser.isSet(shots)) {
        QTimer::singleShot(300, &w, [&w, &parser, &shots] {
            takeScreenshots(&w, parser.value(shots));
            w.setQuitting();
            w.close();
        });
        return app.exec();
    }

    if (!isDemo && QSettings().value(QStringLiteral("ui/startNotice"), true).toBool()) {
        QTimer::singleShot(400, &w, [&w] {
            QMessageBox box(&w);
            box.setWindowTitle(QObject::tr("Hinweis"));
            box.setIcon(QMessageBox::Information);
            box.setText(QObject::tr("<b>ViviaBreathe ist kein Medizinprodukt.</b><br/>Es ist ein persönliches Übungs-Tagebuch "
                                    "und ersetzt keine ärztliche Beratung. Brechen Sie eine Übung bei Schwindel, Unwohlsein "
                                    "oder Atemnot sofort ab."));
            auto *dont = new QCheckBox(QObject::tr("Nicht mehr anzeigen"));
            box.setCheckBox(dont);
            box.exec();
            if (dont->isChecked()) QSettings().setValue(QStringLiteral("ui/startNotice"), false);
        });
    }

    // Abgleich beim Start
    const DbSync::Config c = DbSync::config();
    if (!isDemo && c.enabled && c.autoSync && DbSync::isReady(c)) {
        QTimer::singleShot(1500, &DbSync::instance(), [] { DbSync::instance().sync(); });
        DbSync::instance().watchDatabase();
    } else if (!isDemo) {
        DbSync::instance().watchDatabase();
    }
    return app.exec();
}
