/*
** NetXMS - Network Management System
** Copyright (C) 2003-2020 Raden Solutions
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
** File: schedule.cpp
**
**/

#include "nxcore.h"

/**
 * Static fields
 */
static StringObjectMap<SchedulerCallback> s_callbacks(true);
static ObjectArray<ScheduledTask> s_cronSchedules(5, 5, true);
static ObjectArray<ScheduledTask> s_oneTimeSchedules(5, 5, true);
static Condition s_wakeupCondition(false);
static Mutex s_cronScheduleLock;
static Mutex s_oneTimeScheduleLock;

/**
 * Scheduled task execution pool
 */
ThreadPool *g_schedulerThreadPool = NULL;

/**
 * Task handler replacement for missing handlers
 */
static void MissingTaskHandler(const ScheduledTaskParameters *params)
{
}

/**
 * Callback definition for missing task handlers
 */
static SchedulerCallback s_missingTaskHandler(MissingTaskHandler, 0);

/**
 * Constructor for scheduled task transient data
 */
ScheduledTaskTransientData::ScheduledTaskTransientData()
{
}

/**
 * Destructor for scheduled task transient data
 */
ScheduledTaskTransientData::~ScheduledTaskTransientData()
{
}

/**
 * Create recurrent task object
 */
ScheduledTask::ScheduledTask(int id, const TCHAR *taskHandlerId, const TCHAR *schedule,
         ScheduledTaskParameters *parameters, UINT32 flags)
{
   m_id = id;
   m_taskHandlerId = MemCopyString(CHECK_NULL_EX(taskHandlerId));
   m_schedule = MemCopyString(CHECK_NULL_EX(schedule));
   m_parameters = parameters;
   m_executionTime = NEVER;
   m_lastExecution = NEVER;
   m_flags = flags;
}

/**
 * Create one-time execution task object
 */
ScheduledTask::ScheduledTask(int id, const TCHAR *taskHandlerId, time_t executionTime,
         ScheduledTaskParameters *parameters, UINT32 flags)
{
   m_id = id;
   m_taskHandlerId = MemCopyString(CHECK_NULL_EX(taskHandlerId));
   m_schedule = MemCopyString(_T(""));
   m_parameters = parameters;
   m_executionTime = executionTime;
   m_lastExecution = NEVER;
   m_flags = flags;
}

/**
 * Create task object from database record
 * Expected field order: id,taskid,schedule,params,execution_time,last_execution_time,flags,owner,object_id,comments,task_key
 */
ScheduledTask::ScheduledTask(DB_RESULT hResult, int row)
{
   m_id = DBGetFieldULong(hResult, row, 0);
   m_taskHandlerId = DBGetField(hResult, row, 1, NULL, 0);
   m_schedule = DBGetField(hResult, row, 2, NULL, 0);
   m_executionTime = DBGetFieldULong(hResult, row, 4);
   m_lastExecution = DBGetFieldULong(hResult, row, 5);
   m_flags = DBGetFieldULong(hResult, row, 6);

   TCHAR persistentData[1024];
   DBGetField(hResult, row, 3, persistentData, 1024);
   UINT32 userId = DBGetFieldULong(hResult, row, 7);
   UINT32 objectId = DBGetFieldULong(hResult, row, 8);
   TCHAR key[256], comments[256];
   DBGetField(hResult, row, 9, comments, 256);
   DBGetField(hResult, row, 10, key, 256);
   m_parameters = new ScheduledTaskParameters(key, userId, objectId, persistentData, NULL, comments);
}

/**
 * Destructor
 */
ScheduledTask::~ScheduledTask()
{
   MemFree(m_taskHandlerId);
   MemFree(m_schedule);
   delete m_parameters;
}

/**
 * Update task
 */
void ScheduledTask::update(const TCHAR *taskHandlerId, const TCHAR *schedule, ScheduledTaskParameters *parameters, UINT32 flags)
{
   MemFree(m_taskHandlerId);
   m_taskHandlerId = MemCopyString(CHECK_NULL_EX(taskHandlerId));
   MemFree(m_schedule);
   m_schedule = MemCopyString(CHECK_NULL_EX(schedule));
   delete m_parameters;
   m_parameters = parameters;
   m_flags = flags;
}

/**
 * Update task
 */
void ScheduledTask::update(const TCHAR *taskHandlerId, time_t nextExecution, ScheduledTaskParameters *parameters, UINT32 flags)
{
   MemFree(m_taskHandlerId);
   m_taskHandlerId = MemCopyString(CHECK_NULL_EX(taskHandlerId));
   MemFree(m_schedule);
   m_schedule = MemCopyString(_T(""));
   delete m_parameters;
   m_parameters = parameters;
   m_executionTime = nextExecution;
   m_flags = flags;
}

