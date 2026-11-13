// ViviaBreathe – Übersetzungen (Quellsprache Deutsch, Übersetzung Englisch), Aufbau wie in VitaParms
//
// Die Sprache steht in den Einstellungen unter "ui/language" ("de" oder "en", Vorgabe: Deutsch).
// Der Windows-Installer setzt sie passend zur gewählten Installationssprache.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include <QString>

class QCoreApplication;

namespace I18n {
// Lädt die Übersetzungen (ViviaBreathe + Qt-Standarddialoge). Vor dem ersten Fenster aufrufen.
QString install(QCoreApplication &app);
QString language();                 // gespeicherte Sprache: "de" oder "en"
void setLanguage(const QString &lang);
bool isEnglish();
void restartApplication();
}
