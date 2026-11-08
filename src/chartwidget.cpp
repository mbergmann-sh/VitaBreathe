// SPDX-License-Identifier: MIT
#include "chartwidget.h"
#include "breathmodules.h"

#include <QCoreApplication>
#include <QLocale>
#include <QMap>
#include <QSet>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

// Übersetzungskontext "ChartRenderer" (für lupdate erkennbar)
struct ChartTr { Q_DECLARE_TR_FUNCTIONS(ChartRenderer) };

namespace ChartRenderer {

QColor moduleColor(const QString &id)
{
    if (id == QLatin1String("box")) return QColor(0x14, 0x74, 0xBC);
    if (id == QLatin1String("four_seven_eight")) return QColor(0x2E, 0x9E, 0x6B);
    if (id == QLatin1String("four_six")) return QColor(0x8E, 0x5B, 0xC7);
    if (id == QLatin1String("relief")) return QColor(0xC0, 0x39, 0x2B);
    if (id == QLatin1String("pursed_lip")) return QColor(0x17, 0xA2, 0xB8);
    if (id == QLatin1String("diaphragm")) return QColor(0xD4, 0x9A, 0x1C);
    if (id == QLatin1String("nasal_calm")) return QColor(0x5D, 0x6D, 0x7E);
    if (id == QLatin1String("acbt")) return QColor(0x8D, 0x5B, 0x3A);
    if (id == QLatin1String("triball")) return QColor(0xD8, 0x4F, 0x9A);
    // weitere Module: aus der Kennung abgeleitet
    const uint h = qHash(id) % 360;
    return QColor::fromHsv(int(h), 140, 190);
}

namespace {

struct Series {
    QString name;
    QColor color;
    bool filled;
    Qt::PenStyle style;
    QList<QPair<QDateTime, double>> points;
};

void niceRange(double &lo, double &hi, double &step, int ticks = 4)
{
    if (hi - lo < 1e-9) { lo -= 1; hi += 1; }
    const double raw = (hi - lo) / qMax(1, ticks);
    const double mag = qPow(10, qFloor(std::log10(raw)));
    const double n = raw / mag;
    step = (n < 1.5 ? 1 : n < 3 ? 2 : n < 7 ? 5 : 10) * mag;
    lo = qFloor(lo / step) * step;
    hi = qCeil(hi / step) * step;
}

struct Frame {
    QRectF plot;
    QDateTime t0, t1;
    double xFor(const QDateTime &t) const {
        const double span = qMax<qint64>(1, t0.secsTo(t1));
        return plot.left() + plot.width() * t0.secsTo(t) / span;
    }
};

void drawAxes(QPainter &p, const Frame &f, double lo, double hi, double step, const QString &title,
              const QColor &fg, const QFont &small, int days)
{
    QColor grid = fg;
    grid.setAlphaF(0.15);
    p.setFont(small);
    const QFontMetricsF fm(small);
    for (double v = lo; v <= hi + step / 2; v += step) {
        const double y = f.plot.bottom() - f.plot.height() * (v - lo) / (hi - lo);
        p.setPen(QPen(grid, 1));
        p.drawLine(QPointF(f.plot.left(), y), QPointF(f.plot.right(), y));
        p.setPen(fg);
        p.drawText(QRectF(f.plot.left() - 50, y - fm.height() / 2, 44, fm.height()), Qt::AlignRight | Qt::AlignVCenter,
                   QLocale().toString(v, 'f', step < 1 ? 1 : 0));
    }
    // Tagesraster
    const int every = days <= 10 ? 1 : days <= 35 ? 7 : days <= 120 ? 14 : 30;
    for (QDate d = f.t0.date(); d <= f.t1.date(); d = d.addDays(1)) {
        const double x = f.xFor(QDateTime(d, QTime(0, 0)));
        const bool label = (days <= 10) || (every == 7 ? d.dayOfWeek() == 1 : (d.day() == 1 || (every == 14 && d.day() == 15)));
        if (!label) continue;
        p.setPen(QPen(grid, 1));
        p.drawLine(QPointF(x, f.plot.top()), QPointF(x, f.plot.bottom()));
    }
    p.setPen(QPen(fg, 1));
    p.drawLine(f.plot.bottomLeft(), f.plot.bottomRight());
    p.drawLine(f.plot.bottomLeft(), f.plot.topLeft());
    QFont bold = small;
    bold.setBold(true);
    p.setFont(bold);
    p.drawText(QRectF(f.plot.left(), f.plot.top() - fm.height() * 1.4, f.plot.width(), fm.height() * 1.3),
               Qt::AlignLeft | Qt::AlignBottom, title);
}

void drawDateLabels(QPainter &p, const Frame &f, const QColor &fg, const QFont &small, int days)
{
    p.setFont(small);
    p.setPen(fg);
    const QFontMetricsF fm(small);
    const int every = days <= 10 ? 1 : days <= 35 ? 7 : days <= 120 ? 14 : 30;
    for (QDate d = f.t0.date(); d <= f.t1.date(); d = d.addDays(1)) {
        const bool label = (days <= 10) || (every == 7 ? d.dayOfWeek() == 1 : (d.day() == 1 || (every == 14 && d.day() == 15)));
        if (!label) continue;
        const double x = f.xFor(QDateTime(d, QTime(12, 0)));
        const QString t = days <= 10 ? QLocale().toString(d, QStringLiteral("ddd dd.MM.")) : d.toString(QStringLiteral("dd.MM."));
        p.drawText(QRectF(x - 40, f.plot.bottom() + 2, 80, fm.height()), Qt::AlignHCenter | Qt::AlignTop, t);
    }
}

void drawMarker(QPainter &p, const QPointF &pt, const QColor &c, bool filled, double r)
{
    p.setPen(QPen(c, 1.6));
    p.setBrush(filled ? QBrush(c) : QBrush(Qt::white));
    p.drawEllipse(pt, r, r);
}
} // namespace

void paint(QPainter &p, const QRectF &rect, const ChartSpec &spec, bool screen)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    // Bildschirm: Textfarbe kommt vom Widget (Stift vor dem Aufruf), Druck: schwarz
    const QColor text = screen ? p.pen().color() : QColor(Qt::black);
    QFont small = p.font();
    small.setPointSizeF(qMax(7.0, small.pointSizeF() * 0.85));
    const QFontMetricsF fm(small);

