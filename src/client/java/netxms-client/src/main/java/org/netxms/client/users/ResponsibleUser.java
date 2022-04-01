/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2022 Victor Kirhenshtein
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
package org.netxms.client.users;

import org.netxms.base.NXCPMessage;

/**
 * Responsible user
 */
public class ResponsibleUser
{
   public long userId;
   public String tag;

   /**
    * Create new responsible user object.
    *
    * @param userId user ID
    * @param tag user tag
    */
   public ResponsibleUser(long userId, String tag)
   {
      this.userId = userId;
      this.tag = tag;
   }

   /**
    * Create from NXCP message.
    *
    * @param msg NXCP message
    * @param baseId base field ID
    */
   public ResponsibleUser(NXCPMessage msg, long baseId)
   {
      userId = msg.getFieldAsInt64(baseId);
      tag = msg.getFieldAsString(baseId + 1);
   }

   /**
    * Fill NXCP message.
    * 
    * @param msg NXCP message
    * @param baseId base field ID
    */
   public void fillMessage(NXCPMessage msg, long baseId)
   {
      msg.setFieldInt32(baseId, (int)userId);
      msg.setField(baseId + 1, tag);
   }
}
