/*
** NetXMS - Network Management System
** Copyright (C) 2003-2017 Victor Kirhenshtein
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
** File: netmap_element.cpp
**
**/

#include "nxcore.h"

/**************************
 * Network Map Element
 **************************/

/**
 * Generic element default constructor
 */
NetworkMapElement::NetworkMapElement(uint32_t id, uint32_t flags)
{
	m_id = id;
	m_type = MAP_ELEMENT_GENERIC;
	m_posX = 0;
	m_posY = 0;
	m_flags = flags;
}

/**
 * Generic element config constructor
 */
NetworkMapElement::NetworkMapElement(uint32_t id, Config *config, uint32_t flags)
{
	m_id = id;
	m_type = config->getValueAsInt(_T("/type"), MAP_ELEMENT_GENERIC);
	m_posX = config->getValueAsInt(_T("/posX"), 0);
	m_posY = config->getValueAsInt(_T("/posY"), 0);
	m_flags = flags;
}

/**
 * Generic element NXCP constructor
 */
NetworkMapElement::NetworkMapElement(NXCPMessage *msg, uint32_t baseId)
{
	m_id = msg->getFieldAsUInt32(baseId);
	m_type = (LONG)msg->getFieldAsUInt16(baseId + 1);
	m_posX = (LONG)msg->getFieldAsUInt32(baseId + 2);
	m_posY = (LONG)msg->getFieldAsUInt32(baseId + 3);
   m_flags = 0;
}

/**
 * Generic element destructor
 */
NetworkMapElement::~NetworkMapElement()
{
}

/**
 * Update element's persistent configuration
 */
void NetworkMapElement::updateConfig(Config *config)
{
	config->setValue(_T("/type"), m_type);
	config->setValue(_T("/posX"), m_posX);
	config->setValue(_T("/posY"), m_posY);
}

/**
 * Fill NXCP message with element's data
 */
void NetworkMapElement::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
	msg->setField(baseId, m_id);
	msg->setField(baseId + 1, (uint16_t)m_type);
	msg->setField(baseId + 2, (uint32_t)m_posX);
	msg->setField(baseId + 3, (uint32_t)m_posY);
   msg->setField(baseId + 4, m_flags);
}

/**
 * Set element's position
 */
void NetworkMapElement::setPosition(int32_t x, int32_t y)
{
	m_posX = x;
	m_posY = y;
}

/**
 * Update internal fields form previous object
 */
void NetworkMapElement::updateInternalFields(NetworkMapElement *e)
{
   m_flags = e->m_flags;
}

/**
 * Serialize object to JSON
 */
json_t *NetworkMapElement::toJson() const
{
   json_t *root = json_object();
   json_object_set_new(root, "id", json_integer(m_id));
   json_object_set_new(root, "type", json_integer(m_type));
   json_object_set_new(root, "posX", json_integer(m_posX));
   json_object_set_new(root, "posY", json_integer(m_posY));
   json_object_set_new(root, "flags", json_integer(m_flags));
   return root;
}

/**********************
 * Network Map Object
 **********************/

/**
 * Object element default constructor
 */
NetworkMapObject::NetworkMapObject(uint32_t id, uint32_t objectId, uint32_t flags) : NetworkMapElement(id, flags)
{
	m_type = MAP_ELEMENT_OBJECT;
	m_objectId = objectId;
	m_width = 100;
	m_height = 100;
}

/**
 * Object element config constructor
 */
NetworkMapObject::NetworkMapObject(uint32_t id, Config *config, uint32_t flags) : NetworkMapElement(id, config, flags)
{
	m_objectId = config->getValueAsUInt(_T("/objectId"), 0);
	m_width = config->getValueAsUInt(_T("/width"), 100);
   m_height = config->getValueAsUInt(_T("/height"), 100);
}

/**
 * Object element NXCP constructor
 */
NetworkMapObject::NetworkMapObject(NXCPMessage *msg, uint32_t baseId) : NetworkMapElement(msg, baseId)
{
	m_objectId = msg->getFieldAsUInt32(baseId + 10);
	m_width = msg->getFieldAsUInt32(baseId + 11);
   m_height = msg->getFieldAsUInt32(baseId + 12);
}

/**
 * Object element destructor
 */
NetworkMapObject::~NetworkMapObject()
{
}

/**
 * Update element's persistent configuration
 */
void NetworkMapObject::updateConfig(Config *config)
{
	NetworkMapElement::updateConfig(config);
	config->setValue(_T("/objectId"), m_objectId);
	config->setValue(_T("/width"), m_width);
   config->setValue(_T("/height"), m_height);
}

/**
 * Fill NXCP message with element's data
 */