    const QDateTime t0(spec.from, QTime(0, 0));
    const QDateTime t1(spec.to.addDays(1), QTime(0, 0));
    const int days = int(spec.from.daysTo(spec.to)) + 1;

    // Reihen sammeln
    auto collect = [&](auto before, auto after, const QColor &c) {
        Series b{ ChartTr::tr("vorher"), c, false, Qt::DashLine, {} }, a{ ChartTr::tr("nachher"), c, true, Qt::SolidLine, {} };
        for (const SessionRecord &s : spec.sessions) {
            if (before(s) >= 0) b.points << qMakePair(s.started, double(before(s)));
            if (after(s) >= 0) a.points << qMakePair(s.started, double(after(s)));
        }
        auto byTime = [](const QPair<QDateTime, double> &x, const QPair<QDateTime, double> &y) { return x.first < y.first; };
        std::sort(b.points.begin(), b.points.end(), byTime);
        std::sort(a.points.begin(), a.points.end(), byTime);
        return QList<Series>{ b, a };
    };
    struct Panel { QString title; QList<Series> series; bool bars = false; };
    QList<Panel> panels;
    if (spec.showMinutes) panels << Panel{ ChartTr::tr("Übungsminuten je Tag"), {}, true };
    auto addPanel = [&](bool show, const QString &title, const QList<Series> &s) {
        if (show && (!s.at(0).points.isEmpty() || !s.at(1).points.isEmpty())) panels << Panel{ title, s, false };
    };
    addPanel(spec.showSpo2, ChartTr::tr("SpO₂ (%)"), collect([](const SessionRecord &s) { return s.spo2Before; },
                                                    [](const SessionRecord &s) { return s.spo2After; }, QColor(0x14, 0x74, 0xBC)));
    addPanel(spec.showPeakFlow, ChartTr::tr("Peak Flow (l/min)"), collect([](const SessionRecord &s) { return s.pfBefore; },
                                                                 [](const SessionRecord &s) { return s.pfAfter; }, QColor(0xE0, 0x62, 0x0D)));
    addPanel(spec.showPulse, ChartTr::tr("Puls (/min)"), collect([](const SessionRecord &s) { return s.pulseBefore; },
                                                        [](const SessionRecord &s) { return s.pulseAfter; }, QColor(0xC0, 0x39, 0x2B)));
    {   // Atemtrainer: erreichte Bälle und Haltezeit je Übung (eigene Bereiche)
        const auto none = [](const SessionRecord &) { return kNoValue; };
        QList<Series> balls = collect(none, [](const SessionRecord &r) { return r.deviceBalls; }, QColor(0x2E, 0x86, 0xAB));
        QList<Series> hold = collect(none, [](const SessionRecord &r) { return r.deviceHoldSec; }, QColor(0x8E, 0x5B, 0xC7));
        addPanel(spec.showDevice, ChartTr::tr("Atemtrainer: erreichte Bälle"), balls);
        addPanel(spec.showDevice, ChartTr::tr("Atemtrainer: Haltezeit (s)"), hold);
    }
    if (panels.isEmpty()) {
        p.setPen(text);
        p.drawText(rect, Qt::AlignCenter, ChartTr::tr("Keine Daten im gewählten Zeitraum."));
        p.restore();
        return;
    }

