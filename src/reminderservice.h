// VitaBreathe – Erinnerungssystem und Wecker
//
// Prüft alle 15 Sekunden die Ereignisse, Wecker und geplanten Einschlaf-Einträge aller Benutzer. Erinnerungen erscheinen als
// Hinweisfenster (mit Gong) und als Meldung im Infobereich; der Wecker spielt die Wiedergabeliste
// (sanft lauter werdend) bzw. den eingebauten Weckton, mit Schlummern.
// SPDX-License-Identifier: MIT
#pragma once

#include "datatypes.h"

#include <QDateTime>
#include <QDialog>
#include <QObject>
#include <QPointer>

class QLabel;
class QTimer;
class MusicPlayer;

class AlarmRingDialog : public QDialog
{
    Q_OBJECT
public:
    AlarmRingDialog(const Alarm &a, const QString &userName, QWidget *parent = nullptr);
    ~AlarmRingDialog() override;

signals:
    void snoozed(const Alarm &a);
    void exerciseRequested(qint64 userId, const QString &moduleId);

private:
    Alarm        m_alarm;
    MusicPlayer *m_player;
    QLabel      *m_clock;
};

class ReminderService : public QObject
{
    Q_OBJECT
public:
    explicit ReminderService(QObject *parent = nullptr);
    void start();
    void stop();
    void ringAlarm(const Alarm &a);            // auch zum Testen
    QString nextText(qint64 userId) const;     // "Einschlafübung heute 21:30"

signals:
    void startRequested(qint64 userId, const QString &moduleId, const QString &eventName);
    void trayMessage(const QString &title, const QString &text);
    void sleepRequested(const SleepPlan &plan);   // geplantes Einschlafen starten

private:
    void check();
    void remind(const BreathEvent &e);
    void offerSleep(const SleepPlan &p);

    QTimer   *m_timer;
    QDateTime m_last;
    enum class Kind { Event, Alarm, Sleep };
    struct Snooze { qint64 id; Kind kind; QDateTime at; };
    QList<Snooze> m_snoozes;
    QList<QPointer<QDialog>> m_open;
};
