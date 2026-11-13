// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "breathview.h"
#include "breathengine.h"
#include "icons.h"

#include <QPainterPath>

#include <QDateTime>
#include <QPainter>
#include <QTimer>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>

namespace {
double ease(double t) { return 0.5 - 0.5 * qCos(M_PI * qBound(0.0, t, 1.0)); }   // sanft an- und auslaufend

QColor mix(const QColor &a, const QColor &b, double f)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * f, a.greenF() + (b.greenF() - a.greenF()) * f,
                            a.blueF() + (b.blueF() - a.blueF()) * f, a.alphaF() + (b.alphaF() - a.alphaF()) * f);
}
}

BreathView::BreathView(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void BreathView::setModule(const ModuleDef *m, const QVector<double> &phaseSec)
{
    m_module = m;
    m_sec = phaseSec;
    update();
}

void BreathView::setEngine(BreathEngine *e)
{
    if (m_engine) disconnect(m_engine, nullptr, this, nullptr);
    m_engine = e;
    if (e) connect(e, &BreathEngine::tick, this, QOverload<>::of(&QWidget::update));
    update();
}

void BreathView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF all = rect();
    const double captionH = qBound(56.0, all.height() * 0.2, 110.0);
    const QRectF area(all.left(), all.top(), all.width(), all.height() - captionH);
    const QRectF caption(all.left(), area.bottom(), all.width(), captionH);
    if (!m_module) return;

    if (m_module->visual == ModuleDef::Visual::Box) paintBox(p, area);
    else if (m_module->visual == ModuleDef::Visual::TriBall) paintTriBall(p, area);
    else paintCircle(p, area);

    // Beschriftung unter der Grafik: Phase und Hinweis
    QString title, sub;
    if (!m_engine || m_engine->state() == BreathEngine::State::Idle) {
        title = tr("Bereit");
        sub = tr("Muster %1").arg(m_module->defaultPattern());
    } else if (m_engine->state() == BreathEngine::State::Preroll) {
        title = tr("Gleich geht's los …");
        sub = tr("Atmen Sie entspannt aus.");
    } else if (m_engine->state() == BreathEngine::State::Finished) {
        title = tr("Geschafft");
        sub = tr("Atmen Sie wieder ganz normal weiter.");
    } else if (m_engine->state() == BreathEngine::State::Waiting) {
        title = phaseName(PhaseKind::Ready);
        sub = m_engine->step().hint + QStringLiteral("  ") + tr("(Leertaste = weiter)");
    } else {
        const StepDef &st = m_engine->step();
        title = phaseName(st.kind);
        if (m_hints) sub = st.hint;
        if (m_module->hasSegments() && !m_module->singleRound && !st.segmentTitle.isEmpty())
            sub = st.segmentTitle + (sub.isEmpty() ? QString() : QStringLiteral(" · ") + sub);
        if (m_engine->state() == BreathEngine::State::Paused) sub = tr("Pause");
        else if (m_engine->lastCycle()) sub += (sub.isEmpty() ? QString() : QStringLiteral(" · ")) + tr("letzte Runde");
    }
    paintCaption(p, caption, title, sub);
}

void BreathView::paintCaption(QPainter &p, const QRectF &r, const QString &title, const QString &sub)
{
    QFont f = font();
    f.setPointSizeF(qBound(14.0, r.height() * 0.30, 28.0));
    f.setBold(true);
    p.setFont(f);
    p.setPen(palette().color(QPalette::WindowText));
    const QRectF t(r.left(), r.top(), r.width(), r.height() * 0.58);
    p.drawText(t, Qt::AlignHCenter | Qt::AlignBottom, title);
    QFont s = font();
    s.setPointSizeF(qBound(9.0, r.height() * 0.15, 14.0));
    p.setFont(s);
    QColor c = palette().color(QPalette::WindowText);
    c.setAlphaF(0.7);
    p.setPen(c);
    p.drawText(QRectF(r.left(), t.bottom() + 2, r.width(), r.height() - t.height() - 2), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, sub);
}

