/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2014 Victor Kirhenshtein
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
** File: table.cpp
**
**/

#include "libnetxms.h"

/**
 * Create empty table row
 */
TableRow::TableRow(int columnCount)
{
   m_cells = new ObjectArray<TableCell>(columnCount, 8, true);
   for(int i = 0; i < columnCount; i++)
      m_cells->add(new TableCell);
   m_objectId = 0;
}

/**
 * Table row copy constructor
 */
TableRow::TableRow(TableRow *src)
{
   m_cells = new ObjectArray<TableCell>(src->m_cells->size(), 8, true);
   for(int i = 0; i < src->m_cells->size(); i++)
      m_cells->add(new TableCell(src->m_cells->get(i)));
   m_objectId = src->m_objectId;
}

/**
 * Create empty table
 */
Table::Table() : RefCountObject()
{
   m_data = new ObjectArray<TableRow>(32, 32, true);
	m_title = NULL;
   m_source = DS_INTERNAL;
   m_columns = new ObjectArray<TableColumnDefinition>(8, 8, true);
   m_extendedFormat = false;
}

/**
 * Create table from NXCP message
 */
Table::Table(CSCPMessage *msg) : RefCountObject()
{
   m_columns = new ObjectArray<TableColumnDefinition>(8, 8, true);
	createFromMessage(msg);
}

/**
 * Copy constructor
 */
Table::Table(Table *src) : RefCountObject()
{
   m_extendedFormat = src->m_extendedFormat;
   m_data = new ObjectArray<TableRow>(src->m_data->size(), 32, true);
	int i;
   for(i = 0; i < src->m_data->size(); i++)
      m_data->add(new TableRow(src->m_data->get(i)));
   m_title = (src->m_title != NULL) ? _tcsdup(src->m_title) : NULL;
   m_source = src->m_source;
   m_columns = new ObjectArray<TableColumnDefinition>(src->m_columns->size(), 8, true);
	for(i = 0; i < src->m_columns->size(); i++)
      m_columns->add(new TableColumnDefinition(src->m_columns->get(i)));
}

/**
 * Table destructor
 */
Table::~Table()
{
	destroy();
   delete m_columns;
   delete m_data;
}

/**
 * Destroy table data
 */
void Table::destroy()
{
   m_columns->clear();
   m_data->clear();
	safe_free(m_title);
}

/**
 * Create table from NXCP message
 */
void Table::createFromMessage(CSCPMessage *msg)
{
	int i;
	UINT32 dwId;

	int rows = msg->GetVariableLong(VID_TABLE_NUM_ROWS);
	int columns = msg->GetVariableLong(VID_TABLE_NUM_COLS);
	m_title = msg->GetVariableStr(VID_TABLE_TITLE);
   m_source = msg->getFieldAsInt16(VID_DCI_SOURCE_TYPE);
   m_extendedFormat = msg->getFieldAsBoolean(VID_TABLE_EXTENDED_FORMAT);

	for(i = 0, dwId = VID_TABLE_COLUMN_INFO_BASE; i < columns; i++, dwId += 10)
	{
      m_columns->add(new TableColumnDefinition(msg, dwId));
	}
   if (msg->isFieldExist(VID_INSTANCE_COLUMN))
   {
      TCHAR name[MAX_COLUMN_NAME];
      msg->GetVariableStr(VID_INSTANCE_COLUMN, name, MAX_COLUMN_NAME);
      for(i = 0; i < m_columns->size(); i++)
      {
         if (!_tcsicmp(m_columns->get(i)->getName(), name))
         {
            m_columns->get(i)->setInstanceColumn(true);
            break;
         }
      }
   }

	m_data = new ObjectArray<TableRow>(rows, 32, true);
	for(i = 0, dwId = VID_TABLE_DATA_BASE; i < rows; i++)
   {
      TableRow *row = new TableRow(columns);
		m_data->add(row);
      if (m_extendedFormat)
      {
         row->setObjectId(msg->GetVariableLong(dwId++));
         dwId += 9;
      }
      for(int j = 0; j < columns; j++)
      {
         TCHAR *value = msg->GetVariableStr(dwId++);
         if (m_extendedFormat)
         {
            row->setPreallocated(j, value, msg->getFieldAsInt16(dwId++));
            dwId += 8;
         }
         else
         {
            row->setPreallocated(j, value, -1);
         }
      }
   }
}

