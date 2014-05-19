/*
** NetXMS - Network Management System
** Copyright (C) 2003-2014 Victor Kirhenshtein
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

/**
 * Default event policy rule constructor
 */
EPRule::EPRule(UINT32 dwId)
{
   m_dwId = dwId;
   uuid_generate(m_guid);
   m_dwFlags = 0;
   m_dwNumSources = 0;
   m_pdwSourceList = NULL;
   m_dwNumEvents = 0;
   m_pdwEventList = NULL;
   m_dwNumActions = 0;
   m_pdwActionList = NULL;
   m_pszComment = NULL;
   m_iAlarmSeverity = 0;
   m_szAlarmKey[0] = 0;
   m_szAlarmMessage[0] = 0;
   m_pszScript = NULL;
   m_pScript = NULL;
	m_dwAlarmTimeout = 0;
	m_dwAlarmTimeoutEvent = EVENT_ALARM_TIMEOUT;
	m_dwSituationId = 0;
	m_szSituationInstance[0] = 0;
}

/**
 * Create rule from config entry
 */
EPRule::EPRule(ConfigEntry *config)
{
   m_dwId = 0;
   config->getSubEntryValueAsUUID(_T("guid"), m_guid);
   m_dwFlags = config->getSubEntryValueAsUInt(_T("flags"));
   m_dwNumSources = 0;
   m_pdwSourceList = NULL;
   m_dwNumActions = 0;
   m_pdwActionList = NULL;

	ConfigEntry *eventsRoot = config->findEntry(_T("events"));
   if (eventsRoot != NULL)
   {
		ObjectArray<ConfigEntry> *events = eventsRoot->getSubEntries(_T("event#*"));
      m_dwNumEvents = 0;
      m_pdwEventList = (UINT32 *)malloc(sizeof(UINT32) * events->size());
      for(int i = 0; i < events->size(); i++)
      {
         EVENT_TEMPLATE *e = FindEventTemplateByName(events->get(i)->getSubEntryValue(_T("name"), 0, _T("<unknown>")));
         if (e != NULL)
         {
            m_pdwEventList[m_dwNumEvents++] = e->dwCode;
         }
      }
   }

   m_pszComment = _tcsdup(config->getSubEntryValue(_T("comments"), 0, _T("")));
   m_iAlarmSeverity = config->getSubEntryValueAsInt(_T("alarmSeverity"));
	m_dwAlarmTimeout = config->getSubEntryValueAsUInt(_T("alarmTimeout"));
	m_dwAlarmTimeoutEvent = config->getSubEntryValueAsUInt(_T("alarmTimeout"), 0, EVENT_ALARM_TIMEOUT);
   nx_strncpy(m_szAlarmKey, config->getSubEntryValue(_T("alarmKey"), 0, _T("")), MAX_DB_STRING);
   nx_strncpy(m_szAlarmMessage, config->getSubEntryValue(_T("alarmMessage"), 0, _T("")), MAX_DB_STRING);

   m_dwSituationId = 0;
	m_szSituationInstance[0] = 0;

   m_pszScript = _tcsdup(config->getSubEntryValue(_T("script"), 0, _T("")));
   if ((m_pszScript != NULL) && (*m_pszScript != 0))
   {
      TCHAR szError[256];

      m_pScript = NXSLCompileAndCreateVM(m_pszScript, szError, 256, new NXSL_ServerEnv);
      if (m_pScript != NULL)
      {
      	m_pScript->setGlobalVariable(_T("CUSTOM_MESSAGE"), new NXSL_Value(_T("")));
      }
      else
      {
         nxlog_write(MSG_EPRULE_SCRIPT_COMPILATION_ERROR, EVENTLOG_ERROR_TYPE, "ds", m_dwId, szError);
      }
   }
   else
   {
      m_pScript = NULL;
   }
}

/**
 * Construct event policy rule from database record
 * Assuming the following field order:
 * rule_id,rule_guid,flags,comments,alarm_message,alarm_severity,alarm_key,script,
 * alarm_timeout,alarm_timeout_event,situation_id,situation_instance
 */