    const double legendH = fm.height() * 1.6;
    const double gap = fm.height() * 3.2;
    const double avail = rect.height() - legendH - gap * panels.size();
    const double ph = avail / panels.size();
    double y = rect.top() + legendH;

    // Legende
    {
        double x = rect.left() + 56;
        p.setFont(small);
        auto legendItem = [&](const QColor &c, bool filled, bool bar, const QString &label) {
            const double cy = rect.top() + legendH / 2;
            if (bar) {
                p.setPen(Qt::NoPen);
                p.setBrush(c);
                p.drawRect(QRectF(x, cy - 5, 12, 10));
            } else {
                drawMarker(p, QPointF(x + 6, cy), c, filled, 4);
            }
            p.setPen(text);
            const double w = fm.horizontalAdvance(label);
            p.drawText(QRectF(x + 16, cy - fm.height() / 2, w + 4, fm.height()), Qt::AlignVCenter, label);
            x += w + 34;
        };
        if (spec.showMinutes) {
            QSet<QString> used;
            for (const SessionRecord &r : spec.sessions) used.insert(r.moduleId);
            for (const ModuleDef &m : BreathModules::all())
                if (used.contains(m.id)) legendItem(moduleColor(m.id), true, true, m.shortName);
        }
        legendItem(Qt::gray, false, false, ChartTr::tr("vor der Übung"));
        legendItem(Qt::gray, true, false, ChartTr::tr("nach der Übung"));
    }

