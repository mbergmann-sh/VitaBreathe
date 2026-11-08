// VitaBreathe – MP3-Wiedergabeliste bearbeiten (Wecker, Einschlafen) und MP3-Dateiauswahl
// SPDX-License-Identifier: MIT
#pragma once

#include <QWidget>

class QListWidget;
class QLineEdit;
class QToolButton;
class MusicPlayer;

class PlaylistWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PlaylistWidget(QWidget *parent = nullptr);
    void setFiles(const QStringList &files);
    QStringList files() const;
    void setPreviewVolume(int v) { m_previewVolume = v; }

signals:
    void changed();

private:
    void refreshButtons();
    QListWidget *m_list;
    QToolButton *m_add, *m_remove, *m_up, *m_down, *m_preview;
    MusicPlayer *m_player = nullptr;
    int          m_previewVolume = 50;
};

// Eine einzelne MP3-Datei wählen (Hintergrundmusik einer Übung), mit Probehören
class MusicFileEdit : public QWidget
{
    Q_OBJECT
public:
    explicit MusicFileEdit(QWidget *parent = nullptr);
    void setFile(const QString &f);
    QString file() const;
    void setPreviewVolume(int v);

signals:
    void changed();

private:
    QLineEdit   *m_edit;
    QToolButton *m_preview;
    MusicPlayer *m_player = nullptr;
    int          m_volume = 50;
};

QString chooseMusicDir();
void rememberMusicDir(const QString &file);
QString musicFileFilter();
