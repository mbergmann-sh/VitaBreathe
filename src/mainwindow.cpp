// SPDX-License-Identifier: MIT
#include "mainwindow.h"
#include "alarmpage.h"
#include "breathmodules.h"
#include "database.h"
#include "dbsync.h"
#include "eventspage.h"
#include "historypage.h"
#include "i18n.h"
#include "icons.h"
#include "reminderservice.h"
#include "schedule.h"
#include "sessiondialog.h"
#include "settingsdialog.h"
#include "themes.h"
#include "userdialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
enum Tab { TabExercises, TabHistory, TabEvents, TabAlarm };
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("VitaBreathe"));
    setWindowIcon(Icons::app());
    m_reminders = new ReminderService(this);

    auto *central = new QWidget;
    auto *lay = new QVBoxLayout(central);

    // Kopfbereich zur gewählten Person (wie das Stammblatt in VitaParms)
    auto *head = new QFrame;
    head->setFrameShape(QFrame::StyledPanel);
    head->setFrameShadow(QFrame::Sunken);
    auto *hl = new QHBoxLayout(head);
    auto *logo = new QLabel;
    logo->setPixmap(QIcon(QStringLiteral(":/icons/vitabreathe.png")).pixmap(44, 44));
    hl->addWidget(logo, 0, Qt::AlignTop);
    auto *ht = new QVBoxLayout;
    m_headName = new QLabel;
    QFont nf = m_headName->font();
    nf.setPointSizeF(nf.pointSizeF() * 1.45);
    nf.setBold(true);
    m_headName->setFont(nf);
    ht->addWidget(m_headName);
    m_headInfo = new QLabel;
    m_headInfo->setWordWrap(true);
    m_headInfo->setTextFormat(Qt::RichText);
    ht->addWidget(m_headInfo);
    hl->addLayout(ht, 1);
    lay->addWidget(head);

    m_tabs = new QTabWidget;
    m_tabs->addTab(buildExercisesPage(), Icons::get(QStringLiteral("play")), tr("Übungen"));
    m_history = new HistoryPage;
    m_tabs->addTab(m_history, Icons::get(QStringLiteral("chart")), tr("Verlauf && Grafik"));
    m_events = new EventsPage;
    m_tabs->addTab(m_events, Icons::get(QStringLiteral("bell")), tr("Ereignisse && Erinnerungen"));
    m_alarms = new AlarmPage;
    m_tabs->addTab(m_alarms, Icons::get(QStringLiteral("alarm")), tr("Wecker && Einschlafen"));
    lay->addWidget(m_tabs, 1);
    setCentralWidget(central);

    buildMenus();
    buildToolBar();

    // Statuszeile
    m_statusDb = new QLabel;
    statusBar()->addWidget(m_statusDb, 1);
    m_statusSync = new QLabel;
    statusBar()->addPermanentWidget(m_statusSync);
    m_statusLock = new QLabel;
    m_statusLock->setText(tr("🔒 verschlüsselt"));
    m_statusLock->setToolTip(tr("Die Datenbank ist mit dem Master-Passwort verschlüsselt (ChaCha20-Poly1305)."));
    statusBar()->addPermanentWidget(m_statusLock);
    m_statusDb->setText(tr("Datenbank: %1").arg(QDir::toNativeSeparators(Database::instance().path())));

    connect(&Database::instance(), &Database::changed, this, [this] {
        // gebündelt aktualisieren
        static QTimer *t = nullptr;
        if (!t) {
            t = new QTimer(this);
            t->setSingleShot(true);
            t->setInterval(50);
            connect(t, &QTimer::timeout, this, &MainWindow::refreshAll);
        }
        t->start();
    });
    connect(&Database::instance(), &Database::usersChanged, this, &MainWindow::reloadUsers);
    connect(m_events, &EventsPage::startRequested, this, [this](const QString &m, const QString &e) { startExercise(m, e); });
    connect(m_alarms, &AlarmPage::testAlarm, m_reminders, &ReminderService::ringAlarm);
    connect(m_alarms, &AlarmPage::sleepExerciseRequested, this, [this](const QString &module) {
        startExercise(module, tr("Einschlafübung"));
        m_alarms->startSleepMusic();
    });
    connect(m_reminders, &ReminderService::startRequested, this,
            [this](qint64 userId, const QString &module, const QString &event) {
                showNormal();
                raise();
                activateWindow();
                selectUser(userId);
                startExercise(module, event);
            });
    connect(&DbSync::instance(), &DbSync::started, this, [this] { m_statusSync->setText(tr("Abgleich läuft …")); });
    connect(&DbSync::instance(), &DbSync::finished, this, [this](bool ok, const QString &msg) {
        m_statusSync->setText(ok ? tr("Abgeglichen %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm")))
                                 : tr("Abgleich fehlgeschlagen"));
        m_statusSync->setToolTip(msg);
    });

    setupTray();
    reloadUsers();
    m_reminders->start();
    // Kopfzeile minütlich aktualisieren (nächste Erinnerung, „heute“)
    auto *minute = new QTimer(this);
    connect(minute, &QTimer::timeout, this, &MainWindow::refreshHeader);
    minute->start(60000);

    QSettings s;
    restoreGeometry(s.value(QStringLiteral("ui/geometry")).toByteArray());
    m_tabs->setCurrentIndex(s.value(QStringLiteral("ui/tab"), 0).toInt());
    if (!s.contains(QStringLiteral("ui/geometry"))) resize(1100, 760);
}

