/*
** NetXMS - Network Management System
** Copyright (C) 2003-2019 Victor Kirhenshtein
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
** File: alarm.cpp
**
**/

#include "nxcore.h"

#define DEBUG_TAG _T("alarm")

/**
 * Column list for loading alarms from database
 */
#define ALARM_LOAD_COLUMN_LIST \
   _T("alarm_id,source_object_id,zone_uin,") \
   _T("source_event_code,source_event_id,message,") \
   _T("original_severity,current_severity,") \
   _T("alarm_key,creation_time,last_change_time,") \
   _T("hd_state,hd_ref,ack_by,repeat_count,") \
   _T("alarm_state,timeout,timeout_event,resolved_by,") \
   _T("ack_timeout,dci_id,alarm_category_ids,") \
   _T("rule_guid,event_tags")

/**
 * Alarm list
 */
class AlarmList
{
private:
   Mutex m_lock;
   ObjectArray<Alarm> m_list;
   StringObjectMap<Alarm> m_keyIndex;

public:
   AlarmList() : m_list(256, 256, true), m_keyIndex(false) { }
   ~AlarmList() { }

   void lock() { m_lock.lock(); }
   void unlock() { m_lock.unlock(); }

   int size() { return m_list.size(); }

   Alarm *get(int index) { return m_list.get(index); }
   Alarm *get(const TCHAR *key) { return m_keyIndex.get(key); }

   void add(Alarm *alarm)
   {
      m_list.add(alarm);
      if (*alarm->getKey() != 0)
         m_keyIndex.set(alarm->getKey(), alarm);
   }

   void remove(int index)
   {
      Alarm *alarm = m_list.get(index);
      if (*alarm->getKey() != 0)
         m_keyIndex.remove(alarm->getKey());
      m_list.remove(index);
   }

   void remove(Alarm *alarm)
   {
      if (*alarm->getKey() != 0)
         m_keyIndex.remove(alarm->getKey());
      m_list.remove(alarm);
   }
};

/**
 * Global instance of alarm manager
 */
static AlarmList s_alarmList;
static Condition s_shutdown(true);
static THREAD s_watchdogThread = INVALID_THREAD_HANDLE;
static UINT32 s_resolveExpirationTime = 0;

/**
 * Get number of comments for alarm
 */
static UINT32 GetCommentCount(DB_HANDLE hdb, UINT32 alarmId)
{
   UINT32 value = 0;
   DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT count(*) FROM alarm_notes WHERE alarm_id=?"));
   if (hStmt != NULL)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, alarmId);
      DB_RESULT hResult = DBSelectPrepared(hStmt);
      if (hResult != NULL)
      {
         if (DBGetNumRows(hResult) > 0)
            value = DBGetFieldULong(hResult, 0, 0);
         DBFreeResult(hResult);
      }
      DBFreeStatement(hStmt);
   }
   return value;
}

/**
 * Create new alarm from event
 */
Alarm::Alarm(Event *event, const uuid& rule, const TCHAR *message, const TCHAR *key, int state, int severity, UINT32 timeout, UINT32 timeoutEvent, UINT32 ackTimeout, IntegerArray<UINT32> *alarmCategoryList)
{
   m_alarmId = CreateUniqueId(IDG_ALARM);
   m_sourceEventId = event->getId();
   m_sourceEventCode = event->getCode();
   m_eventTags = MemCopyString(event->getTagsAsList());
   m_rule = rule;
   m_sourceObject = event->getSourceId();
   m_zoneUIN = event->getZoneUIN();
   m_dciId = event->getDciId();
   m_creationTime = time(NULL);
   m_lastChangeTime = m_creationTime;
   m_state = state;
   m_originalSeverity = severity;
   m_currentSeverity = severity;
   m_repeatCount = 1;
   m_helpDeskState = ALARM_HELPDESK_IGNORED;
   m_helpDeskRef[0] = 0;
   m_timeout = timeout;
   m_timeoutEvent = timeoutEvent;
   m_commentCount = 0;
   m_ackTimeout = 0;
   m_ackByUser = 0;
   m_resolvedByUser = 0;
   m_termByUser = 0;
   m_relatedEvents = new IntegerArray<UINT64>(16, 16);
   m_relatedEvents->add(event->getId());
   _tcslcpy(m_message, message, MAX_EVENT_MSG_LENGTH);
   _tcslcpy(m_key, key, MAX_DB_STRING);
   m_alarmCategoryList = new IntegerArray<UINT32>(alarmCategoryList);
   m_notificationCode = 0;
}

/**
 * Create alarm object from database record
 */
Alarm::Alarm(DB_HANDLE hdb, DB_RESULT hResult, int row)
{
   m_alarmId = DBGetFieldULong(hResult, row, 0);
   m_sourceObject = DBGetFieldULong(hResult, row, 1);
   m_zoneUIN = DBGetFieldULong(hResult, row, 2);
   m_sourceEventCode = DBGetFieldULong(hResult, row, 3);
   m_sourceEventId = DBGetFieldUInt64(hResult, row, 4);
   DBGetField(hResult, row, 5, m_message, MAX_EVENT_MSG_LENGTH);
   m_originalSeverity = (BYTE)DBGetFieldLong(hResult, row, 6);
   m_currentSeverity = (BYTE)DBGetFieldLong(hResult, row, 7);
   DBGetField(hResult, row, 8, m_key, MAX_DB_STRING);
   m_creationTime = DBGetFieldULong(hResult, row, 9);
   m_lastChangeTime = DBGetFieldULong(hResult, row, 10);
   m_helpDeskState = (BYTE)DBGetFieldLong(hResult, row, 11);
   DBGetField(hResult, row, 12, m_helpDeskRef, MAX_HELPDESK_REF_LEN);
   m_ackByUser = DBGetFieldULong(hResult, row, 13);
   m_repeatCount = DBGetFieldULong(hResult, row, 14);
   m_state = (BYTE)DBGetFieldLong(hResult, row, 15);
   m_timeout = DBGetFieldULong(hResult, row, 16);
   m_timeoutEvent = DBGetFieldULong(hResult, row, 17);
   m_resolvedByUser = DBGetFieldULong(hResult, row, 18);
   m_ackTimeout = DBGetFieldULong(hResult, row, 19);
   m_dciId = DBGetFieldULong(hResult, row, 20);

   TCHAR categoryList[MAX_DB_STRING];
   DBGetField(hResult, row, 21, categoryList, MAX_DB_STRING);
   m_alarmCategoryList = new IntegerArray<UINT32>(16, 16);

   int count;
   TCHAR **ids = SplitString(categoryList, _T(','), &count);
   for(int i = 0; i < count; i++)
   {
      m_alarmCategoryList->add(_tcstoul(ids[i], NULL, 10));
      MemFree(ids[i]);
   }
   MemFree(ids);

   m_rule = DBGetFieldGUID(hResult, row, 22);
   m_eventTags = DBGetField(hResult, row, 23, NULL, 0);
   m_notificationCode = 0;

   m_commentCount = GetCommentCount(hdb, m_alarmId);

   m_termByUser = 0;
   m_relatedEvents = new IntegerArray<UINT64>(16, 16);

   TCHAR query[256];
   _sntprintf(query, 256, _T("SELECT event_id FROM alarm_events WHERE alarm_id=%d"), (int)m_alarmId);
   DB_RESULT eventResult = DBSelect(hdb, query);
   if (eventResult != NULL)
   {
      int count = DBGetNumRows(eventResult);
      for(int j = 0; j < count; j++)
      {
         m_relatedEvents->add(DBGetFieldUInt64(eventResult, j, 0));
      }
      DBFreeResult(eventResult);
   }
}

/**
 * Copy constructor
 */
Alarm::Alarm(const Alarm *src, bool copyEvents, UINT32 notificationCode)
{
   m_sourceEventId = src->m_sourceEventId;
   m_alarmId = src->m_alarmId;
   m_creationTime = src->m_creationTime;
   m_lastChangeTime = src->m_lastChangeTime;
   m_rule = src->m_rule;
   m_sourceObject = src->m_sourceObject;
   m_zoneUIN = src->m_zoneUIN;
   m_sourceEventCode = src->m_sourceEventCode;
   m_eventTags = MemCopyString(src->m_eventTags);
   m_dciId = src->m_dciId;
   m_currentSeverity = src->m_currentSeverity;
   m_originalSeverity = src->m_originalSeverity;
   m_state = src->m_state;
   m_helpDeskState = src->m_helpDeskState;
   m_ackByUser = src->m_ackByUser;
   m_resolvedByUser = src->m_resolvedByUser;
   m_termByUser = src->m_termByUser;
   m_ackTimeout = src->m_ackTimeout;
   m_repeatCount = src->m_repeatCount;
   m_timeout = src->m_timeout;
   m_timeoutEvent = src->m_timeoutEvent;
   _tcscpy(m_message, src->m_message);
   _tcscpy(m_key, src->m_key);
   _tcscpy(m_helpDeskRef, src->m_helpDeskRef);
   m_commentCount = src->m_commentCount;
   if (copyEvents && (src->m_relatedEvents != NULL))
   {
      m_relatedEvents = new IntegerArray<UINT64>(src->m_relatedEvents);
   }
   else
   {
      m_relatedEvents = NULL;
   }
   m_alarmCategoryList = new IntegerArray<UINT32>(src->m_alarmCategoryList);
   m_notificationCode = notificationCode;
}

/**
 * Alarm destructor
 */
Alarm::~Alarm()
{
   delete m_relatedEvents;
   delete m_alarmCategoryList;
   MemFree(m_eventTags);
}

/**
 * Convert alarm category list to string
 */