/**
 * Save task to database
 */
void ScheduledTask::saveToDatabase(bool newObject) const
{
   DB_HANDLE db = DBConnectionPoolAcquireConnection();

   DB_STATEMENT hStmt;
   if (newObject)
   {
		hStmt = DBPrepare(db,
                    _T("INSERT INTO scheduled_tasks (taskId,schedule,params,execution_time,")
                    _T("last_execution_time,flags,owner,object_id,comments,task_key,id) VALUES (?,?,?,?,?,?,?,?,?,?,?)"));
   }
   else
   {
      hStmt = DBPrepare(db,
                    _T("UPDATE scheduled_tasks SET taskId=?,schedule=?,params=?,")
                    _T("execution_time=?,last_execution_time=?,flags=?,owner=?,object_id=?,")
                    _T("comments=?,task_key=? WHERE id=?"));
   }

   if (hStmt != NULL)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_VARCHAR, m_taskHandlerId, DB_BIND_STATIC);
      DBBind(hStmt, 2, DB_SQLTYPE_VARCHAR, m_schedule, DB_BIND_STATIC);
      DBBind(hStmt, 3, DB_SQLTYPE_VARCHAR, m_parameters->m_persistentData, DB_BIND_STATIC, 1023);
      DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, (UINT32)m_executionTime);
      DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, (UINT32)m_lastExecution);
      DBBind(hStmt, 6, DB_SQLTYPE_INTEGER, (LONG)m_flags);
      DBBind(hStmt, 7, DB_SQLTYPE_INTEGER, m_parameters->m_userId);
      DBBind(hStmt, 8, DB_SQLTYPE_INTEGER, m_parameters->m_objectId);
      DBBind(hStmt, 9, DB_SQLTYPE_VARCHAR, m_parameters->m_comments, DB_BIND_STATIC, 255);
      DBBind(hStmt, 10, DB_SQLTYPE_VARCHAR, m_parameters->m_taskKey, DB_BIND_STATIC, 255);
      DBBind(hStmt, 11, DB_SQLTYPE_INTEGER, (LONG)m_id);

      DBExecute(hStmt);
      DBFreeStatement(hStmt);
   }
	DBConnectionPoolReleaseConnection(db);
	NotifyClientSessions(NX_NOTIFY_SCHEDULE_UPDATE, 0);
}

/**
 * Scheduled task comparator (used for task sorting)
 */
static int ScheduledTaskComparator(const ScheduledTask **e1, const ScheduledTask **e2)
{
   const ScheduledTask *s1 = *e1;
   const ScheduledTask *s2 = *e2;

   // Executed schedules should go down
   if (s1->checkFlag(SCHEDULED_TASK_COMPLETED) != s2->checkFlag(SCHEDULED_TASK_COMPLETED))
   {
      return s1->checkFlag(SCHEDULED_TASK_COMPLETED) ? 1 : -1;
   }

   // Schedules with execution time 0 should go down, others should be compared
   if (s1->getExecutionTime() == s2->getExecutionTime())
   {
      return 0;
   }

   return (((s1->getExecutionTime() < s2->getExecutionTime()) && (s1->getExecutionTime() != 0)) || (s2->getExecutionTime() == 0)) ? -1 : 1;
}

/**
 * Run scheduled task
 */
void ScheduledTask::run(SchedulerCallback *callback)
{
   bool oneTimeSchedule = !_tcscmp(m_schedule, _T(""));

	NotifyClientSessions(NX_NOTIFY_SCHEDULE_UPDATE, 0);
   callback->m_func(m_parameters);
   setLastExecutionTime(time(NULL));

   if (oneTimeSchedule)
   {
      s_oneTimeScheduleLock.lock();
   }

   removeFlag(SCHEDULED_TASK_RUNNING);
   setFlag(SCHEDULED_TASK_COMPLETED);
   saveToDatabase(false);
   if (oneTimeSchedule)
   {
      s_oneTimeSchedules.sort(ScheduledTaskComparator);
      s_oneTimeScheduleLock.unlock();
   }

   if (oneTimeSchedule && checkFlag(SCHEDULED_TASK_SYSTEM))
      DeleteScheduledTask(m_id, 0, SYSTEM_ACCESS_FULL);
}

/**
 * Fill NXCP message with task data
 */
void ScheduledTask::fillMessage(NXCPMessage *msg) const
{
   msg->setField(VID_SCHEDULED_TASK_ID, m_id);
   msg->setField(VID_TASK_HANDLER, m_taskHandlerId);
   msg->setField(VID_SCHEDULE, m_schedule);
   msg->setField(VID_PARAMETER, m_parameters->m_persistentData);
   msg->setFieldFromTime(VID_EXECUTION_TIME, m_executionTime);
   msg->setFieldFromTime(VID_LAST_EXECUTION_TIME, m_lastExecution);
   msg->setField(VID_FLAGS, (UINT32)m_flags);
   msg->setField(VID_OWNER, m_parameters->m_userId);
   msg->setField(VID_OBJECT_ID, m_parameters->m_objectId);
   msg->setField(VID_COMMENTS, m_parameters->m_comments);
   msg->setField(VID_TASK_KEY, m_parameters->m_taskKey);
}

