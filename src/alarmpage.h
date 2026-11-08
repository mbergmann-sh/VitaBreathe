// VitaBreathe – Weckfunktion und Einschlaffunktion
//
// Beide verwalten beliebig viele gespeicherte Einträge je Benutzer (Wecker bzw. Einschlafen) mit
// eigener MP3-Wiedergabeliste. Einschlaf-Einträge lassen sich planen (Uhrzeit, Wochentage): Zur
// Uhrzeit erinnert VitaBreathe daran oder startet sie automatisch (siehe ReminderService).
// SPDX-License-Identifier: MIT
#pragma once

#include "datatypes.h"

#include <QDialog>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTableWidget;
class QTimeEdit;
class QTimer;
class MusicPlayer;
class PlaylistWidget;
class WeekdayPicker;

class AlarmDialog : public QDialog
{
    Q_OBJECT
public:
    AlarmDialog(const Alarm &a, QWidget *parent = nullptr);
    Alarm alarm() const;

private:
    Alarm           m_alarm;
    QLineEdit      *m_label;
    QTimeEdit      *m_time;
    WeekdayPicker  *m_days;
    QCheckBox      *m_enabled, *m_shuffle;
    PlaylistWidget *m_playlist;
    QSlider        *m_volume;
    QSpinBox       *m_fade, *m_snooze;
    QComboBox      *m_module;
};

class SleepPlanDialog : public QDialog
{
    Q_OBJECT
public:
    SleepPlanDialog(const SleepPlan &p, QWidget *parent = nullptr);
    SleepPlan plan() const;

private:
    SleepPlan       m_plan;
    QLineEdit      *m_label;
    QCheckBox      *m_scheduled, *m_autoStart, *m_shuffle;
    QTimeEdit      *m_time;
    WeekdayPicker  *m_days;
    PlaylistWidget *m_playlist;
    QSlider        *m_volume;
    QSpinBox       *m_minutes, *m_fade;
    QComboBox      *m_module;
};

class AlarmPage : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmPage(QWidget *parent = nullptr);
    void setUser(qint64 userId);
    void reload();
    void addAlarm();
    void addSleepPlan();
    void startSleep();                     // gewählten bzw. einzigen Eintrag starten (fragt bei mehreren)
    void startSleep(const SleepPlan &p);   // ggf. erst die Übung, dann Musik
    void startSleepMusic();                // nach der Übung: Musik des laufenden Eintrags
    void stopSleep();
    bool sleepRunning() const;

signals:
    void sleepExerciseRequested(const QString &moduleId);
    void testAlarm(const Alarm &a);

private:
    qint64 selectedAlarm() const;
    qint64 selectedPlan() const;
    void editAlarm();
    void editPlan();
    void updateSleepStatus();

    qint64          m_userId = 0;
    QTableWidget   *m_table;
    QPushButton    *m_edit, *m_delete, *m_test;
    QList<Alarm>    m_alarms;
    // Einschlafen
    QTableWidget   *m_sleepTable;
    QPushButton    *m_sleepEdit, *m_sleepDelete, *m_sleepStart, *m_sleepStop;
    QLabel         *m_sleepStatus;
    QList<SleepPlan> m_plans;
    SleepPlan       m_active;              // laufender Eintrag
    MusicPlayer    *m_sleepPlayer;
    QTimer         *m_sleepTimer;
    qint64          m_sleepEnd = 0;        // ms seit Epoche
    bool            m_sleepFading = false;
};
