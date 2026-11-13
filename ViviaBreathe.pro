# ---------------------------------------------------------------------------
#  ViviaBreathe – geführte Atemübungen mit Tagebuch
#  Copyright (c) 2026 Michael Bergmann – MIT License (siehe LICENSE)
#
#  KEIN Medizinprodukt – ViviaBreathe ist ein persönliches Übungs-Tagebuch.
#
#  Build:   qmake6 ViviaBreathe.pro && make        (Linux/macOS)
#           qmake6 ViviaBreathe.pro && nmake/jom   (Windows, MSVC)
#           qmake (Qt 5.15) ViviaBreathe.pro && make – z. B. für macOS 10.13–10.15
#  Benötigt Qt 6.4 oder neuer bzw. Qt 5.15 mit den Modulen:
#           widgets printsupport network svg multimedia
#  (Qt-5/6-Unterschiede gleicht src/qtcompat.h bzw. src/audio.cpp aus)
#  Datenbank: SQLite3 Multiple Ciphers (3rdparty/sqlite3mc, MIT) – fest eingebaut, verschlüsselt
# ---------------------------------------------------------------------------

lessThan(QT_MAJOR_VERSION, 5) | equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 15) {
    error("ViviaBreathe braucht Qt 5.15 oder Qt 6.4 und neuer (gefunden: $$[QT_VERSION])")
}
equals(QT_MAJOR_VERSION, 6):lessThan(QT_MINOR_VERSION, 4) {
    error("ViviaBreathe braucht unter Qt 6 mindestens Qt 6.4 (gefunden: $$[QT_VERSION])")
}
equals(QT_MAJOR_VERSION, 5): message("ViviaBreathe: Build mit Qt $$[QT_VERSION] (Qt-5-Kompatibilitaet)")
macx:equals(QT_MAJOR_VERSION, 5): QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.13

QT       += core gui widgets printsupport network svg

# MP3-Wiedergabe (Hintergrundmusik, Wecker, Einschlafen) und Signaltöne: Qt Multimedia
qtHaveModule(multimedia) {
    QT      += multimedia
    DEFINES += VB_HAVE_MULTIMEDIA
    message("ViviaBreathe: mit Qt Multimedia (MP3, Signaltoene)")
} else {
    warning("ViviaBreathe: OHNE Qt Multimedia - keine Musik, nur Systempiepton")
}

CONFIG   += c++17
TEMPLATE  = app
TARGET    = ViviaBreathe

include(version.pri)
DEFINES  += VIVIABREATHE_VERSION=\\\"$$VERSION\\\"
greaterThan(QT_MAJOR_VERSION, 5): DEFINES += QT_DISABLE_DEPRECATED_UP_TO=0x060400

# Gemeinsamer Kern (später auch für die mobile App unter mobile/)
include(core.pri)
# Desktop-Oberfläche (Qt Widgets)
include(desktop.pri)

RESOURCES += resources/viviabreathe.qrc

# Programmsymbol
win32: RC_ICONS = resources/icons/app/viviabreathe.ico
macx:  ICON     = resources/icons/app/viviabreathe.icns

# Übersetzungen: Quellsprache Deutsch, Englisch als zweite Sprache.
# Aktualisieren:  lupdate ViviaBreathe.pro   –   übersetzen mit Qt Linguist
# Die .qm-Datei wird beim Bauen automatisch erzeugt und eingebettet (:/i18n/viviabreathe_en.qm).
TRANSLATIONS = translations/viviabreathe_en.ts
# Nur deutsche Mehrzahlformen (%n), von lupdate ViviaBreathe.pro nicht angefasst:
#   lupdate -pluralonly -no-obsolete src -ts translations/viviabreathe_de.ts
EXTRA_TRANSLATIONS = translations/viviabreathe_de.ts
CONFIG += lrelease embed_translations

# Windows-Installer: Versionsnummer und Datum in die Installer-Dateien übernehmen
# (*.in → Datei ohne .in; @VP_VERSION@, @VP_RELEASE_DATE@). Läuft auf allen Plattformen,
# damit die erzeugten Dateien im Quellbaum immer zur Programmversion passen (wie in VitaParms).
VB_TEMPLATES = installer/qtifw/config/config_de.xml.in \
               installer/qtifw/config/config_en.xml.in \
               installer/qtifw/meta/de/package.xml.in \
               installer/qtifw/meta/en/package.xml.in
for(t, VB_TEMPLATES) {
    vb_in = $$cat($$PWD/$$t, blob)
    vb_in = $$replace(vb_in, @VP_VERSION@, $$VERSION)
    vb_in = $$replace(vb_in, @VP_RELEASE_DATE@, $$VB_RELEASE_DATE)
    vb_out = $$PWD/$$replace(t, \.in$, )
    vb_cur = $$cat($$vb_out, blob)
    !equals(vb_cur, $$vb_in$$escape_expand(\\n)): write_file($$vb_out, vb_in)
}

# Windows: DLLs bereitstellen und Qt-Installer-Framework-Installer (Deutsch und Englisch)
win32: include(installer/windows_deploy.pri)

OTHER_FILES += installer/README.md installer/windows_deploy.pri \
    installer/qtifw/config/config_de.xml.in installer/qtifw/config/config_en.xml.in \
    installer/qtifw/config/controlscript.qs installer/qtifw/meta/installscript.qs \
    installer/qtifw/meta/de/package.xml.in installer/qtifw/meta/en/package.xml.in
