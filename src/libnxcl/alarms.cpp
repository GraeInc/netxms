/* 
** NetXMS - Network Management System
** Client Library
** Copyright (C) 2003-2013 Victor Kirhenshtein
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
** File: alarms.cpp
**
**/

#include "libnxcl.h"

/**
 * Fill alarm record from message
 */
static void AlarmFromMsg(CSCPMessage *pMsg, NXC_ALARM *pAlarm)
{
   pAlarm->dwAckByUser = pMsg->GetVariableLong(VID_ACK_BY_USER);
   pAlarm->dwResolvedByUser = pMsg->GetVariableLong(VID_RESOLVED_BY_USER);
   pAlarm->dwTermByUser = pMsg->GetVariableLong(VID_TERMINATED_BY_USER);
   pAlarm->qwSourceEventId = pMsg->GetVariableInt64(VID_EVENT_ID);
   pAlarm->dwSourceEventCode = pMsg->GetVariableLong(VID_EVENT_CODE);
   pAlarm->dwSourceObject = pMsg->GetVariableLong(VID_OBJECT_ID);
   pAlarm->dwCreationTime = pMsg->GetVariableLong(VID_CREATION_TIME);
   pAlarm->dwLastChangeTime = pMsg->GetVariableLong(VID_LAST_CHANGE_TIME);
   pMsg->GetVariableStr(VID_ALARM_KEY, pAlarm->szKey, MAX_DB_STRING);
	pMsg->GetVariableStr(VID_ALARM_MESSAGE, pAlarm->szMessage, MAX_EVENT_MSG_LENGTH);
   pAlarm->nState = (BYTE)pMsg->GetVariableShort(VID_STATE);
   pAlarm->nCurrentSeverity = (BYTE)pMsg->GetVariableShort(VID_CURRENT_SEVERITY);
   pAlarm->nOriginalSeverity = (BYTE)pMsg->GetVariableShort(VID_ORIGINAL_SEVERITY);
   pAlarm->dwRepeatCount = pMsg->GetVariableLong(VID_REPEAT_COUNT);
   pAlarm->nHelpDeskState = (BYTE)pMsg->GetVariableShort(VID_HELPDESK_STATE);
   pMsg->GetVariableStr(VID_HELPDESK_REF, pAlarm->szHelpDeskRef, MAX_HELPDESK_REF_LEN);
	pAlarm->dwTimeout = pMsg->GetVariableLong(VID_ALARM_TIMEOUT);
	pAlarm->dwTimeoutEvent = pMsg->GetVariableLong(VID_ALARM_TIMEOUT_EVENT);
	pAlarm->noteCount = pMsg->GetVariableLong(VID_NUM_COMMENTS);
   pAlarm->pUserData = NULL;
}

/**
 * Process CMD_ALARM_UPDATE message
 */
void ProcessAlarmUpdate(NXCL_Session *pSession, CSCPMessage *pMsg)
{
   NXC_ALARM alarm;
   UINT32 dwCode;

   dwCode = pMsg->GetVariableLong(VID_NOTIFICATION_CODE);
   alarm.dwAlarmId = pMsg->GetVariableLong(VID_ALARM_ID);
   AlarmFromMsg(pMsg, &alarm);
   pSession->callEventHandler(NXC_EVENT_NOTIFICATION, dwCode, &alarm);
}

/**
 * Load all alarms from server
 */
