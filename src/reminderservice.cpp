// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "reminderservice.h"
#include "audio.h"
#include "breathmodules.h"
#include "database.h"
#include "icons.h"
#include "schedule.h"

#include <QApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

// ====================================================================== Wecker klingelt

AlarmRingDialog::AlarmRingDialog(const Alarm &a, const QString &userName, QWidget *parent)
    : QDialog(parent), m_alarm(a)
{
    setWindowTitle(tr("Wecker – ViviaBreathe"));
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    auto *lay = new QVBoxLayout(this);
    auto *icon = new QLabel;
    icon->setPixmap(Icons::get(QStringLiteral("alarm")).pixmap(64, 64));
    icon->setAlignment(Qt::AlignCenter);
    lay->addWidget(icon);
    m_clock = new QLabel(QTime::currentTime().toString(QStringLiteral("HH:mm")));
    QFont f = m_clock->font();
    f.setPointSizeF(f.pointSizeF() * 4);
    f.setBold(true);
    m_clock->setFont(f);
    m_clock->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_clock);
    auto *text = new QLabel(tr("Guten Morgen, %1!").arg(userName.toHtmlEscaped())
                            + (a.label.isEmpty() ? QString() : QStringLiteral("<br/><i>%1</i>").arg(a.label.toHtmlEscaped())));
    text->setAlignment(Qt::AlignCenter);
    QFont tf = text->font();
    tf.setPointSizeF(tf.pointSizeF() * 1.3);
    text->setFont(tf);
    lay->addWidget(text);
    auto *bl = new QHBoxLayout;
    auto *snooze = new QPushButton(Icons::get(QStringLiteral("moon")), tr("Schlummern (%1 min)").arg(a.snoozeMin));
    auto *off = new QPushButton(Icons::get(QStringLiteral("stop")), tr("Aus"));
    bl->addWidget(snooze);
    bl->addWidget(off);
    if (!a.moduleId.isEmpty()) {
        auto *ex = new QPushButton(Icons::get(QStringLiteral("play")),
                                   tr("Aus und %1 starten").arg(BreathModules::displayName(a.moduleId)));
        bl->addWidget(ex);
        connect(ex, &QPushButton::clicked, this, [this] {
            emit exerciseRequested(m_alarm.userId, m_alarm.moduleId);
            accept();
        });
    }
    lay->addLayout(bl);
    off->setDefault(true);
    connect(snooze, &QPushButton::clicked, this, [this] {
        emit snoozed(m_alarm);
        accept();
    });
    connect(off, &QPushButton::clicked, this, &QDialog::accept);

    auto *clock = new QTimer(this);
    connect(clock, &QTimer::timeout, this, [this] { m_clock->setText(QTime::currentTime().toString(QStringLiteral("HH:mm"))); });
    clock->start(5000);

    m_player = new MusicPlayer(this);
    m_player->setLoop(true);
    QStringList files;
    for (const QString &fn : a.playlist)
        if (QFileInfo::exists(fn)) files << fn;
    if (files.isEmpty()) files << Tones::builtinAlarmFile();
    m_player->setPlaylist(files, a.shuffle);
    // falls eine Datei nicht abspielbar ist: eingebauter Weckton
    connect(m_player, &MusicPlayer::stopped, this, [this] {
        if (!isVisible() || property("vbFallback").toBool()) return;
        setProperty("vbFallback", true);
        m_player->setPlaylist({ Tones::builtinAlarmFile() });
        m_player->setVolume(m_alarm.volume);
        m_player->play();
    });
    if (MusicPlayer::available()) {
        m_player->setVolume(a.fadeInMin > 0 ? 5 : a.volume);
        m_player->play();
        if (a.fadeInMin > 0) m_player->fadeTo(a.volume, a.fadeInMin * 60000);
    } else {
        auto *beep = new QTimer(this);
        connect(beep, &QTimer::timeout, this, [] { QApplication::beep(); });
        beep->start(1200);
    }
    // nach 30 Minuten von selbst verstummen
    QTimer::singleShot(30 * 60 * 1000, this, &QDialog::accept);
}

AlarmRingDialog::~AlarmRingDialog()
{
    disconnect(m_player, nullptr, this, nullptr);
    m_player->stop();
}

// ====================================================================== Dienst

ReminderService::ReminderService(QObject *parent) : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(15000);
    connect(m_timer, &QTimer::timeout, this, &ReminderService::check);
}

void ReminderService::start()
{
    m_last = QDateTime::currentDateTime();
    m_timer->start();
}

void ReminderService::stop()
{
    m_timer->stop();
}

