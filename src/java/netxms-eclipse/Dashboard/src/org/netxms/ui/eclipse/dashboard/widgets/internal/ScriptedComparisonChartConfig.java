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
package org.netxms.ui.eclipse.dashboard.widgets.internal;

import org.simpleframework.xml.Element;

/**
 * Common base class for scripted comparison chart configurations
 */
public abstract class ScriptedComparisonChartConfig extends AbstractChartConfig
{
   @Element(required = false)
   private String script = "";

   @Element(required = false)
   private long objectId = 0;

	/**
    * @return the script
    */
   public String getScript()
   {
      return script;
   }

   /**
    * @param script the script to set
    */
   public void setScript(String script)
   {
      this.script = script;
   }

   /**
    * @return the objectId
    */
   public long getObjectId()
   {
      return objectId;
   }

   /**
    * @param objectId the objectId to set
    */
   public void setObjectId(long objectId)
   {
      this.objectId = objectId;
   }
}
