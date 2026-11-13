// ViviaBreathe – Taktgeber einer Atemsitzung
//
// Läuft unabhängig von der Darstellung: Vorlauf (3-2-1), dann Runden aus Schritten (siehe
// ModuleSettings::round()), bis die Gesamtdauer erreicht ist. Die Übung endet nicht mitten in
// einem Atemzug, sondern nach der laufenden Runde. Ein Schritt "Ready" wartet, bis der Benutzer
// proceed() aufruft (z. B. nach dem Abhusten); Wartezeit zählt wie eine Pause nicht mit.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include "breathmodules.h"

#include <QElapsedTimer>
#include <QObject>

class QTimer;

class BreathEngine : public QObject
{
    Q_OBJECT
public:
    enum class State { Idle, Preroll, Running, Paused, Waiting, Finished };
    explicit BreathEngine(QObject *parent = nullptr);

    void setup(const QList<StepDef> &round, int totalSec, int prerollSec = 3);
    // einfache Variante: Phasen eines Zyklus (für Tests)
    void setup(const QList<PhaseKind> &kinds, const QVector<double> &seconds, int totalSec, int prerollSec = 3);
    void start();
    void pause();
    void resume();
    void proceed();          // nach einem Ready-Schritt weitermachen
    void stop();             // vorzeitig beenden
    State state() const { return m_state; }

    // aktueller Zustand (für die Animation)
    int     phaseIndex() const { return m_phase; }     // Index des Schritts in der Runde
    const StepDef &step() const;
    PhaseKind phaseKind() const;
    double  phaseProgress() const;     // 0…1 innerhalb des Schritts
    double  phaseRemaining() const;    // Sekunden
    double  phaseElapsed() const;
    double  elapsed() const;           // Übungszeit ohne Vorlauf, Pausen und Wartezeiten
    double  remaining() const;
    double  prerollRemaining() const;
    int     cycles() const { return m_cycles; }   // vollständige Runden
    int     totalSec() const { return m_total; }
    bool    lastCycle() const;
    const QList<StepDef> &steps() const { return m_steps; }

signals:
    void tick();
    void prerollSecond(int secondsLeft);
    void phaseChanged(int index, PhaseKind kind);
    void waitingForUser(const QString &hint);
    void cycleCompleted(int cycles);
    void finished(bool completed);

private:
    void onTimer();
    double now() const;
    int nextActiveStep(int from) const;
    bool advance();                    // zum nächsten Schritt; false = Übung beendet
    void enterStep();

    QTimer          *m_timer = nullptr;
    QElapsedTimer    m_clock;
    qint64           m_pausedMs = 0;
    qint64           m_pauseStart = 0;
    QList<StepDef>   m_steps;
    int              m_total = 300;
    int              m_preroll = 3;
    int              m_lastPrerollSecond = -1;
    State            m_state = State::Idle;
    int              m_phase = 0;
    double           m_phaseStart = 0;
    int              m_cycles = 0;
};
