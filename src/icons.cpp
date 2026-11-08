// SPDX-License-Identifier: MIT
#include "icons.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QWidget>
#include <QFile>
#include <QHash>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>

namespace Icons {

namespace {
QHash<QString, QIcon> &cache()
{
    static QHash<QString, QIcon> c;
    return c;
}
QHash<qint64, QString> &names()      // cacheKey eines ausgegebenen Symbols → Name
{
    static QHash<qint64, QString> n;
    return n;
}

QPixmap render(const QByteArray &svg, int size, qreal dpr)
{
    QSvgRenderer r(svg);
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    r.render(&p);
    p.end();
    pm.setDevicePixelRatio(dpr);
    return pm;
}
} // namespace

QColor accent() { return QColor(0xE0, 0x62, 0x0D); }
QColor primary() { return QColor(0x14, 0x74, 0xBC); }

QIcon app()
{
    QIcon i;
    i.addFile(QStringLiteral(":/icons/vitabreathe.png"));
    return i;
}

QIcon get(const QString &name)
{
    auto it = cache().constFind(name);
    if (it != cache().constEnd()) return *it;
    QFile f(QStringLiteral(":/icons/ui/%1.svg").arg(name));
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray src = f.readAll();
    const QPalette pal = QApplication::palette();
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor disabled = pal.color(QPalette::Disabled, QPalette::WindowText);
    // Akzent auf dunklem Hintergrund etwas heller
    const QColor acc = pal.color(QPalette::Window).lightness() < 110 ? QColor(0xF5, 0x8A, 0x3C) : accent();
    auto colored = [&](const QColor &c, const QColor &a) {
        QByteArray s = src;
        s.replace("#COLOR", c.name().toLatin1());
        s.replace("#ACCENT", a.name().toLatin1());
        return s;
    };
    const QByteArray normal = colored(fg, acc), off = colored(disabled, disabled);
    QIcon icon;
    for (int size : { 16, 20, 24, 32, 48 }) {
        icon.addPixmap(render(normal, size, 1.0), QIcon::Normal);
        icon.addPixmap(render(normal, size, 2.0), QIcon::Normal);
        icon.addPixmap(render(off, size, 1.0), QIcon::Disabled);
        icon.addPixmap(render(off, size, 2.0), QIcon::Disabled);
    }
    cache().insert(name, icon);
    names().insert(icon.cacheKey(), name);
    return icon;
}

QPixmap illustration(const QString &name, const QSize &size, qreal dpr)
{
    QFile f(QStringLiteral(":/illustrations/%1.svg").arg(name));
    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray svg = f.readAll();
    const QPalette pal = QApplication::palette();
    const bool dark = pal.color(QPalette::Window).lightness() < 110;
    svg.replace("#COLOR", pal.color(QPalette::WindowText).name().toLatin1());
    svg.replace("#ACCENT", dark ? QByteArray("#f58a3c") : accent().name().toLatin1());
    svg.replace("#BLUE", dark ? QByteArray("#4a9ee0") : primary().name().toLatin1());
    svg.replace("#GREEN", dark ? QByteArray("#4cc28a") : QByteArray("#2e9e6b"));
    QSvgRenderer r(svg);
    QSize target = r.defaultSize();
    target.scale(size, Qt::KeepAspectRatio);
    QPixmap pm(int(target.width() * dpr), int(target.height() * dpr));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    r.render(&p);
    p.end();
    pm.setDevicePixelRatio(dpr);
    return pm;
}

void clearCache()
{
    cache().clear();
}

void refreshWidgets()
{
    const QHash<qint64, QString> old = names();
    clearCache();
    names().clear();
    for (QWidget *top : QApplication::topLevelWidgets()) {
        for (QAbstractButton *b : top->findChildren<QAbstractButton *>()) {
            const QString n = old.value(b->icon().cacheKey());
            if (!n.isEmpty()) b->setIcon(get(n));
        }
        for (QAction *a : top->findChildren<QAction *>()) {
            const QString n = old.value(a->icon().cacheKey());
            if (!n.isEmpty()) a->setIcon(get(n));
        }
    }
}

} // namespace Icons
