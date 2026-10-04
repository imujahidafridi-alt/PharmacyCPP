; Inno Setup Script for Pakistani Pharmacy & Retail POS
#define MyAppName "Pak Pharmacy & Retail POS"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Bismillah Technologies"
#define MyAppExeName "PakPharmacyPOS.exe"

[Setup]
AppId={{D37B47E3-5599-4A7B-9E33-9F93F8D34091}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\PakPharmacyPOS
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\installer_output
OutputBaseFilename=PakPharmacyPOS_Setup_v1.0.0
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\dist\PakPharmacyPOS\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\PakPharmacyPOS\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
