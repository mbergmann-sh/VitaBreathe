// VitaBreathe – Verknüpfungen und Programmsprache
//
// Die Programmsprache wird passend zur Installationssprache gesetzt (Registrierung:
// HKCU\Software\mbergmann-sh\VitaBreathe\ui\language = de bzw. en). In VitaBreathe lässt sie
// sich jederzeit unter Extras → Einstellungen → Darstellung ändern.
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
    component.addOperation("GlobalConfig", "UserScope", "mbergmann-sh", "VitaBreathe", "ui/language", de ? "de" : "en");

    var tool = de ? "VitaBreathe-Wartung.exe" : "VitaBreathe-Maintenance.exe";
    component.addOperation("CreateShortcut", "@TargetDir@/VitaBreathe.exe", "@StartMenuDir@/VitaBreathe.lnk",
        "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/VitaBreathe.exe", "iconId=0",
        de ? "description=Geführte Atemübungen mit Tagebuch" : "description=Guided breathing exercises with diary");
    component.addOperation("CreateShortcut", "@TargetDir@/VitaBreathe.exe",
        de ? "@StartMenuDir@/VitaBreathe - Demo.lnk" : "@StartMenuDir@/VitaBreathe - demo.lnk", "--demo",
        "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/VitaBreathe.exe", "iconId=0",
        de ? "description=Demo mit erfundenen Daten (Master-Passwort: demo)"
           : "description=Demo with invented data (master password: demo)");
    component.addOperation("CreateShortcut", "@TargetDir@/" + tool,
        de ? "@StartMenuDir@/VitaBreathe aktualisieren oder entfernen.lnk"
           : "@StartMenuDir@/Update or remove VitaBreathe.lnk",
        "workingDirectory=@TargetDir@",
        de ? "description=VitaBreathe aktualisieren oder deinstallieren" : "description=Update or uninstall VitaBreathe");
    component.addOperation("CreateShortcut", "@TargetDir@/VitaBreathe.exe", "@DesktopDir@/VitaBreathe.lnk",
        "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/VitaBreathe.exe", "iconId=0");
}
