/*
** NetXMS - Network Management System
** Database Abstraction Library
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
** File: session.cpp
**
**/

#include "libnxdb.h"

/**
 * Check if statement handle is valid
 */
#define IS_VALID_STATEMENT_HANDLE(s) ((s != NULL) && (s->m_connection != NULL))

/**
 * Invalidate all prepared statements on connection
 */
static void InvalidatePreparedStatements(DB_HANDLE hConn)
{
   for(int i = 0; i < hConn->m_preparedStatements->size(); i++)
   {
      db_statement_t *stmt = hConn->m_preparedStatements->get(i);
      hConn->m_driver->m_fpDrvFreeStatement(stmt->m_statement);
      stmt->m_statement = NULL;
      stmt->m_connection = NULL;
   }
   hConn->m_preparedStatements->clear();
}

/**
 * Connect to database
 */
DB_HANDLE LIBNXDB_EXPORTABLE DBConnect(DB_DRIVER driver, const TCHAR *server, const TCHAR *dbName,
                                       const TCHAR *login, const TCHAR *password, const TCHAR *schema, TCHAR *errorText)
{
   DBDRV_CONNECTION hDrvConn;
   DB_HANDLE hConn = NULL;

	__DBDbgPrintf(8, _T("DBConnect: server=%s db=%s login=%s schema=%s"), CHECK_NULL(server), CHECK_NULL(dbName), CHECK_NULL(login), CHECK_NULL(schema));
#ifdef UNICODE
	char *mbServer = (server == NULL) ? NULL : MBStringFromWideString(server);
	char *mbDatabase = (dbName == NULL) ? NULL : MBStringFromWideString(dbName);
	char *mbLogin = (login == NULL) ? NULL : MBStringFromWideString(login);
	char *mbPassword = (password == NULL) ? NULL : MBStringFromWideString(password);
	char *mbSchema = (schema == NULL) ? NULL : MBStringFromWideString(schema);
	errorText[0] = 0;
   hDrvConn = driver->m_fpDrvConnect(mbServer, mbLogin, mbPassword, mbDatabase, mbSchema, errorText);
#else
	WCHAR wcErrorText[DBDRV_MAX_ERROR_TEXT] = L"";
   hDrvConn = driver->m_fpDrvConnect(server, login, password, dbName, schema, wcErrorText);
	WideCharToMultiByte(CP_ACP, WC_DEFAULTCHAR | WC_COMPOSITECHECK, wcErrorText, -1, errorText, DBDRV_MAX_ERROR_TEXT, NULL, NULL);
	errorText[DBDRV_MAX_ERROR_TEXT - 1] = 0;
#endif
   if (hDrvConn != NULL)
   {
      hConn = (DB_HANDLE)malloc(sizeof(struct db_handle_t));
      if (hConn != NULL)
      {
			hConn->m_driver = driver;
			hConn->m_dumpSql = driver->m_dumpSql;
			hConn->m_reconnectEnabled = true;
			hConn->m_connection = hDrvConn;
         hConn->m_mutexTransLock = MutexCreateRecursive();
         hConn->m_transactionLevel = 0;
         hConn->m_preparedStatements = new ObjectArray<db_statement_t>(4, 4, false);
#ifdef UNICODE
         hConn->m_dbName = mbDatabase;
         hConn->m_login = mbLogin;
         hConn->m_password = mbPassword;
         hConn->m_server = mbServer;
         hConn->m_schema = mbSchema;
#else
         hConn->m_dbName = (dbName == NULL) ? NULL : _tcsdup(dbName);
         hConn->m_login = (login == NULL) ? NULL : _tcsdup(login);
         hConn->m_password = (password == NULL) ? NULL : _tcsdup(password);
         hConn->m_server = (server == NULL) ? NULL : _tcsdup(server);
         hConn->m_schema = (schema == NULL) ? NULL : _tcsdup(schema);
#endif
		   __DBDbgPrintf(4, _T("New DB connection opened: handle=%p"), hConn);
      }
      else
      {
         driver->m_fpDrvDisconnect(hDrvConn);
      }
   }
#ifdef UNICODE
	if (hConn == NULL)
	{
		safe_free(mbServer);
		safe_free(mbDatabase);
		safe_free(mbLogin);
		safe_free(mbPassword);
		safe_free(mbSchema);
	}
#endif
   return hConn;
}

/**
 * Disconnect from database
 */
void LIBNXDB_EXPORTABLE DBDisconnect(DB_HANDLE hConn)
{
   if (hConn == NULL)
      return;

   __DBDbgPrintf(4, _T("DB connection %p closed"), hConn);
   
   InvalidatePreparedStatements(hConn);

	hConn->m_driver->m_fpDrvDisconnect(hConn->m_connection);
   MutexDestroy(hConn->m_mutexTransLock);
   safe_free(hConn->m_dbName);
   safe_free(hConn->m_login);
   safe_free(hConn->m_password);
   safe_free(hConn->m_server);
   safe_free(hConn->m_schema);
   delete hConn->m_preparedStatements;
   free(hConn);
}

/**
 * Enable/disable reconnect
 */
void LIBNXDB_EXPORTABLE DBEnableReconnect(DB_HANDLE hConn, bool enabled)
{
	if (hConn != NULL)
	{
		hConn->m_reconnectEnabled = enabled;
	}
}

/**
 * Reconnect to database
 */
static void DBReconnect(DB_HANDLE hConn)
{
   int nCount;
	WCHAR errorText[DBDRV_MAX_ERROR_TEXT];

   __DBDbgPrintf(4, _T("DB reconnect: handle=%p"), hConn);

   InvalidatePreparedStatements(hConn);
	hConn->m_driver->m_fpDrvDisconnect(hConn->m_connection);
   for(nCount = 0; ; nCount++)
   {
		hConn->m_connection = hConn->m_driver->m_fpDrvConnect(hConn->m_server, hConn->m_login,
                                                            hConn->m_password, hConn->m_dbName, hConn->m_schema, errorText);
      if (hConn->m_connection != NULL)
         break;
      if (nCount == 0)
      {
			MutexLock(hConn->m_driver->m_mutexReconnect);
         if ((hConn->m_driver->m_reconnect == 0) && (hConn->m_driver->m_fpEventHandler != NULL))
				hConn->m_driver->m_fpEventHandler(DBEVENT_CONNECTION_LOST, NULL, NULL, hConn->m_driver->m_userArg);
         hConn->m_driver->m_reconnect++;
         MutexUnlock(hConn->m_driver->m_mutexReconnect);
      }
      ThreadSleepMs(1000);
   }
   if (nCount > 0)
   {
      MutexLock(hConn->m_driver->m_mutexReconnect);
      hConn->m_driver->m_reconnect--;
      if ((hConn->m_driver->m_reconnect == 0) && (hConn->m_driver->m_fpEventHandler != NULL))
			hConn->m_driver->m_fpEventHandler(DBEVENT_CONNECTION_RESTORED, NULL, NULL, hConn->m_driver->m_userArg);
      MutexUnlock(hConn->m_driver->m_mutexReconnect);
   }
}

/**
 * Perform a non-SELECT SQL query
 */