String Alarm::categoryListToString()
{
   String buffer;
   for(int i = 0; i < m_alarmCategoryList->size(); i++)
   {
      if (buffer.length() > 0)
         buffer.append(_T(','));
      buffer.append(m_alarmCategoryList->get(i));
   }
   return buffer;
}

/**
 * Check alarm category access
 */
bool Alarm::checkCategoryAccess(ClientSession *session) const
{
   if (session->checkSysAccessRights(SYSTEM_ACCESS_VIEW_ALL_ALARMS))
      return true;

   for(int i = 0; i < m_alarmCategoryList->size(); i++)
   {
      if (CheckAlarmCategoryAccess(session->getUserId(), m_alarmCategoryList->get(i)))
         return true;
   }
   return false;
}

/**
 * Client notification data
 */
struct CLIENT_NOTIFICATION_DATA
{
   UINT32 code;
   const Alarm *alarm;
};

/**
 * Callback for client session enumeration
 */
static void SendAlarmNotification(ClientSession *session, void *arg)
{
   session->onAlarmUpdate(((CLIENT_NOTIFICATION_DATA *)arg)->code,
                          ((CLIENT_NOTIFICATION_DATA *)arg)->alarm);
}

/**
 * Callback for client session enumeration
 */
static void SendBulkAlarmTerminateNotification(ClientSession *session, void *arg)
{
   session->sendMessage((NXCPMessage *)arg);
}

/**
 * Notify connected clients about changes
 */
static void NotifyClients(UINT32 code, const Alarm *alarm)
{
   CALL_ALL_MODULES(pfAlarmChangeHook, (code, alarm));

   CLIENT_NOTIFICATION_DATA data;
   data.code = code;
   data.alarm = alarm;
   EnumerateClientSessions(SendAlarmNotification, &data);
}

/**
 * Create alarm record in database
 */
void Alarm::createInDatabase()
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();

   DB_STATEMENT hStmt = DBPrepare(hdb,
              _T("INSERT INTO alarms (alarm_id,creation_time,last_change_time,")
              _T("source_object_id,zone_uin,source_event_code,message,original_severity,")
              _T("current_severity,alarm_key,alarm_state,ack_by,resolved_by,hd_state,")
              _T("hd_ref,repeat_count,term_by,timeout,timeout_event,source_event_id,")
              _T("ack_timeout,dci_id,alarm_category_ids,rule_guid,event_tags) ")
              _T("VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
   if (hStmt != NULL)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_alarmId);
      DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, (UINT32)m_creationTime);
      DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, (UINT32)m_lastChangeTime);
      DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, m_sourceObject);
      DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, m_zoneUIN);
      DBBind(hStmt, 6, DB_SQLTYPE_INTEGER, m_sourceEventCode);
      DBBind(hStmt, 7, DB_SQLTYPE_VARCHAR, m_message, DB_BIND_STATIC);
      DBBind(hStmt, 8, DB_SQLTYPE_INTEGER, (INT32)m_originalSeverity);
      DBBind(hStmt, 9, DB_SQLTYPE_INTEGER, (INT32)m_currentSeverity);
      DBBind(hStmt, 10, DB_SQLTYPE_VARCHAR, m_key, DB_BIND_STATIC);
      DBBind(hStmt, 11, DB_SQLTYPE_INTEGER, (INT32)m_state);
      DBBind(hStmt, 12, DB_SQLTYPE_INTEGER, m_ackByUser);
      DBBind(hStmt, 13, DB_SQLTYPE_INTEGER, m_resolvedByUser);
      DBBind(hStmt, 14, DB_SQLTYPE_INTEGER, (INT32)m_helpDeskState);
      DBBind(hStmt, 15, DB_SQLTYPE_VARCHAR, m_helpDeskRef, DB_BIND_STATIC);
      DBBind(hStmt, 16, DB_SQLTYPE_INTEGER, m_repeatCount);
      DBBind(hStmt, 17, DB_SQLTYPE_INTEGER, m_termByUser);
      DBBind(hStmt, 18, DB_SQLTYPE_INTEGER, m_timeout);
      DBBind(hStmt, 19, DB_SQLTYPE_INTEGER, m_timeoutEvent);
      DBBind(hStmt, 20, DB_SQLTYPE_BIGINT, m_sourceEventId);
      DBBind(hStmt, 21, DB_SQLTYPE_INTEGER, (UINT32)m_ackTimeout);
      DBBind(hStmt, 22, DB_SQLTYPE_INTEGER, m_dciId);
      DBBind(hStmt, 23, DB_SQLTYPE_VARCHAR, categoryListToString(), DB_BIND_TRANSIENT);
      if (!m_rule.isNull())
         DBBind(hStmt, 24, DB_SQLTYPE_VARCHAR, m_rule);
      else
         DBBind(hStmt, 24, DB_SQLTYPE_VARCHAR, _T(""), DB_BIND_STATIC);
      DBBind(hStmt, 25, DB_SQLTYPE_VARCHAR, m_eventTags, DB_BIND_STATIC);

      DBExecute(hStmt);
      DBFreeStatement(hStmt);
   }

   DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Update alarm information in database
 */
void Alarm::updateInDatabase()
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();

   DB_STATEMENT hStmt = DBPrepare(hdb,
            _T("UPDATE alarms SET alarm_state=?,ack_by=?,term_by=?,")
            _T("last_change_time=?,current_severity=?,repeat_count=?,")
            _T("hd_state=?,hd_ref=?,timeout=?,timeout_event=?,")
            _T("message=?,resolved_by=?,ack_timeout=?,source_object_id=?,")
            _T("dci_id=?,alarm_category_ids=?,rule_guid=?,event_tags=? WHERE alarm_id=?"));
   if (hStmt != NULL)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, (INT32)m_state);
      DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, m_ackByUser);
      DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, m_termByUser);
      DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, (UINT32)m_lastChangeTime);
      DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, (INT32)m_currentSeverity);
      DBBind(hStmt, 6, DB_SQLTYPE_INTEGER, m_repeatCount);
      DBBind(hStmt, 7, DB_SQLTYPE_INTEGER, (INT32)m_helpDeskState);
      DBBind(hStmt, 8, DB_SQLTYPE_VARCHAR, m_helpDeskRef, DB_BIND_STATIC);
      DBBind(hStmt, 9, DB_SQLTYPE_INTEGER, m_timeout);
      DBBind(hStmt, 10, DB_SQLTYPE_INTEGER, m_timeoutEvent);
      DBBind(hStmt, 11, DB_SQLTYPE_VARCHAR, m_message, DB_BIND_STATIC);
      DBBind(hStmt, 12, DB_SQLTYPE_INTEGER, m_resolvedByUser);
      DBBind(hStmt, 13, DB_SQLTYPE_INTEGER, (UINT32)m_ackTimeout);
      DBBind(hStmt, 14, DB_SQLTYPE_INTEGER, m_sourceObject);
      DBBind(hStmt, 15, DB_SQLTYPE_INTEGER, m_dciId);
      DBBind(hStmt, 16, DB_SQLTYPE_VARCHAR, categoryListToString(), DB_BIND_TRANSIENT);
      if (!m_rule.isNull())
         DBBind(hStmt, 17, DB_SQLTYPE_VARCHAR, m_rule);
      else
         DBBind(hStmt, 17, DB_SQLTYPE_VARCHAR, _T(""), DB_BIND_STATIC);
      DBBind(hStmt, 18, DB_SQLTYPE_VARCHAR, m_eventTags, DB_BIND_STATIC);
      DBBind(hStmt, 19, DB_SQLTYPE_INTEGER, m_alarmId);
      DBExecute(hStmt);
      DBFreeStatement(hStmt);
   }

	if (m_state == ALARM_STATE_TERMINATED)
	{
	   TCHAR query[256];
		_sntprintf(query, 256, _T("DELETE FROM alarm_events WHERE alarm_id=%d"), m_alarmId);
		QueueSQLRequest(query);

		DeleteAlarmNotes(hdb, m_alarmId);
	}
   DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Fill NXCP message with alarm data
 */
void Alarm::fillMessage(NXCPMessage *msg) const
{
   msg->setField(VID_ALARM_ID, m_alarmId);
   msg->setField(VID_ACK_BY_USER, m_ackByUser);
   msg->setField(VID_RESOLVED_BY_USER, m_resolvedByUser);
   msg->setField(VID_TERMINATED_BY_USER, m_termByUser);
   msg->setField(VID_RULE_ID, m_rule);
   msg->setField(VID_EVENT_CODE, m_sourceEventCode);
   msg->setField(VID_EVENT_ID, m_sourceEventId);
   msg->setField(VID_TAGS, m_eventTags);
   msg->setField(VID_OBJECT_ID, m_sourceObject);
   msg->setField(VID_DCI_ID, m_dciId);
   msg->setFieldFromTime(VID_CREATION_TIME, m_creationTime);
   msg->setFieldFromTime(VID_LAST_CHANGE_TIME, m_lastChangeTime);
   msg->setField(VID_ALARM_KEY, m_key);
   msg->setField(VID_ALARM_MESSAGE, m_message);
   msg->setField(VID_STATE, (WORD)(m_state & ALARM_STATE_MASK)); // send only state to client, without flags
   msg->setField(VID_IS_STICKY, (WORD)((m_state & ALARM_STATE_STICKY) ? 1 : 0));
   msg->setField(VID_CURRENT_SEVERITY, (WORD)m_currentSeverity);
   msg->setField(VID_ORIGINAL_SEVERITY, (WORD)m_originalSeverity);
   msg->setField(VID_HELPDESK_STATE, (WORD)m_helpDeskState);
   msg->setField(VID_HELPDESK_REF, m_helpDeskRef);
   msg->setField(VID_REPEAT_COUNT, m_repeatCount);
   msg->setField(VID_ALARM_TIMEOUT, m_timeout);
   msg->setField(VID_ALARM_TIMEOUT_EVENT, m_timeoutEvent);
   msg->setField(VID_NUM_COMMENTS, m_commentCount);
   msg->setField(VID_TIMESTAMP, (UINT32)((m_ackTimeout != 0) ? (m_ackTimeout - time(NULL)) : 0));
   msg->setFieldFromInt32Array(VID_CATEGORY_LIST, m_alarmCategoryList);
   if (m_notificationCode != 0)
      msg->setField(VID_NOTIFICATION_CODE, m_notificationCode);
}

