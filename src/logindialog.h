// ViviaBreathe – Master-Passwort beim Start (Anmeldung bzw. Ersteinrichtung)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QCheckBox;
class QStackedWidget;
class QPushButton;

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(const QString &dbPath, QWidget *parent = nullptr);
    QString password() const;
    QString firstUserName() const;
    bool isNewDatabase() const { return m_create; }
    QString databasePath() const { return m_path; }
    void setError(const QString &e);

    static QString passwordProblem(const QString &pw);   // leer = in Ordnung

private:
    void setMode(bool create);
    void downloadFromServer();
    void chooseDatabase();
    void tryAccept();

    QString         m_path;
    bool            m_create = false;
    QStackedWidget *m_stack;
    QLabel         *m_pathLabel, *m_error, *m_strength;
    QLineEdit      *m_pw, *m_newPw, *m_newPw2, *m_userName;
    QCheckBox      *m_understood;
    QPushButton    *m_ok;
};
