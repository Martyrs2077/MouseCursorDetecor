; Built only through the user-invoked scripts/build.ps1 entry point.
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
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=MouseCursorDetector-Setup
SetupIconFile=..\src\mouse_cursor_detector\resources\app.ico
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
{ Runtime-created config/ and styles/ are intentionally retained on uninstall. }
