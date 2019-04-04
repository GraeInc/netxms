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
** File: nms_events.h
**
**/

#ifndef _nms_events_h_
#define _nms_events_h_

#include <nxevent.h>
#include <jansson.h>

#include <nxcore_schedule.h>


//
// Constants
//

#define EVENTLOG_MAX_MESSAGE_SIZE   255
#define EVENTLOG_MAX_USERTAG_SIZE   63

/**
 * Event template
 */
class EventTemplate : public RefCountObject
{
private:
   UINT32 m_code;
   int m_severity;
   uuid m_guid;
   TCHAR m_name[MAX_EVENT_NAME];
   UINT32 m_flags;
   TCHAR *m_messageTemplate;
   TCHAR *m_description;

protected:
   virtual ~EventTemplate();

public:
   EventTemplate(DB_RESULT hResult, int row);

   UINT32 getCode() const { return m_code; }
   int getSeverity() const { return m_severity; }
   const uuid& getGuid() const { return m_guid; }
   const TCHAR *getName() const { return m_name; }
   UINT32 getFlags() const { return m_flags; }
   const TCHAR *getMessageTemplate() const { return m_messageTemplate; }
   const TCHAR *getDescription() const { return m_description; }

   json_t *toJson() const;
};

/**
 * Event
 */
class NXCORE_EXPORTABLE Event
{
private:
   UINT64 m_id;
   UINT64 m_rootId;    // Root event id
   UINT32 m_code;
   int m_severity;
   UINT32 m_flags;
   UINT32 m_sourceId;
   UINT32 m_zoneUIN;
   UINT32 m_dciId;
	TCHAR m_name[MAX_EVENT_NAME];
   TCHAR *m_messageText;
   TCHAR *m_messageTemplate;
   time_t m_timeStamp;
	TCHAR *m_userTag;
	TCHAR *m_customMessage;
	Array m_parameters;
	StringList m_parameterNames;

public:
   Event();
   Event(const Event *src);
   Event(const EventTemplate *eventTemplate, UINT32 sourceId, UINT32 dciId, const TCHAR *userTag, const char *format, const TCHAR **names, va_list args);
   ~Event();

   UINT64 getId() const { return m_id; }
   UINT32 getCode() const { return m_code; }
   UINT32 getSeverity() const { return m_severity; }
   UINT32 getFlags() const { return m_flags; }
   UINT32 getSourceId() const { return m_sourceId; }
   UINT32 getZoneUIN() const { return m_zoneUIN; }
   UINT32 getDciId() const { return m_dciId; }
	const TCHAR *getName() const { return m_name; }
   const TCHAR *getMessage() const { return m_messageText; }
   const TCHAR *getUserTag() const { return m_userTag; }
   time_t getTimeStamp() const { return m_timeStamp; }

   void setSeverity(int severity) { m_severity = severity; }

   UINT64 getRootId() const { return m_rootId; }
   void setRootId(UINT64 id) { m_rootId = id; }

   void prepareMessage(NXCPMessage *msg) const;

   void expandMessageText();
   TCHAR *expandText(const TCHAR *textTemplate, const TCHAR *alarmMsg = NULL, const TCHAR *alarmKey = NULL) const;
   static TCHAR *expandText(const Event *event, UINT32 sourceObject, const TCHAR *textTemplate, const TCHAR *alarmMsg, const TCHAR *alarmKey);
   void setMessage(const TCHAR *text) { free(m_messageText); m_messageText = _tcsdup_ex(text); }
   void setUserTag(const TCHAR *text) { free(m_userTag); m_userTag = _tcsdup_ex(text); }

   int getParametersCount() const { return m_parameters.size(); }
   const TCHAR *getParameter(int index) const { return static_cast<TCHAR*>(m_parameters.get(index)); }
   const TCHAR *getParameterName(int index) const { return m_parameterNames.get(index); }
   UINT32 getParameterAsULong(int index) const { const TCHAR *v = static_cast<TCHAR*>(m_parameters.get(index)); return (v != NULL) ? _tcstoul(v, NULL, 0) : 0; }
   UINT64 getParameterAsUInt64(int index) const { const TCHAR *v = static_cast<TCHAR*>(m_parameters.get(index)); return (v != NULL) ? _tcstoull(v, NULL, 0) : 0; }