// ---------------------------------------------------------------- Menüs und Werkzeugleiste

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&Datei"));
    file->addAction(Icons::get(QStringLiteral("sync")), tr("Datenbank &abgleichen"), this, [this] { runSync(true); })
        ->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+A")));
    file->addAction(Icons::get(QStringLiteral("database")), tr("Datenbank &sichern …"), this, &MainWindow::backup);
    file->addSeparator();
    QAction *quit = file->addAction(Icons::get(QStringLiteral("quit")), tr("&Beenden"), this, [this] {
        m_quitting = true;
        close();
    });
    quit->setShortcut(QKeySequence::Quit);

    QMenu *users = menuBar()->addMenu(tr("&Benutzer"));
    users->addAction(Icons::get(QStringLiteral("user-add")), tr("&Neuer Benutzer …"), this, [this] {
        if (const qint64 id = UserDialog::createUser(this)) selectUser(id);
    });
    users->addAction(Icons::get(QStringLiteral("users")), tr("Benutzer &verwalten …"), this, [this] {
        UserDialog(this).exec();
    });

    QMenu *ex = menuBar()->addMenu(tr("Üb&ungen"));
    int n = 1;
    QString category;
    for (const ModuleDef &m : BreathModules::all()) {
        if (m.category != category) {
            category = m.category;
            ex->addSection(category);
        }
        QAction *a = ex->addAction(Icons::get(m.icon), m.name + (m.quickHelp ? QString() : QStringLiteral(" …")), this,
                                   [this, id = m.id] { startExercise(id); });
        if (m.quickHelp) a->setShortcut(QKeySequence(Qt::Key_F2));
        else if (n <= 9) a->setShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(n++)));
    }

    QMenu *hist = menuBar()->addMenu(tr("&Verlauf"));
    hist->addAction(Icons::get(QStringLiteral("add")), tr("Übungseinheit &nachtragen …"), this, [this] {
        m_tabs->setCurrentIndex(TabHistory);
        m_history->addEntry();
    });
    hist->addAction(Icons::get(QStringLiteral("print")), tr("Verlauf &drucken …"), this, [this] { m_history->printReport(); })
        ->setShortcut(QKeySequence::Print);
    hist->addAction(Icons::get(QStringLiteral("pdf")), tr("Verlauf als &PDF speichern …"), this, [this] { m_history->savePdf(); });

    QMenu *rem = menuBar()->addMenu(tr("&Erinnerungen"));
    rem->addAction(Icons::get(QStringLiteral("event")), tr("Neues &Ereignis …"), this, [this] {
        m_tabs->setCurrentIndex(TabEvents);
        m_events->addEvent();
    });
    rem->addAction(Icons::get(QStringLiteral("alarm")), tr("Neuer &Wecker …"), this, [this] {
        m_tabs->setCurrentIndex(TabAlarm);
        m_alarms->addAlarm();
    });
    rem->addAction(Icons::get(QStringLiteral("moon")), tr("E&inschlafen starten"), this, [this] {
        m_tabs->setCurrentIndex(TabAlarm);
        m_alarms->startSleep();
    });

    QMenu *extras = menuBar()->addMenu(tr("E&xtras"));
    extras->addAction(Icons::get(QStringLiteral("settings")), tr("&Einstellungen …"), this, [this] {
        openSettings(SettingsDialog::Appearance);
    })->setShortcut(QKeySequence::Preferences);
    QMenu *themes = extras->addMenu(Icons::get(QStringLiteral("theme")), tr("&Theme"));
    connect(themes, &QMenu::aboutToShow, this, [this, themes] {
        themes->clear();
        for (const QString &id : Themes::available()) {
            QAction *a = themes->addAction(Themes::displayName(id), this, [this, id] {
                Themes::apply(id);
                refreshIcons();
            });
            a->setCheckable(true);
            a->setChecked(id == Themes::current());
        }
    });

    QMenu *help = menuBar()->addMenu(tr("&Hilfe"));
    help->addAction(Icons::get(QStringLiteral("info")), tr("Ü&ber VitaBreathe …"), this, &MainWindow::showAbout);
    help->addAction(tr("Über &Qt …"), qApp, &QApplication::aboutQt);
}