BOOL LIBNXDB_EXPORTABLE DBQueryEx(DB_HANDLE hConn, const TCHAR *szQuery, TCHAR *errorText)
{
   DWORD dwResult;
   INT64 ms;
#ifdef UNICODE
#define pwszQuery szQuery
#define wcErrorText errorText
#else
   WCHAR *pwszQuery = WideStringFromMBString(szQuery);
	WCHAR wcErrorText[DBDRV_MAX_ERROR_TEXT] = L"";
#endif

   MutexLock(hConn->m_mutexTransLock);
   if (hConn->m_driver->m_dumpSql)
      ms = GetCurrentTimeMs();

   dwResult = hConn->m_driver->m_fpDrvQuery(hConn->m_connection, pwszQuery, wcErrorText);
   if ((dwResult == DBERR_CONNECTION_LOST) && hConn->m_reconnectEnabled)
   {
      DBReconnect(hConn);
      dwResult = hConn->m_driver->m_fpDrvQuery(hConn->m_connection, pwszQuery, wcErrorText);
   }

   if (hConn->m_driver->m_dumpSql)
   {
      ms = GetCurrentTimeMs() - ms;
      __DBDbgPrintf(9, _T("%s sync query: \"%s\" [%d ms]"), (dwResult == DBERR_SUCCESS) ? _T("Successful") : _T("Failed"), szQuery, ms);
   }
   
   MutexUnlock(hConn->m_mutexTransLock);

#ifndef UNICODE
	WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, wcErrorText, -1, errorText, DBDRV_MAX_ERROR_TEXT, NULL, NULL);
	errorText[DBDRV_MAX_ERROR_TEXT - 1] = 0;
#endif

   if (dwResult != DBERR_SUCCESS)
	{	
		if (hConn->m_driver->m_logSqlErrors)
			nxlog_write(g_sqlErrorMsgCode, EVENTLOG_ERROR_TYPE, "ss", szQuery, errorText);
		if (hConn->m_driver->m_fpEventHandler != NULL)
			hConn->m_driver->m_fpEventHandler(DBEVENT_QUERY_FAILED, pwszQuery, wcErrorText, hConn->m_driver->m_userArg);
	}

#ifndef UNICODE
   free(pwszQuery);
#endif
   
   return dwResult == DBERR_SUCCESS;
#undef pwszQuery
#undef wcErrorText
}

BOOL LIBNXDB_EXPORTABLE DBQuery(DB_HANDLE hConn, const TCHAR *query)
{
   TCHAR errorText[DBDRV_MAX_ERROR_TEXT];

	return DBQueryEx(hConn, query, errorText);
}

/**
 * Perform SELECT query
 */
DB_RESULT LIBNXDB_EXPORTABLE DBSelectEx(DB_HANDLE hConn, const TCHAR *szQuery, TCHAR *errorText)
{
   DBDRV_RESULT hResult;
	DB_RESULT result = NULL;
   DWORD dwError = DBERR_OTHER_ERROR;
   INT64 ms;
#ifdef UNICODE
#define pwszQuery szQuery
#define wcErrorText errorText
#else
   WCHAR *pwszQuery = WideStringFromMBString(szQuery);
	WCHAR wcErrorText[DBDRV_MAX_ERROR_TEXT] = L"";
#endif
   
   MutexLock(hConn->m_mutexTransLock);
   if (hConn->m_driver->m_dumpSql)
      ms = GetCurrentTimeMs();
   hResult = hConn->m_driver->m_fpDrvSelect(hConn->m_connection, pwszQuery, &dwError, wcErrorText);
   if ((hResult == NULL) && (dwError == DBERR_CONNECTION_LOST) && hConn->m_reconnectEnabled)
   {
      DBReconnect(hConn);
      hResult = hConn->m_driver->m_fpDrvSelect(hConn->m_connection, pwszQuery, &dwError, wcErrorText);
   }

   if (hConn->m_driver->m_dumpSql)
   {
      ms = GetCurrentTimeMs() - ms;
      __DBDbgPrintf(9, _T("%s sync query: \"%s\" [%d ms]"), (hResult != NULL) ? _T("Successful") : _T("Failed"), szQuery, (DWORD)ms);
   }
   MutexUnlock(hConn->m_mutexTransLock);

#ifndef UNICODE
	WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, wcErrorText, -1, errorText, DBDRV_MAX_ERROR_TEXT, NULL, NULL);
	errorText[DBDRV_MAX_ERROR_TEXT - 1] = 0;
#endif

	if (hResult == NULL)
	{
		if (hConn->m_driver->m_logSqlErrors)
			nxlog_write(g_sqlErrorMsgCode, EVENTLOG_ERROR_TYPE, "ss", szQuery, errorText);
		if (hConn->m_driver->m_fpEventHandler != NULL)
			hConn->m_driver->m_fpEventHandler(DBEVENT_QUERY_FAILED, pwszQuery, wcErrorText, hConn->m_driver->m_userArg);
	}

#ifndef UNICODE
   free(pwszQuery);
#endif
   
	if (hResult != NULL)
	{
		result = (DB_RESULT)malloc(sizeof(db_result_t));
		result->m_driver = hConn->m_driver;
		result->m_connection = hConn;
		result->m_data = hResult;
	}

   return result;
#undef pwszQuery
#undef wcErrorText
}

DB_RESULT LIBNXDB_EXPORTABLE DBSelect(DB_HANDLE hConn, const TCHAR *query)
{
   TCHAR errorText[DBDRV_MAX_ERROR_TEXT];

	return DBSelectEx(hConn, query, errorText);
}

/**
 * Get number of columns
 */
int LIBNXDB_EXPORTABLE DBGetColumnCount(DB_RESULT hResult)
{
	return hResult->m_driver->m_fpDrvGetColumnCount(hResult->m_data);
}

/**
 * Get column name
 */
BOOL LIBNXDB_EXPORTABLE DBGetColumnName(DB_RESULT hResult, int column, TCHAR *buffer, int bufSize)
{
	const char *name;

	name = hResult->m_driver->m_fpDrvGetColumnName(hResult->m_data, column);
	if (name != NULL)
	{
#ifdef UNICODE
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, -1, buffer, bufSize);
		buffer[bufSize - 1] = 0;
#else
		nx_strncpy(buffer, name, bufSize);
#endif
	}
	return name != NULL;
}

/**
 * Get column count for async request
 */
int LIBNXDB_EXPORTABLE DBGetColumnCountAsync(DB_ASYNC_RESULT hResult)
{
	return hResult->m_driver->m_fpDrvGetColumnCountAsync(hResult->m_data);
}

/**
 * Get column name for async request
 */
BOOL LIBNXDB_EXPORTABLE DBGetColumnNameAsync(DB_ASYNC_RESULT hResult, int column, TCHAR *buffer, int bufSize)
{
	const char *name;

	name = hResult->m_driver->m_fpDrvGetColumnNameAsync(hResult->m_data, column);
	if (name != NULL)
	{
#ifdef UNICODE
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, -1, buffer, bufSize);
		buffer[bufSize - 1] = 0;
#else
		nx_strncpy(buffer, name, bufSize);
#endif
	}
	return name != NULL;
}

/**
 * Get field's value. If buffer is NULL, dynamically allocated string will be returned.
 * Caller is responsible for destroying it by calling free().
 */
TCHAR LIBNXDB_EXPORTABLE *DBGetField(DB_RESULT hResult, int iRow, int iColumn, TCHAR *pszBuffer, int nBufLen)
{
#ifdef UNICODE
   if (pszBuffer != NULL)
   {
      return hResult->m_driver->m_fpDrvGetField(hResult->m_data, iRow, iColumn, pszBuffer, nBufLen);
   }
   else
   {
      WCHAR *pszTemp;
      LONG nLen = hResult->m_driver->m_fpDrvGetFieldLength(hResult->m_data, iRow, iColumn);
      if (nLen == -1)
      {
         pszTemp = NULL;
      }
      else
      {
         nLen++;
         pszTemp = (WCHAR *)malloc(nLen * sizeof(WCHAR));
         hResult->m_driver->m_fpDrvGetField(hResult->m_data, iRow, iColumn, pszTemp, nLen);
      }
      return pszTemp;
   }
#else
	return DBGetFieldA(hResult, iRow, iColumn, pszBuffer, nBufLen);
#endif
}