/**
 * Fill NXCP message with task data
 */
void ScheduledTask::fillMessage(NXCPMessage *msg, UINT32 base) const
{
   msg->setField(base, m_id);
   msg->setField(base + 1, m_taskHandlerId);
   msg->setField(base + 2, m_schedule);
   msg->setField(base + 3, m_parameters->m_persistentData);
   msg->setFieldFromTime(base + 4, m_executionTime);
   msg->setFieldFromTime(base + 5, m_lastExecution);
   msg->setField(base + 6, m_flags);
   msg->setField(base + 7, m_parameters->m_userId);
   msg->setField(base + 8, m_parameters->m_objectId);
   msg->setField(base + 9, m_parameters->m_comments);
   msg->setField(base + 10, m_parameters->m_taskKey);
}

/**
 * Check if user can access this scheduled task
 */
bool ScheduledTask::canAccess(UINT32 userId, UINT64 systemAccess) const
{
   if (userId == 0)
      return true;

   if (systemAccess & SYSTEM_ACCESS_ALL_SCHEDULED_TASKS)
      return true;

   if(systemAccess & SYSTEM_ACCESS_USER_SCHEDULED_TASKS)
      return !checkFlag(SCHEDULED_TASK_SYSTEM);

   if (systemAccess & SYSTEM_ACCESS_OWN_SCHEDULED_TASKS)
      return userId == m_parameters->m_userId;

   return false;
}

/**
 * Function that adds to list task handler function
 */
void RegisterSchedulerTaskHandler(const TCHAR *id, scheduled_action_executor exec, UINT64 accessRight)
{
   s_callbacks.set(id, new SchedulerCallback(exec, accessRight));
   DbgPrintf(6, _T("Registered scheduler task %s"), id);
}

/**
 * Scheduled task creation function
 */
UINT32 NXCORE_EXPORTABLE AddRecurrentScheduledTask(const TCHAR *task, const TCHAR *schedule, const TCHAR *persistentData,
         ScheduledTaskTransientData *transientData, UINT32 owner, UINT32 objectId, UINT64 systemRights,
         const TCHAR *comments, UINT32 flags, const TCHAR *key)
{
   if ((systemRights & (SYSTEM_ACCESS_ALL_SCHEDULED_TASKS | SYSTEM_ACCESS_USER_SCHEDULED_TASKS | SYSTEM_ACCESS_OWN_SCHEDULED_TASKS)) == 0)
      return RCC_ACCESS_DENIED;
   DbgPrintf(7, _T("AddSchedule: Add cron schedule %s, %s, %s"), task, schedule, persistentData);
   s_cronScheduleLock.lock();
   ScheduledTask *sh = new ScheduledTask(CreateUniqueId(IDG_SCHEDULED_TASK), task, schedule,
            new ScheduledTaskParameters(key, owner, objectId, persistentData, transientData, comments), flags);
   sh->saveToDatabase(true);
   s_cronSchedules.add(sh);
   s_cronScheduleLock.unlock();
   return RCC_SUCCESS;
}

/**
 * Create scheduled task only if task with same task ID does not exist
 */
UINT32 NXCORE_EXPORTABLE AddUniqueRecurrentScheduledTask(const TCHAR *task, const TCHAR *schedule, const TCHAR *persistentData,
         ScheduledTaskTransientData *transientData, UINT32 owner, UINT32 objectId, UINT64 systemRights,
         const TCHAR *comments, UINT32 flags, const TCHAR *key)
{
   if (FindScheduledTaskByHandlerId(task) != NULL)
      return RCC_SUCCESS;
   return AddRecurrentScheduledTask(task, schedule, persistentData, transientData, owner, objectId, systemRights, comments, flags, key);
}

/**
 * One time schedule creation function
 */
