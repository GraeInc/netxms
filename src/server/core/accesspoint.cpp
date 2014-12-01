/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2013 Victor Kirhenshtein
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
** File: accesspoint.cpp
**/

#include "nxcore.h"

/**
 * Default constructor
 */
AccessPoint::AccessPoint() : DataCollectionTarget()
{
	m_nodeId = 0;
	memset(m_macAddr, 0, MAC_ADDR_LENGTH);
	m_vendor = NULL;
	m_model = NULL;
	m_serialNumber = NULL;
	m_radioInterfaces = NULL;
   m_state = AP_ADOPTED;
   m_prevState = m_state;
}

/**
 * Constructor for creating new mobile device object
 */
AccessPoint::AccessPoint(const TCHAR *name, BYTE *macAddr) : DataCollectionTarget(name)
{
	m_nodeId = 0;
	memcpy(m_macAddr, macAddr, MAC_ADDR_LENGTH);
	m_vendor = NULL;
	m_model = NULL;
	m_serialNumber = NULL;
	m_radioInterfaces = NULL;
   m_state = AP_ADOPTED;
   m_prevState = m_state;
	m_isHidden = true;
}

/**
 * Destructor
 */
AccessPoint::~AccessPoint()
{
	safe_free(m_vendor);
	safe_free(m_model);
	safe_free(m_serialNumber);
	delete m_radioInterfaces;
}

/**
 * Create object from database data
 */
BOOL AccessPoint::loadFromDatabase(UINT32 dwId)
{
   m_id = dwId;

   if (!loadCommonProperties())
   {
      DbgPrintf(2, _T("Cannot load common properties for access point object %d"), dwId);
      return FALSE;
   }

	TCHAR query[256];
	_sntprintf(query, 256, _T("SELECT mac_address,vendor,model,serial_number,node_id,ap_state FROM access_points WHERE id=%d"), (int)m_id);
	DB_RESULT hResult = DBSelect(g_hCoreDB, query);
	if (hResult == NULL)
		return FALSE;

	DBGetFieldByteArray2(hResult, 0, 0, m_macAddr, MAC_ADDR_LENGTH, 0);
	m_vendor = DBGetField(hResult, 0, 1, NULL, 0);
	m_model = DBGetField(hResult, 0, 2, NULL, 0);
	m_serialNumber = DBGetField(hResult, 0, 3, NULL, 0);
	m_nodeId = DBGetFieldULong(hResult, 0, 4);
   m_state = (DBGetFieldLong(hResult, 0, 5) == AP_ADOPTED) ? AP_ADOPTED : AP_UNADOPTED;
   m_prevState = (m_state != AP_DOWN) ? m_state : AP_ADOPTED;
	DBFreeResult(hResult);

   // Load DCI and access list
   loadACLFromDB();
   loadItemsFromDB();
   for(int i = 0; i < m_dcObjects->size(); i++)
      if (!m_dcObjects->get(i)->loadThresholdsFromDB())
         return FALSE;

   // Link access point to node
	BOOL success = FALSE;
   if (!m_isDeleted)
   {
      NetObj *object = FindObjectById(m_nodeId);
      if (object == NULL)
      {
         nxlog_write(MSG_INVALID_NODE_ID, EVENTLOG_ERROR_TYPE, "dd", dwId, m_nodeId);
      }
      else if (object->getObjectClass() != OBJECT_NODE)
      {
         nxlog_write(MSG_NODE_NOT_NODE, EVENTLOG_ERROR_TYPE, "dd", dwId, m_nodeId);
      }
      else
      {
         object->AddChild(this);
         AddParent(object);
         success = TRUE;
      }
   }
   else
   {
      success = TRUE;
   }

   return success;
}

/**
 * Save object to database
 */
