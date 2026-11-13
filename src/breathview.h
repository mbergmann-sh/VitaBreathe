// ViviaBreathe – Animation einer Atemübung
//
//  * Box: Ein farbiger Punkt wandert im Uhrzeigersinn, beginnend links oben, von Ecke zu Ecke
//    eines andersfarbigen Quadrats – jede Seite ist eine Phase. Unter dem Quadrat steht die Phase.
//  * Kreis: Die Grafik dehnt sich beim Einatmen aus, bleibt beim Halten stehen und zieht sich
//    beim Ausatmen zusammen. Die Geschwindigkeit folgt den eingestellten Phasendauern.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include "breathmodules.h"

#include <QPointer>
#include <QWidget>

class BreathEngine;

class BreathView : public QWidget
{
    Q_OBJECT
public:
    explicit BreathView(QWidget *parent = nullptr);
    void setModule(const ModuleDef *m, const QVector<double> &phaseSec);
    void setEngine(BreathEngine *e);
    void setShowHints(bool on) { m_hints = on; update(); }
    void setDeviceTarget(int balls) { m_target = qBound(1, balls, 3); update(); }   // TriBall: Ziel 1–3 Bälle
    QSize sizeHint() const override { return { 520, 520 }; }
    QSize minimumSizeHint() const override { return { 240, 260 }; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void paintBox(QPainter &p, const QRectF &area);
    void paintCircle(QPainter &p, const QRectF &area);
    void paintTriBall(QPainter &p, const QRectF &area);
    void paintCaption(QPainter &p, const QRectF &r, const QString &title, const QString &sub);

    const ModuleDef        *m_module = nullptr;
    QVector<double>         m_sec;
    QPointer<BreathEngine>  m_engine;
    bool                    m_hints = true;
    int                     m_target = 1;
};
