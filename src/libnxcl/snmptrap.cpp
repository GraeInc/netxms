/*
** NetXMS - Network Management System
** Client Library
** Copyright (C) 2003-2010 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: snmptrap.cpp
**
**/

#include "libnxcl.h"


/**
 * Fill trap configuration record from message
 */
static void TrapCfgFromMsg(NXCPMessage *pMsg, NXC_TRAP_CFG_ENTRY *pTrap)
{
   UINT32 i, dwId1, dwId2, dwId3, dwId4;

   pTrap->dwEventCode = pMsg->getFieldAsUInt32(VID_EVENT_CODE);
   pMsg->getFieldAsString(VID_DESCRIPTION, pTrap->szDescription, MAX_DB_STRING);
   pMsg->getFieldAsString(VID_USER_TAG, pTrap->szUserTag, MAX_USERTAG_LENGTH);
   pTrap->dwOidLen = pMsg->getFieldAsUInt32(VID_TRAP_OID_LEN);
   pTrap->pdwObjectId = (UINT32 *)malloc(sizeof(UINT32) * pTrap->dwOidLen);
   pMsg->getFieldAsInt32Array(VID_TRAP_OID, pTrap->dwOidLen, pTrap->pdwObjectId);
   pTrap->dwNumMaps = pMsg->getFieldAsUInt32(VID_TRAP_NUM_MAPS);
   pTrap->pMaps = (NXC_OID_MAP *)malloc(sizeof(NXC_OID_MAP) * pTrap->dwNumMaps);
   for(i = 0, dwId1 = VID_TRAP_PLEN_BASE, dwId2 = VID_TRAP_PNAME_BASE, dwId3 = VID_TRAP_PDESCR_BASE, dwId4 = VID_TRAP_PFLAGS_BASE;
       i < pTrap->dwNumMaps; i++, dwId1++, dwId2++, dwId3++, dwId4++)
   {
      pTrap->pMaps[i].dwOidLen = pMsg->getFieldAsUInt32(dwId1);
      if ((pTrap->pMaps[i].dwOidLen & 0x80000000) == 0)
      {
         pTrap->pMaps[i].pdwObjectId = (UINT32 *)malloc(sizeof(UINT32) * pTrap->pMaps[i].dwOidLen);
         pMsg->getFieldAsInt32Array(dwId2, pTrap->pMaps[i].dwOidLen, pTrap->pMaps[i].pdwObjectId);
      }
      else
      {
         pTrap->pMaps[i].pdwObjectId = NULL;
      }
      pMsg->getFieldAsString(dwId3, pTrap->pMaps[i].szDescription, MAX_DB_STRING);
		pTrap->pMaps[i].dwFlags = pMsg->getFieldAsUInt32(dwId4);
   }
}


/**
 * Process CMD_TRAP_CFG_UPDATE message
 */
void ProcessTrapCfgUpdate(NXCL_Session *pSession, NXCPMessage *pMsg)
{
   NXC_TRAP_CFG_ENTRY trapCfg;
   UINT32 dwCode;

	memset(&trapCfg, 0, sizeof(NXC_TRAP_CFG_ENTRY));

   dwCode = pMsg->getFieldAsUInt32(VID_NOTIFICATION_CODE);
   trapCfg.dwId = pMsg->getFieldAsUInt32(VID_TRAP_ID);
   if (dwCode != NX_NOTIFY_TRAPCFG_DELETED)
      TrapCfgFromMsg(pMsg, &trapCfg);

   pSession->callEventHandler(NXC_EVENT_NOTIFICATION, dwCode, &trapCfg);

	for(UINT32 i = 0; i < trapCfg.dwNumMaps; i++)
      safe_free(trapCfg.pMaps[i].pdwObjectId);
   safe_free(trapCfg.pMaps);
   safe_free(trapCfg.pdwObjectId);
}


/**
 * Copy NXC_TRAP_CFG_ENTRY
 */
void LIBNXCL_EXPORTABLE NXCCopyTrapCfgEntry(NXC_TRAP_CFG_ENTRY *dst, NXC_TRAP_CFG_ENTRY *src)
{
	memcpy(dst, src, sizeof(NXC_TRAP_CFG_ENTRY));
	if (src->pdwObjectId != NULL)
		dst->pdwObjectId = (UINT32 *)nx_memdup(src->pdwObjectId, sizeof(UINT32) * src->dwOidLen);
	if (src->pMaps != NULL)
	{
		dst->pMaps = (NXC_OID_MAP *)nx_memdup(src->pMaps, sizeof(NXC_OID_MAP) * src->dwNumMaps);
		for(UINT32 i = 0; i < src->dwNumMaps; i++)
		{
			if (src->pMaps[i].pdwObjectId != NULL)
				dst->pMaps[i].pdwObjectId = (UINT32 *)nx_memdup(src->pMaps[i].pdwObjectId, sizeof(UINT32) * src->pMaps[i].dwOidLen);
		}
	}
}


