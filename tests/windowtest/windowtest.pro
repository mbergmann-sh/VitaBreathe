# Test: Fensterpositionen und -größen werden gemerkt und wiederhergestellt
QT += core gui widgets
CONFIG += c++17 console
TARGET = windowtest
INCLUDEPATH += ../../src
SOURCES += main.cpp ../../src/windowstate.cpp ../../src/screenfit.cpp
HEADERS += ../../src/windowstate.h ../../src/screenfit.h
