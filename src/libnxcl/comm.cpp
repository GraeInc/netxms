/* 
** NetXMS - Network Management System
** Client Library
** Copyright (C) 2003-2011 Victor Kirhenshtein
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
** File: comm.cpp
**
**/

#include "libnxcl.h"

#ifdef _WIN32
#define write	_write
#define close	_close
#endif

/**
 * Network receiver thread
 */
THREAD_RESULT THREAD_CALL NetReceiver(NXCL_Session *pSession)
{
   NXCPMessage *pMsg;
   NXCP_MESSAGE *pRawMsg;
   NXCP_BUFFER *pMsgBuffer;
   BYTE *pDecryptionBuffer = NULL;
   int i, iErr;
   BOOL bMsgNotNeeded;
   TCHAR szBuffer[256];

   // Initialize raw message receiving function
   pMsgBuffer = (NXCP_BUFFER *)malloc(sizeof(NXCP_BUFFER));
   RecvNXCPMessage(0, NULL, pMsgBuffer, 0, NULL, NULL, 0);

   // Allocate space for raw message
   pRawMsg = (NXCP_MESSAGE *)malloc(pSession->m_dwReceiverBufferSize);
#ifdef _WITH_ENCRYPTION
   pDecryptionBuffer = (BYTE *)malloc(pSession->m_dwReceiverBufferSize);
#endif

   // Message receiving loop
   while(1)
   {
      // Receive raw message
      if ((iErr = RecvNXCPMessage(pSession->m_hSocket, pRawMsg, 
                                  pMsgBuffer, pSession->m_dwReceiverBufferSize,
                                  &pSession->m_pCtx, pDecryptionBuffer, INFINITE)) <= 0)
         break;

      // Check if we get too large message
      if (iErr == 1)
      {
         DebugPrintf(_T("Received too large message %s (%d bytes)"), 
                     NXCPMessageCodeName(ntohs(pRawMsg->code), szBuffer),
                     ntohl(pRawMsg->size));
         continue;
      }

      // Check for decryption errors
      if (iErr == 2)
      {
         DebugPrintf(_T("Message decryption error")); 
         continue;
      }

      // Check that actual received packet size is equal to encoded in packet
      if ((int)ntohl(pRawMsg->size) != iErr)
      {
         DebugPrintf(_T("RecvMsg: Bad packet length [size=%d ActualSize=%d]"), ntohl(pRawMsg->size), iErr);
         continue;   // Bad packet, wait for next
      }

      // Create message object from raw message
      if (IsBinaryMsg(pRawMsg))
      {
         // Convert numeric fields to host byte order
         pRawMsg->code = ntohs(pRawMsg->code);
         pRawMsg->flags = ntohs(pRawMsg->flags);
         pRawMsg->size = ntohl(pRawMsg->size);
         pRawMsg->id = ntohl(pRawMsg->id);
         pRawMsg->numFields = ntohl(pRawMsg->numFields);

         DebugPrintf(_T("RecvRawMsg(\"%s\", id:%d)"), NXCPMessageCodeName(pRawMsg->code, szBuffer), pRawMsg->id);

         // Process message
         switch(pRawMsg->code)
         {
            case CMD_FILE_DATA:
               MutexLock(pSession->m_mutexFileRq);
               if ((pSession->m_hCurrFile != -1) && (pSession->m_dwFileRqId == pRawMsg->id))
               {
                  if (write(pSession->m_hCurrFile, pRawMsg->fields, pRawMsg->numFields) == (int)pRawMsg->numFields)
                  {
                     if (pRawMsg->flags & MF_END_OF_FILE)
                     {
                        close(pSession->m_hCurrFile);
                        pSession->m_dwFileRqCompletion = RCC_SUCCESS;
                        ConditionSet(pSession->m_condFileRq);
                     }
                  }
                  else
                  {
                     // I/O error
                     close(pSession->m_hCurrFile);
                     pSession->m_dwFileRqCompletion = RCC_FILE_IO_ERROR;
                     ConditionSet(pSession->m_condFileRq);
                  }
               }
               MutexUnlock(pSession->m_mutexFileRq);
               break;
            case CMD_ABORT_FILE_TRANSFER:
               MutexLock(pSession->m_mutexFileRq);
               if ((pSession->m_hCurrFile != -1) && (pSession->m_dwFileRqId == pRawMsg->id))
               {
                  // I/O error
                  close(pSession->m_hCurrFile);
                  pSession->m_dwFileRqCompletion = RCC_FILE_IO_ERROR;
                  ConditionSet(pSession->m_condFileRq);
               }
               MutexUnlock(pSession->m_mutexFileRq);
               break;
            default:    // Put unknown raw messages into the wait queue
               pSession->m_msgWaitQueue.put((NXCP_MESSAGE *)nx_memdup(pRawMsg, pRawMsg->size));
               break;
         }
      }
      else
      {
         pMsg = new NXCPMessage(pRawMsg);
         bMsgNotNeeded = TRUE;
         DebugPrintf(_T("RecvMsg(\"%s\", id:%d)"), NXCPMessageCodeName(pMsg->getCode(), szBuffer), pMsg->getId());

         // Process message
         switch(pMsg->getCode())
         {
            case CMD_KEEPALIVE:     // Keepalive message
               pSession->SetTimeStamp(pMsg->getFieldAsUInt32(VID_TIMESTAMP));
               break;
            case CMD_REQUEST_SESSION_KEY:
               if (pSession->m_pCtx == NULL)
               {
                  NXCPMessage *pResponse;

                  SetupEncryptionContext(pMsg, &pSession->m_pCtx, &pResponse, NULL, NXCP_VERSION);
                  pSession->SendMsg(pResponse);
                  delete pResponse;
               }
            case CMD_OBJECT:        // Object information
            case CMD_OBJECT_UPDATE:
            case CMD_OBJECT_LIST_END:
               pSession->processObjectUpdate(pMsg);
               break;
            case CMD_EVENTLOG_RECORDS:
               ProcessEventLogRecords(pSession, pMsg);
               break;
            case CMD_SYSLOG_RECORDS:
               ProcessSyslogRecords(pSession, pMsg);
               break;
            case CMD_TRAP_LOG_RECORDS:
               ProcessTrapLogRecords(pSession, pMsg);
               break;
            case CMD_EVENT_DB_RECORD:
               ProcessEventDBRecord(pSession, pMsg);
               break;
            case CMD_USER_DATA:
            case CMD_GROUP_DATA:
            case CMD_USER_DB_EOF:
               pSession->processUserDBRecord(pMsg);
               break;
            case CMD_USER_DB_UPDATE:
               pSession->processUserDBUpdate(pMsg);
               break;
            case CMD_NODE_DCI:
               pSession->processDCI(pMsg);
               break;
            case CMD_ALARM_UPDATE:
               ProcessAlarmUpdate(pSession, pMsg);
               break;
            case CMD_ACTION_DB_UPDATE:
               ProcessActionUpdate(pSession, pMsg);
               break;
            case CMD_TRAP_CFG_UPDATE:
               ProcessTrapCfgUpdate(pSession, pMsg);
               break;
            case CMD_EVENT_DB_UPDATE:
               ProcessEventDBUpdate(pSession, pMsg);
               break;
            case CMD_NOTIFY:
               pSession->onNotify(pMsg);
               break;
				case CMD_SITUATION_CHANGE:
					ProcessSituationChange(pSession, pMsg);
					break;
            default:
               pSession->m_msgWaitQueue.put(pMsg);
               bMsgNotNeeded = FALSE;
               break;
         }
         if (bMsgNotNeeded)
            delete pMsg;
      }
   }

   for(i = 0; i < SYNC_OP_COUNT; i++)
      pSession->CompleteSync(i, RCC_COMM_FAILURE);    // Abort active sync operation
   DebugPrintf(_T("Network receiver thread stopped"));
   free(pRawMsg);
   free(pMsgBuffer);
#ifdef _WITH_ENCRYPTION
   free(pDecryptionBuffer);
#endif

   // Close socket
   shutdown(pSession->m_hSocket, SHUT_WR);
   {
	   char cTmp;
	   while(recv(pSession->m_hSocket, &cTmp, 1, 0) > 0);
   }
   shutdown(pSession->m_hSocket, SHUT_RD);
   closesocket(pSession->m_hSocket);
   return THREAD_OK;
}