/**
 * Update object status after alarm acknowledgment or deletion
 */
static void UpdateObjectStatus(UINT32 objectId)
{
   NetObj *object = FindObjectById(objectId);
   if (object != NULL)
      object->calculateCompoundStatus();
}

/**
 * Fill NXCP message with event data from SQL query
 * Expected field order: event_id,event_code,event_name,severity,source_object_id,event_timestamp,message
 */
static void FillEventData(NXCPMessage *msg, UINT32 baseId, DB_RESULT hResult, int row, QWORD rootId)
{
	TCHAR buffer[MAX_EVENT_MSG_LENGTH];

	msg->setField(baseId, DBGetFieldUInt64(hResult, row, 0));
	msg->setField(baseId + 1, rootId);
	msg->setField(baseId + 2, DBGetFieldULong(hResult, row, 1));
	msg->setField(baseId + 3, DBGetField(hResult, row, 2, buffer, MAX_DB_STRING));
	msg->setField(baseId + 4, (WORD)DBGetFieldLong(hResult, row, 3));	// severity
	msg->setField(baseId + 5, DBGetFieldULong(hResult, row, 4));  // source object
	msg->setField(baseId + 6, DBGetFieldULong(hResult, row, 5));  // timestamp
	msg->setField(baseId + 7, DBGetField(hResult, row, 6, buffer, MAX_EVENT_MSG_LENGTH));
}

/**
 * Get events correlated to given event into NXCP message
 *
 * @return number of consumed variable identifiers
 */
static UINT32 GetCorrelatedEvents(QWORD eventId, NXCPMessage *msg, UINT32 baseId, DB_HANDLE hdb)
{
	UINT32 varId = baseId;
	DB_STATEMENT hStmt = DBPrepare(hdb,
		(g_dbSyntax == DB_SYNTAX_ORACLE) ?
			_T("SELECT e.event_id,e.event_code,c.event_name,e.event_severity,e.event_source,e.event_timestamp,e.event_message ")
			_T("FROM event_log e,event_cfg c WHERE zero_to_null(e.root_event_id)=? AND c.event_code=e.event_code")
		:
			_T("SELECT e.event_id,e.event_code,c.event_name,e.event_severity,e.event_source,e.event_timestamp,e.event_message ")
			_T("FROM event_log e,event_cfg c WHERE e.root_event_id=? AND c.event_code=e.event_code"));
	if (hStmt != NULL)
	{
		DBBind(hStmt, 1, DB_SQLTYPE_BIGINT, eventId);
		DB_RESULT hResult = DBSelectPrepared(hStmt);
		if (hResult != NULL)
		{
			int count = DBGetNumRows(hResult);
			for(int i = 0; i < count; i++)
			{
				FillEventData(msg, varId, hResult, i, eventId);
				varId += 10;
				QWORD eventId = DBGetFieldUInt64(hResult, i, 0);
				varId += GetCorrelatedEvents(eventId, msg, varId, hdb);
			}
			DBFreeResult(hResult);
		}
		DBFreeStatement(hStmt);
	}
	return varId - baseId;
}

/**
 * Fill NXCP message with alarm's related events
 */
static void FillAlarmEventsMessage(NXCPMessage *msg, UINT32 alarmId)
{
	DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
	const TCHAR *query;
	switch(g_dbSyntax)
	{
		case DB_SYNTAX_ORACLE:
			query = _T("SELECT * FROM (SELECT event_id,event_code,event_name,severity,source_object_id,event_timestamp,message FROM alarm_events WHERE alarm_id=? ORDER BY event_timestamp DESC) WHERE ROWNUM<=200");
			break;
		case DB_SYNTAX_MSSQL:
			query = _T("SELECT TOP 200 event_id,event_code,event_name,severity,source_object_id,event_timestamp,message FROM alarm_events WHERE alarm_id=? ORDER BY event_timestamp DESC");
			break;
		case DB_SYNTAX_DB2:
			query = _T("SELECT event_id,event_code,event_name,severity,source_object_id,event_timestamp,message ")
			   _T("FROM alarm_events WHERE alarm_id=? ORDER BY event_timestamp DESC FETCH FIRST 200 ROWS ONLY");
			break;
		default:
			query = _T("SELECT event_id,event_code,event_name,severity,source_object_id,event_timestamp,message FROM alarm_events WHERE alarm_id=? ORDER BY event_timestamp DESC LIMIT 200");
			break;
	}
	DB_STATEMENT hStmt = DBPrepare(hdb, query);
	if (hStmt != NULL)
	{
		DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, alarmId);
		DB_RESULT hResult = DBSelectPrepared(hStmt);
		if (hResult != NULL)
		{
			int count = DBGetNumRows(hResult);
			UINT32 varId = VID_ELEMENT_LIST_BASE;
			for(int i = 0; i < count; i++)
			{
				FillEventData(msg, varId, hResult, i, 0);
				varId += 10;
				QWORD eventId = DBGetFieldUInt64(hResult, i, 0);
				varId += GetCorrelatedEvents(eventId, msg, varId, hdb);
			}
			DBFreeResult(hResult);
			msg->setField(VID_NUM_ELEMENTS, (varId - VID_ELEMENT_LIST_BASE) / 10);
		}
		DBFreeStatement(hStmt);
	}
	DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Update existing alarm from event
 */
void Alarm::updateFromEvent(Event *event, int state, int severity, UINT32 timeout, UINT32 timeoutEvent, UINT32 ackTimeout, const TCHAR *message, IntegerArray<UINT32> *alarmCategoryList)
{
   m_repeatCount++;
   m_lastChangeTime = (UINT32)time(NULL);
   m_sourceObject = event->getSourceId();
   m_dciId = event->getDciId();
   if ((m_state & ALARM_STATE_STICKY) == 0)
      m_state = state;
   m_currentSeverity = severity;
   m_timeout = timeout;
   m_timeoutEvent = timeoutEvent;
   if ((m_state & ALARM_STATE_STICKY) == 0)
      m_ackTimeout = ackTimeout;
   _tcslcpy(m_message, message, MAX_EVENT_MSG_LENGTH);
   delete m_alarmCategoryList;
   m_alarmCategoryList = new IntegerArray<UINT32>(alarmCategoryList);

   NotifyClients(NX_NOTIFY_ALARM_CHANGED, this);
   updateInDatabase();
}

/**
 * Create new alarm
 */
UINT32 NXCORE_EXPORTABLE CreateNewAlarm(const uuid& rule, TCHAR *message, TCHAR *key, int state, int severity, UINT32 timeout,
         UINT32 timeoutEvent, Event *event, UINT32 ackTimeout, IntegerArray<UINT32> *alarmCategoryList, bool openHelpdeskIssue)
{
   UINT32 alarmId = 0;
   bool newAlarm = true;
   bool updateRelatedEvent = false;

   // Expand alarm's message and key
   String expMsg = event->expandText(message);
   String expKey = event->expandText(key);

   // Check if we have a duplicate alarm
   if (((state & ALARM_STATE_MASK) != ALARM_STATE_TERMINATED) && !expKey.isEmpty())
   {
      s_alarmList.lock();

      Alarm *alarm = s_alarmList.get(expKey);
      if (alarm != NULL)
      {
         alarm->updateFromEvent(event, state, severity, timeout, timeoutEvent, ackTimeout, expMsg, alarmCategoryList);
         if (!alarm->isEventRelated(event->getId()))
         {
            alarmId = alarm->getAlarmId();      // needed for correct update of related events
            updateRelatedEvent = true;
            alarm->addRelatedEvent(event->getId());
         }

         if (openHelpdeskIssue)
            alarm->openHelpdeskIssue(NULL);

         newAlarm = false;
      }

      s_alarmList.unlock();
   }

   if (newAlarm)
   {
      // Create new alarm structure
      Alarm *alarm = new Alarm(event, rule, expMsg, expKey, state, severity, timeout, timeoutEvent, ackTimeout, alarmCategoryList);
      alarmId = alarm->getAlarmId();

      // Open helpdesk issue
      if (openHelpdeskIssue)
         alarm->openHelpdeskIssue(NULL);

      // Add new alarm to active alarm list if needed
		if ((alarm->getState() & ALARM_STATE_MASK) != ALARM_STATE_TERMINATED)
      {
         s_alarmList.lock();
         nxlog_debug_tag(DEBUG_TAG, 7, _T("AlarmManager: adding new active alarm, current alarm count %d"), s_alarmList.size());
         s_alarmList.add(alarm);
         s_alarmList.unlock();
      }

		alarm->createInDatabase();
      updateRelatedEvent = true;

      // Notify connected clients about new alarm
      NotifyClients(NX_NOTIFY_NEW_ALARM, alarm);
   }

   // Update status of related object if needed
   if ((state & ALARM_STATE_MASK) != ALARM_STATE_TERMINATED)
		UpdateObjectStatus(event->getSourceId());

   if (updateRelatedEvent)
   {
      // Add record to alarm_events table
      TCHAR valAlarmId[16], valEventId[32], valEventCode[16], valSeverity[16], valSource[16], valTimestamp[16];
      const TCHAR *values[8] = { valAlarmId, valEventId, valEventCode, event->getName(), valSeverity, valSource, valTimestamp, event->getMessage() };
      _sntprintf(valAlarmId, 16, _T("%d"), (int)alarmId);
      _sntprintf(valEventId, 32, UINT64_FMT, event->getId());
      _sntprintf(valEventCode, 16, _T("%d"), (int)event->getCode());
      _sntprintf(valSeverity, 16, _T("%d"), (int)event->getSeverity());
      _sntprintf(valSource, 16, _T("%d"), event->getSourceId());
      _sntprintf(valTimestamp, 16, _T("%u"), (UINT32)event->getTimestamp());
      static int sqlTypes[8] = { DB_SQLTYPE_INTEGER, DB_SQLTYPE_BIGINT, DB_SQLTYPE_INTEGER, DB_SQLTYPE_VARCHAR, DB_SQLTYPE_INTEGER, DB_SQLTYPE_INTEGER, DB_SQLTYPE_INTEGER, DB_SQLTYPE_VARCHAR };
      QueueSQLRequest(_T("INSERT INTO alarm_events (alarm_id,event_id,event_code,event_name,severity,source_object_id,event_timestamp,message) VALUES (?,?,?,?,?,?,?,?)"),
                      8, sqlTypes, values);
   }

   return alarmId;
}

