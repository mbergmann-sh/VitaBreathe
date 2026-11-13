// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "schedule.h"
#include "datatypes.h"

#include <QCoreApplication>
#include <QLocale>

namespace Schedule {

QDateTime next(const QTime &time, int weekdays, const QDateTime &after)
{
    if (!(weekdays & Weekdays::kAll) || !time.isValid()) return {};
    for (int i = 0; i < 8; ++i) {
        const QDate d = after.date().addDays(i);
        if (!Weekdays::contains(weekdays, d.dayOfWeek())) continue;
        const QDateTime dt(d, time);
        if (dt >= after) return dt;
    }
    return {};
}

bool dueBetween(const QTime &time, int weekdays, const QDateTime &from, const QDateTime &to, QDateTime *when)
{
    const QDateTime n = next(time, weekdays, from.addSecs(1));
    if (n.isValid() && n <= to) {
        if (when) *when = n;
        return true;
    }
    return false;
}

QString relative(const QDateTime &when)
{
    if (!when.isValid()) return QCoreApplication::translate("Schedule", "–");
    const QDate today = QDate::currentDate();
    const QString t = when.time().toString(QStringLiteral("HH:mm"));
    if (when.date() == today) return QCoreApplication::translate("Schedule", "heute %1").arg(t);
    if (when.date() == today.addDays(1)) return QCoreApplication::translate("Schedule", "morgen %1").arg(t);
    return QLocale().toString(when.date(), QStringLiteral("ddd, dd.MM.")) + QLatin1Char(' ') + t;
}

} // namespace Schedule
