/* 
** NetXMS - Network Management System
** NetXMS Foundation Library
** Copyright (C) 2003-2020 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published
** by the Free Software Foundation; either version 3 of the License, or
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
** File: msgwq.cpp
**
**/

#include "libnetxms.h"
#include <nxcpapi.h>

/** 
 * Interval between checking messages TTL in milliseconds
 */
#define TTL_CHECK_INTERVAL    30000

/**
 * Buffer allocation step
 */
#define ALLOCATION_STEP       16

/**
 * Housekeeper data
 */
Mutex __EXPORT MsgWaitQueue::m_housekeeperLock;
HashMap<uint64_t, MsgWaitQueue> __EXPORT *MsgWaitQueue::m_activeQueues = new HashMap<uint64_t, MsgWaitQueue>(Ownership::False);
Condition __EXPORT MsgWaitQueue::m_shutdownCondition(true);
THREAD __EXPORT MsgWaitQueue::m_housekeeperThread = INVALID_THREAD_HANDLE;

/**
 * Constructor
 */
MsgWaitQueue::MsgWaitQueue()
{
   m_holdTime = 30000;      // Default message TTL is 30 seconds
   m_size = 0;
   m_allocated = 0;
   m_elements = nullptr;
   m_sequence = 1;
#if defined(_WIN32)
   InitializeCriticalSectionAndSpinCount(&m_mutex, 4000);
   memset(m_wakeupEvents, 0, MAX_MSGQUEUE_WAITERS * sizeof(HANDLE));
   m_wakeupEvents[0] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
   memset(m_waiters, 0, MAX_MSGQUEUE_WAITERS);
#elif defined(_USE_GNU_PTH)
   pth_mutex_init(&m_mutex);
   pth_cond_init(&m_wakeupCondition);
#else
   pthread_mutex_init(&m_mutex, nullptr);
   pthread_cond_init(&m_wakeupCondition, nullptr);
#endif

   // register new queue
   m_housekeeperLock.lock();
   if (m_activeQueues != nullptr)
      m_activeQueues->set(CAST_FROM_POINTER(this, uint64_t), this);
   if (m_housekeeperThread == INVALID_THREAD_HANDLE)
   {
      m_housekeeperThread = ThreadCreateEx(MsgWaitQueue::housekeeperThread, 0, nullptr);
   }
   m_housekeeperLock.unlock();
}

/**
 * Destructor
 */
MsgWaitQueue::~MsgWaitQueue()
{
   // unregister queue
   m_housekeeperLock.lock();
   if (m_activeQueues != nullptr)
      m_activeQueues->remove(CAST_FROM_POINTER(this, uint64_t));
   m_housekeeperLock.unlock();

   clear();

#if defined(_WIN32)
   DeleteCriticalSection(&m_mutex);
   for(int i = 0; i < MAX_MSGQUEUE_WAITERS; i++)
      if (m_wakeupEvents[i] != NULL)
         CloseHandle(m_wakeupEvents[i]);
#elif defined(_USE_GNU_PTH)
   // nothing to do if libpth is used
#else
   pthread_mutex_destroy(&m_mutex);
   pthread_cond_destroy(&m_wakeupCondition);
#endif
}

/**
 * Clear queue
 */
void MsgWaitQueue::clear()
{
   lock();

   for(int i = 0; i < m_allocated; i++)
   {
      if (m_elements[i].msg == NULL)
         continue;

      if (m_elements[i].isBinary)
      {
         MemFree(m_elements[i].msg);
      }
      else
      {
         delete (NXCPMessage *)(m_elements[i].msg);
      }
   }
   m_size = 0;
   m_allocated = 0;
   MemFreeAndNull(m_elements);
   unlock();
}

/**
 * Put message into queue
 */
void MsgWaitQueue::put(NXCPMessage *pMsg)
{
   lock();

   int pos;
   if (m_size == m_allocated)
   {
      pos = m_allocated;
      m_allocated += ALLOCATION_STEP;
      m_elements = MemRealloc(m_elements, sizeof(WAIT_QUEUE_ELEMENT) * m_allocated);
      memset(&m_elements[pos], 0, sizeof(WAIT_QUEUE_ELEMENT) * ALLOCATION_STEP);
   }
   else
   {
      for(pos = 0; m_elements[pos].msg != NULL; pos++);
   }
	
   m_elements[pos].code = pMsg->getCode();
   m_elements[pos].isBinary = 0;
   m_elements[pos].id = pMsg->getId();
   m_elements[pos].ttl = m_holdTime;
   m_elements[pos].msg = pMsg;
   m_elements[pos].sequence = m_sequence++;
   m_size++;

#if defined(_WIN32)
   for(int i = 0; i < MAX_MSGQUEUE_WAITERS; i++)
      if (m_waiters[i])
         SetEvent(m_wakeupEvents[i]);
#elif defined(_USE_GNU_PTH)
   pth_cond_notify(&m_wakeupCondition, TRUE);
#else
   pthread_cond_broadcast(&m_wakeupCondition);
#endif

   unlock();
}