UINT32 NXCORE_EXPORTABLE AddOneTimeScheduledTask(const TCHAR *task, time_t nextExecutionTime, const TCHAR *persistentData,
         ScheduledTaskTransientData *transientData, UINT32 owner, UINT32 objectId, UINT64 systemRights,
         const TCHAR *comments, UINT32 flags, const TCHAR *key)
{
   if ((systemRights & (SYSTEM_ACCESS_ALL_SCHEDULED_TASKS | SYSTEM_ACCESS_USER_SCHEDULED_TASKS | SYSTEM_ACCESS_OWN_SCHEDULED_TASKS)) == 0)
      return RCC_ACCESS_DENIED;
   DbgPrintf(5, _T("AddOneTimeAction: Add one time schedule %s, %d, %s"), task, nextExecutionTime, persistentData);
   s_oneTimeScheduleLock.lock();
   ScheduledTask *sh = new ScheduledTask(CreateUniqueId(IDG_SCHEDULED_TASK), task, nextExecutionTime,
            new ScheduledTaskParameters(key, owner, objectId, persistentData, transientData, comments), flags);
   sh->saveToDatabase(true);
   s_oneTimeSchedules.add(sh);
   s_oneTimeSchedules.sort(ScheduledTaskComparator);
   s_oneTimeScheduleLock.unlock();
   s_wakeupCondition.set();
   return RCC_SUCCESS;
}

/**
 * Recurrent scheduled task update
 */
UINT32 NXCORE_EXPORTABLE UpdateRecurrentScheduledTask(int id, const TCHAR *task, const TCHAR *schedule, const TCHAR *persistentData,
         ScheduledTaskTransientData *transientData, const TCHAR *comments, UINT32 owner, UINT32 objectId,
         UINT64 systemAccessRights, UINT32 flags, const TCHAR *key)
{
   DbgPrintf(5, _T("UpdateSchedule: update cron schedule %d, %s, %s, %s"), id, task, schedule, persistentData);
   s_cronScheduleLock.lock();
   UINT32 rcc = RCC_SUCCESS;
   bool found = false;
   for (int i = 0; i < s_cronSchedules.size(); i++)
   {
      if (s_cronSchedules.get(i)->getId() == id)
      {
         if (!s_cronSchedules.get(i)->canAccess(owner, systemAccessRights))
         {
            rcc = RCC_ACCESS_DENIED;
            break;
         }
         s_cronSchedules.get(i)->update(task, schedule,
                  new ScheduledTaskParameters(key, owner, objectId, persistentData, transientData, comments), flags);
         s_cronSchedules.get(i)->saveToDatabase(false);
         found = true;
         break;
      }
   }
   s_cronScheduleLock.unlock();

   if (!found)
   {
      //check in different queue and if exists - remove from one and add to another
      ScheduledTask *st = NULL;
      s_oneTimeScheduleLock.lock();
      for(int i = 0; i < s_oneTimeSchedules.size(); i++)
      {
         if (s_oneTimeSchedules.get(i)->getId() == id)
         {
            if (!s_oneTimeSchedules.get(i)->canAccess(owner, systemAccessRights))
            {
               rcc = RCC_ACCESS_DENIED;
               break;
            }
            st = s_oneTimeSchedules.get(i);
            s_oneTimeSchedules.unlink(i);
            s_oneTimeSchedules.sort(ScheduledTaskComparator);
            st->update(task, schedule, new ScheduledTaskParameters(key, owner, objectId, persistentData, transientData, comments), flags);
            st->saveToDatabase(false);
            found = true;
            break;
         }
      }
      s_oneTimeScheduleLock.unlock();
      if(found && st != NULL)
      {
         s_cronScheduleLock.lock();
         s_cronSchedules.add(st);
         s_cronScheduleLock.unlock();
      }
   }

   return rcc;
}

/**
 * One time action update
 */
UINT32 NXCORE_EXPORTABLE UpdateOneTimeScheduledTask(int id, const TCHAR *task, time_t nextExecutionTime, const TCHAR *persistentData,
         ScheduledTaskTransientData *transientData, const TCHAR *comments, UINT32 owner, UINT32 objectId,
         UINT64 systemAccessRights, UINT32 flags, const TCHAR *key)
{
   DbgPrintf(7, _T("UpdateOneTimeAction: update one time schedule %d, %s, %d, %s"), id, task, nextExecutionTime, persistentData);
   bool found = false;
   s_oneTimeScheduleLock.lock();
   UINT32 rcc = RCC_SUCCESS;
   for (int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      if (s_oneTimeSchedules.get(i)->getId() == id)
      {
         if(!s_oneTimeSchedules.get(i)->canAccess(owner, systemAccessRights))
         {
            rcc = RCC_ACCESS_DENIED;
            break;
         }
         s_oneTimeSchedules.get(i)->update(task, nextExecutionTime,
                  new ScheduledTaskParameters(key, owner, objectId, persistentData, transientData, comments), flags);
         s_oneTimeSchedules.get(i)->saveToDatabase(false);
         s_oneTimeSchedules.sort(ScheduledTaskComparator);
         found = true;
         break;
      }
   }
   s_oneTimeScheduleLock.unlock();

   if (!found && (rcc == RCC_SUCCESS))
   {
      //check in different queue and if exists - remove from one and add to another
      ScheduledTask *st = NULL;
      s_cronScheduleLock.lock();
      for (int i = 0; i < s_cronSchedules.size(); i++)
      {
         if (s_cronSchedules.get(i)->getId() == id)
         {
            if (!s_cronSchedules.get(i)->canAccess(owner, systemAccessRights))
            {
               rcc = RCC_ACCESS_DENIED;
               break;
            }
            st = s_cronSchedules.get(i);
            s_cronSchedules.unlink(i);
            st->update(task, nextExecutionTime, new ScheduledTaskParameters(key, owner, objectId, persistentData, transientData, comments), flags);
            st->saveToDatabase(false);
            found = true;
            break;
         }
      }
      s_cronScheduleLock.unlock();

      if (found && st != NULL)
      {
         s_oneTimeScheduleLock.lock();
         s_oneTimeSchedules.add(st);
         s_oneTimeSchedules.sort(ScheduledTaskComparator);
         s_oneTimeScheduleLock.unlock();
      }
   }

   if (found)
      s_wakeupCondition.set();
   return rcc;
}

