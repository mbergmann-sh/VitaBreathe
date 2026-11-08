// VitaBreathe – Termine von Ereignissen und Weckern berechnen
// SPDX-License-Identifier: MIT
#pragma once

#include <QDateTime>
#include <QTime>

namespace Schedule {
// Nächster Zeitpunkt >= 'after' an einem der Wochentage (Bitmaske, siehe Weekdays)
QDateTime next(const QTime &time, int weekdays, const QDateTime &after = QDateTime::currentDateTime());
// Fiel ein Zeitpunkt in das Intervall (from, to]?
bool dueBetween(const QTime &time, int weekdays, const QDateTime &from, const QDateTime &to, QDateTime *when = nullptr);
QString relative(const QDateTime &when);   // "heute 21:30", "morgen 07:00", "Mo., 10.11. 07:00"
}