void NetworkMapObject::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
	NetworkMapElement::fillMessage(msg, baseId);
	msg->setField(baseId + 10, m_objectId);
   msg->setField(baseId + 11, m_width);
   msg->setField(baseId + 12, m_height);
}

/**
 * Serialize to JSON
 */
json_t *NetworkMapObject::toJson() const
{
   json_t *root = NetworkMapElement::toJson();
   json_object_set_new(root, "objectId", json_integer(m_objectId));
   json_object_set_new(root, "width", json_integer(m_width));
   json_object_set_new(root, "height", json_integer(m_height));
   return root;
}

/**************************
 * Network Map Decoration
 **************************/

/**
 * Decoration element default constructor
 */
NetworkMapDecoration::NetworkMapDecoration(uint32_t id, LONG decorationType, uint32_t flags) : NetworkMapElement(id, flags)
{
	m_type = MAP_ELEMENT_DECORATION;
	m_decorationType = decorationType;
	m_color = 0;
	m_title = nullptr;
	m_width = 50;
	m_height = 20;
}

/**
 * Decoration element config constructor
 */
NetworkMapDecoration::NetworkMapDecoration(uint32_t id, Config *config, uint32_t flags) : NetworkMapElement(id, config, flags)
{
	m_decorationType = config->getValueAsInt(_T("/decorationType"), 0);
	m_color = config->getValueAsUInt(_T("/color"), 0);
	m_title = MemCopyString(config->getValue(_T("/title"), _T("")));
	m_width = config->getValueAsInt(_T("/width"), 0);
	m_height = config->getValueAsInt(_T("/height"), 0);
}

/**
 * Decoration element NXCP constructor
 */
NetworkMapDecoration::NetworkMapDecoration(NXCPMessage *msg, uint32_t baseId) : NetworkMapElement(msg, baseId)
{
	m_decorationType = (LONG)msg->getFieldAsUInt32(baseId + 10);
	m_color = msg->getFieldAsUInt32(baseId + 11);
	m_title = msg->getFieldAsString(baseId + 12);
	m_width = (LONG)msg->getFieldAsUInt32(baseId + 13);
	m_height = (LONG)msg->getFieldAsUInt32(baseId + 14);
}

/**
 * Decoration element destructor
 */
NetworkMapDecoration::~NetworkMapDecoration()
{
	MemFree(m_title);
}

/**
 * Update decoration element's persistent configuration
 */
void NetworkMapDecoration::updateConfig(Config *config)
{
	NetworkMapElement::updateConfig(config);
	config->setValue(_T("/decorationType"), m_decorationType);
	config->setValue(_T("/color"), m_color);
	config->setValue(_T("/title"), CHECK_NULL_EX(m_title));
	config->setValue(_T("/width"), m_width);
	config->setValue(_T("/height"), m_height);
}

/**
 * Fill NXCP message with element's data
 */
void NetworkMapDecoration::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
	NetworkMapElement::fillMessage(msg, baseId);
	msg->setField(baseId + 10, (uint32_t)m_decorationType);
	msg->setField(baseId + 11, m_color);
	msg->setField(baseId + 12, CHECK_NULL_EX(m_title));
	msg->setField(baseId + 13, (uint32_t)m_width);
	msg->setField(baseId + 14, (uint32_t)m_height);
}

/**
 * Serialize to JSON
 */
json_t *NetworkMapDecoration::toJson() const
{
   json_t *root = NetworkMapElement::toJson();
   json_object_set_new(root, "decorationType", json_integer(m_decorationType));
   json_object_set_new(root, "color", json_integer(m_color));
   json_object_set_new(root, "title", json_string_t(m_title));
   json_object_set_new(root, "width", json_integer(m_width));
   json_object_set_new(root, "height", json_integer(m_height));
   return root;
}

/*****************************
 * Network Map DCI Container
 *****************************/

/**
 * DCI container default constructor
 */
NetworkMapDCIContainer::NetworkMapDCIContainer(uint32_t id, TCHAR* xmlDCIList, uint32_t flags) : NetworkMapElement(id, flags)
{
	m_type = MAP_ELEMENT_DCI_CONTAINER;
   m_xmlDCIList = MemCopyString(xmlDCIList);
}

/**
 * DCI container config constructor
 */
NetworkMapDCIContainer::NetworkMapDCIContainer(uint32_t id, Config *config, uint32_t flags) : NetworkMapElement(id, config, flags)
{
   m_xmlDCIList = MemCopyString(config->getValue(_T("/DCIList"), _T("")));
}

/**
 * DCI container NXCP constructor
 */
