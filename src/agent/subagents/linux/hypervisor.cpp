/*
** NetXMS subagent for GNU/Linux
** Copyright (C) 2004-2020 Raden Solutions
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

#if HAVE_CPUID_H
#include <cpuid.h>
#endif

/**
 * CPU vendor ID
 */
static char s_cpuVendorId[16] = "UNKNOWN";

/**
 * Read CPU vendor ID
 */
void ReadCPUVendorId()
{
#if HAVE_GET_CPUID
   unsigned int eax, ebx, ecx, edx;
   if (__get_cpuid(0, &eax, &ebx, &ecx, &edx) == 1)
   {
      memcpy(s_cpuVendorId, &ebx, 4);
      memcpy(&s_cpuVendorId[4], &edx, 4);
      memcpy(&s_cpuVendorId[8], &ecx, 4);
      s_cpuVendorId[12] = 0;
   }
#endif
}

/**
 * Check if /proc/1/sched reports PID different from 1 (won't work on kernel 4.15 or higher)
 */
static bool CheckPid1Sched()
{
   FILE *f = fopen("/proc/1/sched", "r");
   if (f == NULL)
      return false;

   bool result = false;

   char line[1024] = "";
   if (fgets(line, 1024, f) != NULL)
   {
      char *p1 = strrchr(line, '(');
      if (p1 != NULL)
      {
         p1++;
         char *p2 = strchr(p1, ',');
         if (p2 != NULL)
         {
            *p2 = 0;
            result = (strtol(p1, NULL, 10) != 1);
         }
      }
   }
   
   fclose(f);
   return result;
}

/**
 * Check cgroup
 */
static bool DetectContainerByCGroup(char *detectedType)
{
   FILE *f = fopen("/proc/1/cgroup", "r");
   if (f == NULL)
      return false;

   bool result = false;
   char line[1024];
   while(!feof(f))
   {
      if (fgets(line, 1024, f) == NULL)
         break;

      char *p = strchr(line, ':');
      if (p == NULL)
         continue;

      p++;
      p = strchr(p, ':');
      if (p == NULL)
         continue;

      p++;
      if (!strncmp(p, "/docker/", 8) || !strncmp(p, "/ecs/", 5))
      {
         result = true;
         if (detectedType != NULL)
            strcpy(detectedType, "Docker");
         break;
      }

      if (!strncmp(p, "/lxc/", 5))
      {
         result = true;
         if (detectedType != NULL)
            strcpy(detectedType, "LXC");
         break;
      }
   }

   fclose(f);
   return result;
}

/**
 * Check for OpenVZ container
 */
static bool IsOpenVZ()
{
   return (access("/proc/vz/vzaquota", R_OK) == 0) ||
          (access("/proc/user_beancounters", R_OK) == 0);
}

/**
 * Check Linux-VServer container
 */
static bool IsLinuxVServer()
{
   FILE *f = fopen("/proc/self/status", "r");
   if (f == NULL)
      return false;

   bool result = false;
   char line[1024];
   while(!feof(f))
   {
      if (fgets(line, 1024, f) == NULL)
         break;

      if (strncmp(line, "VxID:", 5) && strncmp(line, "s_context:", 10))
         continue;

      char *p = strchr(line, ':');
      if (p == NULL)
         continue;

      p++;
      while(isspace(*p))
         p++;
      if (strtol(p, NULL, 10) != 0) // ID 0 is for host
         result = true;
      break;
   }

   fclose(f);
   return result;
}

/**
 * Detect container from /proc/1/environ (requires root or CAP_SYS_PTRACE)
 */
static bool DetectContainerByInitEnv(char *detectedType)
{
   UINT32 size;
   char *env = reinterpret_cast<char*>(LoadFileA("/proc/1/environ", &size));
   if (env == NULL)
      return false;

   bool result = false;
   char *curr = env;
   while(curr < env + size)
   {
      if (!strncmp(curr, "container=", 10))
      {
         result = true;
         if (detectedType != NULL)
         {
            if (!strcmp(&curr[10], "lxc"))
               strcpy(detectedType, "LXC");
            else
               strcpy(detectedType, &curr[10]);
         }
         break;
      }
      curr += strlen(curr) + 1;
   }
   free(env);
   return result;
}