/**
 * Removes scheduled task from database by id
 */
static void DeleteScheduledTaskFromDB(UINT32 id)
{
   DB_HANDLE db = DBConnectionPoolAcquireConnection();
   TCHAR query[256];
   _sntprintf(query, 256, _T("DELETE FROM scheduled_tasks WHERE id = %d"), id);
   DBQuery(db, query);
	DBConnectionPoolReleaseConnection(db);
	NotifyClientSessions(NX_NOTIFY_SCHEDULE_UPDATE,0);
}

/**
 * Removes scheduled task by id
 */
UINT32 NXCORE_EXPORTABLE DeleteScheduledTask(UINT32 id, UINT32 user, UINT64 systemRights)
{
   DbgPrintf(7, _T("RemoveSchedule: schedule(%d) removed"), id);
   UINT32 rcc = RCC_INVALID_OBJECT_ID;

   s_cronScheduleLock.lock();
   for(int i = 0; i < s_cronSchedules.size(); i++)
   {
      if (s_cronSchedules.get(i)->getId() == id)
      {
         if (!s_cronSchedules.get(i)->canAccess(user, systemRights))
         {
            rcc = RCC_ACCESS_DENIED;
            break;
         }
         s_cronSchedules.remove(i);
         rcc = RCC_SUCCESS;
         break;
      }
   }
   s_cronScheduleLock.unlock();

   s_oneTimeScheduleLock.lock();
   for(int i = 0; i < s_oneTimeSchedules.size() && rcc == RCC_INVALID_OBJECT_ID; i++)
   {
      if (s_oneTimeSchedules.get(i)->getId() == id)
      {
         if (!s_cronSchedules.get(i)->canAccess(user, systemRights))
         {
            rcc = RCC_ACCESS_DENIED;
            break;
         }
         s_oneTimeSchedules.remove(i);
         s_oneTimeSchedules.sort(ScheduledTaskComparator);
         s_wakeupCondition.set();
         rcc = RCC_SUCCESS;
         break;
      }
   }
   s_oneTimeScheduleLock.unlock();

   if (rcc == RCC_SUCCESS)
   {
      DeleteScheduledTaskFromDB(id);
   }

   return rcc;
}

/**
 * Find scheduled task by task handler id
 */
ScheduledTask *FindScheduledTaskByHandlerId(const TCHAR *taskHandlerId)
{
   ScheduledTask *task;
   bool found = false;

   s_cronScheduleLock.lock();
   for (int i = 0; i < s_cronSchedules.size(); i++)
   {
      if (_tcscmp(s_cronSchedules.get(i)->getTaskHandlerId(), taskHandlerId) == 0)
      {
         task = s_cronSchedules.get(i);
         found = true;
         break;
      }
   }
   s_cronScheduleLock.unlock();

   if (found)
      return task;

   s_oneTimeScheduleLock.lock();
   for (int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      if (_tcscmp(s_oneTimeSchedules.get(i)->getTaskHandlerId(), taskHandlerId) == 0)
      {
         task = s_oneTimeSchedules.get(i);
         found = true;
         break;
      }
   }
   s_oneTimeScheduleLock.unlock();

   if (found)
      return task;

   return NULL;
}

/**
 * Delete scheduled task(s) by task handler id
 */
