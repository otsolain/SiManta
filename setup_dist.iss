
#define MyAppName "Simanta"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Simanta Classroom"
#define MyAppURL "https://Simanta.id"
#define MyAppCopyright "(c) 2026 Simanta. All rights reserved."

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVerName={#MyAppName}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppCopyright={#MyAppCopyright}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=installer_output
OutputBaseFilename=Simanta
SetupIconFile=installer\logo.ico
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
WizardSizePercent=110,110
PrivilegesRequired=admin
DisableProgramGroupPage=yes
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\logo.ico
VersionInfoDescription={#MyAppName} - Classroom Management System

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"


[Files]
Source: "dist\teacher\*"; DestDir: "{app}\Teacher"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: IsTeacherInstall
Source: "dist\student\*"; DestDir: "{app}\Student"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: IsStudentInstall
Source: "installer\logo.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "installer\simanta-recovery.bat"; DestDir: "{app}"; Flags: ignoreversion


[Icons]
Name: "{group}\Simanta Teacher"; Filename: "{app}\Teacher\SimantaTeacher.exe"; Check: IsTeacherInstall; Comment: "Launch Teacher Console"
Name: "{autodesktop}\Simanta Teacher"; Filename: "{app}\Teacher\SimantaTeacher.exe"; Check: IsTeacherInstall; Comment: "Simanta Teacher Console"

Name: "{group}\Simanta Student"; Filename: "{app}\Student\SimantaStudent.exe"; Check: IsStudentInstall; Comment: "Launch Student Agent"

Name: "{group}\Uninstall Simanta"; Filename: "{uninstallexe}"


[Registry]
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "SimantaStudent"; ValueData: """{app}\Student\SimantaStudent.exe"""; Check: IsStudentInstall; Flags: uninsdeletevalue


[Run]
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""Simanta Teacher TCP"" dir=in action=allow program=""{app}\Teacher\SimantaTeacher.exe"" enable=yes profile=any protocol=tcp"; Check: IsTeacherInstall; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""Simanta Teacher UDP"" dir=in action=allow program=""{app}\Teacher\SimantaTeacher.exe"" enable=yes profile=any protocol=udp"; Check: IsTeacherInstall; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""Simanta Beacon Out"" dir=out action=allow protocol=udp localport=5401 enable=yes profile=any"; Check: IsTeacherInstall; Flags: runhidden

Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""Simanta Student TCP"" dir=in action=allow program=""{app}\Student\SimantaStudent.exe"" enable=yes profile=any protocol=tcp"; Check: IsStudentInstall; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""Simanta Student UDP"" dir=in action=allow program=""{app}\Student\SimantaStudent.exe"" enable=yes profile=any protocol=udp"; Check: IsStudentInstall; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""Simanta Discovery In"" dir=in action=allow protocol=udp localport=5401 enable=yes profile=any"; Check: IsStudentInstall; Flags: runhidden


Filename: "{app}\Teacher\SimantaTeacher.exe"; Description: "Jalankan Simanta Teacher sekarang"; Check: IsTeacherInstall; Flags: nowait postinstall skipifsilent unchecked
Filename: "{app}\Student\SimantaStudent.exe"; Description: "Jalankan Simanta Student sekarang"; Check: IsStudentInstall; Flags: nowait postinstall skipifsilent unchecked


[UninstallRun]
Filename: "taskkill"; Parameters: "/f /im SimantaTeacher.exe"; Flags: runhidden
Filename: "taskkill"; Parameters: "/f /im SimantaStudent.exe"; Flags: runhidden
; Wait a moment for processes to fully terminate
Filename: "cmd"; Parameters: "/c timeout /t 1 /nobreak >nul"; Flags: runhidden
; Remove firewall rules
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""Simanta Teacher TCP"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""Simanta Teacher UDP"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""Simanta Beacon Out"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""Simanta Student TCP"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""Simanta Student UDP"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""Simanta Discovery In"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""Simanta Block QUIC"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""Simanta Block TCP"""; Flags: runhidden
; Reset proxy/PAC settings that the student agent may have set
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\Microsoft\Windows\CurrentVersion\Internet Settings"" /v ProxySettingsPerUser /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\Microsoft\Windows\CurrentVersion\Internet Settings"" /v EnableLegacyAutoProxyFeatures /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings"" /v AutoConfigURL /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings"" /v AutoConfigURL /f"; Flags: runhidden
Filename: "reg"; Parameters: "add ""HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings"" /v ProxyEnable /t REG_DWORD /d 0 /f"; Flags: runhidden
Filename: "reg"; Parameters: "add ""HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings"" /v ProxyEnable /t REG_DWORD /d 0 /f"; Flags: runhidden
; Re-enable Task Manager, Registry Editor, Control Panel, USB
Filename: "reg"; Parameters: "delete ""HKLM\Software\Microsoft\Windows\CurrentVersion\Policies\System"" /v DisableTaskMgr /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Policies\System"" /v DisableTaskMgr /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\Software\Microsoft\Windows\CurrentVersion\Policies\System"" /v DisableRegistryTools /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Policies\System"" /v DisableRegistryTools /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer"" /v NoControlPanel /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer"" /v NoControlPanel /f"; Flags: runhidden
Filename: "reg"; Parameters: "add ""HKLM\System\CurrentControlSet\Services\USBSTOR"" /v Start /t REG_DWORD /d 3 /f"; Flags: runhidden
; Remove browser policy values Simanta wrote (keep the parent keys intact so
; other software's policy values aren't collateral damage).
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\Google\Chrome"" /v DnsOverHttpsMode /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\Google\Chrome"" /v QuicAllowed /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\Google\Chrome"" /v ProxySettings /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\Microsoft\Edge"" /v DnsOverHttpsMode /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\Microsoft\Edge"" /v QuicAllowed /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\Microsoft\Edge"" /v ProxySettings /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\BraveSoftware\Brave"" /v DnsOverHttpsMode /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\BraveSoftware\Brave"" /v QuicAllowed /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\Mozilla\Firefox"" /v DNSOverHTTPS /f"; Flags: runhidden
Filename: "reg"; Parameters: "delete ""HKLM\SOFTWARE\Policies\Mozilla\Firefox\Proxy"" /f"; Flags: runhidden


[UninstallDelete]
Type: filesandordirs; Name: "{app}"


[Code]
var
  RolePage: TWizardPage;
  TeacherRadio: TRadioButton;
  StudentRadio: TRadioButton;
  TeacherIPPage: TInputQueryWizardPage;
  RoleSelected: Integer; // 0 = teacher, 1 = student

function IsTeacherInstall(): Boolean;
begin
  Result := (RoleSelected = 0);
end;

function IsStudentInstall(): Boolean;
begin
  Result := (RoleSelected = 1);
end;

function GetUninstallString(): String;
var
  sUnInstPath: String;
  sUnInstallString: String;
begin
  Result := '';
  sUnInstPath := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}_is1';
  if RegQueryStringValue(HKLM, sUnInstPath, 'UninstallString', sUnInstallString) then
    Result := sUnInstallString
  else if RegQueryStringValue(HKCU, sUnInstPath, 'UninstallString', sUnInstallString) then
    Result := sUnInstallString;
end;

function GetInstalledVersion(): String;
var
  sUnInstPath: String;
  sVersion: String;
begin
  Result := '';
  sUnInstPath := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}_is1';
  if RegQueryStringValue(HKLM, sUnInstPath, 'DisplayVersion', sVersion) then
    Result := sVersion
  else if RegQueryStringValue(HKCU, sUnInstPath, 'DisplayVersion', sVersion) then
    Result := sVersion;
