// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "i18n.h"
#include "qtcompat.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QProcess>
#include <QSettings>
#include <QTranslator>

namespace I18n {

// Immer die Einstellungen des normalen Programms (auch im Demo-Modus mit eigenem Namen)
QString language()
{
    const QString l = QSettings(QCoreApplication::organizationName(), QStringLiteral("ViviaBreathe"))
                          .value(QStringLiteral("ui/language"), QStringLiteral("de")).toString().left(2).toLower();
    return l == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("de");
}

void setLanguage(const QString &lang)
{
    QSettings(QCoreApplication::organizationName(), QStringLiteral("ViviaBreathe"))
        .setValue(QStringLiteral("ui/language"), lang == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("de"));
}

bool isEnglish() { return language() == QLatin1String("en"); }

QString install(QCoreApplication &app)
{
    static QList<QTranslator *> s_installed;
    for (QTranslator *t : qAsConst(s_installed)) {
        app.removeTranslator(t);
        delete t;
    }
    s_installed.clear();
    const QString lang = language();
    const QLocale locale = lang == QLatin1String("en") ? QLocale(QLocale::English, QLocale::UnitedKingdom)
                                                       : QLocale(QLocale::German, QLocale::Germany);
    QLocale::setDefault(locale);

    auto *qtTr = new QTranslator(&app);
    if (qtTr->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"), vpLibraryPath(QLibraryInfo::TranslationsPath))
        || qtTr->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"), QCoreApplication::applicationDirPath()
                                                                                 + QStringLiteral("/translations")))
        app.installTranslator(qtTr);
    s_installed << qtTr;

    // Englisch: vollständige Übersetzung; Deutsch: nur die Mehrzahlformen der %n-Texte
    auto *appTr = new QTranslator(&app);
    if (appTr->load(QStringLiteral(":/i18n/viviabreathe_%1.qm").arg(lang)))
        app.installTranslator(appTr);
    s_installed << appTr;
    return lang;
}

void restartApplication()
{
    QStringList args = QCoreApplication::arguments();
    args.removeFirst();
    QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
    qApp->quit();
}

} // namespace I18n
