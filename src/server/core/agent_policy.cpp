/*
** NetXMS - Network Management System
** Copyright (C) 2003-2020 Raden Solutions
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
** File: agent_policy.cpp
**
**/

#include "nxcore.h"

#define DEBUG_TAG _T("agent.policy")

/**
 * Constructor for user-initiated object creation
 */
GenericAgentPolicy::GenericAgentPolicy(const uuid& guid, const TCHAR *type, uint32_t ownerId)
{
   m_name[0] = 0;
   _tcslcpy(m_type, type, MAX_POLICY_TYPE_LEN);
   m_guid = guid;
   m_ownerId = ownerId;
   m_content = NULL;
   m_version = 1;
}

/**
 * Copy constructor
 */
GenericAgentPolicy::GenericAgentPolicy(const GenericAgentPolicy *src)
{
   _tcslcpy(m_name, src->m_name, MAX_OBJECT_NAME);
   _tcslcpy(m_type, src->m_type, MAX_POLICY_TYPE_LEN);
   m_guid = src->m_guid;
   m_ownerId = src->m_ownerId;
   m_content = MemCopyStringA(src->m_content);
   m_version = src->m_version;
}

/**
 * Constructor for user-initiated object creation
 */
GenericAgentPolicy::GenericAgentPolicy(const TCHAR *name, const TCHAR *type, uint32_t ownerId)
{
   _tcslcpy(m_name, name, MAX_OBJECT_NAME);
   _tcslcpy(m_type, type, MAX_POLICY_TYPE_LEN);
   m_guid = uuid::generate();
   m_ownerId = ownerId;
   m_content = NULL;
   m_version = 1;
}

/**
 * Destructor
 */
GenericAgentPolicy::~GenericAgentPolicy()
{
   MemFree(m_content);
}

/**
 * Create copy of this policy object
 */
GenericAgentPolicy *GenericAgentPolicy::clone() const
{
   return new GenericAgentPolicy(this);
}

/**
 * Save to database
 */
bool GenericAgentPolicy::saveToDatabase(DB_HANDLE hdb)
{
   DB_STATEMENT hStmt;
   if (!IsDatabaseRecordExist(hdb, _T("ap_common"), _T("guid"), m_guid)) //Policy can be only created. Policy type can't be changed.
      hStmt = DBPrepare(hdb, _T("INSERT INTO ap_common (policy_name,owner_id,policy_type,file_content,version,guid) VALUES (?,?,?,?,?,?)"));
   else
      hStmt = DBPrepare(hdb, _T("UPDATE ap_common SET policy_name=?,owner_id=?,policy_type=?,file_content=?,version=? WHERE guid=?"));

   if (hStmt == NULL)
      return false;

   DBBind(hStmt, 1, DB_SQLTYPE_VARCHAR, m_name, DB_BIND_STATIC);
   DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, m_ownerId);
   DBBind(hStmt, 3, DB_SQLTYPE_VARCHAR, m_type, DB_BIND_STATIC);
   DBBind(hStmt, 4, DB_SQLTYPE_TEXT, DB_CTYPE_UTF8_STRING, m_content, DB_BIND_STATIC);
   DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, m_version);
   DBBind(hStmt, 6, DB_SQLTYPE_VARCHAR, m_guid);
   bool success = DBExecute(hStmt);
   DBFreeStatement(hStmt);

   return success;
}

/**
 * Delete from database
 */
bool GenericAgentPolicy::deleteFromDatabase(DB_HANDLE hdb)
{
   TCHAR query[256];
   _sntprintf(query, 256, _T("DELETE FROM ap_common WHERE guid='%s'"), (const TCHAR *)m_guid.toString());
   return DBQuery(hdb, query);
}

/**
 * Load from database
 */
bool GenericAgentPolicy::loadFromDatabase(DB_HANDLE hdb)
{
   bool success = false;

   TCHAR query[256];
   _sntprintf(query, 256, _T("SELECT policy_name,owner_id,policy_type,file_content,version FROM ap_common WHERE guid='%s'"), (const TCHAR *)m_guid.toString());
   DB_RESULT hResult = DBSelect(hdb, query);
   if (hResult != NULL)
   {
      if (DBGetNumRows(hResult) > 0)
      {
         DBGetField(hResult, 0, 0, m_name, MAX_OBJECT_NAME);
         m_ownerId = DBGetFieldLong(hResult, 0, 1);
         DBGetField(hResult, 0, 2, m_type, MAX_POLICY_TYPE_LEN);
         m_content = DBGetFieldUTF8(hResult, 0, 3, NULL, 0);
         m_version = DBGetFieldLong(hResult, 0, 4);
         success = true;
      }
      DBFreeResult(hResult);
   }

   return success;
}