void BreathView::paintBox(QPainter &p, const QRectF &area)
{
    const double side = qMin(area.width(), area.height()) * 0.70;
    const QRectF sq(area.center().x() - side / 2, area.center().y() - side / 2, side, side);
    const bool dark = palette().color(QPalette::Window).lightness() < 110;
    const QColor box = dark ? QColor(0x4A, 0x9E, 0xE0) : Icons::primary();
    const QColor dot = dark ? QColor(0xF5, 0x8A, 0x3C) : Icons::accent();
    const double pen = qMax(4.0, side * 0.022);

    // Ecken im Uhrzeigersinn ab links oben
    const QPointF c[4] = { sq.topLeft(), sq.topRight(), sq.bottomRight(), sq.bottomLeft() };
    int edge = -1;
    double prog = 0;
    const bool running = m_engine && (m_engine->state() == BreathEngine::State::Running
                                      || m_engine->state() == BreathEngine::State::Paused);
    if (running) {
        edge = qMax(0, m_engine->step().phaseIndex) % 4;
        prog = m_engine->phaseProgress();
    } else if (m_engine && m_engine->state() == BreathEngine::State::Finished) {
        edge = 3;
        prog = 1;
    }

    // Quadrat (Fläche leicht getönt)
    QColor fill = box;
    fill.setAlphaF(dark ? 0.12 : 0.08);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRoundedRect(sq, side * 0.03, side * 0.03);
    QColor edgeColor = box;
    edgeColor.setAlphaF(0.55);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(edgeColor, pen, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawRect(sq);
    // aktuelle Seite hervorheben (bereits zurückgelegter Teil)
    if (edge >= 0) {
        const QPointF a = c[edge], b = c[(edge + 1) % 4];
        const QPointF pos = a + (b - a) * prog;
        p.setPen(QPen(box, pen * 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(a, pos);
    }
    // Eckmarken
    p.setPen(Qt::NoPen);
    p.setBrush(box);
    for (const QPointF &pt : c) p.drawEllipse(pt, pen * 0.9, pen * 0.9);

    // Punkt
    QPointF pos = c[0];
    if (edge >= 0) pos = c[edge] + (c[(edge + 1) % 4] - c[edge]) * prog;
    if (m_engine && m_engine->state() == BreathEngine::State::Finished) pos = c[0];
    const double r = qMax(9.0, side * 0.045);
    QColor glow = dot;
    glow.setAlphaF(0.25);
    p.setBrush(glow);
    p.drawEllipse(pos, r * 1.9, r * 1.9);
    p.setBrush(dot);
    p.drawEllipse(pos, r, r);

    // Sekunden in der Mitte
    QString center;
    if (m_engine && m_engine->state() == BreathEngine::State::Preroll)
        center = QString::number(qCeil(m_engine->prerollRemaining()));
    else if (running)
        center = QString::number(qMax(1, qCeil(m_engine->phaseRemaining() - 0.001)));
    if (!center.isEmpty()) {
        QFont f = font();
        f.setPixelSize(int(side * 0.28));
        f.setBold(true);
        p.setFont(f);
        QColor tc = box;
        tc.setAlphaF(0.85);
        p.setPen(tc);
        p.drawText(sq, Qt::AlignCenter, center);
    }
}

void BreathView::paintCircle(QPainter &p, const QRectF &area)
{
    const QPointF ctr = area.center();
    const double R = qMin(area.width(), area.height()) * 0.44;
    const double rMin = R * 0.38;
    const bool dark = palette().color(QPalette::Window).lightness() < 110;
    const QColor blue = dark ? QColor(0x4A, 0x9E, 0xE0) : Icons::primary();
    const QColor orange = dark ? QColor(0xF5, 0x8A, 0x3C) : Icons::accent();

    double f = 0;      // 0 = klein, 1 = groß
    double prog = 0;
    bool hold = false, huff = false, waiting = false;
    const auto st = m_engine ? m_engine->state() : BreathEngine::State::Idle;
    if (st == BreathEngine::State::Running || st == BreathEngine::State::Paused) {
        prog = m_engine->phaseProgress();
        switch (m_engine->phaseKind()) {
        case PhaseKind::Inhale:  f = ease(prog); break;
        case PhaseKind::HoldIn:  f = 1; hold = true; break;
        case PhaseKind::Exhale:  f = 1 - ease(prog); break;
        case PhaseKind::HoldOut: f = 0; hold = true; break;
        case PhaseKind::Calm:    // ruhiges, freies Atmen: sanftes Wogen ohne festen Takt
            f = 0.45 + 0.15 * qSin(m_engine->phaseElapsed() * 2 * M_PI / 5.0);
            break;
        case PhaseKind::Huff:    // kräftiges Aushauchen: schnell kleiner
            f = 1 - ease(qMin(1.0, prog * 1.6));
            huff = true;
            break;
        case PhaseKind::Ready:   f = 0.2; waiting = true; break;
        }
    } else if (st == BreathEngine::State::Waiting) {
        f = 0.2;
        waiting = true;
    }
    const double r = rMin + (R - rMin) * f;

    // Führungsringe (größte und kleinste Ausdehnung)
    QColor guide = palette().color(QPalette::WindowText);
    guide.setAlphaF(0.18);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(guide, 1.5, Qt::DashLine));
    p.drawEllipse(ctr, R, R);
    p.setPen(QPen(guide, 1.0, Qt::DotLine));
    p.drawEllipse(ctr, rMin, rMin);

    // Atemkreis
    QRadialGradient g(ctr, r);
    const QColor base = huff ? mix(blue, orange, 0.45) : blue;
    g.setColorAt(0.0, mix(base, Qt::white, dark ? 0.25 : 0.55));
    g.setColorAt(0.75, base);
    g.setColorAt(1.0, mix(base, Qt::black, 0.15));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(ctr, r, r);

    // Halten: ruhiger, pulsierender Ring
    if (hold) {
        QColor ring = orange;
        ring.setAlphaF(0.35 + 0.25 * qSin(prog * M_PI * 2 * qMax(1.0, m_engine->step().sec / 2)));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(ring, qMax(3.0, R * 0.025)));
        p.drawEllipse(ctr, r + R * 0.04, r + R * 0.04);
    }
    // Warten auf den Benutzer (Abhusten): pulsierender Ring
    if (waiting) {
        QColor ring = orange;
        ring.setAlphaF(0.5 + 0.3 * qSin(QDateTime::currentMSecsSinceEpoch() / 400.0));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(ring, qMax(3.0, R * 0.03)));
        p.drawEllipse(ctr, r + R * 0.08, r + R * 0.08);
        if (m_engine) QTimer::singleShot(40, this, QOverload<>::of(&QWidget::update));
    }
    // Fortschritt der Phase als Bogen außen
    if (st == BreathEngine::State::Running || st == BreathEngine::State::Paused) {
        const double w = qMax(4.0, R * 0.03);
        const QRectF arc(ctr.x() - R - w * 2, ctr.y() - R - w * 2, 2 * (R + w * 2), 2 * (R + w * 2));
        p.setPen(QPen(orange, w, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(arc, 90 * 16, -int(prog * 360 * 16));
    }

    QString center;
    if (st == BreathEngine::State::Preroll) center = QString::number(qCeil(m_engine->prerollRemaining()));
    else if (st == BreathEngine::State::Running || st == BreathEngine::State::Paused)
        center = QString::number(qMax(1, qCeil(m_engine->phaseRemaining() - 0.001)));
    if (!center.isEmpty()) {
        QFont fnt = font();
        fnt.setPixelSize(int(qMax(18.0, rMin * 0.8)));
        fnt.setBold(true);
        p.setFont(fnt);
        p.setPen(Qt::white);
        p.drawText(QRectF(ctr.x() - R, ctr.y() - R, 2 * R, 2 * R), Qt::AlignCenter, center);
    }
}

void BreathView::paintTriBall(QPainter &p, const QRectF &area)
{
    const bool dark = palette().color(QPalette::Window).lightness() < 110;
    const QColor blue = dark ? QColor(0x4A, 0x9E, 0xE0) : Icons::primary();
    const QColor ballColors[3] = { blue, dark ? QColor(0x4C, 0xC2, 0x8A) : QColor(0x2E, 0x9E, 0x6B),
                                   dark ? QColor(0xF5, 0x8A, 0x3C) : Icons::accent() };
    const QColor fg = palette().color(QPalette::WindowText);

    // Ballhöhen (0 = unten, 1 = oben) aus Phase und Fortschritt
    double up[3] = { 0, 0, 0 };
    const auto st = m_engine ? m_engine->state() : BreathEngine::State::Idle;
    const bool active = st == BreathEngine::State::Running || st == BreathEngine::State::Paused;
    if (active) {
        const StepDef &step = m_engine->step();
        const double prog = m_engine->phaseProgress();
        for (int i = 0; i < m_target; ++i) {
            switch (step.kind) {
            case PhaseKind::Inhale: {          // Bälle steigen nacheinander bis zum Ziel
                const double start = m_target > 1 ? 0.55 * i / (m_target - 1) : 0.0;
                up[i] = ease(qBound(0.0, (prog - start) / 0.45, 1.0));
                break;
            }
            case PhaseKind::HoldIn:            // oben halten, leichtes Tanzen
                up[i] = 0.96 + 0.04 * qSin(m_engine->phaseElapsed() * 7 + i);
                break;
            case PhaseKind::Exhale:            // nach dem Halten: Bälle fallen
                up[i] = step.phaseIndex > 0 ? 1 - ease(qMin(1.0, prog * 3)) : 0;
                break;
            default:
                break;
            }
        }
    }

    // Gerät: drei Kammern auf einem Sockel, Mundstück mit Schlauch rechts
    const double devH = qMin(area.height() * 0.62, area.width() * 0.62);
    const double cw = devH * 0.26, gap = devH * 0.07, ch = devH;
    const double devW = 3 * cw + 2 * gap;
    const double x0 = area.center().x() - devW / 2 - devH * 0.12, y0 = area.center().y() - ch / 2 - devH * 0.04;
    const double pen = qMax(3.0, devH * 0.018);
    QColor fill = blue;
    fill.setAlphaF(dark ? 0.10 : 0.06);
    p.setPen(QPen(blue, pen, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(fill);
    const QRectF base(x0 - cw * 0.25, y0 + ch, devW + cw * 0.5, ch * 0.13);
    p.drawRoundedRect(base, ch * 0.04, ch * 0.04);
    for (int i = 0; i < 3; ++i) {
        const QRectF chamber(x0 + i * (cw + gap), y0, cw, ch);
        p.setPen(QPen(blue, pen));
        p.setBrush(fill);
        p.drawRoundedRect(chamber, cw / 2, cw / 2);
        const double r = cw / 2 - pen * 1.6;
        const double travel = ch - 2 * r - pen * 3.2;
        const QPointF c(chamber.center().x(), chamber.bottom() - r - pen * 1.6 - up[i] * travel);
        // Zielmarke: welche Bälle angehoben werden sollen
        if (i < m_target) {
            QColor mark = ballColors[i];
            mark.setAlphaF(0.35);
            p.setPen(QPen(mark, pen * 0.7, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(chamber.center().x(), chamber.top() + r + pen * 1.6), r, r);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(ballColors[i]);
        p.drawEllipse(c, r, r);
    }
    // Schlauch und Mundstück
    const QPointF hoseStart(base.right(), base.center().y());
    const QPointF mouth(base.right() + devH * 0.32, y0 + ch * 0.05);
    QPainterPath hose(hoseStart);
    hose.cubicTo(hoseStart + QPointF(devH * 0.2, 0), mouth + QPointF(-devH * 0.05, devH * 0.45), mouth);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(blue, pen * 2.2, Qt::SolidLine, Qt::RoundCap));
    p.drawPath(hose);
    p.setPen(QPen(blue, pen));
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(mouth.x() - devH * 0.03, mouth.y() - devH * 0.09, devH * 0.1, devH * 0.09), 3, 3);

    // Restsekunden des Schritts links oben
    if (st == BreathEngine::State::Preroll || active) {
        QFont f = font();
        f.setPixelSize(int(qMax(18.0, devH * 0.16)));
        f.setBold(true);
        p.setFont(f);
        QColor tc = fg;
        tc.setAlphaF(0.75);
        p.setPen(tc);
        const QString n = st == BreathEngine::State::Preroll ? QString::number(qCeil(m_engine->prerollRemaining()))
                                                              : QString::number(qMax(1, qCeil(m_engine->phaseRemaining() - 0.001)));
        p.drawText(QRectF(area.left() + 12, area.top() + 8, devH * 0.5, devH * 0.25), Qt::AlignLeft | Qt::AlignTop, n);
    }
    // Ziel
    QFont sf = font();
    sf.setPointSizeF(qMax(9.0, sf.pointSizeF()));
    p.setFont(sf);
    QColor tc = fg;
    tc.setAlphaF(0.7);
    p.setPen(tc);
    p.drawText(QRectF(base.left(), base.bottom() + 4, base.width(), 24), Qt::AlignHCenter | Qt::AlignTop,
               tr("Ziel: %n Ball/Bälle oben halten", nullptr, m_target));
}
