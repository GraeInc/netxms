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
** File: nms_alarm.h
**
**/

#ifndef _nms_alarm_h_
#define _nms_alarm_h_

/**
 * Alarm manager class
 */
class NXCORE_EXPORTABLE AlarmManager
{
private:
   int m_numAlarms;
   NXC_ALARM *m_pAlarmList;
   MUTEX m_mutex;
   UINT32 m_dwNotifyCode;
   NXC_ALARM *m_pNotifyAlarmInfo;
	CONDITION m_condShutdown;
	THREAD m_hWatchdogThread;

   void lock() { MutexLock(m_mutex); }
   void unlock() { MutexUnlock(m_mutex); }

   static void sendAlarmNotification(ClientSession *pSession, void *pArg);

   void updateAlarmInDB(NXC_ALARM *pAlarm);
   void notifyClients(UINT32 dwCode, NXC_ALARM *pAlarm);
   void updateObjectStatus(UINT32 dwObjectId);

   UINT32 doAck(NXC_ALARM *alarm, ClientSession *session, bool sticky, UINT32 acknowledgmentActionTime);
	UINT32 doUpdateAlarmComment(NXC_ALARM *alarm, UINT32 noteId, const TCHAR *text, UINT32 userId, bool syncWithHelpdesk);

public:
   AlarmManager();
   ~AlarmManager();

	void watchdogThread();

   BOOL init();
   void newAlarm(TCHAR *pszMsg, TCHAR *pszKey, int nState,
	              int iSeverity, UINT32 dwTimeout, UINT32 dwTimeoutEvent, Event *pEvent, UINT32 ackTimeout);
   UINT32 ackById(UINT32 dwAlarmId, ClientSession *session, bool sticky, UINT32 time);
   UINT32 ackByHDRef(const TCHAR *hdref, ClientSession *session, bool sticky, UINT32 time);
   UINT32 resolveById(UINT32 dwAlarmId, ClientSession *session, bool terminate);
   void resolveByKey(const TCHAR *key, bool useRegexp, bool terminate, Event *pEvent);
   UINT32 resolveByHDRef(const TCHAR *hdref, ClientSession *session, bool terminate);
   void deleteAlarm(UINT32 dwAlarmId, bool objectCleanup);
   bool deleteObjectAlarms(UINT32 objectId, DB_HANDLE hdb);
   UINT32 openHelpdeskIssue(UINT32 alarmId, ClientSession *session, TCHAR *hdref);
   UINT32 getHelpdeskIssueUrl(UINT32 alarmId, TCHAR *url, size_t size);
   UINT32 unlinkIssueById(UINT32 dwAlarmId, ClientSession *session);
   UINT32 unlinkIssueByHDRef(const TCHAR *hdref, ClientSession *session);
	UINT32 addAlarmComment(const TCHAR *hdref, const TCHAR *text, UINT32 userId);
	UINT32 updateAlarmComment(UINT32 alarmId, UINT32 noteId, const TCHAR *text, UINT32 userId);
	UINT32 deleteAlarmCommentByID(UINT32 alarmId, UINT32 noteId);

   UINT32 getAlarm(UINT32 dwAlarmId, CSCPMessage *msg);
   UINT32 getAlarmEvents(UINT32 dwAlarmId, CSCPMessage *msg);
   void sendAlarmsToClient(UINT32 dwRqId, ClientSession *pSession);
   void getAlarmStats(CSCPMessage *pMsg);
	UINT32 getAlarmComments(UINT32 alarmId, CSCPMessage *msg);

   NetObj *getAlarmSourceObject(UINT32 dwAlarmId);
   NetObj *getAlarmSourceObject(const TCHAR *hdref);
   int getMostCriticalStatusForObject(UINT32 dwObjectId);

   ObjectArray<NXC_ALARM> *getAlarms(UINT32 objectId);
};

/**
 * Functions
 */
void FillAlarmInfoMessage(CSCPMessage *pMsg, NXC_ALARM *pAlarm);
void DeleteAlarmNotes(DB_HANDLE hdb, UINT32 alarmId);
void DeleteAlarmEvents(DB_HANDLE hdb, UINT32 alarmId);
UINT32 ResolveAlarmByHDRef(const TCHAR *hdref);
UINT32 TerminateAlarmByHDRef(const TCHAR *hdref);
void LoadHelpDeskLink();
UINT32 CreateHelpdeskIssue(const TCHAR *description, TCHAR *hdref);
UINT32 AddHelpdeskIssueComment(const TCHAR *hdref, const TCHAR *text);
UINT32 GetHelpdeskIssueUrl(const TCHAR *hdref, TCHAR *url, size_t size);

/**
 * Global instance of alarm manager
 */
extern AlarmManager NXCORE_EXPORTABLE g_alarmMgr;

#endif
