// VitaBreathe – Einstellungen: Darstellung (Themes), Abgleich, Master-Passwort, Allgemeines
// SPDX-License-Identifier: MIT
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr, int page = 0);
    enum Page { Appearance, Sync, Security, General };

signals:
    void themeChanged();
    void languageChanged();

private:
    QWidget *appearancePage();
    QWidget *syncPage();
    QWidget *securityPage();
    QWidget *generalPage();
    void saveSync();

    QComboBox *m_theme, *m_language;
    QCheckBox *m_syncOn, *m_syncAuto, *m_tray, *m_startTips;
    QLineEdit *m_url, *m_key;
    QLabel    *m_syncState;
    QLineEdit *m_oldPw, *m_newPw, *m_newPw2;
};