/**
 * Get field's value as UTF8 string. If buffer is NULL, dynamically allocated string will be returned.
 * Caller is responsible for destroying it by calling free().
 */
char LIBNXDB_EXPORTABLE *DBGetFieldUTF8(DB_RESULT hResult, int iRow, int iColumn, char *pszBuffer, int nBufLen)
{
   if (hResult->m_driver->m_fpDrvGetFieldUTF8 != NULL)
   {
      if (pszBuffer != NULL)
      {
         return hResult->m_driver->m_fpDrvGetFieldUTF8(hResult->m_data, iRow, iColumn, pszBuffer, nBufLen);
      }
      else
      {
         char *pszTemp;
         LONG nLen = hResult->m_driver->m_fpDrvGetFieldLength(hResult->m_data, iRow, iColumn);
         if (nLen == -1)
         {
            pszTemp = NULL;
         }
         else
         {
            nLen *= 2;  // increase buffer size because driver may return field length in characters
            pszTemp = (char *)malloc(nLen);
            hResult->m_driver->m_fpDrvGetFieldUTF8(hResult->m_data, iRow, iColumn, pszTemp, nLen);
         }
         return pszTemp;
      }
   }
   else
   {
      LONG nLen = hResult->m_driver->m_fpDrvGetFieldLength(hResult->m_data, iRow, iColumn);
      if (nLen == -1)
         return NULL;
      nLen *= 2;  // increase buffer size because driver may return field length in characters

      WCHAR *wtemp = (WCHAR *)malloc(nLen * sizeof(WCHAR));
      hResult->m_driver->m_fpDrvGetField(hResult->m_data, iRow, iColumn, wtemp, nLen);
      char *value = (pszBuffer != NULL) ? pszBuffer : (char *)malloc(nLen);
      WideCharToMultiByte(CP_UTF8, 0, wtemp, -1, value, (pszBuffer != NULL) ? nBufLen : nLen, NULL, NULL);
      free(wtemp);
      return value;
   }
}

/**
 * Get field's value as multibyte string. If buffer is NULL, dynamically allocated string will be returned.
 * Caller is responsible for destroying it by calling free().
 */
char LIBNXDB_EXPORTABLE *DBGetFieldA(DB_RESULT hResult, int iRow, int iColumn, char *pszBuffer, int nBufLen)
{
   WCHAR *pwszData, *pwszBuffer;
   char *pszRet;
   int nLen;

   if (pszBuffer != NULL)
   {
      pwszBuffer = (WCHAR *)malloc(nBufLen * sizeof(WCHAR));
      pwszData = hResult->m_driver->m_fpDrvGetField(hResult->m_data, iRow, iColumn, pwszBuffer, nBufLen);
      if (pwszData != NULL)
      {
         WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, pwszData, -1, pszBuffer, nBufLen, NULL, NULL);
         pszRet = pszBuffer;
      }
      else
      {
         pszRet = NULL;
      }
      free(pwszBuffer);
   }
   else
   {
      nLen = hResult->m_driver->m_fpDrvGetFieldLength(hResult->m_data, iRow, iColumn);
      if (nLen == -1)
      {
         pszRet = NULL;
      }
      else
      {
         nLen++;
         pwszBuffer = (WCHAR *)malloc(nLen * sizeof(WCHAR));
         pwszData = hResult->m_driver->m_fpDrvGetField(hResult->m_data, iRow, iColumn, pwszBuffer, nLen);
         if (pwszData != NULL)
         {
            nLen = (int)wcslen(pwszData) + 1;
            pszRet = (char *)malloc(nLen);
            WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, pwszData, -1, pszRet, nLen, NULL, NULL);
         }
         else
         {
            pszRet = NULL;
         }
         free(pwszBuffer);
      }
   }
   return pszRet;
}

/**
 * Get field's value as unsigned long
 */
UINT32 LIBNXDB_EXPORTABLE DBGetFieldULong(DB_RESULT hResult, int iRow, int iColumn)
{
   INT32 iVal;
   UINT32 dwVal;
   TCHAR *pszVal, szBuffer[256];

   pszVal = DBGetField(hResult, iRow, iColumn, szBuffer, 256);
   if (pszVal == NULL)
      return 0;
	StrStrip(pszVal);
	if (*pszVal == _T('-'))
	{
		iVal = _tcstol(pszVal, NULL, 10);
		memcpy(&dwVal, &iVal, sizeof(INT32));   // To prevent possible conversion
	}
	else
	{
		dwVal = _tcstoul(pszVal, NULL, 10);
	}
   return dwVal;
}

/**
 * Get field's value as unsigned 64-bit int
 */
UINT64 LIBNXDB_EXPORTABLE DBGetFieldUInt64(DB_RESULT hResult, int iRow, int iColumn)
{
   INT64 iVal;
   UINT64 qwVal;
   TCHAR *pszVal, szBuffer[256];

   pszVal = DBGetField(hResult, iRow, iColumn, szBuffer, 256);
   if (pszVal == NULL)
      return 0;
	StrStrip(pszVal);
	if (*pszVal == _T('-'))
	{
		iVal = _tcstoll(pszVal, NULL, 10);
		memcpy(&qwVal, &iVal, sizeof(INT64));   // To prevent possible conversion
	}
	else
	{
		qwVal = _tcstoull(pszVal, NULL, 10);
	}
   return qwVal;
}

/**
 * Get field's value as signed long
 */
INT32 LIBNXDB_EXPORTABLE DBGetFieldLong(DB_RESULT hResult, int iRow, int iColumn)
{
   TCHAR *pszVal, szBuffer[256];

   pszVal = DBGetField(hResult, iRow, iColumn, szBuffer, 256);
   return pszVal == NULL ? 0 : _tcstol(pszVal, NULL, 10);
}

/**
 * Get field's value as signed 64-bit int
 */
INT64 LIBNXDB_EXPORTABLE DBGetFieldInt64(DB_RESULT hResult, int iRow, int iColumn)
{
   TCHAR *pszVal, szBuffer[256];

   pszVal = DBGetField(hResult, iRow, iColumn, szBuffer, 256);
   return pszVal == NULL ? 0 : _tcstoll(pszVal, NULL, 10);
}

/**
 * Get field's value as double
 */
double LIBNXDB_EXPORTABLE DBGetFieldDouble(DB_RESULT hResult, int iRow, int iColumn)
{
   TCHAR *pszVal, szBuffer[256];

   pszVal = DBGetField(hResult, iRow, iColumn, szBuffer, 256);
   return pszVal == NULL ? 0 : _tcstod(pszVal, NULL);
}

/**
 * Get field's value as IP address
 */
UINT32 LIBNXDB_EXPORTABLE DBGetFieldIPAddr(DB_RESULT hResult, int iRow, int iColumn)
{
   TCHAR *pszVal, szBuffer[256];

   pszVal = DBGetField(hResult, iRow, iColumn, szBuffer, 256);
   return pszVal == NULL ? INADDR_NONE : ntohl(_t_inet_addr(pszVal));
}

/**
 * Get field's value as integer array from byte array encoded in hex
 */
BOOL LIBNXDB_EXPORTABLE DBGetFieldByteArray(DB_RESULT hResult, int iRow, int iColumn,
                                            int *pnArray, int nSize, int nDefault)
{
   char pbBytes[128];
   BOOL bResult;
   int i, nLen;
   TCHAR *pszVal, szBuffer[256];

   pszVal = DBGetField(hResult, iRow, iColumn, szBuffer, 256);
   if (pszVal != NULL)
   {
      StrToBin(pszVal, (BYTE *)pbBytes, 128);
      nLen = (int)_tcslen(pszVal) / 2;
      for(i = 0; (i < nSize) && (i < nLen); i++)
         pnArray[i] = pbBytes[i];
      for(; i < nSize; i++)
         pnArray[i] = nDefault;
      bResult = TRUE;
   }
   else
   {
      for(i = 0; i < nSize; i++)
         pnArray[i] = nDefault;
      bResult = FALSE;
   }
   return bResult;
}

