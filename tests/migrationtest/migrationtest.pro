# Test der Datenbank-Migration: Format 1 (ViviaBreathe 0.2) → aktuelles Format
QT += core gui widgets network svg
qtHaveModule(multimedia): QT += multimedia
CONFIG += c++17 console
TARGET = migrationtest
DEFINES += VIVIABREATHE_VERSION=\\\"test\\\"
include(../../core.pri)
SOURCES += main.cpp
