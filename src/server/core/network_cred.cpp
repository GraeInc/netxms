/*
** NetXMS - Network Management System
** Copyright (C) 2020-2022 Raden Solutions
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
** File: network_cred.cpp
**
**/

#include "nxcore.h"

/**
 * Get list of configured SNMP communities for given zone into NXCP message
 */
void GetZoneCommunityList(NXCPMessage *msg, int32_t zoneUIN)
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   TCHAR query[256];
   _sntprintf(query, 255, _T("SELECT community FROM snmp_communities WHERE zone=%d ORDER BY id ASC"), zoneUIN);
   DB_RESULT hResult = DBSelect(hdb, query);
   if (hResult != nullptr)
   {
      int count = DBGetNumRows(hResult);
      UINT32 stringBase = VID_COMMUNITY_STRING_LIST_BASE;
      msg->setField(VID_NUM_STRINGS, (UINT32)count);
      TCHAR buffer[256];
      for(int i = 0; i < count; i++)
      {
         DBGetField(hResult, i, 0, buffer, 256);
         msg->setField(stringBase++, buffer);
      }
      DBFreeResult(hResult);
      msg->setField(VID_RCC, RCC_SUCCESS);
   }
   else
   {
      msg->setField(VID_RCC, RCC_DB_FAILURE);
   }
   DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Get list of configured SNMP communities for all zones into NXCP message
 */
void GetFullCommunityList(NXCPMessage *msg)
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   DB_RESULT hResult = DBSelect(hdb, _T("SELECT community,zone FROM snmp_communities ORDER BY zone DESC, id ASC"));
   if (hResult != nullptr)
   {
      int count = DBGetNumRows(hResult);
      UINT32 stringBase = VID_COMMUNITY_STRING_LIST_BASE, zoneBase = VID_COMMUNITY_STRING_ZONE_LIST_BASE;
      msg->setField(VID_NUM_STRINGS, (UINT32)count);
      TCHAR buffer[256];
      for(int i = 0; i < count; i++)
      {
         DBGetField(hResult, i, 0, buffer, 256);
         msg->setField(stringBase++, buffer);
         msg->setField(zoneBase++, DBGetFieldULong(hResult, i, 1));
      }
      DBFreeResult(hResult);
      msg->setField(VID_RCC, RCC_SUCCESS);
   }
   else
   {
      msg->setField(VID_RCC, RCC_DB_FAILURE);
   }
   DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Get list of configured SNMP USM credentials for given zone into NXCP message
 */
void GetZoneUsmCredentialList(NXCPMessage *msg, int32_t zoneUIN)
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   TCHAR query[256];
   _sntprintf(query, 255, _T("SELECT user_name,auth_method,priv_method,auth_password,priv_password,comments FROM usm_credentials WHERE zone=%d ORDER BY id ASC"), zoneUIN);
   DB_RESULT hResult = DBSelect(hdb, query);
   if (hResult != nullptr)
   {
      TCHAR buffer[MAX_DB_STRING];
      int count = DBGetNumRows(hResult);
      msg->setField(VID_NUM_RECORDS, (UINT32)count);
      for(int i = 0, id = VID_USM_CRED_LIST_BASE; i < count; i++, id += 3)
      {
         DBGetField(hResult, i, 0, buffer, MAX_DB_STRING);  // security name
         msg->setField(id++, buffer);

         msg->setField(id++, (WORD)DBGetFieldLong(hResult, i, 1)); // auth method
         msg->setField(id++, (WORD)DBGetFieldLong(hResult, i, 2)); // priv method

         DBGetField(hResult, i, 3, buffer, MAX_DB_STRING);  // auth password
         msg->setField(id++, buffer);

         DBGetField(hResult, i, 4, buffer, MAX_DB_STRING);  // priv password
         msg->setField(id++, buffer);

         msg->setField(id++, zoneUIN); // zone ID

         TCHAR comments[256];
         msg->setField(id++, DBGetField(hResult, i, 5, comments, 256)); //comment
      }
      DBFreeResult(hResult);
      msg->setField(VID_RCC, RCC_SUCCESS);
   }
   else
   {
      msg->setField(VID_RCC, RCC_DB_FAILURE);
   }
   DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Get list of configured SNMP USM credentials for all zones into NXCP message
 */
