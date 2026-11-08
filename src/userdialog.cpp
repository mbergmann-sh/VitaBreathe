// SPDX-License-Identifier: MIT
#include "userdialog.h"
#include "database.h"
#include "icons.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

UserDialog::UserDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Benutzer verwalten"));
    auto *lay = new QVBoxLayout(this);
    auto *info = new QLabel(tr("Jeder Benutzer hat eigene Einstellungen, ein eigenes Tagebuch, eigene Erinnerungen und Wecker."));
    info->setWordWrap(true);
    lay->addWidget(info);
    auto *row = new QHBoxLayout;
    m_list = new QListWidget;
    row->addWidget(m_list, 1);
    auto *col = new QVBoxLayout;
    auto *add = new QPushButton(Icons::get(QStringLiteral("user-add")), tr("Neu …"));
    m_rename = new QPushButton(Icons::get(QStringLiteral("edit")), tr("Umbenennen …"));
    m_delete = new QPushButton(Icons::get(QStringLiteral("delete")), tr("Löschen …"));
    col->addWidget(add);
    col->addWidget(m_rename);
    col->addWidget(m_delete);
    col->addStretch();
    row->addLayout(col);
    lay->addLayout(row);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);

    connect(add, &QPushButton::clicked, this, [this] {
        if (createUser(this)) reload();
    });
    connect(m_rename, &QPushButton::clicked, this, [this] {
        const qint64 id = selected();
        const auto u = Database::instance().user(id);
        if (!u) return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Umbenennen"), tr("Name:"), QLineEdit::Normal, u->name, &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        Database::instance().renameUser(id, name);
        reload();
    });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        const qint64 id = selected();
        const auto u = Database::instance().user(id);
        if (!u) return;
        if (Database::instance().users().size() <= 1) {
            QMessageBox::information(this, tr("Löschen"), tr("Der letzte Benutzer kann nicht gelöscht werden."));
            return;
        }
        bool ok = false;
        const QString confirm = QInputDialog::getText(
            this, tr("Benutzer löschen"),
            tr("„%1“ mit allen Übungseinheiten, Einstellungen, Erinnerungen und Weckern endgültig löschen?\n\n"
               "Zur Bestätigung bitte den Namen eintippen:").arg(u->name), QLineEdit::Normal, QString(), &ok);
        if (!ok) return;
        if (confirm.trimmed() != u->name) {
            QMessageBox::information(this, tr("Löschen"), tr("Der Name stimmt nicht überein – nichts gelöscht."));
            return;
        }
        Database::instance().deleteUser(id);
        reload();
    });
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int r) {
        m_rename->setEnabled(r >= 0);
        m_delete->setEnabled(r >= 0);
    });
    reload();
    resize(420, 320);
}

void UserDialog::reload()
{
    m_list->clear();
    for (const User &u : Database::instance().users()) {
        auto *it = new QListWidgetItem(Icons::get(QStringLiteral("user")), u.name, m_list);
        it->setData(Qt::UserRole, u.id);
    }
    m_rename->setEnabled(false);
    m_delete->setEnabled(false);
}

qint64 UserDialog::selected() const
{
    return m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toLongLong() : 0;
}

qint64 UserDialog::createUser(QWidget *parent)
{
    bool ok = false;
    const QString name = QInputDialog::getText(parent, tr("Neuer Benutzer"), tr("Name des neuen Benutzers:"),
                                               QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) return 0;
    for (const User &u : Database::instance().users())
        if (u.name.compare(name, Qt::CaseInsensitive) == 0) {
            QMessageBox::information(parent, tr("Neuer Benutzer"), tr("Einen Benutzer „%1“ gibt es bereits.").arg(name));
            return 0;
        }
    QString err;
    const qint64 id = Database::instance().addUser(name, &err);
    if (!id) QMessageBox::warning(parent, tr("Neuer Benutzer"), err);
    return id;
}
