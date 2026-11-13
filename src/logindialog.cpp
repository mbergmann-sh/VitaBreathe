// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "logindialog.h"
#include "database.h"
#include "dbsync.h"
#include "i18n.h"
#include "icons.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

QString LoginDialog::passwordProblem(const QString &pw)
{
    if (pw.size() < 8) return tr("Das Master-Passwort muss mindestens 8 Zeichen lang sein.");
    int classes = 0;
    bool lower = false, upper = false, digit = false, other = false;
    for (const QChar c : pw) {
        if (c.isLower()) lower = true;
        else if (c.isUpper()) upper = true;
        else if (c.isDigit()) digit = true;
        else other = true;
    }
    classes = int(lower) + int(upper) + int(digit) + int(other);
    if (classes < 2 && pw.size() < 14)
        return tr("Bitte Buchstaben mit Ziffern oder Sonderzeichen mischen – oder einen langen Satz (ab 14 Zeichen) verwenden.");
    return {};
}

LoginDialog::LoginDialog(const QString &dbPath, QWidget *parent) : QDialog(parent), m_path(dbPath)
{
    setWindowTitle(tr("ViviaBreathe"));
    setWindowIcon(Icons::app());
    auto *lay = new QVBoxLayout(this);

    auto *head = new QHBoxLayout;
    auto *logo = new QLabel;
    logo->setPixmap(QIcon(QStringLiteral(":/icons/viviabreathe.png")).pixmap(72, 72));
    head->addWidget(logo, 0, Qt::AlignTop);
    auto *title = new QLabel(tr("<p style='font-size:x-large'><b>ViviaBreathe</b></p>"
                                "<p>Geführte Atemübungen mit Tagebuch – Version %1</p>").arg(vbVersion()));
    head->addWidget(title, 1);
    auto *lang = new QComboBox;
    lang->addItem(QStringLiteral("Deutsch"), QStringLiteral("de"));
    lang->addItem(QStringLiteral("English"), QStringLiteral("en"));
    lang->setCurrentIndex(qMax(0, lang->findData(I18n::language())));
    lang->setToolTip(QStringLiteral("Sprache / Language"));
    head->addWidget(lang, 0, Qt::AlignTop);
    connect(lang, qOverload<int>(&QComboBox::currentIndexChanged), this, [lang] {
        I18n::setLanguage(lang->currentData().toString());
        I18n::restartApplication();        // ViviaBreathe in der neuen Sprache neu starten
    });
    lay->addLayout(head);

    m_stack = new QStackedWidget;
    // --- Anmeldung
    auto *login = new QWidget;
    auto *ll = new QFormLayout(login);
    m_pw = new QLineEdit;
    m_pw->setEchoMode(QLineEdit::Password);
    m_pw->setPlaceholderText(tr("Master-Passwort"));
    ll->addRow(tr("Master-Passwort:"), m_pw);
    auto *showPw = new QCheckBox(tr("Passwort anzeigen"));
    connect(showPw, &QCheckBox::toggled, this, [this](bool on) { m_pw->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password); });
    ll->addRow(QString(), showPw);
    m_stack->addWidget(login);

    // --- Ersteinrichtung
    auto *create = new QWidget;
    auto *cl = new QVBoxLayout(create);
    cl->setContentsMargins(0, 0, 0, 0);
    auto *welcome = new QLabel(tr("<b>Willkommen!</b> Ihre Daten werden in einer verschlüsselten Datenbank gespeichert. "
                                  "Legen Sie dafür ein Master-Passwort fest – es wird bei jedem Start abgefragt und gilt für "
                                  "alle Benutzer dieses Geräts (und für alle Geräte, die Sie abgleichen)."));
    welcome->setWordWrap(true);
    cl->addWidget(welcome);
    auto *cf = new QFormLayout;
    m_newPw = new QLineEdit;
    m_newPw->setEchoMode(QLineEdit::Password);
    m_newPw2 = new QLineEdit;
    m_newPw2->setEchoMode(QLineEdit::Password);
    cf->addRow(tr("Master-Passwort:"), m_newPw);
    cf->addRow(tr("Wiederholen:"), m_newPw2);
    m_strength = new QLabel;
    m_strength->setWordWrap(true);
    cf->addRow(QString(), m_strength);
    m_userName = new QLineEdit;
    m_userName->setPlaceholderText(tr("z. B. Ihr Vorname"));
    cf->addRow(tr("Erster Benutzer:"), m_userName);
    cl->addLayout(cf);
    m_understood = new QCheckBox(tr("Mir ist bewusst: Ein vergessenes Master-Passwort kann niemand wiederherstellen – "
                                    "auch nicht der Hersteller."));
    m_understood->setStyleSheet(QStringLiteral("QCheckBox { font-weight: bold; }"));
    cl->addWidget(m_understood);
    connect(m_newPw, &QLineEdit::textChanged, this, [this](const QString &t) {
        const QString p = passwordProblem(t);
        m_strength->setText(t.isEmpty() ? QString() : p.isEmpty() ? tr("✓ Passwort in Ordnung") : p);
        m_strength->setStyleSheet(p.isEmpty() ? QStringLiteral("color:#2e8b57") : QStringLiteral("color:#c0392b"));
    });
    m_stack->addWidget(create);
    lay->addWidget(m_stack);

    m_error = new QLabel;
    m_error->setStyleSheet(QStringLiteral("color:#c0392b; font-weight:bold"));
    m_error->setWordWrap(true);
    m_error->hide();
    lay->addWidget(m_error);

    m_pathLabel = new QLabel;
    m_pathLabel->setEnabled(false);
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(m_pathLabel);

    auto *notice = new QLabel(tr("ViviaBreathe ist ein persönliches Übungs-Tagebuch und <b>kein Medizinprodukt</b>."));
    notice->setWordWrap(true);
    lay->addWidget(notice);

    auto *bb = new QDialogButtonBox;
    m_ok = bb->addButton(tr("Öffnen"), QDialogButtonBox::AcceptRole);
    m_ok->setDefault(true);
    auto *server = bb->addButton(tr("Vom Server laden …"), QDialogButtonBox::ActionRole);
    server->setIcon(Icons::get(QStringLiteral("sync")));
    server->setToolTip(tr("Weiteres Gerät einrichten: die Datenbank von Ihrem Abgleich-Server übernehmen"));
    auto *other = bb->addButton(tr("Andere Datenbank …"), QDialogButtonBox::ActionRole);
    other->setIcon(Icons::get(QStringLiteral("database")));
    bb->addButton(QDialogButtonBox::Cancel);
    connect(m_ok, &QPushButton::clicked, this, &LoginDialog::tryAccept);
    connect(server, &QPushButton::clicked, this, &LoginDialog::downloadFromServer);
    connect(other, &QPushButton::clicked, this, &LoginDialog::chooseDatabase);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);

    setMode(!Database::fileHasContent(m_path));
    resize(560, sizeHint().height());
}

