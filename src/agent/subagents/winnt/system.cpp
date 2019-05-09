/* 
** Windows 2000+ NetXMS subagent
** Copyright (C) 2003-2019 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: system.cpp
** WinNT+ specific system information parameters
**/

#include "winnt_subagent.h"
#include <wuapi.h>

/**
 * Handler for System.ServiceState parameter
 */
LONG H_ServiceState(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   SC_HANDLE hManager, hService;
   TCHAR szServiceName[MAX_PATH];
   LONG iResult = SYSINFO_RC_SUCCESS;

   if (!AgentGetParameterArg(cmd, 1, szServiceName, MAX_PATH))
      return SYSINFO_RC_UNSUPPORTED;

   hManager = OpenSCManager(NULL, NULL, GENERIC_READ);
   if (hManager == NULL)
   {
      return SYSINFO_RC_ERROR;
   }

   hService = OpenService(hManager, szServiceName, SERVICE_QUERY_STATUS);
   if (hService == NULL)
   {
      iResult = SYSINFO_RC_UNSUPPORTED;
   }
   else
   {
      SERVICE_STATUS status;

      if (QueryServiceStatus(hService, &status))
      {
         int i;
         static DWORD dwStates[7]={ SERVICE_RUNNING, SERVICE_PAUSED, SERVICE_START_PENDING,
                                    SERVICE_PAUSE_PENDING, SERVICE_CONTINUE_PENDING,
                                    SERVICE_STOP_PENDING, SERVICE_STOPPED };

         for(i = 0; i < 7; i++)
            if (status.dwCurrentState == dwStates[i])
               break;
         ret_uint(value, i);
      }
      else
      {
         ret_uint(value, 255);    // Unable to retrieve information
      }
      CloseServiceHandle(hService);
   }

   CloseServiceHandle(hManager);
   return iResult;
}

/**
 * Handler for System.Services list
 */
LONG H_ServiceList(const TCHAR *pszCmd, const TCHAR *pArg, StringList *value, AbstractCommSession *session)
{
   SC_HANDLE hManager = OpenSCManager(NULL, NULL, GENERIC_READ);
   if (hManager == NULL)
   {
      return SYSINFO_RC_ERROR;
   }

   LONG rc = SYSINFO_RC_ERROR;
   DWORD bytes, count;
   EnumServicesStatusEx(hManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &bytes, &count, NULL, NULL);
   if (GetLastError() == ERROR_MORE_DATA)
   {
      ENUM_SERVICE_STATUS_PROCESS *services = (ENUM_SERVICE_STATUS_PROCESS *)malloc(bytes);
      if (EnumServicesStatusEx(hManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, (BYTE *)services, bytes, &bytes, &count, NULL, NULL))
      {
         for(DWORD i = 0; i < count; i++)
            value->add(services[i].lpServiceName);
         rc = SYSINFO_RC_SUCCESS;
      }
      free(services);
   }

   CloseServiceHandle(hManager);
   return rc;
}

/**
 * Handler for System.Services table
 */
