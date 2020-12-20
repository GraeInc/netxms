/*
** NetXMS - Network Management System
** Copyright (C) 2003-2020 Victor Kirhenshtein
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
** File: container.cpp
**
**/

#include "nxcore.h"

/**
 * Default constructor
 */
ConditionObject::ConditionObject() : super()
{
   m_scriptSource = nullptr;
   m_script = nullptr;
   m_dciList = nullptr;
   m_dciCount = 0;
   m_sourceObject = 0;
   m_activeStatus = STATUS_MAJOR;
   m_inactiveStatus = STATUS_NORMAL;
   m_isActive = false;
   m_lastPoll = 0;
   m_queuedForPolling = false;
   m_activationEventCode = EVENT_CONDITION_ACTIVATED;
   m_deactivationEventCode = EVENT_CONDITION_DEACTIVATED;
}

/**
 * Constructor for new objects
 */
ConditionObject::ConditionObject(bool hidden) : super()
{
   m_scriptSource = nullptr;
   m_script = nullptr;
   m_dciList = nullptr;
   m_dciCount = 0;
   m_sourceObject = 0;
   m_activeStatus = STATUS_MAJOR;
   m_inactiveStatus = STATUS_NORMAL;
   m_isHidden = hidden;
   m_isActive = false;
   m_lastPoll = 0;
   m_queuedForPolling = false;
   m_activationEventCode = EVENT_CONDITION_ACTIVATED;
   m_deactivationEventCode = EVENT_CONDITION_DEACTIVATED;
   setCreationTime();
}

/**
 * Destructor
 */
ConditionObject::~ConditionObject()
{
   free(m_dciList);
   free(m_scriptSource);
   delete m_script;
}

/**
 * Load object from database
 */
bool ConditionObject::loadFromDatabase(DB_HANDLE hdb, UINT32 dwId)
{
   TCHAR szQuery[512];
   DB_RESULT hResult;

   m_id = dwId;

   if (!loadCommonProperties(hdb))
      return false;

   // Load properties
   _sntprintf(szQuery, 512, _T("SELECT activation_event,deactivation_event,")
                            _T("source_object,active_status,inactive_status,")
                            _T("script FROM conditions WHERE id=%d"), dwId);
   hResult = DBSelect(hdb, szQuery);
   if (hResult == nullptr)
      return false;     // Query failed

   if (DBGetNumRows(hResult) == 0)
   {
      // No object with given ID in database
      DBFreeResult(hResult);
      return false;
   }

   m_activationEventCode = DBGetFieldULong(hResult, 0, 0);
   m_deactivationEventCode = DBGetFieldULong(hResult, 0, 1);
   m_sourceObject = DBGetFieldULong(hResult, 0, 2);
   m_activeStatus = DBGetFieldLong(hResult, 0, 3);
   m_inactiveStatus = DBGetFieldLong(hResult, 0, 4);
   m_scriptSource = DBGetField(hResult, 0, 5, nullptr, 0);

   DBFreeResult(hResult);

   // Compile script
   m_script = NXSLCompileAndCreateVM(m_scriptSource, szQuery, 512, new NXSL_ServerEnv());
   if (m_script == nullptr)
      nxlog_write(NXLOG_ERROR, _T("Failed to compile evaluation script for condition object %s [%u] (%s)"), m_name, m_id, szQuery);

   // Load DCI map
   _sntprintf(szQuery, 512, _T("SELECT dci_id,node_id,dci_func,num_polls ")
                            _T("FROM cond_dci_map WHERE condition_id=%d ORDER BY sequence_number"), dwId);
   hResult = DBSelect(hdb, szQuery);
   if (hResult == nullptr)
      return false;     // Query failed

   m_dciCount = DBGetNumRows(hResult);
   if (m_dciCount > 0)
   {
      m_dciList = (INPUT_DCI *)malloc(sizeof(INPUT_DCI) * m_dciCount);
      for(int i = 0; i < m_dciCount; i++)
      {
         m_dciList[i].id = DBGetFieldULong(hResult, i, 0);
         m_dciList[i].nodeId = DBGetFieldULong(hResult, i, 1);
         m_dciList[i].function = DBGetFieldLong(hResult, i, 2);
         m_dciList[i].polls = DBGetFieldLong(hResult, i, 3);
      }
   }
   DBFreeResult(hResult);

   return loadACLFromDB(hdb);
}

/**
 * Save object to database
 */
