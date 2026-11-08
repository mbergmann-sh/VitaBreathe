// SPDX-License-Identifier: MIT
#include "playlistwidget.h"
#include "audio.h"
#include "icons.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QSettings>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

QString chooseMusicDir()
{
    const QString def = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    return QSettings().value(QStringLiteral("ui/musicDir"), def).toString();
}

void rememberMusicDir(const QString &file)
{
    QSettings().setValue(QStringLiteral("ui/musicDir"), QFileInfo(file).absolutePath());
}

QString musicFileFilter()
{
    return QObject::tr("Musik (*.mp3 *.MP3 *.m4a *.ogg *.wav *.flac);;Alle Dateien (*)");
}

namespace {
QToolButton *toolButton(const QString &icon, const QString &tip)
{
    auto *b = new QToolButton;
    b->setIcon(Icons::get(icon));
    b->setToolTip(tip);
    b->setAutoRaise(false);
    return b;
}
}

PlaylistWidget::PlaylistWidget(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_list = new QListWidget;
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setMinimumHeight(90);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    lay->addWidget(m_list, 1);
    auto *col = new QVBoxLayout;
    m_add = toolButton(QStringLiteral("add"), tr("MP3-Dateien hinzufügen …"));
    m_remove = toolButton(QStringLiteral("delete"), tr("Entfernen"));
    m_up = new QToolButton;
    m_up->setArrowType(Qt::UpArrow);
    m_up->setToolTip(tr("Nach oben"));
    m_down = new QToolButton;
    m_down->setArrowType(Qt::DownArrow);
    m_down->setToolTip(tr("Nach unten"));
    m_preview = toolButton(QStringLiteral("play"), tr("Probehören (ausgewählter Titel)"));
    m_preview->setCheckable(true);
    for (QToolButton *b : { m_add, m_remove, m_up, m_down, m_preview }) col->addWidget(b);
    col->addStretch();
    lay->addLayout(col);
    m_player = new MusicPlayer(this);
    m_player->setLoop(false);

    connect(m_add, &QToolButton::clicked, this, [this] {
        const QStringList files = QFileDialog::getOpenFileNames(this, tr("Musik hinzufügen"), chooseMusicDir(), musicFileFilter());
        if (files.isEmpty()) return;
        rememberMusicDir(files.first());
        for (const QString &f : files) {
            auto *it = new QListWidgetItem(QFileInfo(f).fileName(), m_list);
            it->setData(Qt::UserRole, f);
            it->setToolTip(f);
        }
        refreshButtons();
        emit changed();
    });
    connect(m_remove, &QToolButton::clicked, this, [this] {
        qDeleteAll(m_list->selectedItems());
        refreshButtons();
        emit changed();
    });
    auto move = [this](int delta) {
        const int r = m_list->currentRow();
        const int n = r + delta;
        if (r < 0 || n < 0 || n >= m_list->count()) return;
        QListWidgetItem *it = m_list->takeItem(r);
        m_list->insertItem(n, it);
        m_list->setCurrentRow(n);
        emit changed();
    };
    connect(m_up, &QToolButton::clicked, this, [move] { move(-1); });
    connect(m_down, &QToolButton::clicked, this, [move] { move(1); });
    connect(m_preview, &QToolButton::toggled, this, [this](bool on) {
        if (!on) { m_player->stop(); return; }
        QListWidgetItem *it = m_list->currentItem() ? m_list->currentItem() : m_list->item(0);
        if (!it) { m_preview->setChecked(false); return; }
        m_player->setPlaylist({ it->data(Qt::UserRole).toString() });
        m_player->setVolume(m_previewVolume);
        m_player->play();
    });
    connect(m_player, &MusicPlayer::stopped, this, [this] { m_preview->setChecked(false); });
    connect(m_list, &QListWidget::itemSelectionChanged, this, &PlaylistWidget::refreshButtons);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, &PlaylistWidget::changed);
    m_preview->setEnabled(MusicPlayer::available());
    refreshButtons();
}

void PlaylistWidget::refreshButtons()
{
    const bool sel = !m_list->selectedItems().isEmpty();
    m_remove->setEnabled(sel);
    m_up->setEnabled(sel && m_list->currentRow() > 0);
    m_down->setEnabled(sel && m_list->currentRow() < m_list->count() - 1);
}

void PlaylistWidget::setFiles(const QStringList &files)
{
    m_list->clear();
    for (const QString &f : files) {
        auto *it = new QListWidgetItem(QFileInfo(f).fileName(), m_list);
        it->setData(Qt::UserRole, f);
        it->setToolTip(f);
        if (!QFileInfo::exists(f)) {
            it->setForeground(Qt::red);
            it->setToolTip(tr("Datei nicht gefunden (z. B. von einem anderen Gerät): %1").arg(f));
        }
    }
    refreshButtons();
}

QStringList PlaylistWidget::files() const
{
    QStringList l;
    for (int i = 0; i < m_list->count(); ++i) l << m_list->item(i)->data(Qt::UserRole).toString();
    return l;
}

// ====================================================================== einzelne Datei

MusicFileEdit::MusicFileEdit(QWidget *parent) : QWidget(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_edit = new QLineEdit;
    m_edit->setPlaceholderText(tr("MP3-Datei wählen …"));
    m_edit->setClearButtonEnabled(true);
    lay->addWidget(m_edit, 1);
    auto *browse = new QToolButton;
    browse->setText(QStringLiteral("…"));
    browse->setToolTip(tr("Datei wählen"));
    lay->addWidget(browse);
    m_preview = new QToolButton;
    m_preview->setIcon(Icons::get(QStringLiteral("play")));
    m_preview->setToolTip(tr("Probehören"));
    m_preview->setCheckable(true);
    m_preview->setEnabled(MusicPlayer::available());
    lay->addWidget(m_preview);
    m_player = new MusicPlayer(this);
    m_player->setLoop(false);
    connect(browse, &QToolButton::clicked, this, [this] {
        const QString start = m_edit->text().isEmpty() ? chooseMusicDir() : QFileInfo(m_edit->text()).absolutePath();
        const QString f = QFileDialog::getOpenFileName(this, tr("Hintergrundmusik wählen"), start, musicFileFilter());
        if (f.isEmpty()) return;
        rememberMusicDir(f);
        m_edit->setText(QDir::toNativeSeparators(f));
    });
    connect(m_edit, &QLineEdit::textChanged, this, [this] {
        m_edit->setStyleSheet(m_edit->text().isEmpty() || QFileInfo::exists(file()) ? QString()
                                                                                   : QStringLiteral("color: #c0392b;"));
        emit changed();
    });
    connect(m_preview, &QToolButton::toggled, this, [this](bool on) {
        if (!on || file().isEmpty()) { m_player->stop(); m_preview->setChecked(false); return; }
        m_player->setPlaylist({ file() });
        m_player->setVolume(m_volume);
        m_player->play();
    });
    connect(m_player, &MusicPlayer::stopped, this, [this] { m_preview->setChecked(false); });
}

void MusicFileEdit::setFile(const QString &f) { m_edit->setText(QDir::toNativeSeparators(f)); }
QString MusicFileEdit::file() const { return QDir::fromNativeSeparators(m_edit->text().trimmed()); }
void MusicFileEdit::setPreviewVolume(int v)
{
    m_volume = v;
    if (m_player->isPlaying()) m_player->setVolume(v);
}