end;

function IsUpgrade(): Boolean;
begin
  Result := (GetUninstallString() <> '');
end;

function UninstallOldVersion(): Integer;
var
  sUnInstallString: String;
  iResultCode: Integer;
begin
  Result := 0;
  sUnInstallString := GetUninstallString();
  if sUnInstallString <> '' then
  begin
    sUnInstallString := RemoveQuotes(sUnInstallString);
    if Exec(sUnInstallString, '/SILENT /NORESTART /SUPPRESSMSGBOXES', '', SW_HIDE, ewWaitUntilTerminated, iResultCode) then
      Result := 0
    else
      Result := 1;
  end;
end;

function InitializeSetup(): Boolean;
var
  OldVersion: String;
  Msg: String;
  Answer: Integer;
begin
  Result := True;

  if IsUpgrade() then
  begin
    OldVersion := GetInstalledVersion();
    if OldVersion = '' then
      OldVersion := 'versi tidak diketahui';

    Msg := 'Simanta sudah terinstall di komputer ini.' + #13#10 + #13#10 +
           'Pilih tindakan:' + #13#10 +
           '  * Klik YA untuk update (hapus lama, install baru)' + #13#10 +
           '  * Klik TIDAK untuk membatalkan instalasi';

    Answer := MsgBox(Msg, mbConfirmation, MB_YESNO or MB_DEFBUTTON1);

    if Answer = IDYES then
    begin
      Exec('taskkill', '/f /im SimantaTeacher.exe', '', SW_HIDE, ewWaitUntilTerminated, Answer);
      Exec('taskkill', '/f /im SimantaStudent.exe', '', SW_HIDE, ewWaitUntilTerminated, Answer);
      Sleep(500);
      UninstallOldVersion();
      Sleep(1000);
      Result := True;
    end
    else
    begin
      Result := False; // Cancel installation
    end;
  end;
end;

procedure InitializeWizard();
var
  HeaderLabel: TLabel;
  DescLabel: TLabel;
  TeacherDesc: TLabel;
  StudentDesc: TLabel;
