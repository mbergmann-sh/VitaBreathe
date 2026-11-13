// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "reportprinter.h"
#include "breathmodules.h"
#include "database.h"

#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QLocale>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QTextDocument>
#include <QUrl>

// Übersetzungskontext "ReportPrinter" (für lupdate erkennbar)
struct ReportTr { Q_DECLARE_TR_FUNCTIONS(ReportPrinter) };

namespace ReportPrinter {

namespace {
QString esc(const QString &s) { return s.toHtmlEscaped(); }

void setupPage(QPrinter *printer)
{
    printer->setPageSize(QPageSize(QPageSize::A4));
    printer->setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
}
} // namespace

void render(QPrinter *printer, const User &user, const ChartSpec &spec, const Options &o)
{
    QTextDocument doc;
    const QRectF page = printer->pageLayout().paintRectPixels(printer->resolution());
    doc.setPageSize(page.size());
    QFont base(QStringLiteral("Sans Serif"));
    base.setPointSizeF(9);
    doc.setDefaultFont(base);

    // Kennzahlen
    int total = 0, minutes = 0, complete = 0;
    for (const SessionRecord &s : spec.sessions) {
        ++total;
        minutes += s.actualSec;
        if (s.completed) ++complete;
    }
    QString html = QStringLiteral("<html><body>");
    html += QStringLiteral("<h2 style='color:#1474BC'>%1</h2>").arg(ReportTr::tr("ViviaBreathe – Übungstagebuch"));
    html += QStringLiteral("<p><b>%1</b> &nbsp;·&nbsp; %2 %3 – %4<br/>%5</p>")
                .arg(esc(user.name), ReportTr::tr("Zeitraum:"), QLocale().toString(spec.from, QLocale::ShortFormat),
                     QLocale().toString(spec.to, QLocale::ShortFormat),
                     ReportTr::tr("%1 Übungseinheiten, davon %2 vollständig · zusammen %3 Minuten")
                         .arg(total).arg(complete).arg(minutes / 60));

    if (o.chart) {
        const QSize sz(int(page.width()), int(page.width() * 0.62));
        QImage img(sz, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::white);
        {
            QPainter p(&img);
            QFont f = base;
            f.setPixelSize(qMax(10, sz.width() / 70));
            p.setFont(f);
            p.setPen(Qt::black);
            ChartRenderer::paint(p, QRectF(QPointF(0, 0), QSizeF(sz)).adjusted(4, 4, -4, -4), spec, false);
        }
        doc.addResource(QTextDocument::ImageResource, QUrl(QStringLiteral("vb://chart")), img);
        html += QStringLiteral("<p><img src='vb://chart' width='%1'/></p>").arg(sz.width());
    }
    if (o.table) {
        html += QStringLiteral("<table border='1' cellspacing='0' cellpadding='3' width='100%' style='border-color:#999'>"
                               "<thead><tr style='background-color:#e8f1f8'>");
        const QStringList heads = { ReportTr::tr("Datum / Uhrzeit"), ReportTr::tr("Übung"), ReportTr::tr("Anlass"), ReportTr::tr("Muster"), ReportTr::tr("Dauer"),
                                    ReportTr::tr("Zyklen"), ReportTr::tr("SpO₂ vor/nach"), ReportTr::tr("Peak Flow vor/nach"), ReportTr::tr("Puls vor/nach"),
                                    ReportTr::tr("Befinden vor/nach"), ReportTr::tr("Atemtrainer") };
        for (const QString &h : heads) html += QStringLiteral("<th>%1</th>").arg(esc(h));
        html += QStringLiteral("</tr></thead>");
        auto device = [](const SessionRecord &r) {
            QString t;
            if (r.deviceBalls >= 0) t = ReportTr::tr("%n Ball/Bälle", nullptr, r.deviceBalls);
            if (r.deviceHoldSec >= 0) t += (t.isEmpty() ? QString() : QStringLiteral(", ")) + QStringLiteral("%1 s").arg(r.deviceHoldSec);
            return t;
        };
        auto pair = [](int a, int b) {
            if (a < 0 && b < 0) return QString();
            return QStringLiteral("%1 / %2").arg(a >= 0 ? QString::number(a) : QStringLiteral("–"),
                                                 b >= 0 ? QString::number(b) : QStringLiteral("–"));
        };
        for (const SessionRecord &s : spec.sessions) {
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td align='right'>%5%6</td>"
                                   "<td align='right'>%7</td><td align='center'>%8</td><td align='center'>%9</td>"
                                   "<td align='center'>%10</td><td align='center'>%11</td><td align='center'>%12</td></tr>")
                        .arg(QLocale().toString(s.started, QStringLiteral("ddd dd.MM.yyyy HH:mm")),
                             esc(BreathModules::displayName(s.moduleId)), esc(s.eventName), esc(s.pattern),
                             formatDuration(s.actualSec), s.completed ? QString() : QStringLiteral(" *"),
                             QString::number(s.cycles), pair(s.spo2Before, s.spo2After), pair(s.pfBefore, s.pfAfter))
                        .arg(pair(s.pulseBefore, s.pulseAfter), pair(s.feelingBefore, s.feelingAfter), device(s));
            if (o.notes && !s.note.isEmpty())
                html += QStringLiteral("<tr><td></td><td colspan='10'><i>%1</i></td></tr>").arg(esc(s.note));
        }
        html += QStringLiteral("</table><p style='font-size:7pt'>%1</p>")
                    .arg(ReportTr::tr("* vorzeitig beendet. SpO₂ in %, Peak Flow in l/min, Puls in Schlägen/min, Befinden 1–10."));
    }
    html += QStringLiteral("<p style='font-size:7pt;color:#666'>%1</p></body></html>")
                .arg(ReportTr::tr("Erstellt mit ViviaBreathe %1 am %2. ViviaBreathe ist ein persönliches Übungs-Tagebuch und kein "
                        "Medizinprodukt.").arg(vbVersion(), QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat)));
    doc.setHtml(html);
    doc.print(printer);
}

void preview(QWidget *parent, const User &user, const ChartSpec &spec, const Options &o)
{
    QPrinter printer(QPrinter::HighResolution);
    setupPage(&printer);
    QPrintPreviewDialog dlg(&printer, parent);
    dlg.setWindowTitle(ReportTr::tr("Druckvorschau – Übungstagebuch"));
    dlg.resize(900, 1000);
    QObject::connect(&dlg, &QPrintPreviewDialog::paintRequested, &dlg,
                     [&](QPrinter *pr) { render(pr, user, spec, o); });
    dlg.exec();
}

bool savePdf(QWidget *, const User &user, const ChartSpec &spec, const Options &o, const QString &file)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(file);
    printer.setDocName(ReportTr::tr("ViviaBreathe – Übungstagebuch"));
    printer.setCreator(QStringLiteral("ViviaBreathe %1").arg(vbVersion()));
    setupPage(&printer);
    render(&printer, user, spec, o);
    return QFile::exists(file);
}

} // namespace ReportPrinter
