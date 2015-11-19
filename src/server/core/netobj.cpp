/*
** NetXMS - Network Management System
** Copyright (C) 2003-2015 Victor Kirhenshtein
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
** File: netobj.cpp
**
**/

#include "nxcore.h"

/**
 * Default constructor
 */
NetObj::NetObj()
{
   int i;

   m_id = 0;
   m_dwRefCount = 0;
   m_mutexProperties = MutexCreate();
   m_mutexRefCount = MutexCreate();
   m_mutexACL = MutexCreate();
   m_rwlockParentList = RWLockCreate();
   m_rwlockChildList = RWLockCreate();
   m_iStatus = STATUS_UNKNOWN;
   m_name[0] = 0;
   m_pszComments = NULL;
   m_isModified = false;
   m_isDeleted = false;
   m_isHidden = false;
	m_isSystem = false;
	m_maintenanceMode = false;
	m_maintenanceEventId = 0;
   m_dwChildCount = 0;
   m_pChildList = NULL;
   m_dwParentCount = 0;
   m_pParentList = NULL;
   m_pAccessList = new AccessList;
   m_bInheritAccessRights = TRUE;
	m_dwNumTrustedNodes = 0;
	m_pdwTrustedNodes = NULL;
   m_pollRequestor = NULL;
   m_iStatusCalcAlg = SA_CALCULATE_DEFAULT;
   m_iStatusPropAlg = SA_PROPAGATE_DEFAULT;
   m_iFixedStatus = STATUS_WARNING;
   m_iStatusShift = 0;
   m_iStatusSingleThreshold = 75;
   m_dwTimeStamp = 0;
   for(i = 0; i < 4; i++)
   {
      m_iStatusTranslation[i] = i + 1;
      m_iStatusThresholds[i] = 80 - i * 20;
   }
	m_submapId = 0;
   m_moduleData = NULL;
   m_postalAddress = new PostalAddress();
   m_dashboards = new IntegerArray<UINT32>();
}

/**
 * Destructor
 */
NetObj::~NetObj()
{
   MutexDestroy(m_mutexProperties);
   MutexDestroy(m_mutexRefCount);
   MutexDestroy(m_mutexACL);
   RWLockDestroy(m_rwlockParentList);
   RWLockDestroy(m_rwlockChildList);
   safe_free(m_pChildList);
   safe_free(m_pParentList);
   delete m_pAccessList;
	safe_free(m_pdwTrustedNodes);
   safe_free(m_pszComments);
   delete m_moduleData;
   delete m_postalAddress;
   delete m_dashboards;
}

/**
 * Create object from database data
 */
bool NetObj::loadFromDatabase(DB_HANDLE hdb, UINT32 dwId)
{
   return false;     // Abstract objects cannot be loaded from database
}

/**
 * Save object to database
 */
BOOL NetObj::saveToDatabase(DB_HANDLE hdb)
{
   return FALSE;     // Abstract objects cannot be saved to database
}

/**
 * Parameters for DeleteModuleDataCallback and SaveModuleDataCallback
 */
struct ModuleDataDatabaseCallbackParams
{
   UINT32 id;
   DB_HANDLE hdb;
};

/**
 * Callback for deleting module data from database
 */
static EnumerationCallbackResult DeleteModuleDataCallback(const TCHAR *key, const void *value, void *data)
{
   return ((ModuleData *)value)->deleteFromDatabase(((ModuleDataDatabaseCallbackParams *)data)->hdb, ((ModuleDataDatabaseCallbackParams *)data)->id) ? _CONTINUE : _STOP;
}

/**
 * Delete object from database
 */
bool NetObj::deleteFromDatabase(DB_HANDLE hdb)
{
   // Delete ACL
   bool success = executeQueryOnObject(hdb, _T("DELETE FROM acl WHERE object_id=?"));
   if (success)
      success = executeQueryOnObject(hdb, _T("DELETE FROM object_properties WHERE object_id=?"));
   if (success)
      success = executeQueryOnObject(hdb, _T("DELETE FROM object_custom_attributes WHERE object_id=?"));

   // Delete events
   if (success && ConfigReadInt(_T("DeleteEventsOfDeletedObject"), 1))
   {
      success = executeQueryOnObject(hdb, _T("DELETE FROM event_log WHERE event_source=?"));
   }

   // Delete alarms
   if (success && ConfigReadInt(_T("DeleteAlarmsOfDeletedObject"), 1))
   {
      success = DeleteObjectAlarms(m_id, hdb);
   }

   // Delete module data
   if (success && (m_moduleData != NULL))
   {
      ModuleDataDatabaseCallbackParams data;
      data.id = m_id;
      data.hdb = hdb;
      success = (m_moduleData->forEach(DeleteModuleDataCallback, &data) == _CONTINUE);
   }

   return success;
}

/**
 * Load common object properties from database
 */
bool NetObj::loadCommonProperties(DB_HANDLE hdb)
{
   bool success = false;

   // Load access options
	DB_STATEMENT hStmt = DBPrepare(hdb,
	                          _T("SELECT name,status,is_deleted,")
                             _T("inherit_access_rights,last_modified,status_calc_alg,")
                             _T("status_prop_alg,status_fixed_val,status_shift,")
                             _T("status_translation,status_single_threshold,")
                             _T("status_thresholds,comments,is_system,")
									  _T("location_type,latitude,longitude,location_accuracy,")
									  _T("location_timestamp,guid,image,submap_id,country,city,")
                             _T("street_address,postcode,maint_mode,maint_event_id FROM object_properties ")
                             _T("WHERE object_id=?"));
	if (hStmt != NULL)
	{
		DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_id);
		DB_RESULT hResult = DBSelectPrepared(hStmt);
		if (hResult != NULL)
		{
			if (DBGetNumRows(hResult) > 0)
			{
				DBGetField(hResult, 0, 0, m_name, MAX_OBJECT_NAME);
				m_iStatus = DBGetFieldLong(hResult, 0, 1);
				m_isDeleted = DBGetFieldLong(hResult, 0, 2) ? true : false;
				m_bInheritAccessRights = DBGetFieldLong(hResult, 0, 3) ? TRUE : FALSE;
				m_dwTimeStamp = DBGetFieldULong(hResult, 0, 4);
				m_iStatusCalcAlg = DBGetFieldLong(hResult, 0, 5);
				m_iStatusPropAlg = DBGetFieldLong(hResult, 0, 6);
				m_iFixedStatus = DBGetFieldLong(hResult, 0, 7);
				m_iStatusShift = DBGetFieldLong(hResult, 0, 8);
				DBGetFieldByteArray(hResult, 0, 9, m_iStatusTranslation, 4, STATUS_WARNING);
				m_iStatusSingleThreshold = DBGetFieldLong(hResult, 0, 10);
				DBGetFieldByteArray(hResult, 0, 11, m_iStatusThresholds, 4, 50);
				safe_free(m_pszComments);
				m_pszComments = DBGetField(hResult, 0, 12, NULL, 0);
				m_isSystem = DBGetFieldLong(hResult, 0, 13) ? true : false;

				int locType = DBGetFieldLong(hResult, 0, 14);
				if (locType != GL_UNSET)
				{
					TCHAR lat[32], lon[32];

					DBGetField(hResult, 0, 15, lat, 32);
					DBGetField(hResult, 0, 16, lon, 32);
					m_geoLocation = GeoLocation(locType, lat, lon, DBGetFieldLong(hResult, 0, 17), DBGetFieldULong(hResult, 0, 18));
				}
				else
				{
					m_geoLocation = GeoLocation();
				}

				m_guid = DBGetFieldGUID(hResult, 0, 19);
				m_image = DBGetFieldGUID(hResult, 0, 20);
				m_submapId = DBGetFieldULong(hResult, 0, 21);

            TCHAR country[64], city[64], streetAddress[256], postcode[32];
            DBGetField(hResult, 0, 22, country, 64);
            DBGetField(hResult, 0, 23, city, 64);
            DBGetField(hResult, 0, 24, streetAddress, 256);
            DBGetField(hResult, 0, 25, postcode, 32);
            delete m_postalAddress;
            m_postalAddress = new PostalAddress(country, city, streetAddress, postcode);

            m_maintenanceMode = DBGetFieldLong(hResult, 0, 26) ? true : false;
            m_maintenanceEventId = DBGetFieldUInt64(hResult, 0, 27);

				success = true;
			}
			DBFreeResult(hResult);
		}
		DBFreeStatement(hStmt);
	}

	// Load custom attributes
	if (success)
	{
		hStmt = DBPrepare(hdb, _T("SELECT attr_name,attr_value FROM object_custom_attributes WHERE object_id=?"));
		if (hStmt != NULL)
		{
			DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_id);
			DB_RESULT hResult = DBSelectPrepared(hStmt);
			if (hResult != NULL)
			{
				int i, count;
				TCHAR *name, *value;

				count = DBGetNumRows(hResult);
				for(i = 0; i < count; i++)
				{
		   		name = DBGetField(hResult, i, 0, NULL, 0);
		   		if (name != NULL)
		   		{
						value = DBGetField(hResult, i, 1, NULL, 0);
						if (value != NULL)
						{
							m_customAttributes.setPreallocated(name, value);
						}
					}
				}
				DBFreeResult(hResult);
			}
			else
			{
				success = false;
			}
			DBFreeStatement(hStmt);
		}
		else
		{
			success = false;
		}
	}

   // Load associated dashboards
   if (success)
   {
      hStmt = DBPrepare(hdb, _T("SELECT dashboard_id FROM dashboard_associations WHERE object_id=?"));
      if (hStmt != NULL)
      {
         DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_id);
         DB_RESULT hResult = DBSelectPrepared(hStmt);
         if (hResult != NULL)
         {
            int count = DBGetNumRows(hResult);
            for(int i = 0; i < count; i++)
            {
               m_dashboards->add(DBGetFieldULong(hResult, i, 0));
            }
            DBFreeResult(hResult);
         }
         else
         {
            success = false;
         }
         DBFreeStatement(hStmt);
      }
      else
      {
         success = false;
      }
   }

	if (success)
		success = loadTrustedNodes(hdb);

	if (!success)
		DbgPrintf(4, _T("NetObj::loadCommonProperties() failed for object %s [%ld] class=%d"), m_name, (long)m_id, getObjectClass());

   return success;
}

