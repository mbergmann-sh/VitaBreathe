# Test der Datenbank-Migration: Format 1 (VitaBreathe 0.2) → aktuelles Format
QT += core gui widgets network svg
qtHaveModule(multimedia): QT += multimedia
CONFIG += c++17 console
TARGET = migrationtest
DEFINES += VITABREATHE_VERSION=\\\"test\\\"
include(../../core.pri)
SOURCES += main.cpp