/**
 * Create NXCP message with policy data
 */
void GenericAgentPolicy::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
   msg->setField(baseId, m_guid);
   msg->setField(baseId + 1, m_type);
   msg->setField(baseId + 2, m_name);
   msg->setFieldFromUtf8String(baseId + 3, CHECK_NULL_EX_A(m_content));
}

/**
 * Create NXCP message with policy data for notifications
 */
void GenericAgentPolicy::fillUpdateMessage(NXCPMessage *msg)
{
   msg->setField(VID_GUID, m_guid);
   msg->setField(VID_NAME, m_name);
   msg->setField(VID_POLICY_TYPE, m_type);
   msg->setFieldFromUtf8String(VID_CONFIG_FILE_DATA, CHECK_NULL_EX_A(m_content));
}

/**
 * Modify policy from message
 */
UINT32 GenericAgentPolicy::modifyFromMessage(const NXCPMessage *msg)
{
   msg->getFieldAsString(VID_NAME, m_name, MAX_DB_STRING);
   if (msg->isFieldExist(VID_CONFIG_FILE_DATA))
   {
      MemFree(m_content);
      m_content = msg->getFieldAsUtf8String(VID_CONFIG_FILE_DATA);
   }
   m_version++;
   return RCC_SUCCESS;
}

/**
 * Create deployment message
 */
bool GenericAgentPolicy::createDeploymentMessage(NXCPMessage *msg, bool newTypeFormatSupported)
{
   if (m_content == NULL)
      return false;  // Policy cannot be deployed

   msg->setField(VID_CONFIG_FILE_DATA, reinterpret_cast<BYTE*>(m_content), strlen(m_content));

   if (newTypeFormatSupported)
   {
      msg->setField(VID_POLICY_TYPE, m_type);
   }
   else
   {
      if (!_tcscmp(m_type, _T("AgentConfig")))
      {
         msg->setField(VID_POLICY_TYPE, static_cast<UINT16>(AGENT_POLICY_CONFIG));
      }
      else if (!_tcscmp(m_type, _T("LogParserConfig")))
      {
         msg->setField(VID_POLICY_TYPE, static_cast<UINT16>(AGENT_POLICY_LOG_PARSER));
      }
   }
   msg->setField(VID_GUID, m_guid);
   msg->setField(VID_VERSION, m_version);

   return true;
}

/**
 * Deploy policy to agent. Default implementation calls connector's deployPolicy() method
 */
UINT32 GenericAgentPolicy::deploy(AgentConnectionEx *conn, bool newTypeFormatSupported, const TCHAR *debugId)
{
   nxlog_debug_tag(DEBUG_TAG, 4, _T("Calling GenericAgentPolicy::deploy at %s (type=%s, newTypeFormat=%d)"), debugId, m_type, newTypeFormatSupported);
   return conn->deployPolicy(this, newTypeFormatSupported);
}

/**
 * Serialize object to JSON
 */
json_t *GenericAgentPolicy::toJson()
{
   json_t *root = json_object();
   json_object_set_new(root, "guid", json_string_t(m_guid.toString()));
   json_object_set_new(root, "name", json_string_t(m_name));
   json_object_set_new(root, "type", json_string_t(m_type));
   json_object_set_new(root, "content", json_string(CHECK_NULL_EX_A(m_content)));
   return root;
}

/**
 * Update policy from imported configuration
 */
void GenericAgentPolicy::updateFromImport(const ConfigEntry *config)
{
   _tcslcpy(m_name, config->getSubEntryValue(_T("name"), 0, _T("Unnamed")), MAX_OBJECT_NAME);
   _tcslcpy(m_type, config->getSubEntryValue(_T("type"), 0, _T("Unknown")), MAX_POLICY_TYPE_LEN);
   const TCHAR *content = config->getSubEntryValue(_T("content"), 0, _T(""));
   MemFree(m_content);
   m_content = UTF8StringFromTString(content);
}

/**
 * Create export record
 */
void GenericAgentPolicy::createExportRecord(StringBuffer &xml, uint32_t recordId)
{
   xml.append(_T("\t\t\t\t<agentPolicy id=\""));
   xml.append(recordId);
   xml.append(_T("\">\n\t\t\t\t\t<guid>"));
   xml.append(m_guid);
   xml.append(_T("</guid>\n\t\t\t\t\t<name>"));
   xml.append(EscapeStringForXML2(m_name));
   xml.append(_T("</name>\n\t\t\t\t\t<type>"));
   xml.append(m_type);
   xml.append(_T("</type>\n\t\t\t\t\t<content>"));
   TCHAR *content = TStringFromUTF8String(CHECK_NULL_EX_A(m_content));
   xml.append(EscapeStringForXML2(content));
   MemFree(content);
   xml.append(_T("</content>\n"));
   xml.append(_T("\t\t\t\t</agentPolicy>\n"));
}