/**
 * Callback for saving custom attribute in database
 */
static EnumerationCallbackResult SaveAttributeCallback(const TCHAR *key, const void *value, void *data)
{
   DB_STATEMENT hStmt = (DB_STATEMENT)data;
   DBBind(hStmt, 2, DB_SQLTYPE_VARCHAR, key, DB_BIND_STATIC);
   DBBind(hStmt, 3, DB_SQLTYPE_VARCHAR, (const TCHAR *)value, DB_BIND_STATIC);
   return DBExecute(hStmt) ? _CONTINUE : _STOP;
}

/**
 * Callback for saving module data in database
 */
static EnumerationCallbackResult SaveModuleDataCallback(const TCHAR *key, const void *value, void *data)
{
   return ((ModuleData *)value)->saveToDatabase(((ModuleDataDatabaseCallbackParams *)data)->hdb, ((ModuleDataDatabaseCallbackParams *)data)->id) ? _CONTINUE : _STOP;
}

/**
 * Save common object properties to database
 */
bool NetObj::saveCommonProperties(DB_HANDLE hdb)
{
	DB_STATEMENT hStmt;
	if (IsDatabaseRecordExist(hdb, _T("object_properties"), _T("object_id"), m_id))
	{
		hStmt = DBPrepare(hdb,
                    _T("UPDATE object_properties SET name=?,status=?,")
                    _T("is_deleted=?,inherit_access_rights=?,")
                    _T("last_modified=?,status_calc_alg=?,status_prop_alg=?,")
                    _T("status_fixed_val=?,status_shift=?,status_translation=?,")
                    _T("status_single_threshold=?,status_thresholds=?,")
                    _T("comments=?,is_system=?,location_type=?,latitude=?,")
						  _T("longitude=?,location_accuracy=?,location_timestamp=?,")
						  _T("guid=?,image=?,submap_id=?,country=?,city=?,")
                    _T("street_address=?,postcode=?,maint_mode=?,maint_event_id=? WHERE object_id=?"));
	}
	else
	{
		hStmt = DBPrepare(hdb,
                    _T("INSERT INTO object_properties (name,status,is_deleted,")
                    _T("inherit_access_rights,last_modified,status_calc_alg,")
                    _T("status_prop_alg,status_fixed_val,status_shift,status_translation,")
                    _T("status_single_threshold,status_thresholds,comments,is_system,")
						  _T("location_type,latitude,longitude,location_accuracy,location_timestamp,")
						  _T("guid,image,submap_id,country,city,street_address,postcode,maint_mode,")
						  _T("maint_event_id,object_id) ")
                    _T("VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
	}
	if (hStmt == NULL)
		return FALSE;

   TCHAR szTranslation[16], szThresholds[16], lat[32], lon[32];
   for(int i = 0, j = 0; i < 4; i++, j += 2)
   {
      _sntprintf(&szTranslation[j], 16 - j, _T("%02X"), (BYTE)m_iStatusTranslation[i]);
      _sntprintf(&szThresholds[j], 16 - j, _T("%02X"), (BYTE)m_iStatusThresholds[i]);
   }
	_sntprintf(lat, 32, _T("%f"), m_geoLocation.getLatitude());
	_sntprintf(lon, 32, _T("%f"), m_geoLocation.getLongitude());

	DBBind(hStmt, 1, DB_SQLTYPE_VARCHAR, m_name, DB_BIND_STATIC);
	DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, (LONG)m_iStatus);
   DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, (LONG)(m_isDeleted ? 1 : 0));
	DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, (LONG)m_bInheritAccessRights);
	DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, (LONG)m_dwTimeStamp);
	DBBind(hStmt, 6, DB_SQLTYPE_INTEGER, (LONG)m_iStatusCalcAlg);
	DBBind(hStmt, 7, DB_SQLTYPE_INTEGER, (LONG)m_iStatusPropAlg);
	DBBind(hStmt, 8, DB_SQLTYPE_INTEGER, (LONG)m_iFixedStatus);
	DBBind(hStmt, 9, DB_SQLTYPE_INTEGER, (LONG)m_iStatusShift);
	DBBind(hStmt, 10, DB_SQLTYPE_VARCHAR, szTranslation, DB_BIND_STATIC);
	DBBind(hStmt, 11, DB_SQLTYPE_INTEGER, (LONG)m_iStatusSingleThreshold);
	DBBind(hStmt, 12, DB_SQLTYPE_VARCHAR, szThresholds, DB_BIND_STATIC);
	DBBind(hStmt, 13, DB_SQLTYPE_VARCHAR, m_pszComments, DB_BIND_STATIC);
   DBBind(hStmt, 14, DB_SQLTYPE_INTEGER, (LONG)(m_isSystem ? 1 : 0));
	DBBind(hStmt, 15, DB_SQLTYPE_INTEGER, (LONG)m_geoLocation.getType());
	DBBind(hStmt, 16, DB_SQLTYPE_VARCHAR, lat, DB_BIND_STATIC);
	DBBind(hStmt, 17, DB_SQLTYPE_VARCHAR, lon, DB_BIND_STATIC);
	DBBind(hStmt, 18, DB_SQLTYPE_INTEGER, (LONG)m_geoLocation.getAccuracy());
	DBBind(hStmt, 19, DB_SQLTYPE_INTEGER, (UINT32)m_geoLocation.getTimestamp());
	DBBind(hStmt, 20, DB_SQLTYPE_VARCHAR, m_guid);
	DBBind(hStmt, 21, DB_SQLTYPE_VARCHAR, m_image);
	DBBind(hStmt, 22, DB_SQLTYPE_INTEGER, m_submapId);
	DBBind(hStmt, 23, DB_SQLTYPE_VARCHAR, m_postalAddress->getCountry(), DB_BIND_STATIC);
	DBBind(hStmt, 24, DB_SQLTYPE_VARCHAR, m_postalAddress->getCity(), DB_BIND_STATIC);
	DBBind(hStmt, 25, DB_SQLTYPE_VARCHAR, m_postalAddress->getStreetAddress(), DB_BIND_STATIC);
	DBBind(hStmt, 26, DB_SQLTYPE_VARCHAR, m_postalAddress->getPostCode(), DB_BIND_STATIC);
   DBBind(hStmt, 27, DB_SQLTYPE_VARCHAR, m_maintenanceMode ? _T("1") : _T("0"), DB_BIND_STATIC);
   DBBind(hStmt, 28, DB_SQLTYPE_BIGINT, m_maintenanceEventId);
	DBBind(hStmt, 29, DB_SQLTYPE_INTEGER, m_id);

   bool success = DBExecute(hStmt);
	DBFreeStatement(hStmt);

   // Save custom attributes
   if (success)
   {
		TCHAR szQuery[512];
		_sntprintf(szQuery, 512, _T("DELETE FROM object_custom_attributes WHERE object_id=%d"), m_id);
      success = DBQuery(hdb, szQuery);
		if (success)
		{
			hStmt = DBPrepare(hdb, _T("INSERT INTO object_custom_attributes (object_id,attr_name,attr_value) VALUES (?,?,?)"));
			if (hStmt != NULL)
			{
				DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_id);
            success = (m_customAttributes.forEach(SaveAttributeCallback, hStmt) == _CONTINUE);
				DBFreeStatement(hStmt);
			}
			else
			{
				success = false;
			}
		}
   }

   // Save dashboard associations
   if (success)
   {
      TCHAR szQuery[512];
      _sntprintf(szQuery, 512, _T("DELETE FROM dashboard_associations WHERE object_id=%d"), m_id);
      success = DBQuery(hdb, szQuery);
      if (success && (m_dashboards->size() > 0))
      {
         hStmt = DBPrepare(hdb, _T("INSERT INTO dashboard_associations (object_id,dashboard_id) VALUES (?,?)"));
         if (hStmt != NULL)
         {
            DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_id);
            for(int i = 0; (i < m_dashboards->size()) && success; i++)
            {
               DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, m_dashboards->get(i));
               success = DBExecute(hStmt);
            }
            DBFreeStatement(hStmt);
         }
         else
         {
            success = false;
         }
      }
   }

   // Save module data
   if (success && (m_moduleData != NULL))
   {
      ModuleDataDatabaseCallbackParams data;
      data.id = m_id;
      data.hdb = hdb;
      success = (m_moduleData->forEach(SaveModuleDataCallback, &data) == _CONTINUE);
   }

	if (success)
		success = saveTrustedNodes(hdb);

   return success;
}