void GetFullUsmCredentialList(NXCPMessage *msg)
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   DB_RESULT hResult = DBSelect(hdb, _T("SELECT user_name,auth_method,priv_method,auth_password,priv_password,zone,comments FROM usm_credentials ORDER BY zone DESC, id ASC"));
   if (hResult != nullptr)
   {
      TCHAR buffer[MAX_DB_STRING];
      int count = DBGetNumRows(hResult);
      msg->setField(VID_NUM_RECORDS, (UINT32)count);
      for(int i = 0, id = VID_USM_CRED_LIST_BASE; i < count; i++, id += 3)
      {
         DBGetField(hResult, i, 0, buffer, MAX_DB_STRING);  // security name
         msg->setField(id++, buffer);

         msg->setField(id++, (WORD)DBGetFieldLong(hResult, i, 1)); // auth method
         msg->setField(id++, (WORD)DBGetFieldLong(hResult, i, 2)); // priv method

         DBGetField(hResult, i, 3, buffer, MAX_DB_STRING);  // auth password
         msg->setField(id++, buffer);

         DBGetField(hResult, i, 4, buffer, MAX_DB_STRING);  // priv password
         msg->setField(id++, buffer);

         msg->setField(id++, DBGetFieldULong(hResult, i, 5)); // zone ID

         TCHAR comments[256];
         msg->setField(id++, DBGetField(hResult, i, 6, comments, 256)); //comment
      }
      DBFreeResult(hResult);
      msg->setField(VID_RCC, RCC_SUCCESS);
   }
   else
   {
      msg->setField(VID_RCC, RCC_DB_FAILURE);
   }
   DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Get list of well-known ports for given zone and tag
 * @param tag if no ports found, tag based default values are returned: "snmp" - 161, "ssh" - 22
 */
IntegerArray<uint16_t> GetWellKnownPorts(const TCHAR *tag, int32_t zoneUIN)
{
   IntegerArray<uint16_t> ports;

   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT port FROM well_known_ports WHERE tag=? AND (zone=? OR zone=-1) ORDER BY zone DESC, id ASC"));
   if (hStmt != nullptr)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_VARCHAR, tag, DB_BIND_STATIC);
      DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, zoneUIN);

      DB_RESULT hResult = DBSelectPrepared(hStmt);
      if (hResult != nullptr)
      {
         int count = DBGetNumRows(hResult);
         for(int i = 0; i < count; i++)

            ports.add(DBGetFieldLong(hResult, i, 0));
         DBFreeResult(hResult);
      }
      DBFreeStatement(hStmt);
   }
   DBConnectionPoolReleaseConnection(hdb);

   if (ports.size() == 0)
      if(!_tcscmp(tag, _T("snmp")))
         ports.add(161);
      else if(!_tcscmp(tag, _T("ssh")))
         ports.add(22);

   return ports;
}

/**
 * Get list of configured ports for all zones into NXCP message
 */
void FullWellKnownPortListToMessage(const TCHAR *tag, NXCPMessage *msg)
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT port,zone FROM well_known_ports WHERE tag=? ORDER BY zone DESC, id ASC"));
   if (hStmt != nullptr)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_VARCHAR, tag, DB_BIND_STATIC);
      DB_RESULT hResult = DBSelectPrepared(hStmt);
      if (hResult != nullptr)
      {
         int count = DBGetNumRows(hResult);
         uint32_t fieldId = VID_ZONE_PORT_LIST_BASE;
         for(int i = 0; i < count; i++, fieldId +=8)
         {
            msg->setField(fieldId++, static_cast<uint16_t>(DBGetFieldLong(hResult, i, 0)));
            msg->setField(fieldId++, DBGetFieldULong(hResult, i, 1));
         }
         msg->setField(VID_ZONE_PORT_COUNT, static_cast<uint32_t>(count));
         msg->setField(VID_RCC, RCC_SUCCESS);
         DBFreeResult(hResult);
      }
      else
      {
         msg->setField(VID_RCC, RCC_DB_FAILURE);
      }
      DBFreeStatement(hStmt);
   }
   else
   {
      msg->setField(VID_RCC, RCC_DB_FAILURE);
   }
   DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Get list of configured ports for given zone into NXCP message
 */
