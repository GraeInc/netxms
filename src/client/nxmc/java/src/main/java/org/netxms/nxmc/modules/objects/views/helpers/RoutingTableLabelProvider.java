/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2014 Victor Kirhenshtein
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
package org.netxms.nxmc.modules.objects.views.helpers;

import org.eclipse.jface.viewers.ITableLabelProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.swt.graphics.Image;
import org.netxms.client.topology.Route;
import org.netxms.nxmc.modules.objects.views.RoutingTableView;

/**
 * Label provider for routing table view
 */
public class RoutingTableLabelProvider extends LabelProvider implements ITableLabelProvider
{
   /* (non-Javadoc)
    * @see org.eclipse.jface.viewers.ITableLabelProvider#getColumnImage(java.lang.Object, int)
    */
   @Override
   public Image getColumnImage(Object element, int columnIndex)
   {
      return null;
   }

   /* (non-Javadoc)
    * @see org.eclipse.jface.viewers.ITableLabelProvider#getColumnText(java.lang.Object, int)
    */
   @Override
   public String getColumnText(Object element, int columnIndex)
   {
      Route r = (Route)element;
      switch(columnIndex)
      {
         case RoutingTableView.COLUMN_DESTINATION:
            return r.getDestination().getHostAddress() + "/" + r.getPrefixLength(); //$NON-NLS-1$
         case RoutingTableView.COLUMN_INTERFACE:
            return r.getIfName();
         case RoutingTableView.COLUMN_NEXT_HOP:
            return r.getNextHop().getHostAddress();
         case RoutingTableView.COLUMN_TYPE:
            return Integer.toString(r.getType());
      }
      return null;
   }
}
