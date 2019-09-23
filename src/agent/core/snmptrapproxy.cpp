/*
** NetXMS multiplatform core agent
** Copyright (C) 2014-2019 Raden Solutions
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
** File: snmptrapproxy.cpp
**
**/

#include "nxagentd.h"

/**
 * Counter for received SNMP traps
 */
UINT64 g_snmpTraps = 0;

/**
 * Raw SNMP packet received from network
 */
struct SNMPPacket
{
   InetAddress addr;  // Source address
   UINT16 port;       // Receiver port
   BYTE *data;        // Raw packet data
   size_t lenght;     // Data length

   SNMPPacket(const InetAddress& _addr, UINT16 _port, BYTE *_data, size_t _length)
   {
      addr = _addr;
      port = _port;
      data = _data;
      lenght = _length;
   }

   ~SNMPPacket()
   {
      MemFree(data);
   }
};

/**
 * Sender queue
 */
static ObjectQueue<SNMPPacket> s_snmpTrapQueue(true);

/**
 * Shutdown trap sender
 */
void ShutdownSNMPTrapSender()
{
	s_snmpTrapQueue.setShutdownMode();
}

/**
 * SNMP trap read thread
 */
THREAD_RESULT THREAD_CALL SNMPTrapReceiver(void *pArg)
{
   if (g_dwFlags & AF_DISABLE_IPV4)
   {
      nxlog_debug(1, _T("SNMPTrapReceiver: IPv4 disabled, exiting"));
      return THREAD_OK;
   }

   SOCKET hSocket = CreateSocket(AF_INET, SOCK_DGRAM, 0);
   if (hSocket == INVALID_SOCKET)
   {
      TCHAR buffer[1024];
      nxlog_debug(1, _T("SNMPTrapReceiver: cannot create socket (%s)"), GetLastSocketErrorText(buffer, 1024));
      return THREAD_OK;
   }

   SetSocketExclusiveAddrUse(hSocket);
   SetSocketReuseFlag(hSocket);

   // Fill in local address structure
   struct sockaddr_in sa;
   memset(&sa, 0, sizeof(sa));
   sa.sin_family = AF_INET;
   if (!_tcscmp(g_szSNMPTrapListenAddress, _T("*")))
   {
      sa.sin_addr.s_addr = htonl(INADDR_ANY);
   }
   else
	{
      InetAddress bindAddress = InetAddress::resolveHostName(g_szSNMPTrapListenAddress, AF_INET);
      if (bindAddress.isValid() && (bindAddress.getFamily() == AF_INET))
      {
		   sa.sin_addr.s_addr = htonl(bindAddress.getAddressV4());
      }
      else
      {
   		sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      }
	}
   sa.sin_port = htons(g_snmpTrapPort);

   // Bind socket
   if (bind(hSocket, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) != 0)
   {
      TCHAR buffer[1024];
      nxlog_debug(1, _T("SNMPTrapReceiver: cannot bind socket (%s)"), GetLastSocketErrorText(buffer, 1024));
      closesocket(hSocket);
      return THREAD_OK;
   }

   TCHAR ipAddrStr[64];
   DebugPrintf(3, _T("SNMPTrapReceiver: listening on %s:%d"),
      IpToStr(ntohl(sa.sin_addr.s_addr), ipAddrStr), (int)ntohs(sa.sin_port));

   SNMP_TrapProxyTransport *pTransport = new SNMP_TrapProxyTransport(hSocket);
   pTransport->enableEngineIdAutoupdate(true);
   pTransport->setPeerUpdatedOnRecv(true);

   // Wait for packets
   while(!(g_dwFlags & AF_SHUTDOWN))
   {
      BYTE *packet = NULL;
      socklen_t nAddrLen = sizeof(struct sockaddr_in);
      int bytes = pTransport->readRawMessage(&packet, 2000, (struct sockaddr *)&sa, &nAddrLen);
      if ((bytes > 0) && (packet != NULL))
      {
         SNMPPacket *message = new SNMPPacket(InetAddress::createFromSockaddr(reinterpret_cast<struct sockaddr*>(&sa)), g_snmpTrapPort, packet, bytes);
         nxlog_debug(6, _T("SNMPTrapReceiver: packet received from %s"), message->addr.toString(ipAddrStr));
         g_snmpTraps++;
         s_snmpTrapQueue.put(message);
      }
      else
      {
         // Sleep on error
         ThreadSleepMs(100);
      }
   }

   delete pTransport;
   DebugPrintf(1, _T("SNMP Trap Receiver terminated"));
   return THREAD_OK;
}

