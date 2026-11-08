// VitaBreathe – Verlauf (Tagebuch) und Verlaufsgrafik der Übungseinheiten
// SPDX-License-Identifier: MIT
#pragma once

#include "chartwidget.h"

#include <QWidget>

class QComboBox;
class QDateEdit;
class QTableWidget;
class QLabel;
class QPushButton;
class QCheckBox;

class HistoryPage : public QWidget
{
    Q_OBJECT
public:
    explicit HistoryPage(QWidget *parent = nullptr);
    void setUser(qint64 userId);
    void reload();
    void printReport();
    void savePdf();
    void addEntry();

private:
    void applyPeriod();
    void editSelected();
    void deleteSelected();
    void exportCsv();
    ChartSpec currentSpec() const;

    qint64        m_userId = 0;
    QComboBox    *m_period, *m_module;
    QDateEdit    *m_from, *m_to;
    QCheckBox    *m_cMinutes, *m_cSpo2, *m_cPf, *m_cPulse, *m_cDevice;
    QTableWidget *m_table;
    ChartWidget  *m_chart;
    QLabel       *m_count;
    QPushButton  *m_edit, *m_delete;
    QList<SessionRecord> m_sessions;
};
