/* 
** NetXMS - Network Management System
** NetXMS Scripting Language Interpreter
** Copyright (C) 2003-2021 Victor Kirhenshtein
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
** File: table.cpp
**
**/

#include "libnxsl.h"

/**
 * Instance of NXSL_Table class
 */
NXSL_TableClass LIBNXSL_EXPORTABLE g_nxslTableClass;
NXSL_StaticTableClass LIBNXSL_EXPORTABLE g_nxslStaticTableClass;
NXSL_TableRowClass LIBNXSL_EXPORTABLE g_nxslTableRowClass;
NXSL_TableColumnClass LIBNXSL_EXPORTABLE g_nxslTableColumnClass;

/**
 * Table row reference
 */
class TableRowReference
{
private:
   Table *m_table;
   int m_index;

public:
   TableRowReference(Table *table, int index)
   {
      m_table = table;
      m_index = index;
      table->incRefCount();
   }
   ~TableRowReference()
   {
      m_table->decRefCount();
   }

   const TCHAR *get(int col) { return m_table->getAsString(m_index, col); }
   int getIndex() { return m_index; }
   Table *getTable() { return m_table; }

   void set(int col, const TCHAR *value) { m_table->setAt(m_index, col, value); }
};

/**
 * addRow() method
 */
NXSL_METHOD_DEFINITION(Table, addRow)
{
   *result = vm->createValue(static_cast<Table*>(object->getData())->addRow());
   return 0;
}

/**
 * addColumn(name, [type], [displayName], [isInstance]) method
 */
NXSL_METHOD_DEFINITION(Table, addColumn)
{
   if ((argc < 1) || (argc > 4))
      return NXSL_ERR_INVALID_ARGUMENT_COUNT;

   if (!argv[0]->isString())
      return NXSL_ERR_NOT_STRING;

   int dataType = DCI_DT_STRING;
   if (argc >= 2)
   {
      if (!argv[1]->isInteger())
         return NXSL_ERR_NOT_INTEGER;
      dataType = argv[1]->getValueAsInt32();
   }

   const TCHAR *displayName = nullptr;
   if (argc >= 3)
   {
      if (!argv[2]->isString())
         return NXSL_ERR_NOT_STRING;
      displayName = argv[2]->getValueAsCString();
   }

   bool isInstance = false;
   if (argc >= 4)
   {
      if (!argv[3]->isBoolean())
         return NXSL_ERR_NOT_BOOLEAN;
      isInstance = argv[3]->isTrue();
   }

   *result = vm->createValue(static_cast<Table*>(object->getData())->addColumn(argv[0]->getValueAsCString(), dataType, displayName, isInstance));
   return 0;
}

/**
 * deleteColumn(index) method
 */
NXSL_METHOD_DEFINITION(Table, deleteColumn)
{
   if (!argv[0]->isInteger())
      return NXSL_ERR_NOT_INTEGER;

   static_cast<Table*>(object->getData())->deleteColumn(argv[0]->getValueAsInt32());
   *result = vm->createValue();
   return 0;
}

/**
 * deleteRow(index) method
 */
NXSL_METHOD_DEFINITION(Table, deleteRow)
{
   if (!argv[0]->isInteger())
      return NXSL_ERR_NOT_INTEGER;

   static_cast<Table*>(object->getData())->deleteRow(argv[0]->getValueAsInt32());
   *result = vm->createValue();
   return 0;
}

/**
 * findRowByInstance(instance) method
 */
NXSL_METHOD_DEFINITION(Table, findRowByInstance)
{
   if (!argv[0]->isString())
      return NXSL_ERR_NOT_STRING;

   auto table = static_cast<Table*>(object->getData());
   int index = table->findRowByInstance(argv[0]->getValueAsCString());
   *result = (index != -1) ? vm->createValue(new NXSL_Object(vm, &g_nxslTableRowClass, new TableRowReference(table, index))) : vm->createValue();
   return 0;
}