	const TCHAR *getNamedParameter(const TCHAR *name) const { return getParameter(m_parameterNames.indexOfIgnoreCase(name)); }
   UINT32 getNamedParameterAsULong(const TCHAR *name) const { return getParameterAsULong(m_parameterNames.indexOfIgnoreCase(name)); }
   UINT64 getNamedParameterAsUInt64(const TCHAR *name) const { return getParameterAsUInt64(m_parameterNames.indexOfIgnoreCase(name)); }

	void addParameter(const TCHAR *name, const TCHAR *value);
	void setNamedParameter(const TCHAR *name, const TCHAR *value);
	void setParameter(int index, const TCHAR *name, const TCHAR *value);

   const TCHAR *getCustomMessage() const { return CHECK_NULL_EX(m_customMessage); }
   void setCustomMessage(const TCHAR *message) { free(m_customMessage); m_customMessage = _tcsdup_ex(message); }

   json_t *toJson();
   static Event *createFromJson(json_t *json);
};

/**
 * Transient data for scheduled action execution
 */
class ActionExecutionTransientData : public ScheduledTaskTransientData
{
private:
   Event *m_event;
   Alarm *m_alarm;

public:
   ActionExecutionTransientData(const Event *e, const Alarm *a);
   virtual ~ActionExecutionTransientData();

   const Event *getEvent() const { return m_event; }
   const Alarm *getAlarm() const { return m_alarm; }
};

/**
 * Defines for type of persistent storage action
 */
 #define PSTORAGE_SET      1
 #define PSTORAGE_DELETE   2

/**
 * Action execution
 */
struct ActionExecutionConfiguration
{
   UINT32 actionId;
   UINT32 timerDelay;
   TCHAR *timerKey;

   ActionExecutionConfiguration(UINT32 i, UINT32 d, TCHAR *k)
   {
      actionId = i;
      timerDelay = d;
      timerKey = k;
   }

   ~ActionExecutionConfiguration()
   {
      MemFree(timerKey);
   }
};

/**
 * Event policy rule
 */
class EPRule
{
private:
   UINT32 m_id;
   uuid m_guid;
   UINT32 m_flags;
   IntegerArray<UINT32> m_sources;
   IntegerArray<UINT32> m_events;
   ObjectArray<ActionExecutionConfiguration> m_actions;
   StringList m_timerCancellations;
   TCHAR *m_comments;
   TCHAR *m_scriptSource;
   NXSL_VM *m_script;

   TCHAR m_alarmMessage[MAX_EVENT_MSG_LENGTH];
   int m_alarmSeverity;
   TCHAR m_alarmKey[MAX_DB_STRING];
	UINT32 m_alarmTimeout;
	UINT32 m_alarmTimeoutEvent;
	IntegerArray<UINT32> m_alarmCategoryList;
	StringMap m_pstorageSetActions;
	StringList m_pstorageDeleteActions;

   bool matchSource(UINT32 objectId);
   bool matchEvent(UINT32 eventCode);
   bool matchSeverity(UINT32 severity);
   bool matchScript(Event *event);

   UINT32 generateAlarm(Event *event);

public:
   EPRule(UINT32 id);
   EPRule(DB_RESULT hResult, int row);
   EPRule(NXCPMessage *msg);
   EPRule(ConfigEntry *config);
   ~EPRule();

   UINT32 getId() const { return m_id; }
   const uuid& getGuid() const { return m_guid; }
   void setId(UINT32 newId) { m_id = newId; }
   bool loadFromDB(DB_HANDLE hdb);
	bool saveToDB(DB_HANDLE hdb);
   bool processEvent(Event *pEvent);
   void createMessage(NXCPMessage *pMsg);
   void createNXMPRecord(String &str);
   json_t *toJson() const;

