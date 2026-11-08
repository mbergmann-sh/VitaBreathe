// SPDX-License-Identifier: MIT
#include "alarmpage.h"
#include "audio.h"
#include "breathmodules.h"
#include "database.h"
#include "eventspage.h"
#include "icons.h"
#include "playlistwidget.h"
#include "schedule.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFileInfo>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimeEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QComboBox *moduleCombo(const QString &current, const QString &noneText)
{
    auto *c = new QComboBox;
    c->addItem(noneText, QString());
    for (const ModuleDef &m : BreathModules::all()) c->addItem(Icons::get(m.icon), m.name, m.id);
    c->setCurrentIndex(qMax(0, c->findData(current)));
    return c;
}
QSlider *slider(int v)
{
    auto *s = new QSlider(Qt::Horizontal);
    s->setRange(0, 100);
    s->setValue(v);
    return s;
}
}

// ====================================================================== Wecker-Dialog

AlarmDialog::AlarmDialog(const Alarm &a, QWidget *parent) : QDialog(parent), m_alarm(a)
{
    setWindowTitle(a.id ? tr("Wecker bearbeiten") : tr("Neuer Wecker"));
    auto *lay = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_label = new QLineEdit(a.label);
    m_label->setPlaceholderText(tr("z. B. Arbeitstag"));
    form->addRow(tr("Bezeichnung:"), m_label);
    m_time = new QTimeEdit(a.time);
    m_time->setDisplayFormat(QStringLiteral("HH:mm"));
    form->addRow(tr("Weckzeit:"), m_time);
    m_days = new WeekdayPicker;
    m_days->setMask(a.weekdays);
    form->addRow(tr("Tage:"), m_days);
    m_enabled = new QCheckBox(tr("Wecker aktiv"));
    m_enabled->setChecked(a.enabled);
    form->addRow(QString(), m_enabled);
    m_playlist = new PlaylistWidget;
    m_playlist->setFiles(a.playlist);
    m_playlist->setPreviewVolume(a.volume);
    form->addRow(tr("Musik:"), m_playlist);
    auto *hint = new QLabel(tr("Ohne Musik ertönt der eingebaute Weckton."));
    hint->setEnabled(false);
    form->addRow(QString(), hint);
    m_shuffle = new QCheckBox(tr("Zufällige Reihenfolge"));
    m_shuffle->setChecked(a.shuffle);
    form->addRow(QString(), m_shuffle);
    m_volume = slider(a.volume);
    connect(m_volume, &QSlider::valueChanged, m_playlist, &PlaylistWidget::setPreviewVolume);
    form->addRow(tr("Lautstärke:"), m_volume);
    m_fade = new QSpinBox;
    m_fade->setRange(0, 15);
    m_fade->setSuffix(tr(" min"));
    m_fade->setSpecialValueText(tr("sofort volle Lautstärke"));
    m_fade->setValue(a.fadeInMin);
    form->addRow(tr("Sanft lauter werden über:"), m_fade);
    m_snooze = new QSpinBox;
    m_snooze->setRange(1, 30);
    m_snooze->setSuffix(tr(" min"));
    m_snooze->setValue(a.snoozeMin);
    form->addRow(tr("Schlummern:"), m_snooze);
    m_module = moduleCombo(a.moduleId, tr("keine Übung anbieten"));
    form->addRow(tr("Nach dem Wecken:"), m_module);
    lay->addLayout(form);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, [this] {
        if (m_days->mask() == 0) {
            QMessageBox::information(this, windowTitle(), tr("Bitte mindestens einen Wochentag wählen."));
            return;
        }
        accept();
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);
}

Alarm AlarmDialog::alarm() const
{
    Alarm a = m_alarm;
    a.label = m_label->text().trimmed();
    a.time = m_time->time();
    a.weekdays = m_days->mask();
    a.enabled = m_enabled->isChecked();
    a.playlist = m_playlist->files();
    a.shuffle = m_shuffle->isChecked();
    a.volume = m_volume->value();
    a.fadeInMin = m_fade->value();
    a.snoozeMin = m_snooze->value();
    a.moduleId = m_module->currentData().toString();
    return a;
}

