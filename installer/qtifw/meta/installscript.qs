// ViviaBreathe – Verknüpfungen und Programmsprache
//
// Die Programmsprache wird passend zur Installationssprache gesetzt (Registrierung:
// HKCU\Software\mbergmann-sh\ViviaBreathe\ui\language = de bzw. en). In ViviaBreathe lässt sie
// sich jederzeit unter Extras → Einstellungen → Darstellung ändern.
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
function Component()
{
}

function isGerman()
{
    return String(installer.value("UILanguage")).indexOf("de") === 0;
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    if (systemInfo.productType !== "windows")
        return;

    var de = isGerman();
    // Programmsprache = Installationssprache
    component.addOperation("GlobalConfig", "UserScope", "mbergmann-sh", "ViviaBreathe", "ui/language", de ? "de" : "en");

    var tool = de ? "ViviaBreathe-Wartung.exe" : "ViviaBreathe-Maintenance.exe";
    component.addOperation("CreateShortcut", "@TargetDir@/ViviaBreathe.exe", "@StartMenuDir@/ViviaBreathe.lnk",
        "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/ViviaBreathe.exe", "iconId=0",
        de ? "description=Geführte Atemübungen mit Tagebuch" : "description=Guided breathing exercises with diary");
    component.addOperation("CreateShortcut", "@TargetDir@/ViviaBreathe.exe",
        de ? "@StartMenuDir@/ViviaBreathe - Demo.lnk" : "@StartMenuDir@/ViviaBreathe - demo.lnk", "--demo",
        "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/ViviaBreathe.exe", "iconId=0",
        de ? "description=Demo mit erfundenen Daten (Master-Passwort: demo)"
           : "description=Demo with invented data (master password: demo)");
    component.addOperation("CreateShortcut", "@TargetDir@/" + tool,
        de ? "@StartMenuDir@/ViviaBreathe aktualisieren oder entfernen.lnk"
           : "@StartMenuDir@/Update or remove ViviaBreathe.lnk",
        "workingDirectory=@TargetDir@",
        de ? "description=ViviaBreathe aktualisieren oder deinstallieren" : "description=Update or uninstall ViviaBreathe");
    component.addOperation("CreateShortcut", "@TargetDir@/ViviaBreathe.exe", "@DesktopDir@/ViviaBreathe.lnk",
        "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/ViviaBreathe.exe", "iconId=0");
}