/**
 * Add reference to the new child object
 */
void NetObj::AddChild(NetObj *pObject)
{
   UINT32 i;

   LockChildList(TRUE);
   for(i = 0; i < m_dwChildCount; i++)
      if (m_pChildList[i] == pObject)
      {
         UnlockChildList();
         return;     // Already in the child list
      }
   m_pChildList = (NetObj **)realloc(m_pChildList, sizeof(NetObj *) * (m_dwChildCount + 1));
   m_pChildList[m_dwChildCount++] = pObject;
   UnlockChildList();
	incRefCount();
   setModified();
}

/**
 * Add reference to parent object
 */
void NetObj::AddParent(NetObj *pObject)
{
   UINT32 i;

   LockParentList(TRUE);
   for(i = 0; i < m_dwParentCount; i++)
      if (m_pParentList[i] == pObject)
      {
         UnlockParentList();
         return;     // Already in the parents list
      }
   m_pParentList = (NetObj **)realloc(m_pParentList, sizeof(NetObj *) * (m_dwParentCount + 1));
   m_pParentList[m_dwParentCount++] = pObject;
   UnlockParentList();
	incRefCount();
   setModified();
}

/**
 * Delete reference to child object
 */
void NetObj::DeleteChild(NetObj *pObject)
{
   UINT32 i;

   LockChildList(TRUE);
   for(i = 0; i < m_dwChildCount; i++)
      if (m_pChildList[i] == pObject)
         break;

   if (i == m_dwChildCount)   // No such object
   {
      UnlockChildList();
      return;
   }
   m_dwChildCount--;
   if (m_dwChildCount > 0)
   {
      memmove(&m_pChildList[i], &m_pChildList[i + 1], sizeof(NetObj *) * (m_dwChildCount - i));
      m_pChildList = (NetObj **)realloc(m_pChildList, sizeof(NetObj *) * m_dwChildCount);
   }
   else
   {
      free(m_pChildList);
      m_pChildList = NULL;
   }
   UnlockChildList();
	decRefCount();
   setModified();
}

/**
 * Delete reference to parent object
 */
void NetObj::DeleteParent(NetObj *pObject)
{
   UINT32 i;

   LockParentList(TRUE);
   for(i = 0; i < m_dwParentCount; i++)
      if (m_pParentList[i] == pObject)
         break;
   if (i == m_dwParentCount)   // No such object
   {
      UnlockParentList();
      return;
   }
   m_dwParentCount--;
   if (m_dwParentCount > 0)
   {
      memmove(&m_pParentList[i], &m_pParentList[i + 1], sizeof(NetObj *) * (m_dwParentCount - i));
      m_pParentList = (NetObj **)realloc(m_pParentList, sizeof(NetObj *) * m_dwParentCount);
   }
   else
   {
      free(m_pParentList);
      m_pParentList = NULL;
   }
   UnlockParentList();
	decRefCount();
   setModified();
}

/**
 * Walker callback to call OnObjectDelete for each active object
 */
void NetObj::onObjectDeleteCallback(NetObj *object, void *data)
{
	UINT32 currId = ((NetObj *)data)->getId();
	if ((object->getId() != currId) && !object->isDeleted())
		object->onObjectDelete(currId);
}

/**
 * Prepare object for deletion - remove all references, etc.
 *
 * @param initiator pointer to parent object which causes recursive deletion or NULL
 */
void NetObj::deleteObject(NetObj *initiator)
{
   DbgPrintf(4, _T("Deleting object %d [%s]"), m_id, m_name);

	// Prevent object change propagation until it's marked as deleted
	// (to prevent the object's incorrect appearance in GUI
	lockProperties();
   m_isHidden = true;
	unlockProperties();

	// Notify modules about object deletion
   CALL_ALL_MODULES(pfPreObjectDelete, (this));

   prepareForDeletion();

   DbgPrintf(5, _T("NetObj::deleteObject(): deleting object %d from indexes"), m_id);
   NetObjDeleteFromIndexes(this);

   // Delete references to this object from child objects
   DbgPrintf(5, _T("NetObj::deleteObject(): clearing child list for object %d"), m_id);
   ObjectArray<NetObj> *deleteList = NULL;
   LockChildList(TRUE);
   for(UINT32 i = 0; i < m_dwChildCount; i++)
   {
      if (m_pChildList[i]->getParentCount() == 1)
      {
         // last parent, delete object
         if (deleteList == NULL)
            deleteList = new ObjectArray<NetObj>(16, 16, false);
			deleteList->add(m_pChildList[i]);
      }
      else
      {
         m_pChildList[i]->DeleteParent(this);
      }
		decRefCount();
   }
   free(m_pChildList);
   m_pChildList = NULL;
   m_dwChildCount = 0;
   UnlockChildList();

   // Delete orphaned child objects
   if (deleteList != NULL)
   {
      for(int i = 0; i < deleteList->size(); i++)
      {
         NetObj *o = deleteList->get(i);
         DbgPrintf(5, _T("NetObj::deleteObject(): calling deleteObject() on %s [%d]"), o->getName(), o->getId());
         o->deleteObject(this);
      }
      delete deleteList;
   }

   // Remove references to this object from parent objects
   DbgPrintf(5, _T("NetObj::Delete(): clearing parent list for object %d"), m_id);
   LockParentList(TRUE);
   for(UINT32 i = 0; i < m_dwParentCount; i++)
   {
      // If parent is deletion initiator then this object already
      // removed from parent's list
      if (m_pParentList[i] != initiator)
      {
         m_pParentList[i]->DeleteChild(this);
         m_pParentList[i]->calculateCompoundStatus();
      }
		decRefCount();
   }
   free(m_pParentList);
   m_pParentList = NULL;
   m_dwParentCount = 0;
   UnlockParentList();

   lockProperties();
   m_isHidden = false;
   m_isDeleted = true;
   setModified();
   unlockProperties();

   // Notify all other objects about object deletion
   DbgPrintf(5, _T("NetObj::deleteObject(): calling onObjectDelete(%d)"), m_id);
	g_idxObjectById.forEach(onObjectDeleteCallback, this);

   DbgPrintf(4, _T("Object %d successfully deleted"), m_id);
}

/**
 * Default handler for object deletion notification
 */
void NetObj::onObjectDelete(UINT32 dwObjectId)
{
}

/**
 * Get childs IDs in printable form
 */
const TCHAR *NetObj::dbgGetChildList(TCHAR *szBuffer)
{
   UINT32 i;
   TCHAR *pBuf = szBuffer;

   *pBuf = 0;
   LockChildList(FALSE);
   for(i = 0, pBuf = szBuffer; i < m_dwChildCount; i++)
   {
      _sntprintf(pBuf, 10, _T("%d "), m_pChildList[i]->getId());
      while(*pBuf)
         pBuf++;
   }
   UnlockChildList();
   if (pBuf != szBuffer)
      *(pBuf - 1) = 0;
   return szBuffer;
}

/**
 * Get parents IDs in printable form
 */
const TCHAR *NetObj::dbgGetParentList(TCHAR *szBuffer)
{
   UINT32 i;
   TCHAR *pBuf = szBuffer;

   *pBuf = 0;
   LockParentList(FALSE);
   for(i = 0; i < m_dwParentCount; i++)
   {
      _sntprintf(pBuf, 10, _T("%d "), m_pParentList[i]->getId());
      while(*pBuf)
         pBuf++;
   }
   UnlockParentList();
   if (pBuf != szBuffer)
      *(pBuf - 1) = 0;
   return szBuffer;
}

/**
 * Calculate status for compound object based on childs' status
 */