void MainWindow::buildToolBar()
{
    QToolBar *tb = addToolBar(tr("Werkzeugleiste"));
    tb->setObjectName(QStringLiteral("mainToolBar"));
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->setIconSize(QSize(24, 24));
    tb->addWidget(new QLabel(tr(" Benutzer: ")));
    m_userCombo = new QComboBox;
    m_userCombo->setMinimumContentsLength(16);
    m_userCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_userCombo->setToolTip(tr("Benutzer auswählen – Einstellungen, Tagebuch und Erinnerungen gelten je Benutzer"));
    tb->addWidget(m_userCombo);
    connect(m_userCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (i < 0) return;
        m_userId = m_userCombo->itemData(i).toLongLong();
        Database::instance().setDeviceSetting(QStringLiteral("ui/lastUser"), QString::number(m_userId));
        refreshAll();
    });
    tb->addAction(Icons::get(QStringLiteral("user-add")), tr("Neuer Benutzer"), this, [this] {
        if (const qint64 id = UserDialog::createUser(this)) selectUser(id);
    });
    tb->addSeparator();
    m_startButton = new QToolButton;
    m_startButton->setText(tr("Übung starten"));
    m_startButton->setIcon(Icons::get(QStringLiteral("play")));
    m_startButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_startButton->setPopupMode(QToolButton::MenuButtonPopup);
    auto *menu = new QMenu(m_startButton);
    QString cat;
    for (const ModuleDef &m : BreathModules::all()) {
        if (m.category != cat) { cat = m.category; menu->addSection(cat); }
        menu->addAction(Icons::get(m.icon), m.name, this, [this, id = m.id] { startExercise(id); });
    }
    m_startButton->setMenu(menu);
    connect(m_startButton, &QToolButton::clicked, this, [this] {
        startExercise(Database::instance().userSetting(m_userId, QStringLiteral("ui/lastModule"), QStringLiteral("box")));
    });
    tb->addWidget(m_startButton);
    for (const ModuleDef &m : BreathModules::all())
        if (m.quickHelp) {
            QAction *help = tb->addAction(Icons::get(m.icon), m.shortName, this, [this, id = m.id] { startExercise(id); });
            help->setToolTip(tr("%1 – sofort starten (F2)").arg(m.name));
        }
    tb->addAction(Icons::get(QStringLiteral("bell")), tr("Erinnerungen"), this, [this] { m_tabs->setCurrentIndex(TabEvents); });
    tb->addAction(Icons::get(QStringLiteral("alarm")), tr("Wecker"), this, [this] { m_tabs->setCurrentIndex(TabAlarm); });
    tb->addAction(Icons::get(QStringLiteral("moon")), tr("Einschlafen"), this, [this] {
        m_tabs->setCurrentIndex(TabAlarm);
        m_alarms->startSleep();
    });
    tb->addSeparator();
    tb->addAction(Icons::get(QStringLiteral("print")), tr("Verlauf drucken"), this, [this] { m_history->printReport(); });
    tb->addAction(Icons::get(QStringLiteral("sync")), tr("Abgleichen"), this, [this] { runSync(true); });
    tb->addAction(Icons::get(QStringLiteral("settings")), tr("Einstellungen"), this, [this] {
        openSettings(SettingsDialog::Appearance);
    });
}

