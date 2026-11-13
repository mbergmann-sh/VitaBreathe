// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "eventspage.h"
#include "breathmodules.h"
#include "database.h"
#include "icons.h"
#include "schedule.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTimeEdit>
#include <QToolButton>
#include <QVBoxLayout>

// ====================================================================== Wochentage

WeekdayPicker::WeekdayPicker(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    auto *days = new QHBoxLayout;
    const QString names[7] = { tr("Mo"), tr("Di"), tr("Mi"), tr("Do"), tr("Fr"), tr("Sa"), tr("So") };
    for (int i = 0; i < 7; ++i) {
        m_days[i] = new QCheckBox(names[i]);
        days->addWidget(m_days[i]);
    }
    days->addStretch();
    lay->addLayout(days);
    auto *quick = new QHBoxLayout;
    auto addQuick = [&](const QString &t, int m) {
        auto *b = new QToolButton;
        b->setText(t);
        connect(b, &QToolButton::clicked, this, [this, m] { setMask(m); });
        quick->addWidget(b);
    };
    addQuick(tr("täglich"), Weekdays::kAll);
    addQuick(tr("werktags"), Weekdays::kWorkdays);
    addQuick(tr("Wochenende"), 0x60);
    quick->addStretch();
    lay->addLayout(quick);
}

void WeekdayPicker::setMask(int m)
{
    for (int i = 0; i < 7; ++i) m_days[i]->setChecked(m & (1 << i));
}

int WeekdayPicker::mask() const
{
    int m = 0;
    for (int i = 0; i < 7; ++i)
        if (m_days[i]->isChecked()) m |= 1 << i;
    return m;
}

// ====================================================================== Dialog

EventDialog::EventDialog(const BreathEvent &e, QWidget *parent) : QDialog(parent), m_ev(e)
{
    setWindowTitle(e.id ? tr("Ereignis bearbeiten") : tr("Neues Ereignis"));
    auto *lay = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_name = new QComboBox;
    m_name->setEditable(true);
    m_name->addItems(Database::instance().eventNames(e.userId));
    m_name->setCurrentText(e.name);
    form->addRow(tr("Ereignis:"), m_name);
    m_module = new QComboBox;
    for (const ModuleDef &m : BreathModules::all()) m_module->addItem(Icons::get(m.icon), m.name, m.id);
    m_module->setCurrentIndex(qMax(0, m_module->findData(e.moduleId)));
    form->addRow(tr("Übung:"), m_module);
    m_time = new QTimeEdit(e.time);
    m_time->setDisplayFormat(QStringLiteral("HH:mm"));
    form->addRow(tr("Uhrzeit:"), m_time);
    m_days = new WeekdayPicker;
    m_days->setMask(e.weekdays);
    form->addRow(tr("Tage:"), m_days);
    m_enabled = new QCheckBox(tr("Erinnerung aktiv"));
    m_enabled->setChecked(e.enabled);
    form->addRow(QString(), m_enabled);
    m_sound = new QCheckBox(tr("mit Erinnerungston"));
    m_sound->setChecked(e.sound);
    form->addRow(QString(), m_sound);
    m_note = new QPlainTextEdit(e.note);
    m_note->setMaximumHeight(70);
    form->addRow(tr("Notiz:"), m_note);
    lay->addLayout(form);
    auto *info = new QLabel(tr("Zur eingestellten Zeit erinnert ViviaBreathe – auch im Hintergrund (Symbol im Infobereich) – "
                               "und bietet an, die Übung direkt zu starten."));
    info->setWordWrap(true);
    info->setEnabled(false);
    lay->addWidget(info);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, [this] {
        if (m_name->currentText().trimmed().isEmpty() || m_days->mask() == 0) {
            QMessageBox::information(this, windowTitle(), tr("Bitte einen Namen und mindestens einen Wochentag angeben."));
            return;
        }
        accept();
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);
}

BreathEvent EventDialog::breathEvent() const
{
    BreathEvent e = m_ev;
    e.name = m_name->currentText().trimmed();
    e.moduleId = m_module->currentData().toString();
    e.time = m_time->time();
    e.weekdays = m_days->mask();
    e.enabled = m_enabled->isChecked();
    e.sound = m_sound->isChecked();
    e.note = m_note->toPlainText().trimmed();
    return e;
}

// ====================================================================== Seite