void NetObj::calculateCompoundStatus(BOOL bForcedRecalc)
{
   if (m_iStatus == STATUS_UNMANAGED)
      return;

   int mostCriticalAlarm = GetMostCriticalStatusForObject(m_id);
   int mostCriticalDCI =
      (getObjectClass() == OBJECT_NODE || getObjectClass() == OBJECT_MOBILEDEVICE || getObjectClass() == OBJECT_CLUSTER || getObjectClass() == OBJECT_ACCESSPOINT) ?
         ((DataCollectionTarget *)this)->getMostCriticalDCIStatus() : STATUS_UNKNOWN;

   UINT32 i;
   int oldStatus = m_iStatus;
   int mostCriticalStatus, count, iStatusAlg;
   int nSingleThreshold, *pnThresholds;
   int nRating[5], iChildStatus, nThresholds[4];

   lockProperties();
   if (m_iStatusCalcAlg == SA_CALCULATE_DEFAULT)
   {
      iStatusAlg = GetDefaultStatusCalculation(&nSingleThreshold, &pnThresholds);
   }
   else
   {
      iStatusAlg = m_iStatusCalcAlg;
      nSingleThreshold = m_iStatusSingleThreshold;
      pnThresholds = m_iStatusThresholds;
   }
   if (iStatusAlg == SA_CALCULATE_SINGLE_THRESHOLD)
   {
      for(i = 0; i < 4; i++)
         nThresholds[i] = nSingleThreshold;
      pnThresholds = nThresholds;
   }

   switch(iStatusAlg)
   {
      case SA_CALCULATE_MOST_CRITICAL:
         LockChildList(FALSE);
         for(i = 0, count = 0, mostCriticalStatus = -1; i < m_dwChildCount; i++)
         {
            iChildStatus = m_pChildList[i]->getPropagatedStatus();
            if ((iChildStatus < STATUS_UNKNOWN) &&
                (iChildStatus > mostCriticalStatus))
            {
               mostCriticalStatus = iChildStatus;
               count++;
            }
         }
         m_iStatus = (count > 0) ? mostCriticalStatus : STATUS_UNKNOWN;
         UnlockChildList();
         break;
      case SA_CALCULATE_SINGLE_THRESHOLD:
      case SA_CALCULATE_MULTIPLE_THRESHOLDS:
         // Step 1: calculate severity raitings
         memset(nRating, 0, sizeof(int) * 5);
         LockChildList(FALSE);
         for(i = 0, count = 0; i < m_dwChildCount; i++)
         {
            iChildStatus = m_pChildList[i]->getPropagatedStatus();
            if (iChildStatus < STATUS_UNKNOWN)
            {
               while(iChildStatus >= 0)
                  nRating[iChildStatus--]++;
               count++;
            }
         }
         UnlockChildList();

         // Step 2: check what severity rating is above threshold
         if (count > 0)
         {
            for(i = 4; i > 0; i--)
               if (nRating[i] * 100 / count >= pnThresholds[i - 1])
                  break;
            m_iStatus = i;
         }
         else
         {
            m_iStatus = STATUS_UNKNOWN;
         }
         break;
      default:
         m_iStatus = STATUS_UNKNOWN;
         break;
   }

   // If alarms exist for object, apply alarm severity to object's status
   if (mostCriticalAlarm != STATUS_UNKNOWN)
   {
      if (m_iStatus == STATUS_UNKNOWN)
      {
         m_iStatus = mostCriticalAlarm;
      }
      else
      {
         m_iStatus = max(m_iStatus, mostCriticalAlarm);
      }
   }

   // If DCI status is calculated for object apply DCI object's statud
   if (mostCriticalDCI != STATUS_UNKNOWN)
   {
      if (m_iStatus == STATUS_UNKNOWN)
      {
         m_iStatus = mostCriticalDCI;
      }
      else
      {
         m_iStatus = max(m_iStatus, mostCriticalDCI);
      }
   }

   // Query loaded modules for object status
   ENUMERATE_MODULES(pfCalculateObjectStatus)
   {
      int moduleStatus = g_pModuleList[__i].pfCalculateObjectStatus(this);
      if (moduleStatus != STATUS_UNKNOWN)
      {
         if (m_iStatus == STATUS_UNKNOWN)
         {
            m_iStatus = moduleStatus;
         }
         else
         {
            m_iStatus = max(m_iStatus, moduleStatus);
         }
      }
   }

   unlockProperties();

   // Cause parent object(s) to recalculate it's status
   if ((oldStatus != m_iStatus) || bForcedRecalc)
   {
      LockParentList(FALSE);
      for(i = 0; i < m_dwParentCount; i++)
         m_pParentList[i]->calculateCompoundStatus();
      UnlockParentList();
      lockProperties();
      setModified();
      unlockProperties();
   }
}

/**
 * Load ACL from database
 */
bool NetObj::loadACLFromDB(DB_HANDLE hdb)
{
   bool success = false;

	DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT user_id,access_rights FROM acl WHERE object_id=?"));
	if (hStmt != NULL)
	{
		DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_id);
		DB_RESULT hResult = DBSelectPrepared(hStmt);
		if (hResult != NULL)
		{
			int i, iNumRows;

			iNumRows = DBGetNumRows(hResult);
			for(i = 0; i < iNumRows; i++)
				m_pAccessList->addElement(DBGetFieldULong(hResult, i, 0),
												  DBGetFieldULong(hResult, i, 1));
			DBFreeResult(hResult);
			success = true;
		}
		DBFreeStatement(hStmt);
	}
   return success;
}

/**
 * ACL enumeration parameters structure
 */
struct SAVE_PARAM
{
   DB_HANDLE hdb;
   UINT32 dwObjectId;
};

/**
 * Handler for ACL elements enumeration
 */
static void EnumerationHandler(UINT32 dwUserId, UINT32 dwAccessRights, void *pArg)
{
   TCHAR szQuery[256];

   _sntprintf(szQuery, sizeof(szQuery) / sizeof(TCHAR), _T("INSERT INTO acl (object_id,user_id,access_rights) VALUES (%d,%d,%d)"),
           ((SAVE_PARAM *)pArg)->dwObjectId, dwUserId, dwAccessRights);
   DBQuery(((SAVE_PARAM *)pArg)->hdb, szQuery);
}

/**
 * Save ACL to database
 */
bool NetObj::saveACLToDB(DB_HANDLE hdb)
{
   TCHAR szQuery[256];
   bool success = false;
   SAVE_PARAM sp;

   // Save access list
   lockACL();
   _sntprintf(szQuery, sizeof(szQuery) / sizeof(TCHAR), _T("DELETE FROM acl WHERE object_id=%d"), m_id);
   if (DBQuery(hdb, szQuery))
   {
      sp.dwObjectId = m_id;
      sp.hdb = hdb;
      m_pAccessList->enumerateElements(EnumerationHandler, &sp);
      success = true;
   }
   unlockACL();
   return success;
}

/**
 * Data for SendModuleDataCallback
 */
struct SendModuleDataCallbackData
{
   NXCPMessage *msg;
   UINT32 id;
};

/**
 * Callback for sending module data in NXCP message
 */
static EnumerationCallbackResult SendModuleDataCallback(const TCHAR *key, const void *value, void *data)
{
   ((SendModuleDataCallbackData *)data)->msg->setField(((SendModuleDataCallbackData *)data)->id, key);
   ((ModuleData *)value)->fillMessage(((SendModuleDataCallbackData *)data)->msg, ((SendModuleDataCallbackData *)data)->id + 1);
   ((SendModuleDataCallbackData *)data)->id += 0x100000;
   return _CONTINUE;
}

/**
 * Fill NXCP message with object's data
 * Object's properties are locked when this method is called. Method should not do any other locks.
 * Data required other locks should be filled in fillMessageInternalStage2().
 */
