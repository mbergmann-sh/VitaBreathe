// ViviaBreathe – Messwerte vor/nach einer Übung (Trainingsmodus) und Bearbeiten eines Tagebucheintrags
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include "datatypes.h"

#include <QDialog>
#include <QWidget>

class QSpinBox;
class QComboBox;
class QPlainTextEdit;
class QDateTimeEdit;
class QLabel;
class QFormLayout;

// Eingabefelder für SpO2, Peak Flow, Puls und Befinden (jeweils optional: "–" = nicht erfasst)
class MeasureForm : public QWidget
{
    Q_OBJECT
public:
    explicit MeasureForm(QWidget *parent = nullptr);
    void setVisibleFields(bool spo2, bool peakFlow, bool pulse, bool feeling);
    void setValues(int spo2, int pf, int pulse, int feeling);
    int spo2() const;
    int peakFlow() const;
    int pulse() const;
    int feeling() const;
    void setHeading(const QString &text);

private:
    QLabel   *m_heading;
    QSpinBox *m_spo2, *m_pf, *m_pulse, *m_feeling;
    QWidget  *m_rows[4];
    QFormLayout *m_form = nullptr;
};

// Tagebucheintrag bearbeiten (auch nachträglich erfassen)
class SessionEditDialog : public QDialog
{
    Q_OBJECT
public:
    SessionEditDialog(const SessionRecord &s, QWidget *parent = nullptr);
    SessionRecord record() const;

private:
    SessionRecord   m_rec;
    QComboBox      *m_module, *m_event;
    QDateTimeEdit  *m_started;
    QSpinBox       *m_minutes, *m_cycles, *m_devBalls, *m_devHold;
    MeasureForm    *m_before, *m_after;
    QPlainTextEdit *m_note;
};