/**
 * findRowIndexByInstance(instance) method
 */
NXSL_METHOD_DEFINITION(Table, findRowIndexByInstance)
{
   if (!argv[0]->isString())
      return NXSL_ERR_NOT_STRING;

   *result = vm->createValue(static_cast<Table*>(object->getData())->findRowByInstance(argv[0]->getValueAsCString()));
   return 0;
}

/**
 * get(row, column) method
 */
NXSL_METHOD_DEFINITION(Table, get)
{
   if (!argv[0]->isInteger())
      return NXSL_ERR_NOT_INTEGER;

   if (!argv[1]->isString())
      return NXSL_ERR_NOT_STRING;

   int columnIndex = argv[1]->isInteger() ?
      argv[1]->getValueAsInt32() :
      static_cast<Table*>(object->getData())->getColumnIndex(argv[1]->getValueAsCString());

   const TCHAR *value = static_cast<Table*>(object->getData())->getAsString(argv[0]->getValueAsInt32(), columnIndex);
   *result = (value != nullptr) ? vm->createValue(value) : vm->createValue();
   return 0;
}

/**
 * getColumnIndex(name) method
 */
NXSL_METHOD_DEFINITION(Table, getColumnIndex)
{
   if (!argv[0]->isString())
      return NXSL_ERR_NOT_STRING;

   *result = vm->createValue((LONG)((Table *)object->getData())->getColumnIndex(argv[0]->getValueAsCString()));
   return 0;
}

/**
 * getColumnName(column) method
 */
NXSL_METHOD_DEFINITION(Table, getColumnName)
{
   if (!argv[0]->isInteger())
      return NXSL_ERR_NOT_INTEGER;

   const TCHAR *value = ((Table *)object->getData())->getColumnName(argv[0]->getValueAsInt32());
   *result = (value != nullptr) ? vm->createValue(value) : vm->createValue();
   return 0;
}

/**
 * set(row, column, value) method
 */
NXSL_METHOD_DEFINITION(Table, set)
{
   if (!argv[0]->isInteger())
      return NXSL_ERR_NOT_INTEGER;
   if (!argv[1]->isString() || !argv[2]->isString())
      return NXSL_ERR_NOT_STRING;

   int columnIndex = argv[1]->isInteger() ?
      argv[1]->getValueAsInt32() :
      ((Table *)object->getData())->getColumnIndex(argv[1]->getValueAsCString());

   ((Table *)object->getData())->setAt(argv[0]->getValueAsInt32(), columnIndex, argv[2]->getValueAsCString());
   *result = vm->createValue();
   return 0;
}

/**
 * Implementation of "Table" class: constructor
 */
NXSL_TableClass::NXSL_TableClass() : NXSL_Class()
{
   setName(_T("Table"));

   NXSL_REGISTER_METHOD(Table, addColumn, -1);
   NXSL_REGISTER_METHOD(Table, addRow, 0);
   NXSL_REGISTER_METHOD(Table, deleteColumn, 1);
   NXSL_REGISTER_METHOD(Table, deleteRow, 1);
   NXSL_REGISTER_METHOD(Table, findRowByInstance, 1);
   NXSL_REGISTER_METHOD(Table, findRowIndexByInstance, 1);
   NXSL_REGISTER_METHOD(Table, get, 2);
   NXSL_REGISTER_METHOD(Table, getColumnIndex, 1);
   NXSL_REGISTER_METHOD(Table, getColumnName, 1);
   NXSL_REGISTER_METHOD(Table, set, 3);
}

/**
 * Implementation of "Table" class: destructor
 */
NXSL_TableClass::~NXSL_TableClass()
{
}

/**
 * Object delete
 */
void NXSL_TableClass::onObjectDelete(NXSL_Object *object)
{
   static_cast<Table*>(object->getData())->decRefCount();
}

/**
 * Implementation of "Table" class: get attribute
 */
