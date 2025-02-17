/* 
** NetXMS - Network Management System
** NetXMS Foundation Library
** Copyright (C) 2003-2022 Victor Kirhenshtein
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
** File: strmap.cpp
**
**/

#include "libnetxms.h"
#include "strmap-internal.h"
#include <nxcpapi.h>

/**
 * Copy constructor
 */
StringMap::StringMap(const StringMap &src) : StringMapBase(Ownership::True)
{
	m_objectOwner = src.m_objectOwner;
   m_ignoreCase = src.m_ignoreCase;
   m_objectDestructor = src.m_objectDestructor;

   StringMapEntry *entry, *tmp;
   HASH_ITER(hh, src.m_data, entry, tmp)
   {
      setObject(MemCopyString(m_ignoreCase ? entry->originalKey : entry->key), MemCopyString((TCHAR *)entry->value), true);
   }
}

/**
 * Create string map from NXCP message
 */
StringMap::StringMap(const NXCPMessage& msg, uint32_t baseFieldId, uint32_t sizeFieldId) : StringMapBase(Ownership::True)
{
   addAllFromMessage(msg, baseFieldId, sizeFieldId);
}

/**
 * Assignment
 */
StringMap& StringMap::operator =(const StringMap &src)
{
	clear();
	m_objectOwner = src.m_objectOwner;
   m_ignoreCase = src.m_ignoreCase;
   m_objectDestructor = src.m_objectDestructor;

   StringMapEntry *entry, *tmp;
   HASH_ITER(hh, src.m_data, entry, tmp)
   {
      setObject(MemCopyString(m_ignoreCase ? entry->originalKey : entry->key), MemCopyString((TCHAR *)entry->value), true);
   }
	return *this;
}

/**
 * Add all values from another string map
 */
void StringMap::addAll(const StringMap *src, bool (*filter)(const TCHAR *, const TCHAR *, void *), void *context)
{
   StringMapEntry *entry, *tmp;
   HASH_ITER(hh, src->m_data, entry, tmp)
   {
      const TCHAR *k = src->m_ignoreCase ? entry->originalKey : entry->key;
      if ((filter == NULL) || filter(k, static_cast<TCHAR*>(entry->value), context))
      {
         setObject(MemCopyString(k), MemCopyString(static_cast<TCHAR*>(entry->value)), true);
      }
   }
}

/**
 * Set value from INT32
 */
void StringMap::set(const TCHAR *key, int32_t value)
{
   TCHAR buffer[32];
   set(key, IntegerToString(value, buffer));
}

/**
 * Set value from UINT32
 */
void StringMap::set(const TCHAR *key, uint32_t value)
{
	TCHAR buffer[32];
   set(key, IntegerToString(value, buffer));
}

/**
 * Set value from INT64
 */
void StringMap::set(const TCHAR *key, int64_t value)
{
   TCHAR buffer[64];
   set(key, IntegerToString(value, buffer));
}

/**
 * Set value from UINT64
 */
void StringMap::set(const TCHAR *key, uint64_t value)
{
   TCHAR buffer[64];
   set(key, IntegerToString(value, buffer));
}

/**
 * Get value by key as INT32
 */
int32_t StringMap::getInt32(const TCHAR *key, int32_t defaultValue) const
{
	const TCHAR *value = get(key);
	if (value == nullptr)
		return defaultValue;
	return _tcstol(value, nullptr, 0);
}

/**
 * Get value by key as UINT32
 */
uint32_t StringMap::getUInt32(const TCHAR *key, uint32_t defaultValue) const
{
   const TCHAR *value = get(key);
   if (value == nullptr)
      return defaultValue;
   return _tcstoul(value, nullptr, 0);
}

/**
 * Get value by key as INT64
 */
int64_t StringMap::getInt64(const TCHAR *key, int64_t defaultValue) const
{
   const TCHAR *value = get(key);
   if (value == nullptr)
      return defaultValue;
   return _tcstoll(value, nullptr, 0);
}

/**
 * Get value by key as UINT64
 */
uint64_t StringMap::getUInt64(const TCHAR *key, uint64_t defaultValue) const
{
   const TCHAR *value = get(key);
   if (value == nullptr)
      return defaultValue;
   return _tcstoull(value, nullptr, 0);
}

/**
 * Get value by key as double
 */
double StringMap::getDouble(const TCHAR *key, double defaultValue) const
{
   const TCHAR *value = get(key);
   if (value == nullptr)
      return defaultValue;
   return _tcstod(value, nullptr);
}

/**
 * Get value by key as boolean
 */
bool StringMap::getBoolean(const TCHAR *key, bool defaultValue) const
{
	const TCHAR *value = get(key);
	if (value == nullptr)
		return defaultValue;
	if (!_tcsicmp(value, _T("false")))
		return false;
	if (!_tcsicmp(value, _T("true")))
		return true;
	return _tcstoul(value, nullptr, 0) != 0;
}

/**
 * Fill NXCP message with map data
 */
void StringMap::fillMessage(NXCPMessage *msg, uint32_t baseFieldId, uint32_t sizeFieldId) const
{
   msg->setField(sizeFieldId, static_cast<uint32_t>(size()));
   uint32_t id = baseFieldId;
   StringMapEntry *entry, *tmp;
   HASH_ITER(hh, m_data, entry, tmp)
   {
      msg->setField(id++, m_ignoreCase ? entry->originalKey : entry->key);
      msg->setField(id++, static_cast<TCHAR*>(entry->value));
   }
}

/**
 * Load data from NXCP message
 */
void StringMap::addAllFromMessage(const NXCPMessage& msg, uint32_t baseFieldId, uint32_t sizeFieldId)
{
   int count = msg.getFieldAsInt32(sizeFieldId);
   uint32_t id = baseFieldId;
   for(int i = 0; i < count; i++)
   {
      TCHAR *key = msg.getFieldAsString(id++);
      TCHAR *value = msg.getFieldAsString(id++);
      setPreallocated(key, value);
   }
}

/**
 * Serialize as JSON
 */
json_t *StringMap::toJson() const
{
   json_t *root = json_array();
   StringMapEntry *entry, *tmp;
   HASH_ITER(hh, m_data, entry, tmp)
   {
      json_t *e = json_array();
      json_array_append_new(e, json_string_t(m_ignoreCase ? entry->originalKey : entry->key));
      json_array_append_new(e, json_string_t((TCHAR *)entry->value));
      json_array_append_new(root, e);
   }
   return root;
}