bool ConditionObject::saveToDatabase(DB_HANDLE hdb)
{
   bool success = super::saveToDatabase(hdb);
   if (success && (m_modified & MODIFY_OTHER))
   {
      static const TCHAR *columns[] = { _T("activation_event"), _T("deactivation_event"),
               _T("source_object"), _T("active_status"), _T("inactive_status"), _T("script"), nullptr };
      DB_STATEMENT hStmt = DBPrepareMerge(hdb, _T("conditions"), _T("id"), m_id, columns);
      if (hStmt != nullptr)
      {
         lockProperties();

         DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_activationEventCode);
         DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, m_deactivationEventCode);
         DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, m_sourceObject);
         DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, m_activeStatus);
         DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, m_inactiveStatus);
         DBBind(hStmt, 6, DB_SQLTYPE_TEXT, m_scriptSource, DB_BIND_STATIC);
         DBBind(hStmt, 7, DB_SQLTYPE_INTEGER, m_id);
         success = DBExecute(hStmt);
         DBFreeStatement(hStmt);

         unlockProperties();
      }
      else
      {
         success = false;
      }

      if (success)
         success = executeQueryOnObject(hdb, _T("DELETE FROM cond_dci_map WHERE condition_id=?"));

      lockProperties();
      if (success && (m_dciCount > 0))
      {
         DB_STATEMENT hStmt = DBPrepare(hdb, _T("INSERT INTO cond_dci_map (condition_id,sequence_number,dci_id,node_id,dci_func,num_polls) VALUES (?,?,?,?,?,?)"));
         if (hStmt != nullptr)
         {
            DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_id);
            for(int i = 0; (i < m_dciCount) && success; i++)
            {
               DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, i);
               DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, m_dciList[i].id);
               DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, m_dciList[i].nodeId);
               DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, m_dciList[i].function);
               DBBind(hStmt, 6, DB_SQLTYPE_INTEGER, m_dciList[i].polls);
               success = DBExecute(hStmt);
            }
            DBFreeStatement(hStmt);
         }
         else
         {
            success = false;
         }
      }
      unlockProperties();
   }
   return success;
}

/**
 * Delete object from database
 */
bool ConditionObject::deleteFromDatabase(DB_HANDLE hdb)
{
   bool success = super::deleteFromDatabase(hdb);
   if (success)
      success = executeQueryOnObject(hdb, _T("DELETE FROM conditions WHERE id=?"));
   if (success)
      success = executeQueryOnObject(hdb, _T("DELETE FROM cond_dci_map WHERE condition_id=?"));
   return success;
}

/**
 * Create NXCP message from object
 */
void ConditionObject::fillMessageInternal(NXCPMessage *pMsg, UINT32 userId)
{
   super::fillMessageInternal(pMsg, userId);

   pMsg->setField(VID_SCRIPT, CHECK_NULL_EX(m_scriptSource));
   pMsg->setField(VID_ACTIVATION_EVENT, m_activationEventCode);
   pMsg->setField(VID_DEACTIVATION_EVENT, m_deactivationEventCode);
   pMsg->setField(VID_SOURCE_OBJECT, m_sourceObject);
   pMsg->setField(VID_ACTIVE_STATUS, (WORD)m_activeStatus);
   pMsg->setField(VID_INACTIVE_STATUS, (WORD)m_inactiveStatus);
   pMsg->setField(VID_NUM_ITEMS, m_dciCount);
   UINT32 dwId = VID_DCI_LIST_BASE;
   for(int i = 0; (i < m_dciCount) && (dwId < (VID_DCI_LIST_LAST + 1)); i++)
   {
      pMsg->setField(dwId++, m_dciList[i].id);
      pMsg->setField(dwId++, m_dciList[i].nodeId);
      pMsg->setField(dwId++, (WORD)m_dciList[i].function);
      pMsg->setField(dwId++, (WORD)m_dciList[i].polls);
      pMsg->setField(dwId++, (WORD)GetDCObjectType(m_dciList[i].nodeId, m_dciList[i].id));
      dwId += 5;
   }
}

/**
 * Modify object from NXCP message
 */
