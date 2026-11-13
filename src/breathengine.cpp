// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "breathengine.h"

#include <QTimer>
#include <QtMath>

BreathEngine::BreathEngine(QObject *parent) : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, &BreathEngine::onTimer);
}

void BreathEngine::setup(const QList<StepDef> &round, int totalSec, int prerollSec)
{
    m_steps = round;
    m_total = qMax(10, totalSec);
    m_preroll = qMax(0, prerollSec);
    m_state = State::Idle;
}

void BreathEngine::setup(const QList<PhaseKind> &kinds, const QVector<double> &seconds, int totalSec, int prerollSec)
{
    QList<StepDef> steps;
    for (int i = 0; i < kinds.size(); ++i) {
        StepDef s;
        s.kind = kinds.at(i);
        s.sec = seconds.value(i);
        s.phaseIndex = i;
        steps << s;
    }
    setup(steps, totalSec, prerollSec);
}

double BreathEngine::now() const
{
    if (!m_clock.isValid()) return 0;
    qint64 ms = m_clock.elapsed() - m_pausedMs;
    if (m_state == State::Paused || m_state == State::Waiting) ms -= (m_clock.elapsed() - m_pauseStart);
    return ms / 1000.0 - m_preroll;     // negativ während des Vorlaufs
}

int BreathEngine::nextActiveStep(int from) const
{
    for (int i = 0; i < m_steps.size(); ++i) {
        const int p = (from + i) % m_steps.size();
        const StepDef &s = m_steps.at(p);
        if (s.kind == PhaseKind::Ready || s.sec > 0.05) return p;
    }
    return -1;
}

const StepDef &BreathEngine::step() const
{
    static const StepDef empty;
    return (m_phase >= 0 && m_phase < m_steps.size()) ? m_steps.at(m_phase) : empty;
}

void BreathEngine::start()
{
    // mindestens ein zeitgesteuerter Schritt nötig
    bool timed = false;
    for (const StepDef &s : m_steps) timed |= s.sec > 0.05;
    if (!timed) return;
    m_pausedMs = 0;
    m_cycles = 0;
    m_phase = nextActiveStep(0);
    m_phaseStart = 0;
    m_lastPrerollSecond = -1;
    m_clock.start();
    m_state = m_preroll > 0 ? State::Preroll : State::Running;
    m_timer->start();
    if (m_state == State::Running) enterStep();
    onTimer();
}

void BreathEngine::pause()
{
    if (m_state != State::Running && m_state != State::Preroll) return;
    m_pauseStart = m_clock.elapsed();
    m_state = State::Paused;
    emit tick();
}

void BreathEngine::resume()
{
    if (m_state != State::Paused) return;
    m_pausedMs += m_clock.elapsed() - m_pauseStart;
    m_state = now() < 0 ? State::Preroll : State::Running;
    emit tick();
}

void BreathEngine::proceed()
{
    if (m_state != State::Waiting) return;
    m_pausedMs += m_clock.elapsed() - m_pauseStart;
    m_state = State::Running;
    m_phaseStart = now();
    if (advance()) emit tick();
}

void BreathEngine::stop()
{
    if (m_state == State::Idle || m_state == State::Finished) return;
    if (m_state == State::Paused || m_state == State::Waiting) m_pausedMs += m_clock.elapsed() - m_pauseStart;
    m_timer->stop();
    m_state = State::Finished;
    emit finished(false);
}

PhaseKind BreathEngine::phaseKind() const { return step().kind; }

double BreathEngine::phaseElapsed() const
{
    return qMax(0.0, now() - m_phaseStart);
}

double BreathEngine::phaseProgress() const
{
    if (m_state == State::Preroll || m_state == State::Idle) return 0;
    if (m_state == State::Finished) return 1;
    const double d = step().sec;
    if (d <= 0) return 0;
    return qBound(0.0, (now() - m_phaseStart) / d, 1.0);
}

double BreathEngine::phaseRemaining() const
{
    return qMax(0.0, step().sec - (now() - m_phaseStart));
}

double BreathEngine::elapsed() const { return qMax(0.0, now()); }
double BreathEngine::remaining() const { return qMax(0.0, m_total - elapsed()); }
double BreathEngine::prerollRemaining() const { return qMax(0.0, -now()); }
bool BreathEngine::lastCycle() const { return elapsed() >= m_total; }

void BreathEngine::enterStep()
{
    emit phaseChanged(m_phase, phaseKind());
    if (phaseKind() == PhaseKind::Ready) {
        m_pauseStart = m_clock.elapsed();
        m_state = State::Waiting;
        emit waitingForUser(step().hint);
    }
}

bool BreathEngine::advance()
{
    const int next = nextActiveStep(m_phase + 1);
    if (next <= m_phase) {                 // Runde vollständig
        ++m_cycles;
        emit cycleCompleted(m_cycles);
        if (m_phaseStart >= m_total) {
            m_timer->stop();
            m_state = State::Finished;
            emit tick();
            emit finished(true);
            return false;
        }
    }
    m_phase = next;
    enterStep();
    return true;
}

void BreathEngine::onTimer()
{
    if (m_state != State::Running && m_state != State::Preroll) return;
    const double t = now();
    if (m_state == State::Preroll) {
        if (t < 0) {
            const int s = int(qCeil(-t));
            if (s != m_lastPrerollSecond) {
                m_lastPrerollSecond = s;
                emit prerollSecond(s);
            }
            emit tick();
            return;
        }
        m_state = State::Running;
        m_phaseStart = 0;
        enterStep();
        if (m_state != State::Running) { emit tick(); return; }
    }
    // Schritte weiterschalten (auch mehrere, falls der Rechner kurz hing)
    while (m_state == State::Running && t - m_phaseStart >= step().sec) {
        m_phaseStart += step().sec;
        if (!advance()) return;
    }
    emit tick();
}
