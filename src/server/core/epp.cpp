/*
** NetXMS - Network Management System
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
** File: epp.cpp
**
**/

#include "nxcore.h"

#define DEBUG_TAG _T("event.policy")

/**
 * Default event policy rule constructor
 */
EPRule::EPRule(UINT32 id)
{
   m_id = id;
   m_guid = uuid::generate();
   m_flags = 0;
   m_comments = NULL;
   m_alarmSeverity = 0;
   m_alarmKey[0] = 0;
   m_alarmMessage[0] = 0;
   m_scriptSource = NULL;
   m_script = NULL;
	m_alarmTimeout = 0;
	m_alarmTimeoutEvent = EVENT_ALARM_TIMEOUT;
}

/**
 * Create rule from config entry
 */
EPRule::EPRule(ConfigEntry *config)
{
   m_id = 0;
   m_guid = config->getSubEntryValueAsUUID(_T("guid"));
   if (m_guid.isNull())
      m_guid = uuid::generate(); // generate random GUID if rule was imported without GUID
   m_flags = config->getSubEntryValueAsUInt(_T("flags"));

	ConfigEntry *eventsRoot = config->findEntry(_T("events"));
   if (eventsRoot != NULL)
   {
		ObjectArray<ConfigEntry> *events = eventsRoot->getSubEntries(_T("event#*"));
      for(int i = 0; i < events->size(); i++)
      {
         EventTemplate *e = FindEventTemplateByName(events->get(i)->getSubEntryValue(_T("name"), 0, _T("<unknown>")));
         if (e != NULL)
         {
            m_events.add(e->getCode());
            e->decRefCount();
         }
      }
      delete events;
   }

   m_comments = _tcsdup(config->getSubEntryValue(_T("comments"), 0, _T("")));
   m_alarmSeverity = config->getSubEntryValueAsInt(_T("alarmSeverity"));
	m_alarmTimeout = config->getSubEntryValueAsUInt(_T("alarmTimeout"));
	m_alarmTimeoutEvent = config->getSubEntryValueAsUInt(_T("alarmTimeoutEvent"), 0, EVENT_ALARM_TIMEOUT);
   _tcslcpy(m_alarmKey, config->getSubEntryValue(_T("alarmKey"), 0, _T("")), MAX_DB_STRING);
   _tcslcpy(m_alarmMessage, config->getSubEntryValue(_T("alarmMessage"), 0, _T("")), MAX_DB_STRING);

   ConfigEntry *pStorageEntry = config->findEntry(_T("pStorageActions"));
   if (pStorageEntry != NULL)
   {
      ObjectArray<ConfigEntry> *tmp = pStorageEntry->getSubEntries(_T("setValue"));
      if (tmp->size() > 0)
      {
         tmp = tmp->get(0)->getSubEntries(_T("value"));
         for(int i = 0; i < tmp->size(); i++)
         {
            m_pstorageSetActions.set(tmp->get(i)->getAttribute(_T("key")), tmp->get(i)->getValue());
         }
      }
      delete tmp;

      tmp = pStorageEntry->getSubEntries(_T("deleteValue"));
      if (tmp->size() > 0)
      {
         tmp = tmp->get(0)->getSubEntries(_T("value"));
         for(int i = 0; i < tmp->size(); i++)
         {
            m_pstorageDeleteActions.add(tmp->get(i)->getAttribute(_T("key")));
         }
      }
      delete tmp;
   }

   m_scriptSource = _tcsdup(config->getSubEntryValue(_T("script"), 0, _T("")));
   if ((m_scriptSource != NULL) && (*m_scriptSource != 0))
   {
      TCHAR szError[256];

      m_script = NXSLCompileAndCreateVM(m_scriptSource, szError, 256, new NXSL_ServerEnv);
      if (m_script != NULL)
      {
      	m_script->setGlobalVariable(_T("CUSTOM_MESSAGE"), new NXSL_Value(_T("")));
      }
      else
      {
         nxlog_write(MSG_EPRULE_SCRIPT_COMPILATION_ERROR, EVENTLOG_ERROR_TYPE, "ds", m_id, szError);
      }
   }
   else
   {
      m_script = NULL;
   }

   ConfigEntry *actionsRoot = config->findEntry(_T("actions"));
   if (actionsRoot != NULL)
   {
      ObjectArray<ConfigEntry> *actions = actionsRoot->getSubEntries(_T("action#*"));
      for(int i = 0; i < actions->size(); i++)
      {
         uuid guid = actions->get(i)->getSubEntryValueAsUUID(_T("guid"));
         if (!guid.isNull())
         {
            UINT32 actionId = FindActionByGUID(guid);
            if (actionId != 0)
               m_actions.add(actionId);
         }
         else
         {
            UINT32 actionId = actions->get(i)->getId();
            if (IsValidActionId(actionId))
               m_actions.add(actionId);
         }
      }
      delete actions;
   }
}