// ---------------------------------------------------------------- Übungen

QWidget *MainWindow::buildExercisesPage()
{
    auto *page = new QWidget;
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    m_cardsHost = new QWidget;
    m_cardsLayout = new QVBoxLayout(m_cardsHost);
    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setWidget(m_cardsHost);
    outer->addWidget(sa);
    return page;
}

void MainWindow::refreshExerciseCards()
{
    while (QLayoutItem *it = m_cardsLayout->takeAt(0)) {
        delete it->widget();
        delete it;
    }
    if (!m_userId) return;
    const QList<SessionRecord> recent = Database::instance().sessions(m_userId, QDateTime::currentDateTime().addDays(-60));
    QString category;
    for (const ModuleDef &m : BreathModules::all()) {
        if (m.category != category) {
            category = m.category;
            auto *head = new QLabel(category);
            QFont hf = head->font();
            hf.setBold(true);
            hf.setPointSizeF(hf.pointSizeF() * 1.15);
            head->setFont(hf);
            head->setContentsMargins(2, m_cardsLayout->count() ? 10 : 0, 0, 0);
            m_cardsLayout->addWidget(head);
        }
        const ModuleSettings st = ModuleSettings::load(m_userId, m);
        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("vbCard"));
        card->setFrameShape(QFrame::StyledPanel);
        card->setStyleSheet(QStringLiteral("#vbCard { border: 1px solid rgba(20,116,188,0.45); border-radius: 8px;"
                                           " background-color: rgba(20,116,188,0.06); }"));
        auto *cl = new QHBoxLayout(card);
        auto *icon = new QLabel;
        icon->setPixmap(Icons::get(m.icon).pixmap(56, 56));
        cl->addWidget(icon, 0, Qt::AlignTop);
        auto *text = new QVBoxLayout;
        auto *name = new QLabel(m.name);
        QFont f = name->font();
        f.setBold(true);
        f.setPointSizeF(f.pointSizeF() * 1.25);
        name->setFont(f);
        text->addWidget(name);
        auto *desc = new QLabel(m.description);
        desc->setWordWrap(true);
        text->addWidget(desc);
        QString last;
        for (const SessionRecord &s : recent)
            if (s.moduleId == m.id) { last = Schedule::relative(s.started); break; }
        QStringList parts;
        if (m.hasSegments()) {
            QStringList segs;
            const QList<SegmentDef> sd = m.effectiveSegments();
            for (int g = 0; g < sd.size(); ++g)
                segs << (st.repeats.value(g, 1) > 1 ? QStringLiteral("%1 × %2").arg(st.repeats.value(g)).arg(sd.at(g).title)
                                                     : sd.at(g).title);
            parts << segs.join(QStringLiteral(" → "));
        } else {
            QStringList phases;
            for (int i = 0; i < m.phases.size(); ++i)
                if (st.phaseSec.value(i) > 0)
                    phases << QStringLiteral("%1 %2 s").arg(phaseName(m.phases.at(i).kind), formatSeconds(st.phaseSec.value(i)));
            parts << phases.join(QStringLiteral(" → "));
        }
        parts << tr("%1 min").arg(st.totalSec / 60);
        if (st.musicEnabled && !st.musicFile.isEmpty()) parts << tr("mit Musik");
        if (st.trainingMode) parts << tr("Trainingsmodus");
        auto *mine = new QLabel(tr("<b>Ihre Einstellung:</b> %1").arg(parts.join(QStringLiteral(" · ")).toHtmlEscaped())
                                + (last.isEmpty() ? QString() : tr("<br/><b>Zuletzt geübt:</b> %1").arg(last)));
        mine->setWordWrap(true);
        mine->setTextFormat(Qt::RichText);
        text->addWidget(mine);
        cl->addLayout(text, 1);
        auto *btn = new QPushButton(Icons::get(QStringLiteral("play")), m.quickHelp ? tr("Sofort starten") : tr("Üben …"));
        btn->setMinimumHeight(40);
        btn->setToolTip(m.quickHelp ? tr("Startet ohne Vorbereitung") : tr("Einstellungen prüfen und Übung starten"));
        if (m.quickHelp) {
            card->setStyleSheet(QStringLiteral("#vbCard { border: 1px solid rgba(224,98,13,0.7); border-radius: 8px;"
                                               " background-color: rgba(224,98,13,0.08); }"));
            QFont bf = btn->font();
            bf.setBold(true);
            btn->setFont(bf);
        }
        connect(btn, &QPushButton::clicked, this, [this, id = m.id] { startExercise(id); });
        cl->addWidget(btn, 0, Qt::AlignVCenter);
        m_cardsLayout->addWidget(card);
    }
    auto *more = new QLabel(tr("Weitere Übungsmodule folgen in künftigen Versionen. VitaBreathe ist ein persönliches "
                               "Übungs-Tagebuch und kein Medizinprodukt – bei Beschwerden fragen Sie bitte Ihre Ärztin oder Ihren Arzt."));
    more->setWordWrap(true);
    more->setEnabled(false);
    m_cardsLayout->addWidget(more);
    m_cardsLayout->addStretch();
}