LONG H_ServiceTable(const TCHAR *pszCmd, const TCHAR *pArg, Table *value, AbstractCommSession *session)
{
   SC_HANDLE hManager = OpenSCManager(NULL, NULL, GENERIC_READ);
   if (hManager == NULL)
   {
      return SYSINFO_RC_ERROR;
   }

   LONG rc = SYSINFO_RC_ERROR;
   DWORD bytes, count;
   EnumServicesStatusEx(hManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &bytes, &count, NULL, NULL);
   if (GetLastError() == ERROR_MORE_DATA)
   {
      ENUM_SERVICE_STATUS_PROCESS *services = (ENUM_SERVICE_STATUS_PROCESS *)malloc(bytes);
      if (EnumServicesStatusEx(hManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, (BYTE *)services, bytes, &bytes, &count, NULL, NULL))
      {
         value->addColumn(_T("NAME"), DCI_DT_STRING, _T("Name"), true);
         value->addColumn(_T("DISPNAME"), DCI_DT_STRING, _T("Display name"));
         value->addColumn(_T("TYPE"), DCI_DT_STRING, _T("Type"));
         value->addColumn(_T("STATE"), DCI_DT_STRING, _T("State"));
         value->addColumn(_T("STARTUP"), DCI_DT_STRING, _T("Startup"));
         value->addColumn(_T("PID"), DCI_DT_UINT, _T("PID"));
         value->addColumn(_T("BINARY"), DCI_DT_STRING, _T("Binary"));
         value->addColumn(_T("DEPENDENCIES"), DCI_DT_STRING, _T("Dependencies"));
         for(DWORD i = 0; i < count; i++)
         {
            value->addRow();
            value->set(0, services[i].lpServiceName);
            value->set(1, services[i].lpDisplayName);
            value->set(2, (services[i].ServiceStatusProcess.dwServiceType == SERVICE_WIN32_SHARE_PROCESS) ? _T("Shared") : _T("Own"));
            switch(services[i].ServiceStatusProcess.dwCurrentState)
            {
               case SERVICE_CONTINUE_PENDING:
                  value->set(3, _T("Continue Pending"));
                  break;
               case SERVICE_PAUSE_PENDING:
                  value->set(3, _T("Pausing"));
                  break;
               case SERVICE_PAUSED:
                  value->set(3, _T("Paused"));
                  break;
               case SERVICE_RUNNING:
                  value->set(3, _T("Running"));
                  break;
               case SERVICE_START_PENDING:
                  value->set(3, _T("Starting"));
                  break;
               case SERVICE_STOP_PENDING:
                  value->set(3, _T("Stopping"));
                  break;
               case SERVICE_STOPPED:
                  value->set(3, _T("Stopped"));
                  break;
               default:
                  value->set(3, (UINT32)services[i].ServiceStatusProcess.dwCurrentState);
                  break;
            }
            if (services[i].ServiceStatusProcess.dwProcessId != 0)
               value->set(5, (UINT32)services[i].ServiceStatusProcess.dwProcessId);

            SC_HANDLE hService = OpenService(hManager, services[i].lpServiceName, SERVICE_QUERY_CONFIG);
            if (hService != NULL)
            {
               BYTE buffer[8192];
               QUERY_SERVICE_CONFIG *cfg = (QUERY_SERVICE_CONFIG *)&buffer;
               if (QueryServiceConfig(hService, cfg, 8192, &bytes))
               {
                  switch(cfg->dwStartType)
                  {
                     case SERVICE_AUTO_START:
                        value->set(4, _T("Auto"));
                        break;
                     case SERVICE_BOOT_START:
                        value->set(4, _T("Boot"));
                        break;
                     case SERVICE_DEMAND_START:
                        value->set(4, _T("Manual"));
                        break;
                     case SERVICE_DISABLED:
                        value->set(4, _T("Disabled"));
                        break;
                     case SERVICE_SYSTEM_START:
                        value->set(4, _T("System"));
                        break;
                     default:
                        value->set(4, (UINT32)cfg->dwStartType);
                        break;
                  }
                  value->set(6, cfg->lpBinaryPathName);
                  value->set(7, cfg->lpDependencies);
               }
               CloseServiceHandle(hService);
            }
         }
         rc = SYSINFO_RC_SUCCESS;
      }
      free(services);
   }

   CloseServiceHandle(hManager);
   return rc;
}

/**
 * Handler for System.ThreadCount
 */
LONG H_ThreadCount(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   PERFORMANCE_INFORMATION pi;
   pi.cb = sizeof(PERFORMANCE_INFORMATION);
   if (!GetPerformanceInfo(&pi, sizeof(PERFORMANCE_INFORMATION)))
      return SYSINFO_RC_ERROR;
   ret_uint(value, pi.ThreadCount);
   return SYSINFO_RC_SUCCESS;
}

/**
 * Handler for System.HandleCount
 */
LONG H_HandleCount(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   PERFORMANCE_INFORMATION pi;
   pi.cb = sizeof(PERFORMANCE_INFORMATION);
   if (!GetPerformanceInfo(&pi, sizeof(PERFORMANCE_INFORMATION)))
      return SYSINFO_RC_ERROR;
   ret_uint(value, pi.HandleCount);
   return SYSINFO_RC_SUCCESS;
}

/**
 * Handler for System.ConnectedUsers parameter
 */
