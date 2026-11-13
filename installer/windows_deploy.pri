# ---------------------------------------------------------------------------
#  ViviaBreathe – Windows: DLLs bereitstellen und Qt-Installer-Framework-Installer bauen
#  (Aufbau wie in VitaParms)
#
#  Nach jedem erfolgreichen Windows-Build (Qt Creator oder Konsole):
#    1. windeployqt kopiert die nötigen Qt-DLLs, Plugins und Übersetzungen neben
#       ViviaBreathe.exe – das Programm startet direkt aus dem Build-Ordner.
#  Zusätzlich beim Release-Build:
#    2. installer/qtifw/data wird neu befüllt: ViviaBreathe.exe, alle DLLs und
#       windeployqt-Unterordner, LICENSE.txt, README.md
#    3. Je Sprache ein Paketbaum installer/qtifw/packages_de bzw. packages_en
#       (gleiche Programmdateien, Beschreibung und Lizenz in der jeweiligen Sprache)
#    4. Ist das Qt Installer Framework installiert:
#         installer/Output/ViviaBreathe-<Version>-Setup-de.exe   (Installation auf Deutsch)
#         installer/Output/ViviaBreathe-<Version>-Setup-en.exe   (installation in English)
#       Das Qt Installer Framework legt die Sprache beim Start fest (config.xml → Translations);
#       eine Umschaltung zur Laufzeit gibt es nicht. Daher je Sprache ein Installer. Beide setzen
#       die Programmsprache passend und erkennen eine vorhandene Installation der anderen Sprache.
#
#  binarycreator wird beim qmake-Lauf gesucht; abweichender Ort über die Umgebungsvariable
#    VB_BINARYCREATOR   Pfad zu binarycreator.exe   (Vorgabe: <Qt>/Tools/QtInstallerFramework/<neueste>/bin
#                                                     bzw. %QTIFWDIR%/bin)
#  Installer nicht automatisch bauen:   qmake CONFIG+=vb_no_installer
#  Alles wie von windeployqt vorgesehen bereitstellen:   qmake CONFIG+=vb_full_deploy
#
#  Alle Pfade mit Schrägstrich und in Anführungszeichen – das funktioniert unverändert in
#  cmd.exe und in einer sh.exe. Dateien kopiert qmake selbst (qmake -install qinstall).
# ---------------------------------------------------------------------------

defineReplace(vbQuote) {
    return("\"$$clean_path($$1)\"")
}

VB_INST     = $$clean_path($$PWD)
VB_IFW      = $$VB_INST/qtifw
VB_DATA     = $$VB_IFW/data
VB_PKG      = de.mbergmann.viviabreathe
VB_OUTPUT   = $$VB_INST/Output

VB_WINDEPLOYQT = $$vbQuote($$[QT_INSTALL_BINS]/windeployqt.exe) --verbose 0

# Schlank ausliefern: reine Widgets-Anwendung (kein OpenGL/Direct3D/Qt Quick). Das
# Multimedia-Backend (unter Qt 6 FFmpeg bzw. Windows Media) bleibt – es spielt die MP3-Dateien.
# Qt-Übersetzungen: Deutsch und Englisch.
!vb_full_deploy {
    VB_WINDEPLOYQT += --no-opengl-sw --no-system-d3d-compiler --translations de,en
    versionAtLeast(QT_VERSION, 6.8.0): VB_WINDEPLOYQT += --no-system-dxc-compiler
}
VB_QINSTALL    = $$vbQuote($$QMAKE_QMAKE) -install qinstall
VB_EXE         = \"$(DESTDIR_TARGET)\"
VB_NL          = $$escape_expand(\\n\\t)

# 1. Qt-DLLs neben das Programm im Build-Ordner
QMAKE_POST_LINK += $$VB_WINDEPLOYQT $$VB_EXE