BOOL LIBNXDB_EXPORTABLE DBGetFieldByteArray2(DB_RESULT hResult, int iRow, int iColumn,
                                             BYTE *data, int nSize, int nDefault)
{
   BOOL bResult;
   TCHAR *pszVal, szBuffer[256];

   pszVal = DBGetField(hResult, iRow, iColumn, szBuffer, 256);
   if (pszVal != NULL)
   {
      int bytes = (int)StrToBin(pszVal, data, nSize);
		if (bytes < nSize)
			memset(&data[bytes], 0, nSize - bytes);
      bResult = TRUE;
   }
   else
   {
		memset(data, nDefault, nSize);
      bResult = FALSE;
   }
   return bResult;
}

/**
 * Get field's value as GUID
 */
BOOL LIBNXDB_EXPORTABLE DBGetFieldGUID(DB_RESULT hResult, int iRow, int iColumn, uuid_t guid)
{
   TCHAR *pszVal, szBuffer[256];
   BOOL bResult;

   pszVal = DBGetField(hResult, iRow, iColumn, szBuffer, 256);
   if (pszVal != NULL)
   {
      if (uuid_parse(pszVal, guid) == 0)
      {
         bResult = TRUE;
      }
      else
      {
         uuid_clear(guid);
         bResult = FALSE;
      }
   }
   else
   {
      uuid_clear(guid);
      bResult = FALSE;
   }
   return bResult;
}

/**
 * Get number of rows in result
 */
int LIBNXDB_EXPORTABLE DBGetNumRows(DB_RESULT hResult)
{
   if (hResult == NULL)
      return 0;
   return hResult->m_driver->m_fpDrvGetNumRows(hResult->m_data);
}

/**
 * Free result
 */
void LIBNXDB_EXPORTABLE DBFreeResult(DB_RESULT hResult)
{
   if (hResult != NULL)
	{
      hResult->m_driver->m_fpDrvFreeResult(hResult->m_data);
		free(hResult);
	}
}

/**
 * Asyncronous SELECT query
 */
DB_ASYNC_RESULT LIBNXDB_EXPORTABLE DBAsyncSelectEx(DB_HANDLE hConn, const TCHAR *szQuery, TCHAR *errorText)
{
   DBDRV_ASYNC_RESULT hResult;
	DB_ASYNC_RESULT result = NULL;
   DWORD dwError = DBERR_OTHER_ERROR;
   INT64 ms;
#ifdef UNICODE
#define pwszQuery szQuery
#define wcErrorText errorText
#else
   WCHAR *pwszQuery = WideStringFromMBString(szQuery);
	WCHAR wcErrorText[DBDRV_MAX_ERROR_TEXT] = L"";
#endif
   
   MutexLock(hConn->m_mutexTransLock);
	if (hConn->m_driver->m_dumpSql)
      ms = GetCurrentTimeMs();
   hResult = hConn->m_driver->m_fpDrvAsyncSelect(hConn->m_connection, pwszQuery, &dwError, wcErrorText);
   if ((hResult == NULL) && (dwError == DBERR_CONNECTION_LOST) && hConn->m_reconnectEnabled)
   {
      DBReconnect(hConn);
      hResult = hConn->m_driver->m_fpDrvAsyncSelect(hConn->m_connection, pwszQuery, &dwError, wcErrorText);
   }

   if (hConn->m_driver->m_dumpSql)
   {
      ms = GetCurrentTimeMs() - ms;
      __DBDbgPrintf(9, _T("%s async query: \"%s\" [%d ms]"), (hResult != NULL) ? _T("Successful") : _T("Failed"), szQuery, (DWORD)ms);
   }
   if (hResult == NULL)
   {
      MutexUnlock(hConn->m_mutexTransLock);

#ifndef UNICODE
		WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, wcErrorText, -1, errorText, DBDRV_MAX_ERROR_TEXT, NULL, NULL);
		errorText[DBDRV_MAX_ERROR_TEXT - 1] = 0;
#endif

		if (hConn->m_driver->m_logSqlErrors)
        nxlog_write(g_sqlErrorMsgCode, EVENTLOG_ERROR_TYPE, "ss", szQuery, errorText);
		if (hConn->m_driver->m_fpEventHandler != NULL)
			hConn->m_driver->m_fpEventHandler(DBEVENT_QUERY_FAILED, pwszQuery, wcErrorText, hConn->m_driver->m_userArg);
   }

#ifndef UNICODE
   free(pwszQuery);
#endif
   
	if (hResult != NULL)
	{
		result = (DB_ASYNC_RESULT)malloc(sizeof(db_async_result_t));
		result->m_driver = hConn->m_driver;
		result->m_connection = hConn;
		result->m_data = hResult;
	}

   return result;
#undef pwszQuery
#undef wcErrorText
}

DB_ASYNC_RESULT LIBNXDB_EXPORTABLE DBAsyncSelect(DB_HANDLE hConn, const TCHAR *query)
{
   TCHAR errorText[DBDRV_MAX_ERROR_TEXT];

	return DBAsyncSelectEx(hConn, query, errorText);
}

/**
 * Fetch next row from asynchronous SELECT result
 */
BOOL LIBNXDB_EXPORTABLE DBFetch(DB_ASYNC_RESULT hResult)
{
	return hResult->m_driver->m_fpDrvFetch(hResult->m_data);
}

/**
 * Get field's value from asynchronous SELECT result
 */
TCHAR LIBNXDB_EXPORTABLE *DBGetFieldAsync(DB_ASYNC_RESULT hResult, int iColumn, TCHAR *pBuffer, int iBufSize)
{
#ifdef UNICODE
   if (pBuffer != NULL)
   {
	   return hResult->m_driver->m_fpDrvGetFieldAsync(hResult->m_data, iColumn, pBuffer, iBufSize);
   }
   else
   {
      INT32 nLen;
      WCHAR *pszTemp;

      nLen = hResult->m_driver->m_fpDrvGetFieldLengthAsync(hResult->m_data, iColumn);
      if (nLen == -1)
      {
         pszTemp = NULL;
      }
      else
      {
         nLen++;
         pszTemp = (WCHAR *)malloc(nLen * sizeof(WCHAR));
         hResult->m_driver->m_fpDrvGetFieldAsync(hResult->m_data, iColumn, pszTemp, nLen);
      }
      return pszTemp;
   }
#else
   WCHAR *pwszData, *pwszBuffer;
   char *pszRet;
   int nLen;

   if (pBuffer != NULL)
   {
		pwszBuffer = (WCHAR *)malloc(iBufSize * sizeof(WCHAR));
		if (hResult->m_driver->m_fpDrvGetFieldAsync(hResult->m_data, iColumn, pwszBuffer, iBufSize) != NULL)
		{
			WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR,
									  pwszBuffer, -1, pBuffer, iBufSize, NULL, NULL);
			pszRet = pBuffer;
		}
		else
		{
			pszRet = NULL;
		}
		free(pwszBuffer);
   }
   else
   {
		nLen = hResult->m_driver->m_fpDrvGetFieldLengthAsync(hResult->m_data, iColumn);
      if (nLen == -1)
      {
         pszRet = NULL;
      }
      else
      {
         nLen++;
         pwszBuffer = (WCHAR *)malloc(nLen * sizeof(WCHAR));
			pwszData = hResult->m_driver->m_fpDrvGetFieldAsync(hResult->m_data, iColumn, pwszBuffer, nLen);
         if (pwszData != NULL)
         {
            nLen = (int)wcslen(pwszData) + 1;
            pszRet = (char *)malloc(nLen);
            WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR,
                                pwszData, -1, pszRet, nLen, NULL, NULL);
         }
         else
         {
            pszRet = NULL;
         }
         free(pwszBuffer);
      }
   }
   return pszRet;