/**
 * Do acknowledge
 */
UINT32 Alarm::acknowledge(ClientSession *session, bool sticky, UINT32 acknowledgmentActionTime)
{
   if ((m_state & ALARM_STATE_MASK) != ALARM_STATE_OUTSTANDING)
      return RCC_ALARM_NOT_OUTSTANDING;

   if (session != NULL)
   {
      WriteAuditLog(AUDIT_OBJECTS, TRUE, session->getUserId(), session->getWorkstation(), session->getId(), m_sourceObject,
         _T("Acknowledged alarm %d (%s) on object %s"), m_alarmId, m_message,
         GetObjectName(m_sourceObject, _T("")));
   }

   UINT32 endTime = acknowledgmentActionTime != 0 ? (UINT32)time(NULL) + acknowledgmentActionTime : 0;
   m_ackTimeout = endTime;
   m_state = ALARM_STATE_ACKNOWLEDGED;
	if (sticky)
      m_state |= ALARM_STATE_STICKY;
   m_ackByUser = (session != NULL) ? session->getUserId() : 0;
   m_lastChangeTime = (UINT32)time(NULL);
   NotifyClients(NX_NOTIFY_ALARM_CHANGED, this);
   updateInDatabase();
   return RCC_SUCCESS;
}

/**
 * Acknowledge alarm with given ID
 */
UINT32 NXCORE_EXPORTABLE AckAlarmById(UINT32 alarmId, ClientSession *session, bool sticky, UINT32 acknowledgmentActionTime)
{
   UINT32 dwObject, dwRet = RCC_INVALID_ALARM_ID;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (alarm->getAlarmId() == alarmId)
      {
         dwRet = alarm->acknowledge(session, sticky, acknowledgmentActionTime);
         dwObject = alarm->getSourceObject();
         break;
      }
   }
   s_alarmList.unlock();

   if (dwRet == RCC_SUCCESS)
      UpdateObjectStatus(dwObject);
   return dwRet;
}

/**
 * Acknowledge alarm with given helpdesk reference
 */
UINT32 NXCORE_EXPORTABLE AckAlarmByHDRef(const TCHAR *hdref, ClientSession *session, bool sticky, UINT32 acknowledgmentActionTime)
{
   UINT32 dwObject, dwRet = RCC_INVALID_ALARM_ID;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (!_tcscmp(alarm->getHelpDeskRef(), hdref))
      {
         dwRet = alarm->acknowledge(session, sticky, acknowledgmentActionTime);
         dwObject = alarm->getSourceObject();
         break;
      }
   }
   s_alarmList.unlock();

   if (dwRet == RCC_SUCCESS)
      UpdateObjectStatus(dwObject);
   return dwRet;
}

/**
 * Resolve alarm
 */
void Alarm::resolve(UINT32 userId, Event *event, bool terminate, bool notify)
{
   if (terminate)
      m_termByUser = userId;
   else
      m_resolvedByUser = userId;
   m_lastChangeTime = (UINT32)time(NULL);
   m_state = terminate ? ALARM_STATE_TERMINATED : ALARM_STATE_RESOLVED;
   m_ackTimeout = 0;
   if (m_helpDeskState != ALARM_HELPDESK_IGNORED)
      m_helpDeskState = ALARM_HELPDESK_CLOSED;
   if (notify)
      NotifyClients(terminate ? NX_NOTIFY_ALARM_TERMINATED : NX_NOTIFY_ALARM_CHANGED, this);
   updateInDatabase();

   if (!terminate && (event != NULL) && (m_relatedEvents != NULL) && !m_relatedEvents->contains(event->getId()))
   {
      // Add record to alarm_events table if alarm is resolved
      m_relatedEvents->add(event->getId());

      TCHAR valAlarmId[16], valEventId[32], valEventCode[16], valSeverity[16], valSource[16], valTimestamp[16];
      const TCHAR *values[8] = { valAlarmId, valEventId, valEventCode, event->getName(), valSeverity, valSource, valTimestamp, event->getMessage() };
      _sntprintf(valAlarmId, 16, _T("%d"), (int)m_alarmId);
      _sntprintf(valEventId, 32, UINT64_FMT, event->getId());
      _sntprintf(valEventCode, 16, _T("%d"), (int)event->getCode());
      _sntprintf(valSeverity, 16, _T("%d"), (int)event->getSeverity());
      _sntprintf(valSource, 16, _T("%d"), event->getSourceId());
      _sntprintf(valTimestamp, 16, _T("%u"), (UINT32)event->getTimestamp());
      static int sqlTypes[8] = { DB_SQLTYPE_INTEGER, DB_SQLTYPE_BIGINT, DB_SQLTYPE_INTEGER, DB_SQLTYPE_VARCHAR, DB_SQLTYPE_INTEGER, DB_SQLTYPE_INTEGER, DB_SQLTYPE_INTEGER, DB_SQLTYPE_VARCHAR };
      QueueSQLRequest(_T("INSERT INTO alarm_events (alarm_id,event_id,event_code,event_name,severity,source_object_id,event_timestamp,message) VALUES (?,?,?,?,?,?,?,?)"),
                      8, sqlTypes, values);
   }
}

/**
 * Resolve and possibly terminate alarm with given ID
 * Should return RCC which can be sent to client
 */
UINT32 NXCORE_EXPORTABLE ResolveAlarmById(UINT32 alarmId, ClientSession *session, bool terminate)
{
   IntegerArray<UINT32> list(1), failIds, failCodes;
   list.add(alarmId);
   ResolveAlarmsById(&list, &failIds, &failCodes, session, terminate);

   if (failCodes.size() > 0)
   {
      return failCodes.get(0);
   }
   else
   {
      return RCC_SUCCESS;
   }
}

/**
 * Resolve and possibly terminate alarms with given ID
 * Should return RCC which can be sent to client
 */
void NXCORE_EXPORTABLE ResolveAlarmsById(IntegerArray<UINT32> *alarmIds, IntegerArray<UINT32> *failIds, IntegerArray<UINT32> *failCodes, ClientSession *session, bool terminate)
{
   IntegerArray<UINT32> processedAlarms, updatedObjects;

   s_alarmList.lock();
   time_t changeTime = time(NULL);
   for(int i = 0; i < alarmIds->size(); i++)
   {
      int n;
      for(n = 0; n < s_alarmList.size(); n++)
      {
         Alarm *alarm = s_alarmList.get(n);
         if (alarm->getAlarmId() == alarmIds->get(i))
         {
            // If alarm is open in helpdesk, it cannot be terminated
            if ((alarm->getHelpDeskState() != ALARM_HELPDESK_OPEN) || ConfigReadBoolean(_T("Alarms.IgnoreHelpdeskState"), false))
            {
               if (terminate || (alarm->getState() != ALARM_STATE_RESOLVED))
               {
                  NetObj *object = GetAlarmSourceObject(alarmIds->get(i), true);
                  if (session != NULL)
                  {
                     // If user does not have the required object access rights, the alarm cannot be terminated
                     if (!object->checkAccessRights(session->getUserId(), terminate ? OBJECT_ACCESS_TERM_ALARMS : OBJECT_ACCESS_UPDATE_ALARMS))
                     {
                        failIds->add(alarmIds->get(i));
                        failCodes->add(RCC_ACCESS_DENIED);
                        break;
                     }

                     WriteAuditLog(AUDIT_OBJECTS, TRUE, session->getUserId(), session->getWorkstation(), session->getId(), object->getId(),
                        _T("%s alarm %d (%s) on object %s"), terminate ? _T("Terminated") : _T("Resolved"),
                        alarm->getAlarmId(), alarm->getMessage(), object->getName());
                  }

                  alarm->resolve((session != NULL) ? session->getUserId() : 0, NULL, terminate, false);
                  processedAlarms.add(alarm->getAlarmId());
                  if (!updatedObjects.contains(object->getId()))
                     updatedObjects.add(object->getId());
                  if (terminate)
                  {
                     s_alarmList.remove(n);
                     n--;
                  }
               }
               else
               {
                  // Alarm is already resolved, just mark it as processed
                  processedAlarms.add(alarm->getAlarmId());
               }
            }
            else
            {
               failIds->add(alarmIds->get(i));
               failCodes->add(RCC_ALARM_OPEN_IN_HELPDESK);
            }
            break;
         }
      }
      if (n == s_alarmList.size())
      {
         failIds->add(alarmIds->get(i));
         failCodes->add(RCC_INVALID_ALARM_ID);
      }
   }
   s_alarmList.unlock();

   NXCPMessage notification;
   notification.setCode(CMD_BULK_ALARM_STATE_CHANGE);
   notification.setField(VID_NOTIFICATION_CODE, terminate ? NX_NOTIFY_MULTIPLE_ALARMS_TERMINATED : NX_NOTIFY_MULTIPLE_ALARMS_RESOLVED);
   notification.setField(VID_USER_ID, (session != NULL) ? session->getUserId() : 0);
   notification.setFieldFromTime(VID_LAST_CHANGE_TIME, changeTime);
   notification.setFieldFromInt32Array(VID_ALARM_ID_LIST, &processedAlarms);
   EnumerateClientSessions(SendBulkAlarmTerminateNotification, &notification);

   for(int i = 0; i < updatedObjects.size(); i++)
      UpdateObjectStatus(updatedObjects.get(i));
}

