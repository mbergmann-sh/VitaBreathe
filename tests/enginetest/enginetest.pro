# Test des Taktgebers (Phasen, Abschnitte, Warten auf den Benutzer, Ende nach der Runde)
QT += core gui widgets network svg
qtHaveModule(multimedia): QT += multimedia
CONFIG += c++17 console
TARGET = enginetest
DEFINES += VIVIABREATHE_VERSION=\\\"test\\\"
include(../../core.pri)
SOURCES += main.cpp