/**
 * SNMP trap read thread
 */
THREAD_RESULT THREAD_CALL SNMPTrapSender(void *pArg)
{
	DebugPrintf(1, _T("SNMP Trap sender thread started"));
   while(1)
   {
      DebugPrintf(8, _T("SNMPTrapSender: waiting for message"));
      SNMPPacket *pdu = s_snmpTrapQueue.getOrBlock();
      if (pdu == INVALID_POINTER_VALUE)
         break;

      DebugPrintf(6, _T("SNMPTrapSender: got trap from queue"));
      bool sent = false;

      NXCPMessage *msg = new NXCPMessage(CMD_SNMP_TRAP, GenerateMessageId(), 4); // Use version 4
      msg->setField(VID_IP_ADDRESS, pdu->addr);
      msg->setField(VID_PORT, pdu->port);
      msg->setField(VID_PDU_SIZE, static_cast<UINT32>(pdu->lenght));
      msg->setField(VID_PDU, pdu->data, pdu->lenght);
      msg->setField(VID_ZONE_UIN, g_zoneUIN);

      if (g_dwFlags & AF_SUBAGENT_LOADER)
      {
         sent = SendMessageToMasterAgent(msg);
      }
      else
      {
         MutexLock(g_hSessionListAccess);
         for(UINT32 i = 0; i < g_dwMaxSessions; i++)
         {
            if (g_pSessionList[i] != NULL)
            {
               if (g_pSessionList[i]->canAcceptTraps())
               {
                  g_pSessionList[i]->sendMessage(msg);
                  sent = true;
               }
            }
         }
         MutexUnlock(g_hSessionListAccess);
      }

      delete msg;
      if (!sent)
      {
         DebugPrintf(6, _T("Cannot forward SNMP trap to server"));
         s_snmpTrapQueue.insert(pdu);
			ThreadSleep(1);
      }
      else
      {
         DebugPrintf(6, _T("SNMP trap successfully forwarded to server"));
         delete pdu;
      }
   }
	DebugPrintf(1, _T("SNMP trap sender thread terminated"));
   return THREAD_OK;
}

/** Implementation of class SNMP_TrapProxyTransport **/

/**
 * Constructor
 */
SNMP_TrapProxyTransport::SNMP_TrapProxyTransport(SOCKET hSocket) : SNMP_UDPTransport(hSocket)
{
}

/**
 * Read PDU from socket but do not decode and parse it
 */
int SNMP_TrapProxyTransport::readRawMessage(BYTE **rawData, UINT32 dwTimeout, struct sockaddr *pSender, socklen_t *piAddrSize)
{
   int bytes;
   size_t pduLength;

   if (m_dwBytesInBuffer < 2)
   {
      bytes = recvData(dwTimeout, pSender, piAddrSize);
      if (bytes <= 0)
      {
         clearBuffer();
         return bytes;
      }
      m_dwBytesInBuffer += bytes;
   }

   pduLength = preParsePDU();
   if (pduLength == 0)
   {
      // Clear buffer
      clearBuffer();
      return 0;
   }

   // Move existing data to the beginning of buffer if there are not enough space at the end
   if (pduLength > m_dwBufferSize - m_dwBufferPos)
   {
      memmove(m_pBuffer, &m_pBuffer[m_dwBufferPos], m_dwBytesInBuffer);
      m_dwBufferPos = 0;
   }

   // Read entire PDU into buffer
   while(m_dwBytesInBuffer < pduLength)
   {
      bytes = recvData(dwTimeout, pSender, piAddrSize);
      if (bytes <= 0)
      {
         clearBuffer();
         return bytes;
      }
      m_dwBytesInBuffer += bytes;
   }

   *rawData = (BYTE*)MemAlloc(pduLength);
   memcpy(*rawData, &m_pBuffer[m_dwBufferPos], pduLength);

   m_dwBytesInBuffer -= pduLength;
   if (m_dwBytesInBuffer == 0)
      m_dwBufferPos = 0;

   return (int)pduLength;
}