/**
 * Resolve and possibly terminate all alarms with given key
 */
void NXCORE_EXPORTABLE ResolveAlarmByKey(const TCHAR *pszKey, bool useRegexp, bool terminate, Event *pEvent)
{
   if (useRegexp)
   {
      IntegerArray<UINT32> objectList;
      s_alarmList.lock();
      for(int i = 0; i < s_alarmList.size(); i++)
      {
         Alarm *alarm = s_alarmList.get(i);
         if (RegexpMatch(alarm->getKey(), pszKey, true) &&
             ((alarm->getHelpDeskState() != ALARM_HELPDESK_OPEN) || ConfigReadBoolean(_T("Alarms.IgnoreHelpdeskState"), false)) &&
             (terminate || (alarm->getState() != ALARM_STATE_RESOLVED)))
         {
            // Add alarm's source object to update list
            if (!objectList.contains(alarm->getSourceObject()))
               objectList.add(alarm->getSourceObject());

            // Resolve or terminate alarm
            alarm->resolve(0, pEvent, terminate, true);
            if (terminate)
            {
               s_alarmList.remove(i);
               i--;
            }
         }
      }
      s_alarmList.unlock();

      // Update status of objects
      for(int i = 0; i < objectList.size(); i++)
         UpdateObjectStatus(objectList.get(i));
   }
   else
   {
      UINT32 objectId = 0;
      s_alarmList.lock();
      Alarm *alarm = s_alarmList.get(pszKey);
      if ((alarm != NULL) &&
          ((alarm->getHelpDeskState() != ALARM_HELPDESK_OPEN) || ConfigReadBoolean(_T("Alarms.IgnoreHelpdeskState"), false)) &&
          (terminate || (alarm->getState() != ALARM_STATE_RESOLVED)))
      {
         // Add alarm's source object to update list
         objectId = alarm->getSourceObject();

         // Resolve or terminate alarm
         alarm->resolve(0, pEvent, terminate, true);
         if (terminate)
         {
            s_alarmList.remove(alarm);
         }
      }
      s_alarmList.unlock();

      if (objectId != 0)
         UpdateObjectStatus(objectId);
   }
}

/**
 * Resolve and possibly terminate all alarms related to given data collection object
 */
void NXCORE_EXPORTABLE ResolveAlarmByDCObjectId(UINT32 dciId, bool terminate)
{
   IntegerArray<UINT32> objectList;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if ((alarm->getDciId() == dciId) &&
          ((alarm->getHelpDeskState() != ALARM_HELPDESK_OPEN) || ConfigReadBoolean(_T("Alarms.IgnoreHelpdeskState"), false)) &&
          (terminate || (alarm->getState() != ALARM_STATE_RESOLVED)))
      {
         // Add alarm's source object to update list
         if (!objectList.contains(alarm->getSourceObject()))
            objectList.add(alarm->getSourceObject());

         // Resolve or terminate alarm
         alarm->resolve(0, NULL, terminate, true);
         if (terminate)
         {
            s_alarmList.remove(i);
            i--;
         }
      }
   }
   s_alarmList.unlock();

   // Update status of objects
   for (int i = 0; i < objectList.size(); i++)
      UpdateObjectStatus(objectList.get(i));
}

/**
 * Resolve and possibly terminate alarm with given helpdesk reference.
 * Automatically change alarm's helpdesk state to "closed"
 */
UINT32 NXCORE_EXPORTABLE ResolveAlarmByHDRef(const TCHAR *hdref, ClientSession *session, bool terminate)
{
   UINT32 objectId = 0;
   UINT32 rcc = RCC_INVALID_ALARM_ID;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (!_tcscmp(alarm->getHelpDeskRef(), hdref))
      {
         if (terminate || (alarm->getState() != ALARM_STATE_RESOLVED))
         {
            objectId = alarm->getSourceObject();
            if (session != NULL)
            {
               WriteAuditLog(AUDIT_OBJECTS, TRUE, session->getUserId(), session->getWorkstation(), session->getId(), objectId,
                  _T("%s alarm %d (%s) on object %s"), terminate ? _T("Terminated") : _T("Resolved"),
                  alarm->getAlarmId(), alarm->getMessage(), GetObjectName(objectId, _T("")));
            }

            alarm->resolve((session != NULL) ? session->getUserId() : 0, NULL, terminate, true);
            if (terminate)
            {
               s_alarmList.remove(i);
            }
            nxlog_debug_tag(DEBUG_TAG, 5, _T("Alarm with helpdesk reference \"%s\" %s"), hdref, terminate ? _T("terminated") : _T("resolved"));
         }
         else
         {
            nxlog_debug_tag(DEBUG_TAG, 5, _T("Alarm with helpdesk reference \"%s\" already resolved"), hdref);
         }
         rcc = RCC_SUCCESS;
         break;
      }
   }
   s_alarmList.unlock();

   if (objectId != 0)
      UpdateObjectStatus(objectId);
   return rcc;
}

/**
 * Resolve alarm by helpdesk reference
 */
UINT32 NXCORE_EXPORTABLE ResolveAlarmByHDRef(const TCHAR *hdref)
{
   return ResolveAlarmByHDRef(hdref, NULL, false);
}

/**
 * Terminate alarm by helpdesk reference
 */
UINT32 NXCORE_EXPORTABLE TerminateAlarmByHDRef(const TCHAR *hdref)
{
   return ResolveAlarmByHDRef(hdref, NULL, true);
}

/**
 * Open issue in helpdesk system
 */
UINT32 Alarm::openHelpdeskIssue(TCHAR *hdref)
{
   UINT32 rcc;

   if (m_helpDeskState == ALARM_HELPDESK_IGNORED)
   {
      /* TODO: unlock alarm list before call */
      const TCHAR *nodeName = GetObjectName(m_sourceObject, _T("[unknown]"));
      size_t messageLen = _tcslen(nodeName) + _tcslen(m_message) + 32;
      TCHAR *message = MemAllocArrayNoInit<TCHAR>(messageLen);
      _sntprintf(message, messageLen, _T("%s: %s"), nodeName, m_message);
      rcc = CreateHelpdeskIssue(message, m_helpDeskRef);
      MemFree(message);
      if (rcc == RCC_SUCCESS)
      {
         m_helpDeskState = ALARM_HELPDESK_OPEN;
         NotifyClients(NX_NOTIFY_ALARM_CHANGED, this);
         updateInDatabase();
         if (hdref != NULL)
            _tcslcpy(hdref, m_helpDeskRef, MAX_HELPDESK_REF_LEN);
         nxlog_debug_tag(DEBUG_TAG, 5, _T("Helpdesk issue created for alarm %d, reference \"%s\""), m_alarmId, m_helpDeskRef);
      }
   }
   else
   {
      rcc = RCC_OUT_OF_STATE_REQUEST;
   }
   return rcc;
}

/**
 * Open issue in helpdesk system
 */
UINT32 OpenHelpdeskIssue(UINT32 alarmId, ClientSession *session, TCHAR *hdref)
{
   UINT32 rcc = RCC_INVALID_ALARM_ID;
   *hdref = 0;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (alarm->getAlarmId() == alarmId)
      {
         if (alarm->checkCategoryAccess(session))
            rcc = alarm->openHelpdeskIssue(hdref);
         else
            rcc = RCC_ACCESS_DENIED;
         break;
      }
   }
   s_alarmList.unlock();
   return rcc;
}

/**
 * Get helpdesk issue URL for given alarm
 */
UINT32 GetHelpdeskIssueUrlFromAlarm(UINT32 alarmId, UINT32 userId, TCHAR *url, size_t size, ClientSession *session)
{
   UINT32 rcc = RCC_INVALID_ALARM_ID;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      if (s_alarmList.get(i)->getAlarmId() == alarmId)
      {
         if (s_alarmList.get(i)->checkCategoryAccess(session))
         {
            if ((s_alarmList.get(i)->getHelpDeskState() != ALARM_HELPDESK_IGNORED) && (s_alarmList.get(i)->getHelpDeskRef()[0] != 0))
            {
               rcc = GetHelpdeskIssueUrl(s_alarmList.get(i)->getHelpDeskRef(), url, size);
            }
            else
            {
               rcc = RCC_OUT_OF_STATE_REQUEST;
            }
         }
         else
         {
            rcc = RCC_ACCESS_DENIED;
         }
         break;
      }
   }
   s_alarmList.unlock();
   return rcc;
}

