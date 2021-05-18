/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2011 Victor Kirhenshtein
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

import java.io.StringWriter;
import java.io.Writer;
import java.util.Map;
import java.util.Set;
import org.netxms.client.xml.XMLTools;
import org.netxms.ui.eclipse.dashboard.dialogs.helpers.ObjectIdMatchingData;
import org.simpleframework.xml.Element;
import org.simpleframework.xml.Serializer;

/**
 * Configuration for alarm viewer widget
 */
public class AlarmViewerConfig extends DashboardElementConfig
{
   @Element(required = true)
	private long objectId = 0;

   @Element(required = false)
	private String title = ""; //$NON-NLS-1$
	
   @Element(required = false)
	private int severityFilter = 0xFF;

   @Element(required = false)
   private int stateFilter = 0xFF;

   @Element(required = false)
   private boolean isLocalSoundEnabled = false;

	/**
	 * Create line chart settings object from XML document
	 * 
	 * @param xml XML document
	 * @return deserialized object
	 * @throws Exception if the object cannot be fully deserialized
	 */
	public static AlarmViewerConfig createFromXml(final String xml) throws Exception
	{
		Serializer serializer = XMLTools.createSerializer();
		return serializer.read(AlarmViewerConfig.class, xml);
	}
	
   /**
    * @see org.netxms.ui.eclipse.dashboard.widgets.internal.DashboardElementConfig#createXml()
    */
	@Override
	public String createXml() throws Exception
	{
		Serializer serializer = XMLTools.createSerializer();
		Writer writer = new StringWriter();
		serializer.write(this, writer);
		return writer.toString();
	}

   /**
    * @see org.netxms.ui.eclipse.dashboard.widgets.internal.DashboardElementConfig#getObjects()
    */
	@Override
	public Set<Long> getObjects()
	{
		Set<Long> objects = super.getObjects();
		objects.add(objectId);
		return objects;
	}

   /**
    * @see org.netxms.ui.eclipse.dashboard.widgets.internal.DashboardElementConfig#remapObjects(java.util.Map)
    */
	@Override
	public void remapObjects(Map<Long, ObjectIdMatchingData> remapData)
	{
		super.remapObjects(remapData);
		ObjectIdMatchingData md = remapData.get(objectId);
		if (md != null)
			objectId = md.dstId;
	}

	/**
	 * @return the title
	 */
	public String getTitle()
	{
		return title;
	}

	/**
	 * @param title the title to set
	 */
	public void setTitle(String title)
	{
		this.title = title;
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

	/**
	 * @return the severityFilter
	 */
	public int getSeverityFilter()
	{
		return severityFilter;
	}

	/**
	 * @param severityFilter the severityFilter to set
	 */
	public void setSeverityFilter(int severityFilter)
	{
		this.severityFilter = severityFilter;
	}

   /**
    * Get alarm state filter.
    *
    * @return alarm state filter
    */
   public int getStateFilter()
   {
      return stateFilter;
   }

   /**
    * Set alarm state filter.
    *
    * @param stateFilter new alarm state filter
    */
   public void setStateFilter(int stateFilter)
   {
      this.stateFilter = stateFilter;
   }

   public boolean getIsLocalSoundEnabled()
   {
      return isLocalSoundEnabled;
   }

   public void setIsLocalSoundEnabled(boolean isLocalSoundEnabled)
   {
      this.isLocalSoundEnabled = isLocalSoundEnabled;
   }
}
