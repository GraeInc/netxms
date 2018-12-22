/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2018 Victor Kirhenshtein
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
** File: nddrv.h
**
**/

#ifndef _nddrv_h_
#define _nddrv_h_

#include <nms_common.h>
#include <nxsrvapi.h>

/**
 * API version
 */
#define NDDRV_API_VERSION           7

/**
 * Begin driver list
 */
#define NDD_BEGIN_DRIVER_LIST static ObjectArray<NetworkDeviceDriver> *s_createInstances() { \
   ObjectArray<NetworkDeviceDriver> *drivers = new ObjectArray<NetworkDeviceDriver>(4, 4, false);

/**
 * Declare driver within list
 */
#define NDD_DRIVER(implClass) \
   drivers->add(new implClass);

/**
 * End driver list
 */
#define NDD_END_DRIVER_LIST \
   return drivers; \
}

/**
 * NDD module entry point
 */
#define DECLARE_NDD_MODULE_ENTRY_POINT \
extern "C" __EXPORT_VAR(int nddAPIVersion); \
__EXPORT_VAR(int nddAPIVersion) = NDDRV_API_VERSION; \
extern "C" __EXPORT ObjectArray<NetworkDeviceDriver> *nddCreateInstances() { return s_createInstances(); }

/**
 * NDD module entry point - single driver
 */
#define DECLARE_NDD_ENTRY_POINT(implClass) \
extern "C" __EXPORT_VAR(int nddAPIVersion); \
__EXPORT_VAR(int nddAPIVersion) = NDDRV_API_VERSION; \
extern "C" __EXPORT ObjectArray<NetworkDeviceDriver> *nddCreateInstances() { \
   ObjectArray<NetworkDeviceDriver> *drivers = new ObjectArray<NetworkDeviceDriver>(4, 4, false); \
   drivers->add(new implClass); \
   return drivers; \
}

/**
 * Port numbering schemes
 */
enum PortNumberingScheme
{
	NDD_PN_UNKNOWN = 0, // port layout not known to driver
	NDD_PN_CUSTOM = 1,  // custom layout, driver defines location of each port
	NDD_PN_LR_UD = 2,   // left-to-right, then up-down:
	                    //  1 2 3 4
	                    //  5 6 7 8
	NDD_PN_LR_DU = 3,   // left-to-right, then down-up:
	                    //  5 6 7 8
	                    //  1 2 3 4
	NDD_PN_UD_LR = 4,   // up-down, then left-right:
	                    //  1 3 5 7
	                    //  2 4 6 8
	NDD_PN_DU_LR = 5    // down-up, then left-right:
	                    //  2 4 6 8
	                    //  1 3 5 7
};

/**
 * Modules orientation on the switch
 */
enum ModuleOrientation
{
	NDD_ORIENTATION_HORIZONTAL = 0,
	NDD_ORIENTATION_VERTICAL = 1
};

/**
 * Cluster modes
 */
enum
{
   CLUSTER_MODE_UNKNOWN = -1,
   CLUSTER_MODE_STANDALONE = 0,
   CLUSTER_MODE_ACTIVE = 1,
   CLUSTER_MODE_STANDBY = 2
};


/**
 * Access point state
 */
enum AccessPointState
{
   AP_ADOPTED = 0,
   AP_UNADOPTED = 1,
   AP_DOWN = 2,
   AP_UNKNOWN = 3
};

/**
 * Module layout definition
 */
struct NDD_MODULE_LAYOUT
{
	int rows;					// number of port rows on the module
	int numberingScheme;		// port numbering scheme
	int columns;            // number of columns for custom layout
	WORD portRows[256];     // row numbers for ports
	WORD portColumns[256];  // column numbers for ports
};

/**
 * Radio interface information
 */
struct LIBNXSRV_EXPORTABLE RadioInterfaceInfo
{
	int index;
	TCHAR name[64];
	BYTE macAddr[MAC_ADDR_LENGTH];
   UINT32 channel;
   INT32 powerDBm;
   INT32 powerMW;

   json_t *toJson() const;
};

/**
 * Wireless access point information
 */
class LIBNXSRV_EXPORTABLE AccessPointInfo
{
private:
   UINT32 m_index;
   BYTE m_macAddr[MAC_ADDR_LENGTH];
   InetAddress m_ipAddr;
   AccessPointState m_state;
   TCHAR *m_name;
   TCHAR *m_vendor;
   TCHAR *m_model;
   TCHAR *m_serial;
	ObjectArray<RadioInterfaceInfo> *m_radioInterfaces;

public:
   AccessPointInfo(UINT32 index, const BYTE *macAddr, const InetAddress& ipAddr, AccessPointState state, const TCHAR *name, const TCHAR *vendor, const TCHAR *model, const TCHAR *serial);
   ~AccessPointInfo();

	void addRadioInterface(RadioInterfaceInfo *iface);

   UINT32 getIndex() { return m_index; }
	const BYTE *getMacAddr() { return m_macAddr; }
   const InetAddress& getIpAddr() { return m_ipAddr; }
	AccessPointState getState() { return m_state; }
	const TCHAR *getName() { return m_name; }
	const TCHAR *getVendor() { return m_vendor; }
	const TCHAR *getModel() { return m_model; }
	const TCHAR *getSerial() { return m_serial; }
	const ObjectArray<RadioInterfaceInfo> *getRadioInterfaces() { return m_radioInterfaces; }
};

/**
 * Wireless station AP match policy
 */
#define AP_MATCH_BY_RFINDEX   0
#define AP_MATCH_BY_BSSID     1

/**
 * Wireless station information
 */