EPRule::EPRule(DB_RESULT hResult, int iRow)
{
   m_dwId = DBGetFieldULong(hResult, iRow, 0);
   DBGetFieldGUID(hResult, iRow, 1, m_guid);
   m_dwFlags = DBGetFieldULong(hResult, iRow, 2);
   m_pszComment = DBGetField(hResult, iRow, 3, NULL, 0);
	DBGetField(hResult, iRow, 4, m_szAlarmMessage, MAX_EVENT_MSG_LENGTH);
   m_iAlarmSeverity = DBGetFieldLong(hResult, iRow, 5);
   DBGetField(hResult, iRow, 6, m_szAlarmKey, MAX_DB_STRING);
   m_pszScript = DBGetField(hResult, iRow, 7, NULL, 0);
   if ((m_pszScript != NULL) && (*m_pszScript != 0))
   {
      TCHAR szError[256];

      m_pScript = NXSLCompileAndCreateVM(m_pszScript, szError, 256, new NXSL_ServerEnv);
      if (m_pScript != NULL)
      {
      	m_pScript->setGlobalVariable(_T("CUSTOM_MESSAGE"), new NXSL_Value(_T("")));
      }
      else
      {
         nxlog_write(MSG_EPRULE_SCRIPT_COMPILATION_ERROR, EVENTLOG_ERROR_TYPE,
                  "ds", m_dwId, szError);
      }
   }
   else
   {
      m_pScript = NULL;
   }
	m_dwAlarmTimeout = DBGetFieldULong(hResult, iRow, 8);
	m_dwAlarmTimeoutEvent = DBGetFieldULong(hResult, iRow, 9);
	m_dwSituationId = DBGetFieldULong(hResult, iRow, 10);
   DBGetField(hResult, iRow, 11, m_szSituationInstance, MAX_DB_STRING);
}

/**
 * Construct event policy rule from NXCP message
 */
EPRule::EPRule(CSCPMessage *pMsg)
{
	UINT32 i, id, count;
	TCHAR *name, *value;

   m_dwFlags = pMsg->GetVariableLong(VID_FLAGS);
   m_dwId = pMsg->GetVariableLong(VID_RULE_ID);
   pMsg->GetVariableBinary(VID_GUID, m_guid, UUID_LENGTH);
   m_pszComment = pMsg->GetVariableStr(VID_COMMENTS);

   m_dwNumActions = pMsg->GetVariableLong(VID_NUM_ACTIONS);
   m_pdwActionList = (UINT32 *)malloc(sizeof(UINT32) * m_dwNumActions);
   pMsg->getFieldAsInt32Array(VID_RULE_ACTIONS, m_dwNumActions, m_pdwActionList);

   m_dwNumEvents = pMsg->GetVariableLong(VID_NUM_EVENTS);
   m_pdwEventList = (UINT32 *)malloc(sizeof(UINT32) * m_dwNumEvents);
   pMsg->getFieldAsInt32Array(VID_RULE_EVENTS, m_dwNumEvents, m_pdwEventList);

   m_dwNumSources = pMsg->GetVariableLong(VID_NUM_SOURCES);
   m_pdwSourceList = (UINT32 *)malloc(sizeof(UINT32) * m_dwNumSources);
   pMsg->getFieldAsInt32Array(VID_RULE_SOURCES, m_dwNumSources, m_pdwSourceList);

   pMsg->GetVariableStr(VID_ALARM_KEY, m_szAlarmKey, MAX_DB_STRING);
   pMsg->GetVariableStr(VID_ALARM_MESSAGE, m_szAlarmMessage, MAX_DB_STRING);
   m_iAlarmSeverity = pMsg->GetVariableShort(VID_ALARM_SEVERITY);
	m_dwAlarmTimeout = pMsg->GetVariableLong(VID_ALARM_TIMEOUT);
	m_dwAlarmTimeoutEvent = pMsg->GetVariableLong(VID_ALARM_TIMEOUT_EVENT);

	m_dwSituationId = pMsg->GetVariableLong(VID_SITUATION_ID);
   pMsg->GetVariableStr(VID_SITUATION_INSTANCE, m_szSituationInstance, MAX_DB_STRING);
   count = pMsg->GetVariableLong(VID_SITUATION_NUM_ATTRS);
   for(i = 0, id = VID_SITUATION_ATTR_LIST_BASE; i < count; i++)
   {
   	name = pMsg->GetVariableStr(id++);
   	value = pMsg->GetVariableStr(id++);
   	m_situationAttrList.setPreallocated(name, value);
   }

   m_pszScript = pMsg->GetVariableStr(VID_SCRIPT);
   if ((m_pszScript != NULL) && (*m_pszScript != 0))
   {
      TCHAR szError[256];

      m_pScript = NXSLCompileAndCreateVM(m_pszScript, szError, 256, new NXSL_ServerEnv);
      if (m_pScript != NULL)
      {
      	m_pScript->setGlobalVariable(_T("CUSTOM_MESSAGE"), new NXSL_Value(_T("")));
      }
      else
      {
         nxlog_write(MSG_EPRULE_SCRIPT_COMPILATION_ERROR, EVENTLOG_ERROR_TYPE,
                  "ds", m_dwId, szError);
      }
   }
   else
   {
      m_pScript = NULL;
   }
}

