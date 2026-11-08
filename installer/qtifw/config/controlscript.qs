// VitaBreathe – Steuerskript für den Qt-Installer-Framework-Installer (deutsch und englisch)
//
// Aktualisieren mit einem neuen Offline-Installer: Ist VitaBreathe schon installiert, wird die
// alte Version vor der Installation über ihr Wartungsprogramm entfernt ("purge") und die neue
// in denselben Ordner installiert. Datenbank und Einstellungen liegen im Benutzerprofil und
// bleiben dabei erhalten. Beide Sprachversionen des Installers erkennen sich gegenseitig.

var kTools = ["VitaBreathe-Wartung.exe", "VitaBreathe-Maintenance.exe"];

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
    var tool = previousTool();
    if (tool === "")
        return;
    var dir = installer.value("TargetDir");
    var text = isGerman()
        ? "VitaBreathe ist bereits in " + dir + " installiert.\n"
          + "Die vorhandene Version wird jetzt entfernt und anschließend die neue installiert. "
          + "Ihre Datenbank und Ihre Einstellungen bleiben erhalten."
        : "VitaBreathe is already installed in " + dir + ".\n"
          + "The existing version will now be removed and the new one installed afterwards. "
          + "Your database and your settings are kept.";
    var answer = QMessageBox.question("vitabreathe.update", "VitaBreathe", text,
                                      QMessageBox.Ok | QMessageBox.Cancel);
    if (answer !== QMessageBox.Ok) {
        installer.setCanceled();
        return;
    }
    installer.execute(tool, ["purge", "-c"]);
    installer.setValue("TargetDir", dir);
}
