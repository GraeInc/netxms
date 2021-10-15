/* 
** NetXMS subagent for Darwin
** Copyright (C) 2021 Raden Solutions
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

#include "darwin.h"
#include <libproc.h>
/**
 * Maximum possible length of process name
 */
#define MAX_PROCESS_NAME_LEN 32

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
      handle = strtol(e->d_name, nullptr, 10);

      char path[MAX_PATH];
      snprintf(path, MAX_PATH, "%s/%s", base, e->d_name);

      char fname[MAX_PATH];
      int len = (int)readlink(path, fname, MAX_PATH - 1);
      if (len >= 0)
      {
         fname[len] = 0;
         name = MemCopyStringA(fname);
      }
      else
      {
         name = MemCopyStringA("");
      }
   }

   ~FileDescriptor()
   {
      MemFree(name);
   }
};

/**
 * Process entry
 */
class Process
{
public:
   uint32_t pid;
   char name[MAX_PROCESS_NAME_LEN];
   uint32_t parent;      // PID of parent process
   uint32_t group;       // Group ID
   char state;           // Process state
   long threads;         // Number of threads
   unsigned long ktime;  // Number of ticks spent in kernel mode
   unsigned long utime;  // Number of ticks spent in user mode
   unsigned long vmsize; // Size of process's virtual memory in bytes
   long rss;             // Process's resident set size in pages
   unsigned long minflt; // Number of minor page faults
   unsigned long majflt; // Number of major page faults
   ObjectArray<FileDescriptor> *fd;
   char *cmdLine; // Process command line

   Process(uint32_t _pid, const char *_name, char *_cmdLine)
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
      fd = nullptr;
      cmdLine = _cmdLine;
   }

   ~Process()
   {
      delete fd;
      MemFree(cmdLine);
   }
};

/**
 * Filter for reading only pid directories from /proc
 */
static int ProcFilter(const struct dirent *pEnt)
{
   char *pTmp;

   if (pEnt == nullptr)
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

   ObjectArray<FileDescriptor> *fd = new ObjectArray<FileDescriptor>(count, 16, Ownership::True);
   while (count-- > 0)
   {
      fd->add(new FileDescriptor(handles[count], path));
      free(handles[count]);
   }
   free(handles);
   return fd;
}

/**
 * Build process command line
 */
/*static char* BuildProcessCommandLine(kvm_t *kd, struct kinfo_proc *p, size_t maxSize)
{
   char *cmdLine = 0;
   char **argv = kvm_getargv(kd, p, 0);
   if (argv != nullptr)
   {
      if (argv[0] != nullptr)
      {
         for (int i = 0, pos = 0; (argv[i] != nullptr) && (pos < maxSize); i++)
         {
            if (i > 0)
               cmdLine[pos++] = ' ';
            strlcpy(&cmdLine[pos], argv[i], maxSize - pos);
            pos += strlen(argv[i]);
         }
      }
      else
      {
         // Use process name if command line is empty
         cmdLine[0] = '[';
         strlcpy(&cmdLine[1], PNAME, maxSize - 2);
         strcat(cmdLine, "]");
      }
   }
   else
   {
      // Use process name if command line cannot be obtained
      cmdLine[0] = '[';
      strlcpy(&cmdLine[1], PNAME, maxSize - 2);
      strcat(cmdLine, "]");
   }
   return null;
}*/

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
static int ProcRead(int **pList, bool extended, const char *procNameFilter, const char *cmdLineFilter, const char *procUserFilter, bool readHandles, bool readCmdLine)
{
   nxlog_debug_tag(DEBUG_TAG, 5, _T("ProcRead(%p, \"%hs\",\"%hs\",\"%hs\")"), pList, CHECK_NULL_A(procNameFilter), CHECK_NULL_A(cmdLineFilter), CHECK_NULL_A(procUserFilter));

   int *pidList = new int[512];
   int pidListSize = 512 * sizeof(int);
   int count = proc_listallpids(pidList, pidListSize);
   if (count < 0)
      return -1;
   if (count == 0)
   {
      free(pidList);
      return -1; // consider 0 as error as there should not be 0 processes
   }

   //Increase pidList size if too small
   while (count == pidListSize / sizeof(int))
   {
      pidListSize += 512 * sizeof(int);
      delete[] pidList;
      pidList = new int[pidListSize];
      count = proc_listallpids(pidList, pidListSize);
   }
   printf("\nCOUNT = %i\n", count);
   printf("\nprocNameFilter = %s\n", procNameFilter);
   printf("\ncmdLineFilter = %s\n", cmdLineFilter);
   printf("\nprocUserFilter = %s\n", procUserFilter);

   // get process count without filtering, we can skip long loop
   if ((procNameFilter == nullptr || *procNameFilter == 0) &&
       (cmdLineFilter == nullptr || *cmdLineFilter == 0) &&
       (procUserFilter == nullptr || *procUserFilter == 0))
   {
      printf("\nSKIP CHECKS\n");
      if (pList != nullptr)
      {
         *pList = pidList;
      }
      free(pidList);
      return count;
   }

   int found = 0;
   while (count-- >= 0)
   {
      bool procFound = true, cmdFound = true, userFound = true;
      int pid = pidList[count];

      struct proc_bsdinfo proc;
      if(proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &proc, PROC_PIDTBSDINFO_SIZE) > 0)
      {
         printf("\nPPPP\n");
         //Process name match
         if ((procNameFilter != nullptr) && (*procNameFilter != 0))
         {
            char procName[256];
            int procNameSize = sizeof(procName);
            procFound = RegexpMatchA(proc.pbi_name, procNameFilter, false);
            //procFound = (strcmp(pProcName, procNameFilter) == 0);
         }

         //User name match
         if (procUserFilter != nullptr && *procUserFilter != 0)
         {
            passwd *resultbuf = new passwd();
            char buffer[512];
            size_t buflen = 512;
            passwd *pUserInfo = new passwd();
            getpwuid_r(proc.pbi_uid, resultbuf, buffer, buflen, &pUserInfo);
            if (pUserInfo != nullptr)
               userFound = RegexpMatchA(pUserInfo->pw_name, procUserFilter, true);
            else
               userFound = false;
         }

         /*
         kvm_t *kd = kvm_openfiles(NULL, "/dev/null", NULL, O_RDONLY, NULL);
         if (kd != nullptr)
         {
            struct kinfo_proc *kp = kvm_getprocs(kd, KERN_PROC_PROC, 0, &nCount);
            if (kp != nullptr)
            {
               printf("KD & KP ARE OK WORKING GOOD MY FRIEND");

               //Cmd line filter
               if (cmdLine != nullptr && *cmdLine != 0)
               {
                  char processCmdLine[32768];
                  BuildProcessCommandLine(kd, p, processCmdLine, sizeof(processCmdLine));
                  if (!RegexpMatchA(processCmdLine, cmdLine, true))
                     return false;
               }
               if (procFound && cmdFound && userFound)
               {
                  found++;
               }
            }
         }*/
      }
   }
   free(pidList);
   return found;
}