/**
 * Construct event policy rule from database record
 * Assuming the following field order:
 * rule_id,rule_guid,flags,comments,alarm_message,alarm_severity,alarm_key,script,
 * alarm_timeout,alarm_timeout_event
 */
EPRule::EPRule(DB_RESULT hResult, int row)
{
   m_id = DBGetFieldULong(hResult, row, 0);
   m_guid = DBGetFieldGUID(hResult, row, 1);
   m_flags = DBGetFieldULong(hResult, row, 2);
   m_comments = DBGetField(hResult, row, 3, NULL, 0);
	DBGetField(hResult, row, 4, m_alarmMessage, MAX_EVENT_MSG_LENGTH);
   m_alarmSeverity = DBGetFieldLong(hResult, row, 5);
   DBGetField(hResult, row, 6, m_alarmKey, MAX_DB_STRING);
   m_scriptSource = DBGetField(hResult, row, 7, NULL, 0);
   if ((m_scriptSource != NULL) && (*m_scriptSource != 0))
   {
      TCHAR szError[256];

      m_script = NXSLCompileAndCreateVM(m_scriptSource, szError, 256, new NXSL_ServerEnv);
      if (m_script != NULL)
      {
      	m_script->setGlobalVariable(_T("CUSTOM_MESSAGE"), new NXSL_Value(_T("")));
      }
      else
      {
         nxlog_write(MSG_EPRULE_SCRIPT_COMPILATION_ERROR, EVENTLOG_ERROR_TYPE,
                  "ds", m_id, szError);
      }
   }
   else
   {
      m_script = NULL;
   }
	m_alarmTimeout = DBGetFieldULong(hResult, row, 8);
	m_alarmTimeoutEvent = DBGetFieldULong(hResult, row, 9);
}

/**
 * Construct event policy rule from NXCP message
 */
EPRule::EPRule(NXCPMessage *msg)
{
   m_flags = msg->getFieldAsUInt32(VID_FLAGS);
   m_id = msg->getFieldAsUInt32(VID_RULE_ID);
   m_guid = msg->getFieldAsGUID(VID_GUID);
   m_comments = msg->getFieldAsString(VID_COMMENTS);

   msg->getFieldAsInt32Array(VID_RULE_ACTIONS, &m_actions);
   msg->getFieldAsInt32Array(VID_RULE_EVENTS, &m_events);
   msg->getFieldAsInt32Array(VID_RULE_SOURCES, &m_sources);

   msg->getFieldAsString(VID_ALARM_KEY, m_alarmKey, MAX_DB_STRING);
   msg->getFieldAsString(VID_ALARM_MESSAGE, m_alarmMessage, MAX_DB_STRING);
   m_alarmSeverity = msg->getFieldAsUInt16(VID_ALARM_SEVERITY);
	m_alarmTimeout = msg->getFieldAsUInt32(VID_ALARM_TIMEOUT);
	m_alarmTimeoutEvent = msg->getFieldAsUInt32(VID_ALARM_TIMEOUT_EVENT);

   msg->getFieldAsInt32Array(VID_ALARM_CATEGORY_ID, &m_alarmCategoryList);

   int count = msg->getFieldAsInt32(VID_NUM_SET_PSTORAGE);
   int base = VID_PSTORAGE_SET_LIST_BASE;
   for(int i = 0; i < count; i++, base+=2)
   {
      m_pstorageSetActions.setPreallocated(msg->getFieldAsString(base), msg->getFieldAsString(base+1));
   }

   count = msg->getFieldAsInt32(VID_NUM_DELETE_PSTORAGE);
   base = VID_PSTORAGE_DELETE_LIST_BASE;
   for(int i = 0; i < count; i++, base++)
   {
      m_pstorageDeleteActions.addPreallocated(msg->getFieldAsString(base));
   }

   m_scriptSource = msg->getFieldAsString(VID_SCRIPT);
   if ((m_scriptSource != NULL) && (*m_scriptSource != 0))
   {
      TCHAR szError[256];

      m_script = NXSLCompileAndCreateVM(m_scriptSource, szError, 256, new NXSL_ServerEnv);
      if (m_script != NULL)
      {
      	m_script->setGlobalVariable(_T("CUSTOM_MESSAGE"), new NXSL_Value(_T("")));
      }
      else
      {
         nxlog_write(MSG_EPRULE_SCRIPT_COMPILATION_ERROR, EVENTLOG_ERROR_TYPE, "ds", m_id, szError);
      }
   }
   else
   {
      m_script = NULL;
   }
}

/**
 * Event policy rule destructor
 */
EPRule::~EPRule()
{
   free(m_comments);
   free(m_scriptSource);
   delete m_script;
}

/**
 * Callback for filling export XML with set persistent storage actions
 */
static EnumerationCallbackResult AddPSSetActionsToSML(const TCHAR *key, const void *value, void *data)
{
   String *str = (String *)data;
   str->appendFormattedString(_T("\t\t\t\t\t<value key=\"%d\">%s</value>\n"), key, value);
   return _CONTINUE;
}

/**
 * Create management pack record
 */