// ====================================================================== Einschlaf-Dialog

SleepPlanDialog::SleepPlanDialog(const SleepPlan &p, QWidget *parent) : QDialog(parent), m_plan(p)
{
    setWindowTitle(p.id ? tr("Einschlafen bearbeiten") : tr("Neuer Einschlaf-Eintrag"));
    auto *lay = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_label = new QLineEdit(p.label);
    m_label->setPlaceholderText(tr("z. B. Wochentags, Wochenende, Meeresrauschen"));
    form->addRow(tr("Bezeichnung:"), m_label);
    m_playlist = new PlaylistWidget;
    m_playlist->setFiles(p.playlist);
    m_playlist->setPreviewVolume(p.volume);
    form->addRow(tr("Musik:"), m_playlist);
    m_shuffle = new QCheckBox(tr("Zufällige Reihenfolge"));
    m_shuffle->setChecked(p.shuffle);
    form->addRow(QString(), m_shuffle);
    m_volume = slider(p.volume);
    connect(m_volume, &QSlider::valueChanged, m_playlist, &PlaylistWidget::setPreviewVolume);
    form->addRow(tr("Lautstärke:"), m_volume);
    m_minutes = new QSpinBox;
    m_minutes->setRange(5, 240);
    m_minutes->setSingleStep(5);
    m_minutes->setSuffix(tr(" min"));
    m_minutes->setValue(p.minutes);
    form->addRow(tr("Wiedergabedauer:"), m_minutes);
    m_fade = new QSpinBox;
    m_fade->setRange(0, 60);
    m_fade->setSuffix(tr(" min"));
    m_fade->setValue(p.fadeOutMin);
    form->addRow(tr("davon leiser werden:"), m_fade);
    m_module = moduleCombo(p.moduleId, tr("keine Übung – nur Musik"));
    form->addRow(tr("Vorher üben:"), m_module);

    auto *plan = new QGroupBox(tr("Planen"));
    auto *pf = new QFormLayout(plan);
    m_scheduled = new QCheckBox(tr("zu einer festen Uhrzeit anbieten"));
    m_scheduled->setChecked(p.scheduled);
    pf->addRow(QString(), m_scheduled);
    m_time = new QTimeEdit(p.time);
    m_time->setDisplayFormat(QStringLiteral("HH:mm"));
    pf->addRow(tr("Uhrzeit:"), m_time);
    m_days = new WeekdayPicker;
    m_days->setMask(p.weekdays);
    pf->addRow(tr("Tage:"), m_days);
    m_autoStart = new QCheckBox(tr("ohne Rückfrage automatisch starten"));
    m_autoStart->setChecked(p.autoStart);
    m_autoStart->setToolTip(tr("Sonst erinnert VitaBreathe zur Uhrzeit und fragt, ob die Einschlaffunktion starten soll."));
    pf->addRow(QString(), m_autoStart);
    auto syncEnabled = [this] {
        const bool on = m_scheduled->isChecked();
        m_time->setEnabled(on);
        m_days->setEnabled(on);
        m_autoStart->setEnabled(on);
    };
    connect(m_scheduled, &QCheckBox::toggled, this, syncEnabled);
    syncEnabled();
    lay->addLayout(form);
    lay->addWidget(plan);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, [this] {
        if (m_label->text().trimmed().isEmpty()) {
            QMessageBox::information(this, windowTitle(), tr("Bitte eine Bezeichnung eingeben."));
            return;
        }
        if (m_scheduled->isChecked() && m_days->mask() == 0) {
            QMessageBox::information(this, windowTitle(), tr("Bitte mindestens einen Wochentag wählen."));
            return;
        }
        accept();
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);
}