/**
 * Event policy rule destructor
 */
EPRule::~EPRule()
{
   safe_free(m_pdwSourceList);
   safe_free(m_pdwEventList);
   safe_free(m_pdwActionList);
   safe_free(m_pszComment);
   safe_free(m_pszScript);
   delete m_pScript;
}

/**
 * Create management pack record
 */
void EPRule::createNXMPRecord(String &str)
{
   str.addFormattedString(_T("\t\t<rule id=\"%d\">\n")
                          _T("\t\t\t<flags>%d</flags>\n")
                          _T("\t\t\t<alarmMessage>%s</alarmMessage>\n")
                          _T("\t\t\t<alarmKey>%s</alarmKey>\n")
                          _T("\t\t\t<alarmSeverity>%d</alarmSeverity>\n")
                          _T("\t\t\t<alarmTimeout>%d</alarmTimeout>\n")
                          _T("\t\t\t<alarmTimeoutEvent>%d</alarmTimeoutEvent>\n")
                          _T("\t\t\t<situation>%d</situation>\n")
                          _T("\t\t\t<situationInstance>%s</situationInstance>\n")
                          _T("\t\t\t<script>%s</script>\n")
                          _T("\t\t\t<comments>%s</comments>\n")
                          _T("\t\t\t<sources>\n"),
                          m_dwId, m_dwFlags,
                          (const TCHAR *)EscapeStringForXML2(m_szAlarmMessage),
                          (const TCHAR *)EscapeStringForXML2(m_szAlarmKey),
                          m_iAlarmSeverity, m_dwAlarmTimeout, m_dwAlarmTimeoutEvent,
                          m_dwSituationId, (const TCHAR *)EscapeStringForXML2(m_szSituationInstance),
                          (const TCHAR *)EscapeStringForXML2(m_pszScript),
                          (const TCHAR *)EscapeStringForXML2(m_pszComment));

   for(UINT32 i = 0; i < m_dwNumSources; i++)
   {
      NetObj *object = FindObjectById(m_pdwSourceList[i]);
      if (object != NULL)
      {
         uuid_t guid;
         object->getGuid(guid);
         TCHAR guidText[128];
         str.addFormattedString(_T("\t\t\t\t<source id=\"%d\">\n")
                                _T("\t\t\t\t\t<name>%s</name>\n")
                                _T("\t\t\t\t\t<guid>%s</guid>\n")
                                _T("\t\t\t\t\t<class>%d</class>\n")
                                _T("\t\t\t\t</source>\n"),
                                object->Id(),
                                (const TCHAR *)EscapeStringForXML2(object->Name()),
                                uuid_to_string(guid, guidText), object->Type());
      }
   }

   str += _T("\t\t\t</sources>\n\t\t\t<events>\n");

   for(UINT32 i = 0; i < m_dwNumEvents; i++)
   {
      TCHAR eventName[MAX_EVENT_NAME];
      EventNameFromCode(m_pdwEventList[i], eventName);
      str.addFormattedString(_T("\t\t\t\t<event id=\"%d\">\n")
                             _T("\t\t\t\t\t<name>%s</name>\n")
                             _T("\t\t\t\t</event>\n"),
                             m_pdwEventList[i], (const TCHAR *)EscapeStringForXML2(eventName));
   }

   str += _T("\t\t\t</events>\n\t\t\t<actions>\n");

   for(UINT32 i = 0; i < m_dwNumActions; i++)
   {
      str.addFormattedString(_T("\t\t\t\t<action id=\"%d\">\n")
                             _T("\t\t\t\t</action>\n"),
                             m_pdwActionList[i]);
   }

   str += _T("\t\t\t</actions>\n\t\t</rule>\n");
}

/**
 * Check if source object's id match to the rule
 */
