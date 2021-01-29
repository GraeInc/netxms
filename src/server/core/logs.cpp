/*
** NetXMS - Network Management System
** Copyright (C) 2003-2020 Victor Kirhenshtein
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
** File: logs.cpp
**
**/

#include "nxcore.h"
#include <nxcore_logs.h>

/**
 * Defined logs
 */
static NXCORE_LOG s_logs[] =
{
	{ _T("AlarmLog"), _T("alarms"), _T("alarm_id"), _T("source_object_id"), SYSTEM_ACCESS_VIEW_EVENT_LOG,
		{
			{ _T("alarm_id"), _T("Alarm ID"), LC_INTEGER, 0 },
			{ _T("alarm_state"), _T("State"), LC_ALARM_STATE, 0 },
			{ _T("hd_state"), _T("Helpdesk State"), LC_ALARM_HD_STATE, 0 },
			{ _T("source_object_id"), _T("Source"), LC_OBJECT_ID, 0 },
         { _T("zone_uin"), _T("Zone"), LC_ZONE_UIN, 0 },
         { _T("dci_id"), _T("DCI"), LC_INTEGER, 0 },
			{ _T("current_severity"), _T("Severity"), LC_SEVERITY, 0 },
			{ _T("original_severity"), _T("Original Severity"), LC_SEVERITY, 0 },
			{ _T("source_event_code"), _T("Event"), LC_EVENT_CODE, 0 },
			{ _T("message"), _T("Message"), LC_TEXT, 0 },
			{ _T("repeat_count"), _T("Repeat Count"), LC_INTEGER, 0 },
			{ _T("creation_time"), _T("Created"), LC_TIMESTAMP, 0 },
			{ _T("last_change_time"), _T("Last Changed"), LC_TIMESTAMP, 0 },
			{ _T("ack_by"), _T("Ack by"), LC_USER_ID, 0 },
			{ _T("resolved_by"), _T("Resolved by"), LC_USER_ID, 0 },
			{ _T("term_by"), _T("Terminated by"), LC_USER_ID, 0 },
         { _T("rule_guid"), _T("Rule"), LC_TEXT, 0 },
			{ _T("alarm_key"), _T("Key"), LC_TEXT, 0 },
         { _T("event_tags"), _T("Event Tags"), LC_TEXT, 0 },
			{ nullptr, nullptr, 0, 0 }
		}
	},
	{ _T("AuditLog"), _T("audit_log"), _T("record_id"), _T("object_id"), SYSTEM_ACCESS_VIEW_AUDIT_LOG,
		{
			{ _T("record_id"), _T("Record ID"), LC_INTEGER, 0 },
			{ _T("timestamp"), _T("Timestamp"), LC_TIMESTAMP, 0 },
			{ _T("subsystem"), _T("Subsystem"), LC_TEXT, 0 },
         { _T("object_id"), _T("Object"), LC_OBJECT_ID, 0 },
			{ _T("user_id"), _T("User"), LC_USER_ID, 0 },
         { _T("session_id"), _T("Session"), LC_INTEGER, 0 },
			{ _T("workstation"), _T("Workstation"), LC_TEXT, 0 },
			{ _T("message"), _T("Message"), LC_TEXT, 0 },
         { _T("old_value"), _T("Old value"), LC_TEXT_DETAILS, 0 },
         { _T("new_value"), _T("New value"), LC_TEXT_DETAILS, 0 },
         { _T("value_type"), _T("Value type"), LC_TEXT_DETAILS, 0 },
         { _T("hmac"), _T("HMAC"), LC_TEXT_DETAILS, 0 },
			{ nullptr, nullptr, 0, 0 }
		}
	},
	{ _T("EventLog"), _T("event_log"), _T("event_id"), _T("event_source"), SYSTEM_ACCESS_VIEW_EVENT_LOG,
		{
         { _T("event_id"), _T("ID"), LC_INTEGER, 0 },
			{ _T("event_timestamp"), _T("Time"), LC_TIMESTAMP, LCF_TSDB_TIMESTAMPTZ },
         { _T("origin_timestamp"), _T("Origin time"), LC_TIMESTAMP, 0 },
         { _T("origin"), _T("Origin"), LC_EVENT_ORIGIN, 0 },
			{ _T("event_source"), _T("Source"), LC_OBJECT_ID, 0 },
         { _T("zone_uin"), _T("Zone"), LC_ZONE_UIN, 0 },
         { _T("dci_id"), _T("DCI"), LC_INTEGER, 0 },
			{ _T("event_code"), _T("Event"), LC_EVENT_CODE, 0 },
			{ _T("event_severity"), _T("Severity"), LC_SEVERITY, 0 },
			{ _T("event_message"), _T("Message"), LC_TEXT, 0 },
			{ _T("event_tags"), _T("Event tags"), LC_TEXT, 0 },
         { _T("root_event_id"), _T("Root ID"), LC_INTEGER, 0 },
         { _T("raw_data"), _T("Raw data"), LC_JSON_DETAILS, 0 },
			{ nullptr, nullptr, 0, 0 }
		}
	},
	{ _T("SnmpTrapLog"), _T("snmp_trap_log"), _T("trap_id"), _T("object_id"), SYSTEM_ACCESS_VIEW_TRAP_LOG,
		{
			{ _T("trap_timestamp"), _T("Time"), LC_TIMESTAMP, LCF_TSDB_TIMESTAMPTZ },
			{ _T("ip_addr"), _T("Source IP"), LC_TEXT, 0 },
			{ _T("object_id"), _T("Object"), LC_OBJECT_ID, 0 },
         { _T("zone_uin"), _T("Zone"), LC_ZONE_UIN, 0 },
			{ _T("trap_oid"), _T("Trap OID"), LC_TEXT, 0 },
			{ _T("trap_varlist"), _T("Varbinds"), LC_TEXT, 0 },
			{ nullptr, nullptr, 0, 0 }
		}
	},
	{ _T("syslog"), _T("syslog"), _T("msg_id"), _T("source_object_id"), SYSTEM_ACCESS_VIEW_SYSLOG,
		{
			{ _T("msg_timestamp"), _T("Time"), LC_TIMESTAMP, LCF_TSDB_TIMESTAMPTZ },
			{ _T("source_object_id"), _T("Source"), LC_OBJECT_ID, 0 },
         { _T("zone_uin"), _T("Zone"), LC_ZONE_UIN, 0 },
			{ _T("facility"), _T("Facility"), LC_INTEGER, 0 },
			{ _T("severity"), _T("Severity"), LC_INTEGER, 0 },
			{ _T("hostname"), _T("Host"), LC_TEXT, 0 },
			{ _T("msg_tag"), _T("Tag"), LC_TEXT, 0 },
			{ _T("msg_text"), _T("Text"), LC_TEXT, 0 },
         { nullptr, nullptr, 0, 0 }
		}
	},
   { _T("WindowsEventLog"), _T("win_event_log"), _T("id"), _T("node_id"), SYSTEM_ACCESS_VIEW_SYSLOG,
      {
         { _T("id"), _T("ID"), LC_INTEGER, 0 },
         { _T("event_timestamp"), _T("Time"), LC_TIMESTAMP, LCF_TSDB_TIMESTAMPTZ },
         { _T("origin_timestamp"), _T("Origin time"), LC_TIMESTAMP, 0 },
         { _T("node_id"), _T("Source"), LC_OBJECT_ID, 0 },
         { _T("zone_uin"), _T("Zone"), LC_ZONE_UIN, 0 },
         { _T("log_name"), _T("Log name"), LC_TEXT, 0 },
         { _T("event_source"), _T("Event source"), LC_TEXT, 0 },
         { _T("event_severity"), _T("Event severity"), LC_INTEGER, 0 },
         { _T("event_code"), _T("Event code"), LC_INTEGER, 0 },
         { _T("message"), _T("Message"), LC_TEXT, 0 },
         { _T("raw_data"), _T("Raw data"), LC_TEXT_DETAILS, 0 },
         { nullptr, nullptr, 0, 0 }
      }
   },
	{ nullptr, nullptr, nullptr, nullptr, 0 }
};