/**
 * Connect to server
 */
#if defined(__HP_aCC) || defined(__SUNPRO_CC)
extern "C"
#endif
UINT32 LIBNXCL_EXPORTABLE NXCConnect(UINT32 dwFlags, const TCHAR *pszServer, const TCHAR *pszLogin, 
                                    const TCHAR *pszPassword, UINT32 dwCertLen,
                                    BOOL (* pfSign)(BYTE *, UINT32, BYTE *, UINT32 *, void *),
                                    void *pSignArg, NXC_SESSION *phSession, const TCHAR *pszClientInfo,
                                    TCHAR **ppszUpgradeURL)
{
   struct sockaddr_in servAddr;
   NXCPMessage msg, *pResp;
   UINT32 dwRetCode = RCC_COMM_FAILURE;
   SOCKET hSocket;
   THREAD hThread;
   TCHAR  *pszPort, szBuffer[64], szHostName[128];
	BYTE challenge[CLIENT_CHALLENGE_SIZE];
   WORD wPort = SERVER_LISTEN_PORT_FOR_CLIENTS;

   nx_strncpy(szHostName, pszServer, 128);

	if (ppszUpgradeURL != NULL)
		*ppszUpgradeURL = NULL;

   // Check if server given in form host:port
   pszPort = _tcschr(szHostName, _T(':'));
   if (pszPort != NULL)
   {
      TCHAR *pErr;
      int nTemp;

      *pszPort = 0;
      pszPort++;
      nTemp = _tcstol(pszPort, &pErr, 10);
      if ((*pErr != 0) || (nTemp < 1) || (nTemp > 65535))
         return RCC_INVALID_ARGUMENT;
      wPort = (WORD)nTemp;
   }

   // Prepare address structure
   memset(&servAddr, 0, sizeof(struct sockaddr_in));
   servAddr.sin_family = AF_INET;
   servAddr.sin_port = htons(wPort);
   servAddr.sin_addr.s_addr = ResolveHostName(szHostName);

   if (servAddr.sin_addr.s_addr != INADDR_NONE)
   {
      // Create socket
      if ((hSocket = socket(AF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET)
      {
         // Connect to target
         if (connect(hSocket, (struct sockaddr *)&servAddr, sizeof(struct sockaddr_in)) == 0)
         {
            NXCL_Session *pSession;

            // Create new session and start receiver thread
            pSession = new NXCL_Session;
            pSession->attach(hSocket);
            hThread = ThreadCreateEx((THREAD_RESULT (THREAD_CALL *)(void *))NetReceiver, 0, pSession);
            if (hThread != INVALID_THREAD_HANDLE)
               pSession->SetRecvThread(hThread);

            // Query server information
            msg.setId(pSession->CreateRqId());
            msg.setCode(CMD_GET_SERVER_INFO);
            if (pSession->SendMsg(&msg))
            {
               // Receive response message
               pResp = pSession->WaitForMessage(CMD_REQUEST_COMPLETED, msg.getId());
               if (pResp != NULL)
               {
                  dwRetCode = pResp->getFieldAsUInt32(VID_RCC);
                  if (dwRetCode == RCC_SUCCESS)
                  {
                     pResp->getFieldAsBinary(VID_SERVER_ID, pSession->m_bsServerId, 8);
                     if (dwFlags & NXCF_EXACT_VERSION_MATCH)
                     {
                        TCHAR szServerVersion[64];

                        pResp->getFieldAsString(VID_SERVER_VERSION, szServerVersion, 64);
                        if (_tcsncmp(szServerVersion, NETXMS_VERSION_STRING, 64))
                           dwRetCode = RCC_VERSION_MISMATCH;
                     }
							if (!(dwFlags & NXCF_IGNORE_PROTOCOL_VERSION))
							{
								if (pResp->getFieldAsUInt32(VID_PROTOCOL_VERSION) != CLIENT_PROTOCOL_VERSION)
									dwRetCode = RCC_BAD_PROTOCOL;
							}
							if (ppszUpgradeURL != NULL)
								*ppszUpgradeURL = pResp->getFieldAsString(VID_CONSOLE_UPGRADE_URL);
							pResp->getFieldAsBinary(VID_CHALLENGE, challenge, CLIENT_CHALLENGE_SIZE);
							pResp->getFieldAsString(VID_TIMEZONE, pSession->getServerTimeZone(), MAX_TZ_LEN);
                  }
                  delete pResp;

                  // Request encryption if needed
                  if ((dwRetCode == RCC_SUCCESS) && (dwFlags & NXCF_ENCRYPT))
                  {
                     msg.deleteAllFields();
                     msg.setId(pSession->CreateRqId());
                     msg.setCode(CMD_REQUEST_ENCRYPTION);
                     if (pSession->SendMsg(&msg))
                     {
                        dwRetCode = pSession->WaitForRCC(msg.getId());
                     }
                     else
                     {
                        dwRetCode = RCC_COMM_FAILURE;
                     }
                  }

                  if (dwRetCode == RCC_SUCCESS)
                  {
                     // Prepare login message
                     msg.deleteAllFields();
                     msg.setId(pSession->CreateRqId());
                     msg.setCode(CMD_LOGIN);
                     msg.setField(VID_LOGIN_NAME, pszLogin);
							if (dwFlags & NXCF_USE_CERTIFICATE)
							{
								BYTE signature[256];
								UINT32 dwSigLen;

								dwSigLen = 256;
								if (!pfSign(challenge, CLIENT_CHALLENGE_SIZE, signature, &dwSigLen, pSignArg))
								{
									dwRetCode = RCC_LOCAL_CRYPTO_ERROR;
									goto crypto_error;
								}
								msg.setField(VID_SIGNATURE, signature, dwSigLen);
								msg.setField(VID_CERTIFICATE, (BYTE *)pszPassword, dwCertLen);
								msg.setField(VID_AUTH_TYPE, (WORD)NETXMS_AUTH_TYPE_CERTIFICATE);
							}
							else
							{
								msg.setField(VID_PASSWORD, pszPassword);
								msg.setField(VID_AUTH_TYPE, (WORD)NETXMS_AUTH_TYPE_PASSWORD);
							}
                     msg.setField(VID_CLIENT_INFO, pszClientInfo);
                     msg.setField(VID_LIBNXCL_VERSION, NETXMS_VERSION_STRING);
                     GetOSVersionString(szBuffer, 64);
                     msg.setField(VID_OS_INFO, szBuffer);
                     if (pSession->SendMsg(&msg))
                     {
                        // Receive response message
                        pResp = pSession->WaitForMessage(CMD_LOGIN_RESP, msg.getId());
                        if (pResp != NULL)
                        {
                           dwRetCode = pResp->getFieldAsUInt32(VID_RCC);
                           if (dwRetCode == RCC_SUCCESS)
                              pSession->parseLoginMessage(pResp);
                           delete pResp;
                        }
                        else
                        {
                           // Connection is broken or timed out
                           dwRetCode = RCC_TIMEOUT;
                        }
                     }
                     else
                     {
                        dwRetCode = RCC_COMM_FAILURE;
                     }
crypto_error:
							;
                  }
               }
               else
               {
                  // Connection is broken or timed out
                  dwRetCode = RCC_TIMEOUT;
               }
            }

            if (dwRetCode == RCC_SUCCESS)
            {
               *phSession = pSession;
            }
            else
            {
               delete pSession;
            }
         }
         else  // connect() failed
         {
            closesocket(hSocket);
         }
      }
   }

   if (dwRetCode != RCC_SUCCESS)
      *phSession = NULL;

   return dwRetCode;
}


//
// Disconnect from server
//

void LIBNXCL_EXPORTABLE NXCDisconnect(NXC_SESSION hSession)
{
   if (hSession != NULL)
   {
//      ((NXCL_Session *)hSession)->Disconnect();
      delete ((NXCL_Session *)hSession);
   }
}