bool EPRule::matchSource(UINT32 dwObjectId)
{
   UINT32 i;
   NetObj *pObject;
   bool bMatch = FALSE;

   if (m_dwNumSources == 0)   // No sources in list means "any"
   {
      bMatch = true;
   }
   else
   {
      for(i = 0; i < m_dwNumSources; i++)
         if (m_pdwSourceList[i] == dwObjectId)
         {
            bMatch = true;
            break;
         }
         else
         {
            pObject = FindObjectById(m_pdwSourceList[i]);
            if (pObject != NULL)
            {
               if (pObject->isChild(dwObjectId))
               {
                  bMatch = true;
                  break;
               }
            }
            else
            {
               nxlog_write(MSG_INVALID_EPP_OBJECT, EVENTLOG_ERROR_TYPE, "d", m_pdwSourceList[i]);
            }
         }
   }
   return (m_dwFlags & RF_NEGATED_SOURCE) ? !bMatch : bMatch;
}

/**
 * Check if event's id match to the rule
 */
bool EPRule::matchEvent(UINT32 dwEventCode)
{
   UINT32 i;
   bool bMatch = false;

   if (m_dwNumEvents == 0)   // No sources in list means "any"
   {
      bMatch = true;
   }
   else
   {
      for(i = 0; i < m_dwNumEvents; i++)
         if (m_pdwEventList[i] & GROUP_FLAG_BIT)
         {
            /* TODO: check group membership */
         }
         else
         {
            if (m_pdwEventList[i] == dwEventCode)
            {
               bMatch = true;
               break;
            }
         }
   }
   return (m_dwFlags & RF_NEGATED_EVENTS) ? !bMatch : bMatch;
}

/**
 * Check if event's severity match to the rule
 */
bool EPRule::matchSeverity(UINT32 dwSeverity)
{
   static UINT32 dwSeverityFlag[] = { RF_SEVERITY_INFO, RF_SEVERITY_WARNING,
                                     RF_SEVERITY_MINOR, RF_SEVERITY_MAJOR,
                                     RF_SEVERITY_CRITICAL };
	return (dwSeverityFlag[dwSeverity] & m_dwFlags) ? true : false;
}

/**
 * Check if event match to the script
 */
