; Lab Monitor Installer - InnoSetup Script
; Builds a Windows installer with Teacher/Student component selection
;
; Prerequisites:
;   - Build lab-teacher.exe and lab-student.exe with Qt6 on Windows
;   - Run windeployqt on both executables
;   - Place everything in the dist/ folder structure below
;
; Build: iscc installer.iss

#define AppName "Lab Monitor"
#define AppVersion "1.0.0"
#define AppPublisher "Lab Monitor"
#define AppURL "https://github.com/labmonitor"

[Setup]
AppId={{B5F6E3A2-8C4D-4E9F-A1B3-7D2F5E6C8A9B}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppSupportURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir=output
OutputBaseFilename=LabMonitor-Setup-{#AppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Types]
Name: "teacher"; Description: "Teacher Console - Monitor and control student computers"
Name: "student"; Description: "Student Agent - Runs on student computers"
Name: "full"; Description: "Full Installation (Teacher + Student)"

[Components]
Name: "teacher"; Description: "Teacher Console"; Types: teacher full
Name: "student"; Description: "Student Agent"; Types: student full

[Files]
; Teacher files
Source: "dist\teacher\*"; DestDir: "{app}\teacher"; Flags: ignoreversion recursesubdirs; Components: teacher
; Student files
Source: "dist\student\*"; DestDir: "{app}\student"; Flags: ignoreversion recursesubdirs; Components: student

[Icons]
; Teacher shortcuts
Name: "{group}\Lab Monitor Teacher"; Filename: "{app}\teacher\lab-teacher.exe"; Components: teacher
Name: "{commondesktop}\Lab Monitor Teacher"; Filename: "{app}\teacher\lab-teacher.exe"; Components: teacher
; Student shortcuts
Name: "{group}\Lab Monitor Student"; Filename: "{app}\student\lab-student.exe"; Parameters: "--teacher 192.168.1.1"; Components: student

[Run]
; Auto-start student agent after install
Filename: "{app}\student\lab-student.exe"; Parameters: "--teacher 192.168.1.1"; Description: "Start Student Agent now"; Flags: nowait postinstall skipifsilent; Components: student

[Registry]
; Auto-start student agent on boot
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "LabMonitorStudent"; ValueData: """{app}\student\lab-student.exe"" --teacher 192.168.1.1"; Flags: uninsdeletevalue; Components: student

[UninstallRun]
Filename: "taskkill"; Parameters: "/F /IM lab-student.exe"; Flags: runhidden; Components: student
Filename: "taskkill"; Parameters: "/F /IM lab-teacher.exe"; Flags: runhidden; Components: teacher

[Code]
var
  TeacherIPPage: TInputQueryWizardPage;

procedure InitializeWizard();
begin
  // Add a page to configure the teacher IP (only for student install)
  TeacherIPPage := CreateInputQueryPage(wpSelectComponents,
    'Teacher Configuration',
    'Enter the Teacher Console IP address',
    'The student agent needs to know the IP address of the teacher console.');
  TeacherIPPage.Add('Teacher IP Address:', False);
  TeacherIPPage.Values[0] := '192.168.1.1';
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  // Skip teacher IP page if student component is not selected
  if PageID = TeacherIPPage.ID then
    Result := not IsComponentSelected('student');
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  TeacherIP: string;
begin
  if CurStep = ssPostInstall then
  begin
    TeacherIP := TeacherIPPage.Values[0];
    if IsComponentSelected('student') then
    begin
      // Update registry with actual teacher IP
      RegWriteStringValue(HKEY_LOCAL_MACHINE,
        'SOFTWARE\Microsoft\Windows\CurrentVersion\Run',
        'LabMonitorStudent',
        '"' + ExpandConstant('{app}') + '\student\lab-student.exe" --teacher ' + TeacherIP);
    end;
  end;
end;