LONG H_ConnectedUsers(const TCHAR *pszCmd, const TCHAR *pArg, TCHAR *pValue, AbstractCommSession *session)
{
   LONG nRet;
   WTS_SESSION_INFO *pSessionList;
   DWORD i, dwNumSessions, dwCount;

   if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionList, &dwNumSessions))
   {
      for(i = 0, dwCount = 0; i < dwNumSessions; i++)
         if ((pSessionList[i].State == WTSActive) ||
             (pSessionList[i].State == WTSConnected))
            dwCount++;
      WTSFreeMemory(pSessionList);
      ret_uint(pValue, dwCount);
      nRet = SYSINFO_RC_SUCCESS;
   }
   else
   {
      nRet = SYSINFO_RC_ERROR;
   }
   return nRet;
}

/**
 * Handler for System.ActiveUserSessions enum
 */
LONG H_ActiveUserSessions(const TCHAR *pszCmd, const TCHAR *pArg, StringList *value, AbstractCommSession *session)
{
   LONG nRet;
   WTS_SESSION_INFO *pSessionList;
   DWORD i, dwNumSessions, dwBytes;
   TCHAR *pszClientName, *pszUserName, szBuffer[1024];

   if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionList, &dwNumSessions))
   {
      for(i = 0; i < dwNumSessions; i++)
		{
         if ((pSessionList[i].State == WTSActive) ||
             (pSessionList[i].State == WTSConnected))
         {
            if (!WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, pSessionList[i].SessionId,
                                            WTSClientName, &pszClientName, &dwBytes))
            {
               pszClientName = NULL;
            }
            if (!WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, pSessionList[i].SessionId,
                                            WTSUserName, &pszUserName, &dwBytes))
            {
               pszUserName = NULL;
            }

            _sntprintf(szBuffer, 1024, _T("\"%s\" \"%s\" \"%s\""),
                       pszUserName == NULL ? _T("UNKNOWN") : pszUserName,
                       pSessionList[i].pWinStationName,
                       pszClientName == NULL ? _T("UNKNOWN") : pszClientName);
            value->add(szBuffer);

            if (pszUserName != NULL)
               WTSFreeMemory(pszUserName);
            if (pszClientName != NULL)
               WTSFreeMemory(pszClientName);
         }
		}
      WTSFreeMemory(pSessionList);
      nRet = SYSINFO_RC_SUCCESS;
   }
   else
   {
      nRet = SYSINFO_RC_ERROR;
   }
   return nRet;
}

/**
 * Callback for window stations enumeration
 */
static BOOL CALLBACK WindowStationsEnumCallback(LPTSTR lpszWindowStation, LPARAM lParam)
{
   ((StringList *)lParam)->add(lpszWindowStation);
   return TRUE;
}

/**
 * Handler for System.WindowStations list
 */
LONG H_WindowStations(const TCHAR *cmd, const TCHAR *arg, StringList *value, AbstractCommSession *session)
{
   return EnumWindowStations(WindowStationsEnumCallback, (LONG_PTR)value) ? SYSINFO_RC_SUCCESS : SYSINFO_RC_ERROR;
}

/**
 * Callback for desktop enumeration
 */
static BOOL CALLBACK DesktopsEnumCallback(LPTSTR lpszDesktop, LPARAM lParam)
{
   ((StringList *)lParam)->add(lpszDesktop);
   return TRUE;
}

/**
 * Handler for System.Desktops list
 */
LONG H_Desktops(const TCHAR *cmd, const TCHAR *arg, StringList *value, AbstractCommSession *session)
{
   TCHAR wsName[256];
   AgentGetParameterArg(cmd, 1, wsName, 256);
   HWINSTA ws = OpenWindowStation(wsName, FALSE, WINSTA_ENUMDESKTOPS);
   if (ws == NULL)
      return SYSINFO_RC_ERROR;

   LONG rc = EnumDesktops(ws, DesktopsEnumCallback, (LONG_PTR)value) ? SYSINFO_RC_SUCCESS : SYSINFO_RC_ERROR;
   CloseWindowStation(ws);
   return rc;
}

/**
 * Handler for Agent.Desktop parameter
 */
