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
** File: dbwrite.cpp
**
**/

#include "nxcore.h"

/**
 * MAximum supported number of database writers
 */
#define MAX_DB_WRITERS     16

/**
 * Generic DB writer queue
 */
Queue *g_dbWriterQueue = NULL;

/**
 * DCI data (idata_* tables) writer queue
 */
Queue *g_dciDataWriterQueue = NULL;

/**
 * Raw DCI data writer queue
 */
Queue *g_dciRawDataWriterQueue = NULL;

/**
 * Static data
 */
static int m_numWriters = 1;
static THREAD m_hWriteThreadList[MAX_DB_WRITERS];
static THREAD m_hIDataWriterThread;
static THREAD m_hRawDataWriterThread;

/**
 * Put SQL request into queue for later execution
 */
void NXCORE_EXPORTABLE QueueSQLRequest(const TCHAR *query)
{
	DELAYED_SQL_REQUEST *rq = (DELAYED_SQL_REQUEST *)malloc(sizeof(DELAYED_SQL_REQUEST) + (_tcslen(query) + 1) * sizeof(TCHAR));
	rq->query = (TCHAR *)&rq->bindings[0];
	_tcscpy(rq->query, query);
	rq->bindCount = 0;
   g_dbWriterQueue->Put(rq);
	DbgPrintf(8, _T("SQL request queued: %s"), query);
}

/**
 * Put parameterized SQL request into queue for later execution
 */
void NXCORE_EXPORTABLE QueueSQLRequest(const TCHAR *query, int bindCount, int *sqlTypes, const TCHAR **values)
{
	int size = sizeof(DELAYED_SQL_REQUEST) + ((int)_tcslen(query) + 1) * sizeof(TCHAR) + bindCount * sizeof(TCHAR *) + bindCount;
	for(int i = 0; i < bindCount; i++)
		size += ((int)_tcslen(values[i]) + 1) * sizeof(TCHAR) + sizeof(TCHAR *);
	DELAYED_SQL_REQUEST *rq = (DELAYED_SQL_REQUEST *)malloc(size);

	BYTE *base = (BYTE *)&rq->bindings[bindCount];
	int pos = 0;
	int align = sizeof(TCHAR *);

	rq->query = (TCHAR *)base;
	_tcscpy(rq->query, query);
	rq->bindCount = bindCount;
	pos += ((int)_tcslen(query) + 1) * sizeof(TCHAR);

	rq->sqlTypes = &base[pos];
	pos += bindCount;
	if (pos % align != 0)
		pos += align - pos % align;

	for(int i = 0; i < bindCount; i++)
	{
		rq->sqlTypes[i] = (BYTE)sqlTypes[i];
		rq->bindings[i] = (TCHAR *)&base[pos];
		_tcscpy(rq->bindings[i], values[i]);
		pos += ((int)_tcslen(values[i]) + 1) * sizeof(TCHAR);
		if (pos % align != 0)
			pos += align - pos % align;
	}

   g_dbWriterQueue->Put(rq);
	DbgPrintf(8, _T("SQL request queued: %s"), query);
}

/**
 * Queue INSERT request for idata_xxx table
 */
void QueueIDataInsert(time_t timestamp, UINT32 nodeId, UINT32 dciId, const TCHAR *value)
{
	DELAYED_IDATA_INSERT *rq = (DELAYED_IDATA_INSERT *)malloc(sizeof(DELAYED_IDATA_INSERT));
	rq->timestamp = timestamp;
	rq->nodeId = nodeId;
	rq->dciId = dciId;
	nx_strncpy(rq->value, value, MAX_RESULT_LENGTH);
	g_dciDataWriterQueue->Put(rq);
}

/**
 * Queue UPDATE request for raw_dci_values table
 */
void QueueRawDciDataUpdate(time_t timestamp, UINT32 dciId, const TCHAR *rawValue, const TCHAR *transformedValue)
{
	DELAYED_RAW_DATA_UPDATE *rq = (DELAYED_RAW_DATA_UPDATE *)malloc(sizeof(DELAYED_RAW_DATA_UPDATE));
	rq->timestamp = timestamp;
	rq->dciId = dciId;
	nx_strncpy(rq->rawValue, rawValue, MAX_RESULT_LENGTH);
	nx_strncpy(rq->transformedValue, transformedValue, MAX_RESULT_LENGTH);
	g_dciRawDataWriterQueue->Put(rq);
}

