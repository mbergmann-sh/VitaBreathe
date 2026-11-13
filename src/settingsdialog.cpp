// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "settingsdialog.h"
#include "database.h"
#include "dbsync.h"
#include "i18n.h"
#include "icons.h"
#include "logindialog.h"
#include "themes.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QLocale>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent, int page) : QDialog(parent)
{
    setWindowTitle(tr("Einstellungen"));
    auto *lay = new QVBoxLayout(this);
    auto *tabs = new QTabWidget;
    tabs->addTab(appearancePage(), Icons::get(QStringLiteral("theme")), tr("Darstellung"));
    tabs->addTab(syncPage(), Icons::get(QStringLiteral("sync")), tr("Abgleich"));
    tabs->addTab(securityPage(), Icons::get(QStringLiteral("lock")), tr("Master-Passwort"));
    tabs->addTab(generalPage(), Icons::get(QStringLiteral("settings")), tr("Allgemein"));
    tabs->setCurrentIndex(page);
    lay->addWidget(tabs);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(bb, &QDialogButtonBox::rejected, this, [this] {
        saveSync();
        reject();
    });
    lay->addWidget(bb);
    resize(600, 460);
}

QWidget *SettingsDialog::appearancePage()
{
    auto *w = new QWidget;
    auto *form = new QFormLayout(w);
    m_language = new QComboBox;
    m_language->addItem(QStringLiteral("Deutsch"), QStringLiteral("de"));
    m_language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_language->setCurrentIndex(qMax(0, m_language->findData(I18n::language())));
    form->addRow(tr("Sprache / Language:"), m_language);
    connect(m_language, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        I18n::setLanguage(m_language->currentData().toString());
        emit languageChanged();
    });
    m_theme = new QComboBox;
    for (const QString &id : Themes::available()) m_theme->addItem(Themes::displayName(id), id);
    m_theme->setCurrentIndex(qMax(0, m_theme->findData(Themes::current())));
    form->addRow(tr("Theme:"), m_theme);
    auto *info = new QLabel(tr("Die Themes entsprechen denen von VitaParms: alle Qt-Styles der Plattform sowie "
                               "„Dark“ und „Visual Studio Code Dark“. Die Änderung wirkt sofort."));
    info->setWordWrap(true);
    info->setEnabled(false);
    form->addRow(QString(), info);
    connect(m_theme, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        Themes::apply(m_theme->currentData().toString());
        Icons::clearCache();
        emit themeChanged();
    });
    return w;
}

QWidget *SettingsDialog::syncPage()
{
    const DbSync::Config c = DbSync::config();
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);
    auto *info = new QLabel(tr("Gleichen Sie ViviaBreathe zwischen mehreren Geräten über Ihren eigenen Webserver ab "
                               "(PHP, Ordner <i>website/</i> aus dem Quelltext). Der Server erhält nur die "
                               "verschlüsselte Datenbank – alle Geräte verwenden dasselbe Master-Passwort."));
    info->setWordWrap(true);
    lay->addWidget(info);
    auto *form = new QFormLayout;
    m_syncOn = new QCheckBox(tr("Abgleich einschalten"));
    m_syncOn->setChecked(c.enabled);
    form->addRow(QString(), m_syncOn);
    m_syncAuto = new QCheckBox(tr("automatisch (beim Start, nach Änderungen, beim Beenden)"));
    m_syncAuto->setChecked(c.autoSync);
    form->addRow(QString(), m_syncAuto);
    m_url = new QLineEdit(c.url);
    m_url->setPlaceholderText(QStringLiteral("https://atem.example.de/"));
    form->addRow(tr("Server-Adresse:"), m_url);
    auto *keyRow = new QHBoxLayout;
    m_key = new QLineEdit(c.uploadKey);
    m_key->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    keyRow->addWidget(m_key, 1);
    auto *gen = new QPushButton(tr("Erzeugen"));
    gen->setToolTip(tr("Neuen zufälligen Zugangsschlüssel erzeugen (nur für einen neuen, leeren Server)"));
    keyRow->addWidget(gen);
    auto *show = new QPushButton(tr("Anzeigen"));
    keyRow->addWidget(show);
    form->addRow(tr("Zugangsschlüssel:"), keyRow);
    lay->addLayout(form);
    auto *btns = new QHBoxLayout;
    auto *test = new QPushButton(tr("Verbindung testen"));
    auto *now = new QPushButton(Icons::get(QStringLiteral("sync")), tr("Jetzt abgleichen"));
    btns->addWidget(test);
    btns->addWidget(now);
    btns->addStretch();
    lay->addLayout(btns);
    m_syncState = new QLabel;
    m_syncState->setWordWrap(true);
    lay->addWidget(m_syncState);
    lay->addStretch();
    QString tls;
    DbSync::tlsAvailable(&tls);
    auto *tlsLabel = new QLabel(tr("TLS: %1").arg(tls));
    tlsLabel->setEnabled(false);
    lay->addWidget(tlsLabel);

    auto refreshState = [this] {
        const DbSync::Status st = DbSync::instance().status();
        m_syncState->setText(st.last.isValid()
                                 ? tr("Letzter Abgleich: %1 – %2 (Revision %3)%4")
                                       .arg(QLocale().toString(st.last, QLocale::ShortFormat), st.lastResult)
                                       .arg(st.revision)
                                       .arg(st.pending ? tr(" · lokale Änderungen ausstehend") : QString())
                                 : tr("Noch nicht abgeglichen."));
    };
    refreshState();
    connect(gen, &QPushButton::clicked, this, [this] {
        if (!m_key->text().isEmpty()
            && QMessageBox::question(this, tr("Zugangsschlüssel"), tr("Den vorhandenen Schlüssel ersetzen? Ein bereits "
                                                                     "eingerichteter Server akzeptiert dann keine Uploads mehr."))
                   != QMessageBox::Yes)
            return;
        m_key->setText(DbSync::generateUploadKey());
        m_key->setEchoMode(QLineEdit::Normal);
    });
    connect(show, &QPushButton::clicked, this, [this] { m_key->setEchoMode(QLineEdit::Normal); });
    connect(test, &QPushButton::clicked, this, [this] {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString msg;
        DbSync::serverRevision(m_url->text(), m_key->text().trimmed(), &msg);
        QApplication::restoreOverrideCursor();
        m_syncState->setText(msg);
    });
    connect(now, &QPushButton::clicked, this, [this, refreshState] {
        saveSync();
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString msg;
        const bool ok = DbSync::instance().syncBlocking(&msg);
        QApplication::restoreOverrideCursor();
        refreshState();
        if (!ok) QMessageBox::warning(this, tr("Abgleich"), msg);
    });
    return w;
}