LONG H_AgentDesktop(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   HWINSTA ws = GetProcessWindowStation();
   if (ws == NULL)
      return SYSINFO_RC_ERROR;

   HDESK desk = GetThreadDesktop(GetCurrentThreadId());
   if (desk == NULL)
      return SYSINFO_RC_ERROR;

   TCHAR wsName[64], deskName[64];
   DWORD size;
   if (GetUserObjectInformation(ws, UOI_NAME, wsName, 64 * sizeof(TCHAR), &size) &&
       GetUserObjectInformation(desk, UOI_NAME, deskName, 64 * sizeof(TCHAR), &size))
   {
      DWORD sid;
      if (ProcessIdToSessionId(GetCurrentProcessId(), &sid))
      {
         TCHAR *sessionName;
         if (WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, sid, WTSWinStationName, &sessionName, &size))
         {
            _sntprintf(value, MAX_RESULT_LENGTH, _T("/%s/%s/%s"), sessionName, wsName, deskName);
            WTSFreeMemory(sessionName);
         }
         else
         {
            _sntprintf(value, MAX_RESULT_LENGTH, _T("/%u/%s/%s"), sid, wsName, deskName);
         }
      }
      else
      {
         _sntprintf(value, MAX_RESULT_LENGTH, _T("/?/%s/%s"), wsName, deskName);
      }
      return SYSINFO_RC_SUCCESS;
   }
   else
   {
      return SYSINFO_RC_ERROR;
   }
}

/**
 * Handler for System.AppAddressSpace
 */
LONG H_AppAddressSpace(const TCHAR *pszCmd, const TCHAR *pArg, TCHAR *pValue, AbstractCommSession *session)
{
	SYSTEM_INFO si;

	GetSystemInfo(&si);
	DWORD_PTR size = (DWORD_PTR)si.lpMaximumApplicationAddress - (DWORD_PTR)si.lpMinimumApplicationAddress;
	ret_uint(pValue, (DWORD)(size / 1024 / 1024));
	return SYSINFO_RC_SUCCESS;
}

/**
 * Read update time from registry
 */
