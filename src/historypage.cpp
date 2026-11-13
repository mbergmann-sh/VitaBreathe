// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "historypage.h"
#include "breathmodules.h"
#include "database.h"
#include "icons.h"
#include "measuredialog.h"
#include "reportprinter.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

namespace {
enum Col { CDate, CModule, CEvent, CPattern, CDuration, CCycles, CSpo2B, CSpo2A, CPfB, CPfA, CPulseB, CPulseA,
           CFeelB, CFeelA, CDevice, CNote, CCount };
QTableWidgetItem *item(const QString &t, Qt::Alignment a = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *it = new QTableWidgetItem(t);
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    it->setTextAlignment(a);
#else
    it->setTextAlignment(int(a));
#endif
    it->setFlags(it->flags() & ~Qt::ItemIsEditable);
    return it;
}
QTableWidgetItem *num(int v)
{
    return item(v >= 0 ? QString::number(v) : QString(), Qt::AlignRight | Qt::AlignVCenter);
}
}

HistoryPage::HistoryPage(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);

    auto *filter = new QHBoxLayout;
    filter->addWidget(new QLabel(tr("Zeitraum:")));
    m_period = new QComboBox;
    m_period->addItem(tr("Letzte 7 Tage"), 7);
    m_period->addItem(tr("Letzte 30 Tage"), 30);
    m_period->addItem(tr("Letzte 90 Tage"), 90);
    m_period->addItem(tr("Letztes Jahr"), 365);
    m_period->addItem(tr("Alles"), -1);
    m_period->addItem(tr("Frei wählen"), 0);
    filter->addWidget(m_period);
    m_from = new QDateEdit;
    m_to = new QDateEdit;
    for (QDateEdit *d : { m_from, m_to }) {
        d->setCalendarPopup(true);
        d->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    }
    filter->addWidget(m_from);
    filter->addWidget(new QLabel(QStringLiteral("–")));
    filter->addWidget(m_to);
    filter->addSpacing(12);
    filter->addWidget(new QLabel(tr("Übung:")));
    m_module = new QComboBox;
    m_module->addItem(tr("alle"), QString());
    for (const ModuleDef &m : BreathModules::all()) m_module->addItem(Icons::get(m.icon), m.shortName, m.id);
    filter->addWidget(m_module);
    filter->addStretch();
    m_count = new QLabel;
    filter->addWidget(m_count);
    lay->addLayout(filter);

    auto *split = new QSplitter(Qt::Vertical);
    m_table = new QTableWidget(0, CCount);
    m_table->setHorizontalHeaderLabels({ tr("Datum / Uhrzeit"), tr("Übung"), tr("Anlass"), tr("Muster [s]"), tr("Dauer"),
                                         tr("Zyklen"), tr("SpO₂\nvor [%]"), tr("SpO₂\nnach [%]"), tr("Peak Flow\nvor [l/min]"),
                                         tr("Peak Flow\nnach [l/min]"), tr("Puls\nvor"), tr("Puls\nnach"),
                                         tr("Befinden\nvor"), tr("Befinden\nnach"), tr("Atemtrainer"), tr("Notiz") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setWordWrap(false);
    split->addWidget(m_table);

    auto *chartBox = new QWidget;
    auto *cb = new QVBoxLayout(chartBox);
    cb->setContentsMargins(0, 0, 0, 0);
    auto *curves = new QHBoxLayout;
    curves->addWidget(new QLabel(tr("Grafik:")));
    m_cMinutes = new QCheckBox(tr("Übungsminuten"));
    m_cSpo2 = new QCheckBox(tr("SpO₂"));
    m_cPf = new QCheckBox(tr("Peak Flow"));
    m_cPulse = new QCheckBox(tr("Puls"));
    m_cDevice = new QCheckBox(tr("Atemtrainer"));
    QSettings st;
    for (QCheckBox *c : { m_cMinutes, m_cSpo2, m_cPf, m_cPulse, m_cDevice }) {
        c->setChecked(st.value(QStringLiteral("chart/") + c->text(), true).toBool());
        curves->addWidget(c);
        connect(c, &QCheckBox::toggled, this, [this, c](bool on) {
            QSettings().setValue(QStringLiteral("chart/") + c->text(), on);
            m_chart->setSpec(currentSpec());
        });
    }
    curves->addStretch();
    cb->addLayout(curves);
    m_chart = new ChartWidget;
    auto *chartScroll = new QScrollArea;
    chartScroll->setWidget(m_chart);
    chartScroll->setWidgetResizable(true);
    chartScroll->setFrameShape(QFrame::NoFrame);
    chartScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    cb->addWidget(chartScroll, 1);
    split->addWidget(chartBox);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    split->restoreState(st.value(QStringLiteral("history/splitter")).toByteArray());
    connect(split, &QSplitter::splitterMoved, this, [split] {
        QSettings().setValue(QStringLiteral("history/splitter"), split->saveState());
    });
    lay->addWidget(split, 1);

    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(Icons::get(QStringLiteral("add")), tr("Nachtragen …"));
    m_edit = new QPushButton(Icons::get(QStringLiteral("edit")), tr("Bearbeiten …"));
    m_delete = new QPushButton(Icons::get(QStringLiteral("delete")), tr("Löschen"));
    buttons->addWidget(add);
    buttons->addWidget(m_edit);
    buttons->addWidget(m_delete);
    buttons->addStretch();
    auto *print = new QPushButton(Icons::get(QStringLiteral("print")), tr("Verlauf drucken …"));
    auto *pdf = new QPushButton(Icons::get(QStringLiteral("pdf")), tr("Als PDF speichern …"));
    auto *csv = new QPushButton(tr("CSV exportieren …"));
    buttons->addWidget(print);
    buttons->addWidget(pdf);
    buttons->addWidget(csv);
    lay->addLayout(buttons);

    connect(add, &QPushButton::clicked, this, &HistoryPage::addEntry);
    connect(m_edit, &QPushButton::clicked, this, &HistoryPage::editSelected);
    connect(m_delete, &QPushButton::clicked, this, &HistoryPage::deleteSelected);
    connect(print, &QPushButton::clicked, this, &HistoryPage::printReport);
    connect(pdf, &QPushButton::clicked, this, &HistoryPage::savePdf);
    connect(csv, &QPushButton::clicked, this, &HistoryPage::exportCsv);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &HistoryPage::editSelected);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        const bool sel = !m_table->selectionModel()->selectedRows().isEmpty();
        m_edit->setEnabled(sel);
        m_delete->setEnabled(sel);
    });
    connect(m_period, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        QSettings().setValue(QStringLiteral("history/period"), m_period->currentIndex());
        applyPeriod();
    });
    connect(m_module, qOverload<int>(&QComboBox::currentIndexChanged), this, &HistoryPage::reload);
    connect(m_from, &QDateEdit::dateChanged, this, [this] { if (m_period->currentData().toInt() == 0) reload(); });
    connect(m_to, &QDateEdit::dateChanged, this, [this] { if (m_period->currentData().toInt() == 0) reload(); });
    m_period->setCurrentIndex(st.value(QStringLiteral("history/period"), 1).toInt());
    applyPeriod();
}