/**
 * File information
 */
struct FileInfo
{
   uuid guid;
   uuid newGuid;
   TCHAR *path;

   ~FileInfo()
   {
      MemFree(path);
   }
};

/**
 * Build file list from path element
 */
static void BuildFileList(ConfigEntry *currEntry, StringBuffer *currPath, ObjectArray<FileInfo> *files, bool updateGuid)
{
   ConfigEntry *children = currEntry->findEntry(_T("children"));
   if (children == NULL)
      return;

   size_t pathPos = currPath->length();
   currPath->append(currEntry->getAttribute(_T("name")));
   currPath->append(_T("/"));

   ObjectArray<ConfigEntry> *elements = children->getSubEntries(_T("*"));
   if (elements != NULL)
   {
      for(int i = 0; i < elements->size(); i++)
      {
         ConfigEntry *e = elements->get(i);
         uuid guid = e->getSubEntryValueAsUUID(_T("guid"));
         if (!guid.isNull())
         {
            size_t pos = currPath->length();
            currPath->append(e->getAttribute(_T("name")));
            auto f = new FileInfo;
            f->guid = guid;
            f->path = MemCopyString(currPath->cstr());
            files->add(f);
            currPath->shrink(currPath->length() - pos);
            if (updateGuid)
            {
               f->newGuid = uuid::generate();
               ObjectArray<ConfigEntry> *values = e->getSubEntries(_T("guid"));
               values->get(0)->setValue(f->newGuid.toString());
               delete values;
            }
         }
         else
         {
            BuildFileList(e, currPath, files, updateGuid);
         }
      }
      delete elements;
   }

   currPath->shrink(currPath->length() - pathPos);
}

/**
 * Clone file delivery policy
 */
GenericAgentPolicy *FileDeliveryPolicy::clone() const
{
   return new FileDeliveryPolicy(this);
}

/**
 * Modify from message and in case of duplicate - duplicate all physical files and update GUID
 */
UINT32 FileDeliveryPolicy::modifyFromMessage(const NXCPMessage *request)
{
   UINT32 result = GenericAgentPolicy::modifyFromMessage(request);
   if (result != RCC_SUCCESS)
      return result;

   if (request->getFieldAsBoolean(VID_DUPLICATE))
   {
      ObjectArray<FileInfo> files(64, 64, Ownership::True);
      Config data;
      data.loadXmlConfigFromMemory(m_content, static_cast<int>(strlen(m_content)), NULL, "FileDeliveryPolicy", false);
      ObjectArray<ConfigEntry> *rootElements = data.getSubEntries(_T("/elements"), _T("*"));
      if (rootElements != NULL)
      {
         for(int i = 0; i < rootElements->size(); i++)
         {
            StringBuffer path;
            BuildFileList(rootElements->get(i), &path, &files, true);
         }
         delete rootElements;
      }
      MemFree(m_content);
      data.setTopLevelTag(_T("FileDeliveryPolicy"));
      m_content = data.createXml().getUTF8String();

      for(int i = 0; i < files.size(); i++)
      {
         nxlog_debug_tag(DEBUG_TAG, 4, _T("FileDeliveryPolicy::modifyFromMessage(): copy file and update guid from %s to %s"), (const TCHAR *)files.get(i)->guid.toString(), (const TCHAR *)files.get(i)->newGuid.toString());

         StringBuffer sourceFile = g_netxmsdDataDir;
         sourceFile.append(DDIR_FILES FS_PATH_SEPARATOR _T("FileDelivery-"));
         sourceFile.append(files.get(i)->guid.toString());

         StringBuffer destinationFile = g_netxmsdDataDir;
         destinationFile.append(DDIR_FILES FS_PATH_SEPARATOR _T("FileDelivery-"));
         destinationFile.append(files.get(i)->newGuid.toString());

         CopyFileOrDirectory(sourceFile, destinationFile);
      }
   }
   return result;
}

/**
 * File delivery policy destructor
 * deletes all files on policy deletion
 */