EventsPage::EventsPage(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Legen Sie Ereignisse wie „Einschlafübung“ oder „Stressbewältigung“ fest. ViviaBreathe "
                                "erinnert Sie zur gewählten Zeit und startet auf Wunsch gleich die passende Übung."));
    intro->setWordWrap(true);
    lay->addWidget(intro);
    m_table = new QTableWidget(0, 6);
    m_table->setHorizontalHeaderLabels({ tr("Ereignis"), tr("Übung"), tr("Uhrzeit"), tr("Tage"), tr("Aktiv"),
                                         tr("Nächste Erinnerung") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setAlternatingRowColors(true);
    lay->addWidget(m_table, 1);
    auto *bl = new QHBoxLayout;
    auto *add = new QPushButton(Icons::get(QStringLiteral("add")), tr("Neues Ereignis …"));
    m_edit = new QPushButton(Icons::get(QStringLiteral("edit")), tr("Bearbeiten …"));
    m_delete = new QPushButton(Icons::get(QStringLiteral("delete")), tr("Löschen"));
    m_start = new QPushButton(Icons::get(QStringLiteral("play")), tr("Übung jetzt starten"));
    bl->addWidget(add);
    bl->addWidget(m_edit);
    bl->addWidget(m_delete);
    bl->addStretch();
    bl->addWidget(m_start);
    lay->addLayout(bl);

    connect(add, &QPushButton::clicked, this, &EventsPage::addEvent);
    connect(m_edit, &QPushButton::clicked, this, &EventsPage::editSelected);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &EventsPage::editSelected);
    connect(m_delete, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedId();
        if (!id) return;
        if (QMessageBox::question(this, tr("Löschen"), tr("Dieses Ereignis löschen?")) == QMessageBox::Yes)
            Database::instance().deleteEvent(id);
    });
    connect(m_start, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedId();
        for (const BreathEvent &e : m_events)
            if (e.id == id) emit startRequested(e.moduleId, e.name);
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        const bool sel = selectedId() != 0;
        m_edit->setEnabled(sel);
        m_delete->setEnabled(sel);
        m_start->setEnabled(sel);
    });
}

void EventsPage::setUser(qint64 userId)
{
    m_userId = userId;
    reload();
}

qint64 EventsPage::selectedId() const
{
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) return 0;
    return m_table->item(rows.first().row(), 0)->data(Qt::UserRole).toLongLong();
}

void EventsPage::reload()
{
    if (!Database::instance().isOpen()) return;
    m_events = Database::instance().events(m_userId);
    m_table->setRowCount(m_events.size());
    for (int r = 0; r < m_events.size(); ++r) {
        const BreathEvent &e = m_events.at(r);
        auto *n = new QTableWidgetItem(Icons::get(QStringLiteral("event")), e.name);
        n->setData(Qt::UserRole, e.id);
        n->setToolTip(e.note);
        m_table->setItem(r, 0, n);
        auto *m = new QTableWidgetItem(BreathModules::displayName(e.moduleId));
        if (const ModuleDef *d = BreathModules::find(e.moduleId)) m->setIcon(Icons::get(d->icon));
        m_table->setItem(r, 1, m);
        m_table->setItem(r, 2, new QTableWidgetItem(e.time.toString(QStringLiteral("HH:mm"))));
        m_table->setItem(r, 3, new QTableWidgetItem(Weekdays::text(e.weekdays)));
        m_table->setItem(r, 4, new QTableWidgetItem(e.enabled ? tr("ja") : tr("nein")));
        m_table->setItem(r, 5, new QTableWidgetItem(e.enabled ? Schedule::relative(Schedule::next(e.time, e.weekdays))
                                                              : QStringLiteral("–")));
    }
    m_table->resizeColumnsToContents();
    m_edit->setEnabled(false);
    m_delete->setEnabled(false);
    m_start->setEnabled(false);
}

void EventsPage::addEvent()
{
    BreathEvent e;
    e.userId = m_userId;
    e.name = tr("Einschlafübung");
    e.moduleId = QStringLiteral("four_six");
    EventDialog dlg(e, this);
    if (dlg.exec() != QDialog::Accepted) return;
    BreathEvent ev = dlg.breathEvent();
    QString err;
    if (!Database::instance().saveEvent(ev, &err)) QMessageBox::warning(this, tr("Ereignis"), err);
}

void EventsPage::editSelected()
{
    const qint64 id = selectedId();
    for (const BreathEvent &e : m_events) {
        if (e.id != id) continue;
        EventDialog dlg(e, this);
        if (dlg.exec() != QDialog::Accepted) return;
        BreathEvent ev = dlg.breathEvent();
        QString err;
        if (!Database::instance().saveEvent(ev, &err)) QMessageBox::warning(this, tr("Ereignis"), err);
        return;
    }
}