void MainWindow::startExercise(const QString &moduleId, const QString &eventName)
{
    if (!m_userId) {
        QMessageBox::information(this, windowTitle(), tr("Bitte zuerst einen Benutzer anlegen."));
        return;
    }
    SessionDialog dlg(m_userId, moduleId, this, eventName);
    dlg.exec();
    refreshAll();
}

// ---------------------------------------------------------------- Benutzer

void MainWindow::reloadUsers()
{
    const qint64 want = m_userId ? m_userId : Database::instance().deviceSetting(QStringLiteral("ui/lastUser")).toLongLong();
    QSignalBlocker b(m_userCombo);
    m_userCombo->clear();
    for (const User &u : Database::instance().users()) m_userCombo->addItem(Icons::get(QStringLiteral("user")), u.name, u.id);
    int idx = m_userCombo->findData(want);
    if (idx < 0 && m_userCombo->count() > 0) idx = 0;
    m_userCombo->setCurrentIndex(idx);
    m_userId = idx >= 0 ? m_userCombo->itemData(idx).toLongLong() : 0;
    refreshAll();
}

void MainWindow::selectUser(qint64 id)
{
    const int idx = m_userCombo->findData(id);
    if (idx < 0) reloadUsers();
    m_userCombo->setCurrentIndex(qMax(0, m_userCombo->findData(id)));
}

void MainWindow::refreshAll()
{
    refreshHeader();
    refreshExerciseCards();
    m_history->setUser(m_userId);
    m_events->setUser(m_userId);
    m_alarms->setUser(m_userId);
}

