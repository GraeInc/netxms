/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2022 Raden Solutions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.websvc.handlers;

import org.json.JSONObject;
import org.netxms.client.NXCObjectCreationData;
import org.netxms.client.NXCObjectModificationData;
import org.netxms.client.NXCSession;
import org.netxms.client.constants.RCC;
import org.netxms.client.objects.AbstractObject;
import org.netxms.websvc.json.JsonTools;
import org.netxms.websvc.json.ResponseContainer;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Persistent storage request handler
 */
public class PersistentStorage extends AbstractObjectHandler
{
   /*
    * @see org.netxms.websvc.handlers.AbstractHandler#getCollection(org.json.JSONObject)
    */
   @Override
   protected Object getCollection(Map<String, String> query) throws Exception
   {
      return new ResponseContainer("persistentstorage", getSession().getPersistentStorageList());
   }

   /*
    * @see org.netxms.websvc.handlers.AbstractHandler#get(java.lang.String)
    */
   @Override
   protected Object get(String id, Map<String, String> query) throws Exception
   {
      return new ResponseContainer("value", getSession().getPersistentStorageList().get(id));
   }

   /*
    * @see org.netxms.websvc.handlers.AbstractHandler#create(org.json.JSONObject)
    */
   @Override
   protected Object create(JSONObject data) throws Exception
   {
      String key = JsonTools.getStringFromJson(data, "key", null);
      String value = JsonTools.getStringFromJson(data, "value", null);
      getSession().setPersistentStorageValue(key, value);
      return null;
   }

   /*
    * @see org.netxms.websvc.handlers.AbstractHandler#update(java.lang.String, org.json.JSONObject)
    */
   @Override
   protected Object update(String id, JSONObject data) throws Exception
   {
      String value = JsonTools.getStringFromJson(data, "value", null);
      getSession().setPersistentStorageValue(id, value);
      return null;
   }

   /*
    * @see org.netxms.websvc.handlers.AbstractHandler#delete(java.lang.String)
    */
   @Override
   protected Object delete(String id) throws Exception
   {
      getSession().deletePersistentStorageValue(id);
      return null;
   }
}