/**
 * Update table from NXCP message
 */
void Table::updateFromMessage(CSCPMessage *msg)
{
	destroy();
   delete m_data; // will be re-created by createFromMessage
	createFromMessage(msg);
}

/**
 * Fill NXCP message with table data
 */
int Table::fillMessage(CSCPMessage &msg, int offset, int rowLimit)
{
	UINT32 id;

	msg.SetVariable(VID_TABLE_TITLE, CHECK_NULL_EX(m_title));
   msg.SetVariable(VID_DCI_SOURCE_TYPE, (UINT16)m_source);
   msg.SetVariable(VID_TABLE_EXTENDED_FORMAT, (UINT16)(m_extendedFormat ? 1 : 0));

	if (offset == 0)
	{
		msg.SetVariable(VID_TABLE_NUM_ROWS, (UINT32)m_data->size());
		msg.SetVariable(VID_TABLE_NUM_COLS, (UINT32)m_columns->size());

      id = VID_TABLE_COLUMN_INFO_BASE;
      for(int i = 0; i < m_columns->size(); i++, id += 10)
         m_columns->get(i)->fillMessage(&msg, id);
	}
	msg.SetVariable(VID_TABLE_OFFSET, (UINT32)offset);

	int stopRow = (rowLimit == -1) ? m_data->size() : min(m_data->size(), offset + rowLimit);
   id = VID_TABLE_DATA_BASE;
	for(int row = offset; row < stopRow; row++)
	{
      TableRow *r = m_data->get(row);
      if (m_extendedFormat)
      {
			msg.SetVariable(id++, r->getObjectId());
         id += 9;
      }
		for(int col = 0; col < m_columns->size(); col++) 
		{
			const TCHAR *tmp = r->getValue(col);
			msg.SetVariable(id++, CHECK_NULL_EX(tmp));
         if (m_extendedFormat)
         {
            msg.SetVariable(id++, (UINT16)r->getStatus(col));
            id += 8;
         }
		}
	}
	msg.SetVariable(VID_NUM_ROWS, (UINT32)(stopRow - offset));

	if (stopRow == m_data->size())
		msg.setEndOfSequence();
	return stopRow;
}

/**
 * Add new column
 */
int Table::addColumn(const TCHAR *name, INT32 dataType, const TCHAR *displayName, bool isInstance)
{
   m_columns->add(new TableColumnDefinition(name, displayName, dataType, isInstance));
   for(int i = 0; i < m_data->size(); i++)
      m_data->get(i)->addColumn();
	return m_columns->size() - 1;
}

/**
 * Get column index by name
 *
 * @param name column name
 * @return column index or -1 if there are no such column
 */
int Table::getColumnIndex(const TCHAR *name)
{
   for(int i = 0; i < m_columns->size(); i++)
      if (!_tcsicmp(name, m_columns->get(i)->getName()))
         return i;
   return -1;
}

/**
 * Add new row
 */
int Table::addRow()
{
   m_data->add(new TableRow(m_columns->size()));
	return m_data->size() - 1;
}

/**
 * Delete row
 */
void Table::deleteRow(int row)
{
   m_data->remove(row);
}

/**
 * Delete column
 */
void Table::deleteColumn(int col)
{
   if ((col < 0) || (col >= m_columns->size()))
      return;

   m_columns->remove(col);
   for(int i = 0; i < m_data->size(); i++)
      m_data->get(i)->deleteColumn(col);
}

/**
 * Set data at position
 */