UINT32 ConditionObject::modifyFromMessageInternal(NXCPMessage *pRequest)
{
   // Change script
   if (pRequest->isFieldExist(VID_SCRIPT))
   {
      TCHAR szError[1024];

      MemFree(m_scriptSource);
      delete m_script;
      m_scriptSource = pRequest->getFieldAsString(VID_SCRIPT);
      m_script = NXSLCompileAndCreateVM(m_scriptSource, szError, 1024, new NXSL_ServerEnv());
      if (m_script == nullptr)
         nxlog_write(NXLOG_ERROR, _T("Failed to compile evaluation script for condition object %s [%u] (%s)"), m_name, m_id, szError);
   }

   // Change activation event
   if (pRequest->isFieldExist(VID_ACTIVATION_EVENT))
      m_activationEventCode = pRequest->getFieldAsUInt32(VID_ACTIVATION_EVENT);

   // Change deactivation event
   if (pRequest->isFieldExist(VID_DEACTIVATION_EVENT))
      m_deactivationEventCode = pRequest->getFieldAsUInt32(VID_DEACTIVATION_EVENT);

   // Change source object
   if (pRequest->isFieldExist(VID_SOURCE_OBJECT))
      m_sourceObject = pRequest->getFieldAsUInt32(VID_SOURCE_OBJECT);

   // Change active status
   if (pRequest->isFieldExist(VID_ACTIVE_STATUS))
      m_activeStatus = pRequest->getFieldAsUInt16(VID_ACTIVE_STATUS);

   // Change inactive status
   if (pRequest->isFieldExist(VID_INACTIVE_STATUS))
      m_inactiveStatus = pRequest->getFieldAsUInt16(VID_INACTIVE_STATUS);

   // Change DCI list
   if (pRequest->isFieldExist(VID_NUM_ITEMS))
   {
      free(m_dciList);
      m_dciCount = pRequest->getFieldAsUInt32(VID_NUM_ITEMS);
      if (m_dciCount > 0)
      {
         m_dciList = (INPUT_DCI *)malloc(sizeof(INPUT_DCI) * m_dciCount);
         UINT32 dwId = VID_DCI_LIST_BASE;
         for(int i = 0; (i < m_dciCount) && (dwId < (VID_DCI_LIST_LAST + 1)); i++)
         {
            m_dciList[i].id = pRequest->getFieldAsUInt32(dwId++);
            m_dciList[i].nodeId = pRequest->getFieldAsUInt32(dwId++);
            m_dciList[i].function = pRequest->getFieldAsUInt16(dwId++);
            m_dciList[i].polls = pRequest->getFieldAsUInt16(dwId++);
            dwId += 6;
         }

         // Update cache size of DCIs
         for(int i = 0; i < m_dciCount; i++)
         {
            shared_ptr<NetObj> object = FindObjectById(m_dciList[i].nodeId);
            if ((object != nullptr) && object->isDataCollectionTarget())
            {
               static_cast<DataCollectionTarget*>(object.get())->updateDCItemCacheSize(m_dciList[i].id, m_id);
            }
         }
      }
      else
      {
         m_dciList = nullptr;
      }
   }

   return super::modifyFromMessageInternal(pRequest);
}

/**
 * Lock for polling
 */
void ConditionObject::lockForPoll()
{
   m_queuedForPolling = TRUE;
}

/**
 * Poller entry point
 */
void ConditionObject::doPoll(PollerInfo *poller)
{
   poller->startExecution();
   check();
   lockProperties();
   m_queuedForPolling = FALSE;
   m_lastPoll = time(nullptr);
   unlockProperties();
   delete poller;
}

/**
 * Check condition
 */
