/* 
** NetXMS - Network Management System
** NetXMS Scripting Language Interpreter
** Copyright (C) 2003-2020 Victor Kirhenshtein
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
** File: hashmap.cpp
**
**/

#include "libnxsl.h"

/**
 * Object destructor for string map of NXSL_Value objects
 */
void NXSL_StringValueMap::destructor(void *object, StringMapBase *map)
{
    static_cast<NXSL_StringValueMap*>(map)->m_vm->destroyValue(static_cast<NXSL_Value*>(object));
}

/**
 * Create empty hash map
 */
NXSL_HashMap::NXSL_HashMap(NXSL_ValueManager *vm) : NXSL_HandleCountObject(vm)
{
   m_values = new NXSL_StringValueMap(vm, Ownership::True);
}

/**
 * Create copy of existing hash map
 */
NXSL_HashMap::NXSL_HashMap(const NXSL_HashMap& src) : NXSL_HandleCountObject(src.m_vm)
{
   m_values = new NXSL_StringValueMap(src.m_vm, Ownership::True);
   StructArray<KeyValuePair<void>> *values = src.m_values->toArray();
   for(int i = 0; i < values->size(); i++)
   {
      KeyValuePair<void> *p = values->get(i);
      m_values->set(p->key, m_vm->createValue(static_cast<const NXSL_Value*>(p->value)));
   }
   delete values;
}

/**
 * Destructor
 */
NXSL_HashMap::~NXSL_HashMap()
{
   delete m_values;
}

/**
 * Get keys as array
 */
NXSL_Value *NXSL_HashMap::getKeys() const
{
   NXSL_Array *array = new NXSL_Array(m_vm);
   StructArray<KeyValuePair<void>> *values = m_values->toArray();
   for(int i = 0; i < values->size(); i++)
   {
      KeyValuePair<void> *p = values->get(i);
      array->append(m_vm->createValue(p->key));
   }
   delete values;
   return m_vm->createValue(array);
}

/**
 * Get values as array
 */
NXSL_Value *NXSL_HashMap::getValues() const
{
   NXSL_Array *array = new NXSL_Array(m_vm);
   StructArray<KeyValuePair<void>> *values = m_values->toArray();
   for(int i = 0; i < values->size(); i++)
   {
      KeyValuePair<void> *p = values->get(i);
      array->append(m_vm->createValue(static_cast<const NXSL_Value*>(p->value)));
   }
   delete values;
   return m_vm->createValue(array);
}

/**
 * Get hash map as string map
 * Resulting string map is dynamically allocated and should be destroyed by caller
 */
StringMap *NXSL_HashMap::toStringMap() const
{
   StringMap *map = new StringMap();
   toStringMap(map);
   return map;
}

/**
 * Get hash map as string map
 */
void NXSL_HashMap::toStringMap(StringMap *map) const
{
   StructArray<KeyValuePair<void>> *values = m_values->toArray();
   for(int i = 0; i < values->size(); i++)
   {
      KeyValuePair<void> *p = values->get(i);
      const TCHAR *s = const_cast<NXSL_Value*>(static_cast<const NXSL_Value*>(p->value))->getValueAsCString();
      if (s != nullptr)
         map->set(p->key, s);
   }
   delete values;
}

/**
 * Convert hash map to string (recursively for array and hash map values)
 */
void NXSL_HashMap::toString(StringBuffer *stringBuffer, const TCHAR *separator, bool withBrackets) const
{
   if (withBrackets)
      stringBuffer->append(_T("{"));

   StructArray<KeyValuePair<void>> *values = m_values->toArray();
   for(int i = 0; i < values->size(); i++)
   {
      KeyValuePair<void> *p = values->get(i);
      if (!stringBuffer->isEmpty() && (!withBrackets || (i > 0)))
         stringBuffer->append(separator);
      stringBuffer->append(p->key);
      stringBuffer->append(_T("="));
      NXSL_Value *e = const_cast<NXSL_Value*>(static_cast<const NXSL_Value*>(p->value));
      if (e->isArray())
      {
         e->getValueAsArray()->toString(stringBuffer, separator, withBrackets);
      }
      else if (e->isHashMap())
      {
         e->getValueAsHashMap()->toString(stringBuffer, separator, withBrackets);
      }
      else
      {
         stringBuffer->append(e->getValueAsCString());
      }
   }
   delete values;

   if (withBrackets)
      stringBuffer->append(_T("}"));
}
