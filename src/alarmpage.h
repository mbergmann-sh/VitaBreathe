// VitaBreathe – Weckfunktion und Einschlaffunktion (jeweils mit eigener MP3-Wiedergabeliste)
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

class AlarmPage : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmPage(QWidget *parent = nullptr);
    void setUser(qint64 userId);
    void reload();
    void addAlarm();
    void startSleep();                 // Einschlaffunktion starten (ggf. mit Übung vorher)
    void startSleepMusic();            // nach der Übung: Musik mit Ausblenden
    void stopSleep();
    bool sleepRunning() const;

signals:
    void sleepExerciseRequested(const QString &moduleId);
    void testAlarm(const Alarm &a);

private:
    qint64 selectedId() const;
    void editSelected();
    void saveSleep();
    void updateSleepStatus();

    qint64          m_userId = 0;
    QTableWidget   *m_table;
    QPushButton    *m_edit, *m_delete, *m_test;
    QList<Alarm>    m_alarms;
    // Einschlafen
    PlaylistWidget *m_sleepList;
    QCheckBox      *m_sleepShuffle;
    QSlider        *m_sleepVolume;
    QSpinBox       *m_sleepMinutes, *m_sleepFade;
    QComboBox      *m_sleepModule;
    QPushButton    *m_sleepStart, *m_sleepStop;
    QLabel         *m_sleepStatus;
    MusicPlayer    *m_sleepPlayer;
    QTimer         *m_sleepTimer;
    qint64          m_sleepEnd = 0;      // ms seit Epoche
    bool            m_sleepFading = false;
    bool            m_loading = false;
};
