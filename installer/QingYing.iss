; 青影安装脚本：由 scripts/package.ps1 调用 ISCC 编译
#define AppName "青影"
#ifndef AppVersion
  #define AppVersion "0.7.3"
#endif
#define AppExe "QingYing.exe"
#ifndef AppArchitecture
  #define AppArchitecture "x64"
#endif
#ifndef AppSourceDir
  #error Use scripts/package.ps1 to prepare the release payload.
#endif
#ifndef OutputDirectory
  #define OutputDirectory "..\dist"
#endif

[Setup]
AppId={{5B1E7C2A-9F43-4D6B-8A17-3C0E6F92D4B8}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=QingYing
AppPublisherURL=https://github.com/nullTerminatedCat/QingYing
AppSupportURL=https://github.com/nullTerminatedCat/QingYing/issues
DefaultDirName={autopf}\QingYing
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir={#OutputDirectory}
OutputBaseFilename=QingYing-{#AppVersion}-windows-{#AppArchitecture}-setup
SetupIconFile=..\resources\QingYing.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName} {#AppVersion}
LicenseFile=..\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; 默认仅为当前用户安装（无需管理员），可在对话框中改为所有用户
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加选项:"; Flags: unchecked
Name: "autostart"; Description: "开机时自动启动青影"; GroupDescription: "附加选项:"

[Files]
Source: "{#AppSourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\卸载{#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "QingYing"; ValueData: """{app}\{#AppExe}"""; Tasks: autostart

[Run]
Filename: "{app}\{#AppExe}"; Description: "立即启动{#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
procedure KillRunning();
var
  Code: Integer;
begin
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM {#AppExe}', '', SW_HIDE, ewWaitUntilTerminated, Code);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  KillRunning();
  Result := '';
end;

function InitializeUninstall(): Boolean;
begin
  KillRunning();
  Result := True;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    // 无论安装时是否勾选，程序设置里也可能写入了自启项，卸载时一并删除
    RegDeleteValue(HKEY_CURRENT_USER, 'Software\Microsoft\Windows\CurrentVersion\Run', 'QingYing');
end;
