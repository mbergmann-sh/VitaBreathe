// VitaBreathe – Datentypen (Benutzer, Übungseinheiten, Ereignisse, Wecker)
// SPDX-License-Identifier: MIT
#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QTime>

// Ein Wert, der nicht erfasst wurde
constexpr int kNoValue = -1;

struct User {
    qint64  id = 0;
    QString uid;
    QString name;
};

// Eine durchgeführte Übungseinheit (Tagebuch-Eintrag)
struct SessionRecord {
    qint64    id = 0;
    qint64    userId = 0;
    QString   moduleId;          // siehe BreathModules
    QString   eventName;         // Anlass, z. B. "Einschlafübung", "Stressbewältigung"
    QDateTime started;           // lokale Zeit
    int       plannedSec = 0;
    int       actualSec = 0;
    int       cycles = 0;
    bool      completed = false;
    QString   pattern;           // verwendete Phasen, z. B. "4-4-4-4"
    // Trainingsmodus (optional): kNoValue = nicht erfasst
    int       spo2Before = kNoValue, spo2After = kNoValue;     // %
    int       pfBefore = kNoValue, pfAfter = kNoValue;         // l/min
    int       pulseBefore = kNoValue, pulseAfter = kNoValue;   // /min
    int       feelingBefore = kNoValue, feelingAfter = kNoValue; // 1 (schlecht) … 10 (sehr gut)
    // Atemtrainer (z. B. TriBall): meist erreichte Bälle (1–3) und längste Haltezeit in Sekunden
    int       deviceBalls = kNoValue, deviceHoldSec = kNoValue;
    QString   note;
    bool hasMeasurements() const {
        return spo2Before >= 0 || spo2After >= 0 || pfBefore >= 0 || pfAfter >= 0
            || pulseBefore >= 0 || pulseAfter >= 0;
    }
};

// Wochentage als Bitmaske: Bit 0 = Montag … Bit 6 = Sonntag
namespace Weekdays {
constexpr int kAll = 0x7F;
constexpr int kWorkdays = 0x1F;
inline bool contains(int mask, int qtDayOfWeek) { return mask & (1 << (qtDayOfWeek - 1)); }
QString text(int mask);   // "Mo, Mi, Fr" / "täglich" / "werktags"
}

// Wiederkehrendes Übungsereignis mit Erinnerung
struct BreathEvent {
    qint64  id = 0;
    qint64  userId = 0;
    QString name;              // "Einschlafübung", "Stressbewältigung" …
    QString moduleId;          // Übung, die gestartet wird
    QTime   time = QTime(21, 30);
    int     weekdays = Weekdays::kAll;
    bool    enabled = true;
    bool    sound = true;      // Erinnerung mit Gong
    QString note;
};

// Weckfunktion
struct Alarm {
    qint64      id = 0;
    qint64      userId = 0;
    QString     label;
    QTime       time = QTime(7, 0);
    int         weekdays = Weekdays::kWorkdays;
    bool        enabled = true;
    QStringList playlist;      // MP3-Dateien; leer = eingebauter Weckton
    bool        shuffle = false;
    int         volume = 70;   // Endlautstärke 0–100
    int         fadeInMin = 2; // sanft lauter werden
    int         snoozeMin = 9;
    QString     moduleId;      // optional: Übung nach dem Aufwachen anbieten
};

// Einschlaffunktion: beliebig viele gespeicherte Einträge je Benutzer, wahlweise geplant
// (Uhrzeit + Wochentage → Erinnerung oder automatischer Start) wie beim Wecker
struct SleepPlan {
    qint64      id = 0;
    qint64      userId = 0;
    QString     label;
    QTime       time = QTime(22, 30);
    int         weekdays = Weekdays::kAll;
    bool        scheduled = false;   // zur Uhrzeit an den Wochentagen anbieten bzw. starten
    bool        autoStart = false;   // geplant: ohne Rückfrage starten
    QStringList playlist;
    bool        shuffle = false;
    int         volume = 50;
    int         minutes = 30;        // Wiedergabedauer
    int         fadeOutMin = 5;      // am Ende leiser werden
    QString     moduleId;            // optional: vorher eine Übung (z. B. 4-6)
};