/**
 * Unlink helpdesk issue from alarm
 */
UINT32 UnlinkHelpdeskIssueById(UINT32 alarmId, ClientSession *session)
{
   UINT32 rcc = RCC_INVALID_ALARM_ID;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (alarm->getAlarmId() == alarmId)
      {
         if (session != NULL)
         {
            WriteAuditLog(AUDIT_OBJECTS, TRUE, session->getUserId(), session->getWorkstation(), session->getId(),
               alarm->getSourceObject(), _T("Helpdesk issue %s unlinked from alarm %d (%s) on object %s"),
               alarm->getHelpDeskRef(), alarm->getAlarmId(), alarm->getMessage(),
               GetObjectName(alarm->getSourceObject(), _T("")));
         }
         alarm->unlinkFromHelpdesk();
			NotifyClients(NX_NOTIFY_ALARM_CHANGED, alarm);
			alarm->updateInDatabase();
         rcc = RCC_SUCCESS;
         break;
      }
   }
   s_alarmList.unlock();

   return rcc;
}

/**
 * Unlink helpdesk issue from alarm
 */
UINT32 UnlinkHelpdeskIssueByHDRef(const TCHAR *hdref, ClientSession *session)
{
   UINT32 rcc = RCC_INVALID_ALARM_ID;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (!_tcscmp(alarm->getHelpDeskRef(), hdref))
      {
         if (session != NULL)
         {
            WriteAuditLog(AUDIT_OBJECTS, TRUE, session->getUserId(), session->getWorkstation(), session->getId(),
               alarm->getSourceObject(), _T("Helpdesk issue %s unlinked from alarm %d (%s) on object %s"),
               alarm->getHelpDeskRef(), alarm->getAlarmId(), alarm->getMessage(),
               GetObjectName(alarm->getSourceObject(), _T("")));
         }
         alarm->unlinkFromHelpdesk();
			NotifyClients(NX_NOTIFY_ALARM_CHANGED, alarm);
			alarm->updateInDatabase();
         rcc = RCC_SUCCESS;
         break;
      }
   }
   s_alarmList.unlock();

   return rcc;
}

/**
 * Delete alarm with given ID
 */
void NXCORE_EXPORTABLE DeleteAlarm(UINT32 alarmId, bool objectCleanup)
{
   DWORD dwObject;
   bool found = false;

   // Delete alarm from in-memory list
   if (!objectCleanup)  // otherwise already locked
      s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (alarm->getAlarmId() == alarmId)
      {
         dwObject = alarm->getSourceObject();
         NotifyClients(NX_NOTIFY_ALARM_DELETED, alarm);
         s_alarmList.remove(i);
         found = true;
         break;
      }
   }
   if (!objectCleanup)
      s_alarmList.unlock();

   // Delete from database
   if (found && !objectCleanup)
   {
      TCHAR szQuery[256];

      _sntprintf(szQuery, 256, _T("DELETE FROM alarms WHERE alarm_id=%d"), (int)alarmId);
      QueueSQLRequest(szQuery);
      _sntprintf(szQuery, 256, _T("DELETE FROM alarm_events WHERE alarm_id=%d"), (int)alarmId);
      QueueSQLRequest(szQuery);

      DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
      DeleteAlarmNotes(hdb, alarmId);
      DBConnectionPoolReleaseConnection(hdb);

      UpdateObjectStatus(dwObject);
   }
}

/**
 * Delete all alarms of given object. Intended to be called only
 * on final stage of object deletion.
 */
bool DeleteObjectAlarms(UINT32 objectId, DB_HANDLE hdb)
{
	s_alarmList.lock();

	// go through from end because s_alarmList.size() is decremented by DeleteAlarm()
	for(int i = s_alarmList.size() - 1; i >= 0; i--)
   {
      Alarm *alarm = s_alarmList.get(i);
		if (alarm->getSourceObject() == objectId)
      {
			DeleteAlarm(alarm->getAlarmId(), true);
      }
	}

	s_alarmList.unlock();

   // Delete all object alarms from database
   bool success = false;
	DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT alarm_id FROM alarms WHERE source_object_id=?"));
	if (hStmt != NULL)
	{
		DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, objectId);
		DB_RESULT hResult = DBSelectPrepared(hStmt);
		if (hResult != NULL)
		{
         success = true;
			int count = DBGetNumRows(hResult);
			for(int i = 0; i < count; i++)
         {
            UINT32 alarmId = DBGetFieldULong(hResult, i, 0);
				DeleteAlarmNotes(hdb, alarmId);
            DeleteAlarmEvents(hdb, alarmId);
         }
			DBFreeResult(hResult);
		}
		DBFreeStatement(hStmt);
	}

   if (success)
   {
	   hStmt = DBPrepare(hdb, _T("DELETE FROM alarms WHERE source_object_id=?"));
	   if (hStmt != NULL)
	   {
		   DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, objectId);
         success = DBExecute(hStmt) ? true : false;
         DBFreeStatement(hStmt);
      }
   }
   return success;
}

/**
 * Send all alarms to client
 */
void SendAlarmsToClient(UINT32 dwRqId, ClientSession *pSession)
{
   DWORD dwUserId = pSession->getUserId();

   // Prepare message
   NXCPMessage msg;
   msg.setCode(CMD_ALARM_DATA);
   msg.setId(dwRqId);

   ObjectArray<Alarm> *alarms = GetAlarms();
   for(int i = 0; i < alarms->size(); i++)
   {
      Alarm *alarm = alarms->get(i);
      NetObj *object = FindObjectById(alarm->getSourceObject());
      if ((object != NULL) &&
          object->checkAccessRights(dwUserId, OBJECT_ACCESS_READ_ALARMS) &&
          alarm->checkCategoryAccess(pSession))
      {
         alarm->fillMessage(&msg);
         pSession->sendMessage(&msg);
         msg.deleteAllFields();
      }
   }
   delete alarms;

   // Send end-of-list indicator
   msg.setField(VID_ALARM_ID, (UINT32)0);
   pSession->sendMessage(&msg);
}

/**
 * Get alarm with given ID into NXCP message
 * Should return RCC that can be sent to client
 */
UINT32 NXCORE_EXPORTABLE GetAlarm(UINT32 alarmId, UINT32 userId, NXCPMessage *msg, ClientSession *session)
{
   UINT32 rcc = RCC_INVALID_ALARM_ID;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (alarm->getAlarmId() == alarmId)
      {
         if (alarm->checkCategoryAccess(session))
         {
            alarm->fillMessage(msg);
            rcc = RCC_SUCCESS;
         }
         else
         {
            rcc = RCC_ACCESS_DENIED;
         }
         break;
      }
   }
   s_alarmList.unlock();

   return rcc;
}

/**
 * Get all related events for alarm with given ID into NXCP message
 * Should return RCC that can be sent to client
 */
UINT32 NXCORE_EXPORTABLE GetAlarmEvents(UINT32 alarmId, UINT32 userId, NXCPMessage *msg, ClientSession *session)
{
   UINT32 dwRet = RCC_INVALID_ALARM_ID;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      if (s_alarmList.get(i)->getAlarmId() == alarmId)
      {
         if (s_alarmList.get(i)->checkCategoryAccess(session))
         {
            dwRet = RCC_SUCCESS;
         }
         else
         {
            dwRet = RCC_ACCESS_DENIED;
         }
         break;
      }
   }

   s_alarmList.unlock();

	// we don't call FillAlarmEventsMessage from within loop
	// to prevent alarm list lock for a long time
	if (dwRet == RCC_SUCCESS)
		FillAlarmEventsMessage(msg, alarmId);

	return dwRet;
}

/**
 * Get source object for given alarm id
 */
NetObj NXCORE_EXPORTABLE *GetAlarmSourceObject(UINT32 alarmId, bool alreadyLocked)
{
   UINT32 dwObjectId = 0;

   if (!alreadyLocked)
      s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (alarm->getAlarmId() == alarmId)
      {
         dwObjectId = alarm->getSourceObject();
         break;
      }
   }

   if (!alreadyLocked)
      s_alarmList.unlock();
   return (dwObjectId != 0) ? FindObjectById(dwObjectId) : NULL;
}

/**
 * Get source object for given alarm helpdesk reference
 */
NetObj NXCORE_EXPORTABLE *GetAlarmSourceObject(const TCHAR *hdref)
{
   UINT32 dwObjectId = 0;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (!_tcscmp(alarm->getHelpDeskRef(), hdref))
      {
         dwObjectId = alarm->getSourceObject();
         break;
      }
   }
   s_alarmList.unlock();
   return (dwObjectId != 0) ? FindObjectById(dwObjectId) : NULL;
}

/**
 * Get most critical status among active alarms for given object
 * Will return STATUS_UNKNOWN if there are no active alarms
 */
int GetMostCriticalStatusForObject(UINT32 dwObjectId)
{
   int status = STATUS_UNKNOWN;

   s_alarmList.lock();
   for(int i = 0; (i < s_alarmList.size()) && (status != STATUS_CRITICAL); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if ((alarm->getSourceObject() == dwObjectId) &&
			 ((alarm->getState() & ALARM_STATE_MASK) < ALARM_STATE_RESOLVED) &&
          ((alarm->getCurrentSeverity() > status) || (status == STATUS_UNKNOWN)))
      {
         status = (int)alarm->getCurrentSeverity();
      }
   }
   s_alarmList.unlock();
   return status;
}

/**
 * Fill message with alarm stats
 */
