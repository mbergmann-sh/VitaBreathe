// Taktgeber-Test mit verkürzten Phasen
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "breathengine.h"
#include "breathmodules.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <cstdio>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { ++fails; std::printf("FEHLER: %s\n", msg); } else std::printf("ok: %s\n", msg); } while (0)

static void spin(int ms) { QEventLoop l; QTimer::singleShot(ms, &l, &QEventLoop::quit); l.exec(); }

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // 1. ACBT: Runde mit Abschnitten und Warten
    const ModuleDef *acbt = BreathModules::find("acbt");
    ModuleSettings st = ModuleSettings::defaults(*acbt);
    for (double &d : st.phaseSec) d = 0.1;     // stark verkürzt
    st.repeats = { 1, 3, 1, 2 };
    const QList<StepDef> round = st.round(*acbt);
    CHECK(round.size() == 1 + 3 * 3 + 1 + 2 * 3 + 1, "ACBT: 18 Schritte je Runde (inkl. Warten)");
    CHECK(round.last().kind == PhaseKind::Ready, "ACBT: Runde endet mit Abhusten/Weiter");
    BreathEngine e;
    int phases = 0, waits = 0;
    bool done = false, completed = false;
    QObject::connect(&e, &BreathEngine::phaseChanged, [&] { ++phases; });
    QObject::connect(&e, &BreathEngine::waitingForUser, [&] { ++waits; });
    QObject::connect(&e, &BreathEngine::finished, [&](bool c) { done = true; completed = c; });
    e.setup(round, 10, 0);                     // 10 s Gesamtdauer (Mindestwert)
    e.start();
    spin(2500);
    CHECK(e.state() == BreathEngine::State::Waiting, "wartet nach der ersten Runde auf „Weiter“");
    CHECK(waits == 1 && phases == 18, "18 Phasenwechsel bis zum Warten");
    const double before = e.elapsed();
    spin(800);
    CHECK(qAbs(e.elapsed() - before) < 0.05, "Wartezeit zählt nicht zur Übungszeit");
    e.proceed();
    spin(50);
    CHECK(e.state() == BreathEngine::State::Running && e.cycles() == 1, "nach „Weiter“: Runde 2 läuft");
    // bis zum Ende: immer wieder weiter
    QElapsedTimer t; t.start();
    while (!done && t.elapsed() < 30000) {
        spin(100);
        if (e.state() == BreathEngine::State::Waiting) e.proceed();
    }
    CHECK(done && completed, "Übung endet nach Erreichen der Gesamtdauer vollständig");
    CHECK(e.elapsed() >= 10.0, "Ende erst nach der laufenden Runde (nicht mittendrin)");

    // 2. Box-Atmen: abgeschaltete Haltephasen werden übersprungen
    const ModuleDef *box = BreathModules::find("box");
    ModuleSettings b = ModuleSettings::defaults(*box);
    b.phaseSec = { 0.2, 0, 0.2, 0 };
    CHECK(b.round(*box).size() == 2, "Box: Haltephasen mit 0 s entfallen");
    BreathEngine e2;
    int ch = 0;
    QObject::connect(&e2, &BreathEngine::phaseChanged, [&] { ++ch; });
    e2.setup(b.round(*box), 10, 0);
    e2.start();
    spin(1100);
    e2.pause();
    const double p0 = e2.elapsed();
    spin(500);
    CHECK(qAbs(e2.elapsed() - p0) < 0.05, "Pause hält die Zeit an");
    e2.resume();
    CHECK(ch >= 5, "Box: Phasenwechsel laufen");
    e2.stop();
    CHECK(e2.state() == BreathEngine::State::Finished, "Abbrechen beendet die Übung");

    std::printf("\n%s (%d Fehler)\n", fails ? "FEHLGESCHLAGEN" : "ALLES OK", fails);
    return fails ? 1 : 0;
}
