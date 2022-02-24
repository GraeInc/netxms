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
package org.netxms.ui.eclipse.dashboard.propertypages;

import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Group;
import org.eclipse.ui.dialogs.PropertyPage;
import org.netxms.client.objects.AbstractObject;
import org.netxms.ui.eclipse.console.resources.StatusDisplayInfo;
import org.netxms.ui.eclipse.dashboard.Messages;
import org.netxms.ui.eclipse.dashboard.widgets.TitleConfigurator;
import org.netxms.ui.eclipse.dashboard.widgets.internal.StatusMapConfig;
import org.netxms.ui.eclipse.objectbrowser.widgets.ObjectSelector;

/**
 * Status map element properties
 */
public class StatusMap extends PropertyPage
{
	private StatusMapConfig config;
	private ObjectSelector objectSelector;
   private TitleConfigurator title;
	private Button[] checkSeverity;
   private Button checkGroupObjects;
   private Button checkHideObjectsInMaintenance;
	private Button checkShowFilter;
   private Button checkRadial;
   private Button checkFitToScreen;

   /**
    * @see org.eclipse.jface.preference.PreferencePage#createContents(org.eclipse.swt.widgets.Composite)
    */
	@Override
	protected Control createContents(Composite parent)
	{
		config = (StatusMapConfig)getElement().getAdapter(StatusMapConfig.class);
		
		Composite dialogArea = new Composite(parent, SWT.NONE);
		
		GridLayout layout = new GridLayout();
		dialogArea.setLayout(layout);
		
      title = new TitleConfigurator(dialogArea, config);
      GridData gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      title.setLayoutData(gd);

		objectSelector = new ObjectSelector(dialogArea, SWT.NONE, true);
		objectSelector.setLabel(Messages.get().AlarmViewer_RootObject);
		objectSelector.setObjectClass(AbstractObject.class);
		objectSelector.setObjectId(config.getObjectId());
      gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		objectSelector.setLayoutData(gd);
		
		Group severityGroup = new Group(dialogArea, SWT.NONE);
		severityGroup.setText(Messages.get().StatusMap_SeverityFilter);
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		severityGroup.setLayoutData(gd);
		
		layout = new GridLayout();
		layout.numColumns = 4;
		layout.makeColumnsEqualWidth = true;
		severityGroup.setLayout(layout);
		
		checkSeverity = new Button[7];
		for(int severity = 6; severity >= 0; severity--)
		{
			checkSeverity[severity] = new Button(severityGroup, SWT.CHECK);
			checkSeverity[severity].setText(StatusDisplayInfo.getStatusText(severity));
			checkSeverity[severity].setSelection((config.getSeverityFilter() & (1 << severity)) != 0);
		}

      Group optionsGroup = new Group(dialogArea, SWT.NONE);
      optionsGroup.setText(Messages.get().StatusMap_Options);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      optionsGroup.setLayoutData(gd);
      
      layout = new GridLayout();
      layout.numColumns = 1;
      layout.makeColumnsEqualWidth = true;
      optionsGroup.setLayout(layout);
		
      checkGroupObjects = new Button(optionsGroup, SWT.CHECK);
      checkGroupObjects.setText(Messages.get().StatusMap_Group);
      checkGroupObjects.setSelection(config.isGroupObjects());

      checkHideObjectsInMaintenance = new Button(optionsGroup, SWT.CHECK);
      checkHideObjectsInMaintenance.setText("Hide objects in &maintenance mode");
      checkHideObjectsInMaintenance.setSelection(config.isHideObjectsInMaintenance());

		checkShowFilter = new Button(optionsGroup, SWT.CHECK);
		checkShowFilter.setText(Messages.get().StatusMap_ShowFilter);
		checkShowFilter.setSelection(config.isShowTextFilter());
		
		checkRadial = new Button(optionsGroup, SWT.CHECK);
		checkRadial.setText("Show in radial form");
		checkRadial.setSelection(config.isShowRadial());
      
      checkFitToScreen = new Button(optionsGroup, SWT.CHECK);
      checkFitToScreen.setText("Fit to screen");
      checkFitToScreen.setSelection(config.isFitToScreen());

		return dialogArea;
	}

   /**
    * @see org.eclipse.jface.preference.PreferencePage#performOk()
    */
	@Override
	public boolean performOk()
	{
      title.updateConfiguration(config);
		config.setObjectId(objectSelector.getObjectId());

		int severityFilter = 0;
		for(int i = 0; i < checkSeverity.length; i++)
			if (checkSeverity[i].getSelection())
				severityFilter |= (1 << i);
		config.setSeverityFilter(severityFilter);
		config.setGroupObjects(checkGroupObjects.getSelection());
      config.setHideObjectsInMaintenance(checkHideObjectsInMaintenance.getSelection());
		config.setShowTextFilter(checkShowFilter.getSelection());
      config.setShowRadial(checkRadial.getSelection());
      config.setFitToScreen(checkFitToScreen.getSelection());

		return true;
	}
}
