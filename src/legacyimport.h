// ViviaBreathe – Übernahme aus „VitaBreathe“ (so hieß das Programm bis Version 0.4.0)
//
// Beim ersten Start nach der Umbenennung werden einmalig
//  * die Einstellungen (QSettings „mbergmann-sh/VitaBreathe“: Sprache, Fensterlage, Abgleich,
//    Datenbankpfad …) übernommen – nur Schlüssel, die unter dem neuen Namen noch fehlen; die vom
//    Installer gesetzte Sprache bleibt also erhalten;
//  * die Datenbank aus dem alten Datenordner in den neuen kopiert, sofern sie am alten Standardort
//    lag. Lag sie an einem selbst gewählten Ort, bleibt sie dort und wird weiter von dort geöffnet.
// Die alten Dateien und Einstellungen bleiben unverändert als Sicherung liegen.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#pragma once

#include <QString>

namespace LegacyImport {

// Vor I18n::install und vor dem Öffnen der Datenbank aufrufen (nicht im Demo-Modus).
// Gibt eine kurze Beschreibung zurück, was übernommen wurde (leer, wenn nichts zu tun war).
QString run();

// Für Tests: dasselbe mit frei wählbaren Namen und Ordnern
QString run(const QString &organization, const QString &oldApp, const QString &newApp,
            const QString &oldDataDir, const QString &newDataDir,
            const QString &oldDbName, const QString &newDbName);

} // namespace LegacyImport
