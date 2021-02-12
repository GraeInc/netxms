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
** File: job.cpp
**
**/

#include "nxcore.h"
#include <math.h>



static VolatileCounter s_id = 0;

/**
 * Unregister job
 */
void UnregisterJob(uint32_t jobId);

/**
 * Constructor
 */
ServerJob::ServerJob(const TCHAR *type, const TCHAR *description, const shared_ptr<NetObj>& object, UINT32 userId, bool createOnHold, int retryCount) : m_object(object)
{
   m_id = InterlockedIncrement(&s_id);
	m_userId = userId;
	m_objectId = (object != nullptr) ? object->getId() : 0;
	_tcslcpy(m_type, CHECK_NULL(type), MAX_JOB_NAME_LEN);
	_tcslcpy(m_description, CHECK_NULL_EX(description), MAX_DB_STRING);
	m_status = createOnHold ? JOB_ON_HOLD : JOB_PENDING;
	m_lastStatusChange = time(NULL);
	m_autoCancelDelay = 600;
	m_progress = 0;
	m_failureMessage = NULL;
	m_owningQueue = NULL;
	m_workerThread = INVALID_THREAD_HANDLE;
	m_lastNotification = 0;
	m_notificationLock = MutexCreate();
	m_blockNextJobsOnFailure = false;
	m_retryCount = (retryCount == -1) ? ConfigReadInt(_T("JobRetryCount"), 5) : retryCount;
	m_valid = (m_object != nullptr);
}

/**
 * Destructor
 */
ServerJob::~ServerJob()
{
	UnregisterJob(m_id);

	ThreadJoin(m_workerThread);

	MemFree(m_failureMessage);
	MutexDestroy(m_notificationLock);
}

/**
 * Send notification to clients
 */
void ServerJob::sendNotification(ClientSession *session, void *arg)
{
	ServerJob *job = (ServerJob *)arg;
	if (job->m_object->checkAccessRights(session->getUserId(), OBJECT_ACCESS_READ))
		session->postMessage(&job->m_notificationMessage);
}

/**
 * Notify clients
 */
void ServerJob::notifyClients(bool isStatusChange)
{
	if (m_object == nullptr)
		return;

	time_t t = time(NULL);
	if (!isStatusChange && (t - m_lastNotification < 3))
		return;	// Don't send progress notifications often then every 3 seconds
	m_lastNotification = t;

	MutexLock(m_notificationLock);
	m_notificationMessage.setCode(CMD_JOB_CHANGE_NOTIFICATION);
	fillMessage(&m_notificationMessage);
	EnumerateClientSessions(ServerJob::sendNotification, this);
	MutexUnlock(m_notificationLock);
}

/**
 * Change status
 */
void ServerJob::changeStatus(ServerJobStatus newStatus)
{
	m_status = newStatus;
	m_lastStatusChange = time(NULL);
	notifyClients(true);
}

/**
 * Set owning queue
 */
void ServerJob::setOwningQueue(ServerJobQueue *queue)
{
	m_owningQueue = queue;
	notifyClients(true);
}

/**
 * Update progress
 */
void ServerJob::markProgress(int pctCompleted)
{
	if ((pctCompleted > m_progress) && (pctCompleted <= 100))
	{
		m_progress = pctCompleted;
		notifyClients(false);
	}
}

/**
 *  Worker thread starter
 */
