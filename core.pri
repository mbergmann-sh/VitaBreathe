# ---------------------------------------------------------------------------
#  VitaBreathe – gemeinsamer Kern: Datenbank, Verschlüsselung, Übungsmodule,
#  Audio, Abgleich. Wird vom Desktop-Programm eingebunden und später von der
#  mobilen App (Android/iOS) wiederverwendet.
# ---------------------------------------------------------------------------
INCLUDEPATH += $$PWD/src
include($$PWD/3rdparty/sqlite3mc/sqlite3mc.pri)
android: include($$PWD/3rdparty/android_openssl/openssl.pri)

SOURCES += \
    $$PWD/src/sqlquery.cpp \
    $$PWD/src/database.cpp \
    $$PWD/src/breathmodules.cpp \
    $$PWD/src/breathengine.cpp \
    $$PWD/src/audio.cpp \
    $$PWD/src/schedule.cpp \
    $$PWD/src/dbsync.cpp \
    $$PWD/src/themes.cpp \
    $$PWD/src/icons.cpp \
    $$PWD/src/i18n.cpp

HEADERS += \
    $$PWD/src/qtcompat.h \
    $$PWD/src/sqlquery.h \
    $$PWD/src/database.h \
    $$PWD/src/datatypes.h \
    $$PWD/src/breathmodules.h \
    $$PWD/src/breathengine.h \
    $$PWD/src/audio.h \
    $$PWD/src/schedule.h \
    $$PWD/src/dbsync.h \
    $$PWD/src/themes.h \
    $$PWD/src/icons.h \
    $$PWD/src/i18n.h
