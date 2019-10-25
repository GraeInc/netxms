[Tasks]
Name: configureServer; Description: "Run server configuration wizard"; Components: server; Flags: checkedonce
Name: upgradeDatabase; Description: "Upgrade database schema if needed"; Components: server
Name: startCore; Description: "Start NetXMS Core service after installation"; Components: server
Name: fspermissions; Description: "Set hardened file system permissions"

[Dirs]
Name: "{app}\etc"
Name: "{app}\database"
Name: "{app}\dump"
Name: "{app}\log"
Name: "{app}\var\backgrounds"
Name: "{app}\var\files"
Name: "{app}\var\images"
Name: "{app}\var\packages"
Name: "{app}\share"

[Registry]
Root: HKLM; Subkey: "Software\NetXMS"; Flags: uninsdeletekeyifempty; Components: server
Root: HKLM; Subkey: "Software\NetXMS\Server"; Flags: uninsdeletekey; Components: server
Root: HKLM; Subkey: "Software\NetXMS\Server"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Components: server
Root: HKLM; Subkey: "Software\NetXMS\Server"; ValueType: string; ValueName: "ConfigFile"; ValueData: "{app}\etc\netxmsd.conf"; Components: server

[Run]
Filename: "{app}\var\rm.exe"; Parameters: "-f ""{app}\bin\*.manifest"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old manifest files..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\bin\Microsoft.VC80.*"""; WorkingDir: "{app}\var"; StatusMsg: "Removing old CRT files..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\Microsoft.VC80.CRT"""; WorkingDir: "{app}\var"; StatusMsg: "Removing old CRT files..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-f ""{app}\bin\libnetxmsw.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old DLL files..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-f ""{app}\bin\libnxclw.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old DLL files..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-f ""{app}\bin\libnxdbw.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old DLL files..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-f ""{app}\bin\libnxmap.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old DLL files..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-f ""{app}\bin\libnxmapw.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old DLL files..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-f ""{app}\bin\libnxsnmpw.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old DLL files..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\var\mibs"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old MIB directory..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\sql"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old SQL files..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\avaya-ers.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\baystack.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\cat2900xl.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\catalyst.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\cisco.dll"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\cisco-esw.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\cisco-sb.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\ers8000.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\h3c.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\hpsw.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\netscreen.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\ntws.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\rm.exe"; Parameters: "-rf ""{app}\lib\ndd\procurve.*"""; WorkingDir: "{app}\bin"; StatusMsg: "Removing old device drivers..."; Flags: runhidden
Filename: "{app}\var\vcredist.exe"; Parameters: "/install /passive /norestart"; WorkingDir: "{app}\var"; StatusMsg: "Installing Visual C++ runtime..."; Flags: waituntilterminated
Filename: "icacls.exe"; Parameters: """{app}"" /inheritance:r"; StatusMsg: "Setting file system permissions..."; Flags: runhidden waituntilterminated; Tasks: fspermissions
Filename: "icacls.exe"; Parameters: """{app}"" /grant:r *S-1-5-18:(OI)(CI)F"; StatusMsg: "Setting file system permissions..."; Flags: runhidden waituntilterminated; Tasks: fspermissions
Filename: "icacls.exe"; Parameters: """{app}"" /grant:r *S-1-5-32-544:(OI)(CI)F"; StatusMsg: "Setting file system permissions..."; Flags: runhidden waituntilterminated; Tasks: fspermissions
Filename: "icacls.exe"; Parameters: """{app}\*"" /reset /T"; StatusMsg: "Setting file system permissions..."; Flags: runhidden waituntilterminated; Tasks: fspermissions
Filename: "{app}\bin\nxmibc.exe"; Parameters: "-z -d ""{app}\share\mibs"" -o ""{app}\var\netxms.mib"""; WorkingDir: "{app}\bin"; StatusMsg: "Compiling MIB files..."; Flags: runhidden; Components: server
Filename: "{app}\bin\nxconfig.exe"; Parameters: "--create-agent-config"; WorkingDir: "{app}\bin"; StatusMsg: "Creating agent's configuration file..."; Components: server
Filename: "{app}\bin\nxagentd.exe"; Parameters: "-c ""{app}\etc\nxagentd.conf"" -I"; WorkingDir: "{app}\bin"; StatusMsg: "Installing agent service..."; Flags: runhidden; Components: server
Filename: "{app}\bin\nxagentd.exe"; Parameters: "-s"; WorkingDir: "{app}\bin"; StatusMsg: "Starting agent service..."; Flags: runhidden; Components: server
Filename: "{app}\bin\nxconfig.exe"; Parameters: "--configure-if-needed"; WorkingDir: "{app}\bin"; StatusMsg: "Running server configuration wizard..."; Components: server; Tasks: configureServer
Filename: "{app}\bin\nxdbmgr.exe"; Parameters: "-c ""{app}\etc\netxmsd.conf"" upgrade"; WorkingDir: "{app}\bin"; StatusMsg: "Upgrading database..."; Flags: runhidden; Components: server; Tasks: upgradeDatabase
Filename: "{app}\bin\netxmsd.exe"; Parameters: "--check-service"; WorkingDir: "{app}\bin"; StatusMsg: "Checking core service configuration..."; Flags: runhidden; Components: server
Filename: "{app}\bin\netxmsd.exe"; Parameters: "-s -m"; WorkingDir: "{app}\bin"; StatusMsg: "Starting core service..."; Flags: runhidden; Components: server; Tasks: startCore