begin
  RoleSelected := 0; // Default: Teacher

  RolePage := CreateCustomPage(wpSelectDir,
    'Tipe Instalasi',
    'Pilih cara install Simanta di komputer ini.');

  HeaderLabel := TLabel.Create(RolePage);
  HeaderLabel.Parent := RolePage.Surface;
  HeaderLabel.Caption := 'Pilih Mode Instalasi:';
  HeaderLabel.Font.Size := 11;
  HeaderLabel.Font.Style := [fsBold];
  HeaderLabel.Left := 0;
  HeaderLabel.Top := 16;
  HeaderLabel.Width := RolePage.SurfaceWidth;

  TeacherRadio := TRadioButton.Create(RolePage);
  TeacherRadio.Parent := RolePage.Surface;
  TeacherRadio.Caption := 'Install sebagai Guru (Teacher)';
  TeacherRadio.Font.Size := 10;
  TeacherRadio.Font.Style := [fsBold];
  TeacherRadio.Left := 16;
  TeacherRadio.Top := 60;
  TeacherRadio.Width := RolePage.SurfaceWidth - 32;
  TeacherRadio.Height := 24;
  TeacherRadio.Checked := True;

  TeacherDesc := TLabel.Create(RolePage);
  TeacherDesc.Parent := RolePage.Surface;
  TeacherDesc.Caption :=
    'Install Teacher Console untuk memonitor dan mengontrol layar murid secara real-time. ' +
    'Gunakan ini di komputer guru.';
  TeacherDesc.WordWrap := True;
  TeacherDesc.Left := 36;
  TeacherDesc.Top := 88;
  TeacherDesc.Width := RolePage.SurfaceWidth - 52;
  TeacherDesc.Font.Color := clGray;

  StudentRadio := TRadioButton.Create(RolePage);
  StudentRadio.Parent := RolePage.Surface;
  StudentRadio.Caption := 'Install sebagai Murid (Student)';
  StudentRadio.Font.Size := 10;
  StudentRadio.Font.Style := [fsBold];
  StudentRadio.Left := 16;
  StudentRadio.Top := 140;
  StudentRadio.Width := RolePage.SurfaceWidth - 32;
  StudentRadio.Height := 24;

  StudentDesc := TLabel.Create(RolePage);
  StudentDesc.Parent := RolePage.Surface;
  StudentDesc.Caption :=
    'Install Student Agent yang berjalan di background. ' +
    'Mengirim screenshot layar ke Teacher Console secara otomatis. ' +
    'Gunakan ini di komputer setiap murid.';
  StudentDesc.WordWrap := True;
  StudentDesc.Left := 36;
  StudentDesc.Top := 168;
  StudentDesc.Width := RolePage.SurfaceWidth - 52;
  StudentDesc.Font.Color := clGray;

  DescLabel := TLabel.Create(RolePage);
  DescLabel.Parent := RolePage.Surface;
  DescLabel.Caption := 'Catatan: Hanya satu mode yang bisa diinstall per komputer.';
  DescLabel.Font.Style := [fsItalic];
  DescLabel.Font.Color := clGray;
  DescLabel.Left := 0;
  DescLabel.Top := 230;
  DescLabel.Width := RolePage.SurfaceWidth;

  TeacherIPPage := CreateInputQueryPage(RolePage.ID,
    'Konfigurasi Koneksi',
    'Konfigurasi cara Student terhubung ke Teacher.',
    'Masukkan IP address komputer guru. ' +
    'Biarkan "auto" untuk auto-discovery (otomatis menemukan guru di jaringan yang sama). ' +
    'Anda bisa mengubah ini nanti di config.ini.');
  TeacherIPPage.Add('IP Address Guru:', False);
  TeacherIPPage.Values[0] := 'auto';
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;

  if CurPageID = RolePage.ID then
  begin
    if TeacherRadio.Checked then
      RoleSelected := 0
    else
      RoleSelected := 1;
  end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = TeacherIPPage.ID then
    Result := IsTeacherInstall();
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ConfigFile: String;
  TeacherIP: String;
begin
  if CurStep = ssPostInstall then
  begin
    if IsStudentInstall() then
    begin
      TeacherIP := TeacherIPPage.Values[0];
      if (TeacherIP = '') or (TeacherIP = 'auto') then
        TeacherIP := '127.0.0.1'; // Will trigger auto-discovery mode

      ConfigFile := ExpandConstant('{app}\Student\config.ini');
      SetIniString('General', 'teacher_ip', TeacherIP, ConfigFile);
      SetIniString('General', 'port', '5400', ConfigFile);
      SetIniString('General', 'interval', '2000', ConfigFile);
      SetIniString('General', 'quality', '92', ConfigFile);
      SetIniString('General', 'scale', '1.0', ConfigFile);
    end;
  end;
end;