BOOL AccessPoint::saveToDatabase(DB_HANDLE hdb)
{
   // Lock object's access
   lockProperties();

   saveCommonProperties(hdb);

   BOOL bResult;
	DB_STATEMENT hStmt;
   if (IsDatabaseRecordExist(hdb, _T("access_points"), _T("id"), m_id))
		hStmt = DBPrepare(hdb, _T("UPDATE access_points SET mac_address=?,vendor=?,model=?,serial_number=?,node_id=?,ap_state=? WHERE id=?"));
	else
		hStmt = DBPrepare(hdb, _T("INSERT INTO access_points (mac_address,vendor,model,serial_number,node_id,ap_state,id) VALUES (?,?,?,?,?,?,?)"));
	if (hStmt != NULL)
	{
		TCHAR macStr[16];
		DBBind(hStmt, 1, DB_SQLTYPE_VARCHAR, BinToStr(m_macAddr, MAC_ADDR_LENGTH, macStr), DB_BIND_STATIC);
		DBBind(hStmt, 2, DB_SQLTYPE_VARCHAR, CHECK_NULL_EX(m_vendor), DB_BIND_STATIC);
		DBBind(hStmt, 3, DB_SQLTYPE_VARCHAR, CHECK_NULL_EX(m_model), DB_BIND_STATIC);
		DBBind(hStmt, 4, DB_SQLTYPE_VARCHAR, CHECK_NULL_EX(m_serialNumber), DB_BIND_STATIC);
		DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, m_nodeId);
		DBBind(hStmt, 6, DB_SQLTYPE_INTEGER, (INT32)m_state);
		DBBind(hStmt, 7, DB_SQLTYPE_INTEGER, m_id);

		bResult = DBExecute(hStmt);

		DBFreeStatement(hStmt);
	}
	else
	{
		bResult = FALSE;
	}

   // Save data collection items
   if (bResult)
   {
		lockDciAccess(false);
      for(int i = 0; i < m_dcObjects->size(); i++)
         m_dcObjects->get(i)->saveToDB(hdb);
		unlockDciAccess();
   }

   // Save access list
   saveACLToDB(hdb);

   // Clear modifications flag and unlock object
	if (bResult)
		m_isModified = false;
   unlockProperties();

   return bResult;
}

/**
 * Delete object from database
 */
bool AccessPoint::deleteFromDatabase(DB_HANDLE hdb)
{
   bool success = DataCollectionTarget::deleteFromDatabase(hdb);
   if (success)
      success = executeQueryOnObject(hdb, _T("DELETE FROM access_points WHERE id=?"));
   return success;
}

/**
 * Create CSCP message with object's data
 */
void AccessPoint::fillMessage(NXCPMessage *msg)
{
   DataCollectionTarget::fillMessage(msg);
	msg->setField(VID_NODE_ID, m_nodeId);
	msg->setField(VID_MAC_ADDR, m_macAddr, MAC_ADDR_LENGTH);
	msg->setField(VID_VENDOR, CHECK_NULL_EX(m_vendor));
	msg->setField(VID_MODEL, CHECK_NULL_EX(m_model));
	msg->setField(VID_SERIAL_NUMBER, CHECK_NULL_EX(m_serialNumber));
   msg->setField(VID_STATE, (UINT16)m_state);

   if (m_radioInterfaces != NULL)
   {
      msg->setField(VID_RADIO_COUNT, (WORD)m_radioInterfaces->size());
      UINT32 varId = VID_RADIO_LIST_BASE;
      for(int i = 0; i < m_radioInterfaces->size(); i++)
      {
         RadioInterfaceInfo *rif = m_radioInterfaces->get(i);
         msg->setField(varId++, (UINT32)rif->index);
         msg->setField(varId++, rif->name);
         msg->setField(varId++, rif->macAddr, MAC_ADDR_LENGTH);
         msg->setField(varId++, rif->channel);
         msg->setField(varId++, (UINT32)rif->powerDBm);
         msg->setField(varId++, (UINT32)rif->powerMW);
         varId += 4;
      }
   }
   else
   {
      msg->setField(VID_RADIO_COUNT, (WORD)0);
   }
}

/**
 * Modify object from message
 */
UINT32 AccessPoint::modifyFromMessage(NXCPMessage *pRequest, BOOL bAlreadyLocked)
{
   if (!bAlreadyLocked)
      lockProperties();

   return DataCollectionTarget::modifyFromMessage(pRequest, TRUE);
}

/**
 * Attach access point to node
 */
void AccessPoint::attachToNode(UINT32 nodeId)
{
	if (m_nodeId == nodeId)
		return;

	if (m_nodeId != 0)
	{
		Node *currNode = (Node *)FindObjectById(m_nodeId, OBJECT_NODE);
		if (currNode != NULL)
		{
			currNode->DeleteChild(this);
			DeleteParent(currNode);
		}
	}

	Node *newNode = (Node *)FindObjectById(nodeId, OBJECT_NODE);
	if (newNode != NULL)
	{
		newNode->AddChild(this);
		AddParent(newNode);
	}

	lockProperties();
	m_nodeId = nodeId;
	setModified();
	unlockProperties();
}