/**
 * Handler for System.ProcessCount, Process.Count() and Process.CountEx() parameters
 */
LONG H_ProcessCount(const TCHAR *pszParam, const TCHAR *pArg, TCHAR *pValue, AbstractCommSession *session)
{
   printf("\nI GOT MESSAGE\n");
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
   
   nCount = ProcRead(nullptr, true, (*pArg != _T('T')) ? procNameFilter : nullptr, (*pArg == _T('E')) ? cmdLineFilter : nullptr, (*pArg == _T('E')) ? userFilter : nullptr, false, false);

   if (nCount == -2)
   {
      nRet = SYSINFO_RC_UNSUPPORTED;
   }
   else if (nCount >= 0)
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
   /*int i, sum, count, ret = SYSINFO_RC_ERROR;
   ObjectArray<Process> procList(128, 128, Ownership::True);

   count = ProcRead(&procList, nullptr, nullptr, nullptr, false, false);
   if (count >= 0)
   {
      for (i = 0, sum = 0; i < procList.size(); i++)
         sum += procList.get(i)->threads;
      ret_int(value, sum);
      ret = SYSINFO_RC_SUCCESS;
   }
   return ret;*/
   return SYSINFO_RC_ERROR;
}

/**
 * Handler for System.HandleCount parameter
 */
LONG H_HandleCount(const TCHAR *param, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   /*int i, sum, count, ret = SYSINFO_RC_ERROR;
   ObjectArray<Process> procList(128, 128, Ownership::True);

   count = ProcRead(&procList, nullptr, nullptr, nullptr, true, false);
   if (count >= 0)
   {
      for (i = 0, sum = 0; i < procList.size(); i++)
      {
         if (procList.get(i)->fd != NULL)
            sum += procList.get(i)->fd->size();
      }
      ret_int(value, sum);
      ret = SYSINFO_RC_SUCCESS;
   }
   return ret;*/
   return SYSINFO_RC_ERROR;
}

/**
 * Count VM regions within process
 */