UINT32 LIBNXCL_EXPORTABLE NXCLoadAllAlarms(NXC_SESSION hSession, UINT32 *pdwNumAlarms, NXC_ALARM **ppAlarmList)
{
   CSCPMessage msg, *pResponse;
   UINT32 dwRqId, dwRetCode = RCC_SUCCESS, dwNumAlarms = 0, dwAlarmId = 0;
   NXC_ALARM *pList = NULL;

   dwRqId = ((NXCL_Session *)hSession)->CreateRqId();

   msg.SetCode(CMD_GET_ALL_ALARMS);
   msg.SetId(dwRqId);
   ((NXCL_Session *)hSession)->SendMsg(&msg);

   do
   {
      pResponse = ((NXCL_Session *)hSession)->WaitForMessage(CMD_ALARM_DATA, dwRqId);
      if (pResponse != NULL)
      {
         dwAlarmId = pResponse->GetVariableLong(VID_ALARM_ID);
         if (dwAlarmId != 0)  // 0 is end of list indicator
         {
            pList = (NXC_ALARM *)realloc(pList, sizeof(NXC_ALARM) * (dwNumAlarms + 1));
            pList[dwNumAlarms].dwAlarmId = dwAlarmId;
            AlarmFromMsg(pResponse, &pList[dwNumAlarms]);
            dwNumAlarms++;
         }
         delete pResponse;
      }
      else
      {
         dwRetCode = RCC_TIMEOUT;
         dwAlarmId = 0;
      }
   }
   while(dwAlarmId != 0);

   // Destroy results on failure or save on success
   if (dwRetCode == RCC_SUCCESS)
   {
      *ppAlarmList = pList;
      *pdwNumAlarms = dwNumAlarms;
   }
   else
   {
      safe_free(pList);
      *ppAlarmList = NULL;
      *pdwNumAlarms = 0;
   }

   return dwRetCode;
}

/**
 * Acknowledge alarm by ID
 */
UINT32 LIBNXCL_EXPORTABLE NXCAcknowledgeAlarm(NXC_SESSION hSession, UINT32 alarmId)
{
   return NXCAcknowledgeAlarmEx(hSession, alarmId, false, 0);
}

/**
 * Acknowledge alarm by ID
 *
 * @param hSession session handle
 * @param alarmId Identifier of alarm to be acknowledged.
 * @param sticky  if set to true, acknowledged state will be made "sticky" (duplicate alarms with same key will not revert it back to outstanding)
 * @param timeout timeout for sticky acknowledge in seconds (0 for infinite)
 */
UINT32 LIBNXCL_EXPORTABLE NXCAcknowledgeAlarmEx(NXC_SESSION hSession, UINT32 alarmId, bool sticky, UINT32 timeout)
{
   CSCPMessage msg;
   UINT32 dwRqId;

   dwRqId = ((NXCL_Session *)hSession)->CreateRqId();

   msg.SetCode(CMD_ACK_ALARM);
   msg.SetId(dwRqId);
   msg.SetVariable(VID_ALARM_ID, alarmId);
   msg.SetVariable(VID_STICKY_FLAG, (UINT16)(sticky ? 1 : 0));
   msg.SetVariable(VID_TIMESTAMP, timeout);
   ((NXCL_Session *)hSession)->SendMsg(&msg);

   return ((NXCL_Session *)hSession)->WaitForRCC(dwRqId);
}

/**
 * Terminate alarm by ID
 */
UINT32 LIBNXCL_EXPORTABLE NXCTerminateAlarm(NXC_SESSION hSession, UINT32 dwAlarmId)
{
   CSCPMessage msg;
   UINT32 dwRqId;

   dwRqId = ((NXCL_Session *)hSession)->CreateRqId();

   msg.SetCode(CMD_TERMINATE_ALARM);
   msg.SetId(dwRqId);
   msg.SetVariable(VID_ALARM_ID, dwAlarmId);
   ((NXCL_Session *)hSession)->SendMsg(&msg);

   return ((NXCL_Session *)hSession)->WaitForRCC(dwRqId);
}

/**
 * Delete alarm by ID
 */
UINT32 LIBNXCL_EXPORTABLE NXCDeleteAlarm(NXC_SESSION hSession, UINT32 dwAlarmId)
{
   CSCPMessage msg;
   UINT32 dwRqId;

   dwRqId = ((NXCL_Session *)hSession)->CreateRqId();

   msg.SetCode(CMD_DELETE_ALARM);
   msg.SetId(dwRqId);
   msg.SetVariable(VID_ALARM_ID, dwAlarmId);
   ((NXCL_Session *)hSession)->SendMsg(&msg);

   return ((NXCL_Session *)hSession)->WaitForRCC(dwRqId);
}

/**
 * Create helpdesk issue from alarm
 */