/**
 * Update radio interfaces information
 */
void AccessPoint::updateRadioInterfaces(ObjectArray<RadioInterfaceInfo> *ri)
{
	lockProperties();
	if (m_radioInterfaces == NULL)
		m_radioInterfaces = new ObjectArray<RadioInterfaceInfo>(ri->size(), 4, true);
	m_radioInterfaces->clear();
	for(int i = 0; i < ri->size(); i++)
	{
		RadioInterfaceInfo *info = new RadioInterfaceInfo;
		memcpy(info, ri->get(i), sizeof(RadioInterfaceInfo));
		m_radioInterfaces->add(info);
	}
	unlockProperties();
}

/**
 * Check if given radio interface index (radio ID) is on this access point
 */
bool AccessPoint::isMyRadio(int rfIndex)
{
	bool result = false;
	lockProperties();
	if (m_radioInterfaces != NULL)
	{
		for(int i = 0; i < m_radioInterfaces->size(); i++)
		{
			if (m_radioInterfaces->get(i)->index == rfIndex)
			{
				result = true;
				break;
			}
		}
	}
	unlockProperties();
	return result;
}

/**
 * Check if given radio MAC address (BSSID) is on this access point
 */
bool AccessPoint::isMyRadio(const BYTE *macAddr)
{
	bool result = false;
	lockProperties();
	if (m_radioInterfaces != NULL)
	{
		for(int i = 0; i < m_radioInterfaces->size(); i++)
		{
         if (!memcmp(m_radioInterfaces->get(i)->macAddr, macAddr, MAC_ADDR_LENGTH))
			{
				result = true;
				break;
			}
		}
	}
	unlockProperties();
	return result;
}

/**
 * Get radio name
 */
void AccessPoint::getRadioName(int rfIndex, TCHAR *buffer, size_t bufSize)
{
	buffer[0] = 0;
	lockProperties();
	if (m_radioInterfaces != NULL)
	{
		for(int i = 0; i < m_radioInterfaces->size(); i++)
		{
			if (m_radioInterfaces->get(i)->index == rfIndex)
			{
				nx_strncpy(buffer, m_radioInterfaces->get(i)->name, bufSize);
				break;
			}
		}
	}
	unlockProperties();
}

/**
 * Get access point's parent node
 */
Node *AccessPoint::getParentNode()
{
   return (Node *)FindObjectById(m_nodeId, OBJECT_NODE);
}

/**
 * Update access point information
 */
void AccessPoint::updateInfo(const TCHAR *vendor, const TCHAR *model, const TCHAR *serialNumber)
{
	lockProperties();

	safe_free(m_vendor);
	m_vendor = (vendor != NULL) ? _tcsdup(vendor) : NULL;

	safe_free(m_model);
	m_model = (model != NULL) ? _tcsdup(model) : NULL;

	safe_free(m_serialNumber);
	m_serialNumber = (serialNumber != NULL) ? _tcsdup(serialNumber) : NULL;

	setModified();
	unlockProperties();
}

/**
 * Update access point state
 */
void AccessPoint::updateState(AccessPointState state)
{
   if (state == m_state)
      return;

	lockProperties();
   if (state == AP_DOWN)
      m_prevState = m_state;
   m_state = state;
   if (m_iStatus != STATUS_UNMANAGED)
   {
      m_iStatus = (state == AP_ADOPTED) ? STATUS_NORMAL : ((state == AP_UNADOPTED) ? STATUS_MAJOR : STATUS_CRITICAL);
   }
   setModified();
	unlockProperties();

   static const TCHAR *names[] = { _T("id"), _T("name"), _T("macAddr"), _T("ipAddr"), _T("vendor"), _T("model"), _T("serialNumber") };
   PostEventWithNames((state == AP_ADOPTED) ? EVENT_AP_ADOPTED : ((state == AP_UNADOPTED) ? EVENT_AP_UNADOPTED : EVENT_AP_DOWN), 
      m_nodeId, "ishasss", names,
      m_id, m_name, m_macAddr, m_dwIpAddr, 
      CHECK_NULL_EX(m_vendor), CHECK_NULL_EX(m_model), CHECK_NULL_EX(m_serialNumber));
}

/**
 * Do status poll
 */
