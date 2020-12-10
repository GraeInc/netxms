/*
 ** File management subagent
 ** Copyright (C) 2014-2020 Raden Solutions
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

#include "filemgr.h"

#ifdef _WIN32
#include <accctrl.h>
#include <aclapi.h>
#else
#include <pwd.h>
#include <grp.h>
#endif

/**
 * Root folders
 */
static ObjectArray<RootFolder> *s_rootDirectories;
static HashMap<uint32_t, VolatileCounter> *s_downloadFileStopMarkers;

/**
 * Monitored file list
 */
MonitoredFileList g_monitorFileList;


#ifdef _WIN32

/**
 * Convert path from UNIX to local format and do macro expansion
 */
static inline void ConvertPathToHost(TCHAR *path, bool allowPathExpansion, bool allowShellCommands)
{
   for(int i = 0; path[i] != 0; i++)
      if (path[i] == _T('/'))
         path[i] = _T('\\');

   if (allowPathExpansion)
      ExpandFileName(path, path, MAX_PATH, allowShellCommands);
}

#else

static inline void ConvertPathToHost(TCHAR *path, bool allowPathExpansion, bool allowShellCommands)
{
   if (allowPathExpansion)
      ExpandFileName(path, path, MAX_PATH, allowShellCommands);
}

#endif

#ifdef _WIN32

/**
 * Convert path from local to UNIX format
 */
static void ConvertPathToNetwork(TCHAR *path)
{
   for(int i = 0; path[i] != 0; i++)
      if (path[i] == _T('\\'))
         path[i] = _T('/');
}

#else

#define ConvertPathToNetwork(x)

#endif

/*
 * Create new RootFolder
 */
RootFolder::RootFolder(const TCHAR *folder)
{
   m_folder = _tcsdup(folder);
   m_readOnly = false;

   TCHAR *ptr = _tcschr(m_folder, _T(';'));
   if (ptr == NULL)
      return;
   ConvertPathToHost(m_folder, false, false);

   *ptr = 0;
   if (_tcscmp(ptr+1, _T("ro")) == 0)
      m_readOnly = true;
}

/**
 * Subagent initialization
 */
static bool SubagentInit(Config *config)
{
   s_rootDirectories = new ObjectArray<RootFolder>(16, 16, Ownership::True);
   s_downloadFileStopMarkers = new HashMap<uint32_t, VolatileCounter>(Ownership::True);
   ConfigEntry *root = config->getEntry(_T("/filemgr/RootFolder"));
   if (root != NULL)
   {
      for(int i = 0; i < root->getValueCount(); i++)
      {
         RootFolder *folder = new RootFolder(root->getValue(i));
         s_rootDirectories->add(folder);
         nxlog_debug_tag(DEBUG_TAG, 5, _T("Added file manager root directory \"%s\""), folder->getFolder());
      }
   }
   nxlog_debug_tag(DEBUG_TAG, 2, _T("File manager subagent initialized"));
   return true;
}

/**
 * Called by master agent at unload
 */
static void SubagentShutdown()
{
   delete s_rootDirectories;
   delete s_downloadFileStopMarkers;
}

#ifndef _WIN32

/**
 * Converts path to absolute removing "//", "../", "./" ...
 */
static TCHAR *GetRealPath(const TCHAR *path)
{
   if ((path == nullptr) || (path[0] == 0))
      return nullptr;
   TCHAR *result = MemAllocString(MAX_PATH);
   _tcscpy(result, path);
   TCHAR *current = result;

   // just remove all dots before path
   if (!_tcsncmp(current, _T("../"), 3))
      memmove(current, current + 3, (_tcslen(current+3) + 1) * sizeof(TCHAR));

   if (!_tcsncmp(current, _T("./"), 2))
      memmove(current, current + 2, (_tcslen(current+2) + 1) * sizeof(TCHAR));

   while(*current != 0)
   {
      if (current[0] == '/')
      {
         switch(current[1])
         {
            case '/':
               memmove(current, current + 1, _tcslen(current) * sizeof(TCHAR));
               break;
            case '.':
               if (current[2] != 0)
               {
                  if (current[2] == '.' && (current[3] == 0 || current[3] == '/'))
                  {
                     if (current == result)
                     {
                        memmove(current, current + 3, (_tcslen(current + 3) + 1) * sizeof(TCHAR));
                     }
                     else
                     {
                        TCHAR *tmp = current;
                        do
                        {
                           tmp--;
                           if (tmp[0] == '/')
                           {
                              break;
                           }
                        } while(result != tmp);
                        memmove(tmp, current + 3, (_tcslen(current+3) + 1) * sizeof(TCHAR));
                     }
                  }
                  else
                  {
                     // dot + something, skip both
                     current += 2;
                  }
               }
               else
               {
                  // "/." at the end
                  *current = 0;
               }
               break;
            default:
               current++;
               break;
         }
      }
      else
      {
         current++;
      }
   }
   return result;
}

#endif