   bool isActionInUse(UINT32 actionId) const;
   bool isCategoryInUse(UINT32 categoryId) const { return m_alarmCategoryList.contains(categoryId); }
};

/**
 * Event policy
 */
class EventPolicy
{
private:
   ObjectArray<EPRule> m_rules;
   RWLOCK m_rwlock;

   void readLock() const { RWLockReadLock(m_rwlock, INFINITE); }
   void writeLock() { RWLockWriteLock(m_rwlock, INFINITE); }
   void unlock() const { RWLockUnlock(m_rwlock); }

public:
   EventPolicy();
   ~EventPolicy();

   UINT32 getNumRules() const { return m_rules.size(); }
   bool loadFromDB();
   bool saveToDB() const;
   void processEvent(Event *pEvent);
   void sendToClient(ClientSession *pSession, UINT32 dwRqId) const;
   void replacePolicy(UINT32 dwNumRules, EPRule **ppRuleList);
   void exportRule(String& str, const uuid& guid) const;
   void importRule(EPRule *rule, bool overwrite);
   void removeRuleCategory (UINT32 categoryId);
   json_t *toJson() const;

   bool isActionInUse(UINT32 actionId) const;
   bool isCategoryInUse(UINT32 categoryId) const;
};

/**
 * Functions
 */
BOOL InitEventSubsystem();
void ShutdownEventSubsystem();
void ReloadEvents();
void DeleteEventTemplateFromList(UINT32 eventCode);
void CreateNXMPEventRecord(String &str, UINT32 eventCode);

void CorrelateEvent(Event *pEvent);
Event *LoadEventFromDatabase(UINT64 eventId);

bool EventNameFromCode(UINT32 eventCode, TCHAR *buffer);
UINT32 NXCORE_EXPORTABLE EventCodeFromName(const TCHAR *name, UINT32 defaultValue = 0);
EventTemplate *FindEventTemplateByCode(UINT32 eventCode);
EventTemplate *FindEventTemplateByName(const TCHAR *pszName);

bool NXCORE_EXPORTABLE PostEvent(UINT32 eventCode, UINT32 sourceId, const char *format, ...);
bool NXCORE_EXPORTABLE PostDciEvent(UINT32 eventCode, UINT32 sourceId, UINT32 dciId, const char *format, ...);
UINT64 NXCORE_EXPORTABLE PostEvent2(UINT32 eventCode, UINT32 sourceId, const char *format, ...);
bool NXCORE_EXPORTABLE PostEventWithNames(UINT32 eventCode, UINT32 sourceId, const char *format, const TCHAR **names, ...);
bool NXCORE_EXPORTABLE PostEventWithNames(UINT32 eventCode, UINT32 sourceId, StringMap *parameters);
bool NXCORE_EXPORTABLE PostDciEventWithNames(UINT32 eventCode, UINT32 sourceId, UINT32 dciId, const char *format, const TCHAR **names, ...);
bool NXCORE_EXPORTABLE PostDciEventWithNames(UINT32 eventCode, UINT32 sourceId, UINT32 dciId, StringMap *parameters);
bool NXCORE_EXPORTABLE PostEventWithTagAndNames(UINT32 eventCode, UINT32 sourceId, const TCHAR *userTag, const char *format, const TCHAR **names, ...);
bool NXCORE_EXPORTABLE PostEventWithTag(UINT32 eventCode, UINT32 sourceId, const TCHAR *userTag, const char *format, ...);
bool NXCORE_EXPORTABLE PostEventEx(Queue *queue, UINT32 eventCode, UINT32 sourceId, const char *format, ...);
void NXCORE_EXPORTABLE ResendEvents(Queue *queue);

const TCHAR NXCORE_EXPORTABLE *GetStatusAsText(int status, bool allCaps);

/**
 * Global variables
 */
extern Queue *g_pEventQueue;
extern EventPolicy *g_pEventPolicy;
extern INT64 g_totalEventsProcessed;

#endif   /* _nms_events_h_ */
