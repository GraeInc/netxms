/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2015 Victor Kirhenshtein
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
** File: libnxcc.h
**
**/

#ifndef _libnxcc_h_
#define _libnxcc_h_

#include <nxcc.h>

/**
 * Max node ID
 */
#define CLUSTER_MAX_NODE_ID   32

/**
 * Max sockets per node
 */
#define CLUSTER_MAX_SOCKETS   16

/**
 * Node information
 */
struct ClusterNodeInfo
{
   UINT32 m_id;
   InetAddress *m_addr;
   UINT16 m_port;
   SOCKET m_socket;
   THREAD m_thread;
   ClusterNodeState m_state;
   bool m_master;
   MUTEX m_mutex;
   THREAD m_receiverThread;
};

/**
 * Internal functions
 */
void ClusterDebug(int level, const TCHAR *format, ...);

void ClusterDisconnect();

/**
 * Global cluster node settings
 */
extern UINT32 g_nxccNodeId;
extern ClusterEventHandler *g_nxccEventHandler;
extern ClusterNodeState g_nxccState;
extern ClusterNodeInfo g_nxccNodes[CLUSTER_MAX_NODE_ID];
extern bool g_nxccInitialized;
extern bool g_nxccMasterNode;
extern bool g_nxccShutdown;
extern UINT16 g_nxccListenPort;

#endif