NXSL_Value *NXSL_TableClass::getAttr(NXSL_Object *object, const char *attr)
{
   NXSL_Value *value = NXSL_Class::getAttr(object, attr);
   if (value != nullptr)
      return value;

   NXSL_VM *vm = object->vm();
   Table *table = static_cast<Table*>(object->getData());
   if (compareAttributeName(attr, "columnCount"))
   {
      value = vm->createValue((LONG)table->getNumColumns());
   }
   else if (compareAttributeName(attr, "columns"))
   {
      NXSL_Array *columns = new NXSL_Array(vm);
      ObjectArray<TableColumnDefinition> *cd = table->getColumnDefinitions();
      for(int i = 0; i < cd->size(); i++)
      {
         columns->set(i, vm->createValue(new NXSL_Object(vm, &g_nxslTableColumnClass, new TableColumnDefinition(cd->get(i)))));
      }
      value = vm->createValue(columns);
   }
   else if (compareAttributeName(attr, "instanceColumns"))
   {
      NXSL_Array *columns = new NXSL_Array(vm);
      ObjectArray<TableColumnDefinition> *cd = table->getColumnDefinitions();
      for(int i = 0, j = 0; i < cd->size(); i++)
      {
         auto column = cd->get(i);
         if (column->isInstanceColumn())
            columns->set(j++, vm->createValue(new NXSL_Object(vm, &g_nxslTableColumnClass, new TableColumnDefinition(column))));
      }
      value = vm->createValue(columns);
   }
   else if (compareAttributeName(attr, "instanceColumnIndexes"))
   {
      NXSL_Array *columns = new NXSL_Array(vm);
      ObjectArray<TableColumnDefinition> *cd = table->getColumnDefinitions();
      for(int i = 0, j = 0; i < cd->size(); i++)
      {
         auto column = cd->get(i);
         if (column->isInstanceColumn())
            columns->set(j++, vm->createValue(i));
      }
      value = vm->createValue(columns);
   }
   else if (compareAttributeName(attr, "rowCount"))
   {
      value = vm->createValue((LONG)table->getNumRows());
   }
   else if (compareAttributeName(attr, "rows"))
   {
      NXSL_Array *rows = new NXSL_Array(vm);
      for(int i = 0; i < table->getNumRows(); i++)
      {
         rows->set(i, vm->createValue(new NXSL_Object(vm, &g_nxslTableRowClass, new TableRowReference(table, i))));
      }
      value = vm->createValue(rows);
   }
   else if (compareAttributeName(attr, "title"))
   {
      value = vm->createValue(table->getTitle());
   }
   return value;
}

/**
 * Implementation of "StaticTable" class: constructor
 */
NXSL_StaticTableClass::NXSL_StaticTableClass() : NXSL_TableClass()
{
}

/**
 * Implementation of "StaticTable" class: destructor
 */
NXSL_StaticTableClass::~NXSL_StaticTableClass()
{
}

/**
 * Static table: object delete
 */
void NXSL_StaticTableClass::onObjectDelete(NXSL_Object *object)
{
}

/**
 * Implementation of "TableColumn" class: constructor
 */
NXSL_TableColumnClass::NXSL_TableColumnClass() : NXSL_Class()
{
   setName(_T("TableColumn"));
}

/**
 * Implementation of "Table" class: destructor
 */
NXSL_TableColumnClass::~NXSL_TableColumnClass()
{
}

/**
 * Object delete
 */
void NXSL_TableColumnClass::onObjectDelete(NXSL_Object *object)
{
   delete (TableColumnDefinition *)object->getData();
}

/**
 * Implementation of "TableColumn" class: get attribute
 */