SleepPlan SleepPlanDialog::plan() const
{
    SleepPlan p = m_plan;
    p.label = m_label->text().trimmed();
    p.playlist = m_playlist->files();
    p.shuffle = m_shuffle->isChecked();
    p.volume = m_volume->value();
    p.minutes = m_minutes->value();
    p.fadeOutMin = qMin(m_fade->value(), p.minutes);
    p.moduleId = m_module->currentData().toString();
    p.scheduled = m_scheduled->isChecked();
    p.time = m_time->time();
    p.weekdays = m_days->mask();
    p.autoStart = m_autoStart->isChecked();
    return p;
}

// ====================================================================== Seite

namespace {
QTableWidget *listTable(const QStringList &headers)
{
    auto *t = new QTableWidget(0, headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setMinimumHeight(130);
    return t;
}
qint64 selectedId(const QTableWidget *t)
{
    const auto rows = t->selectionModel()->selectedRows();
    if (rows.isEmpty()) return 0;
    return t->item(rows.first().row(), 0)->data(Qt::UserRole).toLongLong();
}
}

AlarmPage::AlarmPage(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *content = new QWidget;
    auto *lay = new QVBoxLayout(content);

    // --- Wecker
    auto *wake = new QGroupBox(tr("Weckfunktion"));
    auto *wl = new QVBoxLayout(wake);
    m_table = listTable({ tr("Weckzeit"), tr("Bezeichnung"), tr("Tage"), tr("Aktiv"), tr("Musik"), tr("Nächstes Wecken") });
    wl->addWidget(m_table);
    auto *bl = new QHBoxLayout;
    auto *add = new QPushButton(Icons::get(QStringLiteral("add")), tr("Neuer Wecker …"));
    m_edit = new QPushButton(Icons::get(QStringLiteral("edit")), tr("Bearbeiten …"));
    m_delete = new QPushButton(Icons::get(QStringLiteral("delete")), tr("Löschen"));
    m_test = new QPushButton(Icons::get(QStringLiteral("alarm")), tr("Testen"));
    for (QPushButton *b : { add, m_edit, m_delete }) bl->addWidget(b);
    bl->addStretch();
    bl->addWidget(m_test);
    wl->addLayout(bl);
    auto *wakeInfo = new QLabel(tr("Der Wecker klingelt, solange VitaBreathe läuft (auch minimiert im Infobereich). "
                                   "Bitte verlassen Sie sich nicht allein darauf, wenn Sie einen wichtigen Termin haben."));
    wakeInfo->setWordWrap(true);
    wakeInfo->setEnabled(false);
    wl->addWidget(wakeInfo);
    lay->addWidget(wake);

    // --- Einschlafen
    auto *sleep = new QGroupBox(tr("Einschlaffunktion"));
    auto *sl = new QVBoxLayout(sleep);
    m_sleepTable = listTable({ tr("Bezeichnung"), tr("Geplant"), tr("Tage"), tr("Start"), tr("Musik"), tr("Dauer"),
                               tr("Vorher üben"), tr("Nächster Termin") });
    sl->addWidget(m_sleepTable);
    auto *sb = new QHBoxLayout;
    auto *sAdd = new QPushButton(Icons::get(QStringLiteral("add")), tr("Neuer Eintrag …"));
    m_sleepEdit = new QPushButton(Icons::get(QStringLiteral("edit")), tr("Bearbeiten …"));
    m_sleepDelete = new QPushButton(Icons::get(QStringLiteral("delete")), tr("Löschen"));
    m_sleepStart = new QPushButton(Icons::get(QStringLiteral("moon")), tr("Jetzt einschlafen"));
    m_sleepStop = new QPushButton(Icons::get(QStringLiteral("stop")), tr("Stopp"));
    for (QPushButton *b : { sAdd, m_sleepEdit, m_sleepDelete }) sb->addWidget(b);
    sb->addStretch();
    sb->addWidget(m_sleepStart);
    sb->addWidget(m_sleepStop);
    sl->addLayout(sb);
    m_sleepStatus = new QLabel;
    m_sleepStatus->setWordWrap(true);
    sl->addWidget(m_sleepStatus);
    auto *sleepInfo = new QLabel(tr("Geplante Einträge erinnern zur Uhrzeit an das Einschlafen oder starten automatisch. "
                                    "Ungeplante Einträge starten Sie hier, über die Werkzeugleiste oder das Symbol im Infobereich."));
    sleepInfo->setWordWrap(true);
    sleepInfo->setEnabled(false);
    sl->addWidget(sleepInfo);
    lay->addWidget(sleep);
    lay->addStretch();

    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setWidget(content);
    outer->addWidget(sa);

    m_sleepPlayer = new MusicPlayer(this);
    m_sleepPlayer->setLoop(true);
    m_sleepTimer = new QTimer(this);
    m_sleepTimer->setInterval(1000);
    connect(m_sleepTimer, &QTimer::timeout, this, &AlarmPage::updateSleepStatus);
    connect(m_sleepPlayer, &MusicPlayer::stopped, this, [this] {
        m_sleepTimer->stop();
        m_sleepEnd = 0;
        updateSleepStatus();
    });
    connect(m_sleepPlayer, &MusicPlayer::trackChanged, this, [this] { updateSleepStatus(); });

    // Wecker
    connect(add, &QPushButton::clicked, this, &AlarmPage::addAlarm);
    connect(m_edit, &QPushButton::clicked, this, &AlarmPage::editAlarm);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &AlarmPage::editAlarm);
    connect(m_delete, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedAlarm();
        if (id && QMessageBox::question(this, tr("Löschen"), tr("Diesen Wecker löschen?")) == QMessageBox::Yes)
            Database::instance().deleteAlarm(id);
    });
    connect(m_test, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedAlarm();
        for (const Alarm &a : m_alarms)
            if (a.id == id) emit testAlarm(a);
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        const bool sel = selectedAlarm() != 0;
        m_edit->setEnabled(sel);
        m_delete->setEnabled(sel);
        m_test->setEnabled(sel);
    });
    // Einschlafen
    connect(sAdd, &QPushButton::clicked, this, &AlarmPage::addSleepPlan);
    connect(m_sleepEdit, &QPushButton::clicked, this, &AlarmPage::editPlan);
    connect(m_sleepTable, &QTableWidget::cellDoubleClicked, this, &AlarmPage::editPlan);
    connect(m_sleepDelete, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedPlan();
        if (id && QMessageBox::question(this, tr("Löschen"), tr("Diesen Einschlaf-Eintrag löschen?")) == QMessageBox::Yes)
            Database::instance().deleteSleepPlan(id);
    });
    connect(m_sleepStart, &QPushButton::clicked, this, [this] { startSleep(); });
    connect(m_sleepStop, &QPushButton::clicked, this, &AlarmPage::stopSleep);
    connect(m_sleepTable, &QTableWidget::itemSelectionChanged, this, [this] {
        const bool sel = selectedPlan() != 0;
        m_sleepEdit->setEnabled(sel);
        m_sleepDelete->setEnabled(sel);
        updateSleepStatus();
    });
    if (!MusicPlayer::available()) {
        sleep->setEnabled(false);
        sleep->setToolTip(tr("Ohne Qt Multimedia übersetzt – keine Musikwiedergabe."));
    }
    updateSleepStatus();
}

