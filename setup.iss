; SiManta Installer Script - InnoSetup 6
; Packages both Teacher and Student into a single SiManta.exe installer.

#define MyAppName "SiManta"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "SiManta Classroom"
#define MyAppURL "https://simanta.local"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=installer_output
OutputBaseFilename=SiManta
SetupIconFile=student\logo.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\SiMantaTeacher.exe
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=SiManta Classroom Management System
WizardImageFile=student\logo.png
WizardSmallImageFile=student\logo.png

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Full Installation (Teacher + Student)"
Name: "teacher"; Description: "Teacher Console Only"
Name: "student"; Description: "Student Agent Only"
Name: "custom"; Description: "Custom Installation"; Flags: iscustom

[Components]
Name: "teacher"; Description: "SiManta Teacher Console"; Types: full teacher
Name: "student"; Description: "SiManta Student Agent"; Types: full student

[Tasks]
Name: "desktopicon_teacher"; Description: "Create Teacher desktop shortcut"; Components: teacher
Name: "desktopicon_student"; Description: "Create Student desktop shortcut"; Components: student
Name: "autostart_student"; Description: "Start Student Agent on login"; Components: student; Flags: unchecked

; ── Teacher files ──
[Files]
Source: "build\teacher\Release\SiMantaTeacher.exe"; DestDir: "{app}\Teacher"; Components: teacher; Flags: ignoreversion
Source: "build\teacher\Release\*.dll"; DestDir: "{app}\Teacher"; Components: teacher; Flags: ignoreversion
Source: "build\teacher\Release\platforms\*"; DestDir: "{app}\Teacher\platforms"; Components: teacher; Flags: ignoreversion recursesubdirs
Source: "build\teacher\Release\styles\*"; DestDir: "{app}\Teacher\styles"; Components: teacher; Flags: ignoreversion recursesubdirs
Source: "build\teacher\Release\imageformats\*"; DestDir: "{app}\Teacher\imageformats"; Components: teacher; Flags: ignoreversion recursesubdirs
Source: "build\teacher\Release\iconengines\*"; DestDir: "{app}\Teacher\iconengines"; Components: teacher; Flags: ignoreversion recursesubdirs
Source: "build\teacher\Release\tls\*"; DestDir: "{app}\Teacher\tls"; Components: teacher; Flags: ignoreversion recursesubdirs
Source: "build\teacher\Release\networkinformation\*"; DestDir: "{app}\Teacher\networkinformation"; Components: teacher; Flags: ignoreversion recursesubdirs
Source: "build\teacher\Release\generic\*"; DestDir: "{app}\Teacher\generic"; Components: teacher; Flags: ignoreversion recursesubdirs
Source: "build\teacher\Release\translations\*"; DestDir: "{app}\Teacher\translations"; Components: teacher; Flags: ignoreversion recursesubdirs

; ── Student files ──
Source: "build\student\Release\SiMantaStudent.exe"; DestDir: "{app}\Student"; Components: student; Flags: ignoreversion
Source: "build\student\Release\*.dll"; DestDir: "{app}\Student"; Components: student; Flags: ignoreversion
Source: "build\student\Release\config.ini"; DestDir: "{app}\Student"; Components: student; Flags: ignoreversion
Source: "build\student\Release\platforms\*"; DestDir: "{app}\Student\platforms"; Components: student; Flags: ignoreversion recursesubdirs
Source: "build\student\Release\styles\*"; DestDir: "{app}\Student\styles"; Components: student; Flags: ignoreversion recursesubdirs
Source: "build\student\Release\imageformats\*"; DestDir: "{app}\Student\imageformats"; Components: student; Flags: ignoreversion recursesubdirs
Source: "build\student\Release\iconengines\*"; DestDir: "{app}\Student\iconengines"; Components: student; Flags: ignoreversion recursesubdirs
Source: "build\student\Release\tls\*"; DestDir: "{app}\Student\tls"; Components: student; Flags: ignoreversion recursesubdirs
Source: "build\student\Release\networkinformation\*"; DestDir: "{app}\Student\networkinformation"; Components: student; Flags: ignoreversion recursesubdirs
Source: "build\student\Release\generic\*"; DestDir: "{app}\Student\generic"; Components: student; Flags: ignoreversion recursesubdirs
Source: "build\student\Release\translations\*"; DestDir: "{app}\Student\translations"; Components: student; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\SiManta Teacher"; Filename: "{app}\Teacher\SiMantaTeacher.exe"; Components: teacher
Name: "{group}\SiManta Student"; Filename: "{app}\Student\SiMantaStudent.exe"; Components: student
Name: "{group}\Uninstall SiManta"; Filename: "{uninstallexe}"
Name: "{autodesktop}\SiManta Teacher"; Filename: "{app}\Teacher\SiMantaTeacher.exe"; Tasks: desktopicon_teacher
Name: "{autodesktop}\SiManta Student"; Filename: "{app}\Student\SiMantaStudent.exe"; Tasks: desktopicon_student

; Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "SiMantaStudent"; ValueData: """{app}\Student\SiMantaStudent.exe"""; Flags: uninsdeletevalue; Tasks: autostart_student

[Run]
Filename: "{app}\Teacher\SiMantaTeacher.exe"; Description: "Launch SiManta Teacher"; Flags: nowait postinstall skipifsilent; Components: teacher
Filename: "{app}\Student\SiMantaStudent.exe"; Description: "Launch SiManta Student"; Flags: nowait postinstall skipifsilent; Components: student
Filename: "schtasks"; Parameters: "/create /tn ""SiMantaStudent"" /tr ""\""{app}\Student\SiMantaStudent.exe\"""" /sc onlogon /rl highest /f"; Tasks: autostart_student; Flags: runhidden

[UninstallRun]
Filename: "schtasks"; Parameters: "/delete /tn ""SiMantaStudent"" /f"; Flags: runhidden skipifdoesntexist