#endif
}

/**
 * Get field's value as unsigned long from asynchronous SELECT result
 */
UINT32 LIBNXDB_EXPORTABLE DBGetFieldAsyncULong(DB_ASYNC_RESULT hResult, int iColumn)
{
   INT32 iVal;
   UINT32 dwVal;
   TCHAR szBuffer[64];

   if (DBGetFieldAsync(hResult, iColumn, szBuffer, 64) == NULL)
      return 0;
	StrStrip(szBuffer);
	if (szBuffer[0] == _T('-'))
	{
		iVal = _tcstol(szBuffer, NULL, 10);
		memcpy(&dwVal, &iVal, sizeof(INT32));   // To prevent possible conversion
	}
	else
	{
		dwVal = _tcstoul(szBuffer, NULL, 10);
	}
   return dwVal;
}

/**
 * Get field's value as unsigned 64-bit int from asynchronous SELECT result
 */
UINT64 LIBNXDB_EXPORTABLE DBGetFieldAsyncUInt64(DB_ASYNC_RESULT hResult, int iColumn)
{
   INT64 iVal;
   UINT64 qwVal;
   TCHAR szBuffer[64];

   if (DBGetFieldAsync(hResult, iColumn, szBuffer, 64) == NULL)
      return 0;
	StrStrip(szBuffer);
	if (szBuffer[0] == _T('-'))
	{
		iVal = _tcstoll(szBuffer, NULL, 10);
		memcpy(&qwVal, &iVal, sizeof(INT64));   // To prevent possible conversion
	}
	else
	{
		qwVal = _tcstoull(szBuffer, NULL, 10);
	}
   return qwVal;
}

/**
 * Get field's value as signed long from asynchronous SELECT result
 */
INT32 LIBNXDB_EXPORTABLE DBGetFieldAsyncLong(DB_ASYNC_RESULT hResult, int iColumn)
{
   TCHAR szBuffer[64];
   
   return DBGetFieldAsync(hResult, iColumn, szBuffer, 64) == NULL ? 0 : _tcstol(szBuffer, NULL, 10);
}

/**
 * Get field's value as signed 64-bit int from asynchronous SELECT result
 */
INT64 LIBNXDB_EXPORTABLE DBGetFieldAsyncInt64(DB_ASYNC_RESULT hResult, int iColumn)
{
   TCHAR szBuffer[64];
   
   return DBGetFieldAsync(hResult, iColumn, szBuffer, 64) == NULL ? 0 : _tcstoll(szBuffer, NULL, 10);
}

/**
 * Get field's value as signed long from asynchronous SELECT result
 */
double LIBNXDB_EXPORTABLE DBGetFieldAsyncDouble(DB_ASYNC_RESULT hResult, int iColumn)
{
   TCHAR szBuffer[64];
   
   return DBGetFieldAsync(hResult, iColumn, szBuffer, 64) == NULL ? 0 : _tcstod(szBuffer, NULL);
}

/**
 * Get field's value as IP address from asynchronous SELECT result
 */
UINT32 LIBNXDB_EXPORTABLE DBGetFieldAsyncIPAddr(DB_ASYNC_RESULT hResult, int iColumn)
{
   TCHAR szBuffer[64];
   
   return (DBGetFieldAsync(hResult, iColumn, szBuffer, 64) == NULL) ? INADDR_NONE : ntohl(_t_inet_addr(szBuffer));
}

/**
 * Free asynchronous SELECT result
 */
void LIBNXDB_EXPORTABLE DBFreeAsyncResult(DB_ASYNC_RESULT hResult)
{
	hResult->m_driver->m_fpDrvFreeAsyncResult(hResult->m_data);
	MutexUnlock(hResult->m_connection->m_mutexTransLock);
	free(hResult);
}

/**
 * Prepare statement
 */
DB_STATEMENT LIBNXDB_EXPORTABLE DBPrepareEx(DB_HANDLE hConn, const TCHAR *query, TCHAR *errorText)
{
	DB_STATEMENT result = NULL;
	INT64 ms;

#ifdef UNICODE
#define pwszQuery query
#define wcErrorText errorText
#else
   WCHAR *pwszQuery = WideStringFromMBString(query);
	WCHAR wcErrorText[DBDRV_MAX_ERROR_TEXT] = L"";
#endif

	MutexLock(hConn->m_mutexTransLock);

	if (hConn->m_driver->m_dumpSql)
      ms = GetCurrentTimeMs();

	DWORD errorCode;
	DBDRV_STATEMENT stmt = hConn->m_driver->m_fpDrvPrepare(hConn->m_connection, pwszQuery, &errorCode, wcErrorText);
   if ((stmt == NULL) && (errorCode == DBERR_CONNECTION_LOST) && hConn->m_reconnectEnabled)
   {
      DBReconnect(hConn);
		stmt = hConn->m_driver->m_fpDrvPrepare(hConn->m_connection, pwszQuery, &errorCode, wcErrorText);
	}
   MutexUnlock(hConn->m_mutexTransLock);

	if (stmt != NULL)
	{
		result = (DB_STATEMENT)malloc(sizeof(db_statement_t));
		result->m_driver = hConn->m_driver;
		result->m_connection = hConn;
		result->m_statement = stmt;
		result->m_query = _tcsdup(query);
	}
	else
	{
#ifndef UNICODE
		WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, wcErrorText, -1, errorText, DBDRV_MAX_ERROR_TEXT, NULL, NULL);
		errorText[DBDRV_MAX_ERROR_TEXT - 1] = 0;
#endif

		if (hConn->m_driver->m_logSqlErrors)
        nxlog_write(g_sqlErrorMsgCode, EVENTLOG_ERROR_TYPE, "ss", query, errorText);
		if (hConn->m_driver->m_fpEventHandler != NULL)
			hConn->m_driver->m_fpEventHandler(DBEVENT_QUERY_FAILED, pwszQuery, wcErrorText, hConn->m_driver->m_userArg);
	}

   if (hConn->m_driver->m_dumpSql)
   {
      ms = GetCurrentTimeMs() - ms;
		__DBDbgPrintf(9, _T("{%p} %s prepare: \"%s\" [%d ms]"), result, (result != NULL) ? _T("Successful") : _T("Failed"), query, ms);
	}

#ifndef UNICODE
	free(pwszQuery);
#endif

   if (result != NULL)
   {
      hConn->m_preparedStatements->add(result);
   }

	return result;
#undef pwszQuery
#undef wcErrorText
}

/**
 * Prepare statement
 */
DB_STATEMENT LIBNXDB_EXPORTABLE DBPrepare(DB_HANDLE hConn, const TCHAR *query)
{
	TCHAR errorText[DBDRV_MAX_ERROR_TEXT];
	return DBPrepareEx(hConn, query, errorText);
}

/**
 * Destroy prepared statement
 */
void LIBNXDB_EXPORTABLE DBFreeStatement(DB_STATEMENT hStmt)
{
   if (hStmt == NULL)
      return;

   if (hStmt->m_connection != NULL)
   {
      hStmt->m_connection->m_preparedStatements->remove(hStmt);
   }
   hStmt->m_driver->m_fpDrvFreeStatement(hStmt->m_statement);
   safe_free(hStmt->m_query);
	free(hStmt);
}