void AlarmPage::setUser(qint64 userId)
{
    m_userId = userId;
    reload();
}

qint64 AlarmPage::selectedAlarm() const { return selectedId(m_table); }
qint64 AlarmPage::selectedPlan() const { return selectedId(m_sleepTable); }

void AlarmPage::reload()
{
    if (!Database::instance().isOpen()) return;
    m_alarms = Database::instance().alarms(m_userId);
    m_table->setRowCount(m_alarms.size());
    for (int r = 0; r < m_alarms.size(); ++r) {
        const Alarm &a = m_alarms.at(r);
        auto *t = new QTableWidgetItem(Icons::get(QStringLiteral("alarm")), a.time.toString(QStringLiteral("HH:mm")));
        t->setData(Qt::UserRole, a.id);
        m_table->setItem(r, 0, t);
        m_table->setItem(r, 1, new QTableWidgetItem(a.label));
        m_table->setItem(r, 2, new QTableWidgetItem(Weekdays::text(a.weekdays)));
        m_table->setItem(r, 3, new QTableWidgetItem(a.enabled ? tr("ja") : tr("nein")));
        m_table->setItem(r, 4, new QTableWidgetItem(a.playlist.isEmpty() ? tr("Weckton") : tr("%n Titel", nullptr, a.playlist.size())));
        m_table->setItem(r, 5, new QTableWidgetItem(a.enabled ? Schedule::relative(Schedule::next(a.time, a.weekdays))
                                                              : QStringLiteral("–")));
    }
    m_table->resizeColumnsToContents();
    m_edit->setEnabled(false);
    m_delete->setEnabled(false);
    m_test->setEnabled(false);

    const qint64 keep = selectedPlan();
    m_plans = Database::instance().sleepPlans(m_userId);
    m_sleepTable->setRowCount(m_plans.size());
    for (int r = 0; r < m_plans.size(); ++r) {
        const SleepPlan &p = m_plans.at(r);
        auto *l = new QTableWidgetItem(Icons::get(QStringLiteral("moon")), p.label);
        l->setData(Qt::UserRole, p.id);
        m_sleepTable->setItem(r, 0, l);
        m_sleepTable->setItem(r, 1, new QTableWidgetItem(p.scheduled ? p.time.toString(QStringLiteral("HH:mm")) : tr("nein")));
        m_sleepTable->setItem(r, 2, new QTableWidgetItem(p.scheduled ? Weekdays::text(p.weekdays) : QStringLiteral("–")));
        m_sleepTable->setItem(r, 3, new QTableWidgetItem(!p.scheduled ? QStringLiteral("–")
                                                                       : p.autoStart ? tr("automatisch") : tr("mit Rückfrage")));
        m_sleepTable->setItem(r, 4, new QTableWidgetItem(tr("%n Titel", nullptr, p.playlist.size())));
        m_sleepTable->setItem(r, 5, new QTableWidgetItem(tr("%1 min").arg(p.minutes)));
        m_sleepTable->setItem(r, 6, new QTableWidgetItem(p.moduleId.isEmpty() ? QStringLiteral("–")
                                                                              : BreathModules::displayName(p.moduleId)));
        m_sleepTable->setItem(r, 7, new QTableWidgetItem(p.scheduled ? Schedule::relative(Schedule::next(p.time, p.weekdays))
                                                                     : QStringLiteral("–")));
        if (p.id == keep) m_sleepTable->selectRow(r);
    }
    m_sleepTable->resizeColumnsToContents();
    m_sleepEdit->setEnabled(selectedPlan() != 0);
    m_sleepDelete->setEnabled(selectedPlan() != 0);
    updateSleepStatus();
}

