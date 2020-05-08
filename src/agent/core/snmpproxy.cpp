/*
** NetXMS multiplatform core agent
** Copyright (C) 2003-2016 Victor Kirhenshtein
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
** File: snmpproxy.cpp
**
**/

#include "nxagentd.h"

/**
 * SNMP buffer size
 */
#define SNMP_BUFFER_SIZE		65536

/**
 * SNMP proxy stats
 */
static VolatileCounter64 s_serverRequests = 0;
static VolatileCounter64 s_snmpRequests = 0;
static VolatileCounter64 s_snmpResponses = 0;
extern UINT64 g_snmpTraps;

/**
 * Handler for SNMP proxy information parameters
 */
LONG H_SNMPProxyStats(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   switch(*arg)
   {
      case 'R':   // SNMP requests
         ret_uint64(value, s_snmpRequests);
         break;
      case 'r':   // SNMP responses
         ret_uint64(value, s_snmpResponses);
         break;
      case 'S':   // server requests
         ret_uint64(value, s_serverRequests);
         break;
      case 'T':   // SNMP traps
         ret_uint64(value, g_snmpTraps);
         break;
      default:
         return SYSINFO_RC_UNSUPPORTED;
   }
   return SYSINFO_RC_SUCCESS;
}

/**
 * Read PDU from network
 */
static bool ReadPDU(SOCKET hSocket, BYTE *pdu, UINT32 *pdwSize, UINT32 timeout)
{
	SocketPoller sp;
	sp.add(hSocket);
	if (sp.poll(timeout) <= 0)
	   return false;

	int bytes = recv(hSocket, (char *)pdu, SNMP_BUFFER_SIZE, 0);
	if (bytes >= 0)
	{
		*pdwSize = bytes;
		return true;
	}
	return false;
}

/**
 * Send SNMP request to target, receive response, and send it to server
 */
void CommSession::proxySnmpRequest(NXCPMessage *request)
{
   UINT32 requestId = request->getId();
   NXCPMessage response(CMD_REQUEST_COMPLETED, requestId, m_protocolVersion);

   InterlockedIncrement64(&s_serverRequests);
   size_t sizeIn;
   const BYTE *pduIn = request->getBinaryFieldPtr(VID_PDU, &sizeIn);
   if ((pduIn != NULL) && (sizeIn > 0))
   {
      InetAddress addr = request->getFieldAsInetAddress(VID_IP_ADDRESS);
      SOCKET hSocket = CreateSocket(addr.getFamily(), SOCK_DGRAM, 0);
      if (hSocket != INVALID_SOCKET)
      {
         SockAddrBuffer sa;
         addr.fillSockAddr(&sa, request->getFieldAsUInt16(VID_PORT));
         if (connect(hSocket, (struct sockaddr *)&sa, SA_LEN((struct sockaddr *)&sa)) != -1)
         {
            BYTE *pduOut = (BYTE *)MemAlloc(SNMP_BUFFER_SIZE);
            if (pduOut != nullptr)
            {
               uint32_t serverTimeout = request->getFieldAsUInt32(VID_TIMEOUT);
               uint32_t timeout = (g_snmpTimeout != 0) ? g_snmpTimeout : ((serverTimeout != 0) ? serverTimeout : 1000);   // 1 second if not set

               uint32_t rcc = ERR_REQUEST_TIMEOUT;
               int nRetries;
               for(nRetries = 0; nRetries < 3; nRetries++)
               {
                  if (send(hSocket, (char *)pduIn, (int)sizeIn, 0) == (int)sizeIn)
                  {
                     InterlockedIncrement64(&s_snmpRequests);
                     UINT32 dwSizeOut;
                     if (ReadPDU(hSocket, pduOut, &dwSizeOut, timeout))
                     {
                        InterlockedIncrement64(&s_snmpResponses);
                        response.setField(VID_PDU_SIZE, dwSizeOut);
                        response.setField(VID_PDU, pduOut, dwSizeOut);
                        rcc = ERR_SUCCESS;
                        break;
                     }
                     else
                     {
                        int error = WSAGetLastError();
                        TCHAR buffer[1024];
                        debugPrintf(7, _T("proxySnmpRequest(%d): read failure or timeout (%d: %s)"), requestId, error, GetLastSocketErrorText(buffer, 1024));
                        if (error == WSAECONNREFUSED)
                        {
                           rcc = ERR_SOCKET_ERROR;
                           break; // No point retrying after ECONNREFUSED
                        }
                     }
                  }
                  else
                  {
                     int error = WSAGetLastError();
                     TCHAR buffer[1024];
                     debugPrintf(7, _T("proxySnmpRequest(%d): send() call failed (%d: %s)"), requestId, error, GetLastSocketErrorText(buffer, 1024));
                  }
               }
               MemFree(pduOut);
               response.setField(VID_RCC, rcc);
               debugPrintf(7, _T("proxySnmpRequest(%d, %s): %s (%d retries)"), requestId, addr.toString().cstr(), (nRetries == 3) ? _T("failure") : _T("success"), nRetries);
            }
            else
            {
               response.setField(VID_RCC, ERR_OUT_OF_RESOURCES);
               debugPrintf(7, _T("proxySnmpRequest(%d, %s): memory allocation failure"), requestId, addr.toString().cstr());
            }
         }
         else
         {
            int error = WSAGetLastError();
            TCHAR buffer[1024];
            debugPrintf(7, _T("proxySnmpRequest(%d, %s): connect() call failed (%d: %s)"), requestId, addr.toString().cstr(), error, GetLastSocketErrorText(buffer, 1024));
            response.setField(VID_RCC, ERR_SOCKET_ERROR);
         }
         closesocket(hSocket);
      }
      else
      {
         response.setField(VID_RCC, ERR_SOCKET_ERROR);
         debugPrintf(7, _T("proxySnmpRequest(%d, %s): socket() call failed (%d)"), requestId,  (const TCHAR *)addr.toString(), WSAGetLastError());
      }
   }
   else
   {
      response.setField(VID_RCC, ERR_MALFORMED_COMMAND);
      debugPrintf(7, _T("proxySnmpRequest(%d): input PDU is missing or empty"), requestId);
   }
	sendMessage(&response);
	delete request;
	decRefCount();
}
