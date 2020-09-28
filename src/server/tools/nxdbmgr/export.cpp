/*
** nxdbmgr - NetXMS database manager
** Copyright (C) 2004-2020 Victor Kirhenshtein
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
** File: export.cpp
**
**/

#include "nxdbmgr.h"
#include "sqlite3.h"

/**
 * Tables to export
 */
extern const TCHAR *g_tables[];

/**
 * Escape string for SQLite
 */
static TCHAR *EscapeString(const TCHAR *str)
{
	int len = (int)_tcslen(str) + 3;   // + two quotes and \0 at the end
	int bufferSize = len + 128;
	TCHAR *out = (TCHAR *)malloc(bufferSize * sizeof(TCHAR));
	out[0] = _T('\'');

	const TCHAR *src = str;
	int outPos;
	for(outPos = 1; *src != 0; src++)
	{
		if (*src == _T('\''))
		{
			len++;
			if (len >= bufferSize)
			{
				bufferSize += 128;
				out = (TCHAR *)realloc(out, bufferSize * sizeof(TCHAR));
			}
			out[outPos++] = _T('\'');
			out[outPos++] = _T('\'');
		}
		else
		{
			out[outPos++] = *src;
		}
	}
	out[outPos++] = _T('\'');
	out[outPos++] = 0;

	return out;
}

/**
 * Export single database table
 */
static BOOL ExportTable(sqlite3 *db, const TCHAR *name)
{
	StringBuffer query;
	TCHAR buffer[256];
	char *errmsg;
	int i, columnCount = 0;
	BOOL success = TRUE;

	_tprintf(_T("Exporting table %s\n"), name);

	if (sqlite3_exec(db, "BEGIN", nullptr, nullptr, &errmsg) == SQLITE_OK)
	{
		_sntprintf(buffer, 256, _T("SELECT * FROM %s"), name);

		DB_UNBUFFERED_RESULT hResult = SQLSelectUnbuffered(buffer);
		if (hResult != nullptr)
		{
			while(DBFetch(hResult))
			{
				query = _T("");

				// Column names
				columnCount = DBGetColumnCount(hResult);
				query.appendFormattedString(_T("INSERT INTO %s ("), name);
				for(i = 0; i < columnCount; i++)
				{
					DBGetColumnName(hResult, i, buffer, 256);
					query += buffer;
					query += _T(",");
				}
				query.shrink();
				query += _T(") VALUES (");

				// Data
				TCHAR data[8192];
				for(i = 0; i < columnCount; i++)
				{
					TCHAR *escapedString = EscapeString(DBGetField(hResult, i, data, 8192));
					query.appendPreallocated(escapedString);
					query += _T(",");
				}
				query.shrink();
				query += _T(")");

				char *utf8query = query.getUTF8String();
				if (sqlite3_exec(db, utf8query, nullptr, nullptr, &errmsg) != SQLITE_OK)
				{
				   MemFree(utf8query);
					_tprintf(_T("ERROR: SQLite query failed: %hs\n   Query: %s\n"), errmsg, (const TCHAR *)query);
					sqlite3_free(errmsg);
					success = FALSE;
					break;
				}
				MemFree(utf8query);
			}
			DBFreeResult(hResult);

			if (success)
			{
				if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, &errmsg) != SQLITE_OK)
				{
					_tprintf(_T("ERROR: Cannot commit transaction: %hs"), errmsg);
					sqlite3_free(errmsg);
					success = FALSE;
				}
			}
			else
			{
				if (sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, &errmsg) != SQLITE_OK)
				{
					_tprintf(_T("ERROR: Cannot rollback transaction: %hs"), errmsg);
					sqlite3_free(errmsg);
				}
			}
		}
		else
		{
			success = FALSE;
			if (sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, &errmsg) != SQLITE_OK)
			{
				_tprintf(_T("ERROR: Cannot rollback transaction: %hs"), errmsg);
				sqlite3_free(errmsg);
			}
		}
	}
	else
	{
		success = FALSE;
		_tprintf(_T("ERROR: Cannot start transaction: %hs"), errmsg);
		sqlite3_free(errmsg);
	}

	return success;
}

/**
 * Callback for getting schema version
 */
static int GetSchemaVersionCB(void *arg, int cols, char **data, char **names)
{
	*((int *)arg) = strtol(data[0], NULL, 10);
	return 0;
}

/**
 * Callback for getting idata_xx table creation query
 */
static int GetIDataQueryCB(void *arg, int cols, char **data, char **names)
{
	strncpy((char *)arg, data[0], MAX_DB_STRING);
	((char *)arg)[MAX_DB_STRING - 1] = 0;
	return 0;
}