void GetAlarmStats(NXCPMessage *pMsg)
{
   UINT32 dwCount[5];

   s_alarmList.lock();
   pMsg->setField(VID_NUM_ALARMS, s_alarmList.size());
   memset(dwCount, 0, sizeof(UINT32) * 5);
   for(int i = 0; i < s_alarmList.size(); i++)
      dwCount[s_alarmList.get(i)->getCurrentSeverity()]++;
   s_alarmList.unlock();
   pMsg->setFieldFromInt32Array(VID_ALARMS_BY_SEVERITY, 5, dwCount);
}

/**
 * Get number of active alarms
 */
int GetAlarmCount()
{
   s_alarmList.lock();
   int count = s_alarmList.size();
   s_alarmList.unlock();
   return count;
}

/**
 * Watchdog thread
 */
static THREAD_RESULT THREAD_CALL WatchdogThread(void *arg)
{
   ThreadSetName("AlarmWatchdog");

	while(true)
	{
		if (s_shutdown.wait(1000))
			break;

		if (!(g_flags & AF_SERVER_INITIALIZED))
		   continue;   // Server not initialized yet

		s_alarmList.lock();
		time_t now = time(NULL);
	   for(int i = 0; i < s_alarmList.size(); i++)
		{
         Alarm *alarm = s_alarmList.get(i);
			if ((alarm->getTimeout() > 0) &&
				 ((alarm->getState() & ALARM_STATE_MASK) == ALARM_STATE_OUTSTANDING) &&
				 (((time_t)alarm->getLastChangeTime() + (time_t)alarm->getTimeout()) < now))
			{
            nxlog_debug_tag(DEBUG_TAG, 5, _T("Outstanding timeout: alarm_id=%u, last_change=%u, timeout=%u, now=%u"),
                     alarm->getAlarmId(), alarm->getLastChangeTime(), alarm->getTimeout(), (UINT32)now);

				TCHAR eventName[MAX_EVENT_NAME];
				if (!EventNameFromCode(alarm->getSourceEventCode(), eventName))
				{
				   _sntprintf(eventName, MAX_EVENT_NAME, _T("[%u]"), alarm->getSourceEventCode());
				}
				PostSystemEvent(alarm->getTimeoutEvent(), alarm->getSourceObject(), "dssds",
				          alarm->getAlarmId(), alarm->getMessage(), alarm->getKey(), alarm->getSourceEventCode(), eventName);
				alarm->clearTimeout();	// Disable repeated timeout events
				alarm->updateInDatabase();
			}

			if ((alarm->getAckTimeout() != 0) &&
				 ((alarm->getState() & ALARM_STATE_STICKY) != 0) &&
				 (((time_t)alarm->getAckTimeout() <= now)))
			{
			   nxlog_debug_tag(DEBUG_TAG, 5, _T("Acknowledgment timeout: alarm_id=%u, timeout=%u, now=%u"),
			            alarm->getAlarmId(), alarm->getAckTimeout(), (UINT32)now);

				PostSystemEvent(alarm->getTimeoutEvent(), alarm->getSourceObject(), "dssd",
				         alarm->getAlarmId(), alarm->getMessage(), alarm->getKey(), alarm->getSourceEventCode());
				alarm->onAckTimeoutExpiration();
				alarm->updateInDatabase();
				NotifyClients(NX_NOTIFY_ALARM_CHANGED, alarm);
			}

			if ((s_resolveExpirationTime > 0) &&
			    ((alarm->getState() & ALARM_STATE_MASK) == ALARM_STATE_RESOLVED) &&
			    (alarm->getLastChangeTime() + s_resolveExpirationTime <= now) &&
			    (alarm->getHelpDeskState() != ALARM_HELPDESK_OPEN))
			{
            nxlog_debug_tag(DEBUG_TAG, 5, _T("Resolve timeout: alarm_id=%u, last_change=%u, timeout=%u, now=%u"),
                     alarm->getAlarmId(), alarm->getLastChangeTime(), s_resolveExpirationTime, (UINT32)now);
            alarm->resolve(0, NULL, true, true);
            s_alarmList.remove(i);
            i--;
			}
		}
		s_alarmList.unlock();
	}
   return THREAD_OK;
}

/**
 * Check if given alarm/note id pair is valid
 */
static bool IsValidNoteId(UINT32 alarmId, UINT32 noteId)
{
	bool isValid = false;
	DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
	DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT note_id FROM alarm_notes WHERE alarm_id=? AND note_id=?"));
	if (hStmt != NULL)
	{
		DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, alarmId);
		DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, noteId);
		DB_RESULT hResult = DBSelectPrepared(hStmt);
		if (hResult != NULL)
		{
			isValid = (DBGetNumRows(hResult) > 0);
			DBFreeResult(hResult);
		}
		DBFreeStatement(hStmt);
	}
	DBConnectionPoolReleaseConnection(hdb);
	return isValid;
}

/**
 * Update alarm's comment
 * Will update commentId to correct id if new comment is created
 */
UINT32 Alarm::updateAlarmComment(UINT32 *commentId, const TCHAR *text, UINT32 userId, bool syncWithHelpdesk)
{
   bool newNote = false;
   UINT32 rcc;

	if (*commentId != 0)
	{
      if (IsValidNoteId(m_alarmId, *commentId))
		{
			DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
			DB_STATEMENT hStmt = DBPrepare(hdb, _T("UPDATE alarm_notes SET change_time=?,user_id=?,note_text=? WHERE note_id=?"));
			if (hStmt != NULL)
			{
				DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, (UINT32)time(NULL));
				DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, userId);
				DBBind(hStmt, 3, DB_SQLTYPE_TEXT, text, DB_BIND_STATIC);
				DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, *commentId);
				rcc = DBExecute(hStmt) ? RCC_SUCCESS : RCC_DB_FAILURE;
				DBFreeStatement(hStmt);
			}
			else
			{
				rcc = RCC_DB_FAILURE;
			}
			DBConnectionPoolReleaseConnection(hdb);
		}
		else
		{
			rcc = RCC_INVALID_ALARM_NOTE_ID;
		}
	}
	else
	{
		// new note
		newNote = true;
		*commentId = CreateUniqueId(IDG_ALARM_NOTE);
		DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
		DB_STATEMENT hStmt = DBPrepare(hdb, _T("INSERT INTO alarm_notes (note_id,alarm_id,change_time,user_id,note_text) VALUES (?,?,?,?,?)"));
		if (hStmt != NULL)
		{
			DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, *commentId);
         DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, m_alarmId);
			DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, (UINT32)time(NULL));
			DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, userId);
			DBBind(hStmt, 5, DB_SQLTYPE_TEXT, text, DB_BIND_STATIC);
			rcc = DBExecute(hStmt) ? RCC_SUCCESS : RCC_DB_FAILURE;
			DBFreeStatement(hStmt);
		}
		else
		{
			rcc = RCC_DB_FAILURE;
		}
		DBConnectionPoolReleaseConnection(hdb);
	}
	if (rcc == RCC_SUCCESS)
	{
      if (newNote)
         m_commentCount++;
		NotifyClients(NX_NOTIFY_ALARM_CHANGED, this);
      if (syncWithHelpdesk && (m_helpDeskState == ALARM_HELPDESK_OPEN))
      {
         AddHelpdeskIssueComment(m_helpDeskRef, text);
      }
	}

   return rcc;
}

/**
 * Add alarm's comment by helpdesk reference
 */
UINT32 AddAlarmComment(const TCHAR *hdref, const TCHAR *text, UINT32 userId)
{
   UINT32 rcc = RCC_INVALID_ALARM_ID;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (!_tcscmp(alarm->getHelpDeskRef(), hdref))
      {
         UINT32 id = 0;
         rcc = alarm->updateAlarmComment(&id, text, userId, false);
         break;
      }
   }
   s_alarmList.unlock();

   return rcc;
}

/**
 * Update alarm's comment
 */
UINT32 UpdateAlarmComment(UINT32 alarmId, UINT32 *noteId, const TCHAR *text, UINT32 userId, bool syncWithHelpdesk)
{
   UINT32 rcc = RCC_INVALID_ALARM_ID;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (alarm->getAlarmId() == alarmId)
      {
         rcc = alarm->updateAlarmComment(noteId, text, userId, syncWithHelpdesk);
         break;
      }
   }
   s_alarmList.unlock();

   return rcc;
}

/**
 * Delete comment
 */
UINT32 Alarm::deleteComment(UINT32 commentId)
{
   UINT32 rcc;
   if (IsValidNoteId(m_alarmId, commentId))
   {
      DB_HANDLE db = DBConnectionPoolAcquireConnection();
      DB_STATEMENT stmt = DBPrepare(db, _T("DELETE FROM alarm_notes WHERE note_id=?"));
      if (stmt != NULL)
      {
         DBBind(stmt, 1, DB_SQLTYPE_INTEGER, commentId);
         rcc = DBExecute(stmt) ? RCC_SUCCESS : RCC_DB_FAILURE;
         DBFreeStatement(stmt);
      }
      else
      {
         rcc = RCC_DB_FAILURE;
      }
      DBConnectionPoolReleaseConnection(db);
   }
   else
   {
      rcc = RCC_INVALID_ALARM_NOTE_ID;
   }
   if (rcc == RCC_SUCCESS)
   {
      m_commentCount--;
      NotifyClients(NX_NOTIFY_ALARM_CHANGED, this);
   }
   return rcc;
}

/**
 * Delete comment
 */
UINT32 DeleteAlarmCommentByID(UINT32 alarmId, UINT32 noteId)
{
   UINT32 rcc = RCC_INVALID_ALARM_ID;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if (alarm->getAlarmId() == alarmId)
      {
         rcc = alarm->deleteComment(noteId);
         break;
      }
   }
   s_alarmList.unlock();

   return rcc;
}

