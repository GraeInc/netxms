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
** File: netmap_link.cpp
**
**/

#include "nxcore.h"

/**
 * Constructor
 */
NetworkMapLink::NetworkMapLink(uint32_t id, uint32_t e1, uint32_t e2, int type)
{
   m_id = id;
	m_element1 = e1;
	m_element2 = e2;
	m_type = type;
	m_name = nullptr;
	m_connectorName1 = nullptr;
	m_connectorName2 = nullptr;
	m_colorSource = MAP_LINK_COLOR_SOURCE_DEFAULT;
	m_color = 0;
	m_colorProvider = nullptr;
	m_config = nullptr;
	m_flags = 0;
}

/**
 * Constuctor: create link object from NXCP message
 */
NetworkMapLink::NetworkMapLink(NXCPMessage *msg, uint32_t baseId)
{
   m_id = msg->getFieldAsUInt32(baseId);
   m_name = msg->getFieldAsString(baseId + 1);
	m_type = msg->getFieldAsUInt16(baseId + 2);
	m_connectorName1 = msg->getFieldAsString(baseId + 3);
	m_connectorName2 = msg->getFieldAsString(baseId + 4);
	m_element1 = msg->getFieldAsUInt32(baseId + 5);
	m_element2 = msg->getFieldAsUInt32(baseId + 6);
	m_flags = msg->getFieldAsUInt32(baseId + 7);
   m_colorSource = static_cast<MapLinkColorSource>(msg->getFieldAsInt16(baseId + 8));
   m_color = msg->getFieldAsUInt32(baseId + 9);
   m_colorProvider = msg->getFieldAsString(baseId + 10);
   m_config = msg->getFieldAsString(baseId + 11);
}

/**
 * Map link destructor
 */
NetworkMapLink::~NetworkMapLink()
{
	MemFree(m_name);
	MemFree(m_connectorName1);
	MemFree(m_connectorName2);
   MemFree(m_colorProvider);
	MemFree(m_config);
}

/**
 * Set name
 */
void NetworkMapLink::setName(const TCHAR *name)
{
	MemFree(m_name);
	m_name = MemCopyString(name);
}

/**
 * Set connector 1 name
 */
void NetworkMapLink::setConnector1Name(const TCHAR *name)
{
	MemFree(m_connectorName1);
	m_connectorName1 = MemCopyString(name);
}

/**
 * Set connector 2 name
 */
void NetworkMapLink::setConnector2Name(const TCHAR *name)
{
	MemFree(m_connectorName2);
	m_connectorName2 = MemCopyString(name);
}

/**
 * Set color provider
 */
void NetworkMapLink::setColorProvider(const TCHAR *colorProvider)
{
   MemFree(m_colorProvider);
   m_colorProvider = MemCopyString(colorProvider);
}

/**
 * Set config(bendPoints, dciList, objectStatusList, routing)
 */
void NetworkMapLink::setConfig(const TCHAR *config)
{
   MemFree(m_config);
   m_config = MemCopyString(config);
}

/**
 * Swap connector information
 */
void NetworkMapLink::swap()
{
   uint32_t te = m_element1;
   m_element1 = m_element2;
   m_element2 = te;

   TCHAR *tn = m_connectorName1;
   m_connectorName1 = m_connectorName2;
   m_connectorName2 = tn;
}

/**
 * Fill NXCP message
 */
void NetworkMapLink::fillMessage(NXCPMessage *msg, uint32_t baseId) const
{
   msg->setField(baseId, m_id);
   msg->setField(baseId + 1, getName());
	msg->setField(baseId + 2, static_cast<int16_t>(m_type));
   msg->setField(baseId + 3, m_element1);
   msg->setField(baseId + 4, m_element2);
	msg->setField(baseId + 5, getConnector1Name());
	msg->setField(baseId + 6, getConnector2Name());
   msg->setField(baseId + 7, m_flags);
   msg->setField(baseId + 8, static_cast<int16_t>(m_colorSource));
   msg->setField(baseId + 9, m_color);
   msg->setField(baseId + 10, getColorProvider());
   msg->setField(baseId + 11, m_config);
}

/**
 * Serialize to JSON
 */
json_t *NetworkMapLink::toJson() const
{
   json_t *root = json_object();
   json_object_set_new(root, "element1", json_integer(m_element1));
   json_object_set_new(root, "element2", json_integer(m_element2));
   json_object_set_new(root, "type", json_integer(m_type));
   json_object_set_new(root, "name", json_string_t(m_name));
   json_object_set_new(root, "connectorName1", json_string_t(m_connectorName1));
   json_object_set_new(root, "connectorName2", json_string_t(m_connectorName2));
   json_object_set_new(root, "flags", json_integer(m_flags));
   json_object_set_new(root, "colorSource", json_integer(static_cast<int32_t>(m_colorSource)));
   json_object_set_new(root, "color", json_integer(m_color));
   json_object_set_new(root, "colorProvider", json_string_t(m_colorProvider));
   json_object_set_new(root, "config", json_string_t(m_config));
   return root;
}