static bool ReadSystemUpdateTimeFromRegistry(const TCHAR *type, TCHAR *value)
{
   TCHAR buffer[MAX_PATH];
   _sntprintf(buffer, MAX_PATH, _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\Results\\%s"), type);

   HKEY hKey;
   if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, buffer, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
      return false;

   DWORD size = MAX_PATH * sizeof(TCHAR);
   if (RegQueryValueEx(hKey, _T("LastSuccessTime"), NULL, NULL, (BYTE *)buffer, &size) != ERROR_SUCCESS)
   {
      RegCloseKey(hKey);
      return false;
   }
   RegCloseKey(hKey);

   // Date stored as YYYY-mm-dd HH:MM:SS in UTC
   if (_tcslen(buffer) != 19)
      return false;

   struct tm t;
   memset(&t, 0, sizeof(struct tm));
   t.tm_isdst = 0;

   buffer[4] = 0;
   t.tm_year = _tcstol(buffer, NULL, 10) - 1900;

   buffer[7] = 0;
   t.tm_mon = _tcstol(&buffer[5], NULL, 10) - 1;

   buffer[10] = 0;
   t.tm_mday = _tcstol(&buffer[8], NULL, 10);

   buffer[13] = 0;
   t.tm_hour = _tcstol(&buffer[11], NULL, 10);

   buffer[16] = 0;
   t.tm_min = _tcstol(&buffer[14], NULL, 10);

   t.tm_sec = _tcstol(&buffer[17], NULL, 10);

   ret_int64(value, timegm(&t));
   return true;
}

/**
* Read update time from COM component
*/
static bool ReadSystemUpdateTimeFromCOM(const TCHAR *type, TCHAR *value)
{
   bool success = false;

   CoInitializeEx(NULL, COINIT_MULTITHREADED);

   IAutomaticUpdates2 *updateService;
   if (CoCreateInstance(CLSID_AutomaticUpdates, NULL, CLSCTX_ALL, IID_IAutomaticUpdates2, (void**)&updateService) == S_OK)
   {
      IAutomaticUpdatesResults *results;
      if (updateService->get_Results(&results) == S_OK)
      {
         VARIANT v;
         HRESULT hr = !_tcscmp(type, _T("Detect")) ? results->get_LastSearchSuccessDate(&v)  : results->get_LastInstallationSuccessDate(&v);
         if (hr == S_OK)
         {
            if (v.vt == VT_DATE)
            {
               SYSTEMTIME st;
               VariantTimeToSystemTime(v.date, &st);
               
               FILETIME ft;
               SystemTimeToFileTime(&st, &ft);

               LARGE_INTEGER li;
               li.LowPart = ft.dwLowDateTime;
               li.HighPart = ft.dwHighDateTime;
               li.QuadPart -= EPOCHFILETIME;
               ret_int64(value, li.QuadPart / 10000000); // Convert to seconds
               success = true;
            }
            else if (v.vt == VT_EMPTY)
            {
               ret_int64(value, 0);
               success = true;
            }
         }
      }
      updateService->Release();
   }

   CoUninitialize();
   return success;
}

/**
 * Handler for System.Update.*Time parameters
 */
LONG H_SysUpdateTime(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   if (ReadSystemUpdateTimeFromRegistry(arg, value))
      return SYSINFO_RC_SUCCESS;

   return ReadSystemUpdateTimeFromCOM(arg, value) ? SYSINFO_RC_SUCCESS : SYSINFO_RC_ERROR;
}

/**
 * Handler for System.Uname parameter
 */
LONG H_SystemUname(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   TCHAR computerName[MAX_COMPUTERNAME_LENGTH + 1];
   DWORD dwSize = MAX_COMPUTERNAME_LENGTH + 1;
   GetComputerName(computerName, &dwSize);

   OSVERSIONINFO versionInfo;
   versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
   GetVersionEx(&versionInfo);

   TCHAR osVersion[256];
   if (!GetWindowsVersionString(osVersion, 256))
   {
      switch(versionInfo.dwPlatformId)
      {
         case VER_PLATFORM_WIN32_WINDOWS:
            _sntprintf(osVersion, 256, _T("Windows %s-%s"), versionInfo.dwMinorVersion == 0 ? _T("95") :
               (versionInfo.dwMinorVersion == 10 ? _T("98") :
               (versionInfo.dwMinorVersion == 90 ? _T("Me") : _T("Unknown"))), versionInfo.szCSDVersion);
            break;
         case VER_PLATFORM_WIN32_NT:
            _sntprintf(osVersion, 256, _T("Windows NT %d.%d %s"), versionInfo.dwMajorVersion,
               versionInfo.dwMinorVersion, versionInfo.szCSDVersion);
            break;
         default:
            _tcscpy(osVersion, _T("Windows [Unknown Version]"));
            break;
         }
   }

   const TCHAR *cpuType;
   SYSTEM_INFO sysInfo;
   GetSystemInfo(&sysInfo);
   switch (sysInfo.wProcessorArchitecture)
   {
      case PROCESSOR_ARCHITECTURE_INTEL:
         cpuType = _T("Intel IA-32");
         break;
      case PROCESSOR_ARCHITECTURE_MIPS:
         cpuType = _T("MIPS");
         break;
      case PROCESSOR_ARCHITECTURE_ALPHA:
         cpuType = _T("Alpha");
         break;
      case PROCESSOR_ARCHITECTURE_PPC:
         cpuType = _T("PowerPC");
         break;
      case PROCESSOR_ARCHITECTURE_IA64:
         cpuType = _T("Intel IA-64");
         break;
      case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
         cpuType = _T("IA-32 on IA-64");
         break;
      case PROCESSOR_ARCHITECTURE_AMD64:
         cpuType = _T("AMD-64");
         break;
      default:
         cpuType = _T("unknown");
         break;
   }

   _sntprintf(value, MAX_RESULT_LENGTH, _T("Windows %s %d.%d.%d %s %s"), computerName, versionInfo.dwMajorVersion,
      versionInfo.dwMinorVersion, versionInfo.dwBuildNumber, osVersion, cpuType);
   return SYSINFO_RC_SUCCESS;
}

/**
 * Handler for System.Architecture parameter
 */
LONG H_SystemArchitecture(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   SYSTEM_INFO sysInfo;
   GetSystemInfo(&sysInfo);
   switch (sysInfo.wProcessorArchitecture)
   {
      case PROCESSOR_ARCHITECTURE_INTEL:
         ret_string(value, _T("i686"));
         break;
      case PROCESSOR_ARCHITECTURE_MIPS:
         ret_string(value, _T("mips"));
         break;
      case PROCESSOR_ARCHITECTURE_ALPHA:
         ret_string(value, _T("alpha"));
         break;
      case PROCESSOR_ARCHITECTURE_PPC:
         ret_string(value, _T("ppc"));
         break;
      case PROCESSOR_ARCHITECTURE_IA64:
         ret_string(value, _T("ia64"));
         break;
      case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
      case PROCESSOR_ARCHITECTURE_AMD64:
         ret_string(value, _T("amd64"));
         break;
      default:
         ret_string(value, _T("unknown"));
         break;
   }
   return SYSINFO_RC_SUCCESS;
}

/**
 * Handler for System.OS.* parameters
 */
LONG H_SystemVersionInfo(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   OSVERSIONINFOEX versionInfo;
   versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
   if (!GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&versionInfo)))
      return SYSINFO_RC_ERROR;

   switch(*arg)
   {
      case 'B':
         ret_uint(value, versionInfo.dwBuildNumber);
         break;
      case 'S':
         _sntprintf(value, MAX_RESULT_LENGTH, _T("%u.%u"), versionInfo.wServicePackMajor, versionInfo.wServicePackMinor);
         break;
      case 'T':
         ret_string(value, versionInfo.wProductType == VER_NT_WORKSTATION ? _T("Workstation") : _T("Server"));
         break;
      case 'V':
         _sntprintf(value, MAX_RESULT_LENGTH, _T("%u.%u.%u"), versionInfo.dwMajorVersion, versionInfo.dwMinorVersion, versionInfo.dwBuildNumber);
         break;
   }

   return SYSINFO_RC_SUCCESS;
}