void LoginDialog::setMode(bool create)
{
    m_create = create;
    m_stack->setCurrentIndex(create ? 1 : 0);
    m_ok->setText(create ? tr("Datenbank anlegen") : tr("Öffnen"));
    m_pathLabel->setText(tr("Datenbank: %1").arg(QDir::toNativeSeparators(m_path)));
    if (create) m_newPw->setFocus();
    else m_pw->setFocus();
    m_error->hide();
}

void LoginDialog::setError(const QString &e)
{
    m_error->setText(e);
    m_error->setVisible(!e.isEmpty());
    m_pw->selectAll();
    m_pw->setFocus();
}

QString LoginDialog::password() const
{
    return m_create ? m_newPw->text() : m_pw->text();
}

QString LoginDialog::firstUserName() const
{
    return m_userName->text().trimmed();
}

void LoginDialog::tryAccept()
{
    if (m_create) {
        if (const QString p = passwordProblem(m_newPw->text()); !p.isEmpty()) { setError(p); return; }
        if (m_newPw->text() != m_newPw2->text()) { setError(tr("Die beiden Passwörter stimmen nicht überein.")); return; }
        if (firstUserName().isEmpty()) { setError(tr("Bitte einen Namen für den ersten Benutzer eingeben.")); return; }
        if (!m_understood->isChecked()) { setError(tr("Bitte bestätigen Sie den Hinweis zum Master-Passwort.")); return; }
    } else if (m_pw->text().isEmpty()) {
        setError(tr("Bitte das Master-Passwort eingeben."));
        return;
    }
    accept();
}

void LoginDialog::chooseDatabase()
{
    QFileDialog dlg(this, tr("Datenbank öffnen oder neu anlegen"), QFileInfo(m_path).absolutePath(),
                    tr("ViviaBreathe-Datenbank (*.sqlite *.vbdb);;Alle Dateien (*)"));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setOption(QFileDialog::DontConfirmOverwrite);
    dlg.setDefaultSuffix(QStringLiteral("sqlite"));
    if (dlg.exec() != QDialog::Accepted || dlg.selectedFiles().isEmpty()) return;
    m_path = dlg.selectedFiles().first();
    QSettings().setValue(QStringLiteral("db/path"), m_path);
    setMode(!Database::fileHasContent(m_path));
}

void LoginDialog::downloadFromServer()
{
    QDialog d(this);
    d.setObjectName(QStringLiteral("ServerDownloadDialog"));
    d.setWindowTitle(tr("Datenbank vom Server laden"));
    auto *lay = new QVBoxLayout(&d);
    auto *info = new QLabel(tr("Geben Sie die Adresse Ihres Abgleich-Servers und den Zugangsschlüssel ein "
                               "(in ViviaBreathe auf dem anderen Gerät unter Extras → Einstellungen → Abgleich). "
                               "Danach melden Sie sich mit dem gemeinsamen Master-Passwort an."));
    info->setWordWrap(true);
    lay->addWidget(info);
    auto *form = new QFormLayout;
    auto *url = new QLineEdit;
    url->setPlaceholderText(QStringLiteral("https://atem.example.de/"));
    auto *key = new QLineEdit;
    form->addRow(tr("Adresse:"), url);
    form->addRow(tr("Zugangsschlüssel:"), key);
    lay->addLayout(form);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    lay->addWidget(bb);
    d.resize(520, d.sizeHint().height());
    if (d.exec() != QDialog::Accepted) return;
    if (Database::fileHasContent(m_path)
        && QMessageBox::question(this, windowTitle(), tr("Die vorhandene Datenbank auf diesem Gerät wird ersetzt "
                                                          "(eine Kopie bleibt als .bak erhalten). Fortfahren?"))
               != QMessageBox::Yes)
        return;
    if (Database::fileHasContent(m_path)) {
        QFile::remove(m_path + QStringLiteral(".bak"));
        QFile::copy(m_path, m_path + QStringLiteral(".bak"));
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString err;
    const bool ok = DbSync::downloadInitial(url->text(), key->text().trimmed(), m_path, &err);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, windowTitle(), tr("Laden fehlgeschlagen: %1").arg(err));
        return;
    }
    setMode(false);
    m_error->setStyleSheet(QStringLiteral("color:#2e8b57; font-weight:bold"));
    m_error->setText(tr("Datenbank übernommen – bitte mit dem Master-Passwort anmelden."));
    m_error->show();
}
