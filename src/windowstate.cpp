// SPDX-License-Identifier: MIT
#include "windowstate.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QMainWindow>
#include <QSettings>
#include <QWidget>

namespace {

const char *kRestored = "vbGeometryRestored";

bool isQtOwnDialog(const QWidget *w)
{
    for (const char *cls : { "QMessageBox", "QFileDialog", "QColorDialog", "QFontDialog", "QInputDialog",
                             "QProgressDialog", "QErrorMessage", "QAbstractPrintDialog", "QPageSetupDialog", "QWizard" })
        if (w->inherits(cls)) return true;
    return false;
}

QString keyFor(const QWidget *w)
{
    if (!w || !w->isWindow() || qobject_cast<const QMainWindow *>(w)) return {};
    const Qt::WindowType t = w->windowType();
    if (t != Qt::Window && t != Qt::Dialog && t != Qt::Sheet && t != Qt::Tool) return {};
    if (isQtOwnDialog(w)) return {};
    const QString cls = QString::fromLatin1(w->metaObject()->className());
    if (cls == QLatin1String("QDialog") || cls == QLatin1String("QWidget")) {
        // schlichte Dialoge nur mit eigenem Namen (sonst teilten sich alle denselben Eintrag)
        if (w->objectName().isEmpty()) return {};
        return QStringLiteral("windows/") + w->objectName();
    }
    return QStringLiteral("windows/") + cls;
}

class Filter : public QObject
{
public:
    using QObject::QObject;
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (!o->isWidgetType()) return false;
        auto *w = static_cast<QWidget *>(o);
        switch (e->type()) {
        case QEvent::Show:
            if (w->isWindow() && !w->property(kRestored).toBool()) {
                w->setProperty(kRestored, true);
                WindowState::restore(w);
            }
            break;
        case QEvent::Hide:
        case QEvent::Close:
            if (w->isWindow() && w->property(kRestored).toBool()) WindowState::save(w);
            break;
        default:
            break;
        }
        return false;
    }
};

} // namespace

namespace WindowState {

void install(QApplication &app)
{
    app.installEventFilter(new Filter(&app));
}

void save(QWidget *w)
{
    const QString key = keyFor(w);
    if (key.isEmpty()) return;
    // Vollbild (Übung) und minimiert nicht merken – nur die normale bzw. maximierte Lage
    if (w->isFullScreen() || w->isMinimized()) return;
    QSettings().setValue(key, w->saveGeometry());
}

bool restore(QWidget *w)
{
    const QString key = keyFor(w);
    if (key.isEmpty()) return false;
    const QByteArray g = QSettings().value(key).toByteArray();
    return !g.isEmpty() && w->restoreGeometry(g);
}

} // namespace WindowState
