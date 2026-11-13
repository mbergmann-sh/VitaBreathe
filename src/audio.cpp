// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "audio.h"

#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QUrl>

#ifdef VB_HAVE_MULTIMEDIA
#  include <QMediaPlayer>
#  include <QSoundEffect>
#  include <QAudio>
#  if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#    include <QAudioOutput>
#  endif
#endif

#ifdef Q_OS_ANDROID
#  if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#    include <QJniObject>
#    include <QCoreApplication>
#  endif
#endif

namespace {
// Lautstärke nach Gehör (logarithmisch) → linearer Faktor für die Wiedergabe
qreal linearVolume(int percent)
{
    const qreal v = qBound(0, percent, 100) / 100.0;
#ifdef VB_HAVE_MULTIMEDIA
    return QAudio::convertVolume(v, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
#else
    return v;
#endif
}
}

// ====================================================================== Signaltöne

namespace Tones {

#ifdef VB_HAVE_MULTIMEDIA
namespace {
QString file(Kind k)
{
    switch (k) {
    case Kind::Phase:    return QStringLiteral("phase.wav");
    case Kind::Inhale:   return QStringLiteral("inhale.wav");
    case Kind::Hold:     return QStringLiteral("hold.wav");
    case Kind::Exhale:   return QStringLiteral("exhale.wav");
    case Kind::Done:     return QStringLiteral("done.wav");
    case Kind::Reminder: return QStringLiteral("reminder.wav");
    case Kind::Alarm:    return QStringLiteral("alarm.wav");
    }
    return {};
}
QSoundEffect *effect(Kind k)
{
    static QHash<int, QSoundEffect *> cache;
    QSoundEffect *&e = cache[int(k)];
    if (!e) {
        e = new QSoundEffect(qApp);
        e->setSource(QUrl(QStringLiteral("qrc:/sounds/") + file(k)));
    }
    return e;
}
} // namespace
#endif

void preload()
{
#ifdef VB_HAVE_MULTIMEDIA
    for (Kind k : { Kind::Phase, Kind::Inhale, Kind::Hold, Kind::Exhale, Kind::Done, Kind::Reminder })
        effect(k);
#endif
}

void play(Kind k, int volume)
{
#ifdef VB_HAVE_MULTIMEDIA
    QSoundEffect *e = effect(k);
    if (e->status() == QSoundEffect::Error) {
        QApplication::beep();
        return;
    }
    e->setVolume(linearVolume(volume));
    e->stop();
    e->play();
#else
    Q_UNUSED(k)
    Q_UNUSED(volume)
    QApplication::beep();
#endif
}

QString builtinAlarmFile()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    const QString f = dir + QStringLiteral("/viviabreathe-alarm.wav");
    QFile src(QStringLiteral(":/sounds/alarm.wav"));
    if (!QFile::exists(f) || QFileInfo(f).size() != src.size()) {
        QFile::remove(f);
        src.copy(f);
        QFile::setPermissions(f, QFile::ReadOwner | QFile::WriteOwner);
    }
    return f;
}

bool vibrationAvailable()
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

void vibrate(int ms)
{
#if defined(Q_OS_ANDROID) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QJniObject ctx = QNativeInterface::QAndroidApplication::context();
    QJniObject service = QJniObject::getStaticObjectField("android/content/Context", "VIBRATOR_SERVICE", "Ljava/lang/String;");
    QJniObject vib = ctx.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", service.object<jstring>());
    if (vib.isValid()) vib.callMethod<void>("vibrate", "(J)V", jlong(ms));
#else
    Q_UNUSED(ms)
#endif
}

} // namespace Tones

// ====================================================================== MP3-Wiedergabe

struct MusicPlayer::Impl {
#ifdef VB_HAVE_MULTIMEDIA
    QMediaPlayer *player = nullptr;
#  if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioOutput *output = nullptr;
#  endif
#endif
};