QString ReminderService::nextText(qint64 userId) const
{
    if (!Database::instance().isOpen()) return {};
    QDateTime best;
    QString what;
    for (const BreathEvent &e : Database::instance().events(userId)) {
        if (!e.enabled) continue;
        const QDateTime n = Schedule::next(e.time, e.weekdays);
        if (n.isValid() && (!best.isValid() || n < best)) { best = n; what = e.name; }
    }
    for (const Alarm &a : Database::instance().alarms(userId)) {
        if (!a.enabled) continue;
        const QDateTime n = Schedule::next(a.time, a.weekdays);
        if (n.isValid() && (!best.isValid() || n < best)) { best = n; what = tr("Wecker"); }
    }
    for (const SleepPlan &p : Database::instance().sleepPlans(userId)) {
        if (!p.scheduled) continue;
        const QDateTime n = Schedule::next(p.time, p.weekdays);
        if (n.isValid() && (!best.isValid() || n < best)) { best = n; what = p.label; }
    }
    if (!best.isValid()) return {};
    return tr("%1 %2").arg(what, Schedule::relative(best));
}

void ReminderService::check()
{
    Database &db = Database::instance();
    if (!db.isOpen()) return;
    const QDateTime now = QDateTime::currentDateTime();
    QDateTime from = m_last;
    // Rechner im Ruhezustand: nur Verpasstes der letzten 30 Minuten nachholen
    if (from.secsTo(now) > 30 * 60) from = now.addSecs(-30 * 60);
    m_last = now;

    for (const BreathEvent &e : db.events())
        if (e.enabled && Schedule::dueBetween(e.time, e.weekdays, from, now)) remind(e);
    for (const Alarm &a : db.alarms())
        if (a.enabled && Schedule::dueBetween(a.time, a.weekdays, from, now)) ringAlarm(a);
    for (const SleepPlan &p : db.sleepPlans())
        if (p.scheduled && Schedule::dueBetween(p.time, p.weekdays, from, now)) {
            if (p.autoStart) {
                emit trayMessage(tr("Einschlafen: %1").arg(p.label), tr("Gute Nacht!"));
                emit sleepRequested(p);
            } else {
                offerSleep(p);
            }
        }

    // Schlummern / später erinnern
    for (int i = m_snoozes.size() - 1; i >= 0; --i) {
        const Snooze s = m_snoozes.at(i);
        if (s.at > now) continue;
        m_snoozes.removeAt(i);
        if (s.kind == Kind::Alarm) {
            for (const Alarm &a : db.alarms())
                if (a.id == s.id) ringAlarm(a);
        } else if (s.kind == Kind::Sleep) {
            for (const SleepPlan &p : db.sleepPlans())
                if (p.id == s.id) offerSleep(p);
        } else {
            for (const BreathEvent &e : db.events())
                if (e.id == s.id) remind(e);
        }
    }
}