/**
 * Check if running within container
 */
static bool IsContainer()
{
   return 
      CheckPid1Sched() || 
      DetectContainerByCGroup(NULL) || 
      IsOpenVZ() || 
      IsLinuxVServer() ||
      DetectContainerByInitEnv(NULL);
}

/**
 * Check if running in virtual environment
 */
static VirtualizationType IsVirtual()
{
   // Check for container virtualization
   if (IsContainer())
      return VTYPE_CONTAINER;

   // Check for hardware virtualization
#if HAVE_GET_CPUID
   unsigned int eax, ebx, ecx, edx;
   if (__get_cpuid(0x1, &eax, &ebx, &ecx, &edx) == 1)
      return ((ecx & 0x80000000) != 0) ? VTYPE_FULL : VTYPE_NONE;
#endif
   return VTYPE_NONE;
}

/**
 * Check for VMware host
 */
static bool IsVMware()
{
   DIR *d = opendir("/sys/bus/pci/devices");
   if (d == NULL)
      return false;

   bool result = false;

   struct dirent *e;
   while(!result && ((e = readdir(d)) != NULL))
   {
      if (e->d_name[0] == '.')
         continue;

      char path[1024];
      snprintf(path, 1024, "/sys/bus/pci/devices/%s/vendor", e->d_name);

      UINT32 size;
      BYTE *content = LoadFileA(path, &size);
      if (content != NULL)
      {
         if (!strncasecmp((char *)content, "0x15ad", MIN(size, 6)))
            result = true;
         free(content);
      }
   }
   closedir(d);

   return result;
}

/**
 * Get VMware host version
 */
static bool GetVMwareVersionString(TCHAR *value)
{
   KeyValueOutputProcessExecutor pe(_T("vmware-toolbox-cmd stat raw text session"));
   if (!pe.execute())
      return false;
   if (!pe.waitForCompletion(1000))
      return false;

   const TCHAR *v = pe.getData()->get(_T("version"));
   if (v != NULL)
   {
      _tcslcpy(value, v, MAX_RESULT_LENGTH);
      return true;
   }
   return false;
}

/**
 * Check for XEN host
 */
static bool IsXEN()
{
   if (!strncmp(s_cpuVendorId, "XenVMM", 6))
      return true;

   UINT32 size;
   BYTE *content = LoadFileA("/sys/hypervisor/type", &size);
   if (content == nullptr)
      return false;
   
   bool result = (strncasecmp((char *)content, "xen", MIN(size, 3)) == 0);
   MemFree(content);
   return result;
}

/**
 * Get XEN host version
 */
static bool GetXENVersionString(TCHAR *value)
{
   UINT32 size;
   BYTE *content = LoadFileA("/sys/hypervisor/version/major", &size);
   if (content == NULL)
      return false;
   int major = strtol((char *)content, NULL, 10);
   free(content);

   content = LoadFileA("/sys/hypervisor/version/minor", &size);
   if (content == NULL)
      return false;
   int minor = strtol((char *)content, NULL, 10);
   free(content);

   const char *extra = "";
   content = LoadFileA("/sys/hypervisor/version/extra", &size);
   if (content != NULL)
   {
      char *ptr = strchr((char *)content, '\n');
      if (ptr != NULL)
         *ptr = 0;
      extra = (const char *)content;
   }

   _sntprintf(value, MAX_RESULT_LENGTH, _T("%d.%d%hs"), major, minor, extra);
   return true;
}

/**
 * Check for VirtualBox host
 */
static bool IsVirtualBox()
{
   return !strcmp(SMBIOS_GetHardwareProduct(), "VirtualBox");
}

/**
 * Get VirtualBox host version
 */
static bool GetVirtualBoxVersionString(TCHAR *value)
{
   const char * const *oemStrings = SMBIOS_GetOEMStrings();
   for(int i = 0; oemStrings[i] != NULL; i++)
   {
      if (!strncmp(oemStrings[i], "vboxVer_", 8))
      {
         _sntprintf(value, MAX_RESULT_LENGTH, _T("VirtualBox %hs"), oemStrings[i] + 8);
         return true;
      }
   }
   return false;
}