CONFIG(release, debug|release) {
    # 2. Programmdateien für die Installer frisch aufbauen (führendes "-": Fehler ignorieren)
    QMAKE_POST_LINK += $$VB_NL-$$QMAKE_DEL_TREE $$shell_quote($$shell_path($$VB_DATA))
    QMAKE_POST_LINK += $$VB_NL$$VB_WINDEPLOYQT --dir $$vbQuote($$VB_DATA) $$VB_EXE
    QMAKE_POST_LINK += $$VB_NL$$VB_QINSTALL -exe $$VB_EXE $$vbQuote($$VB_DATA/ViviaBreathe.exe)
    QMAKE_POST_LINK += $$VB_NL$$VB_QINSTALL $$vbQuote($$PWD/../LICENSE) $$vbQuote($$VB_DATA/LICENSE.txt)
    QMAKE_POST_LINK += $$VB_NL$$VB_QINSTALL $$vbQuote($$PWD/../README.md) $$vbQuote($$VB_DATA/README.md)

    # 3. Paketbäume je Sprache
    for(lang, $$list(de en)) {
        VB_PKGDIR = $$VB_IFW/packages_$$lang/$$VB_PKG
        QMAKE_POST_LINK += $$VB_NL-$$QMAKE_DEL_TREE $$shell_quote($$shell_path($$VB_IFW/packages_$$lang))
        QMAKE_POST_LINK += $$VB_NL$$VB_QINSTALL $$vbQuote($$VB_IFW/meta/$$lang/package.xml) $$vbQuote($$VB_PKGDIR/meta/package.xml)
        QMAKE_POST_LINK += $$VB_NL$$VB_QINSTALL $$vbQuote($$VB_IFW/meta/$$lang/license.txt) $$vbQuote($$VB_PKGDIR/meta/license.txt)
        QMAKE_POST_LINK += $$VB_NL$$VB_QINSTALL $$vbQuote($$VB_IFW/meta/installscript.qs) $$vbQuote($$VB_PKGDIR/meta/installscript.qs)
        QMAKE_POST_LINK += $$VB_NL$$VB_QINSTALL $$vbQuote($$VB_DATA) $$vbQuote($$VB_PKGDIR/data)
    }

    # 4. Installer bauen, sofern das Qt Installer Framework vorhanden ist
    !vb_no_installer {
        VB_BINARYCREATOR = $$(VB_BINARYCREATOR)
        isEmpty(VB_BINARYCREATOR) {
            VB_IFW_ROOT = $$(QTIFWDIR)
            !isEmpty(VB_IFW_ROOT): exists($$VB_IFW_ROOT/bin/binarycreator.exe): VB_BINARYCREATOR = $$VB_IFW_ROOT/bin/binarycreator.exe
        }
        isEmpty(VB_BINARYCREATOR) {
            # Qt-Online-Installer: <Qt>/<Version>/<Kit> → <Qt>/Tools/QtInstallerFramework/<Version>/bin
            VB_IFW_VERSIONS = $$files($$clean_path($$[QT_INSTALL_PREFIX]/../../Tools/QtInstallerFramework/*))
            for(d, VB_IFW_VERSIONS): exists($$d/bin/binarycreator.exe): VB_BINARYCREATOR = $$d/bin/binarycreator.exe
        }
        !isEmpty(VB_BINARYCREATOR) {
            message("ViviaBreathe: Qt Installer Framework gefunden – $$VB_BINARYCREATOR")
            QMAKE_POST_LINK += $$VB_NL-$$QMAKE_MKDIR $$shell_quote($$shell_path($$VB_OUTPUT))
            for(lang, $$list(de en)) {
                QMAKE_POST_LINK += $$VB_NL-$$vbQuote($$VB_BINARYCREATOR) --offline-only --compression 9 \
                    -c $$vbQuote($$VB_IFW/config/config_$${lang}.xml) -p $$vbQuote($$VB_IFW/packages_$$lang) \
                    $$vbQuote($$VB_OUTPUT/ViviaBreathe-$${VERSION}-Setup-$${lang}.exe)
            }
        } else {
            message("ViviaBreathe: Qt Installer Framework nicht gefunden – kein Installer (siehe installer/README.md)")
        }
    }
}