/**
 * Data for module table import
 */
struct ModuleTableExportData
{
  const StringList *excludedTables;
  sqlite3 *db;
};

/**
 * Callback for exporting module table
 */
static bool ExportModuleTable(const TCHAR *table, void *context)
{
   if (static_cast<ModuleTableExportData*>(context)->excludedTables->contains(table))
   {
      _tprintf(_T("Skipping table %s\n"), table);
      return true;
   }
   return ExportTable(static_cast<ModuleTableExportData*>(context)->db, table);
}

/**
 * Execute schema file
 */
static bool ExecuteSchemaFile(const TCHAR *prefix, void *userArg)
{
   TCHAR schemaFile[MAX_PATH];
   GetNetXMSDirectory(nxDirShare, schemaFile);
   _tcslcat(schemaFile, FS_PATH_SEPARATOR _T("sql") FS_PATH_SEPARATOR, MAX_PATH);
   if (prefix != nullptr)
   {
      _tcslcat(schemaFile, prefix, MAX_PATH);
      _tcslcat(schemaFile, _T("_"), MAX_PATH);
   }
   _tcslcat(schemaFile, _T("dbschema_sqlite.sql"), MAX_PATH);

   char *data = LoadFileAsUTF8String(schemaFile);
   if (data == nullptr)
   {
      WriteToTerminalEx(_T("\x1b[31;1mERROR:\x1b[0m cannot load schema file \"%s\"\n"), schemaFile);
      return false;
   }

   char *errmsg;
   bool success = (sqlite3_exec(static_cast<sqlite3*>(userArg), data, nullptr, nullptr, &errmsg) == SQLITE_OK);
   if (!success)
   {
      _tprintf(_T("\x1b[31;1mERROR:\x1b[0m unable to apply database schema (%hs)\n"), errmsg);
      sqlite3_free(errmsg);
   }

   MemFree(data);
   return success;
}

/**
 * Export single table performance data
 */
static bool ExportSingleTablePerfData(sqlite3 *db, const StringList& excludedTables)
{
   _tprintf(_T("\x1b[31;1mERROR:\x1b[0m performance data export from this database is unsupported\n"));
   return false;
}

/**
 * Export multi-table performance data
 */
static bool ExportMultiTablePerfData(sqlite3 *db, const StringList& excludedTables)
{
   char queryTemplate[11][MAX_DB_STRING];
   memset(queryTemplate, 0, sizeof(queryTemplate));

   char *errmsg;
   if (sqlite3_exec(db, "SELECT var_value FROM metadata WHERE var_name='IDataTableCreationCommand'",
                    GetIDataQueryCB, queryTemplate[0], &errmsg) != SQLITE_OK)
   {
      _tprintf(_T("ERROR: SQLite query failed (%hs)\n"), errmsg);
      sqlite3_free(errmsg);
      return false;
   }

   for(int i = 0; i < 10; i++)
   {
      char query[256];
      sprintf(query, "SELECT var_value FROM metadata WHERE var_name='TDataTableCreationCommand_%d'", i);
      if (sqlite3_exec(db, query, GetIDataQueryCB, queryTemplate[i + 1], &errmsg) != SQLITE_OK)
      {
         _tprintf(_T("ERROR: SQLite query failed (%hs)\n"), errmsg);
         sqlite3_free(errmsg);
         return false;
      }
      if (queryTemplate[i + 1][0] == 0)
         break;
   }

   shared_ptr<IntegerArray<uint32_t>> targets(GetDataCollectionTargets());
   if (targets == nullptr)
      return false;
   for(int i = 0; i < targets->size(); i++)
   {
      uint32_t id = targets->get(i);

      if (!g_skipDataSchemaMigration)
      {
         for(int j = 0; j < 11; j++)
         {
            if (queryTemplate[j][0] == 0)
               break;

            char query[1024];
            snprintf(query, 1024, queryTemplate[j], id, id);
            if (sqlite3_exec(db, query, NULL, NULL, &errmsg) != SQLITE_OK)
            {
               _tprintf(_T("ERROR: SQLite query failed: %hs (%hs)\n"), query, errmsg);
               sqlite3_free(errmsg);
               return false;
            }
         }
      }

      if (!g_skipDataMigration)
      {
         TCHAR idataTable[128];
         _sntprintf(idataTable, 128, _T("idata_%d"), id);
         if (!excludedTables.contains(idataTable))
         {
            if (!ExportTable(db, idataTable))
            {
               return false;
            }
         }
         else
         {
            _tprintf(_T("Skipping table %s\n"), idataTable);
         }

         _sntprintf(idataTable, 128, _T("tdata_%d"), id);
         if (!excludedTables.contains(idataTable))
         {
            if (!ExportTable(db, idataTable))
            {
               return false;
            }
         }
         else
         {
            _tprintf(_T("Skipping table %s\n"), idataTable);
         }
      }
   }
   return true;
}