static int64_t CountVMRegions(uint32_t pid)
{
   char fname[128];
   sprintf(fname, "/proc/%u/maps", pid);
   int f = _open(fname, O_RDONLY);
   if (f == -1)
      return 0;

   int64_t count = 0;
   char buffer[16384];
   while (true)
   {
      ssize_t bytes = _read(f, buffer, 16384);
      if (bytes <= 0)
         break;
      char *p = buffer;
      for (ssize_t i = 0; i < bytes; i++)
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
  /* int i, count, type;
   INT64 currVal, finalVal;
   char procNameFilter[MAX_PATH], cmdLineFilter[MAX_PATH], buffer[256], userFilter[256] = "";
   static const char *typeList[] = {"min", "max", "avg", "sum", nullptr};

   // Get parameter type arguments
   AgentGetParameterArgA(param, 2, buffer, 256);
   if (buffer[0] == 0) // Omited type
   {
      type = INFOTYPE_SUM;
   }
   else
   {
      for (type = 0; typeList[type] != nullptr; type++)
         if (!stricmp(typeList[type], buffer))
            break;
      if (typeList[type] == nullptr)
         return SYSINFO_RC_UNSUPPORTED; // Unsupported type
   }

   // Get process name
   AgentGetParameterArgA(param, 1, procNameFilter, MAX_PATH);
   AgentGetParameterArgA(param, 3, cmdLineFilter, MAX_PATH);
   AgentGetParameterArgA(param, 4, userFilter, sizeof(userFilter));
   TrimA(cmdLineFilter);

   ObjectArray<Process> procList(128, 128, Ownership::True);
   count = ProcRead(&procList, procNameFilter, (cmdLineFilter[0] != 0) ? cmdLineFilter : nullptr,
                    (userFilter[0] != 0) ? userFilter : nullptr, CAST_FROM_POINTER(arg, int) == PROCINFO_HANDLES, false);
   nxlog_debug_tag(DEBUG_TAG, 5, _T("H_ProcessDetails(\"%hs\"): ProcRead() returns %d"), param, count);
   if (count == -1)
      return SYSINFO_RC_ERROR;
   if (count == -2)
      return SYSINFO_RC_UNSUPPORTED;

   long pageSize = getpagesize();
   long ticksPerSecond = sysconf(_SC_CLK_TCK);
   for (i = 0, finalVal = 0; i < procList.size(); i++)
   {
      Process *p = procList.get(i);
      switch (CAST_FROM_POINTER(arg, int))
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

      switch (type)
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

   ret_int64(value, finalVal);*/
   return SYSINFO_RC_ERROR;
}

/**
 * Handler for System.ProcessList list
 */
LONG H_ProcessList(const TCHAR *pszParam, const TCHAR *pArg, StringList *value, AbstractCommSession *session)
{
   /*int nRet = SYSINFO_RC_ERROR;
   ObjectArray<Process> procList(128, 128, Ownership::True);
   int nCount = ProcRead(&procList, nullptr, nullptr, nullptr, false, false);
   if (nCount >= 0)
   {
      nRet = SYSINFO_RC_SUCCESS;

      for (int i = 0; i < procList.size(); i++)
      {
         Process *p = procList.get(i);
         TCHAR szBuff[128];
         _sntprintf(szBuff, sizeof(szBuff), _T("%d %hs"), p->pid, p->name);
         value->add(szBuff);
      }
   }
   return nRet;*/
   return SYSINFO_RC_ERROR;
}

/**
 * Handler for System.Processes table
 */
LONG H_ProcessTable(const TCHAR *cmd, const TCHAR *arg, Table *value, AbstractCommSession *session)
{
   /*value->addColumn(_T("PID"), DCI_DT_UINT, _T("PID"), true);
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

   ObjectArray<Process> procList(128, 128, Ownership::True);
   int nCount = ProcRead(&procList, nullptr, nullptr, nullptr, true, true);
   if (nCount >= 0)
   {
      rc = SYSINFO_RC_SUCCESS;

      uint64_t pageSize = getpagesize();
      uint64_t ticksPerSecond = sysconf(_SC_CLK_TCK);
      for (int i = 0; i < procList.size(); i++)
      {
         Process *p = procList.get(i);
         value->addRow();
         value->set(0, p->pid);
#ifdef UNICODE
         value->setPreallocated(1, WideStringFromMBString(p->name));
#else
         value->set(1, p->name);
#endif
         value->set(2, static_cast<uint32_t>(p->threads));
         value->set(3, static_cast<uint32_t>((p->fd != nullptr) ? p->fd->size() : 0));
         value->set(4, static_cast<uint64_t>(p->ktime) * 1000 / ticksPerSecond);
         value->set(5, static_cast<uint64_t>(p->utime) * 1000 / ticksPerSecond);
         value->set(6, static_cast<uint64_t>(p->vmsize));
         value->set(7, static_cast<uint64_t>(p->rss) * pageSize);
         value->set(8, static_cast<uint64_t>(p->minflt) + static_cast<uint64_t>(p->majflt));
         value->set(9, p->cmdLine);
      }
   }
   return rc;*/
   return SYSINFO_RC_ERROR;
}

/**
 * Handler for System.OpenFiles table
 */
LONG H_OpenFilesTable(const TCHAR *cmd, const TCHAR *arg, Table *value, AbstractCommSession *session)
{
   /*value->addColumn(_T("PID"), DCI_DT_UINT, _T("PID"), true);
   value->addColumn(_T("PROCNAME"), DCI_DT_STRING, _T("Process"));
   value->addColumn(_T("HANDLE"), DCI_DT_UINT, _T("Handle"), true);
   value->addColumn(_T("NAME"), DCI_DT_STRING, _T("Name"));

   int rc = SYSINFO_RC_ERROR;

   ObjectArray<Process> procList(128, 128, Ownership::True);
   int nCount = ProcRead(&procList, nullptr, nullptr, nullptr, true, false);
   if (nCount >= 0)
   {
      rc = SYSINFO_RC_SUCCESS;

      for (int i = 0; i < procList.size(); i++)
      {
         Process *p = procList.get(i);
         if (p->fd != nullptr)
         {
            for (int j = 0; j < p->fd->size(); j++)
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
   return rc;*/
   return SYSINFO_RC_ERROR;
}