/**
 * Decode product key for Windows 7 and below
 */
static void DecodeProductKeyWin7(const BYTE *digitalProductId, TCHAR *decodedKey)
{
   static const char digits[] = "BCDFGHJKMPQRTVWXY2346789";

   BYTE pid[16];
   memcpy(pid, &digitalProductId[52], 16);
   for(int i = 28; i >= 0; i--)
   {
      if ((i + 1) % 6 == 0) // Every sixth char is a separator.
      {
         decodedKey[i] = '-';
      }
      else
      {
         int digitMapIndex = 0;
         for(int j = 14; j >= 0; j--)
         {
            int byteValue = (digitMapIndex << 8) | pid[j];
            pid[j] = (BYTE)(byteValue / 24);
            digitMapIndex = byteValue % 24;
            decodedKey[i] = digits[digitMapIndex];
         }
      }
   }
   decodedKey[29] = 0;
}

/**
 * Decode product key for Windows 8 and above
 */
static void DecodeProductKeyWin8(const BYTE *digitalProductId, TCHAR *decodedKey)
{
   static const char digits[] = "BCDFGHJKMPQRTVWXY2346789";

   BYTE pid[16];
   memcpy(pid, &digitalProductId[52], 16);

   BYTE isWin8 = (pid[15] / 6) & 1;
   pid[15] = (pid[15] & 0xF7) | ((isWin8 & 2) << 2);

   int digitMapIndex;
   for(int i = 24; i >= 0; i--)
   {
      digitMapIndex = 0;
      for (int j = 14; j >= 0; j--)
      {
         int byteValue = (digitMapIndex << 8) | pid[j];
         pid[j] = (BYTE)(byteValue / 24);
         digitMapIndex = byteValue % 24;
         decodedKey[i] = digits[digitMapIndex];
      }
   }

   TCHAR buffer[32];
   memcpy(buffer, &decodedKey[1], digitMapIndex * sizeof(TCHAR));
   buffer[digitMapIndex] = 'N';
   memcpy(&buffer[digitMapIndex + 1], &decodedKey[digitMapIndex + 1], (25 - digitMapIndex) * sizeof(TCHAR));

   for(int i = 0, j = 0; i < 25; i += 5, j += 6)
   {
      memcpy(&decodedKey[j], &buffer[i], 5 * sizeof(TCHAR));
      decodedKey[j + 5] = '-';
   }
   decodedKey[29] = 0;
}

/**
 * Handler for System.OS.ProductId parameters
 */
