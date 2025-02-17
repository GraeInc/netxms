/*
** NetXMS - Network Management System
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
** File: netxms_maps.h
**
**/

#ifndef _netxms_maps_h_
#define _netxms_maps_h_

#include <nxconfig.h>

/**
 * Constants
 */
#define MAX_CONNECTOR_NAME		128
#define MAX_LINK_NAME         64
#define MAX_PORT_COUNT			16
#define MAX_BEND_POINTS       16

/**
 * User access rights
 */
#define MAP_ACCESS_READ       0x0001
#define MAP_ACCESS_WRITE      0x0002
#define MAP_ACCESS_ACL        0x0004
#define MAP_ACCESS_DELETE     0x0008

/**
 * Object link types
 */
#define LINK_TYPE_NORMAL     	  0
#define LINK_TYPE_VPN        	  1
#define LINK_TYPE_MULTILINK     2
#define LINK_TYPE_AGENT_TUNNEL  3
#define LINK_TYPE_AGENT_PROXY   4
#define LINK_TYPE_SSH_PROXY     5
#define LINK_TYPE_SNMP_PROXY    6
#define LINK_TYPE_ICMP_PROXY    7
#define LINK_TYPE_SENSOR_PROXY  8
#define LINK_TYPE_ZONE_PROXY    9

/**
 * Link between objects
 */
struct NXCORE_EXPORTABLE ObjLink
{
   uint32_t id1;
   uint32_t id2;
   int type;
	TCHAR port1[MAX_CONNECTOR_NAME];
	TCHAR port2[MAX_CONNECTOR_NAME];
	int portIdCount;
	uint32_t portIdArray1[MAX_PORT_COUNT];
	uint32_t portIdArray2[MAX_PORT_COUNT];
	uint32_t flags;
	MutableString name;

   ObjLink();
   ObjLink(const ObjLink& src);

   ObjLink& operator =(const ObjLink& src)
   {
      update(src);
      return *this;
   }

   void update(const ObjLink& src);
 };

#ifdef _WIN32
template class NXCORE_EXPORTABLE ObjectArray<ObjLink>;
#endif

/**
 * Connected object list
 */
class NXCORE_EXPORTABLE NetworkMapObjectList
{
protected:
   IntegerArray<uint32_t> m_objectList;
   ObjectArray<ObjLink> m_linkList;
   bool m_allowDuplicateLinks;

public:
   NetworkMapObjectList();
   NetworkMapObjectList(const NetworkMapObjectList& src);

   void merge(const NetworkMapObjectList& src);

   void addObject(uint32_t id);
   void linkObjects(uint32_t id1, uint32_t id2, int linkType = LINK_TYPE_NORMAL, const TCHAR *linkName = nullptr, const TCHAR *port1 = nullptr, const TCHAR *port2 = nullptr);
   void linkObjectsEx(uint32_t id1, uint32_t id2, const TCHAR *port1, const TCHAR *port2, uint32_t portId1, uint32_t portId2, const TCHAR *name = nullptr);
   void removeObject(uint32_t id);
   void clear();
   void filterObjects(bool (*filter)(uint32_t, void *), void *context);
   template<typename C> void filterObjects(bool (*filter)(uint32_t, C *), C *context) { filterObjects(reinterpret_cast<bool (*)(uint32_t, void *)>(filter), context); }

   int getNumObjects() const { return m_objectList.size(); }
   const IntegerArray<uint32_t>& getObjects() const { return m_objectList; }
   int getNumLinks() const { return m_linkList.size(); }
   const ObjectArray<ObjLink>& getLinks() const { return m_linkList; }

	void createMessage(NXCPMessage *pMsg);

	bool isLinkExist(uint32_t objectId1, uint32_t objectId2, int type) const;
	ObjLink *getLink(uint32_t objectId1, uint32_t objectId2, int linkType);
	bool isObjectExist(uint32_t objectId) const;

	void setAllowDuplicateLinks(bool allowDuplicateLinks) { m_allowDuplicateLinks = allowDuplicateLinks; }
	bool isAllowDuplicateLinks() const { return m_allowDuplicateLinks; }
};

#ifdef _WIN32
template class NXCORE_EXPORTABLE shared_ptr<NetworkMapObjectList>;
#endif

/**
 * Map element types
 */
#define MAP_ELEMENT_GENERIC         0
#define MAP_ELEMENT_OBJECT          1
#define MAP_ELEMENT_DECORATION      2
#define MAP_ELEMENT_DCI_CONTAINER   3
#define MAP_ELEMENT_DCI_IMAGE       4
#define MAP_ELEMENT_TEXT_BOX        5

