/*
** NetXMS - Network Management System
** Copyright (C) 2003-2022 Raden Solutions
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

#include "netsvc.h"

/**
 * Check SSH service
 */
int CheckSSH(const char *hostname, const InetAddress &addr, uint16_t port, uint32_t timeout)
{
	int rc;
	SOCKET hSocket = NetConnectTCP(hostname, addr, port, timeout);
	if (hSocket != INVALID_SOCKET)
	{
		char szBuff[512];
		char szTmp[128];

		rc = PC_ERR_HANDSHAKE;

		if (SocketCanRead(hSocket, 1000))
		{
			if (NetRead(hSocket, szBuff, sizeof(szBuff)) >= 8)
			{
				int nMajor, nMinor;

				if (sscanf(szBuff, "SSH-%d.%d-", &nMajor, &nMinor) == 2)
				{
					snprintf(szTmp, sizeof(szTmp), "SSH-%d.%d-NetXMS\n", nMajor, nMinor);
					if (NetWrite(hSocket, szTmp, (int)strlen(szTmp)))
					{
						rc = PC_ERR_NONE;
					}
				}
			}
		}

		NetClose(hSocket);
	}
	else
	{
		rc = PC_ERR_CONNECT;
	}

	return rc;
}

/**
 * Check SSH service - metric sub-handler
 */
LONG NetworkServiceStatus_SSH(const char *host, const char *port, const OptionList& options, int *result)
{
	if (host[0] == 0)
		return SYSINFO_RC_UNSUPPORTED;

	uint16_t nPort = static_cast<uint16_t>(strtoul(port, nullptr, 10));
	if (nPort == 0)
		nPort = 22;

	*result = CheckSSH(host, InetAddress::INVALID, nPort, options.getAsUInt32(_T("timeout"), g_netsvcTimeout));
	return SYSINFO_RC_SUCCESS;
}

/**
 * Check SSH service - legacy metrics handler
 */
LONG H_CheckSSH(const TCHAR *param, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   LONG nRet = SYSINFO_RC_SUCCESS;

   char szHost[256];
   TCHAR szPort[256];
   AgentGetParameterArgA(param, 1, szHost, sizeof(szHost));
   AgentGetParameterArg(param, 2, szPort, sizeof(szPort));

   if (szHost[0] == 0)
      return SYSINFO_RC_ERROR;

   uint16_t nPort = static_cast<uint16_t>(_tcstoul(szPort, nullptr, 10));
   if (nPort == 0)
      nPort = 22;

   uint32_t timeout = GetTimeoutFromArgs(param, 3);
   int64_t start = GetCurrentTimeMs();
   int result = CheckSSH(szHost, InetAddress::INVALID, nPort, timeout);
   if (*arg == 'R')
   {
      if (result == PC_ERR_NONE)
         ret_int64(value, GetCurrentTimeMs() - start);
      else if (g_netsvcFlags & NETSVC_AF_NEGATIVE_TIME_ON_ERROR)
         ret_int64(value, -(GetCurrentTimeMs() - start));
      else
         nRet = SYSINFO_RC_ERROR;
   }
   else
   {
      ret_int(value, result);
   }
   return nRet;
}