/**
 * Takes folder/file path - make it absolute (result will be written to "fullPath" parameter)
 * and check that this folder/file is under allowed root path.
 * If third parameter is set to true - then request is for getting content and "/" path should be accepted
 * and afterwards interpreted as "give list of all allowed folders".
 */
static bool CheckFullPath(const TCHAR *path, TCHAR **fullPath, bool withHomeDir, bool isModify = false)
{
   nxlog_debug_tag(DEBUG_TAG, 5, _T("CheckFullPath: input is %s"), path);
   if (withHomeDir && !_tcscmp(path, FS_PATH_SEPARATOR))
   {
      *fullPath = MemCopyString(path);
      return true;
   }

   *fullPath = nullptr;

#ifdef _WIN32
   TCHAR *fullPathBuffer = MemAllocString(MAX_PATH);
   TCHAR *fullPathT = _tfullpath(fullPathBuffer, path, MAX_PATH);
#else
   TCHAR *fullPathT = GetRealPath(path);
#endif
   nxlog_debug_tag(DEBUG_TAG, 5, _T("CheckFullPath: Full path %s"), fullPathT);
   if (fullPathT == nullptr)
   {
#ifdef _WIN32
      MemFree(fullPathBuffer);
#endif
      return false;
   }

   for(int i = 0; i < s_rootDirectories->size(); i++)
   {
#if defined(_WIN32) || defined(__APPLE__)
      if (!_tcsnicmp(s_rootDirectories->get(i)->getFolder(), fullPathT, _tcslen(s_rootDirectories->get(i)->getFolder())))
#else
      if (!_tcsncmp(s_rootDirectories->get(i)->getFolder(), fullPathT, _tcslen(s_rootDirectories->get(i)->getFolder())))
#endif
      {
         if (!isModify || !s_rootDirectories->get(i)->isReadOnly())
         {
            *fullPath = fullPathT;
            return true;
         }
         break;
      }
   }

   nxlog_debug_tag(DEBUG_TAG, 5, _T("CheckFullPath: Access denied to %s"), fullPathT);
   MemFree(fullPathT);
   return false;
}

#define REGULAR_FILE    1
#define DIRECTORY       2
#define SYMLINK         4

/**
 * Validate file change operation (upload, delete, etc.). Return strue if operation is allowed.
 */
static bool ValidateFileChangeOperation(const TCHAR *fileName, bool allowOverwrite, NXCPMessage *response)
{
   NX_STAT_STRUCT st;
   if (CALL_STAT(fileName, &st) == 0)
   {
      if (allowOverwrite)
         return true;
      response->setField(VID_RCC, S_ISDIR(st.st_mode) ? ERR_FOLDER_ALREADY_EXISTS : ERR_FILE_ALREADY_EXISTS);
      return false;
   }
   if (errno != ENOENT)
   {
      response->setField(VID_RCC, ERR_IO_FAILURE);
      return false;
   }
   return true;
}

#ifdef _WIN32

/**
 * Get file owner information on Windows
 */
