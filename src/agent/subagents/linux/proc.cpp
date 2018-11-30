/* 
** NetXMS subagent for GNU/Linux
** Copyright (C) 2004-2018 Raden Solutions
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
**/

#include "linux_subagent.h"

/**
 * Maximum possible length of process name
 */
#define MAX_PROCESS_NAME_LEN  32

/**
 * File descriptor
 */
class FileDescriptor
{
public:   
   int handle;
   char *name;
   
   FileDescriptor(struct dirent *e, const char *base)
   {
      handle = strtol(e->d_name, NULL, 10);
      
      char path[MAX_PATH];
      snprintf(path, MAX_PATH, "%s/%s", base, e->d_name);
      
      char fname[MAX_PATH];
      int len = (int)readlink(path, fname, MAX_PATH - 1);
      if (len >= 0)
      {
         fname[len] = 0;
         name = strdup(fname);
      }
      else
      {
         name = strdup("");
      }
   }
   
   ~FileDescriptor()
   {
      free(name);
   }
};

/**
 * Process entry
 */
class Process
{
public:
	UINT32 pid;
	char name[MAX_PROCESS_NAME_LEN];
	UINT32 parent;          // PID of parent process
	UINT32 group;           // Group ID
	char state;             // Process state
	long threads;           // Number of threads
	unsigned long ktime;    // Number of ticks spent in kernel mode
	unsigned long utime;    // Number of ticks spent in user mode
	unsigned long vmsize;   // Size of process's virtual memory in bytes
	long rss;               // Process's resident set size in pages
	unsigned long minflt;   // Number of minor page faults
	unsigned long majflt;   // Number of major page faults
   ObjectArray<FileDescriptor> *fd;
   
   Process(UINT32 _pid, const char *_name)
   {
      pid = _pid;
      strlcpy(name, _name, MAX_PROCESS_NAME_LEN);
      parent = 0;
      group = 0;
      state = '?';
      threads = 0;
      ktime = 0;
      utime = 0;
      vmsize = 0;
      rss = 0;
      minflt = 0;
      majflt = 0;
      fd = NULL;
   }
   
   ~Process()
   {
      delete fd;
   }
};

/**
 * Filter for reading only pid directories from /proc
 */
static int ProcFilter(const struct dirent *pEnt)
{
	char *pTmp;

	if (pEnt == NULL)
	{
		return 0; // ignore file
	}

	pTmp = (char *)pEnt->d_name;
	while (*pTmp != 0)
	{
		if (*pTmp < '0' || *pTmp > '9')
		{
			return 0; // skip
		}
		pTmp++;
	}

	return 1;
}

/**
 * Read handles
 */
static ObjectArray<FileDescriptor> *ReadProcessHandles(UINT32 pid)
{
   char path[MAX_PATH];
   snprintf(path, MAX_PATH, "/proc/%u/fd", pid);
   
	struct dirent **handles;
	int count = scandir(path, &handles, ProcFilter, alphasort);
   if (count < 0)
      return NULL;
      
   ObjectArray<FileDescriptor> *fd = new ObjectArray<FileDescriptor>(count, 16, true);
   while(count-- > 0)
   {
      fd->add(new FileDescriptor(handles[count], path));
      free(handles[count]);
   }
   free(handles);
   return fd;
}

/**
 * Read process information from /proc system
 * Parameters:
 *    plist    - array to fill, can be NULL
 *    procNameFilter - If not NULL, only processes with matched name will
 *               be counted and read. If cmdLineFilter is NULL, then exact
 *               match required to pass filter; otherwise procNameFilter can
 *               be a regular expression.
 *    cmdLineFilter - If not NULL, only processes with command line matched to
 *              regular expression will be counted and read.
 *    procUser - If not NULL, only processes run by this user will be counted.
 * Return value: number of matched processes or -1 in case of error.
 */