void ReminderService::remind(const BreathEvent &e)
{
    const auto user = Database::instance().user(e.userId);
    const QString userName = user ? user->name : QString();
    if (e.sound) Tones::play(Tones::Kind::Reminder, 80);
    emit trayMessage(tr("Zeit für: %1").arg(e.name), tr("%1 – %2").arg(userName, BreathModules::displayName(e.moduleId)));

    auto *dlg = new QDialog;
    dlg->setObjectName(QStringLiteral("ReminderDialog"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Erinnerung – ViviaBreathe"));
    dlg->setWindowFlags(dlg->windowFlags() | Qt::WindowStaysOnTopHint);
    dlg->setWindowIcon(Icons::app());
    auto *lay = new QVBoxLayout(dlg);
    auto *head = new QHBoxLayout;
    auto *icon = new QLabel;
    icon->setPixmap(Icons::get(QStringLiteral("bell")).pixmap(48, 48));
    head->addWidget(icon, 0, Qt::AlignTop);
    auto *text = new QLabel(tr("<p style='font-size:large'><b>Zeit für: %1</b></p><p>für <b>%2</b> · %3 um %4</p>%5")
                                .arg(e.name.toHtmlEscaped(), userName.toHtmlEscaped(),
                                     BreathModules::displayName(e.moduleId).toHtmlEscaped(),
                                     e.time.toString(QStringLiteral("HH:mm")),
                                     e.note.isEmpty() ? QString() : QStringLiteral("<p><i>%1</i></p>").arg(e.note.toHtmlEscaped())));
    text->setWordWrap(true);
    head->addWidget(text, 1);
    lay->addLayout(head);
    auto *bl = new QHBoxLayout;
    auto *startBtn = new QPushButton(Icons::get(QStringLiteral("play")), tr("Jetzt starten"));
    auto *later = new QPushButton(tr("In 10 Minuten erinnern"));
    auto *close = new QPushButton(tr("Schließen"));
    startBtn->setDefault(true);
    bl->addWidget(startBtn);
    bl->addWidget(later);
    bl->addStretch();
    bl->addWidget(close);
    lay->addLayout(bl);
    const BreathEvent ev = e;
    connect(startBtn, &QPushButton::clicked, dlg, [this, dlg, ev] {
        dlg->accept();
        emit startRequested(ev.userId, ev.moduleId, ev.name);
    });
    connect(later, &QPushButton::clicked, dlg, [this, dlg, ev] {
        m_snoozes << Snooze{ ev.id, Kind::Event, QDateTime::currentDateTime().addSecs(600) };
        dlg->accept();
    });
    connect(close, &QPushButton::clicked, dlg, &QDialog::accept);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    m_open << dlg;
}

void ReminderService::ringAlarm(const Alarm &a)
{
    const auto user = Database::instance().user(a.userId);
    auto *dlg = new AlarmRingDialog(a, user ? user->name : QString());
    dlg->setWindowIcon(Icons::app());
    connect(dlg, &AlarmRingDialog::snoozed, this, [this](const Alarm &al) {
        m_snoozes << Snooze{ al.id, Kind::Alarm, QDateTime::currentDateTime().addSecs(al.snoozeMin * 60) };
    });
    connect(dlg, &AlarmRingDialog::exerciseRequested, this, [this](qint64 userId, const QString &moduleId) {
        emit startRequested(userId, moduleId, tr("Morgenroutine"));
    });
    emit trayMessage(tr("Wecker"), a.label.isEmpty() ? a.time.toString(QStringLiteral("HH:mm")) : a.label);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    m_open << dlg;
}

void ReminderService::offerSleep(const SleepPlan &p)
{
    const auto user = Database::instance().user(p.userId);
    const QString userName = user ? user->name : QString();
    Tones::play(Tones::Kind::Reminder, 60);
    emit trayMessage(tr("Einschlafen: %1").arg(p.label), userName);

    auto *dlg = new QDialog;
    dlg->setObjectName(QStringLiteral("SleepOfferDialog"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Einschlafen – ViviaBreathe"));
    dlg->setWindowFlags(dlg->windowFlags() | Qt::WindowStaysOnTopHint);
    dlg->setWindowIcon(Icons::app());
    auto *lay = new QVBoxLayout(dlg);
    auto *head = new QHBoxLayout;
    auto *icon = new QLabel;
    icon->setPixmap(Icons::get(QStringLiteral("moon")).pixmap(48, 48));
    head->addWidget(icon, 0, Qt::AlignTop);
    QString detail = tr("%n Titel", nullptr, p.playlist.size()) + QStringLiteral(" · ") + tr("%1 min").arg(p.minutes);
    if (!p.moduleId.isEmpty()) detail += QStringLiteral(" · ") + tr("vorher: %1").arg(BreathModules::displayName(p.moduleId));
    auto *text = new QLabel(tr("<p style='font-size:large'><b>Zeit zum Einschlafen: %1</b></p><p>für <b>%2</b> · %3</p>")
                                .arg(p.label.toHtmlEscaped(), userName.toHtmlEscaped(), detail.toHtmlEscaped()));
    text->setWordWrap(true);
    head->addWidget(text, 1);
    lay->addLayout(head);
    auto *bl = new QHBoxLayout;
    auto *startBtn = new QPushButton(Icons::get(QStringLiteral("moon")), tr("Jetzt einschlafen"));
    auto *later = new QPushButton(tr("In 10 Minuten erinnern"));
    auto *close = new QPushButton(tr("Schließen"));
    startBtn->setDefault(true);
    bl->addWidget(startBtn);
    bl->addWidget(later);
    bl->addStretch();
    bl->addWidget(close);
    lay->addLayout(bl);
    const SleepPlan plan = p;
    connect(startBtn, &QPushButton::clicked, dlg, [this, dlg, plan] {
        dlg->accept();
        emit sleepRequested(plan);
    });
    connect(later, &QPushButton::clicked, dlg, [this, dlg, plan] {
        m_snoozes << Snooze{ plan.id, Kind::Sleep, QDateTime::currentDateTime().addSecs(600) };
        dlg->accept();
    });
    connect(close, &QPushButton::clicked, dlg, &QDialog::accept);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    m_open << dlg;
}