TCHAR *GetFileOwnerWin(const TCHAR *file)
{
   HANDLE hFile = CreateFile(file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
   if (hFile == INVALID_HANDLE_VALUE)
      return _tcsdup(_T(""));

   // Get the owner SID of the file.
   PSID owner = NULL;
   PSECURITY_DESCRIPTOR sd = NULL;
   if (GetSecurityInfo(hFile, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, NULL, NULL, NULL, &sd) != ERROR_SUCCESS)
   {
      CloseHandle(hFile);
      return _tcsdup(_T(""));
   }
   CloseHandle(hFile);

   // Resolve account and domain name
   TCHAR acctName[256], domainName[256];
   DWORD acctNameSize = 256, domainNameSize = 256;
   SID_NAME_USE use = SidTypeUnknown;
   if (!LookupAccountSid(NULL, owner, acctName, &acctNameSize, domainName, &domainNameSize, &use))
      return _tcsdup(_T(""));

   TCHAR *name = (TCHAR *)malloc(512 * sizeof(TCHAR));
   _sntprintf(name, 512, _T("%s\\%s"), domainName, acctName);
   return name;
}

#endif // _WIN32

static bool FillMessageFolderContent(const TCHAR *filePath, const TCHAR *fileName, NXCPMessage *msg, UINT32 varId)
{
   if (_taccess(filePath, 4) != 0)
      return false;

   NX_STAT_STRUCT st;
   if (CALL_STAT(filePath, &st) == 0)
   {
      msg->setField(varId++, fileName);
      msg->setField(varId++, (QWORD)st.st_size);
      msg->setField(varId++, (QWORD)st.st_mtime);
      UINT32 type = 0;
      TCHAR accessRights[11];
#ifndef _WIN32
      if(S_ISLNK(st.st_mode))
      {
         accessRights[0] = _T('l');
         type |= SYMLINK;
         NX_STAT_STRUCT SYMLINKSt;
         if (CALL_STAT_FOLLOW_SYMLINK(filePath, &SYMLINKSt) == 0)
         {
            type |= S_ISDIR(SYMLINKSt.st_mode) ? DIRECTORY : 0;
         }
      }

      if(S_ISCHR(st.st_mode)) accessRights[0] = _T('c');
      if(S_ISBLK(st.st_mode)) accessRights[0] = _T('b');
      if(S_ISFIFO(st.st_mode)) accessRights[0] = _T('p');
      if(S_ISSOCK(st.st_mode)) accessRights[0] = _T('s');
#endif
      if(S_ISREG(st.st_mode))
      {
         type |= REGULAR_FILE;
         accessRights[0] = _T('-');
      }
      if(S_ISDIR(st.st_mode))
      {
         type |= DIRECTORY;
         accessRights[0] = _T('d');
      }

      msg->setField(varId++, type);
      TCHAR fullName[MAX_PATH];
      _tcscpy(fullName, filePath);
      msg->setField(varId++, fullName);

#ifndef _WIN32
#if HAVE_GETPWUID_R
      struct passwd *pw, pwbuf;
      char pwtxt[4096];
      getpwuid_r(st.st_uid, &pwbuf, pwtxt, 4096, &pw);
#else
      struct passwd *pw = getpwuid(st.st_uid);
#endif
#if HAVE_GETGRGID_R
      struct group *gr, grbuf;
      char grtxt[4096];
      getgrgid_r(st.st_gid, &grbuf, grtxt, 4096, &gr);
#else
      struct group *gr = getgrgid(st.st_gid);
#endif

      if (pw != NULL)
      {
         msg->setFieldFromMBString(varId++, pw->pw_name);
      }
      else
      {
	 TCHAR id[32];
	 _sntprintf(id, 32, _T("[%lu]"), (unsigned long)st.st_uid);
         msg->setField(varId++, id);
      }
      if (gr != NULL)
      {
         msg->setFieldFromMBString(varId++, gr->gr_name);
      }
      else
      {
	 TCHAR id[32];
	 _sntprintf(id, 32, _T("[%lu]"), (unsigned long)st.st_gid);
         msg->setField(varId++, id);
      }

      accessRights[1] = (S_IRUSR & st.st_mode) > 0 ? _T('r') : _T('-');
      accessRights[2] = (S_IWUSR & st.st_mode) > 0 ? _T('w') : _T('-');
      accessRights[3] = (S_IXUSR & st.st_mode) > 0 ? _T('x') : _T('-');
      accessRights[4] = (S_IRGRP & st.st_mode) > 0 ? _T('r') : _T('-');
      accessRights[5] = (S_IWGRP & st.st_mode) > 0 ? _T('w') : _T('-');
      accessRights[6] = (S_IXGRP & st.st_mode) > 0 ? _T('x') : _T('-');
      accessRights[7] = (S_IROTH & st.st_mode) > 0 ? _T('r') : _T('-');
      accessRights[8] = (S_IWOTH & st.st_mode) > 0 ? _T('w') : _T('-');
      accessRights[9] = (S_IXOTH & st.st_mode) > 0 ? _T('x') : _T('-');
      accessRights[10] = 0;
      msg->setField(varId++, accessRights);
#else	/* _WIN32 */
      TCHAR *owner = GetFileOwnerWin(filePath);
      msg->setField(varId++, owner);
      free(owner);
      msg->setField(varId++, _T(""));
      msg->setField(varId++, _T(""));
#endif // _WIN32
      return true;
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("GetFolderContent: cannot get folder %s"), filePath);
      return false;
   }
}

/**
 * Puts in response list of containing files
 */