/**
 * Put raw message into queue
 */
void MsgWaitQueue::put(NXCP_MESSAGE *pMsg)
{
   lock();

   int pos;
   if (m_size == m_allocated)
   {
      pos = m_allocated;
      m_allocated += ALLOCATION_STEP;
      m_elements = MemRealloc(m_elements, sizeof(WAIT_QUEUE_ELEMENT) * m_allocated);
      memset(&m_elements[pos], 0, sizeof(WAIT_QUEUE_ELEMENT) * ALLOCATION_STEP);
   }
   else
   {
      for(pos = 0; m_elements[pos].msg != NULL; pos++);
   }

   m_elements[pos].code = pMsg->code;
   m_elements[pos].isBinary = 1;
   m_elements[pos].id = pMsg->id;
   m_elements[pos].ttl = m_holdTime;
   m_elements[pos].msg = pMsg;
   m_elements[pos].sequence = m_sequence++;
   m_size++;

#ifdef _WIN32
   for(int i = 0; i < MAX_MSGQUEUE_WAITERS; i++)
      if (m_waiters[i])
         SetEvent(m_wakeupEvents[i]);
#elif defined(_USE_GNU_PTH)
   pth_cond_notify(&m_wakeupCondition, TRUE);
#else
   pthread_cond_broadcast(&m_wakeupCondition);
#endif

   unlock();
}

/**
 * Wait for message with specific code and ID
 * Function return pointer to the message on success or
 * NULL on timeout or error
 */
void *MsgWaitQueue::waitForMessageInternal(UINT16 isBinary, UINT16 wCode, UINT32 dwId, UINT32 dwTimeOut)
{
   lock();

#ifdef _WIN32
   int slot = -1;
#endif

   do
   {
      uint64_t minSeq = _ULL(0xFFFFFFFFFFFFFFFF);
      int index = -1;
      for(int i = 0; i < m_allocated; i++)
		{
         if ((m_elements[i].msg != nullptr) &&
             (m_elements[i].id == dwId) &&
             (m_elements[i].code == wCode) &&
             (m_elements[i].isBinary == isBinary))
         {
            if (m_elements[i].sequence < minSeq)
            {
               minSeq = m_elements[i].sequence;
               index = i;
            }
         }
		}

      if (index != -1)
      {
         void *msg = m_elements[index].msg;
         m_elements[index].msg = nullptr;
         m_size--;
#ifdef _WIN32
         if (slot != -1)
            m_waiters[slot] = 0;    // release waiter slot
#endif
         unlock();
         return msg;
      }

      int64_t startTime = GetCurrentTimeMs();
       
#if defined(_WIN32)
      // Find free slot if needed
      if (slot == -1)
      {
         for(slot = 0; slot < MAX_MSGQUEUE_WAITERS; slot++)
            if (!m_waiters[slot])
            {
               m_waiters[slot] = 1;
               if (m_wakeupEvents[slot] == NULL)
                  m_wakeupEvents[slot] = CreateEvent(NULL, FALSE, FALSE, NULL);
               break;
            }

         if (slot == MAX_MSGQUEUE_WAITERS)
         {
            slot = -1;
         }
      }

      LeaveCriticalSection(&m_mutex);
      if (slot != -1)
         WaitForSingleObject(m_wakeupEvents[slot], dwTimeOut);
      else
         Sleep(50);  // Just sleep if there are no waiter slots (highly unlikely during normal operation)
      EnterCriticalSection(&m_mutex);
#elif HAVE_PTHREAD_COND_RELTIMEDWAIT_NP
	   struct timespec ts;

	   ts.tv_sec = dwTimeOut / 1000;
	   ts.tv_nsec = (dwTimeOut % 1000) * 1000000;
      pthread_cond_reltimedwait_np(&m_wakeupCondition, &m_mutex, &ts);
#elif defined(_USE_GNU_PTH)
      pth_event_t ev = pth_event(PTH_EVENT_TIME, pth_timeout(dwTimeOut / 1000, (dwTimeOut % 1000) * 1000));
      pth_cond_await(&m_wakeupCondition, &m_mutex, ev);
      pth_event_free(ev, PTH_FREE_ALL);
#else
	   struct timeval now;
	   struct timespec ts;

	   gettimeofday(&now, NULL);
	   ts.tv_sec = now.tv_sec + (dwTimeOut / 1000);

	   now.tv_usec += (dwTimeOut % 1000) * 1000;
	   ts.tv_sec += now.tv_usec / 1000000;
	   ts.tv_nsec = (now.tv_usec % 1000000) * 1000;

	   pthread_cond_timedwait(&m_wakeupCondition, &m_mutex, &ts);
#endif   /* _WIN32 */

      UINT32 sleepTime = (UINT32)(GetCurrentTimeMs() - startTime);
      dwTimeOut -= std::min(sleepTime, dwTimeOut);
   } while(dwTimeOut > 0);

#ifdef _WIN32
   if (slot != -1)
      m_waiters[slot] = 0;    // release waiter slot
#endif

   unlock();
   return NULL;
}