/**
 * Duplicate NXC_TRAP_CFG_ENTRY
 */
NXC_TRAP_CFG_ENTRY LIBNXCL_EXPORTABLE *NXCDuplicateTrapCfgEntry(NXC_TRAP_CFG_ENTRY *src)
{
	NXC_TRAP_CFG_ENTRY *dst = (NXC_TRAP_CFG_ENTRY *)malloc(sizeof(NXC_TRAP_CFG_ENTRY));
	NXCCopyTrapCfgEntry(dst, src);
	return dst;
}


/**
 * Destroy NXC_TRAP_CFG_ENTRY
 */
void LIBNXCL_EXPORTABLE NXCDestroyTrapCfgEntry(NXC_TRAP_CFG_ENTRY *e)
{
	if (e == NULL)
		return;

	for(UINT32 i = 0; i < e->dwNumMaps; i++)
      safe_free(e->pMaps[i].pdwObjectId);
   safe_free(e->pMaps);
   safe_free(e->pdwObjectId);
	free(e);
}


/**
 * Load trap configuration from server
 */
UINT32 LIBNXCL_EXPORTABLE NXCLoadTrapCfg(NXC_SESSION hSession, UINT32 *pdwNumTraps, NXC_TRAP_CFG_ENTRY **ppTrapList)
{
   NXCPMessage msg, *pResponse;
   UINT32 dwRqId, dwRetCode = RCC_SUCCESS, dwNumTraps = 0, dwTrapId = 0;
   NXC_TRAP_CFG_ENTRY *pList = NULL;

   dwRqId = ((NXCL_Session *)hSession)->CreateRqId();

   msg.setCode(CMD_LOAD_TRAP_CFG);
   msg.setId(dwRqId);
   ((NXCL_Session *)hSession)->SendMsg(&msg);

   dwRetCode = ((NXCL_Session *)hSession)->WaitForRCC(dwRqId);
   if (dwRetCode == RCC_SUCCESS)
   {
      do
      {
         pResponse = ((NXCL_Session *)hSession)->WaitForMessage(CMD_TRAP_CFG_RECORD, dwRqId);
         if (pResponse != NULL)
         {
            dwTrapId = pResponse->getFieldAsUInt32(VID_TRAP_ID);
            if (dwTrapId != 0)  // 0 is end of list indicator
            {
               pList = (NXC_TRAP_CFG_ENTRY *)realloc(pList,
                           sizeof(NXC_TRAP_CFG_ENTRY) * (dwNumTraps + 1));
               pList[dwNumTraps].dwId = dwTrapId;
               TrapCfgFromMsg(pResponse, &pList[dwNumTraps]);
               dwNumTraps++;
            }
            delete pResponse;
         }
         else
         {
            dwRetCode = RCC_TIMEOUT;
            dwTrapId = 0;
         }
      }
      while(dwTrapId != 0);
   }

   // Destroy results on failure or save on success
   if (dwRetCode == RCC_SUCCESS)
   {
      *ppTrapList = pList;
      *pdwNumTraps = dwNumTraps;
   }
   else
   {
      safe_free(pList);
      *ppTrapList = NULL;
      *pdwNumTraps = 0;
   }

   return dwRetCode;
}


/**
 * Destroy list of traps
 */
void LIBNXCL_EXPORTABLE NXCDestroyTrapList(UINT32 dwNumTraps, NXC_TRAP_CFG_ENTRY *pTrapList)
{
   UINT32 i, j;

   if (pTrapList == NULL)
      return;

   for(i = 0; i < dwNumTraps; i++)
   {
      for(j = 0; j < pTrapList[i].dwNumMaps; j++)
         safe_free(pTrapList[i].pMaps[j].pdwObjectId);
      safe_free(pTrapList[i].pMaps);
      safe_free(pTrapList[i].pdwObjectId);
   }
   free(pTrapList);
}


/**
 * Delete trap configuration record by ID
 */