bool NXCORE_EXPORTABLE DeleteScheduledTaskByHandlerId(const TCHAR *taskHandlerId)
{
   IntegerArray<UINT32> deleteList;

   s_oneTimeScheduleLock.lock();
   for (int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      if (!_tcscmp(s_oneTimeSchedules.get(i)->getTaskHandlerId(), taskHandlerId))
      {
         deleteList.add(s_oneTimeSchedules.get(i)->getId());
         s_oneTimeSchedules.remove(i);
         i--;
      }
   }
   if (!deleteList.isEmpty())
      s_oneTimeSchedules.sort(ScheduledTaskComparator);
   s_oneTimeScheduleLock.unlock();

   s_cronScheduleLock.lock();
   for (int i = 0; i < s_cronSchedules.size(); i++)
   {
      if (!_tcscmp(s_cronSchedules.get(i)->getTaskHandlerId(), taskHandlerId))
      {
         deleteList.add(s_cronSchedules.get(i)->getId());
         s_cronSchedules.remove(i);
         i--;
      }
   }
   s_cronScheduleLock.unlock();

   for(int i = 0; i < deleteList.size(); i++)
   {
      DeleteScheduledTaskFromDB(deleteList.get(i));
   }

   return !deleteList.isEmpty();
}

/**
 * Delete scheduled task(s) by task key
 */
bool NXCORE_EXPORTABLE DeleteScheduledTaskByKey(const TCHAR *taskKey)
{
   IntegerArray<UINT32> deleteList;

   s_oneTimeScheduleLock.lock();
   for (int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      const TCHAR *k = s_oneTimeSchedules.get(i)->getTaskKey();
      if ((k != NULL) && !_tcscmp(k, taskKey))
      {
         deleteList.add(s_oneTimeSchedules.get(i)->getId());
         s_oneTimeSchedules.remove(i);
         i--;
      }
   }
   if (!deleteList.isEmpty())
      s_oneTimeSchedules.sort(ScheduledTaskComparator);
   s_oneTimeScheduleLock.unlock();

   s_cronScheduleLock.lock();
   for (int i = 0; i < s_cronSchedules.size(); i++)
   {
      const TCHAR *k = s_cronSchedules.get(i)->getTaskKey();
      if ((k != NULL) && !_tcscmp(k, taskKey))
      {
         deleteList.add(s_cronSchedules.get(i)->getId());
         s_cronSchedules.remove(i);
         i--;
      }
   }
   s_cronScheduleLock.unlock();

   for(int i = 0; i < deleteList.size(); i++)
   {
      DeleteScheduledTaskFromDB(deleteList.get(i));
   }

   return !deleteList.isEmpty();
}

/**
 * Get number of scheduled tasks with given key
 */
int NXCORE_EXPORTABLE CountScheduledTasksByKey(const TCHAR *taskKey)
{
   int count = 0;

   s_oneTimeScheduleLock.lock();
   for (int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      const TCHAR *k = s_oneTimeSchedules.get(i)->getTaskKey();
      if ((k != NULL) && !_tcscmp(k, taskKey))
      {
         count++;
      }
   }
   s_oneTimeScheduleLock.unlock();

   s_cronScheduleLock.lock();
   for (int i = 0; i < s_cronSchedules.size(); i++)
   {
      const TCHAR *k = s_cronSchedules.get(i)->getTaskKey();
      if ((k != NULL) && !_tcscmp(k, taskKey))
      {
         count++;
      }
   }
   s_cronScheduleLock.unlock();

   return count;
}

/**
 * Fills message with scheduled tasks list
 */
void GetScheduledTasks(NXCPMessage *msg, UINT32 userId, UINT64 systemRights)
{
   int scheduleCount = 0;
   int base = VID_SCHEDULE_LIST_BASE;

   s_oneTimeScheduleLock.lock();
   for(int i = 0; i < s_oneTimeSchedules.size(); i++)
   {
      if (s_oneTimeSchedules.get(i)->canAccess(userId, systemRights))
      {
         s_oneTimeSchedules.get(i)->fillMessage(msg, base);
         scheduleCount++;
         base += 100;
      }
   }
   s_oneTimeScheduleLock.unlock();

   s_cronScheduleLock.lock();
   for(int i = 0; i < s_cronSchedules.size(); i++)
   {
      if (s_cronSchedules.get(i)->canAccess(userId, systemRights))
      {
         s_cronSchedules.get(i)->fillMessage(msg, base);
         scheduleCount++;
         base += 100;
      }
   }
   s_cronScheduleLock.unlock();

   msg->setField(VID_SCHEDULE_COUNT, scheduleCount);
}

/**
 * Fills message with task handlers list
 */
void GetSchedulerTaskHandlers(NXCPMessage *msg, UINT64 accessRights)
{
   UINT32 base = VID_CALLBACK_BASE;
   int count = 0;

   StringList *keyList = s_callbacks.keys();
   for(int i = 0; i < keyList->size(); i++)
   {
      if(accessRights & s_callbacks.get(keyList->get(i))->m_accessRight)
      {
         msg->setField(base, keyList->get(i));
         count++;
         base++;
      }
   }
   delete keyList;
   msg->setField(VID_CALLBACK_COUNT, (UINT32)count);
}