static int ProcRead(ObjectArray<Process> *plist, const char *procNameFilter, const char *cmdLineFilter, const char *procUser, bool readHandles)
{
	AgentWriteDebugLog(5, _T("ProcRead(%p, \"%hs\",\"%hs\",\"%hs\")"), plist, CHECK_NULL_A(procNameFilter), CHECK_NULL_A(cmdLineFilter), CHECK_NULL_A(procUser));

   uid_t procUid = -1;
   if (procUser != NULL && *procUser != 0)
   {
      struct passwd pwd;
      struct passwd *result;
      char *buf = (char*)malloc(16384);

      getpwnam_r(procUser, &pwd, buf, 16384, &result);
      if (result == NULL)
      {
         return -2; //If user is set, but it's not found return unsupported
      }
      procUid = pwd.pw_uid;
      free(buf);
   }

	struct dirent **nameList;
	int count = scandir("/proc", &nameList, ProcFilter, alphasort);
   if (count < 0)
      return -1;
   if (count == 0)
   {
      free(nameList);
      return -1;  // consider 0 as error as there should not be 0 processes
   }
   
   // get process count without filtering, we can skip long loop
	if ((plist == NULL) && (procNameFilter == NULL) && (cmdLineFilter == NULL) && (procUser == NULL))
	{
      for(int i = 0; i < count; i++)
         free(nameList[i]);
      free(nameList);
		return count;
	}

   int found = 0;
   while(count-- > 0)
   {
      bool procFound = false;
      bool cmdFound = false;
      bool uidFound = true;
      char *pProcName = NULL;
      char *pProcStat = NULL;
      unsigned long nPid = 0;

      char fileName[MAX_PATH], procNameBuffer[MAX_PROCESS_NAME_LEN + 1];

      snprintf(fileName, MAX_PATH, "/proc/%s/comm", nameList[count]->d_name);
      int hFile = _open(fileName, O_RDONLY);
      if (hFile != -1)
      {
         ssize_t bytes = _read(hFile, procNameBuffer, sizeof(procNameBuffer) - 1);
         if (bytes > 0)
         {
            procNameBuffer[bytes] = 0;
            char *p = strrchr(procNameBuffer, '\n');
            if (p != NULL)
               *p = 0;
            pProcName = procNameBuffer;
         }
         _close(hFile);
      }

      snprintf(fileName, MAX_PATH, "/proc/%s/stat", nameList[count]->d_name);
      hFile = _open(fileName, O_RDONLY);
      if (hFile != -1)
      {
         char szProcStat[1024];
         ssize_t bytes = _read(hFile, szProcStat, sizeof(szProcStat) - 1);
         if (bytes > 0)
         {
            szProcStat[bytes] = 0;
            if (sscanf(szProcStat, "%lu ", &nPid) == 1)
            {
               char *procNamePos = strchr(szProcStat, '(');
               if (procNamePos != NULL)
               {
                  if (pProcName != NULL)
                  {
                     // Already got process name from comm file
                     pProcStat = procNamePos + strlen(pProcName) + 2;   // process name plus 2 brackets
                  }
                  else
                  {
                     pProcStat = strchr(procNamePos, ')');
                     if (pProcStat != NULL)
                     {
                        pProcName = procNamePos + 1;
                        *pProcStat = 0;
                        pProcStat++;
                     }
                  }
                  if (pProcName != NULL)
                  {
                     if ((procNameFilter != NULL) && (*procNameFilter != 0))
                     {
                        if (cmdLineFilter == NULL) // use old style compare
                           procFound = (strcmp(pProcName, procNameFilter) == 0);
                        else
                           procFound = RegexpMatchA(pProcName, procNameFilter, FALSE);
                     }
                     else
                     {
                        procFound = true;
                     }
                  }
               } // pProcName != NULL
            }
         } // _read
         _close(hFile);
      } // hFile

      if (procUid != -1)
      {
         snprintf(fileName, MAX_PATH, "/proc/%s/", nameList[count]->d_name);
         struct stat fileInfo;
         if (stat(fileName, &fileInfo) == 0)
            uidFound = fileInfo.st_uid == procUid;
         else
            uidFound = false;
      }

      if ((cmdLineFilter != NULL) && (*cmdLineFilter != 0))
      {
         snprintf(fileName, MAX_PATH, "/proc/%s/cmdline", nameList[count]->d_name);
         hFile = _open(fileName, O_RDONLY);
         if (hFile != -1)
         {
            size_t len = 0, pos = 0;
            char *processCmdLine = (char *)malloc(4096);
            while(true)
            {
               ssize_t bytes = _read(hFile, &processCmdLine[pos], 4096);
               if (bytes < 0)
                  bytes = 0;
               len += bytes;
               if (bytes < 1024)
               {
                  processCmdLine[len] = 0;
                  break;
               }
               pos += bytes;
               processCmdLine = (char *)realloc(processCmdLine, pos + 4096);
            }
            if (len > 0)
            {
               // got a valid record in format: argv[0]\x00argv[1]\x00...
               // Note: to behave identicaly on different platforms,
               // full command line including argv[0] should be matched
               // replace 0x00 with spaces
               for(int j = 0; j < len - 1; j++)
               {
                  if (processCmdLine[j] == 0)
                  {
                     processCmdLine[j] = ' ';
                  }
               }
               cmdFound = RegexpMatchA(processCmdLine, cmdLineFilter, TRUE);
            }
            else
            {
               cmdFound = RegexpMatchA("", cmdLineFilter, TRUE);
            }
            _close(hFile);
            free(processCmdLine);
         } // hFile != -1
         else
         {
            cmdFound = RegexpMatchA("", cmdLineFilter, TRUE);
         }
      } // cmdLineFilter
      else
      {
         cmdFound = true;
      }

      if (procFound && cmdFound && uidFound)
      {
         if ((plist != NULL) && (pProcName != NULL))
         {
            Process *p = new Process(nPid, pProcName);
            // Parse rest of /proc/pid/stat file
            if (pProcStat != NULL)
            {
               if (sscanf(pProcStat, " %c %d %d %*d %*d %*d %*u %lu %*u %lu %*u %lu %lu %*u %*u %*d %*d %ld %*d %*u %lu %ld ",
                          &p->state, &p->parent, &p->group, &p->minflt, &p->majflt,
                          &p->utime, &p->ktime, &p->threads, &p->vmsize, &p->rss) != 10)
               {
                  AgentWriteDebugLog(2, _T("Error parsing /proc/%d/stat"), nPid);
               }
            }
            p->fd = readHandles ? ReadProcessHandles(nPid) : NULL;
            plist->add(p);
         }
         found++;
      }

      free(nameList[count]);
   }
   free(nameList);
	return found;
}