/**
 * Registered log handles
 */
struct LOG_HANDLE_REGISTRATION
{
	LogHandle *handle;
	session_id_t sessionId;
};
int s_regListSize = 0;
LOG_HANDLE_REGISTRATION *s_regList = nullptr;
MUTEX s_regListMutex = INVALID_MUTEX_HANDLE;

/**
 * Init log access
 */
void InitLogAccess()
{
	s_regListMutex = MutexCreate();
}

/**
 * Register log handle
 */
static int32_t RegisterLogHandle(LogHandle *handle, ClientSession *session)
{
	int i;

	MutexLock(s_regListMutex);

	for(i = 0; i < s_regListSize; i++)
		if (s_regList[i].handle == nullptr)
			break;
	if (i == s_regListSize)
	{
		s_regListSize += 10;
		s_regList = MemReallocArray(s_regList, s_regListSize);
      memset(&s_regList[i], 0, sizeof(LOG_HANDLE_REGISTRATION) * (s_regListSize - i));
	}

	s_regList[i].handle = handle;
	s_regList[i].sessionId = session->getId();

	MutexUnlock(s_regListMutex);
   DbgPrintf(6, _T("RegisterLogHandle: handle object %p registered as %d"), handle, i);
	return i;
}

/**
 * Open log from given log set by name
 *
 * @return log handle on success, -1 on error, -2 if log not found
 */
