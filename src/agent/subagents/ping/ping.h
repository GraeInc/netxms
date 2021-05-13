/*
** NetXMS PING subagent
** Copyright (C) 2004-2018 Victor Kirhenshtein
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
** File: ping.h
**
**/

#ifndef _ping_h_
#define _ping_h_

#include <nms_common.h>
#include <nms_util.h>
#include <nms_agent.h>
#include <math.h>

/**
 * Constants
 */
#define MAX_POLLS_PER_MINUTE     6000

#define PING_OPT_ALLOW_AUTOCONFIGURE   0x0001
#define PING_OPT_DONT_FRAGMENT         0x0002

#define DEBUG_TAG _T("ping")

/**
 * Target information structure
 */
struct PING_TARGET
{
   InetAddress ipAddr;
   TCHAR dnsName[MAX_DB_STRING];
   TCHAR name[MAX_DB_STRING];
   UINT32 packetSize;
   UINT32 avgRTT;
   UINT32 lastRTT;
   UINT32 minRTT;
   UINT32 maxRTT;
   UINT32 stdDevRTT;
   UINT32 packetLoss;
   UINT32 cumulativeMinRTT;
   UINT32 cumulativeMaxRTT;
   UINT32 movingAvgRTT;
   UINT32 history[MAX_POLLS_PER_MINUTE];
   int bufPos;
	int ipAddrAge;
	bool dontFragment;
	bool automatic;
	time_t lastDataRead;
};

StructArray<InetAddress> *ScanAddressRange(const InetAddress& start, const InetAddress& end, UINT32 timeout);

#endif