void EPRule::createNXMPRecord(String &str)
{
   str.append(_T("\t\t<rule id=\""));
   str.append(m_id + 1);
   str.append(_T("\">\n\t\t\t<guid>"));
   str.append(m_guid.toString());
   str.append(_T("</guid>\n\t\t\t<flags>"));
   str.append(m_flags);
   str.append(_T("</flags>\n\t\t\t<alarmMessage>"));
   str.append(EscapeStringForXML2(m_alarmMessage));
   str.append(_T("</alarmMessage>\n\t\t\t<alarmKey>"));
   str.append(EscapeStringForXML2(m_alarmKey));
   str.append(_T("</alarmKey>\n\t\t\t<alarmSeverity>"));
   str.append(m_alarmSeverity);
   str.append(_T("</alarmSeverity>\n\t\t\t<alarmTimeout>"));
   str.append(m_alarmTimeout);
   str.append(_T("</alarmTimeout>\n\t\t\t<alarmTimeoutEvent>"));
   str.append(m_alarmTimeoutEvent);
   str.append(_T("</alarmTimeoutEvent>\n\t\t\t<script>"));
   str.append(EscapeStringForXML2(m_scriptSource));
   str.append(_T("</script>\n\t\t\t<comments>"));
   str.append(EscapeStringForXML2(m_comments));
   str.append(_T("</comments>\n\t\t\t<sources>\n"));

   for(int i = 0; i < m_sources.get(i); i++)
   {
      NetObj *object = FindObjectById(m_sources.get(i));
      if (object != NULL)
      {
         TCHAR guidText[128];
         str.appendFormattedString(_T("\t\t\t\t<source id=\"%d\">\n")
                                _T("\t\t\t\t\t<name>%s</name>\n")
                                _T("\t\t\t\t\t<guid>%s</guid>\n")
                                _T("\t\t\t\t\t<class>%d</class>\n")
                                _T("\t\t\t\t</source>\n"),
                                object->getId(),
                                (const TCHAR *)EscapeStringForXML2(object->getName()),
                                object->getGuid().toString(guidText), object->getObjectClass());
      }
   }

   str += _T("\t\t\t</sources>\n\t\t\t<events>\n");

   for(int i = 0; i < m_events.size(); i++)
   {
      TCHAR eventName[MAX_EVENT_NAME];
      EventNameFromCode(m_events.get(i), eventName);
      str.appendFormattedString(_T("\t\t\t\t<event id=\"%d\">\n")
                             _T("\t\t\t\t\t<name>%s</name>\n")
                             _T("\t\t\t\t</event>\n"),
                             m_events.get(i), (const TCHAR *)EscapeStringForXML2(eventName));
   }

   str += _T("\t\t\t</events>\n\t\t\t<actions>\n");

   for(int i = 0; i < m_actions.size(); i++)
   {
      str.append(_T("\t\t\t\t<action id=\""));
      UINT32 id = m_actions.get(i);
      str.append(id);
      str.append(_T("\">\n\t\t\t\t\t<guid>"));
      str.append(GetActionGUID(id));
      str.append(_T("</guid>\n\t\t\t\t</action>\n"));
   }

   str += _T("\t\t\t</actions>\n\t\t\t<pStorageActions>\n\t\t\t\t<setValue>\n");
   m_pstorageSetActions.forEach(AddPSSetActionsToSML, &str);
   str += _T("\t\t\t\t</setValue>\n\t\t\t\t<deleteValue>\n");
   for(int i = 0; i < m_pstorageDeleteActions.size(); i++)
   {
      str.append(_T("\t\t\t\t\t<value key=\""));
      str.append(m_pstorageDeleteActions.get(i));
      str.append(_T("\" />\n"));
   }
   str += _T("\t\t\t\t</deleteValue>\n\t\t\t</pStorageActions>\n\t\t</rule>\n");
}

/**
 * Check if source object's id match to the rule
 */
bool EPRule::matchSource(UINT32 objectId)
{
   if (m_sources.isEmpty())
      return (m_flags & RF_NEGATED_EVENTS) ? false : true;

   bool match = false;
   for(int i = 0; i < m_sources.size(); i++)
   {
      if (m_sources.get(i) == objectId)
      {
         match = true;
         break;
      }

      NetObj *object = FindObjectById(m_sources.get(i));
      if (object != NULL)
      {
         if (object->isChild(objectId))
         {
            match = true;
            break;
         }
      }
      else
      {
         nxlog_write(MSG_INVALID_EPP_OBJECT, EVENTLOG_ERROR_TYPE, "d", m_sources.get(i));
      }
   }
   return (m_flags & RF_NEGATED_SOURCE) ? !match : match;
}

/**
 * Check if event's id match to the rule
 */
bool EPRule::matchEvent(UINT32 eventCode)
{
   if (m_events.isEmpty())
      return (m_flags & RF_NEGATED_EVENTS) ? false : true;

   bool match = false;
   for(int i = 0; i < m_events.size(); i++)
   {
      UINT32 e = m_events.get(i);
      if (e & GROUP_FLAG_BIT)
      {
         /* TODO: check group membership */
      }
      else
      {
         if (e == eventCode)
         {
            match = true;
            break;
         }
      }
   }
   return (m_flags & RF_NEGATED_EVENTS) ? !match : match;
}

