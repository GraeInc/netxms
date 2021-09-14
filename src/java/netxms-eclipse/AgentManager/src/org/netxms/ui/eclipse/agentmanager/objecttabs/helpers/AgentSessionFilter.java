/**
 * NetXMS - open source network management system
 * Copyright (C) 2019-2021 Raden Solutions
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
package org.netxms.ui.eclipse.agentmanager.objecttabs.helpers;

import org.eclipse.jface.viewers.Viewer;
import org.eclipse.jface.viewers.ViewerFilter;
import org.netxms.client.TableRow;

/**
 * Agent session filter
 */
public class AgentSessionFilter extends ViewerFilter
{
   private String filterString = null;

   /**
    * @see org.eclipse.jface.viewers.ViewerFilter#select(org.eclipse.jface.viewers.Viewer, java.lang.Object, java.lang.Object)
    */
   @Override
   public boolean select(Viewer viewer, Object parentElement, Object element)
   {
      TableRow row = (TableRow)element;

      if ((filterString == null) || (filterString.isEmpty()))
         return true;

      for(int i = 0; i < row.size(); i++)
      {
         if (row.get(i).getValue().toLowerCase().contains(filterString))
            return true;
      }

      return false;
   }

   /**
    * Set filtering text
    * 
    * @param text test to filter
    */
   public void setFilterString(String text)
   {
      filterString = text;
   }
}
