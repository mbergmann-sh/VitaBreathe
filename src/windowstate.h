// VitaBreathe – Positionen und Größen der Fenster merken und wiederherstellen
//
// App-weiter Ereignisfilter: Beim ersten Anzeigen eines Dialogs wird seine zuletzt gespeicherte
// Geometrie (Position, Größe, ggf. maximiert) wiederhergestellt, beim Schließen bzw. Ausblenden
// gespeichert – in den Einstellungen dieses Geräts (QSettings, Gruppe "windows/"), nicht in der
// abgeglichenen Datenbank, denn Bildschirme unterscheiden sich von Gerät zu Gerät.
// Schlüssel: Klassenname, bei schlichten QDialogs zusätzlich der objectName (ohne objectName
// werden schlichte QDialogs nicht gemerkt). Dialoge von Qt selbst (Datei-, Meldungs- …) bleiben
// unberührt, ausgenommen die Druckvorschau. Das Hauptfenster speichert sich selbst (MainWindow).
// Danach passt ScreenFit das Fenster an den aktuellen Bildschirm an (z. B. anderer Monitor).
// SPDX-License-Identifier: MIT
#pragma once

class QApplication;
class QWidget;

namespace WindowState {
// Nach ScreenFit::install() aufrufen: zuletzt installierte Filter laufen zuerst –
// so wird erst wiederhergestellt und dann an den Bildschirm angepasst.
void install(QApplication &app);
void save(QWidget *window);
bool restore(QWidget *window);
}
