// VitaBreathe – Audio: Signaltöne und MP3-Wiedergabe (Qt Multimedia, Qt 5.15 und Qt 6)
//
// Ohne Qt Multimedia übersetzt VitaBreathe trotzdem: Signaltöne werden dann zum
// Systempiepton, Musik entfällt (MusicPlayer::available() == false).
// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QStringList>

class QTimer;

namespace Tones {
enum class Kind { Phase, Inhale, Hold, Exhale, Done, Reminder, Alarm };
void play(Kind k, int volume = 80);      // 0–100
void preload();
void vibrate(int ms = 120);              // Android: Vibrationsimpuls, sonst ohne Wirkung
bool vibrationAvailable();
QString builtinAlarmFile();              // eingebauter Weckton als Datei (für die MP3-Wiedergabe)
}

class MusicPlayer : public QObject
{
    Q_OBJECT
public:
    explicit MusicPlayer(QObject *parent = nullptr);
    ~MusicPlayer() override;
    static bool available();

    void setPlaylist(const QStringList &files, bool shuffle = false);
    void setLoop(bool loop) { m_loop = loop; }       // Liste endlos wiederholen
    void play();
    void pause();
    void resume();
    void stop();
    bool isPlaying() const { return m_playing; }
    QString currentFile() const;

    void setVolume(int v);                            // 0–100 (sofort)
    int  volume() const { return m_volume; }
    // Lautstärke über 'ms' auf 'target' ändern; stopAtEnd: danach anhalten
    void fadeTo(int target, int ms, bool stopAtEnd = false);

signals:
    void trackChanged(const QString &file);
    void errorOccurred(const QString &message);
    void stopped();

private:
    void playIndex(int i);
    void next();
    void applyVolume();

    struct Impl;
    Impl       *d = nullptr;
    QStringList m_list;
    int         m_index = -1;
    bool        m_loop = true;
    bool        m_playing = false;
    int         m_volume = 50;
    // Überblenden
    QTimer     *m_fade = nullptr;
    int         m_fadeFrom = 0, m_fadeTo = 0, m_fadeMs = 0;
    qint64      m_fadeStart = 0;
    bool        m_fadeStop = false;
    int         m_errors = 0;
};