/**
 * Get alarm's comments
 * Will return object array that is not the owner of objects
 */
ObjectArray<AlarmComment> *GetAlarmComments(UINT32 alarmId)
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   ObjectArray<AlarmComment> *comments = new ObjectArray<AlarmComment>(7, 7, false);

   DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT note_id,change_time,user_id,note_text FROM alarm_notes WHERE alarm_id=?"));
   if (hStmt != NULL)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, alarmId);
      DB_RESULT hResult = DBSelectPrepared(hStmt);
      if (hResult != NULL)
      {
         int count = DBGetNumRows(hResult);

         for(int i = 0; i < count; i++)
         {
            comments->add(new AlarmComment(DBGetFieldULong(hResult, i, 0), DBGetFieldULong(hResult, i, 1),
                  DBGetFieldULong(hResult, i, 2), DBGetField(hResult, i, 3, NULL, 0)));
         }
         DBFreeResult(hResult);
      }
      DBFreeStatement(hStmt);
   }

   DBConnectionPoolReleaseConnection(hdb);
   return comments;
}

/**
 * Get alarm's comments
 */
UINT32 GetAlarmComments(UINT32 alarmId, NXCPMessage *msg)
{
	DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
	UINT32 rcc = RCC_DB_FAILURE;

	DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT note_id,change_time,user_id,note_text FROM alarm_notes WHERE alarm_id=?"));
	if (hStmt != NULL)
	{
		DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, alarmId);
		DB_RESULT hResult = DBSelectPrepared(hStmt);
		if (hResult != NULL)
		{
			int count = DBGetNumRows(hResult);
			msg->setField(VID_NUM_ELEMENTS, (UINT32)count);

			UINT32 varId = VID_ELEMENT_LIST_BASE;
			for(int i = 0; i < count; i++)
			{
				msg->setField(varId++, DBGetFieldULong(hResult, i, 0));
				msg->setField(varId++, alarmId);
				msg->setField(varId++, DBGetFieldULong(hResult, i, 1));
            UINT32 userId = DBGetFieldULong(hResult, i, 2);
				msg->setField(varId++, userId);

            TCHAR *text = DBGetField(hResult, i, 3, NULL, 0);
				msg->setField(varId++, CHECK_NULL_EX(text));
				MemFree(text);

            TCHAR userName[MAX_USER_NAME];
            if (ResolveUserId(userId, userName) != NULL)
            {
   				msg->setField(varId++, userName);
            }
            else
            {
               varId++;
            }

            varId += 4;
			}
			DBFreeResult(hResult);
			rcc = RCC_SUCCESS;
		}
		DBFreeStatement(hStmt);
	}

	DBConnectionPoolReleaseConnection(hdb);
	return rcc;
}

/**
 * Get alarms for given object. If objectId set to 0, all alarms will be returned.
 * This method returns copies of alarm objects, so changing them will not
 * affect alarm states. Returned array must be destroyed by the caller.
 *
 * @param objectId object ID or 0 to get all alarms
 * @param recursive if true, will return alarms for child objects
 * @return array of active alarms for given object
 */
ObjectArray<Alarm> NXCORE_EXPORTABLE *GetAlarms(UINT32 objectId, bool recursive)
{
   s_alarmList.lock();
   ObjectArray<Alarm> *result = new ObjectArray<Alarm>(s_alarmList.size(), 16, true);
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *alarm = s_alarmList.get(i);
      if ((objectId == 0) || (alarm->getSourceObject() == objectId) ||
          (recursive && IsParentObject(objectId, alarm->getSourceObject())))
      {
         result->add(new Alarm(alarm, true));
      }
   }
   s_alarmList.unlock();
   return result;
}

/**
 * NXSL extension: Find alarm by ID
 */
int F_FindAlarmById(int argc, NXSL_Value **argv, NXSL_Value **result, NXSL_VM *vm)
{
   if (!argv[0]->isInteger())
      return NXSL_ERR_NOT_INTEGER;

   UINT32 alarmId = argv[0]->getValueAsUInt32();
   Alarm *alarm = FindAlarmById(alarmId);
   *result = (alarm != NULL) ? vm->createValue(new NXSL_Object(vm, &g_nxslAlarmClass, alarm)) : vm->createValue();
   return 0;
}

/**
 * NXSL extension: Find alarm by key
 */
int F_FindAlarmByKey(int argc, NXSL_Value **argv, NXSL_Value **result, NXSL_VM *vm)
{
   if (!argv[0]->isString())
      return NXSL_ERR_NOT_STRING;

   const TCHAR *key = argv[0]->getValueAsCString();
   Alarm *alarm = NULL;

   s_alarmList.lock();
   Alarm *a = s_alarmList.get(key);
   if (a != NULL)
   {
      alarm = new Alarm(a, false);
   }
   s_alarmList.unlock();

   *result = (alarm != NULL) ? vm->createValue(new NXSL_Object(vm, &g_nxslAlarmClass, alarm)) : vm->createValue();
   return 0;
}

/**
 * NXSL extension: Find alarm by key using regular expression
 */
int F_FindAlarmByKeyRegex(int argc, NXSL_Value **argv, NXSL_Value **result, NXSL_VM *vm)
{
   if (!argv[0]->isString())
      return NXSL_ERR_NOT_STRING;

   const TCHAR *key = argv[0]->getValueAsCString();
   Alarm *alarm = NULL;

   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      Alarm *a = s_alarmList.get(i);
      if (RegexpMatch(a->getKey(), key, TRUE))
      {
         alarm = new Alarm(a, false);
         break;
      }
   }
   s_alarmList.unlock();

   *result = (alarm != NULL) ? vm->createValue(new NXSL_Object(vm, &g_nxslAlarmClass, alarm)) : vm->createValue();
   return 0;
}

/**
 * Get alarm by id
 */
Alarm NXCORE_EXPORTABLE *FindAlarmById(UINT32 alarmId)
{
   if (alarmId == 0)
      return NULL;

   Alarm *alarm = NULL;
   s_alarmList.lock();
   for(int i = 0; i < s_alarmList.size(); i++)
   {
      if (s_alarmList.get(i)->getAlarmId() == alarmId)
      {
         alarm = new Alarm(s_alarmList.get(i), false);
         break;
      }
   }
   s_alarmList.unlock();
   return alarm;
}

/**
 * Load alarm from database
 */
Alarm NXCORE_EXPORTABLE *LoadAlarmFromDatabase(UINT32 alarmId)
{
   if (alarmId == 0)
      return NULL;

   Alarm *alarm = NULL;
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT ") ALARM_LOAD_COLUMN_LIST _T(" FROM alarms WHERE alarm_id=?"));
   if (hStmt != NULL)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, alarmId);
      DB_RESULT hResult = DBSelectPrepared(hStmt);
      if (hResult != NULL)
      {
         if (DBGetNumRows(hResult) > 0)
         {
            alarm = new Alarm(hdb, hResult, 0);
         }
         DBFreeResult(hResult);
      }
      DBFreeStatement(hStmt);
   }
   DBConnectionPoolReleaseConnection(hdb);
   return alarm;
}

/**
 * Update alarm expiration times
 */
void UpdateAlarmExpirationTimes()
{
   s_resolveExpirationTime = ConfigReadInt(_T("Alarms.ResolveExpirationTime"), 0);
   nxlog_debug_tag(DEBUG_TAG, 3, _T("Resolved alarms expiration time set to %u seconds"), s_resolveExpirationTime);
}

/**
 * Initialize alarm manager at system startup
 */
bool InitAlarmManager()
{
   s_watchdogThread = INVALID_THREAD_HANDLE;
   s_resolveExpirationTime = ConfigReadInt(_T("Alarms.ResolveExpirationTime"), 0);

   // Load active alarms into memory
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   DB_RESULT hResult = DBSelect(hdb, _T("SELECT ") ALARM_LOAD_COLUMN_LIST _T(" FROM alarms WHERE alarm_state<>3"));
   if (hResult == NULL)
   {
      DBConnectionPoolReleaseConnection(hdb);
      return false;
   }

   DB_HANDLE cachedb = (g_flags & AF_CACHE_DB_ON_STARTUP) ? DBOpenInMemoryDatabase() : NULL;
   if (cachedb != NULL)
   {
      nxlog_debug_tag(DEBUG_TAG, 2, _T("Caching alarm data tables"));
      if (!DBCacheTable(cachedb, hdb, _T("alarm_events"), _T("alarm_id,event_id"), _T("*")) ||
          !DBCacheTable(cachedb, hdb, _T("alarm_notes"), _T("note_id"), _T("note_id,alarm_id")))
      {
         DBCloseInMemoryDatabase(cachedb);
         cachedb = NULL;
      }
   }

   int count = DBGetNumRows(hResult);
   if (count > 0)
   {
      for(int i = 0; i < count; i++)
      {
         s_alarmList.add(new Alarm((cachedb != NULL) ? cachedb : hdb, hResult, i));
      }
   }

   DBFreeResult(hResult);
   DBConnectionPoolReleaseConnection(hdb);

   if (cachedb != NULL)
      DBCloseInMemoryDatabase(cachedb);

   s_watchdogThread = ThreadCreateEx(WatchdogThread, 0, NULL);
   return true;
}

/**
 * Alarm manager destructor
 */
void ShutdownAlarmManager()
{
   s_shutdown.set();
   ThreadJoin(s_watchdogThread);
}

/**
 * Alarm comments constructor
 */
AlarmComment::AlarmComment(UINT32 id, time_t changeTime, UINT32 userId, TCHAR *text)
{
   m_id = id;
   m_changeTime = changeTime;
   m_userId = userId;
   m_text = text;
}