/**
 * Decoration types
 */
#define MAP_DECORATION_GROUP_BOX 0
#define MAP_DECORATION_IMAGE     1

/**
 * Routing modes for connections
 */
#define ROUTING_DEFAULT          0
#define ROUTING_DIRECT           1
#define ROUTING_MANHATTAN        2
#define ROUTING_BENDPOINTS       3

/**
 * Possible flag values for NetworMapElements
 */
#define AUTO_GENERATED           1

/**
 * Generic map element
 */
class NetworkMapElement
{
protected:
   uint32_t m_id;
	int32_t m_type;
   int32_t m_posX;
   int32_t m_posY;
   uint32_t m_flags;

   NetworkMapElement(const NetworkMapElement& src);

public:
	NetworkMapElement(uint32_t id, uint32_t flags = 0);
	NetworkMapElement(uint32_t id, Config *config, uint32_t flags = 0);
	NetworkMapElement(const NXCPMessage& msg, uint32_t baseId);
	virtual ~NetworkMapElement();

   virtual void updateInternalFields(NetworkMapElement *e);
	virtual void updateConfig(Config *config);
	virtual void fillMessage(NXCPMessage *msg, uint32_t baseId);
	virtual json_t *toJson() const;
   virtual NetworkMapElement *clone() const;

   uint32_t getId() const { return m_id; }
   int32_t getType() const { return m_type; }
   int32_t getPosX() const { return m_posX; }
   int32_t getPosY() const { return m_posY; }
   uint32_t getFlags() const { return m_flags; }

	void setPosition(int32_t x, int32_t y);
};

/**
 * Object map element
 */
class NetworkMapObject : public NetworkMapElement
{
protected:
	uint32_t m_objectId;
   uint32_t m_width;
   uint32_t m_height;

   NetworkMapObject(const NetworkMapObject& src);

public:
	NetworkMapObject(uint32_t id, uint32_t objectId, uint32_t flags = 0);
	NetworkMapObject(uint32_t id, Config *config, uint32_t flags = 0);
	NetworkMapObject(const NXCPMessage& msg, uint32_t baseId);
	virtual ~NetworkMapObject();

	virtual void updateConfig(Config *config) override;
	virtual void fillMessage(NXCPMessage *msg, uint32_t baseId) override;
   virtual json_t *toJson() const override;
   virtual NetworkMapElement *clone() const override;

	uint32_t getObjectId() const { return m_objectId; }
};

/**
 * Decoration map element
 */
class NetworkMapDecoration : public NetworkMapElement
{
protected:
   int32_t m_decorationType;
	UINT32 m_color;
	TCHAR *m_title;
   int32_t m_width;
   int32_t m_height;

   NetworkMapDecoration(const NetworkMapDecoration& src);

public:
	NetworkMapDecoration(uint32_t id, LONG decorationType, uint32_t flags = 0);
	NetworkMapDecoration(uint32_t id, Config *config, uint32_t flags = 0);
	NetworkMapDecoration(const NXCPMessage& msg, uint32_t baseId);
	virtual ~NetworkMapDecoration();

	virtual void updateConfig(Config *config) override;
	virtual void fillMessage(NXCPMessage *msg, uint32_t baseId) override;
   virtual json_t *toJson() const override;
   virtual NetworkMapElement *clone() const override;

   int32_t getDecorationType() const { return m_decorationType; }
   uint32_t getColor() const { return m_color; }
	const TCHAR *getTitle() const { return CHECK_NULL_EX(m_title); }

   int32_t getWidth() const { return m_width; }
   int32_t getHeight() const { return m_height; }
};

/**
 * DCI map conatainer
 */
class NetworkMapDCIContainer : public NetworkMapElement
{
protected:
	TCHAR *m_xmlDCIList;

	NetworkMapDCIContainer(const NetworkMapDCIContainer& src);

public:
	NetworkMapDCIContainer(uint32_t id, TCHAR* objectDCIList, uint32_t flags = 0);
	NetworkMapDCIContainer(uint32_t id, Config *config, uint32_t flags = 0);
	NetworkMapDCIContainer(const NXCPMessage& msg, uint32_t baseId);
	virtual ~NetworkMapDCIContainer();

	virtual void updateConfig(Config *config) override;
	virtual void fillMessage(NXCPMessage *msg, uint32_t baseId) override;
   virtual json_t *toJson() const override;
   virtual NetworkMapElement *clone() const override;

	const TCHAR *getObjectDCIList() const { return m_xmlDCIList; }
};

/**
 * Network map text box
 */
