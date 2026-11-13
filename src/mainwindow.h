// ViviaBreathe – Hauptfenster (Aufbau wie VitaParms: Menü, Werkzeugleiste mit Benutzerauswahl,
// Kopfbereich zur gewählten Person, Reiter, Statuszeile)
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>

class QComboBox;
class QLabel;
class QTabWidget;
class QToolButton;
class QVBoxLayout;
class HistoryPage;
class EventsPage;
class AlarmPage;
class ReminderService;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    void showAbout();
    explicit MainWindow(QWidget *parent = nullptr);
    qint64 currentUser() const { return m_userId; }
    void selectUser(qint64 id);
    void startExercise(const QString &moduleId, const QString &eventName = {});
    ReminderService *reminders() const { return m_reminders; }
    QTabWidget *tabs() const { return m_tabs; }
    void setQuitting() { m_quitting = true; }

protected:
    void closeEvent(QCloseEvent *e) override;
    void changeEvent(QEvent *e) override;

private:
    void buildMenus();
    void buildToolBar();
    QWidget *buildExercisesPage();
    void reloadUsers();
    void refreshAll();
    void refreshHeader();
    void refreshExerciseCards();
    void refreshIcons();
    void setupTray();
    void backup();
    void openSettings(int page);
    void runSync(bool interactive);

    qint64           m_userId = 0;
    QComboBox       *m_userCombo = nullptr;
    QLabel          *m_headName = nullptr, *m_headInfo = nullptr;
    QTabWidget      *m_tabs = nullptr;
    QVBoxLayout     *m_cardsLayout = nullptr;
    QWidget         *m_cardsHost = nullptr;
    HistoryPage     *m_history = nullptr;
    EventsPage      *m_events = nullptr;
    AlarmPage       *m_alarms = nullptr;
    ReminderService *m_reminders = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QLabel          *m_statusDb = nullptr, *m_statusSync = nullptr, *m_statusLock = nullptr;
    QToolButton     *m_startButton = nullptr;
    bool             m_quitting = false;
    bool             m_trayHintShown = false;
};