/**
 * Check if event's severity match to the rule
 */
bool EPRule::matchSeverity(UINT32 severity)
{
   static UINT32 severityFlag[] = { RF_SEVERITY_INFO, RF_SEVERITY_WARNING,
                                    RF_SEVERITY_MINOR, RF_SEVERITY_MAJOR,
                                    RF_SEVERITY_CRITICAL };
	return (severityFlag[severity] & m_flags) != 0;
}

/**
 * Check if event match to the script
 */
bool EPRule::matchScript(Event *pEvent)
{
   bool bRet = true;

   if (m_script == NULL)
      return true;

   // Pass event's parameters as arguments and
   // other information as variables
   NXSL_Value **ppValueList = (NXSL_Value **)malloc(sizeof(NXSL_Value *) * pEvent->getParametersCount());
   memset(ppValueList, 0, sizeof(NXSL_Value *) * pEvent->getParametersCount());
   for(int i = 0; i < pEvent->getParametersCount(); i++)
      ppValueList[i] = new NXSL_Value(pEvent->getParameter(i));

   NXSL_VariableSystem *pLocals = new NXSL_VariableSystem;
   pLocals->create(_T("EVENT_CODE"), new NXSL_Value(pEvent->getCode()));
   pLocals->create(_T("SEVERITY"), new NXSL_Value(pEvent->getSeverity()));
   pLocals->create(_T("SEVERITY_TEXT"), new NXSL_Value(GetStatusAsText(pEvent->getSeverity(), true)));
   pLocals->create(_T("OBJECT_ID"), new NXSL_Value(pEvent->getSourceId()));
   pLocals->create(_T("EVENT_TEXT"), new NXSL_Value((TCHAR *)pEvent->getMessage()));
   pLocals->create(_T("USER_TAG"), new NXSL_Value((TCHAR *)pEvent->getUserTag()));
	NetObj *pObject = FindObjectById(pEvent->getSourceId());
	if (pObject != NULL)
	{
      m_script->setGlobalVariable(_T("$object"), pObject->createNXSLObject());
		if (pObject->getObjectClass() == OBJECT_NODE)
			m_script->setGlobalVariable(_T("$node"), pObject->createNXSLObject());
	}
	m_script->setGlobalVariable(_T("$event"), new NXSL_Value(new NXSL_Object(&g_nxslEventClass, pEvent)));
	m_script->setGlobalVariable(_T("CUSTOM_MESSAGE"), new NXSL_Value);

   // Run script
   NXSL_VariableSystem *globals = NULL;
   if (m_script->run(pEvent->getParametersCount(), ppValueList, pLocals, &globals))
   {
      NXSL_Value *value = m_script->getResult();
      if (value != NULL)
      {
         bRet = value->getValueAsInt32() ? true : false;
         if (bRet)
         {
         	NXSL_Variable *var = globals->find(_T("CUSTOM_MESSAGE"));
         	if (var != NULL)
         	{
         		// Update custom message in event
         		pEvent->setCustomMessage(CHECK_NULL_EX(var->getValue()->getValueAsCString()));
         	}
         }
      }
   }
   else
   {
      TCHAR buffer[1024];
      _sntprintf(buffer, 1024, _T("EPP::%d"), m_id + 1);
      PostEvent(EVENT_SCRIPT_ERROR, g_dwMgmtNode, "ssd", buffer, m_script->getErrorText(), 0);
      nxlog_write(MSG_EPRULE_SCRIPT_EXECUTION_ERROR, EVENTLOG_ERROR_TYPE, "ds", m_id + 1, m_script->getErrorText());
   }
   free(ppValueList);
   delete globals;

   return bRet;
}

/**
 * Callback for execution set action on persistent storage
 */
static EnumerationCallbackResult ExecutePstorageSetAction(const TCHAR *key, const void *value, void *data)
{
   Event *pEvent = (Event *)data;
   TCHAR *psValue = pEvent->expandText(key);
   TCHAR *psKey = pEvent->expandText((TCHAR *)value);
   SetPersistentStorageValue(psValue, psKey);
   free(psValue);
   free(psKey);
   return _CONTINUE;
}

/**
 * Check if event match to rule and perform required actions if yes
 * Method will return TRUE if event matched and RF_STOP_PROCESSING flag is set
 */