void HistoryPage::setUser(qint64 userId)
{
    m_userId = userId;
    reload();
}

void HistoryPage::applyPeriod()
{
    const int days = m_period->currentData().toInt();
    const bool custom = days == 0;
    m_from->setEnabled(custom);
    m_to->setEnabled(custom);
    if (!custom) {
        QSignalBlocker b1(m_from), b2(m_to);
        m_to->setDate(QDate::currentDate());
        if (days > 0) m_from->setDate(QDate::currentDate().addDays(-days + 1));
        else {
            QDate first = QDate::currentDate();
            for (const SessionRecord &s : Database::instance().sessions(m_userId))
                if (s.started.date() < first) first = s.started.date();
            m_from->setDate(first);
        }
    }
    reload();
}

void HistoryPage::reload()
{
    if (!Database::instance().isOpen()) return;
    if (m_period->currentData().toInt() > 0) m_to->setDate(QDate::currentDate());
    const QString mod = m_module->currentData().toString();
    m_sessions.clear();
    for (const SessionRecord &s : Database::instance().sessions(m_userId, QDateTime(m_from->date(), QTime(0, 0)),
                                                                 QDateTime(m_to->date().addDays(1), QTime(0, 0))))
        if (mod.isEmpty() || s.moduleId == mod) m_sessions << s;

    m_table->setSortingEnabled(false);
    m_table->setRowCount(m_sessions.size());
    int minutes = 0;
    for (int r = 0; r < m_sessions.size(); ++r) {
        const SessionRecord &s = m_sessions.at(r);
        minutes += s.actualSec;
        auto *d = item(QLocale().toString(s.started, QStringLiteral("ddd dd.MM.yyyy HH:mm")));
        d->setData(Qt::UserRole, s.id);
        m_table->setItem(r, CDate, d);
        auto *m = item(BreathModules::displayName(s.moduleId));
        if (const ModuleDef *def = BreathModules::find(s.moduleId)) m->setIcon(Icons::get(def->icon));
        m_table->setItem(r, CModule, m);
        m_table->setItem(r, CEvent, item(s.eventName));
        m_table->setItem(r, CPattern, item(s.pattern));
        auto *du = item(formatDuration(s.actualSec) + (s.completed ? QString() : QStringLiteral(" *")),
                        Qt::AlignRight | Qt::AlignVCenter);
        if (!s.completed) du->setToolTip(tr("vorzeitig beendet"));
        m_table->setItem(r, CDuration, du);
        m_table->setItem(r, CCycles, num(s.cycles));
        m_table->setItem(r, CSpo2B, num(s.spo2Before));
        m_table->setItem(r, CSpo2A, num(s.spo2After));
        m_table->setItem(r, CPfB, num(s.pfBefore));
        m_table->setItem(r, CPfA, num(s.pfAfter));
        m_table->setItem(r, CPulseB, num(s.pulseBefore));
        m_table->setItem(r, CPulseA, num(s.pulseAfter));
        m_table->setItem(r, CFeelB, num(s.feelingBefore));
        m_table->setItem(r, CFeelA, num(s.feelingAfter));
        QString dev;
        if (s.deviceBalls >= 0) dev = tr("%n Ball/Bälle", nullptr, s.deviceBalls);
        if (s.deviceHoldSec >= 0) dev += (dev.isEmpty() ? QString() : QStringLiteral(" · ")) + tr("%1 s").arg(s.deviceHoldSec);
        m_table->setItem(r, CDevice, item(dev, Qt::AlignCenter));
        auto *n = item(s.note.simplified());
        n->setToolTip(s.note);
        m_table->setItem(r, CNote, n);
    }
    m_table->resizeColumnsToContents();
    m_count->setText(tr("%1 Übungseinheit(en) · %2 min").arg(m_sessions.size()).arg(minutes / 60));
    m_edit->setEnabled(false);
    m_delete->setEnabled(false);
    m_chart->setSpec(currentSpec());
}