void Table::setAt(int nRow, int nCol, const TCHAR *pszData)
{
   TableRow *r = m_data->get(nRow);
   if (r != NULL)
   {
      r->setValue(nCol, pszData);
   }
}

/**
 * Set pre-allocated data at position
 */
void Table::setPreallocatedAt(int nRow, int nCol, TCHAR *pszData)
{
   TableRow *r = m_data->get(nRow);
   if (r != NULL)
   {
      r->setPreallocatedValue(nCol, pszData);
   }
}

/**
 * Set integer data at position
 */
void Table::setAt(int nRow, int nCol, INT32 nData)
{
   TCHAR szBuffer[32];

   _sntprintf(szBuffer, 32, _T("%d"), (int)nData);
   setAt(nRow, nCol, szBuffer);
}

/**
 * Set unsigned integer data at position
 */
void Table::setAt(int nRow, int nCol, UINT32 dwData)
{
   TCHAR szBuffer[32];

   _sntprintf(szBuffer, 32, _T("%u"), (unsigned int)dwData);
   setAt(nRow, nCol, szBuffer);
}

/**
 * Set 64 bit integer data at position
 */
void Table::setAt(int nRow, int nCol, INT64 nData)
{
   TCHAR szBuffer[32];

   _sntprintf(szBuffer, 32, INT64_FMT, nData);
   setAt(nRow, nCol, szBuffer);
}

/**
 * Set unsigned 64 bit integer data at position
 */
void Table::setAt(int nRow, int nCol, UINT64 qwData)
{
   TCHAR szBuffer[32];

   _sntprintf(szBuffer, 32, UINT64_FMT, qwData);
   setAt(nRow, nCol, szBuffer);
}

/**
 * Set floating point data at position
 */
void Table::setAt(int nRow, int nCol, double dData)
{
   TCHAR szBuffer[32];

   _sntprintf(szBuffer, 32, _T("%f"), dData);
   setAt(nRow, nCol, szBuffer);
}

/**
 * Get data from position
 */
const TCHAR *Table::getAsString(int nRow, int nCol)
{
   TableRow *r = m_data->get(nRow);
   return (r != NULL) ? r->getValue(nCol) : NULL;
}

INT32 Table::getAsInt(int nRow, int nCol)
{
   const TCHAR *pszVal;

   pszVal = getAsString(nRow, nCol);
   return pszVal != NULL ? _tcstol(pszVal, NULL, 0) : 0;
}

UINT32 Table::getAsUInt(int nRow, int nCol)
{
   const TCHAR *pszVal;

   pszVal = getAsString(nRow, nCol);
   return pszVal != NULL ? _tcstoul(pszVal, NULL, 0) : 0;
}

INT64 Table::getAsInt64(int nRow, int nCol)
{
   const TCHAR *pszVal;

   pszVal = getAsString(nRow, nCol);
   return pszVal != NULL ? _tcstoll(pszVal, NULL, 0) : 0;
}

UINT64 Table::getAsUInt64(int nRow, int nCol)
{
   const TCHAR *pszVal;

   pszVal = getAsString(nRow, nCol);
   return pszVal != NULL ? _tcstoull(pszVal, NULL, 0) : 0;
}

double Table::getAsDouble(int nRow, int nCol)
{
   const TCHAR *pszVal;

   pszVal = getAsString(nRow, nCol);
   return pszVal != NULL ? _tcstod(pszVal, NULL) : 0;
}

/**
 * Set status of given cell
 */
void Table::setStatusAt(int row, int col, int status)
{
   TableRow *r = m_data->get(row);
   if (r != NULL)
   {
      r->setStatus(col, status);
   }
}

/**
 * Get status of given cell
 */
int Table::getStatus(int nRow, int nCol)
{
   TableRow *r = m_data->get(nRow);
   return (r != NULL) ? r->getStatus(nCol) : -1;
}

/**
 * Add all rows from another table.
 * Identical table format assumed.
 *
 * @param src source table
 */