static void GetFolderContent(TCHAR *folder, NXCPMessage *response, bool rootFolder, bool allowMultipart, AbstractCommSession *session)
{
   nxlog_debug_tag(DEBUG_TAG, 6, _T("GetFolderContent: reading \"%s\" (root=%s, multipart=%s)"),
      folder, rootFolder ? _T("true") : _T("false"), allowMultipart ? _T("true") : _T("false"));

   NXCPMessage *msg;
   if (allowMultipart)
   {
      msg = new NXCPMessage();
      msg->setCode(CMD_REQUEST_COMPLETED);
      msg->setId(response->getId());
      msg->setField(VID_ALLOW_MULTIPART, (INT16)1);
   }
   else
   {
      msg = response;
   }

   UINT32 count = 0;
   UINT32 fieldId = VID_INSTANCE_LIST_BASE;

   if (!_tcscmp(folder, FS_PATH_SEPARATOR) && rootFolder)
   {
      response->setField(VID_RCC, ERR_SUCCESS);

      for(int i = 0; i < s_rootDirectories->size(); i++)
      {
         if (FillMessageFolderContent(s_rootDirectories->get(i)->getFolder(), s_rootDirectories->get(i)->getFolder(), msg, fieldId))
         {
            count++;
            fieldId += 10;
         }
      }
      msg->setField(VID_INSTANCE_COUNT, count);
      if (allowMultipart)
      {
         msg->setEndOfSequence();
         msg->setField(VID_INSTANCE_COUNT, count);
         session->sendMessage(msg);
         delete msg;
      }
      nxlog_debug_tag(DEBUG_TAG, 6, _T("GetFolderContent: reading \"%s\" completed"), folder);
      return;
   }

   _TDIR *dir = _topendir(folder);
   if (dir != NULL)
   {
      response->setField(VID_RCC, ERR_SUCCESS);

      struct _tdirent *d;
      while((d = _treaddir(dir)) != NULL)
      {
         if (!_tcscmp(d->d_name, _T(".")) || !_tcscmp(d->d_name, _T("..")))
            continue;

         TCHAR fullName[MAX_PATH];
         _tcscpy(fullName, folder);
         _tcscat(fullName, FS_PATH_SEPARATOR);
         _tcscat(fullName, d->d_name);

         if (FillMessageFolderContent(fullName, d->d_name, msg, fieldId))
         {
            count++;
            fieldId += 10;
         }
         if (allowMultipart && (count == 64))
         {
            msg->setField(VID_INSTANCE_COUNT, count);
            session->sendMessage(msg);
            msg->deleteAllFields();
            msg->setField(VID_ALLOW_MULTIPART, (INT16)1);
            count = 0;
            fieldId = VID_INSTANCE_LIST_BASE;
         }
      }
      msg->setField(VID_INSTANCE_COUNT, count);
      _tclosedir(dir);

      if (allowMultipart)
      {
         msg->setEndOfSequence();
         msg->setField(VID_INSTANCE_COUNT, count);
         session->sendMessage(msg);
      }
   }
   else
   {
      response->setField(VID_RCC, ERR_IO_FAILURE);
   }

   if (allowMultipart)
      delete msg;

   nxlog_debug_tag(DEBUG_TAG, 6, _T("GetFolderContent: reading \"%s\" completed"), folder);
}

/**
 * Delete file/folder
 */
static BOOL Delete(const TCHAR *name)
{
   NX_STAT_STRUCT st;

   if (CALL_STAT(name, &st) != 0)
   {
      return FALSE;
   }

   bool result = true;

   if (S_ISDIR(st.st_mode))
   {
      // get element list and for each element run Delete
      _TDIR *dir = _topendir(name);
      if (dir != NULL)
      {
         struct _tdirent *d;
         while((d = _treaddir(dir)) != NULL)
         {
            if (!_tcscmp(d->d_name, _T(".")) || !_tcscmp(d->d_name, _T("..")))
            {
               continue;
            }
            TCHAR newName[MAX_PATH];
            _tcscpy(newName, name);
            _tcscat(newName, FS_PATH_SEPARATOR);
            _tcscat(newName, d->d_name);
            result = result && Delete(newName);
         }
         _tclosedir(dir);
      }
      //remove directory
      return _trmdir(name) == 0;
   }
   return _tremove(name) == 0;
}

/**
 * Send file
 */
 THREAD_RESULT THREAD_CALL SendFile(void *dataStruct)
{
   MessageData *data = (MessageData *)dataStruct;

   nxlog_debug_tag(DEBUG_TAG, 5, _T("CommSession::getLocalFile(): request for file \"%s\", follow = %s, compress = %s"),
               data->fileName, data->follow ? _T("true") : _T("false"), data->allowCompression ? _T("true") : _T("false"));
   bool success = AgentSendFileToServer(data->session, data->id, data->fileName, (int)data->offset, data->allowCompression, s_downloadFileStopMarkers->get(data->id));
   if (data->follow && success)
   {
      g_monitorFileList.add(data->fileNameCode);
      FollowData *flData = new FollowData(data->fileName, data->fileNameCode, 0, data->session->getServerAddress());
      ThreadCreateEx(SendFileUpdatesOverNXCP, 0, flData);
   }
   data->session->decRefCount();
   MemFree(data->fileName);
   MemFree(data->fileNameCode);
   s_downloadFileStopMarkers->remove(data->id);
   delete data;
   return THREAD_OK;
}

/**
 * Get folder information
 */
static void GetFolderInfo(const TCHAR *folder, UINT64 *fileCount, UINT64 *folderSize)
{
   _TDIR *dir = _topendir(folder);
   if (dir != NULL)
   {
      NX_STAT_STRUCT st;
      struct _tdirent *d;
      while((d = _treaddir(dir)) != NULL)
      {
         if (sizeof(folder) >= MAX_PATH)
            return;

         if (!_tcscmp(d->d_name, _T(".")) || !_tcscmp(d->d_name, _T("..")))
         {
            continue;
         }

         TCHAR fullName[MAX_PATH];
         _tcscpy(fullName, folder);
         _tcscat(fullName, FS_PATH_SEPARATOR);
         _tcscat(fullName, d->d_name);

         if (CALL_STAT(fullName, &st) == 0)
         {
            if (S_ISDIR(st.st_mode))
            {
               GetFolderInfo(fullName, fileCount, folderSize);
            }
            else
            {
               *folderSize += st.st_size;
               (*fileCount)++;
            }
         }
      }
      _tclosedir(dir);
   }
}

