
#define MyAppName "SiManta"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "SiManta"
#define MyAppURL "https://simanta.id"
#define MyAppCopyright "(c) 2026 SiManta. All rights reserved."

[Setup]
AppId={{A7D3C4E1-B2F5-4A89-8C6D-3E7F1A2B9D0C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppCopyright={#MyAppCopyright}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\dist\installer
OutputBaseFilename=SiManta-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=logo.ico
WizardStyle=modern
WizardSizePercent=110,110
DisableWelcomePage=no
LicenseFile=license.txt
PrivilegesRequired=admin
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\logo.ico
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Classroom Management Installer
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\dist\teacher\*"; DestDir: "{app}\Teacher"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: IsTeacherInstall
Source: "..\dist\student\*"; DestDir: "{app}\Student"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: IsStudentInstall
Source: "logo.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\SiManta - Teacher Console"; Filename: "{app}\Teacher\SiMantaTeacher.exe"; Check: IsTeacherInstall; Comment: "Launch Teacher Console"; IconFilename: "{app}\logo.ico"
Name: "{autodesktop}\SiManta - Teacher"; Filename: "{app}\Teacher\SiMantaTeacher.exe"; Check: IsTeacherInstall; Comment: "SiManta Teacher Console"

Name: "{group}\SiManta - Student Agent"; Filename: "{app}\Student\SiMantaStudent.exe"; Check: IsStudentInstall; Comment: "Launch Student Agent"

Name: "{group}\Uninstall SiManta"; Filename: "{uninstallexe}"

[Registry]
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "SiMantaStudent"; ValueData: """{app}\Student\SiMantaStudent.exe"""; Check: IsStudentInstall; Flags: uninsdeletevalue

[Run]
Filename: "{app}\Teacher\SiMantaTeacher.exe"; Description: "Launch Teacher Console now"; Check: IsTeacherInstall; Flags: nowait postinstall skipifsilent unchecked
Filename: "{app}\Student\SiMantaStudent.exe"; Description: "Launch Student Agent now"; Check: IsStudentInstall; Flags: nowait postinstall skipifsilent unchecked

[Run]
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""SiManta Teacher"" dir=in action=allow program=""{app}\Teacher\SiMantaTeacher.exe"" enable=yes profile=any protocol=tcp"; Check: IsTeacherInstall; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall add rule name=""SiManta Student"" dir=in action=allow program=""{app}\Student\SiMantaStudent.exe"" enable=yes profile=any protocol=tcp"; Check: IsStudentInstall; Flags: runhidden

[UninstallRun]
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""SiManta Teacher"""; Flags: runhidden
Filename: "netsh"; Parameters: "advfirewall firewall delete rule name=""SiManta Student"""; Flags: runhidden
Filename: "taskkill"; Parameters: "/f /im SiMantaTeacher.exe"; Flags: runhidden
Filename: "taskkill"; Parameters: "/f /im SiMantaStudent.exe"; Flags: runhidden

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

procedure InitializeWizard();
var
  HeaderLabel: TLabel;
  DescLabel: TLabel;
  TeacherDesc: TLabel;
  StudentDesc: TLabel;
begin
  RoleSelected := 0; // Default: Teacher

  // ---- Custom Role Selection Page (Radio Buttons) ----
  RolePage := CreateCustomPage(wpLicense,
    'Installation Type',
    'Choose how you want to install SiManta on this computer.');

  // Header
  HeaderLabel := TLabel.Create(RolePage);
  HeaderLabel.Parent := RolePage.Surface;
  HeaderLabel.Caption := 'Select Installation Mode:';
  HeaderLabel.Font.Size := 11;
  HeaderLabel.Font.Style := [fsBold];
  HeaderLabel.Left := 0;
  HeaderLabel.Top := 16;
  HeaderLabel.Width := RolePage.SurfaceWidth;

  // --- Teacher Radio ---
  TeacherRadio := TRadioButton.Create(RolePage);
  TeacherRadio.Parent := RolePage.Surface;
  TeacherRadio.Caption := 'Install as Teacher (Guru)';
  TeacherRadio.Font.Size := 10;
  TeacherRadio.Font.Style := [fsBold];
  TeacherRadio.Left := 16;
  TeacherRadio.Top := 60;
  TeacherRadio.Width := RolePage.SurfaceWidth - 32;
  TeacherRadio.Height := 24;
  TeacherRadio.Checked := True;

  TeacherDesc := TLabel.Create(RolePage);
  TeacherDesc.Parent := RolePage.Surface;
  TeacherDesc.Caption := 'Installs the Teacher Console to monitor and control student screens in real-time. Use this on the teacher''s computer.';
  TeacherDesc.WordWrap := True;
  TeacherDesc.Left := 36;
  TeacherDesc.Top := 88;
  TeacherDesc.Width := RolePage.SurfaceWidth - 52;
  TeacherDesc.Font.Color := clGray;

  // --- Student Radio ---
  StudentRadio := TRadioButton.Create(RolePage);
  StudentRadio.Parent := RolePage.Surface;
  StudentRadio.Caption := 'Install as Student (Murid)';
  StudentRadio.Font.Size := 10;
  StudentRadio.Font.Style := [fsBold];
  StudentRadio.Left := 16;
  StudentRadio.Top := 140;
  StudentRadio.Width := RolePage.SurfaceWidth - 32;
  StudentRadio.Height := 24;

  StudentDesc := TLabel.Create(RolePage);
  StudentDesc.Parent := RolePage.Surface;
  StudentDesc.Caption := 'Installs the Student Agent that runs silently in the background. Captures the screen and sends it to the Teacher Console. Use this on each student''s computer.';
  StudentDesc.WordWrap := True;
  StudentDesc.Left := 36;
  StudentDesc.Top := 168;
  StudentDesc.Width := RolePage.SurfaceWidth - 52;
  StudentDesc.Font.Color := clGray;

  // Description
  DescLabel := TLabel.Create(RolePage);
  DescLabel.Parent := RolePage.Surface;
  DescLabel.Caption := 'Note: Only one mode can be installed per computer.';
  DescLabel.Font.Style := [fsItalic];
  DescLabel.Font.Color := clGray;
  DescLabel.Left := 0;
  DescLabel.Top := 230;
  DescLabel.Width := RolePage.SurfaceWidth;

  // ---- Teacher IP Configuration Page (Student only) ----
  TeacherIPPage := CreateInputQueryPage(RolePage.ID,
    'Teacher Configuration',
    'Configure the Teacher Console IP address for the Student Agent.',
    'Enter the IP address of the teacher''s computer. ' +
    'The Student Agent will automatically connect to this IP on startup. ' +
    'You can change this later in config.ini.');
  TeacherIPPage.Add('Teacher IP Address:', False);
  TeacherIPPage.Values[0] := '127.0.0.1';
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
  // Skip Teacher IP page if installing as Teacher
  if PageID = TeacherIPPage.ID then
    Result := IsTeacherInstall();
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ConfigFile: string;
begin
  if CurStep = ssPostInstall then
  begin
    // Write config.ini for Student Agent with the teacher IP
    if IsStudentInstall() then
    begin
      ConfigFile := ExpandConstant('{app}\Student\config.ini');
      SetIniString('General', 'teacher_ip', TeacherIPPage.Values[0], ConfigFile);
      SetIniString('General', 'port', '5400', ConfigFile);
      SetIniString('General', 'interval', '2000', ConfigFile);
      SetIniString('General', 'quality', '92', ConfigFile);
      SetIniString('General', 'scale', '1.0', ConfigFile);
    end;
  end;
end;