void AlarmPage::addAlarm()
{
    Alarm a;
    a.userId = m_userId;
    AlarmDialog dlg(a, this);
    if (dlg.exec() != QDialog::Accepted) return;
    Alarm n = dlg.alarm();
    QString err;
    if (!Database::instance().saveAlarm(n, &err)) QMessageBox::warning(this, tr("Wecker"), err);
}

void AlarmPage::editAlarm()
{
    const qint64 id = selectedAlarm();
    for (const Alarm &a : m_alarms) {
        if (a.id != id) continue;
        AlarmDialog dlg(a, this);
        if (dlg.exec() != QDialog::Accepted) return;
        Alarm n = dlg.alarm();
        QString err;
        if (!Database::instance().saveAlarm(n, &err)) QMessageBox::warning(this, tr("Wecker"), err);
        return;
    }
}

void AlarmPage::addSleepPlan()
{
    SleepPlan p;
    p.userId = m_userId;
    p.label = m_plans.isEmpty() ? tr("Einschlafen") : QString();
    p.moduleId = QStringLiteral("four_six");
    SleepPlanDialog dlg(p, this);
    if (dlg.exec() != QDialog::Accepted) return;
    SleepPlan n = dlg.plan();
    QString err;
    if (!Database::instance().saveSleepPlan(n, &err)) QMessageBox::warning(this, tr("Einschlafen"), err);
}