void MainWindow::refreshHeader()
{
    const auto u = Database::instance().user(m_userId);
    m_headName->setText(u ? u->name : tr("Kein Benutzer"));
    setWindowTitle(u ? tr("VitaBreathe – %1").arg(u->name) : QStringLiteral("VitaBreathe"));
    if (!u) {
        m_headInfo->setText(tr("Legen Sie über <i>Benutzer → Neuer Benutzer</i> eine Person an."));
        return;
    }
    const QList<SessionRecord> all = Database::instance().sessions(m_userId, QDateTime::currentDateTime().addDays(-400));
    int todayCount = 0, todaySec = 0, weekSec = 0;
    QSet<QDate> days;
    const QDate today = QDate::currentDate();
    for (const SessionRecord &s : all) {
        days.insert(s.started.date());
        if (s.started.date() == today) { ++todayCount; todaySec += s.actualSec; }
        if (s.started.date() > today.addDays(-7)) weekSec += s.actualSec;
    }
    int streak = 0;
    QDate d = days.contains(today) ? today : today.addDays(-1);
    while (days.contains(d)) { ++streak; d = d.addDays(-1); }
    QStringList info;
    info << tr("<b>Heute:</b> %n Übung(en)", nullptr, todayCount) + tr(", %1 min").arg(todaySec / 60);
    info << tr("<b>Letzte 7 Tage:</b> %1 min").arg(weekSec / 60);
    if (streak > 1) info << tr("<b>%1 Tage in Folge</b> geübt").arg(streak);
    const QString next = m_reminders->nextText(m_userId);
    if (!next.isEmpty()) info << tr("<b>Nächste Erinnerung:</b> %1").arg(next.toHtmlEscaped());
    if (m_alarms && m_alarms->sleepRunning()) info << tr("<span style='color:#8e5bc7'><b>Einschlafmusik läuft</b></span>");
    m_headInfo->setText(info.join(QStringLiteral(" &nbsp;·&nbsp; ")));
}

void MainWindow::refreshIcons()
{
    Icons::refreshWidgets();
    refreshAll();
}

// ---------------------------------------------------------------- Infobereich, Schließen

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    m_tray = new QSystemTrayIcon(Icons::app(), this);
    m_tray->setToolTip(QStringLiteral("VitaBreathe"));
    auto *menu = new QMenu(this);
    menu->addAction(tr("VitaBreathe anzeigen"), this, [this] {
        showNormal();
        raise();
        activateWindow();
    });
    for (const ModuleDef &m : BreathModules::all())
        menu->addAction(Icons::get(m.icon), m.name, this, [this, id = m.id] {
            showNormal();
            startExercise(id);
        });
    menu->addAction(Icons::get(QStringLiteral("moon")), tr("Einschlafen starten"), this, [this] { m_alarms->startSleep(); });
    menu->addSeparator();
    menu->addAction(Icons::get(QStringLiteral("quit")), tr("Beenden"), this, [this] {
        m_quitting = true;
        close();
    });
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) {
            showNormal();
            raise();
            activateWindow();
        }
    });
    connect(m_reminders, &ReminderService::trayMessage, this, [this](const QString &t, const QString &m) {
        m_tray->showMessage(t, m, Icons::app(), 10000);
    });
    m_tray->show();
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    if (e->type() == QEvent::PaletteChange || e->type() == QEvent::StyleChange) {
        // Theme gewechselt (auch Systemdunkelmodus)
    }
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    QSettings s;
    s.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    s.setValue(QStringLiteral("ui/tab"), m_tabs->currentIndex());
    const bool tray = m_tray && m_tray->isVisible() && s.value(QStringLiteral("ui/tray"), true).toBool();
    if (!m_quitting && tray) {
        hide();
        if (!m_trayHintShown) {
            m_trayHintShown = true;
            m_tray->showMessage(QStringLiteral("VitaBreathe"),
                                tr("VitaBreathe läuft im Infobereich weiter, damit Erinnerungen und Wecker funktionieren. "
                                   "Beenden über das Symbol oder Datei → Beenden."),
                                Icons::app(), 8000);
        }
        e->ignore();
        return;
    }
    m_reminders->stop();
    m_alarms->stopSleep();
    const DbSync::Config c = DbSync::config();
    if (c.enabled && c.autoSync && DbSync::isReady(c) && DbSync::instance().status().pending) {
        statusBar()->showMessage(tr("Abgleich vor dem Beenden …"));
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString msg;
        DbSync::instance().syncBlocking(&msg);
        QApplication::restoreOverrideCursor();
    }
    if (m_tray) m_tray->hide();
    e->accept();
    qApp->quit();
}