/**
 * Handler for "get folder size" command
 */
static void CH_GetFolderSize(NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   TCHAR directory[MAX_PATH];
   request->getFieldAsString(VID_FILE_NAME, directory, MAX_PATH);
   if (directory[0] == 0)
   {
      response->setField(VID_RCC, ERR_IO_FAILURE);
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_GetFolderSize: File name is not set"));
      return;
   }

   ConvertPathToHost(directory, request->getFieldAsBoolean(VID_ALLOW_PATH_EXPANSION), session->isMasterServer());

   TCHAR *fullPath;
   if (CheckFullPath(directory, &fullPath, false))
   {
      UINT64 fileCount = 0, fileSize = 0;
      GetFolderInfo(fullPath, &fileCount, &fileSize);
      response->setField(VID_RCC, ERR_SUCCESS);
      response->setField(VID_FOLDER_SIZE, fileSize);
      response->setField(VID_FILE_COUNT, fileCount);
      MemFree(fullPath);
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_GetFolderSize: CheckFullPath failed"));
      response->setField(VID_RCC, ERR_ACCESS_DENIED);
   }
}

/**
 * Handler for "get folder content" command
 */
static void CH_GetFolderContent(NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   TCHAR directory[MAX_PATH];
   request->getFieldAsString(VID_FILE_NAME, directory, MAX_PATH);
   if (directory[0] == 0)
   {
      response->setField(VID_RCC, ERR_IO_FAILURE);
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_GetFolderContent: File name is not set"));
      return;
   }

   ConvertPathToHost(directory, request->getFieldAsBoolean(VID_ALLOW_PATH_EXPANSION), session->isMasterServer());

   bool rootFolder = request->getFieldAsUInt16(VID_ROOT) ? 1 : 0;
   TCHAR *fullPath;
   if (CheckFullPath(directory, &fullPath, rootFolder))
   {
      GetFolderContent(fullPath, response, rootFolder, request->getFieldAsBoolean(VID_ALLOW_MULTIPART), session);
      MemFree(fullPath);
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_GetFolderContent: CheckFullPath failed"));
      response->setField(VID_RCC, ERR_ACCESS_DENIED);
   }
}

/**
 * Handler for "create folder" command
 */
static void CH_CreateFolder(NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   TCHAR directory[MAX_PATH];
   request->getFieldAsString(VID_FILE_NAME, directory, MAX_PATH);
   if (directory[0] == 0)
   {
      response->setField(VID_RCC, ERR_IO_FAILURE);
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_CreateFolder: File name is not set"));
      return;
   }

   ConvertPathToHost(directory, request->getFieldAsBoolean(VID_ALLOW_PATH_EXPANSION), session->isMasterServer());

   TCHAR *fullPath = NULL;
   if (CheckFullPath(directory, &fullPath, false, true) && session->isMasterServer())
   {
      if (ValidateFileChangeOperation(fullPath, false, response))
      {
         if (CreateFolder(fullPath))
         {
            response->setField(VID_RCC, ERR_SUCCESS);
         }
         else
         {
            nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_CreateFolder: Could not create directory \"%s\""), fullPath);
            response->setField(VID_RCC, ERR_IO_FAILURE);
         }
      }
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_CreateFolder: CheckFullPath failed"));
      response->setField(VID_RCC, ERR_ACCESS_DENIED);
   }
   MemFree(fullPath);
}

/**
 * Handler for "delete file" command
 */
static void CH_DeleteFile(NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   TCHAR file[MAX_PATH];
   request->getFieldAsString(VID_FILE_NAME, file, MAX_PATH);
   if (file[0] == 0)
   {
      response->setField(VID_RCC, ERR_IO_FAILURE);
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_DeleteFile: File name is not set"));
      return;
   }

   ConvertPathToHost(file, request->getFieldAsBoolean(VID_ALLOW_PATH_EXPANSION), session->isMasterServer());

   TCHAR *fullPath = NULL;
   if (CheckFullPath(file, &fullPath, false, true) && session->isMasterServer())
   {
      if (Delete(fullPath))
      {
         response->setField(VID_RCC, ERR_SUCCESS);
      }
      else
      {
         response->setField(VID_RCC, ERR_IO_FAILURE);
         nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_DeleteFile: Delete failed on \"%s\""), fullPath);
      }
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_DeleteFile: CheckFullPath failed on \"%s\""), file);
      response->setField(VID_RCC, ERR_ACCESS_DENIED);
   }
   MemFree(fullPath);
}

/**
 * Handler for "rename file" command
 */