void SettingsDialog::saveSync()
{
    DbSync::Config c;
    c.enabled = m_syncOn->isChecked();
    c.autoSync = m_syncAuto->isChecked();
    c.url = m_url->text();
    c.uploadKey = m_key->text();
    DbSync::saveConfig(c);
}

QWidget *SettingsDialog::securityPage()
{
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);
    auto *info = new QLabel(tr("Das Master-Passwort verschlüsselt die gesamte Datenbank (ChaCha20-Poly1305). "
                               "Beim Ändern wird die Datenbank neu verschlüsselt. Ändern Sie es bei Abgleich auf allen "
                               "Geräten auf dasselbe neue Passwort – Geräte mit dem alten Passwort können sonst nicht "
                               "mehr abgleichen."));
    info->setWordWrap(true);
    lay->addWidget(info);
    auto *form = new QFormLayout;
    m_oldPw = new QLineEdit;
    m_newPw = new QLineEdit;
    m_newPw2 = new QLineEdit;
    for (QLineEdit *e : { m_oldPw, m_newPw, m_newPw2 }) e->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Bisheriges Passwort:"), m_oldPw);
    form->addRow(tr("Neues Passwort:"), m_newPw);
    form->addRow(tr("Wiederholen:"), m_newPw2);
    lay->addLayout(form);
    auto *change = new QPushButton(Icons::get(QStringLiteral("lock")), tr("Master-Passwort ändern"));
    lay->addWidget(change, 0, Qt::AlignLeft);
    lay->addStretch();
    connect(change, &QPushButton::clicked, this, [this] {
        Database &db = Database::instance();
        if (!db.verifyPassword(m_oldPw->text())) {
            QMessageBox::warning(this, tr("Master-Passwort"), tr("Das bisherige Passwort ist falsch."));
            return;
        }
        if (const QString p = LoginDialog::passwordProblem(m_newPw->text()); !p.isEmpty()) {
            QMessageBox::warning(this, tr("Master-Passwort"), p);
            return;
        }
        if (m_newPw->text() != m_newPw2->text()) {
            QMessageBox::warning(this, tr("Master-Passwort"), tr("Die beiden neuen Passwörter stimmen nicht überein."));
            return;
        }
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString err;
        const bool ok = db.changePassword(m_newPw->text(), &err);
        QApplication::restoreOverrideCursor();
        if (!ok) {
            QMessageBox::warning(this, tr("Master-Passwort"), tr("Ändern fehlgeschlagen: %1").arg(err));
            return;
        }
        for (QLineEdit *e : { m_oldPw, m_newPw, m_newPw2 }) e->clear();
        QMessageBox::information(this, tr("Master-Passwort"), tr("Das Master-Passwort wurde geändert."));
    });
    return w;
}

QWidget *SettingsDialog::generalPage()
{
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);
    QSettings s;
    m_tray = new QCheckBox(tr("Beim Schließen im Infobereich weiterlaufen (für Erinnerungen und Wecker)"));
    m_tray->setChecked(s.value(QStringLiteral("ui/tray"), true).toBool());
    lay->addWidget(m_tray);
    connect(m_tray, &QCheckBox::toggled, this, [](bool on) { QSettings().setValue(QStringLiteral("ui/tray"), on); });
    m_startTips = new QCheckBox(tr("Hinweis „kein Medizinprodukt“ beim Start anzeigen"));
    m_startTips->setChecked(s.value(QStringLiteral("ui/startNotice"), true).toBool());
    lay->addWidget(m_startTips);
    connect(m_startTips, &QCheckBox::toggled, this, [](bool on) { QSettings().setValue(QStringLiteral("ui/startNotice"), on); });
    lay->addStretch();
    auto *db = new QLabel(tr("Datenbank: %1").arg(QDir::toNativeSeparators(Database::instance().path())));
    db->setWordWrap(true);
    db->setTextInteractionFlags(Qt::TextSelectableByMouse);
    db->setEnabled(false);
    lay->addWidget(db);
    return w;
}