bool EPRule::processEvent(Event *event)
{
   bool bStopProcessing = false;

   // Check disable flag
   if (!(m_flags & RF_DISABLED))
   {
      // Check if event match
      if (matchSource(event->getSourceId()) && matchEvent(event->getCode()) &&
          matchSeverity(event->getSeverity()) && matchScript(event))
      {
			nxlog_debug_tag(DEBUG_TAG, 6, _T("Event ") UINT64_FMT _T(" match EPP rule %d"), event->getId(), (int)m_id);

         // Generate alarm if requested
         if (m_flags & RF_GENERATE_ALARM)
            generateAlarm(event);

         // Event matched, perform actions
         if (!m_actions.isEmpty())
         {
            TCHAR *alarmMessage = event->expandText(m_alarmMessage);
            TCHAR *alarmKey = event->expandText(m_alarmKey);
            for(int i = 0; i < m_actions.size(); i++)
               ExecuteAction(m_actions.get(i), event, alarmMessage, alarmKey);
            free(alarmMessage);
            free(alarmKey);
         }

			// Update persistent storage if needed
			if (m_pstorageSetActions.size() > 0)
            m_pstorageSetActions.forEach(ExecutePstorageSetAction, event);
			for(int i = 0; i < m_pstorageDeleteActions.size(); i++)
         {
            TCHAR *psKey = event->expandText(m_pstorageDeleteActions.get(i));
            DeletePersistentStorageValue(psKey);
            free(psKey);
         }

			bStopProcessing = (m_flags & RF_STOP_PROCESSING) ? true : false;
      }
   }

   return bStopProcessing;
}

/**
 * Generate alarm from event
 */
void EPRule::generateAlarm(Event *event)
{
   // Terminate alarms with key == our ack_key
	if ((m_alarmSeverity == SEVERITY_RESOLVE) || (m_alarmSeverity == SEVERITY_TERMINATE))
	{
		TCHAR *pszAckKey = event->expandText(m_alarmKey);
		if (pszAckKey[0] != 0)
		   ResolveAlarmByKey(pszAckKey, (m_flags & RF_TERMINATE_BY_REGEXP) ? true : false, m_alarmSeverity == SEVERITY_TERMINATE, event);
		free(pszAckKey);
	}
	else	// Generate new alarm
	{
		CreateNewAlarm(m_alarmMessage, m_alarmKey, ALARM_STATE_OUTSTANDING,
                     (m_alarmSeverity == SEVERITY_FROM_EVENT) ? event->getSeverity() : m_alarmSeverity,
                     m_alarmTimeout, m_alarmTimeoutEvent, event, 0, &m_alarmCategoryList,
                     ((m_flags & RF_CREATE_TICKET) != 0) ? true : false);
	}
}

/**
 * Load rule from database
 */
bool EPRule::loadFromDB(DB_HANDLE hdb)
{
   DB_RESULT hResult;
   TCHAR szQuery[256];
   bool bSuccess = true;

   // Load rule's sources
   _sntprintf(szQuery, 256, _T("SELECT object_id FROM policy_source_list WHERE rule_id=%d"), m_id);
   hResult = DBSelect(hdb, szQuery);
   if (hResult != NULL)
   {
      int count = DBGetNumRows(hResult);
      for(int i = 0; i < count; i++)
         m_sources.add(DBGetFieldULong(hResult, i, 0));
      DBFreeResult(hResult);
   }
   else
   {
      bSuccess = false;
   }

   // Load rule's events
   _sntprintf(szQuery, 256, _T("SELECT event_code FROM policy_event_list WHERE rule_id=%d"), m_id);
   hResult = DBSelect(hdb, szQuery);
   if (hResult != NULL)
   {
      int count = DBGetNumRows(hResult);
      for(int i = 0; i < count; i++)
         m_events.add(DBGetFieldULong(hResult, i, 0));
      DBFreeResult(hResult);
   }
   else
   {
      bSuccess = false;
   }

   // Load rule's actions
   _sntprintf(szQuery, 256, _T("SELECT action_id FROM policy_action_list WHERE rule_id=%d"), m_id);
   hResult = DBSelect(hdb, szQuery);
   if (hResult != NULL)
   {
      int count = DBGetNumRows(hResult);
      for(int i = 0; i < count; i++)
         m_actions.add(DBGetFieldULong(hResult, i, 0));
      DBFreeResult(hResult);
   }
   else
   {
      bSuccess = false;
   }

   // Load pstorage actions
   _sntprintf(szQuery, 256, _T("SELECT ps_key,action,value FROM policy_pstorage_actions WHERE rule_id=%d"), m_id);
   hResult = DBSelect(hdb, szQuery);
   if (hResult != NULL)
   {
      TCHAR key[MAX_DB_STRING];
      int count = DBGetNumRows(hResult);
      for(int i = 0; i < count; i++)
      {
         DBGetField(hResult, i, 0, key, MAX_DB_STRING);
         if (DBGetFieldULong(hResult, i, 1) == PSTORAGE_SET)
         {
            m_pstorageSetActions.setPreallocated(_tcsdup(key), DBGetField(hResult, i, 2, NULL, 0));
         }
         if (DBGetFieldULong(hResult, i, 1) == PSTORAGE_DELETE)
         {
            m_pstorageDeleteActions.add(key);
         }
      }
      DBFreeResult(hResult);
   }
   else
   {
      bSuccess = false;
   }

   // Load alarm categories
   _sntprintf(szQuery, 256, _T("SELECT category_id FROM alarm_category_map WHERE alarm_id=%d"), m_id);
   hResult = DBSelect(hdb, szQuery);
   if (hResult != NULL)
   {
      int count = DBGetNumRows(hResult);
      for(int i = 0; i < count; i++)
      {
         m_alarmCategoryList.add(DBGetFieldULong(hResult, i, 0));
      }
      DBFreeResult(hResult);
   }
   else
   {
      bSuccess = false;
   }

   return bSuccess;
}