void NetObj::fillMessageInternal(NXCPMessage *pMsg)
{
   pMsg->setField(VID_OBJECT_CLASS, (WORD)getObjectClass());
   pMsg->setField(VID_OBJECT_ID, m_id);
	pMsg->setField(VID_GUID, m_guid);
   pMsg->setField(VID_OBJECT_NAME, m_name);
   pMsg->setField(VID_OBJECT_STATUS, (WORD)m_iStatus);
   pMsg->setField(VID_IS_DELETED, (WORD)(m_isDeleted ? 1 : 0));
   pMsg->setField(VID_IS_SYSTEM, (INT16)(m_isSystem ? 1 : 0));
   pMsg->setField(VID_MAINTENANCE_MODE, (INT16)(m_maintenanceEventId ? 1 : 0));

   pMsg->setField(VID_INHERIT_RIGHTS, (WORD)m_bInheritAccessRights);
   pMsg->setField(VID_STATUS_CALCULATION_ALG, (WORD)m_iStatusCalcAlg);
   pMsg->setField(VID_STATUS_PROPAGATION_ALG, (WORD)m_iStatusPropAlg);
   pMsg->setField(VID_FIXED_STATUS, (WORD)m_iFixedStatus);
   pMsg->setField(VID_STATUS_SHIFT, (WORD)m_iStatusShift);
   pMsg->setField(VID_STATUS_TRANSLATION_1, (WORD)m_iStatusTranslation[0]);
   pMsg->setField(VID_STATUS_TRANSLATION_2, (WORD)m_iStatusTranslation[1]);
   pMsg->setField(VID_STATUS_TRANSLATION_3, (WORD)m_iStatusTranslation[2]);
   pMsg->setField(VID_STATUS_TRANSLATION_4, (WORD)m_iStatusTranslation[3]);
   pMsg->setField(VID_STATUS_SINGLE_THRESHOLD, (WORD)m_iStatusSingleThreshold);
   pMsg->setField(VID_STATUS_THRESHOLD_1, (WORD)m_iStatusThresholds[0]);
   pMsg->setField(VID_STATUS_THRESHOLD_2, (WORD)m_iStatusThresholds[1]);
   pMsg->setField(VID_STATUS_THRESHOLD_3, (WORD)m_iStatusThresholds[2]);
   pMsg->setField(VID_STATUS_THRESHOLD_4, (WORD)m_iStatusThresholds[3]);
   pMsg->setField(VID_COMMENTS, CHECK_NULL_EX(m_pszComments));
	pMsg->setField(VID_IMAGE, m_image);
	pMsg->setField(VID_SUBMAP_ID, m_submapId);
	pMsg->setField(VID_NUM_TRUSTED_NODES, m_dwNumTrustedNodes);
	if (m_dwNumTrustedNodes > 0)
		pMsg->setFieldFromInt32Array(VID_TRUSTED_NODES, m_dwNumTrustedNodes, m_pdwTrustedNodes);
   pMsg->setFieldFromInt32Array(VID_DASHBOARDS, m_dashboards);

   m_customAttributes.fillMessage(pMsg, VID_NUM_CUSTOM_ATTRIBUTES, VID_CUSTOM_ATTRIBUTES_BASE);

   m_pAccessList->fillMessage(pMsg);
	m_geoLocation.fillMessage(*pMsg);

   pMsg->setField(VID_COUNTRY, m_postalAddress->getCountry());
   pMsg->setField(VID_CITY, m_postalAddress->getCity());
   pMsg->setField(VID_STREET_ADDRESS, m_postalAddress->getStreetAddress());
   pMsg->setField(VID_POSTCODE, m_postalAddress->getPostCode());

   if (m_moduleData != NULL)
   {
      pMsg->setField(VID_MODULE_DATA_COUNT, (UINT16)m_moduleData->size());
      SendModuleDataCallbackData data;
      data.msg = pMsg;
      data.id = VID_MODULE_DATA_BASE;
      m_moduleData->forEach(SendModuleDataCallback, &data);
   }
   else
   {
      pMsg->setField(VID_MODULE_DATA_COUNT, (UINT16)0);
   }
}

/**
 * Fill NXCP message with object's data - stage 2
 * Object's properties are not locked when this method is called. Should be
 * used only to fill data where properties lock is not enough (like data 
 * collection configuration).
 */
void NetObj::fillMessageInternalStage2(NXCPMessage *pMsg)
{
}

/**
 * Fill NXCP message with object's data
 */
void NetObj::fillMessage(NXCPMessage *msg)
{ 
   lockProperties(); 
   fillMessageInternal(msg);
   unlockProperties(); 
   fillMessageInternalStage2(msg);

   UINT32 i, dwId;

   LockParentList(FALSE);
   msg->setField(VID_PARENT_CNT, m_dwParentCount);
   for(i = 0, dwId = VID_PARENT_ID_BASE; i < m_dwParentCount; i++, dwId++)
      msg->setField(dwId, m_pParentList[i]->getId());
   UnlockParentList();

   LockChildList(FALSE);
   msg->setField(VID_CHILD_CNT, m_dwChildCount);
   for(i = 0, dwId = VID_CHILD_ID_BASE; i < m_dwChildCount; i++, dwId++)
      msg->setField(dwId, m_pChildList[i]->getId());
   UnlockChildList();
}

/**
 * Handler for EnumerateSessions()
 */
static void BroadcastObjectChange(ClientSession *pSession, void *pArg)
{
   if (pSession->isAuthenticated())
      pSession->onObjectChange((NetObj *)pArg);
}

/**
 * Mark object as modified and put on client's notification queue
 * We assume that object is locked at the time of function call
 */
void NetObj::setModified()
{
   if (g_bModificationsLocked)
      return;

   m_isModified = true;
   m_dwTimeStamp = (UINT32)time(NULL);

   // Send event to all connected clients
   if (!m_isHidden && !m_isSystem)
      EnumerateClientSessions(BroadcastObjectChange, this);
}

/**
 * Modify object from NXCP message - common wrapper
 */
UINT32 NetObj::modifyFromMessage(NXCPMessage *msg)
{ 
   lockProperties(); 
   UINT32 rcc = modifyFromMessageInternal(msg);
   setModified();
   unlockProperties();
   return rcc; 
}

/**
 * Modify object from NXCP message
 */
UINT32 NetObj::modifyFromMessageInternal(NXCPMessage *pRequest)
{
   // Change object's name
   if (pRequest->isFieldExist(VID_OBJECT_NAME))
      pRequest->getFieldAsString(VID_OBJECT_NAME, m_name, MAX_OBJECT_NAME);

   // Change object's status calculation/propagation algorithms
   if (pRequest->isFieldExist(VID_STATUS_CALCULATION_ALG))
   {
      m_iStatusCalcAlg = pRequest->getFieldAsInt16(VID_STATUS_CALCULATION_ALG);
      m_iStatusPropAlg = pRequest->getFieldAsInt16(VID_STATUS_PROPAGATION_ALG);
      m_iFixedStatus = pRequest->getFieldAsInt16(VID_FIXED_STATUS);
      m_iStatusShift = pRequest->getFieldAsInt16(VID_STATUS_SHIFT);
      m_iStatusTranslation[0] = pRequest->getFieldAsInt16(VID_STATUS_TRANSLATION_1);
      m_iStatusTranslation[1] = pRequest->getFieldAsInt16(VID_STATUS_TRANSLATION_2);
      m_iStatusTranslation[2] = pRequest->getFieldAsInt16(VID_STATUS_TRANSLATION_3);
      m_iStatusTranslation[3] = pRequest->getFieldAsInt16(VID_STATUS_TRANSLATION_4);
      m_iStatusSingleThreshold = pRequest->getFieldAsInt16(VID_STATUS_SINGLE_THRESHOLD);
      m_iStatusThresholds[0] = pRequest->getFieldAsInt16(VID_STATUS_THRESHOLD_1);
      m_iStatusThresholds[1] = pRequest->getFieldAsInt16(VID_STATUS_THRESHOLD_2);
      m_iStatusThresholds[2] = pRequest->getFieldAsInt16(VID_STATUS_THRESHOLD_3);
      m_iStatusThresholds[3] = pRequest->getFieldAsInt16(VID_STATUS_THRESHOLD_4);
   }

	// Change image
	if (pRequest->isFieldExist(VID_IMAGE))
		m_image = pRequest->getFieldAsGUID(VID_IMAGE);

   // Change object's ACL
   if (pRequest->isFieldExist(VID_ACL_SIZE))
   {
      UINT32 i, dwNumElements;

      lockACL();
      dwNumElements = pRequest->getFieldAsUInt32(VID_ACL_SIZE);
      m_bInheritAccessRights = pRequest->getFieldAsUInt16(VID_INHERIT_RIGHTS);
      m_pAccessList->deleteAll();
      for(i = 0; i < dwNumElements; i++)
         m_pAccessList->addElement(pRequest->getFieldAsUInt32(VID_ACL_USER_BASE + i),
                                   pRequest->getFieldAsUInt32(VID_ACL_RIGHTS_BASE +i));
      unlockACL();
   }

	// Change trusted nodes list
   if (pRequest->isFieldExist(VID_NUM_TRUSTED_NODES))
   {
      m_dwNumTrustedNodes = pRequest->getFieldAsUInt32(VID_NUM_TRUSTED_NODES);
		m_pdwTrustedNodes = (UINT32 *)realloc(m_pdwTrustedNodes, sizeof(UINT32) * m_dwNumTrustedNodes);
		pRequest->getFieldAsInt32Array(VID_TRUSTED_NODES, m_dwNumTrustedNodes, m_pdwTrustedNodes);
   }

   // Change custom attributes
   if (pRequest->isFieldExist(VID_NUM_CUSTOM_ATTRIBUTES))
   {
      UINT32 i, dwId, dwNumElements;
      TCHAR *name, *value;

      dwNumElements = pRequest->getFieldAsUInt32(VID_NUM_CUSTOM_ATTRIBUTES);
      m_customAttributes.clear();
      for(i = 0, dwId = VID_CUSTOM_ATTRIBUTES_BASE; i < dwNumElements; i++)
      {
      	name = pRequest->getFieldAsString(dwId++);
      	value = pRequest->getFieldAsString(dwId++);
      	if ((name != NULL) && (value != NULL))
	      	m_customAttributes.setPreallocated(name, value);
      }
   }

	// Change geolocation
	if (pRequest->isFieldExist(VID_GEOLOCATION_TYPE))
	{
		m_geoLocation = GeoLocation(*pRequest);
		addLocationToHistory();
	}

	if (pRequest->isFieldExist(VID_SUBMAP_ID))
	{
		m_submapId = pRequest->getFieldAsUInt32(VID_SUBMAP_ID);
	}

   if (pRequest->isFieldExist(VID_COUNTRY))
   {
      TCHAR buffer[64];
      pRequest->getFieldAsString(VID_COUNTRY, buffer, 64);
      m_postalAddress->setCountry(buffer);
   }

   if (pRequest->isFieldExist(VID_CITY))
   {
      TCHAR buffer[64];
      pRequest->getFieldAsString(VID_CITY, buffer, 64);
      m_postalAddress->setCity(buffer);
   }

   if (pRequest->isFieldExist(VID_STREET_ADDRESS))
   {
      TCHAR buffer[256];
      pRequest->getFieldAsString(VID_STREET_ADDRESS, buffer, 256);
      m_postalAddress->setStreetAddress(buffer);
   }

   if (pRequest->isFieldExist(VID_POSTCODE))
   {
      TCHAR buffer[32];
      pRequest->getFieldAsString(VID_POSTCODE, buffer, 32);
      m_postalAddress->setPostCode(buffer);
   }

   // Change dashboard list
   if (pRequest->isFieldExist(VID_DASHBOARDS))
   {
      pRequest->getFieldAsInt32Array(VID_DASHBOARDS, m_dashboards);
   }

   return RCC_SUCCESS;
}