NXSL_Value *NXSL_TableColumnClass::getAttr(NXSL_Object *object, const char *attr)
{
   NXSL_Value *value = NXSL_Class::getAttr(object, attr);
   if (value != nullptr)
      return value;

   NXSL_VM *vm = object->vm();
   TableColumnDefinition *tc = static_cast<TableColumnDefinition*>(object->getData());
   if (compareAttributeName(attr, "dataType"))
   {
      value = vm->createValue(tc->getDataType());
   }
   else if (compareAttributeName(attr, "displayName"))
   {
      value = vm->createValue(tc->getDisplayName());
   }
   else if (compareAttributeName(attr, "isInstanceColumn"))
   {
      value = vm->createValue(tc->isInstanceColumn());
   }
   else if (compareAttributeName(attr, "name"))
   {
      value = vm->createValue(tc->getName());
   }
   return value;
}

/**
 * TableRow::get(column) method
 */
NXSL_METHOD_DEFINITION(TableRow, get)
{
   if (!argv[0]->isString())
      return NXSL_ERR_NOT_STRING;

   int columnIndex = argv[0]->isInteger() ?
      argv[0]->getValueAsInt32() :
      ((TableRowReference *)object->getData())->getTable()->getColumnIndex(argv[0]->getValueAsCString());

   const TCHAR *value = ((TableRowReference *)object->getData())->get(columnIndex);
   *result = (value != nullptr) ? vm->createValue(value) : vm->createValue();
   return 0;
}

/**
 * TableRow::set(column, value) method
 */
NXSL_METHOD_DEFINITION(TableRow, set)
{
   if (!argv[0]->isString() || !argv[1]->isString())
      return NXSL_ERR_NOT_STRING;

   int columnIndex = argv[0]->isInteger() ?
      argv[0]->getValueAsInt32() :
      ((TableRowReference *)object->getData())->getTable()->getColumnIndex(argv[0]->getValueAsCString());

   ((TableRowReference *)object->getData())->set(columnIndex, argv[1]->getValueAsCString());
   *result = vm->createValue();
   return 0;
}

/**
 * Implementation of "TableRow" class: constructor
 */
NXSL_TableRowClass::NXSL_TableRowClass() : NXSL_Class()
{
   setName(_T("TableRow"));

   NXSL_REGISTER_METHOD(TableRow, get, 1);
   NXSL_REGISTER_METHOD(TableRow, set, 2);
}

/**
 * Implementation of "TableRow" class: destructor
 */
NXSL_TableRowClass::~NXSL_TableRowClass()
{
}

/**
 * Object delete
 */
void NXSL_TableRowClass::onObjectDelete(NXSL_Object *object)
{
   delete static_cast<TableRowReference*>(object->getData());
}

/**
 * Implementation of "TableRow" class: get attribute
 */
NXSL_Value *NXSL_TableRowClass::getAttr(NXSL_Object *object, const char *attr)
{
   NXSL_Value *value = NXSL_Class::getAttr(object, attr);
   if (value != nullptr)
      return value;

   NXSL_VM *vm = object->vm();
   TableRowReference *row = static_cast<TableRowReference*>(object->getData());
   if (compareAttributeName(attr, "index"))
   {
      value = vm->createValue(row->getIndex());
   }
   else if (compareAttributeName(attr, "instance"))
   {
      TCHAR instance[1024] = _T("");
      row->getTable()->buildInstanceString(row->getIndex(), instance, 1024);
      value = vm->createValue(instance);
   }
   else if (compareAttributeName(attr, "values"))
   {
      NXSL_Array *values = new NXSL_Array(vm);
      for(int i = 0; i < row->getTable()->getNumColumns(); i++)
      {
         const TCHAR *v = row->get(i);
         values->set(i, (v != nullptr) ? vm->createValue(v) : vm->createValue());
      }
      value = vm->createValue(values);
   }
   return value;
}

/**
 * NXSL constructor for "Table" class
 */
int F_Table(int argc, NXSL_Value **argv, NXSL_Value **result, NXSL_VM *vm)
{
   *result = vm->createValue(new NXSL_Object(vm, &g_nxslTableClass, new Table()));
   return 0;
}