static int32_t OpenLogInternal(NXCORE_LOG *logs, const TCHAR *name, ClientSession *session, uint32_t *rcc)
{
	for(int i = 0; logs[i].name != nullptr; i++)
	{
		if (!_tcsicmp(name, logs[i].name))
		{
			if (session->checkSysAccessRights(logs[i].requiredAccess))
			{
				*rcc = RCC_SUCCESS;
				LogHandle *handle = new LogHandle(&logs[i]);
				return RegisterLogHandle(handle, session);
			}
			else
			{
				*rcc = RCC_ACCESS_DENIED;
				return -1;
			}
		}
	}
   return -2;
}

/**
 * Open log by name
 */
int32_t OpenLog(const TCHAR *name, ClientSession *session, uint32_t *rcc)
{
   int32_t rc = OpenLogInternal(s_logs, name, session, rcc);
   if (rc != -2)
      return rc;

   // Try to find log definition in loaded modules
   ENUMERATE_MODULES(logs)
	{
      rc = OpenLogInternal(CURRENT_MODULE.logs, name, session, rcc);
      if (rc != -2)
         return rc;
	}

	*rcc = RCC_UNKNOWN_LOG_NAME;
	return -1;
}

/**
 * Close log
 */
uint32_t CloseLog(ClientSession *session, int32_t logHandle)
{
   uint32_t rcc = RCC_INVALID_LOG_HANDLE;
   LogHandle *log = NULL;

   DbgPrintf(6, _T("CloseLog: close request from session %d for handle %d"), session->getId(), logHandle);
	MutexLock(s_regListMutex);

	if ((logHandle >= 0) && (logHandle < s_regListSize) &&
	    (s_regList[logHandle].sessionId == session->getId()) &&
		 (s_regList[logHandle].handle != nullptr))
	{
      log = s_regList[logHandle].handle;
      s_regList[logHandle].handle = nullptr;
	}

	MutexUnlock(s_regListMutex);

   if (log != nullptr)
   {
      log->decRefCount();
      rcc = RCC_SUCCESS;
   }
	return rcc;
}

/**
 * Close log
 */
void CloseAllLogsForSession(session_id_t sessionId)
{
   nxlog_debug(6, _T("Closing all logs for session %d"), sessionId);
   MutexLock(s_regListMutex);
   for(int i = 0; i < s_regListSize; i++)
   {
      if ((s_regList[i].sessionId == sessionId) && (s_regList[i].handle != nullptr))
      {
         s_regList[i].handle->decRefCount();
         s_regList[i].handle = nullptr;
      }
   }
   MutexUnlock(s_regListMutex);
}

/**
 * Acquire log handle object
 * Caller must call LogHandle::unlock() when it finish work with acquired object
 */
LogHandle *AcquireLogHandleObject(ClientSession *session, int32_t logHandle)
{
	LogHandle *object = nullptr;

   DbgPrintf(6, _T("AcquireLogHandleObject: request from session %d for handle %d"), session->getId(), logHandle);
	MutexLock(s_regListMutex);

	if ((logHandle >= 0) && (logHandle < s_regListSize) &&
	    (s_regList[logHandle].sessionId == session->getId()) &&
		 (s_regList[logHandle].handle != nullptr))
	{
		object = s_regList[logHandle].handle;
      object->incRefCount();
	}

	MutexUnlock(s_regListMutex);

   if (object != nullptr)
      object->lock();
	return object;
}
