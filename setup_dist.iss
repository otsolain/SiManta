; ================================================================
;  SiManta - Classroom Management System
;  Inno Setup 6 Installer Script
;
;  Features:
;  - Detects existing installation → offers Uninstall or Update
;  - Radio button: Install as Teacher or Student
;  - Teacher IP configuration (student only, default: auto-discovery)
;  - Firewall rules for TCP (5400) and UDP (5401) discovery
;  - Auto-start student agent on login
;  - Proper uninstall cleanup (processes, firewall, registry)
; ================================================================

#define MyAppName "Simanta"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Simanta Classroom"
#define MyAppURL "https://simanta.id"
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
UninstallDisplayIcon={app}\logo.png
VersionInfoDescription={#MyAppName} - Classroom Management System

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

; ================================================================
;  Files - Both are always included, we select at runtime
; ================================================================

[Files]
; Teacher Console files
Source: "dist\teacher\*"; DestDir: "{app}\Teacher"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: IsTeacherInstall
; Student Agent files
Source: "dist\student\*"; DestDir: "{app}\Student"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: IsStudentInstall
; Logo for uninstall display
Source: "installer\logo.png"; DestDir: "{app}"; Flags: ignoreversion
Source: "installer\logo.ico"; DestDir: "{app}"; Flags: ignoreversion

; ================================================================
;  Start Menu & Desktop Shortcuts
; ================================================================

[Icons]
; Teacher shortcuts
Name: "{group}\SiManta Teacher"; Filename: "{app}\Teacher\SiMantaTeacher.exe"; Check: IsTeacherInstall; Comment: "Launch Teacher Console"
Name: "{autodesktop}\SiManta Teacher"; Filename: "{app}\Teacher\SiMantaTeacher.exe"; Check: IsTeacherInstall; Comment: "SiManta Teacher Console"

; Student shortcuts
Name: "{group}\SiManta Student"; Filename: "{app}\Student\SiMantaStudent.exe"; Check: IsStudentInstall; Comment: "Launch Student Agent"

; Uninstaller
Name: "{group}\Uninstall SiManta"; Filename: "{uninstallexe}"

; ================================================================
;  Registry - Auto-start Student Agent on login
; ================================================================

[Registry]
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "SiMantaStudent"; ValueData: """{app}\Student\SiMantaStudent.exe"""; Check: IsStudentInstall; Flags: uninsdeletevalue

; ================================================================
;  Post-Install: Launch App & Add Firewall Rules
; ================================================================

[Run]
; Firewall rules for Teacher (TCP + UDP)
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""SiManta Teacher TCP"" dir=in action=allow program=""{app}\Teacher\SiMantaTeacher.exe"" enable=yes profile=any protocol=tcp"; Check: IsTeacherInstall; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""SiManta Teacher UDP"" dir=in action=allow program=""{app}\Teacher\SiMantaTeacher.exe"" enable=yes profile=any protocol=udp"; Check: IsTeacherInstall; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""SiManta Beacon Out"" dir=out action=allow protocol=udp localport=5401 enable=yes profile=any"; Check: IsTeacherInstall; Flags: runhidden

; Firewall rules for Student (TCP + UDP)
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""SiManta Student TCP"" dir=in action=allow program=""{app}\Student\SiMantaStudent.exe"" enable=yes profile=any protocol=tcp"; Check: IsStudentInstall; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""SiManta Student UDP"" dir=in action=allow program=""{app}\Student\SiMantaStudent.exe"" enable=yes profile=any protocol=udp"; Check: IsStudentInstall; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""SiManta Discovery In"" dir=in action=allow protocol=udp localport=5401 enable=yes profile=any"; Check: IsStudentInstall; Flags: runhidden

; (Task Scheduler removed to prevent invisible background process)

; Launch after install
Filename: "{app}\Teacher\SiMantaTeacher.exe"; Description: "Jalankan SiManta Teacher sekarang"; Check: IsTeacherInstall; Flags: nowait postinstall skipifsilent unchecked
Filename: "{app}\Student\SiMantaStudent.exe"; Description: "Jalankan SiManta Student sekarang"; Check: IsStudentInstall; Flags: nowait postinstall skipifsilent unchecked

; ================================================================
;  Uninstall Cleanup
; ================================================================

[UninstallRun]
; Kill running processes
Filename: "taskkill"; Parameters: "/f /im SiMantaTeacher.exe"; Flags: runhidden
Filename: "taskkill"; Parameters: "/f /im SiMantaStudent.exe"; Flags: runhidden
; Remove ALL SiManta firewall rules
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""SiManta Teacher TCP"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""SiManta Teacher UDP"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""SiManta Beacon Out"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""SiManta Student TCP"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""SiManta Student UDP"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""SiManta Discovery In"""; Flags: runhidden


[UninstallDelete]
Type: filesandordirs; Name: "{app}"

; ================================================================
;  Pascal Script
;  - Detect existing installation
;  - Role selection (Teacher / Student)
;  - Teacher IP config for student
;  - Write config.ini
; ================================================================

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

// ── Detect existing installation and offer uninstall/update ──
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

    Msg := 'SiManta sudah terinstall di komputer ini.' + #13#10 + #13#10 +
           'Pilih tindakan:' + #13#10 +
           '  • Klik YA untuk update (hapus lama, install baru)' + #13#10 +
           '  • Klik TIDAK untuk membatalkan instalasi';

    Answer := MsgBox(Msg, mbConfirmation, MB_YESNO or MB_DEFBUTTON1);

    if Answer = IDYES then
    begin
      // Kill running SiManta processes first
      Exec('taskkill', '/f /im SiMantaTeacher.exe', '', SW_HIDE, ewWaitUntilTerminated, Answer);
      Exec('taskkill', '/f /im SiMantaStudent.exe', '', SW_HIDE, ewWaitUntilTerminated, Answer);
      // Small delay for processes to terminate
      Sleep(500);
      // Uninstall old version silently
      UninstallOldVersion();
      // Small delay for uninstaller to finish
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

  // ════════════════════════════════════════════════
  //  Role Selection Page (Teacher / Student)
  // ════════════════════════════════════════════════
  RolePage := CreateCustomPage(wpSelectDir,
    'Tipe Instalasi',
    'Pilih cara install SiManta di komputer ini.');

  // Header
  HeaderLabel := TLabel.Create(RolePage);
  HeaderLabel.Parent := RolePage.Surface;
  HeaderLabel.Caption := 'Pilih Mode Instalasi:';
  HeaderLabel.Font.Size := 11;
  HeaderLabel.Font.Style := [fsBold];
  HeaderLabel.Left := 0;
  HeaderLabel.Top := 16;
  HeaderLabel.Width := RolePage.SurfaceWidth;

  // --- Teacher Radio ---
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

  // --- Student Radio ---
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

  // Note
  DescLabel := TLabel.Create(RolePage);
  DescLabel.Parent := RolePage.Surface;
  DescLabel.Caption := 'Catatan: Hanya satu mode yang bisa diinstall per komputer.';
  DescLabel.Font.Style := [fsItalic];
  DescLabel.Font.Color := clGray;
  DescLabel.Left := 0;
  DescLabel.Top := 230;
  DescLabel.Width := RolePage.SurfaceWidth;

  // ════════════════════════════════════════════════
  //  Teacher IP Config (Student only)
  // ════════════════════════════════════════════════
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

  // Save the radio selection when leaving the role page
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
  // Skip Teacher IP page if installing as Teacher (teacher doesn't need IP config)
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
    // Write config.ini for Student Agent with teacher IP
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
