/* 
** NetXMS - Network Management System
** NetXMS Foundation Library
** Copyright (C) 2003-2018 Victor Kirhenshtein
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
** File: hashsetbase.cpp
**
**/

#include "libnetxms.h"
#include <uthash.h>

/**
 * Entry
 */
struct HashSetEntry
{
   UT_hash_handle hh;
   union
   {
      BYTE d[16];
      void *p;
   } key;
};

/**
 * Delete key
 */
#define DELETE_KEY(m, e) do { if ((m)->m_keylen > 16) MemFree((e)->key.p); } while(0)

/**
 * Get pointer to key
 */
#define GET_KEY(m, e) (((m)->m_keylen <= 16) ? (e)->key.d : (e)->key.p)

/**
 * Constructors
 */
HashSetBase::HashSetBase(unsigned int keylen)
{
	m_data = NULL;
   m_keylen = keylen;
}

/**
 * Destructor
 */
HashSetBase::~HashSetBase()
{
	clear();
}

/**
 * Clear map
 */
void HashSetBase::clear()
{
   HashSetEntry *entry, *tmp;
   HASH_ITER(hh, m_data, entry, tmp)
   {
      HASH_DEL(m_data, entry);
      DELETE_KEY(this, entry);
      MemFree(entry);
   }
}

/**
 * Check if given entry presented in set
 */
bool HashSetBase::_contains(const void *key) const
{
	if (key == NULL)
		return false;

   HashSetEntry *entry;
   HASH_FIND(hh, m_data, key, m_keylen, entry);
   return entry != NULL;
}

/**
 * Put element
 */
void HashSetBase::_put(const void *key)
{
   if ((key == NULL) || _contains(key))
      return;

   HashSetEntry *entry = MemAllocStruct<HashSetEntry>();
   if (m_keylen <= 16)
      memcpy(entry->key.d, key, m_keylen);
   else
      entry->key.p = MemCopyBlock(key, m_keylen);
   HASH_ADD_KEYPTR(hh, m_data, GET_KEY(this, entry), m_keylen, entry);
}

/**
 * Remove element
 */
void HashSetBase::_remove(const void *key)
{
   HashSetEntry *entry;
   HASH_FIND(hh, m_data, key, m_keylen, entry);
   if (entry != NULL)
   {
      HASH_DEL(m_data, entry);
      DELETE_KEY(this, entry);
      MemFree(entry);
   }
}

/**
 * Enumerate entries
 * Returns true if whole map was enumerated and false if enumeration was aborted by callback.
 */
EnumerationCallbackResult HashSetBase::forEach(EnumerationCallbackResult (*cb)(const void *, void *), void *userData) const
{
   EnumerationCallbackResult result = _CONTINUE;
   HashSetEntry *entry, *tmp;
   HASH_ITER(hh, m_data, entry, tmp)
   {
      if (cb(GET_KEY(this, entry), userData) == _STOP)
      {
         result = _STOP;
         break;
      }
   }
   return result;
}

/**
 * Get size
 */
int HashSetBase::size() const
{
   return HASH_COUNT(m_data);
}

/**
 * Hash set iterator
 */
HashSetConstIterator::HashSetConstIterator(const HashSetBase *hashSet)
{
   m_hashSet = hashSet;
   m_curr = NULL;
   m_next = NULL;
}

/**
 * Next element availability indicator
 */
bool HashSetConstIterator::hasNext()
{
   if (m_hashSet->m_data == NULL)
      return false;

   return (m_curr != NULL) ? (m_next != NULL) : true;
}

/**
 * Get next element
 */
void *HashSetConstIterator::next()
{
   if (m_hashSet->m_data == NULL)
      return NULL;

   if (m_curr == NULL)  // iteration not started
   {
      m_curr = m_hashSet->m_data;
   }
   else
   {
      if (m_next == NULL)
         return NULL;
      m_curr = m_next;
   }
   m_next = static_cast<HashSetEntry*>(m_curr->hh.next);
   return GET_KEY(m_hashSet, m_curr);
}

/**
 * Hash set iterator
 */
HashSetIterator::HashSetIterator(HashSetBase *hashSet)
{
   m_hashSet = hashSet;
   m_curr = NULL;
   m_next = NULL;
}

/**
 * Next element availability indicator
 */
bool HashSetIterator::hasNext()
{
   if (m_hashSet->m_data == NULL)
      return false;

   return (m_curr != NULL) ? (m_next != NULL) : true;
}

/**
 * Get next element
 */
void *HashSetIterator::next()
{
   if (m_hashSet->m_data == NULL)
      return NULL;

   if (m_curr == NULL)  // iteration not started
   {
      m_curr = m_hashSet->m_data;
   }
   else
   {
      if (m_next == NULL)
         return NULL;
      m_curr = m_next;
   }
   m_next = static_cast<HashSetEntry*>(m_curr->hh.next);
   return GET_KEY(m_hashSet, m_curr);
}

/**
 * Remove current element
 */
void HashSetIterator::remove()
{
   if (m_curr == NULL)
      return;

   HASH_DEL(m_hashSet->m_data, m_curr);
   DELETE_KEY(m_hashSet, m_curr);
   MemFree(m_curr);
}

/**
 * Remove current element without destroying it
 */
void HashSetIterator::unlink()
{
   if (m_curr == NULL)
      return;

   HASH_DEL(m_hashSet->m_data, m_curr);
   MemFree(m_curr);
}

