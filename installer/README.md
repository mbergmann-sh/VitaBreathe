# VitaBreathe – Windows-Installer (Qt Installer Framework)

Ein **Release-Build unter Windows** (Qt Creator oder Konsole, MinGW oder MSVC) erledigt alles
automatisch – gesteuert von `installer/windows_deploy.pri`, das `VitaBreathe.pro` unter Windows
einbindet (Aufbau wie bei VitaParms):

| Schritt | Ergebnis |
|---|---|
| `windeployqt` nach **jedem** Build | Qt-DLLs, Plugins (inkl. Multimedia für MP3) und Qt-Übersetzungen DE/EN neben `VitaBreathe.exe` |
| Programmdateien (nur Release) | `installer/qtifw/data`: `VitaBreathe.exe`, DLLs, Plugins, `LICENSE.txt`, `README.md` |
| Paketbäume je Sprache | `installer/qtifw/packages_de` und `packages_en` (Beschreibung und Lizenz in der jeweiligen Sprache) |
| Qt Installer Framework (falls installiert) | `installer/Output/VitaBreathe-<Version>-Setup-de.exe` und `…-Setup-en.exe` |

## Sprache der Installation

Der Installer installiert **wahlweise auf Deutsch oder auf Englisch**: Es entstehen zwei
Setup-Dateien, eine je Sprache. Hintergrund: Das Qt Installer Framework legt die Sprache seiner
Oberfläche beim Start fest – entweder nach der Windows-Sprache oder fest über `<Translations>` in
`config.xml`. Eine Umschaltung zur Laufzeit (etwa über eine Sprachauswahl-Seite) bietet es nicht
(nachgeprüft im IFW-Quelltext, `src/sdk/sdkapp.h`). Deshalb:

* `config_de.xml` lädt `ifw_de` und `qt_de` → der ganze Assistent ist deutsch, auch auf einem englischen Windows.
* `config_en.xml` lädt `ifw_en` → der ganze Assistent ist englisch, auch auf einem deutschen Windows.
* Beschreibung, Lizenz und Haftungshinweis, Startmenü-Einträge und Rückfragen folgen der Installationssprache.
* **Die Programmsprache wird passend gesetzt** (`HKCU\Software\mbergmann-sh\VitaBreathe\ui\language`
  = `de` bzw. `en`). In VitaBreathe lässt sie sich jederzeit ändern (*Extras → Einstellungen →
  Darstellung* bzw. im Anmeldefenster).

## Eigenschaften

* Zielordner Vorgabe **`C:\VitaBreathe`** (änderbar), Lizenz und Haftungshinweis zum Annehmen
* Startmenü: VitaBreathe, VitaBreathe – Demo (`--demo`), „VitaBreathe aktualisieren oder entfernen“;
  Desktop-Verknüpfung; am Ende „VitaBreathe starten“
* Wartungsprogramm `VitaBreathe-Wartung.exe` (deutsch) bzw. `VitaBreathe-Maintenance.exe` (englisch)
* **Update mit einem neueren Installer:** Das Steuerskript erkennt eine vorhandene Installation – auch
  eine in der anderen Sprache –, entfernt sie nach Rückfrage über ihr Wartungsprogramm (`purge`) und
  installiert die neue Version in denselben Ordner. Datenbank und Einstellungen liegen im
  Benutzerprofil (`%APPDATA%\mbergmann-sh\VitaBreathe`) und bleiben erhalten.
* `config_*.xml` ist gegen die Elementliste des IFW-Quelltexts (`settings.cpp`) geprüft, die Skripte
  auf gültige JavaScript-Syntax. Ein Testlauf auf einem echten Windows-Rechner steht noch aus.

## Werkzeuge

* **Qt Installer Framework 4.x** – im Qt Maintenance Tool unter *Developer and Designer Tools*.
  Gesucht in `<Qt>\Tools\QtInstallerFramework\<neueste Version>\bin` und über `QTIFWDIR`;
  sonst Umgebungsvariable `VB_BINARYCREATOR` auf `binarycreator.exe` setzen.
* Gesucht wird beim **qmake-Lauf** (Qt Creator: *Erstellen → qmake ausführen*); die Meldung
  „Qt Installer Framework gefunden …“ steht in der Ausgabe.
* Installer nicht bei jedem Release-Build erzeugen: `qmake CONFIG+=vb_no_installer`, dann von Hand:
  ```
  binarycreator --offline-only -c qtifw\config\config_de.xml -p qtifw\packages_de Output\VitaBreathe-Setup-de.exe
  binarycreator --offline-only -c qtifw\config\config_en.xml -p qtifw\packages_en Output\VitaBreathe-Setup-en.exe
  ```

## Dateien

```
qtifw/config/config_de.xml.in, config_en.xml.in   Vorlagen (qmake trägt Version ein → config_*.xml)
qtifw/config/controlscript.qs                     Update-Erkennung, zweisprachig
qtifw/config/*.png, vitabreathe.ico               Bilder des Assistenten (aus dem Programmsymbol erzeugt)
qtifw/meta/de|en/package.xml.in, license.txt      Paketbeschreibung und Lizenz je Sprache
qtifw/meta/installscript.qs                       Verknüpfungen, Programmsprache
```

**Änderungen bitte immer in den `.in`-Dateien vornehmen** – qmake schreibt die Versionsnummer aus
`version.pri` bei jedem Lauf in `config_*.xml` und `package.xml`.

## Hinweise

* `windeployqt` lässt Software-OpenGL und den Direct3D-Compiler weg (reine Widgets-Anwendung). Das
  Multimedia-Backend bleibt, weil VitaBreathe MP3-Dateien abspielt. Vollständige Bereitstellung:
  `qmake CONFIG+=vb_full_deploy`.
* HTTPS für den Abgleich: Qt 6 nutzt unter Windows das systemeigene TLS (Schannel-Plugin, von
  windeployqt mitgeliefert). Nur ein Build mit **Qt 5.15** braucht OpenSSL 1.1 (`libssl-1_1-x64.dll`,
  `libcrypto-1_1-x64.dll`) neben dem Programm – dann vor dem Installerbau in `installer/qtifw/data` legen.
