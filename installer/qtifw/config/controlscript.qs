// ViviaBreathe – Steuerskript für den Qt-Installer-Framework-Installer (deutsch und englisch)
//
// Aktualisieren mit einem neuen Offline-Installer: Ist ViviaBreathe schon installiert, wird die
// alte Version vor der Installation über ihr Wartungsprogramm entfernt ("purge") und die neue
// in denselben Ordner installiert. Datenbank und Einstellungen liegen im Benutzerprofil und
// bleiben dabei erhalten. Beide Sprachversionen des Installers erkennen sich gegenseitig.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

var kTools = ["ViviaBreathe-Wartung.exe", "ViviaBreathe-Maintenance.exe"];

// Bis Version 0.4.0 hieß das Programm "VitaBreathe" (Vorgabe-Ordner C:/VitaBreathe)
var kLegacyDir = "C:/VitaBreathe";
var kLegacyTools = ["VitaBreathe-Wartung.exe", "VitaBreathe-Maintenance.exe"];

function legacyTool()
{
    for (var i = 0; i < kLegacyTools.length; ++i) {
        if (installer.fileExists(kLegacyDir + "/" + kLegacyTools[i]))
            return kLegacyDir + "/" + kLegacyTools[i];
    }
    return "";
}

// Vorgängerprogramm VitaBreathe anbieten zu entfernen. Datenbank und Einstellungen liegen im
// Benutzerprofil, bleiben erhalten und werden von ViviaBreathe beim ersten Start übernommen.
var legacyAsked = false;

function offerLegacyRemoval()
{
    if (legacyAsked)
        return;
    legacyAsked = true;
    var tool = legacyTool();
    if (tool === "")
        return;
    var text = isGerman()
        ? "Die Vorgängerversion \"VitaBreathe\" ist in " + kLegacyDir + " installiert.\n"
          + "Soll sie entfernt werden? Ihre Datenbank und Ihre Einstellungen bleiben erhalten und "
          + "werden von ViviaBreathe beim ersten Start übernommen."
        : "The previous version \"VitaBreathe\" is installed in " + kLegacyDir + ".\n"
          + "Do you want to remove it? Your database and your settings are kept and will be "
          + "taken over by ViviaBreathe when it is first started.";
    var answer = QMessageBox.question("viviabreathe.legacy", "ViviaBreathe", text,
                                      QMessageBox.Yes | QMessageBox.No);
    if (answer === QMessageBox.Yes)
        installer.execute(tool, ["purge", "-c"]);
}

function Controller()
{
}

function isGerman()
{
    return String(installer.value("UILanguage")).indexOf("de") === 0;
}

// Vorhandene Installation im (Vorgabe-)Zielordner suchen. Den Uninstall-Schlüssel in der
// Registrierung kann das Skript nicht nutzen: IFW benennt ihn nach einer zufälligen UUID.
function previousTool()
{
    var dir = installer.value("TargetDir");
    for (var i = 0; i < kTools.length; ++i) {
        if (installer.fileExists(dir + "/" + kTools[i]))
            return dir + "/" + kTools[i];
    }
    return "";
}

Controller.prototype.IntroductionPageCallback = function()
{
    if (!installer.isInstaller())
        return;
    offerLegacyRemoval();
    var tool = previousTool();
    if (tool === "")
        return;
    var dir = installer.value("TargetDir");
    var text = isGerman()
        ? "ViviaBreathe ist bereits in " + dir + " installiert.\n"
          + "Die vorhandene Version wird jetzt entfernt und anschließend die neue installiert. "
          + "Ihre Datenbank und Ihre Einstellungen bleiben erhalten."
        : "ViviaBreathe is already installed in " + dir + ".\n"
          + "The existing version will now be removed and the new one installed afterwards. "
          + "Your database and your settings are kept.";
    var answer = QMessageBox.question("viviabreathe.update", "ViviaBreathe", text,
                                      QMessageBox.Ok | QMessageBox.Cancel);
    if (answer !== QMessageBox.Ok) {
        installer.setCanceled();
        return;
    }
    installer.execute(tool, ["purge", "-c"]);
    installer.setValue("TargetDir", dir);
}