/**
 * Database "lazy" write thread
 */
static THREAD_RESULT THREAD_CALL DBWriteThread(void *arg)
{
   DB_HANDLE hdb;

   if (g_flags & AF_ENABLE_MULTIPLE_DB_CONN)
   {
		TCHAR errorText[DBDRV_MAX_ERROR_TEXT];
      hdb = DBConnect(g_dbDriver, g_szDbServer, g_szDbName, g_szDbLogin, g_szDbPassword, g_szDbSchema, errorText);
      if (hdb == NULL)
      {
         nxlog_write(MSG_DB_CONNFAIL, EVENTLOG_ERROR_TYPE, "s", errorText);
         return THREAD_OK;
      }
   }
   else
   {
      hdb = g_hCoreDB;
   }

   while(1)
   {
      DELAYED_SQL_REQUEST *rq = (DELAYED_SQL_REQUEST *)g_dbWriterQueue->GetOrBlock();
      if (rq == INVALID_POINTER_VALUE)   // End-of-job indicator
         break;

		if (rq->bindCount == 0)
		{
			DBQuery(hdb, rq->query);
		}
		else
		{
			DB_STATEMENT hStmt = DBPrepare(hdb, rq->query);
			if (hStmt != NULL)
			{
				for(int i = 0; i < rq->bindCount; i++)
				{
					DBBind(hStmt, i + 1, (int)rq->sqlTypes[i], rq->bindings[i], DB_BIND_STATIC);
				}
				DBExecute(hStmt);
				DBFreeStatement(hStmt);
			}
		}
      free(rq);
   }

   if (g_flags & AF_ENABLE_MULTIPLE_DB_CONN)
   {
      DBDisconnect(hdb);
   }
   return THREAD_OK;
}

/**
 * Database "lazy" write thread for idata_xxx INSERTs
 */
static THREAD_RESULT THREAD_CALL IDataWriteThread(void *arg)
{
   DB_HANDLE hdb;

   if (g_flags & AF_ENABLE_MULTIPLE_DB_CONN)
   {
		TCHAR errorText[DBDRV_MAX_ERROR_TEXT];
      hdb = DBConnect(g_dbDriver, g_szDbServer, g_szDbName, g_szDbLogin, g_szDbPassword, g_szDbSchema, errorText);
      if (hdb == NULL)
      {
         nxlog_write(MSG_DB_CONNFAIL, EVENTLOG_ERROR_TYPE, "s", errorText);
         return THREAD_OK;
      }
   }
   else
   {
      hdb = g_hCoreDB;
   }

   while(1)
   {
		DELAYED_IDATA_INSERT *rq = (DELAYED_IDATA_INSERT *)g_dciDataWriterQueue->GetOrBlock();
      if (rq == INVALID_POINTER_VALUE)   // End-of-job indicator
         break;

		if (DBBegin(hdb))
		{
			int count = 0;
			while(1)
			{
				TCHAR query[256];
				BOOL success;

				_sntprintf(query, 256, _T("INSERT INTO idata_%d (item_id,idata_timestamp,idata_value) VALUES (?,?,?)"), (int)rq->nodeId);
				DB_STATEMENT hStmt = DBPrepare(hdb, query);
				if (hStmt != NULL)
				{
					DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, rq->dciId);
					DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, (INT64)rq->timestamp);
					DBBind(hStmt, 3, DB_SQLTYPE_VARCHAR, rq->value, DB_BIND_STATIC);
					success = DBExecute(hStmt);
					DBFreeStatement(hStmt);
				}
				else
				{
					success = FALSE;
				}
				free(rq);

				count++;
				if (!success || (count > 1000))
					break;

				rq = (DELAYED_IDATA_INSERT *)g_dciDataWriterQueue->Get();
				if (rq == NULL)
					break;
				if (rq == INVALID_POINTER_VALUE)   // End-of-job indicator
					goto stop;
			}
			DBCommit(hdb);
		}
		else
		{
			free(rq);
		}
	}

