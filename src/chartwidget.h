// VitaBreathe – Verlaufsgrafik der Übungseinheiten (eigene QPainter-Zeichnung wie in VitaParms,
// ohne Qt Charts – das Modul gibt es in der Open-Source-Variante nur unter GPLv3)
//
// Bereiche: Übungsminuten je Tag (Balken, nach Übung gefärbt), SpO2, Peak Flow und Puls jeweils
// vor (offene Marker, gestrichelt) und nach der Übung (gefüllte Marker). Dieselbe Zeichenroutine
// dient Bildschirm, Druck und PDF.
// SPDX-License-Identifier: MIT
#pragma once

#include "datatypes.h"

#include <QDate>
#include <QImage>
#include <QWidget>

class QPainter;

struct ChartSpec {
    QDate from, to;                    // inklusive
    QList<SessionRecord> sessions;
    bool showMinutes = true, showSpo2 = true, showPeakFlow = true, showPulse = true, showDevice = true;
};

namespace ChartRenderer {
void paint(QPainter &p, const QRectF &rect, const ChartSpec &spec, bool screen = true);
QColor moduleColor(const QString &moduleId);
}

class ChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChartWidget(QWidget *parent = nullptr);
    void setSpec(const ChartSpec &s);
    const ChartSpec &spec() const { return m_spec; }
    // Mindesthöhe wächst mit der Zahl der Bereiche (in der Verlaufsseite scrollbar)
    QSize minimumSizeHint() const override { return { 360, qMax(260, 60 + 85 * m_panels) }; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    ChartSpec m_spec;
    int m_panels = 0;
};