ChartSpec HistoryPage::currentSpec() const
{
    ChartSpec s;
    s.from = m_from->date();
    s.to = m_to->date();
    s.sessions = m_sessions;
    s.showMinutes = m_cMinutes->isChecked();
    s.showSpo2 = m_cSpo2->isChecked();
    s.showPeakFlow = m_cPf->isChecked();
    s.showPulse = m_cPulse->isChecked();
    s.showDevice = m_cDevice->isChecked();
    return s;
}

void HistoryPage::addEntry()
{
    SessionRecord s;
    s.userId = m_userId;
    s.moduleId = Database::instance().userSetting(m_userId, QStringLiteral("ui/lastModule"), QStringLiteral("box"));
    if (const ModuleDef *m = BreathModules::find(s.moduleId)) s.pattern = m->defaultPattern();
    s.started = QDateTime::currentDateTime().addSecs(-600);
    s.actualSec = s.plannedSec = 300;
    s.completed = true;
    SessionEditDialog dlg(s, this);
    if (dlg.exec() != QDialog::Accepted) return;
    SessionRecord r = dlg.record();
    QString err;
    if (!Database::instance().saveSession(r, &err)) QMessageBox::warning(this, tr("Nachtragen"), err);
}

void HistoryPage::editSelected()
{
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    const qint64 id = m_table->item(rows.first().row(), CDate)->data(Qt::UserRole).toLongLong();
    for (const SessionRecord &s : m_sessions) {
        if (s.id != id) continue;
        SessionEditDialog dlg(s, this);
        if (dlg.exec() != QDialog::Accepted) return;
        SessionRecord r = dlg.record();
        QString err;
        if (!Database::instance().saveSession(r, &err)) QMessageBox::warning(this, tr("Bearbeiten"), err);
        return;
    }
}