MusicPlayer::MusicPlayer(QObject *parent) : QObject(parent), d(new Impl)
{
    m_fade = new QTimer(this);
    m_fade->setInterval(100);
    connect(m_fade, &QTimer::timeout, this, [this] {
        const qint64 t = QDateTime::currentMSecsSinceEpoch() - m_fadeStart;
        const double f = m_fadeMs > 0 ? qBound(0.0, double(t) / m_fadeMs, 1.0) : 1.0;
        m_volume = int(m_fadeFrom + (m_fadeTo - m_fadeFrom) * f + 0.5);
        applyVolume();
        if (f >= 1.0) {
            m_fade->stop();
            if (m_fadeStop) stop();
        }
    });
#ifdef VB_HAVE_MULTIMEDIA
    d->player = new QMediaPlayer(this);
#  if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    d->output = new QAudioOutput(this);
    d->player->setAudioOutput(d->output);
    connect(d->player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &msg) {
#  else
    connect(d->player, qOverload<QMediaPlayer::Error>(&QMediaPlayer::error), this, [this](QMediaPlayer::Error) {
        const QString msg = d->player->errorString();
#  endif
        emit errorOccurred(tr("%1 lässt sich nicht abspielen: %2").arg(QFileInfo(currentFile()).fileName(), msg));
        // nächste Datei versuchen, aber nicht endlos
        if (++m_errors < m_list.size()) QTimer::singleShot(200, this, &MusicPlayer::next);
        else stop();
    });
    connect(d->player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s) {
        if (s == QMediaPlayer::BufferedMedia || s == QMediaPlayer::LoadedMedia) m_errors = 0;
        if (s == QMediaPlayer::EndOfMedia && m_playing) next();
    });
#endif
}

MusicPlayer::~MusicPlayer()
{
    delete d;
}

bool MusicPlayer::available()
{
#ifdef VB_HAVE_MULTIMEDIA
    return true;
#else
    return false;
#endif
}

void MusicPlayer::setPlaylist(const QStringList &files, bool shuffle)
{
    m_list.clear();
    for (const QString &f : files)
        if (!f.isEmpty()) m_list << f;
    if (shuffle)
        for (int i = m_list.size() - 1; i > 0; --i)
            m_list.swapItemsAt(i, int(QRandomGenerator::global()->bounded(i + 1)));
    m_index = -1;
}

QString MusicPlayer::currentFile() const
{
    return m_list.value(m_index);
}

void MusicPlayer::play()
{
    if (m_list.isEmpty()) return;
    m_errors = 0;
    m_playing = true;
    playIndex(0);
}

void MusicPlayer::playIndex(int i)
{
    m_index = i;
#ifdef VB_HAVE_MULTIMEDIA
    const QUrl url = QUrl::fromLocalFile(m_list.value(i));
#  if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    d->player->setSource(url);
#  else
    d->player->setMedia(url);
#  endif
    applyVolume();
    d->player->play();
#endif
    emit trackChanged(m_list.value(i));
}

void MusicPlayer::next()
{
    if (!m_playing || m_list.isEmpty()) return;
    int n = m_index + 1;
    if (n >= m_list.size()) {
        if (!m_loop) { stop(); return; }
        n = 0;
    }
    playIndex(n);
}

void MusicPlayer::pause()
{
#ifdef VB_HAVE_MULTIMEDIA
    if (m_playing) d->player->pause();
#endif
}

void MusicPlayer::resume()
{
#ifdef VB_HAVE_MULTIMEDIA
    if (m_playing) d->player->play();
#endif
}

void MusicPlayer::stop()
{
    const bool was = m_playing;
    m_playing = false;
    m_fade->stop();
#ifdef VB_HAVE_MULTIMEDIA
    d->player->stop();
#endif
    if (was) emit stopped();
}

void MusicPlayer::setVolume(int v)
{
    m_fade->stop();
    m_volume = qBound(0, v, 100);
    applyVolume();
}

void MusicPlayer::applyVolume()
{
#ifdef VB_HAVE_MULTIMEDIA
#  if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    d->output->setVolume(float(linearVolume(m_volume)));
#  else
    d->player->setVolume(int(linearVolume(m_volume) * 100));
#  endif
#endif
}

void MusicPlayer::fadeTo(int target, int ms, bool stopAtEnd)
{
    m_fadeFrom = m_volume;
    m_fadeTo = qBound(0, target, 100);
    m_fadeMs = qMax(0, ms);
    m_fadeStop = stopAtEnd;
    m_fadeStart = QDateTime::currentMSecsSinceEpoch();
    m_fade->start();
}
