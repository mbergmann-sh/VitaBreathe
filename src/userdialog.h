// ViviaBreathe – Benutzer verwalten (anlegen, umbenennen, löschen)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include <QDialog>

class QListWidget;
class QPushButton;

class UserDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UserDialog(QWidget *parent = nullptr);
    static qint64 createUser(QWidget *parent);   // fragt den Namen ab; 0 = abgebrochen

private:
    void reload();
    qint64 selected() const;

    QListWidget *m_list;
    QPushButton *m_rename, *m_delete;
};