void ConditionObject::check()
{
   NXSL_Value **ppValueList, *pValue;
   int iOldStatus = m_status;

   if ((m_script == nullptr) || (m_status == STATUS_UNMANAGED) || IsShutdownInProgress())
      return;

   lockProperties();
   ppValueList = (NXSL_Value **)malloc(sizeof(NXSL_Value *) * m_dciCount);
   memset(ppValueList, 0, sizeof(NXSL_Value *) * m_dciCount);
   for(int i = 0; i < m_dciCount; i++)
   {
      shared_ptr<NetObj> pObject = FindObjectById(m_dciList[i].nodeId, OBJECT_NODE);
      if (pObject != nullptr)
      {
         shared_ptr<DCObject> pItem = static_cast<Node*>(pObject.get())->getDCObjectById(m_dciList[i].id, 0);
         if (pItem != nullptr)
         {
            if (pItem->getType() == DCO_TYPE_ITEM)
            {
               ppValueList[i] = static_cast<DCItem*>(pItem.get())->getValueForNXSL(m_script, m_dciList[i].function, m_dciList[i].polls);
            }
            else if (pItem->getType() == DCO_TYPE_TABLE)
            {
               Table *t = static_cast<DCTable*>(pItem.get())->getLastValue();
               if (t != nullptr)
               {
                  ppValueList[i] = m_script->createValue(new NXSL_Object(m_script, &g_nxslTableClass, t));
               }
            }
         }
      }
      if (ppValueList[i] == nullptr)
         ppValueList[i] = m_script->createValue();
   }
   int numValues = m_dciCount;
   unlockProperties();

	// Create array from values
	NXSL_Array *array = new NXSL_Array(m_script);
	for(int i = 0; i < numValues; i++)
	{
		array->set(i + 1, m_script->createValue(ppValueList[i]));
	}
   m_script->setGlobalVariable("$values", m_script->createValue(array));

   DbgPrintf(6, _T("Running evaluation script for condition %d \"%s\""), m_id, m_name);
   if (m_script->run(numValues, ppValueList))
   {
      pValue = m_script->getResult();
      if (pValue->isFalse())
      {
         if (m_isActive)
         {
            // Deactivate condition
            lockProperties();
            m_status = m_inactiveStatus;
            m_isActive = FALSE;
            setModified(MODIFY_RUNTIME);
            unlockProperties();

            PostSystemEvent(m_deactivationEventCode,
                      (m_sourceObject == 0) ? g_dwMgmtNode : m_sourceObject,
                      "dsdd", m_id, m_name, iOldStatus, m_status);

            DbgPrintf(6, _T("Condition %d \"%s\" deactivated"),
                      m_id, m_name);
         }
         else
         {
            DbgPrintf(6, _T("Condition %d \"%s\" still inactive"),
                      m_id, m_name);
            lockProperties();
            if (m_status != m_inactiveStatus)
            {
               m_status = m_inactiveStatus;
               setModified(MODIFY_RUNTIME);
            }
            unlockProperties();
         }
      }
      else
      {
         if (!m_isActive)
         {
            // Activate condition
            lockProperties();
            m_status = m_activeStatus;
            m_isActive = TRUE;
            setModified(MODIFY_RUNTIME);
            unlockProperties();

            PostSystemEvent(m_activationEventCode,
                      (m_sourceObject == 0) ? g_dwMgmtNode : m_sourceObject,
                      "dsdd", m_id, m_name, iOldStatus, m_status);

            DbgPrintf(6, _T("Condition %d \"%s\" activated"),
                      m_id, m_name);
         }
         else
         {
            DbgPrintf(6, _T("Condition %d \"%s\" still active"),
                      m_id, m_name);
            lockProperties();
            if (m_status != m_activeStatus)
            {
               m_status = m_activeStatus;
               setModified(MODIFY_RUNTIME);
            }
            unlockProperties();
         }
      }
   }
   else
   {
      nxlog_write(NXLOG_ERROR, _T("Failed to execute evaluation script for condition object %s [%u] (%s)"), m_name, m_id, m_script->getErrorText());

      lockProperties();
      if (m_status != STATUS_UNKNOWN)
      {
         m_status = STATUS_UNKNOWN;
         setModified(MODIFY_RUNTIME);
      }
      unlockProperties();
   }
   MemFree(ppValueList);

   // Cause parent object(s) to recalculate it's status
   if (iOldStatus != m_status)
   {
      readLockParentList();
      for(int i = 0; i < getParentList().size(); i++)
         getParentList().get(i)->calculateCompoundStatus();
      unlockParentList();
   }
}

/**
 * Determine DCI cache size required by condition object
 */
int ConditionObject::getCacheSizeForDCI(UINT32 itemId, bool noLock)
{
   int nSize = 0;

   if (!noLock)
      lockProperties();
   for(int i = 0; i < m_dciCount; i++)
   {
      if (m_dciList[i].id == itemId)
      {
         switch(m_dciList[i].function)
         {
            case F_LAST:
               nSize = 1;
               break;
            case F_DIFF:
               nSize = 2;
               break;
            default:
               nSize = m_dciList[i].polls;
               break;
         }
         break;
      }
   }
   if (!noLock)
      unlockProperties();
   return nSize;
}

/**
 * Serialize object to JSON
 */
json_t *ConditionObject::toJson()
{
   json_t *root = super::toJson();

   lockProperties();

   json_t *inputs = json_array();
   for(int i = 0; i < m_dciCount; i++)
   {
      json_t *dci = json_object();
      json_object_set_new(dci, "id", json_integer(m_dciList[i].id));
      json_object_set_new(dci, "nodeId", json_integer(m_dciList[i].nodeId));
      json_object_set_new(dci, "function", json_integer(m_dciList[i].function));
      json_object_set_new(dci, "polls", json_integer(m_dciList[i].polls));
      json_array_append_new(inputs, dci);
   }
   json_object_set_new(root, "inputs", inputs);
   json_object_set_new(root, "script", json_string_t(m_scriptSource));
   json_object_set_new(root, "activationEventCode", json_integer(m_activationEventCode));
   json_object_set_new(root, "deactivationEventCode", json_integer(m_deactivationEventCode));
   json_object_set_new(root, "sourceObject", json_integer(m_sourceObject));
   json_object_set_new(root, "activeStatus", json_integer(m_activeStatus));
   json_object_set_new(root, "inactiveStatus", json_integer(m_inactiveStatus));
   json_object_set_new(root, "isActive", json_boolean(m_isActive));
   json_object_set_new(root, "lastPoll", json_integer(m_lastPoll));

   unlockProperties();
   return root;
}