/**
 * Callback for persistent storage set actions
 */
static EnumerationCallbackResult SavePstorageSetActions(const TCHAR *key, const void *value, void *data)
{
   DB_STATEMENT hStmt = (DB_STATEMENT)data;
   DBBind(hStmt, 2, DB_SQLTYPE_VARCHAR, key, DB_BIND_STATIC);
   DBBind(hStmt, 3, DB_SQLTYPE_VARCHAR, (TCHAR *)value, DB_BIND_STATIC);
   return DBExecute(hStmt) ? _CONTINUE : _STOP;
}

/**
 * Save rule to database
 */
bool EPRule::saveToDB(DB_HANDLE hdb)
{
   bool success;
	int i;
	TCHAR pszQuery[1024];
   // General attributes
   DB_STATEMENT hStmt = DBPrepare(hdb, _T("INSERT INTO event_policy (rule_id,rule_guid,flags,comments,alarm_message,")
                                  _T("alarm_severity,alarm_key,script,alarm_timeout,alarm_timeout_event)")
                                  _T("VALUES (?,?,?,?,?,?,?,?,?,?)"));
   if (hStmt != NULL)
   {
      TCHAR guidText[128];
      DBBind(hStmt, 1, DB_CTYPE_INT32, m_id);
      DBBind(hStmt, 2, DB_CTYPE_STRING, m_guid.toString(guidText), DB_BIND_STATIC);
      DBBind(hStmt, 3, DB_CTYPE_INT32, m_flags);
      DBBind(hStmt, 4, DB_CTYPE_STRING,  m_comments, DB_BIND_STATIC);
      DBBind(hStmt, 5, DB_CTYPE_STRING, m_alarmMessage, DB_BIND_STATIC);
      DBBind(hStmt, 6, DB_CTYPE_INT32, m_alarmSeverity);
      DBBind(hStmt, 7, DB_CTYPE_STRING, m_alarmKey, DB_BIND_STATIC);
      DBBind(hStmt, 8, DB_CTYPE_STRING, m_scriptSource, DB_BIND_STATIC);
      DBBind(hStmt, 9, DB_CTYPE_INT32, m_alarmTimeout);
      DBBind(hStmt, 10, DB_CTYPE_INT32, m_alarmTimeoutEvent);
      success = DBExecute(hStmt);
      DBFreeStatement(hStmt);
   }
   else
   {
      success = false;
   }

   // Actions
   if (success && !m_actions.isEmpty())
   {
      for(i = 0; i < m_actions.size() && success; i++)
      {
         _sntprintf(pszQuery, 1024, _T("INSERT INTO policy_action_list (rule_id,action_id) VALUES (%d,%d)"), m_id, m_actions.get(i));
         success = DBQuery(hdb, pszQuery);
      }
   }

   // Events
   if (success && !m_events.isEmpty())
   {
      for(i = 0; i < m_events.size() && success; i++)
      {
         _sntprintf(pszQuery, 1024, _T("INSERT INTO policy_event_list (rule_id,event_code) VALUES (%d,%d)"), m_id, m_events.get(i));
         success = DBQuery(hdb, pszQuery);
      }
   }

   // Sources
   if (success && !m_sources.isEmpty())
   {
      for(i = 0; i < m_sources.size() && success; i++)
      {
         _sntprintf(pszQuery, 1024, _T("INSERT INTO policy_source_list (rule_id,object_id) VALUES (%d,%d)"), m_id, m_sources.get(i));
         success = DBQuery(hdb, pszQuery);
      }
   }

	// Persistent storage actions
   if (success && !m_pstorageSetActions.isEmpty())
   {
      hStmt = DBPrepare(hdb, _T("INSERT INTO policy_pstorage_actions (rule_id,action,ps_key,value) VALUES (?,1,?,?)"), m_pstorageSetActions.size() > 1);
      if (hStmt != NULL)
      {
         DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_id);
         success = _STOP != m_pstorageSetActions.forEach(SavePstorageSetActions, hStmt);
         DBFreeStatement(hStmt);
      }
   }

   if (success && !m_pstorageDeleteActions.isEmpty())
   {
      hStmt = DBPrepare(hdb, _T("INSERT INTO policy_pstorage_actions (rule_id,action,ps_key) VALUES (?,2,?)"), m_pstorageDeleteActions.size() > 1);
      if (hStmt != NULL)
      {
         DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_id);
         for(int i = 0; i < m_pstorageDeleteActions.size() && success; i++)
         {
            DBBind(hStmt, 2, DB_SQLTYPE_VARCHAR, m_pstorageDeleteActions.get(i), DB_BIND_STATIC);
            success = DBExecute(hStmt);
         }
         DBFreeStatement(hStmt);
      }
   }

   // Alarm categories
   if (success && !m_alarmCategoryList.isEmpty())
   {
      hStmt = DBPrepare(hdb, _T("INSERT INTO alarm_category_map (alarm_id,category_id) VALUES (?,?)"), m_alarmCategoryList.size() > 1);
      if (hStmt != NULL)
      {
         DBBind(hStmt, 1, DB_SQLTYPE_INTEGER && success, m_id);
         for(i = 0; (i < m_alarmCategoryList.size()) && success; i++)
         {
            DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, m_alarmCategoryList.get(i));
            success = DBExecute(hStmt);
         }
         DBFreeStatement(hStmt);
      }
   }

   return success;
}