/**
 * Get source query for prepared statement
 */
const TCHAR LIBNXDB_EXPORTABLE *DBGetStatementSource(DB_STATEMENT hStmt)
{
   return hStmt->m_query;
}

/**
 * Bind parameter (generic)
 */
void LIBNXDB_EXPORTABLE DBBind(DB_STATEMENT hStmt, int pos, int sqlType, int cType, void *buffer, int allocType)
{
	if ((pos <= 0) || !IS_VALID_STATEMENT_HANDLE(hStmt))
		return;

	if (hStmt->m_connection->m_driver->m_dumpSql)
   {
		if (cType == DB_CTYPE_STRING)
		{
			__DBDbgPrintf(9, _T("{%p} bind at pos %d: \"%s\""), hStmt, pos, buffer);
		}
		else
		{
			TCHAR text[64];
			switch(cType)
			{
				case DB_CTYPE_INT32:
					_sntprintf(text, 64, _T("%d"), *((INT32 *)buffer));
					break;
				case DB_CTYPE_UINT32:
					_sntprintf(text, 64, _T("%u"), *((UINT32 *)buffer));
					break;
				case DB_CTYPE_INT64:
					_sntprintf(text, 64, INT64_FMT, *((INT64 *)buffer));
					break;
				case DB_CTYPE_UINT64:
					_sntprintf(text, 64, UINT64_FMT, *((UINT64 *)buffer));
					break;
				case DB_CTYPE_DOUBLE:
					_sntprintf(text, 64, _T("%f"), *((double *)buffer));
					break;
			}
			__DBDbgPrintf(9, _T("{%p} bind at pos %d: \"%s\""), hStmt, pos, text);
		}
	}

#ifdef UNICODE
#define wBuffer buffer
#define realAllocType allocType
#else
	void *wBuffer;
	int realAllocType = allocType;
	if (cType == DB_CTYPE_STRING)
	{
		wBuffer = (void *)WideStringFromMBString((char *)buffer);
		if (allocType == DB_BIND_DYNAMIC)
			free(buffer);
		realAllocType = DB_BIND_DYNAMIC;
	}
	else
	{
		wBuffer = buffer;
	}
#endif
	hStmt->m_driver->m_fpDrvBind(hStmt->m_statement, pos, sqlType, cType, wBuffer, realAllocType);
#undef wBuffer
#undef realAllocType
}

/**
 * Bind string parameter
 */
void LIBNXDB_EXPORTABLE DBBind(DB_STATEMENT hStmt, int pos, int sqlType, const TCHAR *value, int allocType)
{
	if (value != NULL)
		DBBind(hStmt, pos, sqlType, DB_CTYPE_STRING, (void *)value, allocType);
	else
		DBBind(hStmt, pos, sqlType, DB_CTYPE_STRING, (void *)_T(""), DB_BIND_STATIC);
}

/**
 * Bind string parameter with length valiadtion
 */
void LIBNXDB_EXPORTABLE DBBind(DB_STATEMENT hStmt, int pos, int sqlType, const TCHAR *value, int allocType, int maxLen)
{
	if (value != NULL)
   {
      if ((int)_tcslen(value) <= maxLen)
      {
		   DBBind(hStmt, pos, sqlType, DB_CTYPE_STRING, (void *)value, allocType);
      }
      else
      {
         if (allocType == DB_BIND_DYNAMIC)
         {
            ((TCHAR *)value)[maxLen] = 0;
   		   DBBind(hStmt, pos, sqlType, DB_CTYPE_STRING, (void *)value, DB_BIND_DYNAMIC);
         }
         else
         {
            TCHAR *temp = (TCHAR *)nx_memdup(value, sizeof(TCHAR) * (maxLen + 1));
            temp[maxLen] = 0;
   		   DBBind(hStmt, pos, sqlType, DB_CTYPE_STRING, temp, DB_BIND_DYNAMIC);
         }
      }
   }
	else
   {
		DBBind(hStmt, pos, sqlType, DB_CTYPE_STRING, (void *)_T(""), DB_BIND_STATIC);
   }
}

/**
 * Bind 32 bit integer parameter
 */
void LIBNXDB_EXPORTABLE DBBind(DB_STATEMENT hStmt, int pos, int sqlType, INT32 value)
{
	DBBind(hStmt, pos, sqlType, DB_CTYPE_INT32, &value, DB_BIND_TRANSIENT);
}

/**
 * Bind 32 bit unsigned integer parameter
 */
void LIBNXDB_EXPORTABLE DBBind(DB_STATEMENT hStmt, int pos, int sqlType, UINT32 value)
{
	// C type intentionally set to INT32 - unsigned numbers will be written as negatives
	// and correctly parsed on read by DBGetFieldULong
	// Setting it to UINT32 may cause INSERT/UPDATE failures on some databases
	DBBind(hStmt, pos, sqlType, DB_CTYPE_INT32, &value, DB_BIND_TRANSIENT);
}

/**
 * Bind 64 bit integer parameter
 */
void LIBNXDB_EXPORTABLE DBBind(DB_STATEMENT hStmt, int pos, int sqlType, INT64 value)
{
	DBBind(hStmt, pos, sqlType, DB_CTYPE_INT64, &value, DB_BIND_TRANSIENT);
}

/**
 * Bind 64 bit unsigned integer parameter
 */
void LIBNXDB_EXPORTABLE DBBind(DB_STATEMENT hStmt, int pos, int sqlType, UINT64 value)
{
	DBBind(hStmt, pos, sqlType, DB_CTYPE_UINT64, &value, DB_BIND_TRANSIENT);
}

/**
 * Bind floating point parameter
 */
void LIBNXDB_EXPORTABLE DBBind(DB_STATEMENT hStmt, int pos, int sqlType, double value)
{
	DBBind(hStmt, pos, sqlType, DB_CTYPE_DOUBLE, &value, DB_BIND_TRANSIENT);
}

/**
 * Execute prepared statement (non-SELECT)
 */
BOOL LIBNXDB_EXPORTABLE DBExecuteEx(DB_STATEMENT hStmt, TCHAR *errorText)
{
   if (!IS_VALID_STATEMENT_HANDLE(hStmt))
   {
      _tcscpy(errorText, _T("Invalid statement handle"));
      return FALSE;
   }

#ifdef UNICODE
#define wcErrorText errorText
#else
	WCHAR wcErrorText[DBDRV_MAX_ERROR_TEXT] = L"";
#endif
	UINT64 ms;

	DB_HANDLE hConn = hStmt->m_connection;
	MutexLock(hConn->m_mutexTransLock);
   if (hConn->m_driver->m_dumpSql)
      ms = GetCurrentTimeMs();

	DWORD dwResult = hConn->m_driver->m_fpDrvExecute(hConn->m_connection, hStmt->m_statement, wcErrorText);
   if (hConn->m_driver->m_dumpSql)
   {
      ms = GetCurrentTimeMs() - ms;
      __DBDbgPrintf(9, _T("%s prepared sync query: \"%s\" [%d ms]"), (dwResult == DBERR_SUCCESS) ? _T("Successful") : _T("Failed"), hStmt->m_query, ms);
   }

   // Do reconnect if needed, but don't retry statement execution
   // because it will fail anyway
   if ((dwResult == DBERR_CONNECTION_LOST) && hConn->m_reconnectEnabled)
   {
      DBReconnect(hConn);
   }
   
   MutexUnlock(hConn->m_mutexTransLock);

#ifndef UNICODE
	WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, wcErrorText, -1, errorText, DBDRV_MAX_ERROR_TEXT, NULL, NULL);
	errorText[DBDRV_MAX_ERROR_TEXT - 1] = 0;