class NetworkMapTextBox : public NetworkMapElement
{
protected:
	TCHAR *m_config;

	NetworkMapTextBox(const NetworkMapTextBox& src);

public:
	NetworkMapTextBox(uint32_t id, TCHAR* objectDCIList, uint32_t flags = 0);
	NetworkMapTextBox(uint32_t id, Config *config, uint32_t flags = 0);
	NetworkMapTextBox(const NXCPMessage& msg, uint32_t baseId);
	virtual ~NetworkMapTextBox();

	virtual void updateConfig(Config *config) override;
	virtual void fillMessage(NXCPMessage *msg, uint32_t baseId) override;
   virtual json_t *toJson() const override;
   virtual NetworkMapElement *clone() const override;

	const TCHAR *getObjectDCIList() const { return m_config; }
};

/**
 * DCI map image
 */
class NetworkMapDCIImage : public NetworkMapElement
{
protected:
   TCHAR *m_config;

   NetworkMapDCIImage(const NetworkMapDCIImage& src);

public:
   NetworkMapDCIImage(uint32_t id, TCHAR* objectDCIList, uint32_t flags = 0);
   NetworkMapDCIImage(uint32_t id, Config *config, uint32_t flags = 0);
   NetworkMapDCIImage(const NXCPMessage& msg, uint32_t baseId);
   virtual ~NetworkMapDCIImage();

   virtual void updateConfig(Config *config) override;
   virtual void fillMessage(NXCPMessage *msg, uint32_t baseId) override;
   virtual json_t *toJson() const override;
   virtual NetworkMapElement *clone() const override;

   const TCHAR *getObjectDCIList() const { return m_config; }
};

/**
 * Network map link color source
 */
enum MapLinkColorSource
{
   MAP_LINK_COLOR_SOURCE_UNDEFINED = -1,
   MAP_LINK_COLOR_SOURCE_DEFAULT = 0,
   MAP_LINK_COLOR_SOURCE_OBJECT_STATUS = 1,
   MAP_LINK_COLOR_SOURCE_CUSTOM_COLOR = 2,
   MAP_LINK_COLOR_SOURCE_SCRIPT = 3
};

/**
 * Link on map
 */
class NetworkMapLink
{
protected:
   uint32_t m_id;
	uint32_t m_element1;
	uint32_t m_element2;
	int m_type;
	TCHAR *m_name;
	TCHAR *m_connectorName1;
	TCHAR *m_connectorName2;
	uint32_t m_flags;
	MapLinkColorSource m_colorSource;
	uint32_t m_color;
	TCHAR *m_colorProvider;
	TCHAR *m_config;

public:
	NetworkMapLink(uint32_t id, uint32_t e1, uint32_t e2, int type);
	NetworkMapLink(const NXCPMessage& msg, uint32_t baseId);
   NetworkMapLink(const NetworkMapLink& src);
	virtual ~NetworkMapLink();

	void fillMessage(NXCPMessage *msg, uint32_t baseId) const;
   json_t *toJson() const;

   uint32_t getId() const { return m_id; }
   uint32_t getElement1() const { return m_element1; }
   uint32_t getElement2() const { return m_element2; }

	const TCHAR *getName() const { return CHECK_NULL_EX(m_name); }
	const TCHAR *getConnector1Name() const { return CHECK_NULL_EX(m_connectorName1); }
	const TCHAR *getConnector2Name() const { return CHECK_NULL_EX(m_connectorName2); }
	int getType() const { return m_type; }
	uint32_t getFlags() const { return m_flags; }
	bool checkFlagSet(uint32_t flag) const { return (m_flags & flag) != 0; }
	MapLinkColorSource getColorSource() const { return m_colorSource; }
	uint32_t getColor() const { return m_color; }
   const TCHAR *getColorProvider() const { return CHECK_NULL_EX(m_colorProvider); }
   const TCHAR *getConfig() const { return CHECK_NULL_EX(m_config); }

   bool update(const ObjLink& src);

	void setName(const TCHAR *name);
	void setConnectedElements(uint32_t e1, uint32_t e2) { m_element1 = e1; m_element2 = e2; }
	void setConnector1Name(const TCHAR *name);
	void setConnector2Name(const TCHAR *name);
	void setFlags(uint32_t flags) { m_flags = flags; }
   void setColorSource(MapLinkColorSource colorSource) { m_colorSource = colorSource; }
   void setColor(uint32_t color) { m_color = color; }
   void setColorProvider(const TCHAR *colorProvider);
	void setConfig(const TCHAR *config);
	void swap();
};

#endif