struct WirelessStationInfo
{
	// This part filled by driver
   BYTE macAddr[MAC_ADDR_LENGTH];
	UINT32 ipAddr;	// IP address, must be in host byte order
	int rfIndex;	// radio interface index
   BYTE bssid[MAC_ADDR_LENGTH];
   short apMatchPolicy;
	TCHAR ssid[MAX_OBJECT_NAME];
   int vlan;
   int signalStrength;
   UINT32 txRate;
   UINT32 rxRate;

	// This part filled by core
	UINT32 apObjectId;
	UINT32 nodeId;
   TCHAR rfName[MAX_OBJECT_NAME];
};

/**
 * Base class for driver data
 */
class LIBNXSRV_EXPORTABLE DriverData
{
protected:
   UINT32 m_nodeId;
   uuid m_nodeGuid;
   TCHAR m_nodeName[MAX_OBJECT_NAME];

public:
   DriverData();
   virtual ~DriverData();

   void attachToNode(UINT32 nodeId, const uuid& nodeGuid, const TCHAR *nodeName);

   UINT32 getNodeId() const { return m_nodeId; }
   const uuid& getNodeGuid() const { return m_nodeGuid; }
   const TCHAR *getNodeName() const { return m_nodeName; }
};

/**
 * Storage type
 */
enum HostMibStorageType
{
   hrStorageCompactDisc = 7,
   hrStorageFixedDisk = 4,
   hrStorageFlashMemory = 9,
   hrStorageFloppyDisk = 6,
   hrStorageNetworkDisk = 10,
   hrStorageOther = 1,
   hrStorageRam = 2,
   hrStorageRamDisk = 8,
   hrStorageRemovableDisk = 5,
   hrStorageVirtualMemory = 3
};

/**
 * Storage entry
 */
struct LIBNXSRV_EXPORTABLE HostMibStorageEntry
{
   TCHAR name[128];
   UINT32 unitSize;
   UINT32 size;
   UINT32 used;
   HostMibStorageType type;
   UINT32 oid[12];
   time_t lastUpdate;

   void getFree(TCHAR *buffer, size_t len) const;
   void getFreePerc(TCHAR *buffer, size_t len) const;
   void getTotal(TCHAR *buffer, size_t len) const;
   void getUsed(TCHAR *buffer, size_t len) const;
   void getUsedPerc(TCHAR *buffer, size_t len) const;
   bool getMetric(const TCHAR *name, TCHAR *buffer, size_t len) const;
};

/**
 * Host MIB support for drivers
 */
class LIBNXSRV_EXPORTABLE HostMibDriverData : public DriverData
{
protected:
   ObjectArray<HostMibStorageEntry> *m_storage;
   time_t m_storageCacheTimestamp;
   MUTEX m_storageCacheMutex;

   UINT32 updateStorageCacheCallback(SNMP_Variable *v, SNMP_Transport *snmp);

public:
   HostMibDriverData();
   virtual ~HostMibDriverData();

   void updateStorageCache(SNMP_Transport *snmp);
   const HostMibStorageEntry *getStorageEntry(SNMP_Transport *snmp, const TCHAR *name, HostMibStorageType type);
   const HostMibStorageEntry *getPhysicalMemory(SNMP_Transport *snmp) { return getStorageEntry(snmp, NULL, hrStorageRam); }
};

/**
 * Base class for device drivers
 */
class LIBNXSRV_EXPORTABLE NetworkDeviceDriver
{
protected:
   void registerHostMibMetrics(ObjectArray<AgentParameterDefinition> *metrics);
   DataCollectionError getHostMibMetric(SNMP_Transport *snmp, HostMibDriverData *driverData, const TCHAR *name, TCHAR *value, size_t size);

public:
   NetworkDeviceDriver();
   virtual ~NetworkDeviceDriver();

   virtual const TCHAR *getName();
   virtual const TCHAR *getVersion();

   virtual const TCHAR *getCustomTestOID();
   virtual int isPotentialDevice(const TCHAR *oid);
   virtual bool isDeviceSupported(SNMP_Transport *snmp, const TCHAR *oid);
   virtual void analyzeDevice(SNMP_Transport *snmp, const TCHAR *oid, StringMap *attributes, DriverData **driverData);
   virtual InterfaceList *getInterfaces(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData, int useAliases, bool useIfXTable);
   virtual void getInterfaceState(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData, UINT32 ifIndex, 
                                  int ifTableSuffixLen, UINT32 *ifTableSuffix, InterfaceAdminState *adminState, InterfaceOperState *operState);
   virtual VlanList *getVlans(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData);
   virtual int getModulesOrientation(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData);
   virtual void getModuleLayout(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData, int module, NDD_MODULE_LAYOUT *layout);
   virtual bool isPerVlanFdbSupported();
   virtual int getClusterMode(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData);
   virtual bool isWirelessController(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData);
   virtual ObjectArray<AccessPointInfo> *getAccessPoints(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData);
   virtual ObjectArray<WirelessStationInfo> *getWirelessStations(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData);
   virtual AccessPointState getAccessPointState(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData,
                                                UINT32 apIndex, const BYTE *macAddr, const InetAddress& ipAddr);
   virtual bool hasMetrics();
   virtual DataCollectionError getMetric(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData, const TCHAR *name, TCHAR *value, size_t size);
   virtual ObjectArray<AgentParameterDefinition> *getAvailableMetrics(SNMP_Transport *snmp, StringMap *attributes, DriverData *driverData);
   virtual ArpCache *getArpCache(SNMP_Transport *snmp, DriverData *driverData);
};

#endif   /* _nddrv_h_ */