void AlarmPage::editPlan()
{
    const qint64 id = selectedPlan();
    for (const SleepPlan &p : m_plans) {
        if (p.id != id) continue;
        SleepPlanDialog dlg(p, this);
        if (dlg.exec() != QDialog::Accepted) return;
        SleepPlan n = dlg.plan();
        QString err;
        if (!Database::instance().saveSleepPlan(n, &err)) QMessageBox::warning(this, tr("Einschlafen"), err);
        return;
    }
}

// ---------------------------------------------------------------- Einschlafen

void AlarmPage::startSleep()
{
    if (m_plans.isEmpty()) {
        if (QMessageBox::question(this, tr("Einschlafen"), tr("Es gibt noch keinen Einschlaf-Eintrag. Jetzt einen anlegen?"))
            == QMessageBox::Yes)
            addSleepPlan();
        return;
    }
    const qint64 sel = selectedPlan();
    for (const SleepPlan &p : m_plans)
        if (p.id == sel) { startSleep(p); return; }
    if (m_plans.size() == 1) { startSleep(m_plans.first()); return; }
    QStringList names;
    for (const SleepPlan &p : m_plans) names << p.label;
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, tr("Einschlafen"), tr("Welcher Eintrag soll starten?"), names, 0, false, &ok);
    if (!ok) return;
    startSleep(m_plans.at(qMax(0, names.indexOf(choice))));
}

void AlarmPage::startSleep(const SleepPlan &p)
{
    if (p.playlist.isEmpty()) {
        QMessageBox::information(this, tr("Einschlafen"), tr("„%1“ enthält noch keine Musik. Bitte MP3-Dateien hinzufügen.").arg(p.label));
        return;
    }
    m_sleepPlayer->stop();
    m_active = p;
    if (!p.moduleId.isEmpty()) emit sleepExerciseRequested(p.moduleId);   // MainWindow startet danach startSleepMusic()
    else startSleepMusic();
}

void AlarmPage::startSleepMusic()
{
    if (m_active.playlist.isEmpty()) return;
    m_sleepPlayer->setPlaylist(m_active.playlist, m_active.shuffle);
    m_sleepPlayer->setVolume(0);
    m_sleepPlayer->play();
    m_sleepPlayer->fadeTo(m_active.volume, 5000);
    m_sleepFading = false;
    m_sleepEnd = QDateTime::currentMSecsSinceEpoch() + qint64(m_active.minutes) * 60000;
    m_sleepTimer->start();
    updateSleepStatus();
}

void AlarmPage::stopSleep()
{
    m_sleepPlayer->fadeTo(0, 1500, true);
    m_sleepEnd = 0;
}

bool AlarmPage::sleepRunning() const
{
    return m_sleepPlayer->isPlaying();
}

void AlarmPage::updateSleepStatus()
{
    const bool running = m_sleepPlayer->isPlaying() && m_sleepEnd > 0;
    m_sleepStart->setEnabled(!running && !m_plans.isEmpty());
    m_sleepStop->setEnabled(running);
    if (!running) {
        m_sleepStatus->setText(m_sleepPlayer->isPlaying() ? tr("wird ausgeblendet …") : QString());
        return;
    }
    const qint64 left = m_sleepEnd - QDateTime::currentMSecsSinceEpoch();
    const qint64 fadeMs = qint64(m_active.fadeOutMin) * 60000;
    if (!m_sleepFading && left <= fadeMs) {
        m_sleepFading = true;
        m_sleepPlayer->fadeTo(0, int(qMax<qint64>(1000, left)), true);
    }
    if (left <= 0 && !m_sleepFading) m_sleepPlayer->stop();
    const int sec = int(qMax<qint64>(0, left) / 1000);
    m_sleepStatus->setText(tr("<b>%1:</b> Gute Nacht – Musik endet in %2:%3 min  ·  %4")
                               .arg(m_active.label.toHtmlEscaped())
                               .arg(sec / 60).arg(sec % 60, 2, 10, QLatin1Char('0'))
                               .arg(QFileInfo(m_sleepPlayer->currentFile()).completeBaseName().toHtmlEscaped()));
}