/**
 * Create NXCP message with rule's data
 */
void EPRule::createMessage(NXCPMessage *msg)
{
   msg->setField(VID_FLAGS, m_flags);
   msg->setField(VID_RULE_ID, m_id);
   msg->setField(VID_GUID, m_guid);
   msg->setField(VID_ALARM_SEVERITY, (WORD)m_alarmSeverity);
   msg->setField(VID_ALARM_KEY, m_alarmKey);
   msg->setField(VID_ALARM_MESSAGE, m_alarmMessage);
   msg->setField(VID_ALARM_TIMEOUT, m_alarmTimeout);
   msg->setField(VID_ALARM_TIMEOUT_EVENT, m_alarmTimeoutEvent);
   msg->setFieldFromInt32Array(VID_ALARM_CATEGORY_ID, &m_alarmCategoryList);
   msg->setField(VID_COMMENTS, CHECK_NULL_EX(m_comments));
   msg->setField(VID_NUM_ACTIONS, (UINT32)m_actions.size());
   msg->setFieldFromInt32Array(VID_RULE_ACTIONS, &m_actions);
   msg->setField(VID_NUM_EVENTS, (UINT32)m_events.size());
   msg->setFieldFromInt32Array(VID_RULE_EVENTS, &m_events);
   msg->setField(VID_NUM_SOURCES, (UINT32)m_sources.size());
   msg->setFieldFromInt32Array(VID_RULE_SOURCES, &m_sources);
   msg->setField(VID_SCRIPT, CHECK_NULL_EX(m_scriptSource));
   m_pstorageSetActions.fillMessage(msg, VID_NUM_SET_PSTORAGE, VID_PSTORAGE_SET_LIST_BASE);
   m_pstorageDeleteActions.fillMessage(msg, VID_PSTORAGE_DELETE_LIST_BASE, VID_NUM_DELETE_PSTORAGE);
}

/**
 * Serialize rule to JSON
 */
json_t *EPRule::toJson() const
{
   json_t *root = json_object();
   json_object_set_new(root, "guid", m_guid.toJson());
   json_object_set_new(root, "flags", json_integer(m_flags));
   json_object_set_new(root, "sources", m_sources.toJson());
   json_object_set_new(root, "events", m_events.toJson());
   json_object_set_new(root, "actions", m_actions.toJson());
   json_object_set_new(root, "comments", json_string_t(m_comments));
   json_object_set_new(root, "script", json_string_t(m_scriptSource));
   json_object_set_new(root, "alarmMessage", json_string_t(m_alarmMessage));
   json_object_set_new(root, "alarmSeverity", json_integer(m_alarmSeverity));
   json_object_set_new(root, "alarmKey", json_string_t(m_alarmKey));
   json_object_set_new(root, "alarmTimeout", json_integer(m_alarmTimeout));
   json_object_set_new(root, "alarmTimeoutEvent", json_integer(m_alarmTimeoutEvent));
   json_object_set_new(root, "categories", m_alarmCategoryList.toJson());
   json_object_set_new(root, "pstorageSetActions", m_pstorageSetActions.toJson());
   json_object_set_new(root, "pstorageDeleteActions", m_pstorageDeleteActions.toJson());

   return root;
}

/**
 * Event processing policy constructor
 */
EventPolicy::EventPolicy() : m_rules(128, 128, true)
{
   m_rwlock = RWLockCreate();
}

/**
 * Event processing policy destructor
 */
EventPolicy::~EventPolicy()
{
   RWLockDestroy(m_rwlock);
}

/**
 * Load event processing policy from database
 */
bool EventPolicy::loadFromDB()
{
   DB_RESULT hResult;
   bool success = false;

   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   hResult = DBSelect(hdb, _T("SELECT rule_id,rule_guid,flags,comments,alarm_message,")
                           _T("alarm_severity,alarm_key,script,alarm_timeout,alarm_timeout_event ")
                           _T("FROM event_policy ORDER BY rule_id"));
   if (hResult != NULL)
   {
      success = true;
      int count = DBGetNumRows(hResult);
      for(int i = 0; (i < count) && success; i++)
      {
         EPRule *rule = new EPRule(hResult, i);
         success = rule->loadFromDB(hdb);
         if (success)
            m_rules.add(rule);
         else
            delete rule;
      }
      DBFreeResult(hResult);
   }

   DBConnectionPoolReleaseConnection(hdb);
   return success;
}

