/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2011 Victor Kirhenshtein
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
** File: netmap.cpp
**
**/

#include "nxcore.h"


/**
 * Default constructor
 */
Dashboard::Dashboard() : Container()
{
	m_elements = new ObjectArray<DashboardElement>();
	m_elements->setOwner(true);
	m_numColumns = 1;
	m_options = 0;
	m_iStatus = STATUS_NORMAL;
}

/**
 * Constructor for creating new dashboard object
 */
Dashboard::Dashboard(const TCHAR *name) : Container(name, 0)
{
	m_elements = new ObjectArray<DashboardElement>();
	m_elements->setOwner(true);
	m_numColumns = 1;
	m_options = 0;
	m_iStatus = STATUS_NORMAL;
}

/**
 * Destructor
 */
Dashboard::~Dashboard()
{
	delete m_elements;
}

/**
 * Redefined status calculation
 */
void Dashboard::calculateCompoundStatus(BOOL bForcedRecalc)
{
   m_iStatus = STATUS_NORMAL;
}

/**
 * Create object from database
 */
BOOL Dashboard::CreateFromDB(UINT32 dwId)
{
	if (!Container::CreateFromDB(dwId))
		return FALSE;

	m_iStatus = STATUS_NORMAL;

	TCHAR query[256];
	_sntprintf(query, 256, _T("SELECT num_columns,options FROM dashboards WHERE id=%d"), (int)dwId);
	DB_RESULT hResult = DBSelect(g_hCoreDB, query);
	if (hResult == NULL)
		return FALSE;
	if (DBGetNumRows(hResult) > 0)
	{
		m_numColumns = (int)DBGetFieldLong(hResult, 0, 0);
		m_options = DBGetFieldULong(hResult, 0, 1);
	}
	DBFreeResult(hResult);

	_sntprintf(query, 256, _T("SELECT element_type,element_data,layout_data FROM dashboard_elements ")
								  _T("WHERE dashboard_id=%d ORDER BY element_id"), (int)dwId);
	hResult = DBSelect(g_hCoreDB, query);
	if (hResult == NULL)
		return FALSE;

	int count = DBGetNumRows(hResult);
	for(int i = 0; i < count; i++)
	{
		DashboardElement *e = new DashboardElement;
		e->m_type = (int)DBGetFieldLong(hResult, i, 0);
		e->m_data = DBGetField(hResult, i, 1, NULL, 0);
		e->m_layout = DBGetField(hResult, i, 2, NULL, 0);
		m_elements->add(e);
	}

	DBFreeResult(hResult);
	return TRUE;
}

/**
 * Save object to database
 */
BOOL Dashboard::SaveToDB(DB_HANDLE hdb)
{
	TCHAR query[256];

	LockData();

	// Check for object's existence in database
	bool isNewObject = true;
   _sntprintf(query, 256, _T("SELECT id FROM dashboards WHERE id=%d"), (int)m_dwId);
   DB_RESULT hResult = DBSelect(hdb, query);
   if (hResult != NULL)
   {
      if (DBGetNumRows(hResult) > 0)
         isNewObject = false;
      DBFreeResult(hResult);
   }

	if (isNewObject)
      _sntprintf(query, 256,
                 _T("INSERT INTO dashboards (id,num_columns,options) VALUES (%d,%d,%d)"),
					  (int)m_dwId, m_numColumns, (int)m_options);
   else
      _sntprintf(query, 256,
                 _T("UPDATE dashboards SET num_columns=%d,options=%d WHERE id=%d"),
					  m_numColumns, (int)m_options, (int)m_dwId);
   if (!DBQuery(hdb, query))
		goto fail;

   // Save elements
   _sntprintf(query, 256, _T("DELETE FROM dashboard_elements WHERE dashboard_id=%d"), (int)m_dwId);
   if (!DBQuery(hdb, query))
		goto fail;
   for(int i = 0; i < m_elements->size(); i++)
   {
		DashboardElement *element = m_elements->get(i);
		String data = DBPrepareString(hdb, element->m_data);
		String layout = DBPrepareString(hdb, element->m_layout);
		int len = data.getSize() + layout.getSize() + 256;
		TCHAR *eq = (TCHAR *)malloc(len * sizeof(TCHAR));
      _sntprintf(eq, len, _T("INSERT INTO dashboard_elements (dashboard_id,element_id,element_type,element_data,layout_data) VALUES (%d,%d,%d,%s,%s)"),
		           (int)m_dwId, i, element->m_type, (const TCHAR *)data, (const TCHAR *)layout);
      if (!DBQuery(hdb, eq))
		{
			free(eq);
			goto fail;
		}
		free(eq);
   }

	UnlockData();
	return Container::SaveToDB(hdb);

fail:
	UnlockData();
	return FALSE;
}

/**
 * Delete object from database
 */
bool Dashboard::deleteFromDB(DB_HANDLE hdb)
{
   bool success = Container::deleteFromDB(hdb);
   if (success)
      success = executeQueryOnObject(hdb, _T("DELETE FROM dashboards WHERE id=?"));
   if (success)
      success = executeQueryOnObject(hdb, _T("DELETE FROM dashboard_elements WHERE dashboard_id=?"));
   return success;
}

/**
 * Create NXCP message with object's data
 */
void Dashboard::CreateMessage(CSCPMessage *msg)
{
	Container::CreateMessage(msg);
	msg->SetVariable(VID_NUM_COLUMNS, (WORD)m_numColumns);
	msg->SetVariable(VID_FLAGS, m_options);
	msg->SetVariable(VID_NUM_ELEMENTS, (UINT32)m_elements->size());

	UINT32 varId = VID_ELEMENT_LIST_BASE;
	for(int i = 0; i < m_elements->size(); i++)
	{
		DashboardElement *element = m_elements->get(i);
		msg->SetVariable(varId++, (WORD)element->m_type);
		msg->SetVariable(varId++, CHECK_NULL_EX(element->m_data));
		msg->SetVariable(varId++, CHECK_NULL_EX(element->m_layout));
		varId += 7;
	}
}

/**
 * Modify object from NXCP message
 */
UINT32 Dashboard::ModifyFromMessage(CSCPMessage *request, BOOL alreadyLocked)
{
	if (!alreadyLocked)
		LockData();

	if (request->isFieldExist(VID_NUM_COLUMNS))
		m_numColumns = (int)request->GetVariableShort(VID_NUM_COLUMNS);

	if (request->isFieldExist(VID_FLAGS))
		m_options = (int)request->GetVariableLong(VID_FLAGS);

	if (request->isFieldExist(VID_NUM_ELEMENTS))
	{
		m_elements->clear();

		int count = (int)request->GetVariableLong(VID_NUM_ELEMENTS);
		UINT32 varId = VID_ELEMENT_LIST_BASE;
		for(int i = 0; i < count; i++)
		{
			DashboardElement *e = new DashboardElement;
			e->m_type = (int)request->GetVariableShort(varId++);
			e->m_data = request->GetVariableStr(varId++);
			e->m_layout = request->GetVariableStr(varId++);
			varId += 7;
			m_elements->add(e);
		}
	}

	return Container::ModifyFromMessage(request, TRUE);
}

/**
 * Called by client session handler to check if threshold summary should be shown for this object.
 */
bool Dashboard::showThresholdSummary()
{
	return false;
}