/**
 * Creates scheduled task from message
 */
UINT32 CreateScehduledTaskFromMsg(NXCPMessage *request, UINT32 owner, UINT64 systemAccessRights)
{
   TCHAR *taskId = request->getFieldAsString(VID_TASK_HANDLER);
   TCHAR *schedule = NULL;
   time_t nextExecutionTime = 0;
   TCHAR *persistentData = request->getFieldAsString(VID_PARAMETER);
   TCHAR *comments = request->getFieldAsString(VID_COMMENTS);
   int flags = request->getFieldAsInt32(VID_FLAGS);
   int objectId = request->getFieldAsInt32(VID_OBJECT_ID);
   UINT32 result;
   if (request->isFieldExist(VID_SCHEDULE))
   {
      schedule = request->getFieldAsString(VID_SCHEDULE);
      result = AddRecurrentScheduledTask(taskId, schedule, persistentData, NULL, owner, objectId, systemAccessRights, comments, flags);
   }
   else
   {
      nextExecutionTime = request->getFieldAsTime(VID_EXECUTION_TIME);
      result = AddOneTimeScheduledTask(taskId, nextExecutionTime, persistentData, NULL, owner, objectId, systemAccessRights, comments, flags);
   }
   MemFree(taskId);
   MemFree(schedule);
   MemFree(persistentData);
   return result;
}

/**
 * Update scheduled task from message
 */
UINT32 UpdateScheduledTaskFromMsg(NXCPMessage *request,  UINT32 owner, UINT64 systemAccessRights)
{
   UINT32 id = request->getFieldAsInt32(VID_SCHEDULED_TASK_ID);
   TCHAR *taskId = request->getFieldAsString(VID_TASK_HANDLER);
   TCHAR *schedule = NULL;
   time_t nextExecutionTime = 0;
   TCHAR *persistentData = request->getFieldAsString(VID_PARAMETER);
   TCHAR *comments = request->getFieldAsString(VID_COMMENTS);
   UINT32 flags = request->getFieldAsInt32(VID_FLAGS);
   UINT32 objectId = request->getFieldAsInt32(VID_OBJECT_ID);
   UINT32 rcc;
   if(request->isFieldExist(VID_SCHEDULE))
   {
      schedule = request->getFieldAsString(VID_SCHEDULE);
      rcc = UpdateRecurrentScheduledTask(id, taskId, schedule, persistentData, NULL, comments, owner, objectId, systemAccessRights, flags);
   }
   else
   {
      nextExecutionTime = request->getFieldAsTime(VID_EXECUTION_TIME);
      rcc = UpdateOneTimeScheduledTask(id, taskId, nextExecutionTime, persistentData, NULL, comments, owner, objectId, systemAccessRights, flags);
   }
   MemFree(taskId);
   MemFree(schedule);
   MemFree(persistentData);
   MemFree(comments);
   return rcc;
}

/**
 * Thread that checks one time schedules and executes them
 */
static THREAD_RESULT THREAD_CALL AdHocScheduler(void *arg)
{
   ThreadSetName("Scheduler/A");
   UINT32 sleepTime = 1;
   UINT32 watchdogId = WatchdogAddThread(_T("Ad hoc scheduler"), 5);
   nxlog_debug(3, _T("Ad hoc scheduler started"));
   while(true)
   {
      WatchdogStartSleep(watchdogId);
      s_wakeupCondition.wait(sleepTime * 1000);
      WatchdogNotify(watchdogId);

      if (g_flags & AF_SHUTDOWN)
         break;

      sleepTime = 3600;

      s_oneTimeScheduleLock.lock();
      time_t now = time(NULL);
      for(int i = 0; i < s_oneTimeSchedules.size(); i++)
      {
         ScheduledTask *sh = s_oneTimeSchedules.get(i);
         if (sh->isDisabled() || sh->isRunning() || sh->isCompleted())
            continue;

         if (sh->getExecutionTime() == NEVER)
            break;   // there won't be any more schedulable tasks

         // execute all tasks that is expected to execute now
         if (now >= sh->getExecutionTime())
         {
            sh->setFlag(SCHEDULED_TASK_RUNNING);
            nxlog_debug(6, _T("AdHocScheduler: run scheduled task with id = %d, execution time = %d"), sh->getId(), sh->getExecutionTime());

            SchedulerCallback *callback = s_callbacks.get(sh->getTaskHandlerId());
            if (callback == NULL)
            {
               DbgPrintf(3, _T("AdHocScheduler: One time execution function with taskId=\'%s\' not found"), sh->getTaskHandlerId());
               callback = &s_missingTaskHandler;
            }

            ThreadPoolExecute(g_schedulerThreadPool, sh, &ScheduledTask::run, callback);
         }
         else
         {
            time_t diff = sh->getExecutionTime() - now;
            if (diff < (time_t)3600)
               sleepTime = (UINT32)diff;
            break;
         }
      }
      s_oneTimeScheduleLock.unlock();
      nxlog_debug(6, _T("AdHocScheduler: sleeping for %d seconds"), sleepTime);
   }
   nxlog_debug(3, _T("Ad hoc scheduler stopped"));
   return THREAD_OK;
}