/**
 * Post-modify hook
 */
void NetObj::postModify()
{
   calculateCompoundStatus(TRUE);
}

/**
 * Get rights to object for specific user
 *
 * @param userId user object ID
 */
UINT32 NetObj::getUserRights(UINT32 userId)
{
   UINT32 dwRights;

   // Admin always has all rights to any object
   if (userId == 0)
      return 0xFFFFFFFF;

	// Non-admin users have no rights to system objects
	if (m_isSystem)
		return 0;

   // Check if have direct right assignment
   lockACL();
   bool hasDirectRights = m_pAccessList->getUserRights(userId, &dwRights);
   unlockACL();

   if (!hasDirectRights)
   {
      // We don't. If this object inherit rights from parents, get them
      if (m_bInheritAccessRights)
      {
         UINT32 i;

         LockParentList(FALSE);
         for(i = 0, dwRights = 0; i < m_dwParentCount; i++)
            dwRights |= m_pParentList[i]->getUserRights(userId);
         UnlockParentList();
      }
   }

   return dwRights;
}

/**
 * Check if given user has specific rights on this object
 *
 * @param userId user object ID
 * @param requiredRights bit mask of requested right
 * @return true if user has all rights specified in requested rights bit mask
 */
BOOL NetObj::checkAccessRights(UINT32 userId, UINT32 requiredRights)
{
   UINT32 effectiveRights = getUserRights(userId);
   return (effectiveRights & requiredRights) == requiredRights;
}

/**
 * Drop all user privileges on current object
 */
void NetObj::dropUserAccess(UINT32 dwUserId)
{
   lockACL();
   bool modified = m_pAccessList->deleteElement(dwUserId);
   unlockACL();
   if (modified)
   {
      lockProperties();
      setModified();
      unlockProperties();
   }
}

/**
 * Set object's management status
 */
void NetObj::setMgmtStatus(BOOL bIsManaged)
{
   UINT32 i;
   int oldStatus;

   lockProperties();

   if ((bIsManaged && (m_iStatus != STATUS_UNMANAGED)) ||
       ((!bIsManaged) && (m_iStatus == STATUS_UNMANAGED)))
   {
      unlockProperties();
      return;  // Status is already correct
   }

   oldStatus = m_iStatus;
   m_iStatus = (bIsManaged ? STATUS_UNKNOWN : STATUS_UNMANAGED);

   // Generate event if current object is a node
   if (getObjectClass() == OBJECT_NODE)
      PostEvent(bIsManaged ? EVENT_NODE_UNKNOWN : EVENT_NODE_UNMANAGED, m_id, "d", oldStatus);

   setModified();
   unlockProperties();

   // Change status for child objects also
   LockChildList(FALSE);
   for(i = 0; i < m_dwChildCount; i++)
      m_pChildList[i]->setMgmtStatus(bIsManaged);
   UnlockChildList();

   // Cause parent object(s) to recalculate it's status
   LockParentList(FALSE);
   for(i = 0; i < m_dwParentCount; i++)
      m_pParentList[i]->calculateCompoundStatus();
   UnlockParentList();
}

/**
 * Check if given object is an our child (possibly indirect, i.e child of child)
 *
 * @param id object ID to test
 */
bool NetObj::isChild(UINT32 id)
{
   UINT32 i;
   bool bResult = false;

   // Check for our own ID (object ID should never change, so we may not lock object's data)
   if (m_id == id)
      bResult = true;

   // First, walk through our own child list
   if (!bResult)
   {
      LockChildList(FALSE);
      for(i = 0; i < m_dwChildCount; i++)
         if (m_pChildList[i]->getId() == id)
         {
            bResult = true;
            break;
         }
      UnlockChildList();
   }

   // If given object is not in child list, check if it is indirect child
   if (!bResult)
   {
      LockChildList(FALSE);
      for(i = 0; i < m_dwChildCount; i++)
         if (m_pChildList[i]->isChild(id))
         {
            bResult = true;
            break;
         }
      UnlockChildList();
   }

   return bResult;
}

/**
 * Send message to client, who requests poll, if any
 * This method is used by Node and Interface class objects
 */
void NetObj::sendPollerMsg(UINT32 dwRqId, const TCHAR *pszFormat, ...)
{
   if (m_pollRequestor != NULL)
   {
      va_list args;
      TCHAR szBuffer[1024];

      va_start(args, pszFormat);
      _vsntprintf(szBuffer, 1024, pszFormat, args);
      va_end(args);
      m_pollRequestor->sendPollerMsg(dwRqId, szBuffer);
   }
}

/**
 * Add child node objects (direct and indirect childs) to list
 */
void NetObj::addChildNodesToList(ObjectArray<Node> *nodeList, UINT32 dwUserId)
{
   UINT32 i;

   LockChildList(FALSE);

   // Walk through our own child list
   for(i = 0; i < m_dwChildCount; i++)
   {
      if (m_pChildList[i]->getObjectClass() == OBJECT_NODE)
      {
         // Check if this node already in the list
			int j;
			for(j = 0; j < nodeList->size(); j++)
				if (nodeList->get(j)->getId() == m_pChildList[i]->getId())
               break;
         if (j == nodeList->size())
         {
            m_pChildList[i]->incRefCount();
				nodeList->add((Node *)m_pChildList[i]);
         }
      }
      else
      {
         if (m_pChildList[i]->checkAccessRights(dwUserId, OBJECT_ACCESS_READ))
            m_pChildList[i]->addChildNodesToList(nodeList, dwUserId);
      }
   }

   UnlockChildList();
}

/**
 * Add child data collection targets (direct and indirect childs) to list
 */
void NetObj::addChildDCTargetsToList(ObjectArray<DataCollectionTarget> *dctList, UINT32 dwUserId)
{
   UINT32 i;

   LockChildList(FALSE);

   // Walk through our own child list
   for(i = 0; i < m_dwChildCount; i++)
   {
      if ((m_pChildList[i]->getObjectClass() == OBJECT_NODE) || (m_pChildList[i]->getObjectClass() == OBJECT_MOBILEDEVICE))
      {
         // Check if this objects already in the list
			int j;
			for(j = 0; j < dctList->size(); j++)
				if (dctList->get(j)->getId() == m_pChildList[i]->getId())
               break;
         if (j == dctList->size())
         {
            m_pChildList[i]->incRefCount();
				dctList->add((DataCollectionTarget *)m_pChildList[i]);
         }
      }
      else
      {
         if (m_pChildList[i]->checkAccessRights(dwUserId, OBJECT_ACCESS_READ))
            m_pChildList[i]->addChildDCTargetsToList(dctList, dwUserId);
      }
   }

   UnlockChildList();
}

/**
 * Hide object and all its childs
 */