/**
 * Handler for Hypervisor.Type parameter
 */
LONG H_HypervisorType(const TCHAR *param, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   if (IsContainer())
   {
      if (IsOpenVZ())
      {
         ret_mbstring(value, "OpenVZ");
         return SYSINFO_RC_SUCCESS;
      }

      if (IsLinuxVServer())
      {
         ret_mbstring(value, "Linux-VServer");
         return SYSINFO_RC_SUCCESS;
      }

      char ctype[64];
      if (DetectContainerByCGroup(ctype) || DetectContainerByInitEnv(ctype))
      {
         ret_mbstring(value, ctype);
         return SYSINFO_RC_SUCCESS;
      }

      // Unknown container type, assume LXC
      ret_mbstring(value, "LXC");
      return SYSINFO_RC_SUCCESS;
   }

   if (IsXEN())
   {
      ret_mbstring(value, "XEN");
      return SYSINFO_RC_SUCCESS;
   }

   if (IsVMware())
   {
      ret_mbstring(value, "VMware");
      return SYSINFO_RC_SUCCESS;
   }

   const char *mf = SMBIOS_GetHardwareManufacturer();
   const char *prod = SMBIOS_GetHardwareProduct();

   if ((!strcmp(mf, "Microsoft Corporation") && !strcmp(prod, "Virtual Machine")) ||
       !strcmp(s_cpuVendorId, "Microsoft Hv"))
   {
      ret_mbstring(value, "Hyper-V");
      return SYSINFO_RC_SUCCESS;
   }

   if ((!strcmp(mf, "Red Hat") && !strcmp(prod, "KVM")) ||
       !strncmp(s_cpuVendorId, "KVM", 3))
   {
      ret_mbstring(value, "KVM");
      return SYSINFO_RC_SUCCESS;
   }

   if (!strcmp(mf, "QEMU"))
   {
      ret_mbstring(value, "QEMU");
      return SYSINFO_RC_SUCCESS;
   }

   if (!strcmp(mf, "Amazon EC2"))
   {
      ret_mbstring(value, "Amazon EC2");
      return SYSINFO_RC_SUCCESS;
   }

   if (IsVirtualBox())
   {
      ret_mbstring(value, "VirtualBox");
      return SYSINFO_RC_SUCCESS;
   }

   if (!strncmp(s_cpuVendorId, "bhyve", 5))
   {
      ret_mbstring(value, "bhyve");
      return SYSINFO_RC_SUCCESS;
   }

   if (!strcmp(s_cpuVendorId, " lrpepyh vr"))
   {
      ret_mbstring(value, "Parallels");
      return SYSINFO_RC_SUCCESS;
   }
   return SYSINFO_RC_UNSUPPORTED;
}

/**
 * Handler for Hypervisor.Version parameter
 */
LONG H_HypervisorVersion(const TCHAR *param, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   // Currently host details cannot be determined for any container
   if (IsContainer())
      return SYSINFO_RC_UNSUPPORTED;

   if (IsXEN() && GetXENVersionString(value))
      return SYSINFO_RC_SUCCESS;
   if (IsVMware() && GetVMwareVersionString(value))
      return SYSINFO_RC_SUCCESS;
   if (IsVirtualBox() && GetVirtualBoxVersionString(value))
      return SYSINFO_RC_SUCCESS;
   if (!strcmp(SMBIOS_GetHardwareManufacturer(), "Amazon EC2"))
   {
      ret_mbstring(value, SMBIOS_GetHardwareProduct());
      return SYSINFO_RC_SUCCESS;
   }
   return SYSINFO_RC_UNSUPPORTED;
}

/**
 * Handler for System.IsVirtual parameter
 */
LONG H_IsVirtual(const TCHAR *param, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   ret_int(value, IsVirtual());
   return SYSINFO_RC_SUCCESS;
}

/**
 * Handler for System.CPU.VendorId parameter
 */
LONG H_CpuVendorId(const TCHAR *param, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   ret_mbstring(value, s_cpuVendorId);
   return SYSINFO_RC_SUCCESS;
}
