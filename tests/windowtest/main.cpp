// Fenster öffnen, verschieben/vergrößern, schließen – beim nächsten Öffnen dieselbe Lage?
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "screenfit.h"
#include "windowstate.h"
#include <QApplication>
#include <QDialog>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdio>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { ++fails; std::printf("FEHLER: %s\n", msg); } else std::printf("ok: %s\n", msg); } while (0)

int main(int argc, char **argv)
{
    QTemporaryDir tmp;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tmp.path());
    QApplication app(argc, argv);
    QApplication::setOrganizationName("vbtest");
    QApplication::setApplicationName("windowtest");
    ScreenFit::install(app);
    WindowState::install(app);

    const QRect want(120, 90, 640, 480);
    {
        QDialog d;
        d.setObjectName("TestDialog");
        d.resize(300, 200);
        d.show();
        app.processEvents();
        d.setGeometry(want);
        app.processEvents();
        d.close();
        app.processEvents();
    }
    CHECK(QSettings().contains("windows/TestDialog"), "Lage beim Schließen gespeichert");
    {
        QDialog d;
        d.setObjectName("TestDialog");
        d.resize(300, 200);                // Vorgabe im Konstruktor wird überschrieben
        d.show();
        app.processEvents();
        CHECK(d.size() == want.size(), qPrintable(QString("Größe wiederhergestellt (%1×%2)").arg(d.width()).arg(d.height())));
        CHECK(d.geometry().topLeft() == want.topLeft(), qPrintable(QString("Position wiederhergestellt (%1,%2)").arg(d.x()).arg(d.y())));
        d.close();
    }
    {
        QDialog anon;                      // ohne Namen: nicht gemerkt
        anon.show();
        app.processEvents();
        anon.close();
        QMessageBox box;                   // Qt-eigene Dialoge: nicht gemerkt
        box.show();
        app.processEvents();
        box.close();
        CHECK(QSettings().childKeys().isEmpty() && QSettings().allKeys().size() == 1, "unbenannte und Qt-eigene Dialoge bleiben unberührt");
    }
    {
        QDialog big;                       // größer als der Bildschirm gespeichert → ScreenFit passt an
        big.setObjectName("BigDialog");
        QSettings().remove("windows/BigDialog");
        big.show();
        app.processEvents();
        big.setGeometry(0, 0, 5000, 4000);
        big.close();
        QDialog again;
        again.setObjectName("BigDialog");
        again.show();
        app.processEvents();
        const QRect avail = again.screen()->availableGeometry();
        CHECK(avail.contains(again.frameGeometry()) || again.width() <= avail.width(), "zu große gespeicherte Lage wird an den Bildschirm angepasst");
        again.close();
    }
    std::printf("\n%s (%d Fehler)\n", fails ? "FEHLGESCHLAGEN" : "ALLES OK", fails);
    return fails ? 1 : 0;
}