void Table::addAll(Table *src)
{
   int numColumns = min(m_columns->size(), src->m_columns->size());
   for(int i = 0; i < src->m_data->size(); i++)
   {
      TableRow *dstRow = new TableRow(m_columns->size());
      TableRow *srcRow = src->m_data->get(i);
      for(int j = 0; j < numColumns; j++)
      {
         dstRow->set(j, srcRow->getValue(j), srcRow->getStatus(j));
      }
      m_data->add(dstRow);
   }
}

/**
 * Copy one row from source table
 */
void Table::copyRow(Table *src, int row)
{
   TableRow *srcRow = src->m_data->get(row);
   if (srcRow == NULL)
      return;

   int numColumns = min(m_columns->size(), src->m_columns->size());
   TableRow *dstRow = new TableRow(m_columns->size());

   for(int j = 0; j < numColumns; j++)
   {
      dstRow->set(j, srcRow->getValue(j), srcRow->getStatus(j));
   }

   m_data->add(dstRow);
}

/**
 * Build instance string
 */
void Table::buildInstanceString(int row, TCHAR *buffer, size_t bufLen)
{
   TableRow *r = m_data->get(row);
   if (r == NULL)
   {
      buffer[0] = 0;
      return;
   }

   String instance;
   bool first = true;
   for(int i = 0; i < m_columns->size(); i++)
   {
      if (m_columns->get(i)->isInstanceColumn())
      {
         if (!first)
            instance += _T("~~~");
         first = false;
         const TCHAR *value = r->getValue(i);
         if (value != NULL)
            instance += value;
      }
   }
   nx_strncpy(buffer, (const TCHAR *)instance, bufLen);
}

/**
 * Find row by instance value
 *
 * @return row number or -1 if no such row
 */
int Table::findRowByInstance(const TCHAR *instance)
{
   for(int i = 0; i < m_data->size(); i++)
   {
      TCHAR currInstance[256];
      buildInstanceString(i, currInstance, 256);
      if (!_tcscmp(instance, currInstance))
         return i;
   }
   return -1;
}

/**
 * Create new table column definition
 */
TableColumnDefinition::TableColumnDefinition(const TCHAR *name, const TCHAR *displayName, INT32 dataType, bool isInstance)
{
   m_name = _tcsdup(CHECK_NULL(name));
   m_displayName = (displayName != NULL) ? _tcsdup(displayName) : _tcsdup(m_name);
   m_dataType = dataType;
   m_instanceColumn = isInstance;
}

/**
 * Create copy of existing table column definition
 */
TableColumnDefinition::TableColumnDefinition(TableColumnDefinition *src)
{
   m_name = _tcsdup(src->m_name);
   m_displayName = _tcsdup(src->m_displayName);
   m_dataType = src->m_dataType;
   m_instanceColumn = src->m_instanceColumn;
}

/**
 * Create table column definition from NXCP message
 */
TableColumnDefinition::TableColumnDefinition(CSCPMessage *msg, UINT32 baseId)
{
   m_name = msg->GetVariableStr(baseId);
   if (m_name == NULL)
      m_name = _tcsdup(_T("(null)"));
   m_dataType = msg->GetVariableLong(baseId + 1);
   m_displayName = msg->GetVariableStr(baseId + 2);
   if (m_displayName == NULL)
      m_displayName = _tcsdup(m_name);
   m_instanceColumn = msg->GetVariableShort(baseId + 3) ? true : false;
}

/**
 * Destructor for table column definition
 */
TableColumnDefinition::~TableColumnDefinition()
{
   free(m_name);
   free(m_displayName);
}

/**
 * Fill message with table column definition data
 */
void TableColumnDefinition::fillMessage(CSCPMessage *msg, UINT32 baseId)
{
   msg->SetVariable(baseId, m_name);
   msg->SetVariable(baseId + 1, (UINT32)m_dataType);
   msg->SetVariable(baseId + 2, m_displayName);
   msg->SetVariable(baseId + 3, (WORD)(m_instanceColumn ? 1 : 0));
}
