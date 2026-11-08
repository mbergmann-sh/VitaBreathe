// VitaBreathe – Übungsmodule (Atemmuster)
//
// Jedes Modul beschreibt ein Atemmuster: Phasen (Einatmen, Halten, Ausatmen, Halten, ruhig
// atmen, Huffen) mit Standarddauern, Standard-Übungsdauer, Tipps und die Art der Animation.
// Einfache Module wiederholen ihre Phasen als Zyklus. Zusammengesetzte Module (z. B. ACBT)
// gliedern eine Runde in Abschnitte mit eigener Wiederholungszahl; nach einem Abschnitt kann
// die Übung auf den Benutzer warten ("weiter, wenn bereit" – z. B. zum Abhusten).
//
// Neue Module werden in breathmodules.cpp (BreathModules::all()) ergänzt – Einstellungen,
// Hintergrundmusik, Signaltöne, Trainingsmodus, Erinnerungen und Verlauf stehen ihnen
// automatisch zur Verfügung.
// SPDX-License-Identifier: MIT
#pragma once

#include <QColor>
#include <QCoreApplication>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

enum class PhaseKind {
    Inhale,     // einatmen
    HoldIn,     // Luft anhalten (nach dem Einatmen)
    Exhale,     // ausatmen
    HoldOut,    // halten (nach dem Ausatmen)
    Calm,       // ruhig und frei atmen (Atemkontrolle)
    Huff,       // Huffen: mit offenem Mund kräftig aushauchen
    Ready       // warten, bis der Benutzer „Weiter“ wählt (z. B. Abhusten)
};

QString phaseName(PhaseKind k);          // "Einatmen", "Luft anhalten" …

struct PhaseDef {
    PhaseKind kind;
    double    defaultSec;
    double    minSec;     // 0 = Phase lässt sich abschalten
    double    maxSec;
    QString   hint;       // "ruhig und tief durch die Nase"
};

// Abschnitt einer Runde: Phasen [firstPhase, firstPhase + phaseCount) werden 'repeats'-mal
// durchlaufen; danach wartet die Übung ggf. auf den Benutzer (readyAfter).
struct SegmentDef {
    QString title;
    int     firstPhase = 0;
    int     phaseCount = 0;
    int     defaultRepeats = 1, minRepeats = 1, maxRepeats = 1;
    bool    readyAfter = false;
    QString readyHint;
};

// Ein Schritt einer Runde, wie ihn der Taktgeber abarbeitet
struct StepDef {
    PhaseKind kind = PhaseKind::Inhale;
    double    sec = 0;          // bei Ready: 0 = unbegrenzt (bis „Weiter“)
    int       phaseIndex = -1;  // Index in ModuleDef::phases (-1 bei Ready)
    int       segment = 0;
    QString   hint;
    QString   segmentTitle;
};

struct ModuleDef {
    enum class Visual { Box, Circle };
    QString           id;
    QString           name;          // "Box-Atmen (4-4-4-4)"
    QString           shortName;     // "Box-Atmen"
    QString           category;      // Gruppe in der Übersicht
    QString           description;
    QStringList       tips;          // Tipps vor dem Start
    QList<PhaseDef>   phases;
    QList<SegmentDef> segments;      // leer = ein Abschnitt aus allen Phasen
    int               defaultTotalSec = 300;
    int               maxTotalSec = 3600;
    Visual            visual = Visual::Circle;
    QString           icon;          // Name eines Symbols in resources/icons/ui
    QString           defaultEvent;  // vorgeschlagener Anlass
    QString           posture;       // empfohlene Haltung (z. B. Kutschersitz)
    QString           emergencyNote; // deutlicher Hinweis während der Übung
    bool              quickHelp = false;   // Schnellstart ohne Vorbereitungsseite
    bool              airway = false;      // Übung aus der Atemphysiotherapie (Asthma/COPD)

    QList<SegmentDef> effectiveSegments() const;
    bool hasSegments() const { return !segments.isEmpty(); }
    QString defaultPattern() const;
};

namespace BreathModules {
const QList<ModuleDef> &all();
const ModuleDef *find(const QString &id);
QString displayName(const QString &id);   // auch für unbekannte (z. B. aus neuerer Version)
QStringList generalTips();
QStringList airwayTips();
QString relaxCategory();
QString airwayCategory();
}

// Einstellungen eines Moduls für einen Benutzer (in der Datenbank gespeichert)
struct ModuleSettings {
    QVector<double> phaseSec;          // je Phase des Moduls
    QVector<int>    repeats;           // je Abschnitt
    int     totalSec = 300;
    bool    signal = true;             // Signalton beim Phasenwechsel
    bool    distinctTones = true;      // unterschiedliche Töne je Phase
    int     signalVolume = 70;
    bool    vibrate = true;            // nur auf Android-Geräten
    bool    musicEnabled = false;
    QString musicFile;                 // MP3 als Hintergrundmusik
    int     musicVolume = 40;
    bool    trainingMode = false;      // Messwerte vor/nach der Übung erfassen
    bool    askSpo2 = true, askPeakFlow = true, askPulse = true, askFeeling = true;
    QString eventName;
    bool    showTips = true;

    static ModuleSettings defaults(const ModuleDef &m);
    static ModuleSettings load(qint64 userId, const ModuleDef &m);
    void save(qint64 userId, const ModuleDef &m) const;
    QString pattern() const;           // "4-7-8"
    double cycleSec() const;           // Summe aller Phasen (einfache Module)
    double roundSec(const ModuleDef &m) const;   // Dauer einer Runde ohne Wartezeiten
    QList<StepDef> round(const ModuleDef &m) const;
};

QString formatSeconds(double s);        // "4", "4,5"
QString formatDuration(int sec);        // "5:00 min"