LONG H_SystemProductInfo(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   HKEY hKey;
   if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"), 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
   {
      TCHAR buffer[1024];
      nxlog_debug_tag(DEBUG_TAG, 5, _T("H_SystemProductInfo: Cannot open registry key (%s)"), GetSystemErrorText(GetLastError(), buffer, 1024));
      return SYSINFO_RC_ERROR;
   }

   LONG rc;
   BYTE buffer[1024];
   DWORD size = sizeof(buffer);
   if (RegQueryValueEx(hKey, arg, NULL, NULL, buffer, &size) == ERROR_SUCCESS)
   {
      if (!_tcscmp(arg, _T("DigitalProductId")))
      {
         OSVERSIONINFO v;
         memset(&v, 0, sizeof(OSVERSIONINFO));
         v.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
         GetVersionEx(&v);
         if ((v.dwMajorVersion > 6) || ((v.dwMajorVersion == 6) && (v.dwMinorVersion >= 2)))
            DecodeProductKeyWin8(buffer, value);
         else
            DecodeProductKeyWin7(buffer, value);
      }
      else
      {
         ret_string(value, reinterpret_cast<TCHAR*>(buffer));
      }
      rc = SYSINFO_RC_SUCCESS;
   }
   else
   {
      TCHAR buffer[1024];
      nxlog_debug_tag(DEBUG_TAG, 5, _T("H_SystemProductInfo: Cannot read registry key %s (%s)"), arg, GetSystemErrorText(GetLastError(), buffer, 1024));
      rc = SYSINFO_RC_ERROR;
   }
   RegCloseKey(hKey);
   return rc;
}

/**
 * Handler for System.Memory.XXX parameters
 */
LONG H_MemoryInfo(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   MEMORYSTATUSEX mse;
   mse.dwLength = sizeof(MEMORYSTATUSEX);
   if (!GlobalMemoryStatusEx(&mse))
      return SYSINFO_RC_ERROR;

   switch (CAST_FROM_POINTER(arg, int))
   {
      case MEMINFO_PHYSICAL_AVAIL:
         ret_uint64(value, mse.ullAvailPhys);
         break;
      case MEMINFO_PHYSICAL_AVAIL_PCT:
         ret_double(value, ((double)mse.ullAvailPhys * 100.0 / mse.ullTotalPhys), 2);
         break;
      case MEMINFO_PHYSICAL_TOTAL:
         ret_uint64(value, mse.ullTotalPhys);
         break;
      case MEMINFO_PHYSICAL_USED:
         ret_uint64(value, mse.ullTotalPhys - mse.ullAvailPhys);
         break;
      case MEMINFO_PHYSICAL_USED_PCT:
         ret_double(value, (((double)mse.ullTotalPhys - mse.ullAvailPhys) * 100.0 / mse.ullTotalPhys), 2);
         break;
      case MEMINFO_VIRTUAL_FREE:
         ret_uint64(value, mse.ullAvailPageFile);
         break;
      case MEMINFO_VIRTUAL_FREE_PCT:
         ret_double(value, ((double)mse.ullAvailPageFile * 100.0 / mse.ullTotalPageFile), 2);
         break;
      case MEMINFO_VIRTUAL_TOTAL:
         ret_uint64(value, mse.ullTotalPageFile);
         break;
      case MEMINFO_VIRTUAL_USED:
         ret_uint64(value, mse.ullTotalPageFile - mse.ullAvailPageFile);
         break;
      case MEMINFO_VIRTUAL_USED_PCT:
         ret_double(value, (((double)mse.ullTotalPageFile - mse.ullAvailPageFile) * 100.0 / mse.ullTotalPageFile), 2);
         break;
      default:
         return SYSINFO_RC_UNSUPPORTED;
   }

   return SYSINFO_RC_SUCCESS;
}

/**
* Handler for System.Memory.XXX parameters
*/
LONG H_MemoryInfo2(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   PERFORMANCE_INFORMATION pi;
   if (!GetPerformanceInfo(&pi, sizeof(PERFORMANCE_INFORMATION)))
      return SYSINFO_RC_ERROR;

   switch (CAST_FROM_POINTER(arg, int))
   {
      case MEMINFO_PHYSICAL_FREE:
         ret_uint64(value, (pi.PhysicalAvailable - pi.SystemCache) * pi.PageSize);
         break;
      case MEMINFO_PHYSICAL_FREE_PCT:
         ret_double(value, ((double)(pi.PhysicalAvailable - pi.SystemCache) * 100.0 / pi.PhysicalTotal), 2);
         break;
      case MEMINFO_PHYSICAL_CACHE:
         ret_uint64(value, pi.SystemCache * pi.PageSize);
         break;
      case MEMINFO_PHYSICAL_CACHE_PCT:
         ret_double(value, ((double)pi.SystemCache * 100.0 / pi.PhysicalTotal), 2);
         break;
      default:
         return SYSINFO_RC_UNSUPPORTED;
   }

   return SYSINFO_RC_SUCCESS;
}