/**
 * Wakes up for execution of one time schedule or for recalculation new wake up timestamp
 */
static THREAD_RESULT THREAD_CALL RecurrentScheduler(void *arg)
{
   ThreadSetName("Scheduler/R");
   UINT32 watchdogId = WatchdogAddThread(_T("Recurrent scheduler"), 5);
   nxlog_debug(3, _T("Recurrent scheduler started"));
   do
   {
      WatchdogNotify(watchdogId);
      time_t now = time(NULL);
      struct tm currLocal;
#if HAVE_LOCALTIME_R
      localtime_r(&now, &currLocal);
#else
      memcpy(&currLocal, localtime(&now), sizeof(struct tm));
#endif

      s_cronScheduleLock.lock();
      for(int i = 0; i < s_cronSchedules.size(); i++)
      {
         ScheduledTask *sh = s_cronSchedules.get(i);
         if (sh->isDisabled() || sh->isRunning())
            continue;

         if (MatchSchedule(sh->getSchedule(), &currLocal, now))
         {
            SchedulerCallback *callback = s_callbacks.get(sh->getTaskHandlerId());
            if (callback == NULL)
            {
               DbgPrintf(3, _T("RecurrentScheduler: Cron execution function with taskId=\'%s\' not found"), sh->getTaskHandlerId());
               callback = &s_missingTaskHandler;
            }

            DbgPrintf(7, _T("RecurrentScheduler: run schedule id=\'%d\', schedule=\'%s\'"), sh->getId(), sh->getSchedule());
            sh->setFlag(SCHEDULED_TASK_RUNNING);
            ThreadPoolExecute(g_schedulerThreadPool, sh, &ScheduledTask::run, callback);
         }
      }
      s_cronScheduleLock.unlock();
      WatchdogStartSleep(watchdogId);
   } while(!SleepAndCheckForShutdown(60)); //sleep 1 minute
   nxlog_debug(3, _T("Recurrent scheduler stopped"));
   return THREAD_OK;
}

/**
 * Scheduler thread handles
 */
static THREAD s_oneTimeEventThread = INVALID_THREAD_HANDLE;
static THREAD s_cronSchedulerThread = INVALID_THREAD_HANDLE;

/**
 * Initialize task scheduler - read all schedules form database and start threads for one time and cron schedules
 */
void InitializeTaskScheduler()
{
   g_schedulerThreadPool = ThreadPoolCreate(_T("SCHEDULER"),
            ConfigReadInt(_T("ThreadPool.Scheduler.BaseSize"), 1),
            ConfigReadInt(_T("ThreadPool.Scheduler.MaxSize"), 64));

   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   DB_RESULT hResult = DBSelect(hdb, _T("SELECT id,taskId,schedule,params,execution_time,last_execution_time,flags,owner,object_id,comments,task_key FROM scheduled_tasks"));
   if (hResult != NULL)
   {
      int count = DBGetNumRows(hResult);
      for(int i = 0; i < count; i++)
      {
         ScheduledTask *sh = new ScheduledTask(hResult, i);
         if(!_tcscmp(sh->getSchedule(), _T("")))
         {
            DbgPrintf(7, _T("InitializeTaskScheduler: Add one time schedule %d, %d"), sh->getId(), sh->getExecutionTime());
            s_oneTimeSchedules.add(sh);
         }
         else
         {
            DbgPrintf(7, _T("InitializeTaskScheduler: Add cron schedule %d, %s"), sh->getId(), sh->getSchedule());
            s_cronSchedules.add(sh);
         }
      }
      DBFreeResult(hResult);
   }
   DBConnectionPoolReleaseConnection(hdb);
   s_oneTimeSchedules.sort(ScheduledTaskComparator);

   s_oneTimeEventThread = ThreadCreateEx(AdHocScheduler, 0, NULL);
   s_cronSchedulerThread = ThreadCreateEx(RecurrentScheduler, 0, NULL);
}

/**
 * Stop all scheduler threads and free all memory
 */
void ShutdownTaskScheduler()
{
   if (g_schedulerThreadPool == NULL)
      return;

   s_wakeupCondition.set();
   ThreadJoin(s_oneTimeEventThread);
   ThreadJoin(s_cronSchedulerThread);
   ThreadPoolDestroy(g_schedulerThreadPool);
}