THREAD_RESULT THREAD_CALL ServerJob::WorkerThreadStarter(void *arg)
{
	ServerJob *job = (ServerJob *)arg;
	DbgPrintf(4, _T("Job %d started"), job->m_id);
	ServerJobResult result = job->run();

	switch(result)
	{
      case JOB_RESULT_SUCCESS:
         job->changeStatus(JOB_COMPLETED);
         break;
      case JOB_RESULT_FAILED:
         if (job->m_status == JOB_CANCEL_PENDING)
            job->changeStatus(JOB_CANCELLED);
         else
            job->changeStatus(JOB_FAILED);
         break;
      case JOB_RESULT_RESCHEDULE:
         job->rescheduleExecution();
         job->changeStatus(JOB_FAILED);
         break;
	}

	DbgPrintf(4, _T("Job %d finished, status=%s"), job->m_id, (job->m_status == JOB_COMPLETED) ? _T("COMPLETED") : ((job->m_status == JOB_CANCELLED) ? _T("CANCELLED") : _T("FAILED")));

	ThreadDetach(job->m_workerThread);
   job->m_workerThread = INVALID_THREAD_HANDLE;

	if (job->m_owningQueue != NULL)
		job->m_owningQueue->jobCompleted(job);
	return THREAD_OK;
}

/**
 * Start job
 */
void ServerJob::start()
{
	m_status = JOB_ACTIVE;
	m_workerThread = ThreadCreateEx(WorkerThreadStarter, 0, this);
}

/**
 * Cancel job
 */
bool ServerJob::cancel()
{
	switch(m_status)
	{
		case JOB_COMPLETED:
		case JOB_CANCEL_PENDING:
			return false;
		case JOB_ACTIVE:
			if (!onCancel())
				return false;
			changeStatus(JOB_CANCEL_PENDING);
			return true;
		default:
			changeStatus(JOB_CANCELLED);
			return true;
	}
}

/**
 * Hold job
 */
bool ServerJob::hold()
{
	if (m_status != JOB_PENDING)
	   return false;

	changeStatus(JOB_ON_HOLD);
	return true;
}

/**
 * Unhold job
 */
bool ServerJob::unhold()
{
	if (m_status != JOB_ON_HOLD)
	   return false;

	changeStatus(JOB_PENDING);
	return true;
}

/**
 * Default run (empty)
 */
ServerJobResult ServerJob::run()
{
	return JOB_RESULT_SUCCESS;
}

/**
 * Default cancel handler
 */
bool ServerJob::onCancel()
{
	return false;
}

/**
 * Set failure message
 */
void ServerJob::setFailureMessage(const TCHAR *msg)
{
	MemFree(m_failureMessage);
	m_failureMessage = MemCopyString(msg);
}

/**
 * Set description
 */
void ServerJob::setDescription(const TCHAR *description)
{
	_tcslcpy(m_description, CHECK_NULL_EX(description), MAX_DB_STRING);
}

/**
 * Fill NXCP message with job's data
 */
void ServerJob::fillMessage(NXCPMessage *msg)
{
	msg->setField(VID_JOB_ID, m_id);
	msg->setField(VID_USER_ID, m_userId);
	msg->setField(VID_JOB_TYPE, m_type);
	msg->setField(VID_OBJECT_ID, m_objectId);
	msg->setField(VID_DESCRIPTION, m_description);
	msg->setField(VID_JOB_STATUS, (WORD)m_status);
	msg->setField(VID_JOB_PROGRESS, (WORD)m_progress);
	if (m_status == JOB_FAILED)
		msg->setField(VID_FAILURE_MESSAGE, (m_failureMessage != NULL) ? m_failureMessage : _T("Internal error"));
	else
		msg->setField(VID_FAILURE_MESSAGE, CHECK_NULL_EX(m_failureMessage));
}

/**
 * Get additional info for logging
 */
const TCHAR *ServerJob::getAdditionalInfo()
{
	return _T("");
}

/**
 * Serializes job parameters into TCHAR line separated by ';'
 */
const String ServerJob::serializeParameters()
{
   return String();
}

/**
 * Schedules execution in 10 minutes
 */
void ServerJob::rescheduleExecution()
{
}

/**
 * Returns delay before retry in seconds
 * Each next interval will be twice as bigger as previous one
 */
int ServerJob::getRetryDelay()
{
   return (int)(pow(2.0f, (4 - m_retryCount)) * JOB_RESCHEDULE_OFFSET);
}
