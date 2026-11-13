// ViviaBreathe – Symbole der Oberfläche
//
// Die Symbole liegen als einfarbige SVG-Strichzeichnungen in resources/icons/ui. Beim Laden
// wird die Strichfarbe an das aktuelle Theme angepasst (hell/dunkel), die Akzentfarbe ist das
// Orange des Programmsymbols. Nach einem Theme-Wechsel Icons::clearCache() aufrufen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QString>

namespace Icons {
QIcon get(const QString &name);
QIcon app();
// Illustration aus resources/illustrations (Haltungen, Anleitungen), an das Theme angepasst
QPixmap illustration(const QString &name, const QSize &size, qreal dpr = 2.0);
QColor accent();          // Orange
QColor primary();         // Blau
void clearCache();
// Nach einem Theme-Wechsel: Symbole aller Schaltflächen und Aktionen neu einfärben
void refreshWidgets();
}