/**
 * Export database
 */
void ExportDatabase(char *file, bool skipAudit, bool skipAlarms, bool skipEvent, bool skipSysLog, bool skipTrapLog, const StringList& excludedTables)
{
   if (!ValidateDatabase())
      return;

	// Create new SQLite database
	_unlink(file);
   sqlite3 *db;
	if (sqlite3_open(file, &db) != SQLITE_OK)
	{
		_tprintf(_T("ERROR: unable to open output file\n"));
		return;
	}

   char *errmsg;
   int legacy = 0, major = 0, minor = 0;
   bool success = false;

   if (sqlite3_exec(db, "PRAGMA page_size=65536", nullptr, nullptr, &errmsg) != SQLITE_OK)
   {
      _tprintf(_T("ERROR: cannot set page size for export file (%hs)\n"), errmsg);
      sqlite3_free(errmsg);
      goto cleanup;
   }

	// Setup database schema
   if (!ExecuteSchemaFile(nullptr, db))
      goto cleanup;
   if (!EnumerateModuleSchemas(ExecuteSchemaFile, db))
      goto cleanup;

	// Check that dbschema_sqlite.sql and database have the same schema version
	if ((sqlite3_exec(db, "SELECT var_value FROM metadata WHERE var_name='SchemaVersion'", GetSchemaVersionCB, &legacy, &errmsg) != SQLITE_OK) ||
	    (sqlite3_exec(db, "SELECT var_value FROM metadata WHERE var_name='SchemaVersionMajor'", GetSchemaVersionCB, &major, &errmsg) != SQLITE_OK) ||
	    (sqlite3_exec(db, "SELECT var_value FROM metadata WHERE var_name='SchemaVersionMinor'", GetSchemaVersionCB, &minor, &errmsg) != SQLITE_OK))
	{
		_tprintf(_T("ERROR: SQL query failed (%hs)\n"), errmsg);
		sqlite3_free(errmsg);
		goto cleanup;
	}

	INT32 dbmajor, dbminor;
	if (!DBGetSchemaVersion(g_dbHandle, &dbmajor, &dbminor))
	{
      _tprintf(_T("ERROR: Cannot determine database schema version. Please check that NetXMS server installed correctly.\n"));
      goto cleanup;
	}
	if ((dbmajor != major) || (dbminor != minor))
	{
		_tprintf(_T("ERROR: Schema version mismatch between dbschema_sqlite.sql and your database. Please check that NetXMS server installed correctly.\n"));
		goto cleanup;
	}

	// Export tables
	for(int i = 0; g_tables[i] != NULL; i++)
	{
	   const TCHAR *table = g_tables[i];
	   if ((skipAudit && !_tcscmp(table, _T("audit_log"))) ||
          (skipEvent && !_tcscmp(table, _T("event_log"))) ||
          (skipAlarms && (!_tcscmp(table, _T("alarms")) ||
                         !_tcscmp(table, _T("alarm_notes")) ||
                         !_tcscmp(table, _T("alarm_events")))) ||
          (skipTrapLog && !_tcscmp(table, _T("snmp_trap_log"))) ||
          (skipSysLog && !_tcscmp(table, _T("syslog"))) ||
          ((g_skipDataMigration || g_skipDataSchemaMigration) && !_tcscmp(table, _T("raw_dci_values"))) ||
          !_tcsncmp(table, _T("idata"), 5) ||
          !_tcsncmp(table, _T("tdata"), 5) ||
          excludedTables.contains(table))
	   {
	      _tprintf(_T("Skipping table %s\n"), table);
         continue;
	   }
      if (!ExportTable(db, table))
			goto cleanup;
	}

	ModuleTableExportData data;
	data.db = db;
	data.excludedTables = &excludedTables;
   if (!EnumerateModuleTables(ExportModuleTable, &data))
      goto cleanup;

	if (!g_skipDataMigration || !g_skipDataSchemaMigration)
	{
	   if (DBMgrMetaDataReadInt32(_T("SingeTablePerfData"), 0))
	   {
	      ExportSingleTablePerfData(db, excludedTables);
	   }
	   else
	   {
         ExportMultiTablePerfData(db, excludedTables);
	   }
	}

	success = TRUE;

cleanup:
	sqlite3_close(db);
	_tprintf(success ? _T("Database export complete.\n") : _T("Database export failed.\n"));
}