static void CH_RenameFile(NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   TCHAR oldName[MAX_PATH];
   request->getFieldAsString(VID_FILE_NAME, oldName, MAX_PATH);
   TCHAR newName[MAX_PATH];
   request->getFieldAsString(VID_NEW_FILE_NAME, newName, MAX_PATH);
   bool allowOverwirite = request->getFieldAsBoolean(VID_OVERWRITE);

   if (oldName[0] == 0 && newName[0] == 0)
   {
      response->setField(VID_RCC, ERR_IO_FAILURE);
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_RenameFile: File names is not set"));
      return;
   }

   bool allowPathExpansion = request->getFieldAsBoolean(VID_ALLOW_PATH_EXPANSION);
   ConvertPathToHost(oldName, allowPathExpansion, session->isMasterServer());
   ConvertPathToHost(newName, allowPathExpansion, session->isMasterServer());

   TCHAR *fullPathOld = NULL, *fullPathNew = NULL;
   if (CheckFullPath(oldName, &fullPathOld, false, true) && CheckFullPath(newName, &fullPathNew, false) && session->isMasterServer())
   {
      if (ValidateFileChangeOperation(fullPathNew, allowOverwirite, response))
      {
         if (_trename(fullPathOld, fullPathNew) == 0)
         {
            response->setField(VID_RCC, ERR_SUCCESS);
         }
         else
         {
            response->setField(VID_RCC, ERR_IO_FAILURE);
         }
      }
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_RenameFile: CheckFullPath failed"));
      response->setField(VID_RCC, ERR_ACCESS_DENIED);
   }
   MemFree(fullPathOld);
   MemFree(fullPathNew);
}

/**
 * Handler for "move file" command
 */
static void CH_MoveFile(NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   TCHAR oldName[MAX_PATH];
   request->getFieldAsString(VID_FILE_NAME, oldName, MAX_PATH);
   TCHAR newName[MAX_PATH];
   request->getFieldAsString(VID_NEW_FILE_NAME, newName, MAX_PATH);
   bool allowOverwirite = request->getFieldAsBoolean(VID_OVERWRITE);
   if ((oldName[0] == 0) && (newName[0] == 0))
   {
      response->setField(VID_RCC, ERR_IO_FAILURE);
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_MoveFile: File names are not set"));
      return;
   }

   bool allowPathExpansion = request->getFieldAsBoolean(VID_ALLOW_PATH_EXPANSION);
   ConvertPathToHost(oldName, allowPathExpansion, session->isMasterServer());
   ConvertPathToHost(newName, allowPathExpansion, session->isMasterServer());

   TCHAR *fullPathOld = NULL, *fullPathNew = NULL;
   if (CheckFullPath(oldName, &fullPathOld, false, true) && CheckFullPath(newName, &fullPathNew, false) && session->isMasterServer())
   {
      if (ValidateFileChangeOperation(fullPathNew, allowOverwirite, response))
      {
         if (MoveFileOrDirectory(fullPathOld, fullPathNew))
         {
            response->setField(VID_RCC, ERR_SUCCESS);
         }
         else
         {
            response->setField(VID_RCC, ERR_IO_FAILURE);
         }
      }
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_MoveFile: CheckFullPath failed"));
      response->setField(VID_RCC, ERR_ACCESS_DENIED);
   }
   MemFree(fullPathOld);
   MemFree(fullPathNew);
}

/**
 * Handler for "copy file" command
 */
static void CH_CopyFile(NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   TCHAR oldName[MAX_PATH];
   request->getFieldAsString(VID_FILE_NAME, oldName, MAX_PATH);
   TCHAR newName[MAX_PATH];
   request->getFieldAsString(VID_NEW_FILE_NAME, newName, MAX_PATH);
   bool allowOverwrite = request->getFieldAsBoolean(VID_OVERWRITE);
   response->setField(VID_RCC, ERR_SUCCESS);

   if ((oldName[0] == 0) && (newName[0] == 0))
   {
      response->setField(VID_RCC, ERR_IO_FAILURE);
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_CopyFile: File names are not set"));
      return;
   }

   bool allowPathExpansion = request->getFieldAsBoolean(VID_ALLOW_PATH_EXPANSION);
   ConvertPathToHost(oldName, allowPathExpansion, session->isMasterServer());
   ConvertPathToHost(newName, allowPathExpansion, session->isMasterServer());

   TCHAR *fullPathOld = NULL, *fullPathNew = NULL;
   if (CheckFullPath(oldName, &fullPathOld, false, true) && CheckFullPath(newName, &fullPathNew, false) && session->isMasterServer())
   {
      if (ValidateFileChangeOperation(fullPathNew, allowOverwrite, response))
      {
         if (!CopyFileOrDirectory(fullPathOld, fullPathNew))
            response->setField(VID_RCC, ERR_IO_FAILURE);
      }
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_CopyFile: CheckFullPath failed"));
      response->setField(VID_RCC, ERR_ACCESS_DENIED);
   }
   MemFree(fullPathOld);
   MemFree(fullPathNew);
}