[UninstallRun]
Filename: "{app}\bin\netxmsd.exe"; Parameters: "-S"; StatusMsg: "Stopping core service..."; RunOnceId: "StopCoreService"; Flags: runhidden; Components: server
Filename: "{app}\bin\netxmsd.exe"; Parameters: "-R"; StatusMsg: "Uninstalling core service..."; RunOnceId: "DelCoreService"; Flags: runhidden; Components: server
Filename: "{app}\bin\nxagentd.exe"; Parameters: "-S"; StatusMsg: "Stopping agent service..."; RunOnceId: "StopAgentService"; Flags: runhidden; Components: server
Filename: "{app}\bin\nxagentd.exe"; Parameters: "-R"; StatusMsg: "Uninstalling agent service..."; RunOnceId: "DelAgentService"; Flags: runhidden; Components: server

[Code]
Var
  HttpdSettingsPage: TInputQueryWizardPage;
  backupFileSuffix: String;

#include "firewall.iss"

Procedure StopAllServices;
Var
  iResult: Integer;
Begin
  Exec('net.exe', 'stop NetXMSCore', ExpandConstant('{app}\bin'), 0, ewWaitUntilTerminated, iResult);
  Exec('net.exe', 'stop NetXMSAgentdW32', ExpandConstant('{app}\bin'), 0, ewWaitUntilTerminated, iResult);
End;

Function InitializeSetup(): Boolean;
Begin
  // Common suffix for backup files
  backupFileSuffix := '.bak.' + GetDateTimeString('yyyymmddhhnnss', #0, #0);
  Result := TRUE
End;

Procedure InitializeWizard;
Begin
  HttpdSettingsPage := CreateInputQueryPage(wpSelectTasks,
    'Select Master Server', 'Where is master server for web interface?',
    'Please enter host name or IP address of NetXMS master server. NetXMS web interface you are installing will provide connectivity to that server.');
  HttpdSettingsPage.Add('NetXMS server:', False);
  HttpdSettingsPage.Values[0] := GetPreviousData('MasterServer', 'localhost');
End;

Procedure RegisterPreviousData(PreviousDataKey: Integer);
Begin
  SetPreviousData(PreviousDataKey, 'MasterServer', HttpdSettingsPage.Values[0]);
End;

Function ShouldSkipPage(PageID: Integer): Boolean;
Begin
  If PageID = HttpdSettingsPage.ID Then
    Result := not IsComponentSelected('websrv')
  Else
    Result := False;
End;

Function GetMasterServer(Param: String): String;
Begin
  Result := HttpdSettingsPage.Values[0];
End;

Procedure RenameOldFile;
Begin
  RenameFile(ExpandConstant(CurrentFileName), ExpandConstant(CurrentFileName) + backupFileSuffix);
End;

Procedure DeleteBackupFiles;
Var
  fd: TFindRec;
  basePath: String;
Begin
  basePath := ExpandConstant('{app}\bin\');
  If FindFirst(basePath + '*.bak.*', fd) Then
  Begin
    Try
      Repeat
        DeleteFile(basePath + fd.Name);
      Until Not FindNext(fd);
    Finally
      FindClose(fd);
    End;
  End;
End;

Procedure CurStepChanged(CurStep: TSetupStep);
Begin
  If CurStep=ssPostInstall Then Begin
    SetFirewallException('NetXMS Server', ExpandConstant('{app}')+'\bin\netxmsd.exe');
    SetFirewallException('NetXMS Agent', ExpandConstant('{app}')+'\bin\nxagentd.exe');
    DeleteBackupFiles;
  End;
End;

Procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
Begin
  If CurUninstallStep=usPostUninstall Then Begin
    RemoveFirewallException(ExpandConstant('{app}')+'\bin\netxmsd.exe');
    RemoveFirewallException(ExpandConstant('{app}')+'\bin\nxagentd.exe');
  End;
End;