/**
 * Handler for System.ProcessCount, Process.Count() and Process.CountEx() parameters
 */
LONG H_ProcessCount(const TCHAR *pszParam, const TCHAR *pArg, TCHAR *pValue, AbstractCommSession *session)
{
	int nRet = SYSINFO_RC_ERROR;
	char procNameFilter[MAX_PATH] = "", cmdLineFilter[MAX_PATH] = "", userFilter[256] = "";
	int nCount = -1;

	if (*pArg != _T('T'))
	{
		AgentGetParameterArgA(pszParam, 1, procNameFilter, sizeof(procNameFilter));
		if (*pArg == _T('E'))
		{
			AgentGetParameterArgA(pszParam, 2, cmdLineFilter, sizeof(cmdLineFilter));
			AgentGetParameterArgA(pszParam, 3, userFilter, sizeof(userFilter));
		}
	}

	nCount = ProcRead(NULL, (*pArg != _T('T')) ? procNameFilter : NULL, (*pArg == _T('E')) ? cmdLineFilter : NULL, (*pArg == _T('E')) ? userFilter : NULL, false);

	if (nCount == -2)
   {
      nRet = SYSINFO_RC_UNSUPPORTED;
   }
	if (nCount >= 0)
	{
      ret_int(pValue, nCount);
      nRet = SYSINFO_RC_SUCCESS;
	}

	return nRet;
}

/**
 * Handler for System.ThreadCount parameter
 */
LONG H_ThreadCount(const TCHAR *param, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
	int i, sum, count, ret = SYSINFO_RC_ERROR;
	ObjectArray<Process> procList(128, 128, true);

	count = ProcRead(&procList, NULL, NULL, NULL, false);
	if (count >= 0)
	{
		for(i = 0, sum = 0; i < procList.size(); i++)
			sum += procList.get(i)->threads;
		ret_int(value, sum);
		ret = SYSINFO_RC_SUCCESS;
	}

	return ret;
}

/**
 * Handler for System.HandleCount parameter
 */
LONG H_HandleCount(const TCHAR *param, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
	int i, sum, count, ret = SYSINFO_RC_ERROR;
	ObjectArray<Process> procList(128, 128, true);

	count = ProcRead(&procList, NULL, NULL, NULL, true);
	if (count >= 0)
	{
		for(i = 0, sum = 0; i < procList.size(); i++)
      {
         if (procList.get(i)->fd != NULL)
            sum += procList.get(i)->fd->size();
      }
		ret_int(value, sum);
		ret = SYSINFO_RC_SUCCESS;
	}

	return ret;
}

/**
 * Count VM regions within process
 */
static INT64 CountVMRegions(UINT32 pid)
{
   char fname[128];
   sprintf(fname, "/proc/%u/maps", pid);
   int f = _open(fname, O_RDONLY);
   if (f == -1)
      return 0;

   INT64 count = 0;
   char buffer[16384];
   while(true)
   {
      ssize_t bytes = _read(f, buffer, 16384);
      if (bytes <= 0)
         break;
      char *p = buffer;
      for(ssize_t i = 0; i < bytes; i++)
         if (*p++ == '\n')
            count++;
   }

   _close(f);
   return count;
}

