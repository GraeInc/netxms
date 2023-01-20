/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2013 Victor Kirhenshtein
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
package org.netxms.ui.eclipse.topology.views.helpers;

import org.eclipse.jface.viewers.ITableLabelProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.swt.graphics.Image;
import org.netxms.client.NXCSession;
import org.netxms.client.topology.RadioInterface;
import org.netxms.ui.eclipse.console.ViewerElementUpdater;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.topology.views.RadioInterfaces;

/**
 * Label provider for radio interface list
 */
public class RadioInterfaceLabelProvider extends LabelProvider implements ITableLabelProvider
{
   private TableViewer viewer;
   private NXCSession session = ConsoleSharedData.getSession();
   
   public RadioInterfaceLabelProvider(TableViewer viewer)
   {
      this.viewer = viewer;
   }
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
		RadioInterface rif = (RadioInterface)element;
		switch(columnIndex)
		{
			case RadioInterfaces.COLUMN_AP_MAC_ADDR:
            String apVendor = session.getVendorByMac(rif.getAccessPoint().getMacAddress(), new ViewerElementUpdater(viewer, element));
            return apVendor != null && !apVendor.isEmpty() ? String.format("%s (%s)",rif.getAccessPoint().getMacAddress().toString(), apVendor) : rif.getAccessPoint().getMacAddress().toString();
			case RadioInterfaces.COLUMN_AP_MODEL:
				return rif.getAccessPoint().getModel();
			case RadioInterfaces.COLUMN_AP_NAME:
				return rif.getAccessPoint().getObjectName();
			case RadioInterfaces.COLUMN_AP_SERIAL:
				return rif.getAccessPoint().getSerialNumber();
			case RadioInterfaces.COLUMN_AP_VENDOR:
				return rif.getAccessPoint().getVendor();
			case RadioInterfaces.COLUMN_CHANNEL:
				return Integer.toString(rif.getChannel());
			case RadioInterfaces.COLUMN_INDEX:
				return Integer.toString(rif.getIndex());
			case RadioInterfaces.COLUMN_MAC_ADDR:
            return rif.getMacAddress().toString();
         case RadioInterfaces.COLUMN_NIC_VENDOR:
            return session.getVendorByMac(rif.getMacAddress(), new ViewerElementUpdater(viewer, element));
			case RadioInterfaces.COLUMN_NAME:
				return rif.getName();
			case RadioInterfaces.COLUMN_TX_POWER_DBM:
				return Integer.toString(rif.getPowerDBm());
			case RadioInterfaces.COLUMN_TX_POWER_MW:
				return Integer.toString(rif.getPowerMW());
		}
		return null;
	}
}