UINT32 LIBNXCL_EXPORTABLE NXCDeleteTrap(NXC_SESSION hSession, UINT32 dwTrapId)
{
   NXCPMessage msg;
   UINT32 dwRqId;

   dwRqId = ((NXCL_Session *)hSession)->CreateRqId();

   msg.setCode(CMD_DELETE_TRAP);
   msg.setId(dwRqId);
   msg.setField(VID_TRAP_ID, dwTrapId);
   ((NXCL_Session *)hSession)->SendMsg(&msg);

   return ((NXCL_Session *)hSession)->WaitForRCC(dwRqId);
}


/**
 * Create new trap configuration record
 */
UINT32 LIBNXCL_EXPORTABLE NXCCreateTrap(NXC_SESSION hSession, UINT32 *pdwTrapId)
{
   NXCPMessage msg, *pResponse;
   UINT32 dwRqId, dwResult;

   dwRqId = ((NXCL_Session *)hSession)->CreateRqId();

   msg.setCode(CMD_CREATE_TRAP);
   msg.setId(dwRqId);
   ((NXCL_Session *)hSession)->SendMsg(&msg);

   pResponse = ((NXCL_Session *)hSession)->WaitForMessage(CMD_REQUEST_COMPLETED, dwRqId);
   if (pResponse != NULL)
   {
      dwResult = pResponse->getFieldAsUInt32(VID_RCC);
      if (dwResult == RCC_SUCCESS)
         *pdwTrapId = pResponse->getFieldAsUInt32(VID_TRAP_ID);
      delete pResponse;
   }
   else
   {
      dwResult = RCC_TIMEOUT;
   }

   return dwResult;
}


/**
 * Update trap configuration record
 */
UINT32 LIBNXCL_EXPORTABLE NXCModifyTrap(NXC_SESSION hSession, NXC_TRAP_CFG_ENTRY *pTrap)
{
   NXCPMessage msg;
   UINT32 i, dwRqId, dwId1, dwId2, dwId3, dwId4;

   dwRqId = ((NXCL_Session *)hSession)->CreateRqId();

   msg.setCode(CMD_MODIFY_TRAP);
   msg.setId(dwRqId);
   msg.setField(VID_TRAP_ID, pTrap->dwId);
   msg.setField(VID_TRAP_OID_LEN, pTrap->dwOidLen);
   msg.setFieldFromInt32Array(VID_TRAP_OID, pTrap->dwOidLen, pTrap->pdwObjectId);
   msg.setField(VID_EVENT_CODE, pTrap->dwEventCode);
   msg.setField(VID_DESCRIPTION, pTrap->szDescription);
   msg.setField(VID_USER_TAG, pTrap->szUserTag);
   msg.setField(VID_TRAP_NUM_MAPS, pTrap->dwNumMaps);
   for(i = 0, dwId1 = VID_TRAP_PLEN_BASE, dwId2 = VID_TRAP_PNAME_BASE, dwId3 = VID_TRAP_PDESCR_BASE, dwId4 = VID_TRAP_PFLAGS_BASE;
       i < pTrap->dwNumMaps; i++, dwId1++, dwId2++, dwId3++, dwId4++)
   {
      msg.setField(dwId1, pTrap->pMaps[i].dwOidLen);
      if ((pTrap->pMaps[i].dwOidLen & 0x80000000) == 0)
         msg.setFieldFromInt32Array(dwId2, pTrap->pMaps[i].dwOidLen, pTrap->pMaps[i].pdwObjectId);
      msg.setField(dwId3, pTrap->pMaps[i].szDescription);
		msg.setField(dwId4, pTrap->pMaps[i].dwFlags);
   }
   ((NXCL_Session *)hSession)->SendMsg(&msg);

   return ((NXCL_Session *)hSession)->WaitForRCC(dwRqId);
}


/**
 * Process SNMP trap log records coming from server
 */
