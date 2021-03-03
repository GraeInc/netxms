/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2021 Victor Kirhenshtein
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
package org.netxms.ui.eclipse.reporter.widgets.helpers;

import java.text.DateFormat;
import org.eclipse.jface.viewers.ITableLabelProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.swt.graphics.Image;
import org.netxms.client.NXCSession;
import org.netxms.client.reporting.ReportResult;
import org.netxms.client.users.AbstractUserObject;
import org.netxms.ui.eclipse.console.UserRefreshRunnable;
import org.netxms.ui.eclipse.console.resources.RegionalSettings;
import org.netxms.ui.eclipse.reporter.Messages;
import org.netxms.ui.eclipse.reporter.widgets.ReportExecutionForm;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.widgets.SortableTableViewer;

/**
 * Label provider for report result list
 */
public class ReportResultLabelProvider extends LabelProvider implements ITableLabelProvider
{
   private NXCSession session = ConsoleSharedData.getSession();
	private DateFormat dateFormat = RegionalSettings.getDateTimeFormat();
	private SortableTableViewer viewer;
	
	/**
	 * Constructor
	 */
	public ReportResultLabelProvider(SortableTableViewer viewer)
	{
	   this.viewer = viewer;
	}

   /**
    * @see org.eclipse.jface.viewers.ITableLabelProvider#getColumnImage(java.lang.Object, int)
    */
	@Override
	public Image getColumnImage(Object element, int columnIndex)
	{
		return null;
	}

   /**
    * @see org.eclipse.jface.viewers.ITableLabelProvider#getColumnText(java.lang.Object, int)
    */
	@Override
	public String getColumnText(Object element, int columnIndex)
	{
		final ReportResult reportResult = (ReportResult)element;
		switch(columnIndex)
		{
			case ReportExecutionForm.COLUMN_RESULT_EXEC_TIME:
				return dateFormat.format(reportResult.getExecutionTime());
			case ReportExecutionForm.COLUMN_RESULT_STARTED_BY:
            AbstractUserObject user = session.findUserDBObjectById(reportResult.getUserId(), new UserRefreshRunnable(viewer, element));
            return (user != null) ? user.getName() : ("[" + reportResult.getUserId() + "]"); //$NON-NLS-1$ //$NON-NLS-2$
			case ReportExecutionForm.COLUMN_RESULT_STATUS:
            return reportResult.isSuccess() ? Messages.get().ReportResultLabelProvider_Success
                  : Messages.get().ReportResultLabelProvider_Failure;
		}
		return "<INTERNAL ERROR>"; //$NON-NLS-1$
	}
}