/**
 * Save event processing policy to database
 */
bool EventPolicy::saveToDB() const
{
	DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   bool success = DBBegin(hdb);
   if (success)
   {
      success = DBQuery(hdb, _T("DELETE FROM event_policy")) &&
                DBQuery(hdb, _T("DELETE FROM policy_action_list")) &&
                DBQuery(hdb, _T("DELETE FROM policy_event_list")) &&
                DBQuery(hdb, _T("DELETE FROM policy_source_list")) &&
                DBQuery(hdb, _T("DELETE FROM policy_pstorage_actions")) &&
                DBQuery(hdb, _T("DELETE FROM alarm_category_map"));

      if (success)
      {
         readLock();
         for(int i = 0; (i < m_rules.size()) && success; i++)
            success = m_rules.get(i)->saveToDB(hdb);
         unlock();
      }

      if (success)
         DBCommit(hdb);
      else
         DBRollback(hdb);
   }
	DBConnectionPoolReleaseConnection(hdb);
	return success;
}

/**
 * Pass event through policy
 */
void EventPolicy::processEvent(Event *pEvent)
{
	nxlog_debug_tag(DEBUG_TAG, 7, _T("EPP: processing event ") UINT64_FMT, pEvent->getId());
   readLock();
   for(int i = 0; i < m_rules.size(); i++)
      if (m_rules.get(i)->processEvent(pEvent))
		{
			nxlog_debug_tag(DEBUG_TAG, 7, _T("EPP: got \"stop processing\" flag for event ") UINT64_FMT _T(" at rule %d"), pEvent->getId(), i + 1);
         break;   // EPRule::ProcessEvent() return TRUE if we should stop processing this event
		}
   unlock();
}

/**
 * Send event policy to client
 */
void EventPolicy::sendToClient(ClientSession *pSession, UINT32 dwRqId) const
{
   NXCPMessage msg;

   readLock();
   msg.setCode(CMD_EPP_RECORD);
   msg.setId(dwRqId);
   for(int i = 0; i < m_rules.size(); i++)
   {
      m_rules.get(i)->createMessage(&msg);
      pSession->sendMessage(&msg);
      msg.deleteAllFields();
   }
   unlock();
}

/**
 * Replace policy with new one
 */
void EventPolicy::replacePolicy(UINT32 dwNumRules, EPRule **ppRuleList)
{
   writeLock();
   m_rules.clear();
   if (ppRuleList != NULL)
   {
      for(int i = 0; i < (int)dwNumRules; i++)
      {
         EPRule *r = ppRuleList[i];
         r->setId(i);
         m_rules.add(r);
      }
   }
   unlock();
}

/**
 * Check if given action is used in policy
 */
bool EventPolicy::isActionInUse(UINT32 actionId) const
{
   bool bResult = false;

   readLock();

   for(int i = 0; i < m_rules.size(); i++)
      if (m_rules.get(i)->isActionInUse(actionId))
      {
         bResult = true;
         break;
      }

   unlock();
   return bResult;
}

/**
 * Check if given category is used in policy
 */
bool EventPolicy::isCategoryInUse(UINT32 categoryId) const
{
   bool bResult = false;

   readLock();

   for(int i = 0; i < m_rules.size(); i++)
      if (m_rules.get(i)->isCategoryInUse(categoryId))
      {
         bResult = true;
         break;
      }

   unlock();
   return bResult;
}

/**
 * Export rule
 */
void EventPolicy::exportRule(String& str, const uuid& guid) const
{
   readLock();
   for(int i = 0; i < m_rules.size(); i++)
   {
      if (guid.equals(m_rules.get(i)->getGuid()))
      {
         m_rules.get(i)->createNXMPRecord(str);
         break;
      }
   }
   unlock();
}

/**
 * Import rule
 */
void EventPolicy::importRule(EPRule *rule)
{
   writeLock();

   // Find rule with same GUID and replace it if found
   bool newRule = true;
   for(int i = 0; i < m_rules.size(); i++)
   {
      if (rule->getGuid().equals(m_rules.get(i)->getGuid()))
      {
         rule->setId(i);
         m_rules.set(i, rule);
         newRule = false;
         break;
      }
   }

   // Add new rule at the end
   if (newRule)
   {
      rule->setId(m_rules.size());
      m_rules.add(rule);
   }

   unlock();
}

/**
 * Create JSON representation
 */
json_t *EventPolicy::toJson() const
{
   json_t *root = json_object();
   json_t *rules = json_array();
   readLock();
   for(int i = 0; i < m_rules.size(); i++)
   {
      json_array_append_new(rules, m_rules.get(i)->toJson());
   }
   unlock();
   json_object_set_new(root, "rules", rules);
   return root;
}