#endif

   if (dwResult != DBERR_SUCCESS)
	{	
		if (hConn->m_driver->m_logSqlErrors)
			nxlog_write(g_sqlErrorMsgCode, EVENTLOG_ERROR_TYPE, "ss", hStmt->m_query, errorText);
		if (hConn->m_driver->m_fpEventHandler != NULL)
		{
#ifdef UNICODE
			hConn->m_driver->m_fpEventHandler(DBEVENT_QUERY_FAILED, hStmt->m_query, wcErrorText, hConn->m_driver->m_userArg);
#else
			WCHAR *query = WideStringFromMBString(hStmt->m_query);
			hConn->m_driver->m_fpEventHandler(DBEVENT_QUERY_FAILED, query, wcErrorText, hConn->m_driver->m_userArg);
			free(query);
#endif
		}
	}

   return dwResult == DBERR_SUCCESS;
#undef wcErrorText
}

/**
 * Execute prepared statement (non-SELECT)
 */
BOOL LIBNXDB_EXPORTABLE DBExecute(DB_STATEMENT hStmt)
{
	TCHAR errorText[DBDRV_MAX_ERROR_TEXT];
	return DBExecuteEx(hStmt, errorText);
}

/**
 * Execute prepared SELECT statement
 */
DB_RESULT LIBNXDB_EXPORTABLE DBSelectPreparedEx(DB_STATEMENT hStmt, TCHAR *errorText)
{
   if (!IS_VALID_STATEMENT_HANDLE(hStmt))
   {
      _tcscpy(errorText, _T("Invalid statement handle"));
      return NULL;
   }

	DB_RESULT result = NULL;
#ifdef UNICODE
#define wcErrorText errorText
#else
	WCHAR wcErrorText[DBDRV_MAX_ERROR_TEXT] = L"";
#endif

	DB_HANDLE hConn = hStmt->m_connection;
   MutexLock(hConn->m_mutexTransLock);

   INT64 ms;
   if (hConn->m_driver->m_dumpSql)
      ms = GetCurrentTimeMs();
   DWORD dwError = DBERR_OTHER_ERROR;
	DBDRV_RESULT hResult = hConn->m_driver->m_fpDrvSelectPrepared(hConn->m_connection, hStmt->m_statement, &dwError, wcErrorText);

   if (hConn->m_driver->m_dumpSql)
   {
      ms = GetCurrentTimeMs() - ms;
      __DBDbgPrintf(9, _T("%s prepared sync query: \"%s\" [%d ms]"), 
		              (hResult != NULL) ? _T("Successful") : _T("Failed"), hStmt->m_query, (DWORD)ms);
   }

   // Do reconnect if needed, but don't retry statement execution
   // because it will fail anyway
   if ((hResult == NULL) && (dwError == DBERR_CONNECTION_LOST) && hConn->m_reconnectEnabled)
   {
      DBReconnect(hConn);
   }

   MutexUnlock(hConn->m_mutexTransLock);

#ifndef UNICODE
	WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, wcErrorText, -1, errorText, DBDRV_MAX_ERROR_TEXT, NULL, NULL);
	errorText[DBDRV_MAX_ERROR_TEXT - 1] = 0;
#endif

	if (hResult == NULL)
	{
		if (hConn->m_driver->m_logSqlErrors)
			nxlog_write(g_sqlErrorMsgCode, EVENTLOG_ERROR_TYPE, "ss", hStmt->m_query, errorText);
		if (hConn->m_driver->m_fpEventHandler != NULL)
		{
#ifdef UNICODE
			hConn->m_driver->m_fpEventHandler(DBEVENT_QUERY_FAILED, hStmt->m_query, wcErrorText, hConn->m_driver->m_userArg);
#else
			WCHAR *query = WideStringFromMBString(hStmt->m_query);
			hConn->m_driver->m_fpEventHandler(DBEVENT_QUERY_FAILED, query, wcErrorText, hConn->m_driver->m_userArg);
			free(query);
#endif
		}
	}

	if (hResult != NULL)
	{
		result = (DB_RESULT)malloc(sizeof(db_result_t));
		result->m_driver = hConn->m_driver;
		result->m_connection = hConn;
		result->m_data = hResult;
	}

   return result;
#undef wcErrorText
}

/**
 * Execute prepared SELECT statement
 */
DB_RESULT LIBNXDB_EXPORTABLE DBSelectPrepared(DB_STATEMENT hStmt)
{
	TCHAR errorText[DBDRV_MAX_ERROR_TEXT];
	return DBSelectPreparedEx(hStmt, errorText);
}

/**
 * Begin transaction
 */
BOOL LIBNXDB_EXPORTABLE DBBegin(DB_HANDLE hConn)
{
   DWORD dwResult;
   BOOL bRet = FALSE;

   MutexLock(hConn->m_mutexTransLock);
   if (hConn->m_transactionLevel == 0)
   {
      dwResult = hConn->m_driver->m_fpDrvBegin(hConn->m_connection);
      if ((dwResult == DBERR_CONNECTION_LOST) && hConn->m_reconnectEnabled)
      {
         DBReconnect(hConn);
         dwResult = hConn->m_driver->m_fpDrvBegin(hConn->m_connection);
      }
      if (dwResult == DBERR_SUCCESS)
      {
         hConn->m_transactionLevel++;
         bRet = TRUE;
			__DBDbgPrintf(9, _T("BEGIN TRANSACTION successful (level %d)"), hConn->m_transactionLevel);
      }
      else
      {
         MutexUnlock(hConn->m_mutexTransLock);
			__DBDbgPrintf(9, _T("BEGIN TRANSACTION failed"), hConn->m_transactionLevel);
      }
   }
   else
   {
      hConn->m_transactionLevel++;
      bRet = TRUE;
		__DBDbgPrintf(9, _T("BEGIN TRANSACTION successful (level %d)"), hConn->m_transactionLevel);
   }
   return bRet;
}

/**
 * Commit transaction
 */
BOOL LIBNXDB_EXPORTABLE DBCommit(DB_HANDLE hConn)
{
   BOOL bRet = FALSE;

   MutexLock(hConn->m_mutexTransLock);
   if (hConn->m_transactionLevel > 0)
   {
      hConn->m_transactionLevel--;
      if (hConn->m_transactionLevel == 0)
         bRet = (hConn->m_driver->m_fpDrvCommit(hConn->m_connection) == DBERR_SUCCESS);
      else
         bRet = TRUE;
		__DBDbgPrintf(9, _T("COMMIT TRANSACTION %s (level %d)"), bRet ? _T("successful") : _T("failed"), hConn->m_transactionLevel);
      MutexUnlock(hConn->m_mutexTransLock);
   }
   MutexUnlock(hConn->m_mutexTransLock);
   return bRet;
}

/**
 * Rollback transaction
 */
BOOL LIBNXDB_EXPORTABLE DBRollback(DB_HANDLE hConn)
{
   BOOL bRet = FALSE;

   MutexLock(hConn->m_mutexTransLock);
   if (hConn->m_transactionLevel > 0)
   {
      hConn->m_transactionLevel--;
      if (hConn->m_transactionLevel == 0)
         bRet = (hConn->m_driver->m_fpDrvRollback(hConn->m_connection) == DBERR_SUCCESS);
      else
         bRet = TRUE;
		__DBDbgPrintf(9, _T("ROLLBACK TRANSACTION %s (level %d)"), bRet ? _T("successful") : _T("failed"), hConn->m_transactionLevel);
      MutexUnlock(hConn->m_mutexTransLock);
   }
   MutexUnlock(hConn->m_mutexTransLock);
   return bRet;
}

/**
 * Prepare string for using in SQL statement
 */