/**
 * Get path to file without file name
 * Will return null if there is only file name
 */
static TCHAR *GetPathToFile(const TCHAR *fullPath)
{
   TCHAR *pathToFile = MemCopyString(fullPath);
   TCHAR *ptr = _tcsrchr(pathToFile, FS_PATH_SEPARATOR_CHAR);
   if (ptr != nullptr)
   {
      *ptr = 0;
   }
   else
   {
      MemFree(pathToFile);
      pathToFile = nullptr;
   }
   return pathToFile;
}

/**
 * Handler for "upload" command
 */
static void CH_Upload(NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   TCHAR name[MAX_PATH];
   request->getFieldAsString(VID_FILE_NAME, name, MAX_PATH);
   if (name[0] == 0)
   {
      response->setField(VID_RCC, ERR_IO_FAILURE);
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_Upload: File name is not set"));
      return;
   }

   ConvertPathToHost(name, request->getFieldAsBoolean(VID_ALLOW_PATH_EXPANSION), session->isMasterServer());

   TCHAR *fullPath = nullptr;
   if (CheckFullPath(name, &fullPath, false, true) && session->isMasterServer())
   {
      TCHAR *pathToFile = GetPathToFile(fullPath);
      if (pathToFile != nullptr)
      {
         CreateFolder(pathToFile);
         MemFree(pathToFile);
      }

      bool allowOverwirite = request->getFieldAsBoolean(VID_OVERWRITE);
      if (ValidateFileChangeOperation(fullPath, allowOverwirite, response))
         response->setField(VID_RCC, session->openFile(fullPath, request->getId(), request->getFieldAsTime(VID_MODIFICATION_TIME)));
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_Upload: CheckFullPath failed"));
      response->setField(VID_RCC, ERR_ACCESS_DENIED);
   }
   MemFree(fullPath);
}

/**
 * Handler for "get file details" command
 */
static void CH_GetFileDetails(NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   TCHAR fileName[MAX_PATH];
   request->getFieldAsString(VID_FILE_NAME, fileName, MAX_PATH);
   ConvertPathToHost(fileName, request->getFieldAsBoolean(VID_ALLOW_PATH_EXPANSION), session->isMasterServer());

   TCHAR *fullPath;
   if (CheckFullPath(fileName, &fullPath, false))
   {
      NX_STAT_STRUCT fs;
      if (CALL_STAT(fullPath, &fs) == 0)
      {
         response->setField(VID_FILE_SIZE, static_cast<uint64_t>(fs.st_size));
         response->setField(VID_MODIFICATION_TIME, static_cast<uint64_t>(fs.st_mtime));
         response->setField(VID_RCC, ERR_SUCCESS);
      }
      else
      {
         response->setField(VID_RCC, ERR_FILE_STAT_FAILED);
      }
      MemFree(fullPath);
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("CH_GetFileDetails: CheckFullPath failed"));
      response->setField(VID_RCC, ERR_ACCESS_DENIED);
   }
}

/**
 * Handler for "get file set details" command
 */
static void CH_GetFileSetDetails(NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   bool allowPathExpansion = request->getFieldAsBoolean(VID_ALLOW_PATH_EXPANSION);
   StringList files(request, VID_ELEMENT_LIST_BASE, VID_NUM_ELEMENTS);
   UINT32 fieldId = VID_ELEMENT_LIST_BASE;
   for(int i = 0; i < files.size(); i++)
   {
      TCHAR fileName[MAX_PATH];
      _tcslcpy(fileName, files.get(i), MAX_PATH);
      ConvertPathToHost(fileName, allowPathExpansion, session->isMasterServer());

      TCHAR *fullPath;
      if (CheckFullPath(fileName, &fullPath, false))
      {
         NX_STAT_STRUCT fs;
         if (CALL_STAT(fullPath, &fs) == 0)
         {
            response->setField(fieldId++, ERR_SUCCESS);
            response->setField(fieldId++, static_cast<UINT64>(fs.st_size));
            response->setField(fieldId++, static_cast<UINT64>(fs.st_mtime));
            BYTE hash[MD5_DIGEST_SIZE];
            if (!CalculateFileMD5Hash(fullPath, hash))
               memset(hash, 0, MD5_DIGEST_SIZE);
            response->setField(fieldId++, hash, MD5_DIGEST_SIZE);
            fieldId += 6;
         }
         else
         {
            response->setField(fieldId++, ERR_FILE_STAT_FAILED);
            fieldId += 9;
         }
         MemFree(fullPath);
      }
      else
      {
         response->setField(fieldId++, ERR_ACCESS_DENIED);
         fieldId += 9;
      }
   }
   response->setField(VID_NUM_ELEMENTS, files.size());
   response->setField(VID_RCC, ERR_SUCCESS);
}

/**
 * Handler for "get file" command
 */
