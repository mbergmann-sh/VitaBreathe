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

// ====================================================================== Seite

AlarmPage::AlarmPage(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *content = new QWidget;
    auto *lay = new QVBoxLayout(content);

    // --- Wecker
    auto *wake = new QGroupBox(tr("Weckfunktion"));
    auto *wl = new QVBoxLayout(wake);
    m_table = new QTableWidget(0, 6);
    m_table->setHorizontalHeaderLabels({ tr("Weckzeit"), tr("Bezeichnung"), tr("Tage"), tr("Aktiv"), tr("Musik"),
                                         tr("Nächstes Wecken") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setMinimumHeight(140);
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
    auto *sg = new QGridLayout(sleep);
    sg->addWidget(new QLabel(tr("Musik:")), 0, 0, Qt::AlignTop);
    m_sleepList = new PlaylistWidget;
    m_sleepList->setMaximumHeight(170);
    sg->addWidget(m_sleepList, 0, 1, 1, 3);
    m_sleepShuffle = new QCheckBox(tr("Zufällige Reihenfolge"));
    sg->addWidget(m_sleepShuffle, 1, 1, 1, 3);
    sg->addWidget(new QLabel(tr("Lautstärke:")), 2, 0);
    m_sleepVolume = slider(50);
    sg->addWidget(m_sleepVolume, 2, 1, 1, 3);
    sg->addWidget(new QLabel(tr("Wiedergabedauer:")), 3, 0);
    m_sleepMinutes = new QSpinBox;
    m_sleepMinutes->setRange(5, 180);
    m_sleepMinutes->setSingleStep(5);
    m_sleepMinutes->setSuffix(tr(" min"));
    sg->addWidget(m_sleepMinutes, 3, 1);
    sg->addWidget(new QLabel(tr("davon leiser werden:")), 3, 2, Qt::AlignRight);
    m_sleepFade = new QSpinBox;
    m_sleepFade->setRange(0, 30);
    m_sleepFade->setSuffix(tr(" min"));
    sg->addWidget(m_sleepFade, 3, 3);
    sg->addWidget(new QLabel(tr("Vorher üben:")), 4, 0);
    m_sleepModule = moduleCombo(QString(), tr("keine Übung – nur Musik"));
    sg->addWidget(m_sleepModule, 4, 1, 1, 3);
    auto *sb = new QHBoxLayout;
    m_sleepStart = new QPushButton(Icons::get(QStringLiteral("moon")), tr("Einschlafen starten"));
    m_sleepStop = new QPushButton(Icons::get(QStringLiteral("stop")), tr("Stopp"));
    m_sleepStatus = new QLabel;
    sb->addWidget(m_sleepStart);
    sb->addWidget(m_sleepStop);
    sb->addWidget(m_sleepStatus, 1);
    sg->addLayout(sb, 5, 0, 1, 4);
    sg->setColumnStretch(1, 1);
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

    connect(add, &QPushButton::clicked, this, &AlarmPage::addAlarm);
    connect(m_edit, &QPushButton::clicked, this, &AlarmPage::editSelected);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &AlarmPage::editSelected);
    connect(m_delete, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedId();
        if (id && QMessageBox::question(this, tr("Löschen"), tr("Diesen Wecker löschen?")) == QMessageBox::Yes)
            Database::instance().deleteAlarm(id);
    });
    connect(m_test, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedId();
        for (const Alarm &a : m_alarms)
            if (a.id == id) emit testAlarm(a);
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        const bool sel = selectedId() != 0;
        m_edit->setEnabled(sel);
        m_delete->setEnabled(sel);
        m_test->setEnabled(sel);
    });
    connect(m_sleepStart, &QPushButton::clicked, this, &AlarmPage::startSleep);
    connect(m_sleepStop, &QPushButton::clicked, this, &AlarmPage::stopSleep);
    connect(m_sleepVolume, &QSlider::valueChanged, this, [this](int v) {
        m_sleepList->setPreviewVolume(v);
        if (m_sleepPlayer->isPlaying() && !m_sleepFading) m_sleepPlayer->setVolume(v);
        saveSleep();
    });
    connect(m_sleepList, &PlaylistWidget::changed, this, &AlarmPage::saveSleep);
    connect(m_sleepShuffle, &QCheckBox::toggled, this, &AlarmPage::saveSleep);
    connect(m_sleepMinutes, qOverload<int>(&QSpinBox::valueChanged), this, &AlarmPage::saveSleep);
    connect(m_sleepFade, qOverload<int>(&QSpinBox::valueChanged), this, &AlarmPage::saveSleep);
    connect(m_sleepModule, qOverload<int>(&QComboBox::currentIndexChanged), this, &AlarmPage::saveSleep);
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