    for (const Panel &pan : panels) {
        y += gap * 0.55;
        Frame f;
        f.plot = QRectF(rect.left() + 56, y, rect.width() - 72, ph);
        f.t0 = t0;
        f.t1 = t1;
        const int ticks = qBound(1, int(ph / (fm.height() * 1.7)), 6);   // Beschriftungen nicht überlappen
        if (pan.bars) {
            QMap<QDate, QMap<QString, double>> perDay;
            double maxDay = 0;
            for (const SessionRecord &s : spec.sessions) {
                if (s.started.date() < spec.from || s.started.date() > spec.to) continue;
                perDay[s.started.date()][s.moduleId] += s.actualSec / 60.0;
            }
            for (auto it = perDay.cbegin(); it != perDay.cend(); ++it) {
                double sum = 0;
                for (double v : it.value()) sum += v;
                maxDay = qMax(maxDay, sum);
            }
            double lo = 0, hi = qMax(10.0, maxDay), step;
            niceRange(lo, hi, step, ticks);
            lo = 0;
            drawAxes(p, f, lo, hi, step, pan.title, text, small, days);
            const double dayW = f.plot.width() / qMax(1, days);
            const double bw = qBound(2.0, dayW * 0.7, 40.0);
            for (auto it = perDay.cbegin(); it != perDay.cend(); ++it) {
                const double cx = f.xFor(QDateTime(it.key(), QTime(12, 0)));
                double acc = 0;
                for (auto m = it.value().cbegin(); m != it.value().cend(); ++m) {
                    const double y0 = f.plot.bottom() - f.plot.height() * acc / hi;
                    acc += m.value();
                    const double y1 = f.plot.bottom() - f.plot.height() * acc / hi;
                    p.setPen(Qt::NoPen);
                    p.setBrush(moduleColor(m.key()));
                    p.drawRect(QRectF(cx - bw / 2, y1, bw, y0 - y1));
                }
            }
        } else {
            double lo = 1e9, hi = -1e9, step;
            for (const Series &s : pan.series)
                for (const auto &pt : s.points) { lo = qMin(lo, pt.second); hi = qMax(hi, pt.second); }
            niceRange(lo, hi, step, ticks);
            drawAxes(p, f, lo, hi, step, pan.title, text, small, days);
            p.save();
            p.setClipRect(f.plot.adjusted(-6, -6, 6, 6));
            for (const Series &s : pan.series) {
                QPainterPath path;
                bool first = true;
                for (const auto &pt : s.points) {
                    const QPointF q(f.xFor(pt.first), f.plot.bottom() - f.plot.height() * (pt.second - lo) / (hi - lo));
                    if (first) path.moveTo(q); else path.lineTo(q);
                    first = false;
                }
                QColor lc = s.color;
                lc.setAlphaF(s.filled ? 0.9 : 0.55);
                p.setPen(QPen(lc, 1.6, s.style));
                p.setBrush(Qt::NoBrush);
                p.drawPath(path);
                for (const auto &pt : s.points)
                    drawMarker(p, QPointF(f.xFor(pt.first), f.plot.bottom() - f.plot.height() * (pt.second - lo) / (hi - lo)),
                               s.color, s.filled, s.points.size() > 60 ? 2.5 : 3.8);
            }
            p.restore();
        }
        drawDateLabels(p, f, text, small, days);
        y += ph + gap * 0.45;
    }
    p.restore();
}

} // namespace ChartRenderer

void ChartWidget::setSpec(const ChartSpec &s)
{
    m_spec = s;
    bool spo2 = false, pf = false, pulse = false, balls = false, hold = false;
    for (const SessionRecord &r : s.sessions) {
        spo2 |= r.spo2Before >= 0 || r.spo2After >= 0;
        pf |= r.pfBefore >= 0 || r.pfAfter >= 0;
        pulse |= r.pulseBefore >= 0 || r.pulseAfter >= 0;
        balls |= r.deviceBalls >= 0;
        hold |= r.deviceHoldSec >= 0;
    }
    m_panels = int(s.showMinutes) + int(s.showSpo2 && spo2) + int(s.showPeakFlow && pf) + int(s.showPulse && pulse)
               + int(s.showDevice && balls) + int(s.showDevice && hold);
    updateGeometry();
    update();
}

ChartWidget::ChartWidget(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().color(QPalette::Base));
    p.setPen(palette().color(QPalette::Text));
    ChartRenderer::paint(p, QRectF(rect()).adjusted(8, 8, -8, -8), m_spec, true);
}