void ProcessTrapLogRecords(NXCL_Session *pSession, NXCPMessage *pMsg)
{
   UINT32 i, dwNumRecords, dwId;
   NXC_SNMP_TRAP_LOG_RECORD rec;
   int nOrder;

   dwNumRecords = pMsg->getFieldAsUInt32(VID_NUM_RECORDS);
   nOrder = (int)pMsg->getFieldAsUInt16(VID_RECORDS_ORDER);
   DebugPrintf(_T("ProcessTrapLogRecords(): %d records in message, in %s order"),
               dwNumRecords, (nOrder == RECORD_ORDER_NORMAL) ? _T("normal") : _T("reversed"));
   for(i = 0, dwId = VID_TRAP_LOG_MSG_BASE; i < dwNumRecords; i++)
   {
      rec.qwId = pMsg->getFieldAsUInt64(dwId++);
      rec.dwTimeStamp = pMsg->getFieldAsUInt32(dwId++);
      rec.dwIpAddr = pMsg->getFieldAsUInt32(dwId++);
      rec.dwObjectId = pMsg->getFieldAsUInt32(dwId++);
      pMsg->getFieldAsString(dwId++, rec.szTrapOID, MAX_DB_STRING);
      rec.pszTrapVarbinds = pMsg->getFieldAsString(dwId++);

      // Call client's callback to handle new record
      pSession->callEventHandler(NXC_EVENT_NEW_SNMP_TRAP, nOrder, &rec);
      free(rec.pszTrapVarbinds);
   }

   // Notify requestor thread if all messages was received
   if (pMsg->isEndOfSequence())
      pSession->CompleteSync(SYNC_TRAP_LOG, RCC_SUCCESS);
}

/**
 * Synchronize trap log
 * This function is NOT REENTRANT
 */
UINT32 LIBNXCL_EXPORTABLE NXCSyncSNMPTrapLog(NXC_SESSION hSession, UINT32 dwMaxRecords)
{
   NXCPMessage msg;
   UINT32 dwRetCode, dwRqId;

   dwRqId = ((NXCL_Session *)hSession)->CreateRqId();
   ((NXCL_Session *)hSession)->PrepareForSync(SYNC_TRAP_LOG);

   msg.setCode(CMD_GET_TRAP_LOG);
   msg.setId(dwRqId);
   msg.setField(VID_MAX_RECORDS, dwMaxRecords);
   ((NXCL_Session *)hSession)->SendMsg(&msg);

   dwRetCode = ((NXCL_Session *)hSession)->WaitForRCC(dwRqId);
   if (dwRetCode == RCC_SUCCESS)
      dwRetCode = ((NXCL_Session *)hSession)->WaitForSync(SYNC_TRAP_LOG, INFINITE);
   else
      ((NXCL_Session *)hSession)->UnlockSyncOp(SYNC_TRAP_LOG);

   return dwRetCode;
}


/**
 * Get read-only trap configuration without parameter bindings
 */
UINT32 LIBNXCL_EXPORTABLE NXCGetTrapCfgRO(NXC_SESSION hSession, UINT32 *pdwNumTraps,
                                         NXC_TRAP_CFG_ENTRY **ppTrapList)
{
   NXCPMessage msg, *pResponse;
   UINT32 i, dwId, dwRqId, dwRetCode;

   dwRqId = ((NXCL_Session *)hSession)->CreateRqId();

   msg.setCode(CMD_GET_TRAP_CFG_RO);
   msg.setId(dwRqId);
   ((NXCL_Session *)hSession)->SendMsg(&msg);

   pResponse = ((NXCL_Session *)hSession)->WaitForMessage(CMD_REQUEST_COMPLETED, dwRqId);
   if (pResponse != NULL)
   {
      dwRetCode = pResponse->getFieldAsUInt32(VID_RCC);
      if (dwRetCode == RCC_SUCCESS)
      {
         *pdwNumTraps = pResponse->getFieldAsUInt32(VID_NUM_TRAPS);
         *ppTrapList = (NXC_TRAP_CFG_ENTRY *)malloc(sizeof(NXC_TRAP_CFG_ENTRY) * (*pdwNumTraps));
         memset(*ppTrapList, 0, sizeof(NXC_TRAP_CFG_ENTRY) * (*pdwNumTraps));

         for(i = 0, dwId = VID_TRAP_INFO_BASE; i < *pdwNumTraps; i++, dwId += 5)
         {
            (*ppTrapList)[i].dwId = pResponse->getFieldAsUInt32(dwId++);
            (*ppTrapList)[i].dwOidLen = pResponse->getFieldAsUInt32(dwId++);
            (*ppTrapList)[i].pdwObjectId = (UINT32 *)malloc(sizeof(UINT32) * (*ppTrapList)[i].dwOidLen);
            pResponse->getFieldAsInt32Array(dwId++, (*ppTrapList)[i].dwOidLen, (*ppTrapList)[i].pdwObjectId);
            (*ppTrapList)[i].dwEventCode = pResponse->getFieldAsUInt32(dwId++);
            pResponse->getFieldAsString(dwId++, (*ppTrapList)[i].szDescription, MAX_DB_STRING);
         }
      }
      delete pResponse;
   }
   else
   {
      dwRetCode = RCC_TIMEOUT;
   }
   return dwRetCode;
}