String LIBNXDB_EXPORTABLE DBPrepareString(DB_HANDLE conn, const TCHAR *str, int maxSize)
{
	String out;
	if ((maxSize > 0) && (str != NULL) && (maxSize < (int)_tcslen(str)))
	{
		TCHAR *temp = (TCHAR *)malloc((maxSize + 1) * sizeof(TCHAR));
		nx_strncpy(temp, str, maxSize + 1);
#ifdef UNICODE
		out.setBuffer(conn->m_driver->m_fpDrvPrepareStringW(temp));
#else
		out.setBuffer(conn->m_driver->m_fpDrvPrepareStringA(temp));
#endif
		free(temp);
	}
	else	
	{
#ifdef UNICODE
		out.setBuffer(conn->m_driver->m_fpDrvPrepareStringW(CHECK_NULL_EX(str)));
#else
		out.setBuffer(conn->m_driver->m_fpDrvPrepareStringA(CHECK_NULL_EX(str)));
#endif
	}
	return out;
}

/**
 * Prepare string for using in SQL statement (multi-byte string version)
 */
#ifdef UNICODE

String LIBNXDB_EXPORTABLE DBPrepareStringA(DB_HANDLE conn, const char *str, int maxSize)
{
	WCHAR *wcs = WideStringFromMBString(str);
	String s = DBPrepareString(conn, wcs, maxSize);
	free(wcs);
	return s;
}

#endif

/**
 * Characters to be escaped before writing to SQL
 */
static TCHAR m_szSpecialChars[] = _T("\x01\x02\x03\x04\x05\x06\x07\x08")
                                  _T("\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10")
                                  _T("\x11\x12\x13\x14\x15\x16\x17\x18")
                                  _T("\x19\x1A\x1B\x1C\x1D\x1E\x1F")
                                  _T("#%\\'\x7F");

/**
 * Escape some special characters in string for writing into database.
 * DEPRECATED!
 */
TCHAR LIBNXDB_EXPORTABLE *EncodeSQLString(const TCHAR *pszIn)
{
   TCHAR *pszOut;
   int iPosIn, iPosOut, iStrSize;

   if ((pszIn != NULL) && (*pszIn != 0))
   {
      // Allocate destination buffer
      iStrSize = (int)_tcslen(pszIn) + 1;
      for(iPosIn = 0; pszIn[iPosIn] != 0; iPosIn++)
         if (_tcschr(m_szSpecialChars, pszIn[iPosIn])  != NULL)
            iStrSize += 2;
      pszOut = (TCHAR *)malloc(iStrSize * sizeof(TCHAR));

      // Translate string
      for(iPosIn = 0, iPosOut = 0; pszIn[iPosIn] != 0; iPosIn++)
         if (_tcschr(m_szSpecialChars, pszIn[iPosIn]) != NULL)
         {
            pszOut[iPosOut++] = _T('#');
            pszOut[iPosOut++] = bin2hex(pszIn[iPosIn] >> 4);
            pszOut[iPosOut++] = bin2hex(pszIn[iPosIn] & 0x0F);
         }
         else
         {
            pszOut[iPosOut++] = pszIn[iPosIn];
         }
      pszOut[iPosOut] = 0;
   }
   else
   {
      // Encode empty strings as #00
      pszOut = (TCHAR *)malloc(4 * sizeof(TCHAR));
      _tcscpy(pszOut, _T("#00"));
   }
   return pszOut;
}

/**
 * Restore characters encoded by EncodeSQLString()
 * Characters are decoded "in place"
 */
void LIBNXDB_EXPORTABLE DecodeSQLString(TCHAR *pszStr)
{
   int iPosIn, iPosOut;

   if (pszStr == NULL)
      return;

   for(iPosIn = 0, iPosOut = 0; pszStr[iPosIn] != 0; iPosIn++)
   {
      if (pszStr[iPosIn] == _T('#'))
      {
         iPosIn++;
         pszStr[iPosOut] = hex2bin(pszStr[iPosIn]) << 4;
         iPosIn++;
         pszStr[iPosOut] |= hex2bin(pszStr[iPosIn]);
         iPosOut++;
      }
      else
      {
         pszStr[iPosOut++] = pszStr[iPosIn];
      }
   }
   pszStr[iPosOut] = 0;
}

/**
 * Get database schema version
 * Will return 0 for unknown and -1 in case of SQL errors
 */
int LIBNXDB_EXPORTABLE DBGetSchemaVersion(DB_HANDLE conn)
{
	DB_RESULT hResult;
	int version = 0;

	// Read schema version from 'metadata' table, where it should
	// be stored starting from schema version 87
	// We ignore SQL error in this case, because table 'metadata'
	// may not exist in old schema versions
   hResult = DBSelect(conn, _T("SELECT var_value FROM metadata WHERE var_name='SchemaVersion'"));
   if (hResult != NULL)
   {
      if (DBGetNumRows(hResult) > 0)
         version = DBGetFieldLong(hResult, 0, 0);
      DBFreeResult(hResult);
   }

	// If database schema version is less than 87, version number
	// will be stored in 'config' table
	if (version == 0)
	{
		hResult = DBSelect(conn, _T("SELECT var_value FROM config WHERE var_name='DBFormatVersion'"));
		if (hResult != NULL)
		{
			if (DBGetNumRows(hResult) > 0)
				version = DBGetFieldLong(hResult, 0, 0);
			DBFreeResult(hResult);
		}
		else
		{
			version = -1;
		}
	}

	return version;
}

/**
 * Get database syntax
 */
int LIBNXDB_EXPORTABLE DBGetSyntax(DB_HANDLE conn)
{
	DB_RESULT hResult;
	TCHAR syntaxId[256];
	BOOL read = FALSE;
	int syntax;

   // Get database syntax
   hResult = DBSelect(conn, _T("SELECT var_value FROM metadata WHERE var_name='Syntax'"));
   if (hResult != NULL)
   {
      if (DBGetNumRows(hResult) > 0)
      {
         DBGetField(hResult, 0, 0, syntaxId, sizeof(syntaxId) / sizeof(TCHAR));
			read = TRUE;
      }
      else
      {
         _tcscpy(syntaxId, _T("UNKNOWN"));
      }
      DBFreeResult(hResult);
   }

	// If database schema version is less than 87, syntax
	// will be stored in 'config' table, so try it
	if (!read)
	{
		hResult = DBSelect(conn, _T("SELECT var_value FROM config WHERE var_name='DBSyntax'"));
		if (hResult != NULL)
		{
			if (DBGetNumRows(hResult) > 0)
			{
				DBGetField(hResult, 0, 0, syntaxId, sizeof(syntaxId) / sizeof(TCHAR));
				read = TRUE;
			}
			else
			{
				_tcscpy(syntaxId, _T("UNKNOWN"));
			}
			DBFreeResult(hResult);
		}
	}

   if (!_tcscmp(syntaxId, _T("MYSQL")))
   {
      syntax = DB_SYNTAX_MYSQL;
   }
   else if (!_tcscmp(syntaxId, _T("PGSQL")))
   {
      syntax = DB_SYNTAX_PGSQL;
   }
   else if (!_tcscmp(syntaxId, _T("MSSQL")))
   {
      syntax = DB_SYNTAX_MSSQL;
   }
   else if (!_tcscmp(syntaxId, _T("ORACLE")))
   {
      syntax = DB_SYNTAX_ORACLE;
   }
   else if (!_tcscmp(syntaxId, _T("SQLITE")))
   {
      syntax = DB_SYNTAX_SQLITE;
   }
   else if (!_tcscmp(syntaxId, _T("DB2")))
   {
      syntax = DB_SYNTAX_DB2;
   }
   else
   {
		syntax = DB_SYNTAX_UNKNOWN;
   }

	return syntax;
}