void HistoryPage::deleteSelected()
{
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    if (QMessageBox::question(this, tr("Löschen"), tr("%n Übungseinheit(en) endgültig löschen?", nullptr, rows.size()))
        != QMessageBox::Yes)
        return;
    QList<qint64> ids;
    for (const QModelIndex &i : rows) ids << m_table->item(i.row(), CDate)->data(Qt::UserRole).toLongLong();
    for (qint64 id : ids) Database::instance().deleteSession(id);
}

void HistoryPage::printReport()
{
    const auto u = Database::instance().user(m_userId);
    if (!u) return;
    ReportPrinter::preview(this, *u, currentSpec(), {});
}

void HistoryPage::savePdf()
{
    const auto u = Database::instance().user(m_userId);
    if (!u) return;
    const QString def = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                        + QStringLiteral("/ViviaBreathe_%1_%2.pdf").arg(QString(u->name).replace(QLatin1Char(' '), QLatin1Char('_')),
                                                                       QDate::currentDate().toString(Qt::ISODate));
    const QString f = QFileDialog::getSaveFileName(this, tr("Verlauf als PDF speichern"), def, tr("PDF (*.pdf)"));
    if (f.isEmpty()) return;
    if (!ReportPrinter::savePdf(this, *u, currentSpec(), {}, f))
        QMessageBox::warning(this, tr("PDF"), tr("Die PDF-Datei konnte nicht geschrieben werden."));
}

void HistoryPage::exportCsv()
{
    const QString f = QFileDialog::getSaveFileName(this, tr("Verlauf als CSV exportieren"),
                                                   QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                                       + QStringLiteral("/ViviaBreathe.csv"),
                                                   tr("CSV (*.csv)"));
    if (f.isEmpty()) return;
    QFile file(f);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("CSV"), file.errorString());
        return;
    }
    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    out << "\xEF\xBB\xBF";   // BOM für Excel
    auto q = [](QString s) { return QLatin1Char('"') + s.replace(QLatin1Char('"'), QStringLiteral("\"\"")) + QLatin1Char('"'); };
    auto n = [](int v) { return v >= 0 ? QString::number(v) : QString(); };
    out << "Beginn;Uebung;Anlass;Muster;Geplant_s;Dauer_s;Zyklen;Vollstaendig;SpO2_vor;SpO2_nach;PeakFlow_vor;PeakFlow_nach;"
           "Puls_vor;Puls_nach;Befinden_vor;Befinden_nach;Atemtrainer_Baelle;Atemtrainer_Haltezeit_s;Notiz\n";
    for (const SessionRecord &s : m_sessions)
        out << s.started.toString(Qt::ISODate) << ';' << q(BreathModules::displayName(s.moduleId)) << ';' << q(s.eventName)
            << ';' << q(s.pattern) << ';' << s.plannedSec << ';' << s.actualSec << ';' << s.cycles << ';'
            << (s.completed ? 1 : 0) << ';' << n(s.spo2Before) << ';' << n(s.spo2After) << ';' << n(s.pfBefore) << ';'
            << n(s.pfAfter) << ';' << n(s.pulseBefore) << ';' << n(s.pulseAfter) << ';' << n(s.feelingBefore) << ';'
            << n(s.feelingAfter) << ';' << n(s.deviceBalls) << ';' << n(s.deviceHoldSec) << ';' << q(s.note) << '\n';
}