/**
 * Handler for Process.xxx() parameters
 * Parameter has the following syntax:
 *    Process.XXX(<process>,<type>,<cmdline>)
 * where
 *    XXX        - requested process attribute (see documentation for list of valid attributes)
 *    <process>  - process name (same as in Process.Count() parameter)
 *    <type>     - representation type (meaningful when more than one process with the same
 *                 name exists). Valid values are:
 *         min - minimal value among all processes named <process>
 *         max - maximal value among all processes named <process>
 *         avg - average value for all processes named <process>
 *         sum - sum of values for all processes named <process>
 *    <cmdline>  - command line
 *    <user>   - user name  (same as in Process.Count() parameter)
 */
LONG H_ProcessDetails(const TCHAR *param, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
	int i, count, type;
	INT64 currVal, finalVal;
	char procNameFilter[MAX_PATH], cmdLineFilter[MAX_PATH], buffer[256], userFilter[256] = "";
   static const char *typeList[]={ "min", "max", "avg", "sum", NULL };

   // Get parameter type arguments
   AgentGetParameterArgA(param, 2, buffer, 256);
   if (buffer[0] == 0)     // Omited type
   {
      type = INFOTYPE_SUM;
   }
   else
   {
      for(type = 0; typeList[type] != NULL; type++)
         if (!stricmp(typeList[type], buffer))
            break;
      if (typeList[type] == NULL)
         return SYSINFO_RC_UNSUPPORTED;     // Unsupported type
   }

	// Get process name
	AgentGetParameterArgA(param, 1, procNameFilter, MAX_PATH);
	AgentGetParameterArgA(param, 3, cmdLineFilter, MAX_PATH);
   AgentGetParameterArgA(param, 4, userFilter, sizeof(userFilter));
	StrStripA(cmdLineFilter);

	ObjectArray<Process> procList(128, 128, true);
	count = ProcRead(&procList, procNameFilter, (cmdLineFilter[0] != 0) ? cmdLineFilter : NULL,
	         (userFilter[0] != 0) ? userFilter : NULL, CAST_FROM_POINTER(arg, int) == PROCINFO_HANDLES);
	AgentWriteDebugLog(5, _T("H_ProcessDetails(\"%hs\"): ProcRead() returns %d"), param, count);
	if (count == -1)
		return SYSINFO_RC_ERROR;
	if (count == -2)
	      return SYSINFO_RC_UNSUPPORTED;

	long pageSize = getpagesize();
	long ticksPerSecond = sysconf(_SC_CLK_TCK);
	for(i = 0, finalVal = 0; i < procList.size(); i++)
	{
      Process *p = procList.get(i);
		switch(CAST_FROM_POINTER(arg, int))
		{
			case PROCINFO_CPUTIME:
				currVal = (p->ktime + p->utime) * 1000 / ticksPerSecond;
				break;
			case PROCINFO_HANDLES:
				currVal = (p->fd != NULL) ? p->fd->size() : 0;
				break;
			case PROCINFO_KTIME:
				currVal = p->ktime * 1000 / ticksPerSecond;
				break;
			case PROCINFO_UTIME:
				currVal = p->utime * 1000 / ticksPerSecond;
				break;
			case PROCINFO_PAGEFAULTS:
				currVal = p->majflt + p->minflt;
				break;
			case PROCINFO_THREADS:
				currVal = p->threads;
				break;
         case PROCINFO_VMREGIONS:
            currVal = CountVMRegions(p->pid);
            break;
			case PROCINFO_VMSIZE:
				currVal = p->vmsize;
				break;
			case PROCINFO_WKSET:
				currVal = p->rss * pageSize;
				break;
			default:
				currVal = 0;
				break;
		}

		switch(type)
		{
			case INFOTYPE_SUM:
			case INFOTYPE_AVG:
				finalVal += currVal;
				break;
			case INFOTYPE_MIN:
				finalVal = std::min(currVal, finalVal);
				break;
			case INFOTYPE_MAX:
				finalVal = std::max(currVal, finalVal);
				break;
		}
	}

	if (type == INFOTYPE_AVG)
		finalVal /= count;

	ret_int64(value, finalVal);
	return SYSINFO_RC_SUCCESS;
}

/**
 * Handler for System.ProcessList list
 */
