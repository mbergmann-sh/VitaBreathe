// ViviaBreathe – Ablauf einer Atemsitzung
//
//  1. Vorbereitung & Einstellungen: Tipps, Gesamtdauer, Atemdauer je Phase (mit Reset),
//     Anlass, Signalton, Hintergrundmusik (MP3, eigene Lautstärke), Trainingsmodus
//  2. Trainingsmodus: Messwerte vor der Übung (SpO2, Peak Flow, Puls, Befinden)
//  3. Übung: minimalistische Animation, Phase, Restzeit – Pause/Beenden, Vollbild
//  4. Trainingsmodus: Messwerte nach der Übung
//  5. Zusammenfassung: Notiz, im Tagebuch speichern
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include "breathmodules.h"
#include "datatypes.h"

#include <QDialog>

class QStackedWidget;
class QLabel;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QSlider;
class QPushButton;
class QPlainTextEdit;
class QGroupBox;
class BreathEngine;
class BreathView;
class MusicPlayer;
class MeasureForm;
class MusicFileEdit;

class SessionDialog : public QDialog
{
    Q_OBJECT
public:
    SessionDialog(qint64 userId, const QString &moduleId, QWidget *parent = nullptr,
                  const QString &eventName = {});
    bool saved() const { return m_saved; }

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void reject() override;

private:
    QWidget *buildPrepPage();
    QWidget *buildMeasurePage(bool before);
    QWidget *buildRunPage();
    QWidget *buildSummaryPage();
    void loadSettings(const ModuleSettings &s);
    ModuleSettings currentSettings() const;
    void updateCycleInfo();
    void startExercise();
    void beginRun();
    void onPhaseChanged(int index, PhaseKind kind);
    void onFinished(bool completed);
    void updateRunInfo();
    void showSummary();
    void togglePause();
    void toggleFullScreen();
    void setSoundOn(bool on);       // Signaltöne während der Übung ein/aus (wird gespeichert)

    qint64              m_userId;
    const ModuleDef    *m_module;
    SessionRecord       m_rec;
    bool                m_saved = false;

    QStackedWidget     *m_stack;
    // Vorbereitung
    QLabel             *m_tips;
    QCheckBox          *m_showTips;
    QSpinBox           *m_total;
    QComboBox          *m_target = nullptr;     // Atemtrainer: Ziel-Bälle
    QVector<QDoubleSpinBox *> m_phaseSpins;
    QList<QSpinBox *>       m_repeatSpins;   // je Abschnitt (nullptr = fest)
    QLabel             *m_cycleInfo = nullptr;
    QComboBox          *m_event;
    QCheckBox          *m_signal, *m_distinct, *m_vibrate, *m_gong;
    QSlider            *m_signalVol;
    QGroupBox          *m_musicBox;
    MusicFileEdit      *m_musicFile;
    QSlider            *m_musicVol;
    QGroupBox          *m_trainingBox;
    QCheckBox          *m_askSpo2, *m_askPf, *m_askPulse, *m_askFeeling;
    // Messungen
    MeasureForm        *m_before = nullptr, *m_after = nullptr;
    // Übung
    BreathEngine       *m_engine;
    BreathView         *m_view;
    QLabel             *m_runTitle, *m_runInfo;
    QPushButton        *m_pauseBtn, *m_stopBtn, *m_fullBtn, *m_nextBtn = nullptr, *m_soundBtn = nullptr;
    QSlider            *m_runMusicVol;
    QLabel             *m_runMusicIcon = nullptr;
    QWidget            *m_runBar, *m_runTop;
    MusicPlayer        *m_music;
    ModuleSettings      m_settings;
    // Zusammenfassung
    QLabel             *m_summary;
    QSpinBox           *m_devBalls = nullptr, *m_devHold = nullptr;   // Atemtrainer-Ergebnis
    QPlainTextEdit     *m_note;
};