void ZoneWellKnownPortListToMessage(const TCHAR *tag, int32_t zoneUIN, NXCPMessage *msg)
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT port FROM well_known_ports WHERE tag=? AND zone=? ORDER BY id ASC"));
   if (hStmt != nullptr)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_VARCHAR, tag, DB_BIND_STATIC);
      DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, zoneUIN);
      DB_RESULT hResult = DBSelectPrepared(hStmt);
      if (hResult != nullptr)
      {
         int count = DBGetNumRows(hResult);
         uint32_t fieldId = VID_ZONE_PORT_LIST_BASE;
         for(int i = 0; i < count; i++)
         {
            msg->setField(fieldId++, static_cast<uint16_t>(DBGetFieldLong(hResult, i, 0)));
         }
         msg->setField(VID_ZONE_PORT_COUNT, (UINT32)count);
         msg->setField(VID_RCC, RCC_SUCCESS);
         DBFreeResult(hResult);
      }
      else
      {
         msg->setField(VID_RCC, RCC_DB_FAILURE);
      }
      DBFreeStatement(hStmt);
   }
   else
   {
      msg->setField(VID_RCC, RCC_DB_FAILURE);
   }
   DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Update list of well-known ports from NXCP message
 */
uint32_t UpdateWellKnownPortList(const NXCPMessage& request, const TCHAR *tag, int32_t zoneUIN)
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   if (!DBBegin(hdb))
   {
      DBConnectionPoolReleaseConnection(hdb);
      return RCC_DB_FAILURE;
   }

   uint32_t rcc;
   DB_STATEMENT hStmt = DBPrepare(hdb, _T("DELETE FROM well_known_ports WHERE tag=? AND zone=?"));
   if (hStmt != nullptr)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_VARCHAR, tag, DB_BIND_STATIC);
      DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, zoneUIN);
      if (DBExecute(hStmt))
      {
         rcc = RCC_SUCCESS;
         int count = request.getFieldAsInt32(VID_ZONE_PORT_COUNT);
         if (count > 0)
         {
            DB_STATEMENT hStmt2 = DBPrepare(hdb, _T("INSERT INTO well_known_ports (id,port,zone,tag) VALUES(?,?,?,?)"), count > 1);
            if (hStmt2 != nullptr)
            {
               DBBind(hStmt2, 3, DB_SQLTYPE_INTEGER, zoneUIN);
               DBBind(hStmt2, 4, DB_SQLTYPE_VARCHAR, tag, DB_BIND_STATIC);

               uint32_t fieldId = VID_ZONE_PORT_LIST_BASE;
               for(int i = 0; i < count; i++)
               {
                  DBBind(hStmt2, 1, DB_SQLTYPE_INTEGER, i + 1);
                  DBBind(hStmt2, 2, DB_SQLTYPE_INTEGER, request.getFieldAsUInt16(fieldId++));
                  if (!DBExecute(hStmt2))
                  {
                     rcc = RCC_DB_FAILURE;
                     break;
                  }
               }
               DBFreeStatement(hStmt2);
            }
            else
            {
               rcc = RCC_DB_FAILURE;
            }
         }
         else
         {
            rcc = RCC_SUCCESS;
         }
      }
      DBFreeStatement(hStmt);
   }
   else
   {
      rcc = RCC_DB_FAILURE;
   }

   if (rcc == RCC_SUCCESS)
   {
      DBCommit(hdb);
      NotifyClientSessions(NX_NOTIFY_PORTS_CONFIG_CHANGED, zoneUIN);
   }
   else
   {
      DBRollback(hdb);
   }

   DBConnectionPoolReleaseConnection(hdb);
   return rcc;
}

/**
 * Get list of configured agent secrets for all zones into NXCP message
 */
void GetFullAgentSecretList(NXCPMessage *msg)
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   DB_RESULT hResult = DBSelect(hdb, _T("SELECT secret,zone FROM shared_secrets ORDER BY zone DESC, id ASC"));
   if (hResult != nullptr)
   {
      int count = DBGetNumRows(hResult);
      UINT32 baseId = VID_SHARED_SECRET_LIST_BASE;
      msg->setField(VID_NUM_ELEMENTS, (UINT32)count);
      for(int i = 0; i < count; i++, baseId +=8)
      {
         TCHAR buffer[MAX_SECRET_LENGTH];
         msg->setField(baseId++, DBGetField(hResult, i, 0, buffer, MAX_SECRET_LENGTH));
         msg->setField(baseId++, DBGetFieldULong(hResult, i, 1));
      }
      DBFreeResult(hResult);
      msg->setField(VID_RCC, RCC_SUCCESS);
   }
   else
   {
      msg->setField(VID_RCC, RCC_DB_FAILURE);
   }
   DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Get list of configured agent secrets for all zones into NXCP message
 */