static void CH_GetFile(NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   if (request->getFieldAsBoolean(VID_FILE_FOLLOW) && !session->isMasterServer())
   {
      response->setField(VID_RCC, ERR_ACCESS_DENIED);
      return;
   }

   TCHAR fileName[MAX_PATH];
   request->getFieldAsString(VID_FILE_NAME, fileName, MAX_PATH);
   ConvertPathToHost(fileName, request->getFieldAsBoolean(VID_ALLOW_PATH_EXPANSION), session->isMasterServer());

   TCHAR *fullPath;
   if (CheckFullPath(fileName, &fullPath, false))
   {
      MessageData *data = new MessageData();
      data->fileName = fullPath;
      data->fileNameCode = request->getFieldAsString(VID_NAME);
      data->follow = request->getFieldAsBoolean(VID_FILE_FOLLOW);
      data->allowCompression = request->getFieldAsBoolean(VID_ENABLE_COMPRESSION);
      data->id = request->getId();
      data->offset = request->getFieldAsUInt32(VID_FILE_OFFSET);
      data->session = session;
      session->incRefCount();
      s_downloadFileStopMarkers->set(request->getId(), new VolatileCounter(0));

      ThreadCreateEx(SendFile, 0, data);

      response->setField(VID_RCC, ERR_SUCCESS);
   }
   else
   {
      response->setField(VID_RCC, ERR_ACCESS_DENIED);
   }
}

/**
 * Handler for "cancel file download" command
 */
static void CH_CancelFileDownload(NXCPMessage *request, NXCPMessage *response)
{
   VolatileCounter *counter = s_downloadFileStopMarkers->get(request->getFieldAsUInt32(VID_REQUEST_ID));
   if (counter != NULL)
   {
      InterlockedIncrement(counter);
      response->setField(VID_RCC, ERR_SUCCESS);
   }
   else
   {
      response->setField(VID_RCC, ERR_INTERNAL_ERROR);
   }
}

/**
 * Handler for "cancel file monitoring" command
 */
static void CH_CancelFileMonitoring(NXCPMessage *request, NXCPMessage *response)
{
   TCHAR fileName[MAX_PATH];
   request->getFieldAsString(VID_FILE_NAME, fileName, MAX_PATH);
   if (g_monitorFileList.remove(fileName))
   {
      response->setField(VID_RCC, ERR_SUCCESS);
   }
   else
   {
      response->setField(VID_RCC, ERR_BAD_ARGUMENTS);
   }
}

/**
 * Process commands like get files in folder, delete file/folder, copy file/folder, move file/folder
 */
static bool ProcessCommands(UINT32 command, NXCPMessage *request, NXCPMessage *response, AbstractCommSession *session)
{
   switch(command)
   {
   	case CMD_GET_FOLDER_SIZE:
   	   CH_GetFolderSize(request, response, session);
         break;
      case CMD_GET_FOLDER_CONTENT:
         CH_GetFolderContent(request, response, session);
         break;
      case CMD_FILEMGR_CREATE_FOLDER:
         CH_CreateFolder(request, response, session);
         break;
      case CMD_GET_FILE_DETAILS:
         CH_GetFileDetails(request, response, session);
         break;
      case CMD_GET_FILE_SET_DETAILS:
         CH_GetFileSetDetails(request, response, session);
         break;
      case CMD_FILEMGR_DELETE_FILE:
         CH_DeleteFile(request, response, session);
         break;
      case CMD_FILEMGR_RENAME_FILE:
         CH_RenameFile(request, response, session);
         break;
      case CMD_FILEMGR_MOVE_FILE:
         CH_MoveFile(request, response, session);
         break;
      case CMD_FILEMGR_COPY_FILE:
         CH_CopyFile(request, response, session);
         break;
      case CMD_FILEMGR_UPLOAD:
         CH_Upload(request, response, session);
         break;
      case CMD_GET_AGENT_FILE:
         CH_GetFile(request, response, session);
         break;
      case CMD_CANCEL_FILE_DOWNLOAD:
         CH_CancelFileDownload(request, response);
         break;
      case CMD_CANCEL_FILE_MONITORING:
         CH_CancelFileMonitoring(request, response);
         break;
      default:
         return false;
   }

   return true;
}

/**
 * Subagent information
 */
static NETXMS_SUBAGENT_INFO m_info =
{
   NETXMS_SUBAGENT_INFO_MAGIC,
   _T("FILEMGR"), NETXMS_VERSION_STRING,
   SubagentInit, SubagentShutdown, ProcessCommands, NULL,
   0, NULL, // parameters
   0, NULL, // lists
   0, NULL, // tables
   0, NULL, // actions
   0, NULL  // push parameters
};

/**
 * Entry point for NetXMS agent
 */
DECLARE_SUBAGENT_ENTRY_POINT(FILEMGR)
{
   *ppInfo = &m_info;
   return true;
}

#ifdef _WIN32

/**
 * DLL entry point
 */
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
   if (dwReason == DLL_PROCESS_ATTACH)
      DisableThreadLibraryCalls(hInstance);
   return TRUE;
}

#endif