void NetObj::hide()
{
   UINT32 i;

   LockChildList(FALSE);
   for(i = 0; i < m_dwChildCount; i++)
      m_pChildList[i]->hide();
   UnlockChildList();

	lockProperties();
   m_isHidden = true;
   unlockProperties();
}

/**
 * Unhide object and all its childs
 */
void NetObj::unhide()
{
   UINT32 i;

   lockProperties();
   m_isHidden = false;
   if (!m_isSystem)
      EnumerateClientSessions(BroadcastObjectChange, this);
   unlockProperties();

   LockChildList(FALSE);
   for(i = 0; i < m_dwChildCount; i++)
      m_pChildList[i]->unhide();
   UnlockChildList();
}

/**
 * Return status propagated to parent
 */
int NetObj::getPropagatedStatus()
{
   int iStatus;

   if (m_iStatusPropAlg == SA_PROPAGATE_DEFAULT)
   {
      iStatus = DefaultPropagatedStatus(m_iStatus);
   }
   else
   {
      switch(m_iStatusPropAlg)
      {
         case SA_PROPAGATE_UNCHANGED:
            iStatus = m_iStatus;
            break;
         case SA_PROPAGATE_FIXED:
            iStatus = ((m_iStatus > STATUS_NORMAL) && (m_iStatus < STATUS_UNKNOWN)) ? m_iFixedStatus : m_iStatus;
            break;
         case SA_PROPAGATE_RELATIVE:
            if ((m_iStatus > STATUS_NORMAL) && (m_iStatus < STATUS_UNKNOWN))
            {
               iStatus = m_iStatus + m_iStatusShift;
               if (iStatus < 0)
                  iStatus = 0;
               if (iStatus > STATUS_CRITICAL)
                  iStatus = STATUS_CRITICAL;
            }
            else
            {
               iStatus = m_iStatus;
            }
            break;
         case SA_PROPAGATE_TRANSLATED:
            if ((m_iStatus > STATUS_NORMAL) && (m_iStatus < STATUS_UNKNOWN))
            {
               iStatus = m_iStatusTranslation[m_iStatus - 1];
            }
            else
            {
               iStatus = m_iStatus;
            }
            break;
         default:
            iStatus = STATUS_UNKNOWN;
            break;
      }
   }
   return iStatus;
}

/**
 * Prepare object for deletion. Method should return only
 * when object deletion is safe
 */
void NetObj::prepareForDeletion()
{
}

/**
 * Set object's comments.
 * NOTE: pszText should be dynamically allocated or NULL
 */
void NetObj::setComments(TCHAR *text)
{
   lockProperties();
   safe_free(m_pszComments);
   m_pszComments = text;
   setModified();
   unlockProperties();
}

/**
 * Copy object's comments to NXCP message
 */
void NetObj::commentsToMessage(NXCPMessage *pMsg)
{
   lockProperties();
   pMsg->setField(VID_COMMENTS, CHECK_NULL_EX(m_pszComments));
   unlockProperties();
}

/**
 * Load trusted nodes list from database
 */
bool NetObj::loadTrustedNodes(DB_HANDLE hdb)
{
	DB_RESULT hResult;
	TCHAR query[256];
	int i, count;

	_sntprintf(query, 256, _T("SELECT target_node_id FROM trusted_nodes WHERE source_object_id=%d"), m_id);
	hResult = DBSelect(hdb, query);
	if (hResult != NULL)
	{
		count = DBGetNumRows(hResult);
		if (count > 0)
		{
			m_dwNumTrustedNodes = count;
			m_pdwTrustedNodes = (UINT32 *)malloc(sizeof(UINT32) * count);
			for(i = 0; i < count; i++)
			{
				m_pdwTrustedNodes[i] = DBGetFieldULong(hResult, i, 0);
			}
		}
		DBFreeResult(hResult);
	}
	return (hResult != NULL);
}

/**
 * Save list of trusted nodes to database
 */
bool NetObj::saveTrustedNodes(DB_HANDLE hdb)
{
	TCHAR query[256];
	UINT32 i;
	bool rc = false;

	_sntprintf(query, 256, _T("DELETE FROM trusted_nodes WHERE source_object_id=%d"), m_id);
	if (DBQuery(hdb, query))
	{
		for(i = 0; i < m_dwNumTrustedNodes; i++)
		{
			_sntprintf(query, 256, _T("INSERT INTO trusted_nodes (source_object_id,target_node_id) VALUES (%d,%d)"),
			           m_id, m_pdwTrustedNodes[i]);
			if (!DBQuery(hdb, query))
				break;
		}
		if (i == m_dwNumTrustedNodes)
			rc = true;
	}
	return rc;
}

/**
 * Check if given node is in trust list
 * Will always return TRUE if system parameter CheckTrustedNodes set to 0
 */
bool NetObj::isTrustedNode(UINT32 id)
{
	bool rc;

	if (g_flags & AF_CHECK_TRUSTED_NODES)
	{
		UINT32 i;

		lockProperties();
		for(i = 0, rc = false; i < m_dwNumTrustedNodes; i++)
		{
			if (m_pdwTrustedNodes[i] == id)
			{
				rc = true;
				break;
			}
		}
		unlockProperties();
	}
	else
	{
		rc = true;
	}
	return rc;
}

/**
 * Get list of parent objects for NXSL script
 */
NXSL_Array *NetObj::getParentsForNXSL()
{
	NXSL_Array *parents = new NXSL_Array;
	int index = 0;

	LockParentList(FALSE);
	for(UINT32 i = 0; i < m_dwParentCount; i++)
	{
		if ((m_pParentList[i]->getObjectClass() == OBJECT_CONTAINER) ||
			 (m_pParentList[i]->getObjectClass() == OBJECT_SERVICEROOT) ||
			 (m_pParentList[i]->getObjectClass() == OBJECT_NETWORK))
		{
			parents->set(index++, new NXSL_Value(new NXSL_Object(&g_nxslNetObjClass, m_pParentList[i])));
		}
	}
	UnlockParentList();

	return parents;
}

/**
 * Get list of child objects for NXSL script
 */
NXSL_Array *NetObj::getChildrenForNXSL()
{
	NXSL_Array *children = new NXSL_Array;
	int index = 0;

	LockChildList(FALSE);
	for(UINT32 i = 0; i < m_dwChildCount; i++)
	{
		if (m_pChildList[i]->getObjectClass() == OBJECT_NODE)
		{
			children->set(index++, new NXSL_Value(new NXSL_Object(&g_nxslNodeClass, m_pChildList[i])));
		}
		else if (m_pChildList[i]->getObjectClass() == OBJECT_INTERFACE)
		{
			children->set(index++, new NXSL_Value(new NXSL_Object(&g_nxslInterfaceClass, m_pChildList[i])));
		}
		else
		{
			children->set(index++, new NXSL_Value(new NXSL_Object(&g_nxslNetObjClass, m_pChildList[i])));
		}
	}
	UnlockChildList();

	return children;
}

/**
 * Get full list of child objects (including both direct and indirect childs)
 */
void NetObj::getFullChildListInternal(ObjectIndex *list, bool eventSourceOnly)
{
	LockChildList(FALSE);
	for(UINT32 i = 0; i < m_dwChildCount; i++)
	{
		if (!eventSourceOnly || IsEventSource(m_pChildList[i]->getObjectClass()))
			list->put(m_pChildList[i]->getId(), m_pChildList[i]);
		m_pChildList[i]->getFullChildListInternal(list, eventSourceOnly);
	}
	UnlockChildList();
}

/**
 * Get full list of child objects (including both direct and indirect childs).
 *  Returned array is dynamically allocated and must be deleted by the caller.
 *
 * @param eventSourceOnly if true, only objects that can be event source will be included
 */
ObjectArray<NetObj> *NetObj::getFullChildList(bool eventSourceOnly, bool updateRefCount)
{
	ObjectIndex list;
	getFullChildListInternal(&list, eventSourceOnly);
	return list.getObjects(updateRefCount);
}

/**
 * Get list of child objects (direct only). Returned array is
 * dynamically allocated and must be deleted by the caller.
 *
 * @param typeFilter Only return objects with class ID equals given value.
 *                   Set to -1 to disable filtering.
 */
ObjectArray<NetObj> *NetObj::getChildList(int typeFilter)
{
	LockChildList(FALSE);
	ObjectArray<NetObj> *list = new ObjectArray<NetObj>((int)m_dwChildCount, 16, false);
	for(UINT32 i = 0; i < m_dwChildCount; i++)
	{
		if ((typeFilter == -1) || (typeFilter == m_pChildList[i]->getObjectClass()))
			list->add(m_pChildList[i]);
	}
	UnlockChildList();
	return list;
}

/**
 * Get list of parent objects (direct only). Returned array is
 * dynamically allocated and must be deleted by the caller.
 *
 * @param typeFilter Only return objects with class ID equals given value.
 *                   Set to -1 to disable filtering.
 */