NetworkMapDCIContainer::NetworkMapDCIContainer(NXCPMessage *msg, uint32_t baseId) : NetworkMapElement(msg, baseId)
{
   m_xmlDCIList = msg->getFieldAsString(baseId + 10);
}

/**
 * DCI container destructor
 */
NetworkMapDCIContainer::~NetworkMapDCIContainer()
{
   MemFree(m_xmlDCIList);
}

/**
 * Update container's persistent configuration
 */
void NetworkMapDCIContainer::updateConfig(Config *config)
{
	NetworkMapElement::updateConfig(config);
	config->setValue(_T("/DCIList"), m_xmlDCIList);
}

/**
 * Fill NXCP message with container's data
 */
void NetworkMapDCIContainer::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
	NetworkMapElement::fillMessage(msg, baseId);
	msg->setField(baseId + 10, m_xmlDCIList);
}

/**
 * Serialize to JSON
 */
json_t *NetworkMapDCIContainer::toJson() const
{
   json_t *root = NetworkMapElement::toJson();
   json_object_set_new(root, "xmlDCIList", json_string_t(m_xmlDCIList));
   return root;
}

/**************************
 * Network Map DCI Image
 **************************/

/**
 * DCI image default constructor
 */
NetworkMapDCIImage::NetworkMapDCIImage(uint32_t id, TCHAR* config, uint32_t flags) : NetworkMapElement(id, flags)
{
	m_type = MAP_ELEMENT_DCI_IMAGE;
   m_config = MemCopyString(config);
}

/**
 * DCI image config constructor
 */
NetworkMapDCIImage::NetworkMapDCIImage(uint32_t id, Config *config, uint32_t flags) : NetworkMapElement(id, config, flags)
{
   m_config = MemCopyString(config->getValue(_T("/DCIList"), _T("")));
}

/**
 * DCI image NXCP constructor
 */
NetworkMapDCIImage::NetworkMapDCIImage(NXCPMessage *msg, uint32_t baseId) : NetworkMapElement(msg, baseId)
{
   m_config = msg->getFieldAsString(baseId + 10);
}

/**
 * DCI image destructor
 */
NetworkMapDCIImage::~NetworkMapDCIImage()
{
   MemFree(m_config);
}

/**
 * Update image's persistent configuration
 */
void NetworkMapDCIImage::updateConfig(Config *config)
{
	NetworkMapElement::updateConfig(config);
	config->setValue(_T("/DCIList"), m_config);
}

/**
 * Fill NXCP message with container's data
 */
void NetworkMapDCIImage::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
	NetworkMapElement::fillMessage(msg, baseId);
	msg->setField(baseId + 10, m_config);
}

/**
 * Serialize to JSON
 */
json_t *NetworkMapDCIImage::toJson() const
{
   json_t *root = NetworkMapElement::toJson();
   json_object_set_new(root, "config", json_string_t(m_config));
   return root;
}

/**************************
 * Network Map Text Box
 **************************/

/**
 * Text Box default constructor
 */
NetworkMapTextBox::NetworkMapTextBox(uint32_t id, TCHAR* config, uint32_t flags) : NetworkMapElement(id, flags)
{
   m_type = MAP_ELEMENT_TEXT_BOX;
   m_config = MemCopyString(config);
}

/**
 * Text Box config constructor
 */
NetworkMapTextBox::NetworkMapTextBox(uint32_t id, Config *config, uint32_t flags) : NetworkMapElement(id, config, flags)
{
   m_config = MemCopyString(config->getValue(_T("/TextBox"), _T("")));
}

/**
 * DCI image NXCP constructor
 */
NetworkMapTextBox::NetworkMapTextBox(NXCPMessage *msg, uint32_t baseId) : NetworkMapElement(msg, baseId)
{
   m_config = msg->getFieldAsString(baseId + 10);
}

/**
 * DCI image destructor
 */
NetworkMapTextBox::~NetworkMapTextBox()
{
   MemFree(m_config);
}

/**
 * Update image's persistent configuration
 */
void NetworkMapTextBox::updateConfig(Config *config)
{
   NetworkMapElement::updateConfig(config);
   config->setValue(_T("/TextBox"), m_config);
}

/**
 * Fill NXCP message with container's data
 */
void NetworkMapTextBox::fillMessage(NXCPMessage *msg, uint32_t baseId)
{
   NetworkMapElement::fillMessage(msg, baseId);
   msg->setField(baseId + 10, m_config);
}

/**
 * Serialize to JSON
 */
json_t *NetworkMapTextBox::toJson() const
{
   json_t *root = NetworkMapElement::toJson();
   json_object_set_new(root, "config", json_string_t(m_config));
   return root;
}
