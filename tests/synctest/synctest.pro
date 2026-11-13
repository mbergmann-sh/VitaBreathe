# Test des Datenbank-Abgleichs gegen einen lokalen PHP-Server (php -S 127.0.0.1:8099 -t website)
QT += core gui widgets network svg
qtHaveModule(multimedia): QT += multimedia
CONFIG += c++17 console
TARGET = synctest
DEFINES += VIVIABREATHE_VERSION=\\\"test\\\"
include(../../core.pri)
SOURCES += main.cpp