ObjectArray<NetObj> *NetObj::getParentList(int typeFilter)
{
    LockParentList(FALSE);
    ObjectArray<NetObj> *list = new ObjectArray<NetObj>((int)m_dwParentCount, 16, false);
    for(UINT32 i = 0; i < m_dwParentCount; i++)
    {
        if ((typeFilter == -1) || (typeFilter == m_pParentList[i]->getObjectClass()))
            list->add(m_pParentList[i]);
    }
    UnlockParentList();
    return list;
}

/**
 * FInd child object by name (with optional class filter)
 */
NetObj *NetObj::findChildObject(const TCHAR *name, int typeFilter)
{
   NetObj *object = NULL;
	LockChildList(FALSE);
	for(UINT32 i = 0; i < m_dwChildCount; i++)
	{
      if (((typeFilter == -1) || (typeFilter == m_pChildList[i]->getObjectClass())) && !_tcsicmp(name, m_pChildList[i]->getName()))
      {
         object = m_pChildList[i];
         break;
      }
	}
	UnlockChildList();
	return object;
}

/**
 * Called by client session handler to check if threshold summary should
 * be shown for this object. Default implementation always returns false.
 */
bool NetObj::showThresholdSummary()
{
	return false;
}

/**
 * Must return true if object is a possible event source
 */
bool NetObj::isEventSource()
{
   return false;
}

/**
 * Get module data
 */
ModuleData *NetObj::getModuleData(const TCHAR *module)
{
   lockProperties();
   ModuleData *data = (m_moduleData != NULL) ? m_moduleData->get(module) : NULL;
   unlockProperties();
   return data;
}

/**
 * Set module data
 */
void NetObj::setModuleData(const TCHAR *module, ModuleData *data)
{
   lockProperties();
   if (m_moduleData == NULL)
      m_moduleData = new StringObjectMap<ModuleData>(true);
   m_moduleData->set(module, data);
   unlockProperties();
}

/**
 * Add new location entry
 */
void NetObj::addLocationToHistory()
{
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   UINT32 startTimestamp;
   bool isSamePlace;
   DB_RESULT hResult;
   if (!isLocationTableExists())
   {
      DbgPrintf(4, _T("NetObj::addLocationToHistory: Geolocation history table will be created for object %s [%d]"), m_name, m_id);
      if (!createLocationHistoryTable(hdb))
      {
         DbgPrintf(4, _T("NetObj::addLocationToHistory: Error creating geolocation history table for object %s [%d]"), m_name, m_id);
         return;
      }
   }
	const TCHAR *query;
	switch(g_dbSyntax)
	{
		case DB_SYNTAX_ORACLE:
			query = _T("SELECT * FROM (latitude,longitude,accuracy,start_timestamp FROM gps_history_%d ORDER BY start_timestamp DESC) WHERE ROWNUM<=1");
			break;
		case DB_SYNTAX_MSSQL:
			query = _T("SELECT TOP 1 latitude,longitude,accuracy,start_timestamp FROM gps_history_%d ORDER BY start_timestamp DESC");
			break;
		case DB_SYNTAX_DB2:
			query = _T("SELECT latitude,longitude,accuracy,start_timestamp FROM gps_history_%d ORDER BY start_timestamp DESC FETCH FIRST 200 ROWS ONLY");
			break;
		default:
			query = _T("SELECT latitude,longitude,accuracy,start_timestamp FROM gps_history_%d ORDER BY start_timestamp DESC LIMIT 1");
			break;
	}
   TCHAR preparedQuery[256];
	_sntprintf(preparedQuery, 256, query, m_id);
	DB_STATEMENT hStmt = DBPrepare(hdb, preparedQuery);

   if (hStmt == NULL)
		goto onFail;

   hResult = DBSelectPrepared(hStmt);
   if (hResult == NULL)
		goto onFail;
   if (DBGetNumRows(hResult) > 0)
   {
      startTimestamp = DBGetFieldULong(hResult, 0, 3);
      isSamePlace = m_geoLocation.sameLocation(DBGetFieldDouble(hResult, 0, 0), DBGetFieldDouble(hResult, 0, 1), DBGetFieldLong(hResult, 0, 2));
      DBFreeStatement(hStmt);
      DBFreeResult(hResult);
   }
   else
   {
      isSamePlace = false;
   }

   if (isSamePlace)
   {
      TCHAR query[256];
      _sntprintf(query, 255, _T("UPDATE gps_history_%d SET end_timestamp = ? WHERE start_timestamp =? "), m_id);
      hStmt = DBPrepare(hdb, query);
      DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, (UINT32)m_geoLocation.getTimestamp());
      DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, startTimestamp);
   }
   else
   {
      TCHAR query[256];
      _sntprintf(query, 255, _T("INSERT INTO gps_history_%d (latitude,longitude,")
                       _T("accuracy,start_timestamp,end_timestamp) VALUES (?,?,?,?,?)"), m_id);
      hStmt = DBPrepare(hdb, query);

      TCHAR lat[32], lon[32];
      _sntprintf(lat, 32, _T("%f"), m_geoLocation.getLatitude());
      _sntprintf(lon, 32, _T("%f"), m_geoLocation.getLongitude());

      DBBind(hStmt, 1, DB_SQLTYPE_VARCHAR, lat, DB_BIND_STATIC);
      DBBind(hStmt, 2, DB_SQLTYPE_VARCHAR, lon, DB_BIND_STATIC);
      DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, (LONG)m_geoLocation.getAccuracy());
      DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, (UINT32)m_geoLocation.getTimestamp());
      DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, (UINT32)m_geoLocation.getTimestamp());
	}

	if (hStmt == NULL)
		goto onFail;

   DBExecute(hStmt);
   DBFreeStatement(hStmt);
   DBConnectionPoolReleaseConnection(hdb);
   return;

onFail:
   DBFreeStatement(hStmt);
   DbgPrintf(4, _T("NetObj::addLocationToHistory(%s [%d]): Failed to add location to history"), m_name, m_id);
   DBConnectionPoolReleaseConnection(hdb);
   return;
}

/**
 * Check if given data table exist
 */
bool NetObj::isLocationTableExists()
{
   TCHAR table[256];
   _sntprintf(table, 256, _T("gps_history_%d"), m_id);
   DB_HANDLE hdb = DBConnectionPoolAcquireConnection();
   int rc = DBIsTableExist(hdb, table);
   if (rc == DBIsTableExist_Failure)
   {
      _tprintf(_T("WARNING: call to DBIsTableExist(\"%s\") failed\n"), table);
   }
   DBConnectionPoolReleaseConnection(hdb);
   return rc != DBIsTableExist_NotFound;
}

/**
 * Create table for storing geolocation history for this object
 */
bool NetObj::createLocationHistoryTable(DB_HANDLE hdb)
{
   TCHAR szQuery[256], szQueryTemplate[256];
   MetaDataReadStr(_T("LocationHistory"), szQueryTemplate, 255, _T(""));
   _sntprintf(szQuery, 256, szQueryTemplate, m_id);
   if (!DBQuery(hdb, szQuery))
		return false;

   return true;
}

/**
 * Set status calculation method
 */
void NetObj::setStatusCalculation(int method, int arg1, int arg2, int arg3, int arg4)
{
   lockProperties();
   m_iStatusCalcAlg = method;
   switch(method)
   {
      case SA_CALCULATE_SINGLE_THRESHOLD:
         m_iStatusSingleThreshold = arg1;
         break;
      case SA_CALCULATE_MULTIPLE_THRESHOLDS:
         m_iStatusThresholds[0] = arg1;
         m_iStatusThresholds[1] = arg2;
         m_iStatusThresholds[2] = arg3;
         m_iStatusThresholds[3] = arg4;
         break;
      default:
         break;
   }
   setModified();
   unlockProperties();
}

/**
 * Set status propagation method
 */
void NetObj::setStatusPropagation(int method, int arg1, int arg2, int arg3, int arg4)
{
   lockProperties();
   m_iStatusPropAlg = method;
   switch(method)
   {
      case SA_PROPAGATE_FIXED:
         m_iFixedStatus = arg1;
         break;
      case SA_PROPAGATE_RELATIVE:
         m_iStatusShift = arg1;
         break;
      case SA_PROPAGATE_TRANSLATED:
         m_iStatusTranslation[0] = arg1;
         m_iStatusTranslation[1] = arg2;
         m_iStatusTranslation[2] = arg3;
         m_iStatusTranslation[3] = arg4;
         break;
      default:
         break;
   }
   setModified();
   unlockProperties();
}

/**
 * Enter maintenance mode
 */
void NetObj::enterMaintenanceMode()
{
}

/**
 * Leave maintenance mode
 */
void NetObj::leaveMaintenanceMode()
{
}