qint64 AlarmPage::selectedId() const
{
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) return 0;
    return m_table->item(rows.first().row(), 0)->data(Qt::UserRole).toLongLong();
}

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

    // Einschlafen
    m_loading = true;
    const SleepSettings s = Database::instance().sleepSettings(m_userId);
    m_sleepList->setFiles(s.playlist);
    m_sleepShuffle->setChecked(s.shuffle);
    m_sleepVolume->setValue(s.volume);
    m_sleepList->setPreviewVolume(s.volume);
    m_sleepMinutes->setValue(s.minutes);
    m_sleepFade->setValue(s.fadeOutMin);
    m_sleepModule->setCurrentIndex(qMax(0, m_sleepModule->findData(s.moduleId)));
    m_loading = false;
}

void AlarmPage::saveSleep()
{
    if (m_loading || !m_userId) return;
    SleepSettings s;
    s.playlist = m_sleepList->files();
    s.shuffle = m_sleepShuffle->isChecked();
    s.volume = m_sleepVolume->value();
    s.minutes = m_sleepMinutes->value();
    s.fadeOutMin = qMin(m_sleepFade->value(), s.minutes);
    s.moduleId = m_sleepModule->currentData().toString();
    Database::instance().saveSleepSettings(m_userId, s);
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

void AlarmPage::editSelected()
{
    const qint64 id = selectedId();
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

// ---------------------------------------------------------------- Einschlafen

void AlarmPage::startSleep()
{
    if (m_sleepList->files().isEmpty()) {
        QMessageBox::information(this, tr("Einschlafen"), tr("Bitte zuerst Musik (MP3-Dateien) zur Liste hinzufügen."));
        return;
    }
    saveSleep();
    const QString module = m_sleepModule->currentData().toString();
    if (!module.isEmpty()) emit sleepExerciseRequested(module);   // MainWindow startet danach startSleepMusic()
    else startSleepMusic();
}

void AlarmPage::startSleepMusic()
{
    const QStringList files = m_sleepList->files();
    if (files.isEmpty()) return;
    m_sleepPlayer->setPlaylist(files, m_sleepShuffle->isChecked());
    m_sleepPlayer->setVolume(0);
    m_sleepPlayer->play();
    m_sleepPlayer->fadeTo(m_sleepVolume->value(), 5000);
    m_sleepFading = false;
    m_sleepEnd = QDateTime::currentMSecsSinceEpoch() + qint64(m_sleepMinutes->value()) * 60000;
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
    m_sleepStart->setEnabled(!running);
    m_sleepStop->setEnabled(running);
    if (!running) {
        m_sleepStatus->setText(m_sleepPlayer->isPlaying() ? tr("wird ausgeblendet …") : QString());
        return;
    }
    const qint64 left = m_sleepEnd - QDateTime::currentMSecsSinceEpoch();
    const qint64 fadeMs = qint64(m_sleepFade->value()) * 60000;
    if (!m_sleepFading && left <= fadeMs) {
        m_sleepFading = true;
        m_sleepPlayer->fadeTo(0, int(qMax<qint64>(1000, left)), true);
    }
    if (left <= 0 && !m_sleepFading) m_sleepPlayer->stop();
    const int sec = int(qMax<qint64>(0, left) / 1000);
    m_sleepStatus->setText(tr("Gute Nacht – Musik endet in %1:%2 min  ·  %3")
                               .arg(sec / 60).arg(sec % 60, 2, 10, QLatin1Char('0'))
                               .arg(QFileInfo(m_sleepPlayer->currentFile()).completeBaseName()));
}
