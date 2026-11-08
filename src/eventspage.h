// VitaBreathe – Ereignisse mit Erinnerung (z. B. "Einschlafübung" täglich 21:30)
// SPDX-License-Identifier: MIT
#pragma once

#include "datatypes.h"

#include <QDialog>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QTimeEdit;
class QPushButton;

// Auswahl der Wochentage mit Schnellwahl
class WeekdayPicker : public QWidget
{
    Q_OBJECT
public:
    explicit WeekdayPicker(QWidget *parent = nullptr);
    void setMask(int m);
    int mask() const;

private:
    QCheckBox *m_days[7];
};

class EventDialog : public QDialog
{
    Q_OBJECT
public:
    EventDialog(const BreathEvent &e, QWidget *parent = nullptr);
    BreathEvent breathEvent() const;

private:
    BreathEvent    m_ev;
    QComboBox     *m_name, *m_module;
    QTimeEdit     *m_time;
    WeekdayPicker *m_days;
    QCheckBox     *m_enabled, *m_sound;
    QPlainTextEdit *m_note;
};

class EventsPage : public QWidget
{
    Q_OBJECT
public:
    explicit EventsPage(QWidget *parent = nullptr);
    void setUser(qint64 userId);
    void reload();
    void addEvent();

signals:
    void startRequested(const QString &moduleId, const QString &eventName);

private:
    qint64 selectedId() const;
    void editSelected();

    qint64        m_userId = 0;
    QTableWidget *m_table;
    QPushButton  *m_edit, *m_delete, *m_start;
    QList<BreathEvent> m_events;
};