void AccessPoint::statusPoll(ClientSession *session, UINT32 rqId, Queue *eventQueue, Node *controller)
{
   m_pollRequestor = session;
   AccessPointState state = m_state;

   sendPollerMsg(rqId, _T("   Starting status poll on access point %s\r\n"), m_name);
   sendPollerMsg(rqId, _T("      Current access point status is %s\r\n"), GetStatusAsText(m_iStatus, true));

   /* TODO: read status from controller via driver and use ping as last resort only */

   if (m_dwIpAddr != 0)
   {
		UINT32 icmpProxy = 0;

      if (IsZoningEnabled() && (controller->getZoneId() != 0))
		{
			Zone *zone = (Zone *)g_idxZoneByGUID.get(controller->getZoneId());
			if (zone != NULL)
			{
				icmpProxy = zone->getIcmpProxy();
			}
		}

		if (icmpProxy != 0)
		{
			sendPollerMsg(rqId, _T("      Starting ICMP ping via proxy\r\n"));
			DbgPrintf(7, _T("AccessPoint::StatusPoll(%d,%s): ping via proxy [%u]"), m_id, m_name, icmpProxy);
			Node *proxyNode = (Node *)g_idxNodeById.get(icmpProxy);
			if ((proxyNode != NULL) && proxyNode->isNativeAgent() && !proxyNode->isDown())
			{
				DbgPrintf(7, _T("AccessPoint::StatusPoll(%d,%s): proxy node found: %s"), m_id, m_name, proxyNode->getName());
				AgentConnection *conn = proxyNode->createAgentConnection();
				if (conn != NULL)
				{
					TCHAR parameter[64], buffer[64];

					_sntprintf(parameter, 64, _T("Icmp.Ping(%s)"), IpToStr(m_dwIpAddr, buffer));
					if (conn->getParameter(parameter, 64, buffer) == ERR_SUCCESS)
					{
						DbgPrintf(7, _T("AccessPoint::StatusPoll(%d,%s): proxy response: \"%s\""), m_id, m_name, buffer);
						TCHAR *eptr;
						long value = _tcstol(buffer, &eptr, 10);
						if ((*eptr == 0) && (value >= 0))
						{
							if (value < 10000)
                     {
            				sendPollerMsg(rqId, POLLER_ERROR _T("      responded to ICMP ping\r\n"));
                        if (state == AP_DOWN)
                           state = m_prevState;  /* FIXME: get actual AP state here */
                     }
                     else
							{
            				sendPollerMsg(rqId, POLLER_ERROR _T("      no response to ICMP ping\r\n"));
                        state = AP_DOWN;
							}
						}
					}
					conn->disconnect();
					delete conn;
				}
				else
				{
					DbgPrintf(7, _T("AccessPoint::StatusPoll(%d,%s): cannot connect to agent on proxy node"), m_id, m_name);
					sendPollerMsg(rqId, POLLER_ERROR _T("      Unable to establish connection with proxy node\r\n"));
				}
			}
			else
			{
				DbgPrintf(7, _T("AccessPoint::StatusPoll(%d,%s): proxy node not available"), m_id, m_name);
				sendPollerMsg(rqId, POLLER_ERROR _T("      ICMP proxy not available\r\n"));
			}
		}
		else	// not using ICMP proxy
		{
			sendPollerMsg(rqId, _T("      Starting ICMP ping\r\n"));
			DbgPrintf(7, _T("AccessPoint::StatusPoll(%d,%s): calling IcmpPing(0x%08X,3,%d,NULL,%d)"), m_id, m_name, htonl(m_dwIpAddr), g_icmpPingTimeout, g_icmpPingSize);
			UINT32 dwPingStatus = IcmpPing(htonl(m_dwIpAddr), 3, g_icmpPingTimeout, NULL, g_icmpPingSize);
			if (dwPingStatus == ICMP_SUCCESS)
         {
				sendPollerMsg(rqId, POLLER_ERROR _T("      responded to ICMP ping\r\n"));
            if (state == AP_DOWN)
               state = m_prevState;  /* FIXME: get actual AP state here */
         }
         else
			{
				sendPollerMsg(rqId, POLLER_ERROR _T("      no response to ICMP ping\r\n"));
            state = AP_DOWN;
			}
			DbgPrintf(7, _T("AccessPoint::StatusPoll(%d,%s): ping result %d, state=%d"), m_id, m_name, dwPingStatus, state);
		}
   }

   updateState(state);

   sendPollerMsg(rqId, _T("      Access point status after poll is %s\r\n"), GetStatusAsText(m_iStatus, true));
	sendPollerMsg(rqId, _T("   Finished status poll on access point %s\r\n"), m_name);
}
