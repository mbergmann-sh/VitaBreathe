// ViviaBreathe – Abgleich der Datenbank zwischen Geräten über einen eigenen Webserver
//
// Aufbau wie in VitaParms: Der Server (website/api/sync.php) verwahrt nur die verschlüsselte
// Datenbank; er zählt eine Revision hoch und lehnt veraltete Uploads ab (HTTP 409).
// Zusammengeführt wird auf dem Gerät – alle Geräte verwenden dasselbe Master-Passwort:
//
//   * jede abgeglichene Zeile trägt eine Kennung (uid) und den Zeitpunkt ihrer letzten Änderung
//   * gelöschte Zeilen hinterlassen einen Löschvermerk (sync_deleted)
//   * pro Zeile gewinnt die jüngere Änderung, ein jüngerer Löschvermerk gewinnt gegen ältere
//     Änderungen, neue Zeilen beider Seiten bleiben erhalten
//
// Gerätebezogen (nicht abgeglichen): Tabelle "device" (Abgleich-Einstellungen, letzter Benutzer).
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

namespace DbMerge {
struct Stats {
    int inserted = 0, updated = 0, deleted = 0;
    int total() const { return inserted + updated + deleted; }
};
bool mergeFrom(const QString &remotePath, const QString &password, Stats *stats, QString *error);
}

class QTimer;

class DbSync : public QObject
{
    Q_OBJECT
public:
    static DbSync &instance();

    struct Config {
        bool    enabled = false;
        bool    autoSync = true;      // nach Änderungen (verzögert), beim Start und beim Beenden
        QString url;                  // z. B. https://atem.example.de/
        QString uploadKey;            // berechtigt zum Hoch-/Herunterladen
    };
    static Config config();
    static void   saveConfig(const Config &c);
    static bool   isReady(const Config &c, QString *why = nullptr);
    static QString generateUploadKey();
    static QString normalizedUrl(const QString &url);
    static bool   tlsAvailable(QString *text = nullptr);

    struct Status {
        qint64    revision = 0;
        QDateTime last;
        QString   lastResult;
        bool      pending = false;
    };
    Status status();

    void sync();                                     // Ergebnis über finished()
    bool syncBlocking(QString *message);
    bool isBusy() const { return m_busy; }

    // Weiteres Gerät einrichten: Datenbank vom Server holen (vor der Anmeldung)
    static bool downloadInitial(const QString &url, const QString &uploadKey, const QString &dbPath, QString *error);
    // nach der ersten Anmeldung mit einer per downloadInitial() geholten Datenbank
    static void adoptInitialState(const QString &dbPath);
    static qint64 serverRevision(const QString &url, const QString &uploadKey, QString *message);

    void watchDatabase();

signals:
    void started();
    void finished(bool ok, const QString &message);

private:
    DbSync();
    bool runSync(QString *message);

    bool    m_busy = false;
    bool    m_watching = false;
    QTimer *m_debounce = nullptr;
};