stop:
   if (g_flags & AF_ENABLE_MULTIPLE_DB_CONN)
   {
      DBDisconnect(hdb);
   }
   return THREAD_OK;
}

/**
 * Database "lazy" write thread for raw_dci_values UPDATEs
 */
static THREAD_RESULT THREAD_CALL RawDataWriteThread(void *arg)
{
   DB_HANDLE hdb;

   if (g_flags & AF_ENABLE_MULTIPLE_DB_CONN)
   {
		TCHAR errorText[DBDRV_MAX_ERROR_TEXT];
      hdb = DBConnect(g_dbDriver, g_szDbServer, g_szDbName, g_szDbLogin, g_szDbPassword, g_szDbSchema, errorText);
      if (hdb == NULL)
      {
         nxlog_write(MSG_DB_CONNFAIL, EVENTLOG_ERROR_TYPE, "s", errorText);
         return THREAD_OK;
      }
   }
   else
   {
      hdb = g_hCoreDB;
   }

   while(1)
   {
		DELAYED_RAW_DATA_UPDATE *rq = (DELAYED_RAW_DATA_UPDATE *)g_dciRawDataWriterQueue->GetOrBlock();
      if (rq == INVALID_POINTER_VALUE)   // End-of-job indicator
         break;

		if (DBBegin(hdb))
		{
			int count = 0;
			while(1)
			{
				BOOL success;
				DB_STATEMENT hStmt = DBPrepare(hdb, _T("UPDATE raw_dci_values SET raw_value=?,transformed_value=?,last_poll_time=? WHERE item_id=?"));
				if (hStmt != NULL)
				{
					DBBind(hStmt, 1, DB_SQLTYPE_VARCHAR, rq->rawValue, DB_BIND_STATIC);
					DBBind(hStmt, 2, DB_SQLTYPE_VARCHAR, rq->transformedValue, DB_BIND_STATIC);
					DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, (INT64)rq->timestamp);
					DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, rq->dciId);
					success = DBExecute(hStmt);
					DBFreeStatement(hStmt);
				}
				else
				{
					success = FALSE;
				}
				free(rq);

				count++;
				if (!success || (count > 1000))
					break;

				rq = (DELAYED_RAW_DATA_UPDATE *)g_dciRawDataWriterQueue->Get();
				if (rq == NULL)
					break;
				if (rq == INVALID_POINTER_VALUE)   // End-of-job indicator
					goto stop;
			}
			DBCommit(hdb);
		}
		else
		{
			free(rq);
		}
	}

stop:
   if (g_flags & AF_ENABLE_MULTIPLE_DB_CONN)
   {
      DBDisconnect(hdb);
   }
   return THREAD_OK;
}

/**
 * Start writer thread
 */
void StartDBWriter()
{
   int i;

   if (g_flags & AF_ENABLE_MULTIPLE_DB_CONN)
   {
      m_numWriters = ConfigReadInt(_T("NumberOfDatabaseWriters"), 1);
      if (m_numWriters < 1)
         m_numWriters = 1;
      if (m_numWriters > MAX_DB_WRITERS)
         m_numWriters = MAX_DB_WRITERS;
   }

   for(i = 0; i < m_numWriters; i++)
      m_hWriteThreadList[i] = ThreadCreateEx(DBWriteThread, 0, NULL);

	m_hIDataWriterThread = ThreadCreateEx(IDataWriteThread, 0, NULL);
	m_hRawDataWriterThread = ThreadCreateEx(RawDataWriteThread, 0, NULL);
}

/**
 * Stop writer thread and wait while all queries will be executed
 */
void StopDBWriter()
{
   int i;

   for(i = 0; i < m_numWriters; i++)
      g_dbWriterQueue->Put(INVALID_POINTER_VALUE);
   for(i = 0; i < m_numWriters; i++)
      ThreadJoin(m_hWriteThreadList[i]);

	g_dciDataWriterQueue->Put(INVALID_POINTER_VALUE);
	g_dciRawDataWriterQueue->Put(INVALID_POINTER_VALUE);
	ThreadJoin(m_hIDataWriterThread);
	ThreadJoin(m_hRawDataWriterThread);
}
