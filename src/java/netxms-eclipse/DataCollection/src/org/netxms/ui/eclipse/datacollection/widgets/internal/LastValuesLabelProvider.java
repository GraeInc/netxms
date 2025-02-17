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
package org.netxms.ui.eclipse.datacollection.widgets.internal;

import org.eclipse.jface.viewers.ITableColorProvider;
import org.eclipse.jface.viewers.ITableLabelProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Image;
import org.netxms.client.constants.ObjectStatus;
import org.netxms.client.constants.Severity;
import org.netxms.client.datacollection.DataCollectionObject;
import org.netxms.client.datacollection.DciValue;
import org.netxms.client.datacollection.Threshold;
import org.netxms.ui.eclipse.console.resources.RegionalSettings;
import org.netxms.ui.eclipse.console.resources.StatusDisplayInfo;
import org.netxms.ui.eclipse.datacollection.Activator;
import org.netxms.ui.eclipse.datacollection.Messages;
import org.netxms.ui.eclipse.datacollection.ThresholdLabelProvider;
import org.netxms.ui.eclipse.datacollection.propertypages.Thresholds;
import org.netxms.ui.eclipse.datacollection.widgets.LastValuesWidget;

/**
 * Label provider for last values view
 */
public class LastValuesLabelProvider extends LabelProvider implements ITableLabelProvider, ITableColorProvider
{
	private Image[] stateImages = new Image[3];
	private boolean useMultipliers = true;
	private boolean showErrors = true;
	private ThresholdLabelProvider thresholdLabelProvider;

	/**
	 * Default constructor 
	 */
	public LastValuesLabelProvider()
	{
		super();

		stateImages[0] = Activator.getImageDescriptor("icons/active.gif").createImage(); //$NON-NLS-1$
		stateImages[1] = Activator.getImageDescriptor("icons/disabled.gif").createImage(); //$NON-NLS-1$
		stateImages[2] = Activator.getImageDescriptor("icons/unsupported.gif").createImage(); //$NON-NLS-1$

		thresholdLabelProvider = new ThresholdLabelProvider();
	}

   /**
    * @see org.eclipse.jface.viewers.ITableLabelProvider#getColumnImage(java.lang.Object, int)
    */
	@Override
	public Image getColumnImage(Object element, int columnIndex)
	{
		switch(columnIndex)
		{
			case LastValuesWidget.COLUMN_ID:
				return stateImages[((DciValue)element).getStatus()];
			case LastValuesWidget.COLUMN_THRESHOLD:
				Threshold threshold = ((DciValue)element).getActiveThreshold();
				return (threshold != null) ? thresholdLabelProvider.getColumnImage(threshold, Thresholds.COLUMN_EVENT) : StatusDisplayInfo.getStatusImage(Severity.NORMAL);
		}
		return null;
	}

   /**
    * @see org.eclipse.jface.viewers.ITableLabelProvider#getColumnText(java.lang.Object, int)
    */
	@Override
	public String getColumnText(Object element, int columnIndex)
	{
	   DciValue dciValue = ((DciValue)element);
		switch(columnIndex)
		{
			case LastValuesWidget.COLUMN_ID:
				return Long.toString(dciValue.getId());
			case LastValuesWidget.COLUMN_DESCRIPTION:
				return dciValue.getDescription();
			case LastValuesWidget.COLUMN_VALUE:
				if (showErrors && dciValue.getErrorCount() > 0)
					return Messages.get().LastValuesLabelProvider_Error;
				if (dciValue.getDcObjectType() == DataCollectionObject.DCO_TYPE_TABLE)
					return Messages.get().LastValuesLabelProvider_Table;		
				return dciValue.getFormattedValue(useMultipliers, RegionalSettings.TIME_FORMATTER);
			case LastValuesWidget.COLUMN_TIMESTAMP:
				if (dciValue.getTimestamp().getTime() <= 1000)
					return null;
				return RegionalSettings.getDateTimeFormat().format(dciValue.getTimestamp());
			case LastValuesWidget.COLUMN_THRESHOLD:
            return formatThreshold(dciValue);
         case LastValuesWidget.COLUMN_EVENT:
            return getEventName(dciValue);
		}
		return null;
	}

	/**
    * Format threshold
    * 
    * @param value DCI value to format threshold for
    * @return formatted threshold
    */
   private String formatThreshold(DciValue value)
	{
      Threshold threshold = value.getActiveThreshold();
      if (threshold == null)
         return Messages.get().LastValuesLabelProvider_OK;
      if (value.getDcObjectType() == DataCollectionObject.DCO_TYPE_TABLE)
         return threshold.getValue(); // For table DCIs server sends pre-formatted condition in "value" field
      return thresholdLabelProvider.getColumnText(threshold, Thresholds.COLUMN_OPERATION);
	}

   /**
    * Get threshold activation event name.
    *
    * @param value DCI value
    * @return threshold activation event name or empty string
    */
   public String getEventName(DciValue value)
   {
      Threshold threshold = value.getActiveThreshold();
      if (threshold == null)
         return "";
      return thresholdLabelProvider.getColumnText(threshold, Thresholds.COLUMN_EVENT);
   }

   /**
    * @see org.eclipse.jface.viewers.IBaseLabelProvider#dispose()
    */
	@Override
	public void dispose()
	{
		for(int i = 0; i < stateImages.length; i++)
			stateImages[i].dispose();
		thresholdLabelProvider.dispose();
		super.dispose();
	}

	/**
	 * @return the useMultipliers
	 */
	public boolean areMultipliersUsed()
	{
		return useMultipliers;
	}

	/**
	 * @param useMultipliers the useMultipliers to set
	 */
	public void setUseMultipliers(boolean useMultipliers)
	{
		this.useMultipliers = useMultipliers;
	}

   /**
    * @see org.eclipse.jface.viewers.ITableColorProvider#getForeground(java.lang.Object, int)
    */
	@Override
	public Color getForeground(Object element, int columnIndex)
	{
		if (((DciValue)element).getStatus() == DataCollectionObject.DISABLED)
			return StatusDisplayInfo.getStatusColor(ObjectStatus.UNMANAGED);
		if (showErrors && ((DciValue)element).getErrorCount() > 0)
			return StatusDisplayInfo.getStatusColor(ObjectStatus.CRITICAL);
		return null;
	}

   /**
    * @see org.eclipse.jface.viewers.ITableColorProvider#getBackground(java.lang.Object, int)
    */
	@Override
	public Color getBackground(Object element, int columnIndex)
	{
		return null;
	}

	/**
	 * @return the showErrors
	 */
	public boolean isShowErrors()
	{
		return showErrors;
	}

	/**
	 * @param showErrors the showErrors to set
	 */
	public void setShowErrors(boolean showErrors)
	{
		this.showErrors = showErrors;
	}
}