/**
 * Housekeeping run
 */
void MsgWaitQueue::housekeeperRun()
{
   lock();
   if (m_size > 0)
   {
      for(int i = 0; i < m_allocated; i++)
	   {
         if (m_elements[i].msg == NULL)
            continue;

         if (m_elements[i].ttl <= TTL_CHECK_INTERVAL)
         {
            if (m_elements[i].isBinary)
            {
               MemFree(m_elements[i].msg);
            }
            else
            {
               delete (NXCPMessage *)(m_elements[i].msg);
            }
            m_elements[i].msg = NULL;
            m_size--;
         }
         else
         {
            m_elements[i].ttl -= TTL_CHECK_INTERVAL;
         }
	   }

      // compact queue if possible
      if ((m_allocated > ALLOCATION_STEP) && (m_size == 0))
      {
         m_allocated = ALLOCATION_STEP;
         MemFree(m_elements);
         m_elements = MemAllocArray<WAIT_QUEUE_ELEMENT>(m_allocated);
      }
   }
   unlock();
}

/**
 * Callback for enumerating active queues
 */
EnumerationCallbackResult MsgWaitQueue::houseKeeperCallback(const uint64_t& key, MsgWaitQueue *queue)
{
   queue->housekeeperRun();
   return _CONTINUE;
}

/**
 * Housekeeper thread
 */
THREAD_RESULT THREAD_CALL MsgWaitQueue::housekeeperThread(void *arg)
{
   ThreadSetName("MsgWaitQueue");
   while(!m_shutdownCondition.wait(TTL_CHECK_INTERVAL))
   {
      m_housekeeperLock.lock();
      m_activeQueues->forEach(MsgWaitQueue::houseKeeperCallback);
      m_housekeeperLock.unlock();
   }
   return THREAD_OK;
}

/**
 * Shutdown message wait queue background tasks
 */
void MsgWaitQueue::shutdown()
{
   m_shutdownCondition.set();
   ThreadJoin(m_housekeeperThread);
   m_housekeeperLock.lock();
   m_housekeeperThread = INVALID_THREAD_HANDLE;
   delete_and_null(m_activeQueues);
   m_housekeeperLock.unlock();
}

/**
 * Diag info callback
 */
EnumerationCallbackResult MsgWaitQueue::diagInfoCallback(const uint64_t& key, MsgWaitQueue *queue, StringBuffer *output)
{
   TCHAR buffer[256];
   _sntprintf(buffer, 256, _T("   %p size=%d holdTime=%d\n"), queue, queue->m_size, queue->m_holdTime);
   output->append(buffer);
   return _CONTINUE;
}

/**
 * Get diagnostic info
 */
StringBuffer MsgWaitQueue::getDiagInfo()
{
   StringBuffer out;
   m_housekeeperLock.lock();
   out.append(m_activeQueues->size());
   out.append(_T(" active queues\nHousekeeper thread state is "));
   out.append((m_housekeeperThread != INVALID_THREAD_HANDLE) ? _T("RUNNING\n") : _T("STOPPED\n"));
   if (m_activeQueues->size() > 0)
   {
      out.append(_T("Active queues:\n"));
      m_activeQueues->forEach(MsgWaitQueue::diagInfoCallback, &out);
   }
   m_housekeeperLock.unlock();
   return out;
}
