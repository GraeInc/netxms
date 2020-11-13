/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2020 Victor Kirhenshtein
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
** File: debug.cpp
**
**/

#include "nxcore.h"

#ifdef _WIN32
#include <dbghelp.h>
#endif

/**
 * Callback for counting DCIs
 */
static void DciCountCallback(NetObj *object, void *data)
{
   if (object->isDataCollectionTarget())
      *static_cast<int*>(data) += static_cast<DataCollectionTarget*>(object)->getItemCount();
}

/**
 * Show server statistics
 */
void ShowServerStats(CONSOLE_CTX console)
{
	int dciCount = 0;
	g_idxObjectById.forEach(DciCountCallback, &dciCount);

	UINT32 s = static_cast<UINT32>(time(NULL) - g_serverStartTime);
   UINT32 d = s / 86400;
   s -= d * 86400;
   UINT32 h = s / 3600;
   s -= h * 3600;
   UINT32 m = s / 60;
   s -= m * 60;

   TCHAR uptime[128];
   _sntprintf(uptime, 128, _T("%u days, %2u:%02u:%02u"), d, h, m, s);

   ConsolePrintf(console, _T("Objects............: %d\n")
                          _T("Monitored nodes....: %d\n")
                          _T("Collectible DCIs...: %d\n")
                          _T("Active alarms......: %d\n")
                          _T("Uptime.............: %s\n")
                          _T("\n"),
	              g_idxObjectById.size(), g_idxNodeById.size(), dciCount, GetAlarmCount(), uptime);
}

/**
 * Show queue stats from function
 */
void ShowQueueStats(CONSOLE_CTX console, int64_t size, const TCHAR *name)
{
   ConsolePrintf(console, _T("%-32s : %u\n"), name, static_cast<unsigned int>(size));
}

/**
 * Show queue stats
 */
void ShowQueueStats(CONSOLE_CTX console, const Queue *queue, const TCHAR *name)
{
   if (queue != nullptr)
      ShowQueueStats(console, queue->size(), name);
}

/**
 * Show queue stats
 */
void ShowThreadPoolPendingQueue(CONSOLE_CTX console, ThreadPool *p, const TCHAR *name)
{
   int size;
   if (p != nullptr)
   {
      ThreadPoolInfo info;
      ThreadPoolGetInfo(p, &info);
      size = (info.activeRequests > info.curThreads) ? info.activeRequests - info.curThreads : 0;
   }
   else
   {
      size = 0;
   }
   ConsolePrintf(console, _T("%-32s : %d\n"), name, size);
}

/**
 * Show thread pool stats
 */
void ShowThreadPool(CONSOLE_CTX console, const TCHAR *p)
{
   ThreadPoolInfo info;
   if (ThreadPoolGetInfo(p, &info))
   {
      ConsolePrintf(console, _T("\x1b[1m%s\x1b[0m\n")
                             _T("   Threads.............. %d (%d/%d)\n")
                             _T("   Load average......... %0.2f %0.2f %0.2f\n")
                             _T("   Current load......... %d%%\n")
                             _T("   Usage................ %d%%\n")
                             _T("   Active requests...... %d\n")
                             _T("   Scheduled requests... %d\n")
                             _T("   Total requests....... ") UINT64_FMT _T("\n")
                             _T("   Thread starts........ ") UINT64_FMT _T("\n")
                             _T("   Thread stops......... ") UINT64_FMT _T("\n")
                             _T("   Average wait time.... %u ms\n\n"),
                    info.name, info.curThreads, info.minThreads, info.maxThreads,
                    info.loadAvg[0], info.loadAvg[1], info.loadAvg[2],
                    info.load, info.usage, info.activeRequests, info.scheduledRequests,
                    info.totalRequests, info.threadStarts, info.threadStops,
                    info.averageWaitTime);
   }
}

/**
 * Get thread pool stat (for internal DCI)
 */
DataCollectionError GetThreadPoolStat(ThreadPoolStat stat, const TCHAR *param, TCHAR *value)
{
   TCHAR poolName[64], options[64];
   if (!AgentGetParameterArg(param, 1, poolName, 64) ||
       !AgentGetParameterArg(param, 2, options, 64))
      return DCE_NOT_SUPPORTED;

   ThreadPoolInfo info;
   if (!ThreadPoolGetInfo(poolName, &info))
      return DCE_NOT_SUPPORTED;

   switch(stat)
   {
      case THREAD_POOL_CURR_SIZE:
         ret_int(value, info.curThreads);
         break;
      case THREAD_POOL_LOAD:
         ret_int(value, info.load);
         break;
      case THREAD_POOL_LOADAVG_1:
         if ((options[0] != 0) && _tcstol(options, NULL, 10))
            ret_double(value, info.loadAvg[0] / info.maxThreads, 2);
         else
            ret_double(value, info.loadAvg[0], 2);
         break;
      case THREAD_POOL_LOADAVG_5:
         if ((options[0] != 0) && _tcstol(options, NULL, 10))
            ret_double(value, info.loadAvg[1] / info.maxThreads, 2);
         else
            ret_double(value, info.loadAvg[1], 2);
         break;
      case THREAD_POOL_LOADAVG_15:
         if ((options[0] != 0) && _tcstol(options, NULL, 10))
            ret_double(value, info.loadAvg[2] / info.maxThreads, 2);
         else
            ret_double(value, info.loadAvg[2], 2);
         break;
      case THREAD_POOL_MAX_SIZE:
         ret_int(value, info.maxThreads);
         break;
      case THREAD_POOL_MIN_SIZE:
         ret_int(value, info.minThreads);
         break;
      case THREAD_POOL_ACTIVE_REQUESTS:
         ret_int(value, info.activeRequests);
         break;
      case THREAD_POOL_SCHEDULED_REQUESTS:
         ret_int(value, info.scheduledRequests);
         break;
      case THREAD_POOL_USAGE:
         ret_int(value, info.usage);
         break;
      case THREAD_POOL_AVERAGE_WAIT_TIME:
         ret_uint(value, info.averageWaitTime);
         break;
      default:
         return DCE_NOT_SUPPORTED;
   }
   return DCE_SUCCESS;
}

/**
 * Write process coredump
 */
#ifdef _WIN32

void DumpProcess(CONSOLE_CTX console)
{
	ConsolePrintf(console, _T("Dumping process to disk...\n"));

	TCHAR cmdLine[MAX_PATH + 64];
	_sntprintf(cmdLine, MAX_PATH + 64, _T("netxmsd.exe --dump-dir \"%s\" --dump %d"), g_szDumpDir, GetCurrentProcessId());

	PROCESS_INFORMATION pi;
   STARTUPINFO si;
	memset(&si, 0, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	if (CreateProcess(NULL, cmdLine, NULL, NULL, FALSE,
            (g_flags & AF_DAEMON) ? CREATE_NO_WINDOW : 0, NULL, NULL, &si, &pi))
	{
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		
		ConsolePrintf(console, _T("Done.\n"));
	}
	else
	{
		TCHAR buffer[256];
		ConsolePrintf(console, _T("Dump error: CreateProcess() failed (%s)\n"), GetSystemErrorText(GetLastError(), buffer, 256));
	}
}

#else

void DumpProcess(CONSOLE_CTX console)
{
	ConsolePrintf(console, _T("DUMP command is not supported for current operating system\n"));
}

#endif
