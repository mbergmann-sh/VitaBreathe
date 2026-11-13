// ViviaBreathe – Verlauf drucken (DIN A4) und als PDF speichern
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include "chartwidget.h"
#include "datatypes.h"

class QWidget;
class QPrinter;

namespace ReportPrinter {
struct Options {
    bool chart = true;
    bool table = true;
    bool notes = true;
};
void preview(QWidget *parent, const User &user, const ChartSpec &spec, const Options &o);
bool savePdf(QWidget *parent, const User &user, const ChartSpec &spec, const Options &o, const QString &file);
void render(QPrinter *printer, const User &user, const ChartSpec &spec, const Options &o);
}
