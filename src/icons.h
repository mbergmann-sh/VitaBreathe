// VitaBreathe – Symbole der Oberfläche
//
// Die Symbole liegen als einfarbige SVG-Strichzeichnungen in resources/icons/ui. Beim Laden
// wird die Strichfarbe an das aktuelle Theme angepasst (hell/dunkel), die Akzentfarbe ist das
// Orange des Programmsymbols. Nach einem Theme-Wechsel Icons::clearCache() aufrufen.
// SPDX-License-Identifier: MIT
#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace Icons {
QIcon get(const QString &name);
QIcon app();
QColor accent();          // Orange
QColor primary();         // Blau
void clearCache();
// Nach einem Theme-Wechsel: Symbole aller Schaltflächen und Aktionen neu einfärben
void refreshWidgets();
}