UINT32 LIBNXCL_EXPORTABLE NXCOpenHelpdeskIssue(NXC_SESSION hSession, UINT32 dwAlarmId, TCHAR *pszHelpdeskRef)
{
   CSCPMessage msg;
   UINT32 dwRqId, rcc;

   dwRqId = ((NXCL_Session *)hSession)->CreateRqId();

   msg.SetCode(CMD_OPEN_HELPDESK_ISSUE);
   msg.SetId(dwRqId);
   msg.SetVariable(VID_ALARM_ID, dwAlarmId);
   ((NXCL_Session *)hSession)->SendMsg(&msg);

   CSCPMessage *pResponse = ((NXCL_Session *)hSession)->WaitForMessage(CMD_REQUEST_COMPLETED, dwRqId);
   if (pResponse != NULL)
   {
      rcc = pResponse->GetVariableLong(VID_RCC);
      if (rcc == RCC_SUCCESS)
      {
         pszHelpdeskRef[0] = 0;
         pResponse->GetVariableStr(VID_HELPDESK_REF, pszHelpdeskRef, MAX_HELPDESK_REF_LEN);
      }
      delete pResponse;
   }
   else
   {
      rcc = RCC_TIMEOUT;
   }
   return rcc;
}

//
// Format text from alarm data
// Valid format specifiers are following:
//		%a Primary IP address of source object
//		%A Primary host name of source object
//    %c Repeat count
//    %e Event code
//    %E Event name
//    %h Helpdesk state as number
//    %H Helpdesk state as text
//    %i Source object identifier
//    %I Alarm identifier
//    %m Message text
//    %n Source object name
//    %s Severity as number
//    %S Severity as text
//    %x Alarm state as number
//    %X Alarm state as text
//    %% Percent sign
//

TCHAR LIBNXCL_EXPORTABLE *NXCFormatAlarmText(NXC_SESSION session, NXC_ALARM *alarm, TCHAR *format)
{
	static const TCHAR *alarmState[] = { _T("OUTSTANDING"), _T("ACKNOWLEDGED"), _T("TERMINATED") };
	static const TCHAR *helpdeskState[] = { _T("IGNORED"), _T("OPEN"), _T("CLOSED") };
	static const TCHAR *severityText[] = { _T("NORMAL"), _T("WARNING"), _T("MINOR"), _T("MAJOR"), _T("CRITICAL") };

	NXC_OBJECT *object = NXCFindObjectById(session, alarm->dwSourceObject);
	if (object == NULL)
	{
		NXCSyncSingleObject(session, alarm->dwSourceObject);
		object = NXCFindObjectById(session, alarm->dwSourceObject);
	}

	String out;
	TCHAR *prev, *curr, ipAddr[32];
	for(prev = format; *prev != 0; prev = curr)
	{
		curr = _tcschr(prev, _T('%'));
		if (curr == NULL)
		{
			out += prev;
			break;
		}
		out.addString(prev, (UINT32)(curr - prev));
		curr++;
		switch(*curr)
		{
			case '%':
				out += _T("%");
				break;
			case 'a':
				out += (object != NULL) ? IpToStr(object->dwIpAddr, ipAddr) : _T("<unknown>");
				break;
			case 'A':
				out += (object != NULL) ? object->node.szPrimaryName : _T("<unknown>");
				break;
			case 'c':
				out.addFormattedString(_T("%u"), (unsigned int)alarm->dwRepeatCount);
				break;
			case 'e':
				out.addFormattedString(_T("%u"), (unsigned int)alarm->dwSourceEventCode);
				break;
			case 'E':
				out += NXCGetEventName(session, alarm->dwSourceEventCode);
				break;
			case 'h':
				out.addFormattedString(_T("%d"), (int)alarm->nHelpDeskState);
				break;
			case 'H':
				out += helpdeskState[alarm->nHelpDeskState];
				break;
			case 'i':
				out.addFormattedString(_T("%u"), (unsigned int)alarm->dwSourceObject);
				break;
			case 'I':
				out.addFormattedString(_T("%u"), (unsigned int)alarm->dwAlarmId);
				break;
			case 'm':
				out += alarm->szMessage;
				break;
			case 'n':
				out += (object != NULL) ? object->szName : _T("<unknown>");
				break;
			case 's':
				out.addFormattedString(_T("%d"), (int)alarm->nCurrentSeverity);
				break;
			case 'S':
				out += severityText[alarm->nCurrentSeverity];
				break;
			case 'x':
				out.addFormattedString(_T("%d"), (int)alarm->nState);
				break;
			case 'X':
				out += alarmState[alarm->nState];
				break;
			case 0:
				curr--;
				break;
			default:
				break;
		}
		curr++;
	}
	return _tcsdup((const TCHAR *)out);
}
