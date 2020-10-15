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
** File: ntcb.h
**
**/

#ifndef _ntcb_h_
#define _ntcb_h_

#include <nms_common.h>
#include <nms_util.h>
#include <nms_core.h>
#include <nxmodule.h>
#include <nxconfig.h>

#define DEBUG_TAG_NTCB  _T("ntcb")

/**
 * NTCB device session
 */
class NTCBDeviceSession
{
private:
   uint32_t m_id;
   SocketConnection m_socket;
   InetAddress m_address;
   uint32_t m_ntcbSenderId;
   uint32_t m_ntcbReceiverId;
   int m_flexProtoVersion;
   int m_flexStructVersion;
   int m_flexFieldCount;
   uint8_t m_flexFieldMask[32];

   void readThread();
   void processNTCBMessage(const void *data, size_t size);
   void processFLEXMessage();
   void processArchiveTelemetry();
   void processCurrentTelemetry();
   void processExtraordinaryTelemetry();
   bool readTelemetryRecord();

   void sendNTCBMessage(const void *data, size_t size);

   void debugPrintf(int level, const TCHAR *format, ...);

public:
   NTCBDeviceSession(SOCKET s, const InetAddress& addr);
   ~NTCBDeviceSession();

   bool start();
};

#endif