LONG H_ProcessList(const TCHAR *pszParam, const TCHAR *pArg, StringList *value, AbstractCommSession *session)
{
   int nRet = SYSINFO_RC_ERROR;

	ObjectArray<Process> procList(128, 128, true);
   int nCount = ProcRead(&procList, NULL, NULL, NULL, false);
   if (nCount >= 0)
   {    
      nRet = SYSINFO_RC_SUCCESS;

      for(int i = 0; i < procList.size(); i++)
      {         
         Process *p = procList.get(i);
         TCHAR szBuff[128];
         _sntprintf(szBuff, sizeof(szBuff), _T("%d %hs"), p->pid, p->name);
         value->add(szBuff);
      }         
   }    
   return nRet;
}

/**
 * Handler for System.Processes table
 */
LONG H_ProcessTable(const TCHAR *cmd, const TCHAR *arg, Table *value, AbstractCommSession *session)
{
   value->addColumn(_T("PID"), DCI_DT_UINT, _T("PID"), true);
   value->addColumn(_T("NAME"), DCI_DT_STRING, _T("Name"));
   value->addColumn(_T("THREADS"), DCI_DT_UINT, _T("Threads"));
   value->addColumn(_T("HANDLES"), DCI_DT_UINT, _T("Handles"));
   value->addColumn(_T("KTIME"), DCI_DT_UINT64, _T("Kernel Time"));
   value->addColumn(_T("UTIME"), DCI_DT_UINT64, _T("User Time"));
   value->addColumn(_T("VMSIZE"), DCI_DT_UINT64, _T("VM Size"));
   value->addColumn(_T("RSS"), DCI_DT_UINT64, _T("RSS"));
   value->addColumn(_T("PAGE_FAULTS"), DCI_DT_UINT64, _T("Page Faults"));
   value->addColumn(_T("CMDLINE"), DCI_DT_STRING, _T("Command Line"));

   int rc = SYSINFO_RC_ERROR;

	ObjectArray<Process> procList(128, 128, true);
   int nCount = ProcRead(&procList, NULL, NULL, NULL, true);
   if (nCount >= 0)
   {    
      rc = SYSINFO_RC_SUCCESS;

      UINT64 pageSize = getpagesize();
      UINT64 ticksPerSecond = sysconf(_SC_CLK_TCK);
      for(int i = 0; i < procList.size(); i++)
      {         
         Process *p = procList.get(i);
         value->addRow();
         value->set(0, p->pid);
#ifdef UNICODE
         value->setPreallocated(1, WideStringFromMBString(p->name));
#else
         value->set(1, p->name);
#endif
         value->set(2, (UINT32)p->threads);
         value->set(3, (UINT32)((p->fd != NULL) ? p->fd->size() : 0));
         value->set(4, (UINT64)p->ktime * 1000 / ticksPerSecond);
         value->set(5, (UINT64)p->utime * 1000 / ticksPerSecond);
         value->set(6, (UINT64)p->vmsize);
         value->set(7, (UINT64)p->rss * pageSize);
         value->set(8, (UINT64)p->minflt + (UINT64)p->majflt);
      }         
   }    
   return rc;
}

/**
 * Handler for System.OpenFiles table
 */
LONG H_OpenFilesTable(const TCHAR *cmd, const TCHAR *arg, Table *value, AbstractCommSession *session)
{
   value->addColumn(_T("PID"), DCI_DT_UINT, _T("PID"), true);
   value->addColumn(_T("PROCNAME"), DCI_DT_STRING, _T("Process"));
   value->addColumn(_T("HANDLE"), DCI_DT_UINT, _T("Handle"), true);
   value->addColumn(_T("NAME"), DCI_DT_STRING, _T("Name"));

   int rc = SYSINFO_RC_ERROR;

	ObjectArray<Process> procList(128, 128, true);
   int nCount = ProcRead(&procList, NULL, NULL, NULL, true);
   if (nCount >= 0)
   {    
      rc = SYSINFO_RC_SUCCESS;

      for(int i = 0; i < procList.size(); i++)
      {         
         Process *p = procList.get(i);
         if (p->fd != NULL)
         {
            for(int j = 0; j < p->fd->size(); j++)
            {
               FileDescriptor *f = p->fd->get(j);
               value->addRow();
               value->set(0, p->pid);
               value->set(2, f->handle);
#ifdef UNICODE
               value->setPreallocated(1, WideStringFromMBString(p->name));
               value->setPreallocated(3, WideStringFromMBString(f->name));
#else
               value->set(1, p->name);
               value->set(3, f->name);
#endif
            }
         }
      }         
   }    
   return rc;
}