bool FileDeliveryPolicy::deleteFromDatabase(DB_HANDLE hdb)
{
   ObjectArray<FileInfo> files(64, 64, Ownership::True);
   Config data;
   data.loadXmlConfigFromMemory(m_content, static_cast<int>(strlen(m_content)), NULL, "FileDeliveryPolicy", false);
   ObjectArray<ConfigEntry> *rootElements = data.getSubEntries(_T("/elements"), _T("*"));
   if (rootElements != NULL)
   {
      for(int i = 0; i < rootElements->size(); i++)
      {
         StringBuffer path;
         BuildFileList(rootElements->get(i), &path, &files, true);
      }
      delete rootElements;
   }

   for(int i = 0; i < files.size(); i++)
   {
      String guid = files.get(i)->guid.toString();
      nxlog_debug_tag(DEBUG_TAG, 4, _T("FileDeliveryPolicy::modifyFromMessage(): copy file %s and update guid"), guid.cstr());

      StringBuffer sourceFile = g_netxmsdDataDir;
      sourceFile.append(DDIR_FILES FS_PATH_SEPARATOR _T("FileDelivery-"));
      sourceFile.append(guid);

      _tremove(sourceFile);
   }

   return GenericAgentPolicy::deleteFromDatabase(hdb);
}

/**
 * Deploy file delivery policy
 */
UINT32 FileDeliveryPolicy::deploy(AgentConnectionEx *conn, bool newTypeFormatSupported, const TCHAR *debugId)
{
   nxlog_debug_tag(DEBUG_TAG, 4, _T("FileDeliveryPolicy::deploy(%s):)"), debugId);

   if (!newTypeFormatSupported)
      return ERR_NOT_IMPLEMENTED;

   if (m_content == NULL)
      return ERR_BAD_ARGUMENTS;

   nxlog_debug_tag(DEBUG_TAG, 6, _T("FileDeliveryPolicy::deploy(%s): preparing file list"), debugId);
   ObjectArray<FileInfo> files(64, 64, Ownership::True);
   Config data;
   data.loadXmlConfigFromMemory(m_content, static_cast<int>(strlen(m_content)), NULL, "FileDeliveryPolicy", false);
   ObjectArray<ConfigEntry> *rootElements = data.getSubEntries(_T("/elements"), _T("*"));
   if (rootElements != NULL)
   {
      for(int i = 0; i < rootElements->size(); i++)
      {
         StringBuffer path;
         BuildFileList(rootElements->get(i), &path, &files, false);
      }
      delete rootElements;
   }

   StringList fileRequest;
   for(int i = 0; i < files.size(); i++)
   {
      nxlog_debug_tag(DEBUG_TAG, 4, _T("FileDeliveryPolicy::deploy(%s): processing file path %s"), debugId, files.get(i)->path);
      fileRequest.add(files.get(i)->path);
   }
   ObjectArray<RemoteFileInfo> *remoteFiles;
   UINT32 rcc = conn->getFileSetInfo(&fileRequest, true, &remoteFiles);
   if (rcc != RCC_SUCCESS)
   {
      nxlog_debug_tag(DEBUG_TAG, 4, _T("FileDeliveryPolicy::deploy(%s): call to AgentConnection::getFileSetInfo failed (%s)"), debugId, AgentErrorCodeToText(rcc));
      return rcc;
   }

   for(int i = 0; i < remoteFiles->size(); i++)
   {
      RemoteFileInfo *remoteFile = remoteFiles->get(i);
      if ((remoteFile->status() != ERR_SUCCESS) && (remoteFile->status() != ERR_FILE_STAT_FAILED))
      {
         nxlog_debug_tag(DEBUG_TAG, 4, _T("FileDeliveryPolicy::deploy(%s): file %s with status %d skipped"), debugId, remoteFile->name(), remoteFile->status());
         continue;
      }

      StringBuffer localFile = g_netxmsdDataDir;
      localFile.append(DDIR_FILES FS_PATH_SEPARATOR _T("FileDelivery-"));
      localFile.append(files.get(i)->guid.toString());

      BYTE localHash[MD5_DIGEST_SIZE];
      if (CalculateFileMD5Hash(localFile, localHash) && ((remoteFile->status() == ERR_FILE_STAT_FAILED) || memcmp(localHash, remoteFile->hash(), MD5_DIGEST_SIZE)))
      {
         nxlog_debug_tag(DEBUG_TAG, 5, _T("FileDeliveryPolicy::deploy(%s): uploading %s"), debugId, files.get(i)->path);
         rcc = conn->uploadFile(localFile, remoteFile->name(), true);
         nxlog_debug_tag(DEBUG_TAG, 5, _T("FileDeliveryPolicy::deploy(%s): upload completed (%s)"), debugId, AgentErrorCodeToText(rcc));
      }
      else
         nxlog_debug_tag(DEBUG_TAG, 5, _T("FileDeliveryPolicy::deploy(%s): remote file %s and local file %s are the same, synchronization skipped"), debugId, remoteFile->name(), localFile.cstr());

   }
   delete remoteFiles;

   return ERR_SUCCESS;
}