// ---------------------------------------------------------------- Sonstiges

void MainWindow::openSettings(int page)
{
    SettingsDialog dlg(this, page);
    connect(&dlg, &SettingsDialog::themeChanged, this, &MainWindow::refreshIcons);
    bool languageChanged = false;
    connect(&dlg, &SettingsDialog::languageChanged, this, [&languageChanged] { languageChanged = true; });
    dlg.exec();
    if (!languageChanged) return;
    if (QMessageBox::question(this, tr("Sprache / Language"),
                              tr("Die Sprache wird nach einem Neustart von VitaBreathe wirksam. Jetzt neu starten?\n\n"
                                 "The language takes effect after restarting VitaBreathe. Restart now?"))
        != QMessageBox::Yes)
        return;
    m_quitting = true;
    close();
    I18n::restartApplication();
}

void MainWindow::backup()
{
    const QString def = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                        + QStringLiteral("/VitaBreathe-Sicherung_%1.sqlite").arg(QDate::currentDate().toString(Qt::ISODate));
    const QString f = QFileDialog::getSaveFileName(this, tr("Datenbank sichern"), def,
                                                   tr("VitaBreathe-Datenbank (*.sqlite)"));
    if (f.isEmpty()) return;
    QString err;
    if (Database::instance().backupTo(f, &err))
        QMessageBox::information(this, tr("Sicherung"), tr("Die Sicherung wurde gespeichert. Sie ist mit demselben "
                                                           "Master-Passwort verschlüsselt."));
    else
        QMessageBox::warning(this, tr("Sicherung"), tr("Sicherung fehlgeschlagen: %1").arg(err));
}

void MainWindow::runSync(bool interactive)
{
    QString why;
    if (!DbSync::isReady(DbSync::config(), &why)) {
        if (interactive
            && QMessageBox::question(this, tr("Abgleich"), tr("%1\n\nJetzt einrichten?").arg(why)) == QMessageBox::Yes) {
            openSettings(SettingsDialog::Sync);
        }
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString msg;
    const bool ok = DbSync::instance().syncBlocking(&msg);
    QApplication::restoreOverrideCursor();
    if (interactive) {
        if (ok) statusBar()->showMessage(msg, 8000);
        else QMessageBox::warning(this, tr("Abgleich"), msg);
    }
    reloadUsers();
}

void MainWindow::showAbout()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("Über VitaBreathe"));
    box.setIconPixmap(QIcon(QStringLiteral(":/icons/vitabreathe.png")).pixmap(96, 96));
    box.setTextFormat(Qt::RichText);
    box.setText(tr("<h3>VitaBreathe %1</h3>"
                   "<p>Geführte Atemübungen mit audiovisueller Unterstützung und Übungs-Tagebuch.</p>"
                   "<p><b>Kein Medizinprodukt.</b> VitaBreathe ist ein persönliches Tagebuch. Es stellt keine Diagnosen "
                   "und ersetzt keine ärztliche Beratung. Die Verwendung erfolgt auf eigene Gefahr.</p>"
                   "<p>© 2026 Michael Bergmann – MIT-Lizenz<br/>"
                   "Datenbank: SQLite3 Multiple Ciphers (MIT, Ulrich Telle)<br/>"
                   "Qt %2</p>").arg(vbVersion(), QString::fromLatin1(qVersion())));
    box.exec();
}