void GetZoneAgentSecretList(NXCPMessage *msg, int32_t zoneUIN)
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   TCHAR query[256];
   _sntprintf(query, 255, _T("SELECT secret FROM shared_secrets WHERE zone=%d ORDER BY id ASC"), zoneUIN);
   DB_RESULT hResult = DBSelect(hdb, query);
   if (hResult != nullptr)
   {
      int count = DBGetNumRows(hResult);
      UINT32 baseId = VID_SHARED_SECRET_LIST_BASE;
      msg->setField(VID_NUM_ELEMENTS, (UINT32)count);
      for(int i = 0; i < count; i++)
      {
         TCHAR buffer[MAX_SECRET_LENGTH];
         msg->setField(baseId++, DBGetField(hResult, i, 0, buffer, MAX_SECRET_LENGTH));
      }
      DBFreeResult(hResult);
      msg->setField(VID_RCC, RCC_SUCCESS);
   }
   else
   {
      msg->setField(VID_RCC, RCC_DB_FAILURE);
   }
   DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Get list of SSH credentials for given zone
 */
static unique_ptr<StructArray<SshCredentials>> GetSshCredentialsCore(const TCHAR* select)
{
   StructArray<SshCredentials>* credentials = new StructArray<SshCredentials>();
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   DB_RESULT hResult = DBSelect(hdb, select);
   if (hResult != nullptr)
   {
      int count = DBGetNumRows(hResult);
      for (int i = 0; i < count; i++)
      {
         SshCredentials crd;
         DBGetField(hResult, i, 0, crd.login, MAX_SSH_LOGIN_LEN);
         DBGetField(hResult, i, 1, crd.password, MAX_SSH_PASSWORD_LEN);
         crd.keyId = DBGetFieldLong(hResult, i, 2);
         credentials->add(crd);
      }
      DBFreeResult(hResult);
   }
   DBConnectionPoolReleaseConnection(hdb);
   return unique_ptr<StructArray<SshCredentials>>(credentials);
}

/**
 * Get list of SSH credentials for given zone
 */
unique_ptr<StructArray<SshCredentials>> GetSshCredentialsZone(int32_t zoneUIN)
{
   TCHAR select[128];
   _sntprintf(select, 128, _T("SELECT login, password, key_id, id FROM ssh_credentials WHERE zone=%d ORDER BY id ASC"), zoneUIN);
   return GetSshCredentialsCore(select);
}

/**
 * Get list of SSH credentials for given zone and global credentials
 */
unique_ptr<StructArray<SshCredentials>> GetSshCredentialsGlobal(int32_t zoneUIN)
{
   TCHAR select[128];
   _sntprintf(select, 128, _T("SELECT login, password, key_id, id, zone FROM ssh_credentials WHERE (zone=%d OR zone=-1) ORDER BY zone DESC, id ASC"), zoneUIN);
   return GetSshCredentialsCore(select);
}

/**
 * Get list of SSH credentials for all zones
 */
unique_ptr<StructArray<SshCredentials>> GetSshCredentialsAll()
{
   TCHAR select[128];
   _sntprintf(select, 128, _T("SELECT login, password, key_id, id, zone FROM ssh_credentials ORDER BY zone DESC, id ASC"));
   return GetSshCredentialsCore(select);
}

/**
 * Get list of SSH credentials into NXCP message
 * @param tag specifies what credentials to get. Possible values are 'Z' for specific zone only, 'G' for specific zone + global cred, and 'A' for all zones
 */
void GetSshCredentialsMessage(NXCPMessage* msg, const TCHAR tag, int32_t zoneUIN)
{
   unique_ptr<StructArray<SshCredentials>> credentials = nullptr;
   switch (tag)
   {
      case _T('Z'):
         credentials = GetSshCredentialsZone(zoneUIN);
         break;
      case _T('G'):
         credentials = GetSshCredentialsGlobal(zoneUIN);
         break;
      case _T('A'):
         credentials = GetSshCredentialsAll();
         break;
   }

   msg->setField(VID_NUM_ELEMENTS, credentials->size());
   int base = VID_ELEMENT_LIST_BASE;
   for (int i = 0; i < credentials->size(); i++, base += 5)
   {
      msg->setField(base, credentials->get(i)->login);
      msg->setField(base + 1, credentials->get(i)->password);
      msg->setField(base + 2, credentials->get(i)->keyId);
   }

   msg->setField(VID_RCC, RCC_SUCCESS);
}
