; Built only through the authorized scripts/build.ps1 entry point.
#ifndef AppExe
  #error AppExe must point to the validated Release executable.
#endif
#define AppVersion GetVersionNumbersString(AppExe)

[Setup]
AppId={{631415DA-CA78-4BD7-92DA-17EE17376894}
AppName=MouseCursorDetector
AppVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\MouseCursorDetector
DefaultGroupName=MouseCursorDetector
PrivilegesRequired=lowest
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0
DisableDirPage=no
DisableProgramGroupPage=yes
OutputBaseFilename=MouseCursorDetector-Setup
SetupIconFile=..\src\mouse_cursor_detector\resources\app.ico
Uninstallable=yes
UninstallFilesDir={app}
UninstallDisplayIcon={app}\MouseCursorDetector.exe
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
AppMutex=Local\MouseCursorDetector.SingleInstance.v1
CloseApplications=no
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Never package the developer's config/ or styles/ directories.
Source: "{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
; Remove legacy settings on uninstall without creating them during installation.
Root: HKCU; Subkey: "Software\MouseCursorDetector"; Flags: dontcreatekey uninsdeletekey

[UninstallDelete]
; These two directories contain this installation's runtime data only.
; Never recursively delete {app}: it may contain unrelated user files.
Type: filesandordirs; Name: "{app}\config"
Type: filesandordirs; Name: "{app}\styles"
Type: dirifempty; Name: "{app}"

[Icons]
Name: "{userdesktop}\MouseCursorDetector"; Filename: "{app}\MouseCursorDetector.exe"; WorkingDir: "{app}"
Name: "{userprograms}\MouseCursorDetector\MouseCursorDetector"; Filename: "{app}\MouseCursorDetector.exe"; WorkingDir: "{app}"
Name: "{userprograms}\MouseCursorDetector\Uninstall MouseCursorDetector"; Filename: "{uninstallexe}"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  StartupCommand: String;
begin
  if CurUninstallStep = usUninstall then
  begin
    { Only remove startup registration owned by this installed copy. }
    if RegQueryStringValue(HKCU,
      'Software\Microsoft\Windows\CurrentVersion\Run',
      'MouseCursorDetector', StartupCommand) then
    begin
      if CompareText(StartupCommand,
        '"' + ExpandConstant('{app}\MouseCursorDetector.exe') + '" --autostart') = 0 then
        RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run',
          'MouseCursorDetector');
    end;
  end;
end;