bool EPRule::matchScript(Event *pEvent)
{
   NXSL_ServerEnv *pEnv;
   NXSL_Value **ppValueList, *pValue;
   NXSL_VariableSystem *pLocals, *pGlobals = NULL;
   bool bRet = true;
   UINT32 i;
	NetObj *pObject;

   if (m_pScript == NULL)
      return true;

   pEnv = new NXSL_ServerEnv;

   // Pass event's parameters as arguments and
   // other information as variables
   ppValueList = (NXSL_Value **)malloc(sizeof(NXSL_Value *) * pEvent->getParametersCount());
   memset(ppValueList, 0, sizeof(NXSL_Value *) * pEvent->getParametersCount());
   for(i = 0; i < pEvent->getParametersCount(); i++)
      ppValueList[i] = new NXSL_Value(pEvent->getParameter(i));

   pLocals = new NXSL_VariableSystem;
   pLocals->create(_T("EVENT_CODE"), new NXSL_Value(pEvent->getCode()));
   pLocals->create(_T("SEVERITY"), new NXSL_Value(pEvent->getSeverity()));
   pLocals->create(_T("SEVERITY_TEXT"), new NXSL_Value(g_szStatusText[pEvent->getSeverity()]));
   pLocals->create(_T("OBJECT_ID"), new NXSL_Value(pEvent->getSourceId()));
   pLocals->create(_T("EVENT_TEXT"), new NXSL_Value((TCHAR *)pEvent->getMessage()));
   pLocals->create(_T("USER_TAG"), new NXSL_Value((TCHAR *)pEvent->getUserTag()));
	pObject = FindObjectById(pEvent->getSourceId());
	if (pObject != NULL)
	{
		if (pObject->Type() == OBJECT_NODE)
			m_pScript->setGlobalVariable(_T("$node"), new NXSL_Value(new NXSL_Object(&g_nxslNodeClass, pObject)));
	}
	m_pScript->setGlobalVariable(_T("$event"), new NXSL_Value(new NXSL_Object(&g_nxslEventClass, pEvent)));
	m_pScript->setGlobalVariable(_T("CUSTOM_MESSAGE"), new NXSL_Value);

   // Run script
   if (m_pScript->run(pEvent->getParametersCount(), ppValueList, pLocals, &pGlobals))
   {
      pValue = m_pScript->getResult();
      if (pValue != NULL)
      {
         bRet = pValue->getValueAsInt32() ? true : false;
         if (bRet)
         {
         	NXSL_Variable *var;

         	var = pGlobals->find(_T("CUSTOM_MESSAGE"));
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
      nxlog_write(MSG_EPRULE_SCRIPT_EXECUTION_ERROR, EVENTLOG_ERROR_TYPE, "ds", m_dwId, m_pScript->getErrorText());
   }
   free(ppValueList);
   delete pGlobals;

   return bRet;
}

/**
 * Check if event match to rule and perform required actions if yes
 * Method will return TRUE if event matched and RF_STOP_PROCESSING flag is set
 */
bool EPRule::processEvent(Event *pEvent)
{
   bool bStopProcessing = false;
   UINT32 i;

   // Check disable flag
   if (!(m_dwFlags & RF_DISABLED))
   {
      // Check if event match
      if (matchSource(pEvent->getSourceId()) && matchEvent(pEvent->getCode()) &&
          matchSeverity(pEvent->getSeverity()) && matchScript(pEvent))
      {
			DbgPrintf(6, _T("Event ") UINT64_FMT _T(" match EPP rule %d"), pEvent->getId(), (int)m_dwId);

         // Generate alarm if requested
         if (m_dwFlags & RF_GENERATE_ALARM)
            generateAlarm(pEvent);

         // Event matched, perform actions
         if (m_dwNumActions > 0)
         {
            TCHAR *alarmMessage = pEvent->expandText(m_szAlarmMessage);
            TCHAR *alarmKey = pEvent->expandText(m_szAlarmKey);
            for(i = 0; i < m_dwNumActions; i++)
               ExecuteAction(m_pdwActionList[i], pEvent, alarmMessage, alarmKey);
            free(alarmMessage);
            free(alarmKey);
         }

			// Update situation of needed
			if (m_dwSituationId != 0)
			{
				Situation *pSituation;
				TCHAR *pszAttr, *pszValue;

				pSituation = FindSituationById(m_dwSituationId);
				if (pSituation != NULL)
				{
					TCHAR *pszText = pEvent->expandText(m_szSituationInstance);
					for(i = 0; i < m_situationAttrList.getSize(); i++)
					{
						pszAttr = pEvent->expandText(m_situationAttrList.getKeyByIndex(i));
						pszValue = pEvent->expandText(m_situationAttrList.getValueByIndex(i));
						pSituation->UpdateSituation(pszText, pszAttr, pszValue);
						free(pszAttr);
						free(pszValue);
					}
					free(pszText);
				}
				else
				{
					DbgPrintf(3, _T("Event Policy: unable to find situation with ID=%d"), m_dwSituationId);
				}
			}

			bStopProcessing = (m_dwFlags & RF_STOP_PROCESSING) ? true : false;
      }
   }

   return bStopProcessing;
}

/**
 * Generate alarm from event
 */
void EPRule::generateAlarm(Event *pEvent)
{
   // Terminate alarms with key == our ack_key
	if ((m_iAlarmSeverity == SEVERITY_RESOLVE) || (m_iAlarmSeverity == SEVERITY_TERMINATE))
	{
		TCHAR *pszAckKey = pEvent->expandText(m_szAlarmKey);
		if (pszAckKey[0] != 0)
			g_alarmMgr.resolveByKey(pszAckKey, (m_dwFlags & RF_TERMINATE_BY_REGEXP) ? true : false, m_iAlarmSeverity == SEVERITY_TERMINATE, pEvent);
		free(pszAckKey);
	}
	else	// Generate new alarm
	{
		g_alarmMgr.newAlarm(m_szAlarmMessage, m_szAlarmKey, ALARM_STATE_OUTSTANDING,
								  (m_iAlarmSeverity == SEVERITY_FROM_EVENT) ? pEvent->getSeverity() : m_iAlarmSeverity,
								  m_dwAlarmTimeout, m_dwAlarmTimeoutEvent, pEvent, 0);
	}
}

/**
 * Load rule from database
 */
bool EPRule::loadFromDB()
{
   DB_RESULT hResult;
   TCHAR szQuery[256], name[MAX_DB_STRING], value[MAX_DB_STRING];
   bool bSuccess = true;
   UINT32 i, count;

   // Load rule's sources
   _sntprintf(szQuery, 256, _T("SELECT object_id FROM policy_source_list WHERE rule_id=%d"), m_dwId);
   hResult = DBSelect(g_hCoreDB, szQuery);
   if (hResult != NULL)
   {
      m_dwNumSources = DBGetNumRows(hResult);
      m_pdwSourceList = (UINT32 *)malloc(sizeof(UINT32) * m_dwNumSources);
      for(i = 0; i < m_dwNumSources; i++)
         m_pdwSourceList[i] = DBGetFieldULong(hResult, i, 0);
      DBFreeResult(hResult);
   }
   else
   {
      bSuccess = false;
   }

   // Load rule's events
   _sntprintf(szQuery, 256, _T("SELECT event_code FROM policy_event_list WHERE rule_id=%d"), m_dwId);
   hResult = DBSelect(g_hCoreDB, szQuery);
   if (hResult != NULL)
   {
      m_dwNumEvents = DBGetNumRows(hResult);
      m_pdwEventList = (UINT32 *)malloc(sizeof(UINT32) * m_dwNumEvents);
      for(i = 0; i < m_dwNumEvents; i++)
         m_pdwEventList[i] = DBGetFieldULong(hResult, i, 0);
      DBFreeResult(hResult);
   }
   else
   {
      bSuccess = false;
   }

   // Load rule's actions
   _sntprintf(szQuery, 256, _T("SELECT action_id FROM policy_action_list WHERE rule_id=%d"), m_dwId);
   hResult = DBSelect(g_hCoreDB, szQuery);
   if (hResult != NULL)
   {
      m_dwNumActions = DBGetNumRows(hResult);
      m_pdwActionList = (UINT32 *)malloc(sizeof(UINT32) * m_dwNumActions);
      for(i = 0; i < m_dwNumActions; i++)
         m_pdwActionList[i] = DBGetFieldULong(hResult, i, 0);
      DBFreeResult(hResult);
   }
   else
   {
      bSuccess = false;
   }

   // Load situation attributes
   _sntprintf(szQuery, 256, _T("SELECT attr_name,attr_value FROM policy_situation_attr_list WHERE rule_id=%d"), m_dwId);
   hResult = DBSelect(g_hCoreDB, szQuery);
   if (hResult != NULL)
   {
      count = DBGetNumRows(hResult);
      for(i = 0; i < count; i++)
      {
         DBGetField(hResult, i, 0, name, MAX_DB_STRING);
         DBGetField(hResult, i, 1, value, MAX_DB_STRING);
         m_situationAttrList.set(name, value);
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
 * Save rule to database
 */
void EPRule::saveToDB(DB_HANDLE hdb)
{
	UINT32 len = (UINT32)(_tcslen(CHECK_NULL(m_pszComment)) + _tcslen(CHECK_NULL(m_pszScript)) + 4096);
   TCHAR *pszQuery = (TCHAR *)malloc(len * sizeof(TCHAR));

   // General attributes
   TCHAR guidText[128];
   _sntprintf(pszQuery, len, _T("INSERT INTO event_policy (rule_id,rule_guid,flags,comments,alarm_message,")
                       _T("alarm_severity,alarm_key,script,alarm_timeout,alarm_timeout_event,")
							  _T("situation_id,situation_instance) ")
                       _T("VALUES (%d,'%s',%d,%s,%s,%d,%s,%s,%d,%d,%d,%s)"),
              m_dwId, uuid_to_string(m_guid, guidText),m_dwFlags, (const TCHAR *)DBPrepareString(hdb, m_pszComment),
				  (const TCHAR *)DBPrepareString(hdb, m_szAlarmMessage), m_iAlarmSeverity,
	           (const TCHAR *)DBPrepareString(hdb, m_szAlarmKey),
	           (const TCHAR *)DBPrepareString(hdb, m_pszScript), m_dwAlarmTimeout, m_dwAlarmTimeoutEvent,
	           m_dwSituationId, (const TCHAR *)DBPrepareString(hdb, m_szSituationInstance));
   DBQuery(hdb, pszQuery);

   // Actions
	UINT32 i;
   for(i = 0; i < m_dwNumActions; i++)
   {
      _sntprintf(pszQuery, len, _T("INSERT INTO policy_action_list (rule_id,action_id) VALUES (%d,%d)"),
                m_dwId, m_pdwActionList[i]);
      DBQuery(hdb, pszQuery);
   }

   // Events
   for(i = 0; i < m_dwNumEvents; i++)
   {
      _sntprintf(pszQuery, len, _T("INSERT INTO policy_event_list (rule_id,event_code) VALUES (%d,%d)"),
                m_dwId, m_pdwEventList[i]);
      DBQuery(hdb, pszQuery);
   }

   // Sources
   for(i = 0; i < m_dwNumSources; i++)
   {
      _sntprintf(pszQuery, len, _T("INSERT INTO policy_source_list (rule_id,object_id) VALUES (%d,%d)"),
                m_dwId, m_pdwSourceList[i]);
      DBQuery(hdb, pszQuery);
   }

	// Situation attributes
	for(i = 0; i < m_situationAttrList.getSize(); i++)
	{
      _sntprintf(pszQuery, len, _T("INSERT INTO policy_situation_attr_list (rule_id,situation_id,attr_name,attr_value) VALUES (%d,%d,%s,%s)"),
                m_dwId, m_dwSituationId,
					 (const TCHAR *)DBPrepareString(hdb, m_situationAttrList.getKeyByIndex(i)),
					 (const TCHAR *)DBPrepareString(hdb, m_situationAttrList.getValueByIndex(i)));
      DBQuery(hdb, pszQuery);
	}

   free(pszQuery);
}

/**
 * Create NXCP message with rule's data
 */
void EPRule::createMessage(CSCPMessage *pMsg)
{
	UINT32 i, id;

   pMsg->SetVariable(VID_FLAGS, m_dwFlags);
   pMsg->SetVariable(VID_RULE_ID, m_dwId);
   pMsg->SetVariable(VID_GUID, m_guid, UUID_LENGTH);
   pMsg->SetVariable(VID_ALARM_SEVERITY, (WORD)m_iAlarmSeverity);
   pMsg->SetVariable(VID_ALARM_KEY, m_szAlarmKey);
   pMsg->SetVariable(VID_ALARM_MESSAGE, m_szAlarmMessage);
   pMsg->SetVariable(VID_ALARM_TIMEOUT, m_dwAlarmTimeout);
   pMsg->SetVariable(VID_ALARM_TIMEOUT_EVENT, m_dwAlarmTimeoutEvent);
   pMsg->SetVariable(VID_COMMENTS, CHECK_NULL_EX(m_pszComment));
   pMsg->SetVariable(VID_NUM_ACTIONS, m_dwNumActions);
   pMsg->setFieldInt32Array(VID_RULE_ACTIONS, m_dwNumActions, m_pdwActionList);
   pMsg->SetVariable(VID_NUM_EVENTS, m_dwNumEvents);
   pMsg->setFieldInt32Array(VID_RULE_EVENTS, m_dwNumEvents, m_pdwEventList);
   pMsg->SetVariable(VID_NUM_SOURCES, m_dwNumSources);
   pMsg->setFieldInt32Array(VID_RULE_SOURCES, m_dwNumSources, m_pdwSourceList);
   pMsg->SetVariable(VID_SCRIPT, CHECK_NULL_EX(m_pszScript));
	pMsg->SetVariable(VID_SITUATION_ID, m_dwSituationId);
	pMsg->SetVariable(VID_SITUATION_INSTANCE, m_szSituationInstance);
	pMsg->SetVariable(VID_SITUATION_NUM_ATTRS, m_situationAttrList.getSize());
	for(i = 0, id = VID_SITUATION_ATTR_LIST_BASE; i < m_situationAttrList.getSize(); i++)
	{
		pMsg->SetVariable(id++, m_situationAttrList.getKeyByIndex(i));
		pMsg->SetVariable(id++, m_situationAttrList.getValueByIndex(i));
	}
}

/**
 * Check if given action is used within rule
 */
bool EPRule::isActionInUse(UINT32 dwActionId)
{
   UINT32 i;

   for(i = 0; i < m_dwNumActions; i++)
      if (m_pdwActionList[i] == dwActionId)
         return true;
   return false;
}

/**
 * Event processing policy constructor
 */
EventPolicy::EventPolicy()
{
   m_dwNumRules = 0;
   m_ppRuleList = NULL;
   m_rwlock = RWLockCreate();
}

/**
 * Event processing policy destructor
 */
EventPolicy::~EventPolicy()
{
   clear();
   RWLockDestroy(m_rwlock);
}

/**
 * Clear existing policy
 */
void EventPolicy::clear()
{
   UINT32 i;

   for(i = 0; i < m_dwNumRules; i++)
      delete m_ppRuleList[i];
   safe_free(m_ppRuleList);
   m_ppRuleList = NULL;
}

/**
 * Load event processing policy from database
 */
bool EventPolicy::loadFromDB()
{
   DB_RESULT hResult;
   bool bSuccess = false;

   hResult = DBSelect(g_hCoreDB, _T("SELECT rule_id,rule_guid,flags,comments,alarm_message,")
                                 _T("alarm_severity,alarm_key,script,alarm_timeout,alarm_timeout_event,")
											_T("situation_id,situation_instance ")
                                 _T("FROM event_policy ORDER BY rule_id"));
   if (hResult != NULL)
   {
      UINT32 i;

      bSuccess = true;
      m_dwNumRules = DBGetNumRows(hResult);
      m_ppRuleList = (EPRule **)malloc(sizeof(EPRule *) * m_dwNumRules);
      for(i = 0; i < m_dwNumRules; i++)
      {
         m_ppRuleList[i] = new EPRule(hResult, i);
         bSuccess = bSuccess && m_ppRuleList[i]->loadFromDB();
      }
      DBFreeResult(hResult);
   }

   return bSuccess;
}

/**
 * Save event processing policy to database
 */
void EventPolicy::saveToDB()
{
   UINT32 i;

	DB_HANDLE hdb = DBConnectionPoolAcquireConnection();

   readLock();
   DBBegin(hdb);
   DBQuery(hdb, _T("DELETE FROM event_policy"));
   DBQuery(hdb, _T("DELETE FROM policy_action_list"));
   DBQuery(hdb, _T("DELETE FROM policy_event_list"));
   DBQuery(hdb, _T("DELETE FROM policy_source_list"));
   DBQuery(hdb, _T("DELETE FROM policy_situation_attr_list"));
   for(i = 0; i < m_dwNumRules; i++)
      m_ppRuleList[i]->saveToDB(hdb);
   DBCommit(hdb);
   unlock();
	DBConnectionPoolReleaseConnection(hdb);
}

/**
 * Pass event through policy
 */
void EventPolicy::processEvent(Event *pEvent)
{
   UINT32 i;

	DbgPrintf(7, _T("EPP: processing event ") UINT64_FMT, pEvent->getId());
   readLock();
   for(i = 0; i < m_dwNumRules; i++)
      if (m_ppRuleList[i]->processEvent(pEvent))
		{
			DbgPrintf(7, _T("EPP: got \"stop processing\" flag for event ") UINT64_FMT _T(" at rule %d"), pEvent->getId(), i + 1);
         break;   // EPRule::ProcessEvent() return TRUE if we should stop processing this event
		}
   unlock();
}

/**
 * Send event policy to client
 */
void EventPolicy::sendToClient(ClientSession *pSession, UINT32 dwRqId)
{
   UINT32 i;
   CSCPMessage msg;

   readLock();
   msg.SetCode(CMD_EPP_RECORD);
   msg.SetId(dwRqId);
   for(i = 0; i < m_dwNumRules; i++)
   {
      m_ppRuleList[i]->createMessage(&msg);
      pSession->sendMessage(&msg);
      msg.deleteAllVariables();
   }
   unlock();
}

/**
 * Replace policy with new one
 */
void EventPolicy::replacePolicy(UINT32 dwNumRules, EPRule **ppRuleList)
{
   UINT32 i;

   writeLock();

   // Replace rule list
   clear();
   m_dwNumRules = dwNumRules;
   m_ppRuleList = ppRuleList;

   // Replace id's in rules
   for(i = 0; i < m_dwNumRules; i++)
      m_ppRuleList[i]->setId(i);

   unlock();
}

/**
 * Check if given action is used in policy
 */
bool EventPolicy::isActionInUse(UINT32 dwActionId)
{
   bool bResult = false;

   readLock();

   for(UINT32 i = 0; i < m_dwNumRules; i++)
      if (m_ppRuleList[i]->isActionInUse(dwActionId))
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
void EventPolicy::exportRule(String &str, uuid_t guid)
{
   readLock();
   for(UINT32 i = 0; i < m_dwNumRules; i++)
   {
      if (!uuid_compare(guid, m_ppRuleList[i]->getGuid()))
      {
         m_ppRuleList[i]->createNXMPRecord(str);
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
   for(UINT32 i = 0; i < m_dwNumRules; i++)
   {
      if (!uuid_compare(rule->getGuid(), m_ppRuleList[i]->getGuid()))
      {
         delete m_ppRuleList[i];
         m_ppRuleList[i] = rule;
         rule->setId(i);
         newRule = false;
         break;
      }
   }

   // Add new rule at the end
   if (newRule)
   {
      rule->setId(m_dwNumRules);
      m_dwNumRules++;
      m_ppRuleList = (EPRule **)realloc(m_ppRuleList, sizeof(EPRule *) * m_dwNumRules);
      m_ppRuleList[m_dwNumRules - 1] = rule;
   }

   unlock();
}
