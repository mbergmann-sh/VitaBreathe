// ViviaBreathe – bebilderte Anleitung eines Übungsmoduls (Haltung, Schritte mit Atemtrainer)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include "breathmodules.h"

#include <QDialog>
#include <QWidget>

// Schritte als Bild mit Text, in Spalten (passt sich der Breite an)
class GuideWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GuideWidget(const QList<GuideStep> &steps, int imageWidth = 200, QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    void relayout();
    QList<QWidget *> m_cells;
    int m_columns = 0;
    int m_imageWidth;
};

// Eigenes Fenster mit der Anleitung (z. B. über „Anleitung …“)
class GuideDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GuideDialog(const ModuleDef &m, QWidget *parent = nullptr);
};
