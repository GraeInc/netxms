/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2017 Raden Solutions
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
package org.netxms.ui.eclipse.datacollection.propertypages;

import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.netxms.client.constants.AgentCacheMode;
import org.netxms.client.datacollection.DataCollectionItem;
import org.netxms.client.objects.GenericObject;
import org.netxms.ui.eclipse.datacollection.Messages;
import org.netxms.ui.eclipse.datacollection.propertypages.helpers.AbstractDCIPropertyPage;
import org.netxms.ui.eclipse.objectbrowser.widgets.ObjectSelector;
import org.netxms.ui.eclipse.tools.WidgetHelper;

/**
 * @author Victor
 *
 */
public class OtherOptions extends AbstractDCIPropertyPage
{
	private DataCollectionItem dci;
	private Button checkShowOnTooltip;
	private Button checkShowInOverview;
   private Button checkCalculateStatus;
   private Button checkHideOnLastValues;
   private Combo multiplierDegree;   
   private Combo agentCacheMode;
   private ObjectSelector relatedObject;

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#createContents(org.eclipse.swt.widgets.Composite)
	 */
	@Override
	protected Control createContents(Composite parent)
	{
	   Composite dialogArea = (Composite)super.createContents(parent);
		dci = editor.getObjectAsItem();
		
		GridLayout layout = new GridLayout();
		layout.verticalSpacing = WidgetHelper.OUTER_SPACING;
		layout.marginWidth = 0;
		layout.marginHeight = 0;
      dialogArea.setLayout(layout);

      checkShowOnTooltip = new Button(dialogArea, SWT.CHECK);
      checkShowOnTooltip.setText(Messages.get().NetworkMaps_ShowInTooltips);
      checkShowOnTooltip.setSelection(dci.isShowOnObjectTooltip());

      checkShowInOverview = new Button(dialogArea, SWT.CHECK);
      checkShowInOverview.setText(Messages.get().OtherOptions_ShowlastValueInObjectOverview);
      checkShowInOverview.setSelection(dci.isShowInObjectOverview());

      checkCalculateStatus = new Button(dialogArea, SWT.CHECK);
      checkCalculateStatus.setText(Messages.get().OtherOptions_UseForStatusCalculation);
      checkCalculateStatus.setSelection(dci.isUsedForNodeStatusCalculation());

      checkHideOnLastValues = new Button(dialogArea, SWT.CHECK);
      checkHideOnLastValues.setText("Hide value on \"Last Values\" page");
      checkHideOnLastValues.setSelection(dci.isHideOnLastValuesView());      
      
      agentCacheMode = WidgetHelper.createLabeledCombo(dialogArea, SWT.READ_ONLY, Messages.get().General_AgentCacheMode, new GridData());
      agentCacheMode.add(Messages.get().General_Default);
      agentCacheMode.add(Messages.get().General_On);
      agentCacheMode.add(Messages.get().General_Off);
      agentCacheMode.select(dci.getCacheMode().getValue());
      
      multiplierDegree = WidgetHelper.createLabeledCombo(dialogArea, SWT.READ_ONLY, "Multiplier degree", new GridData());
      multiplierDegree.add("Fixed to P"); 
      multiplierDegree.add("Fixed to T"); 
      multiplierDegree.add("Fixed to G"); 
      multiplierDegree.add("Fixed to M"); 
      multiplierDegree.add("Fixed to K"); 
      multiplierDegree.add("Default"); 
      multiplierDegree.add("Fixed to m"); 
      multiplierDegree.add("Fixed to μ"); 
      multiplierDegree.add("Fixed to n"); 
      multiplierDegree.add("Fixed to p"); 
      multiplierDegree.add("Fixed to f"); 
      multiplierDegree.select(5 - dci.getMultiplier());

      relatedObject = new ObjectSelector(dialogArea, SWT.NONE, true);
      relatedObject.setLabel("Related object");
      relatedObject.setObjectClass(GenericObject.class);
      relatedObject.setObjectId(dci.getRelatedObject());
      GridData gd = new GridData();
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalAlignment = SWT.FILL;
      relatedObject.setLayoutData(gd);
      
		return dialogArea;
	}

	/**
	 * Apply changes
	 * 
	 * @param isApply true if update operation caused by "Apply" button
	 */
	protected void applyChanges(final boolean isApply)
	{
		dci.setShowOnObjectTooltip(checkShowOnTooltip.getSelection());
		dci.setShowInObjectOverview(checkShowInOverview.getSelection());
      dci.setUsedForNodeStatusCalculation(checkCalculateStatus.getSelection());
      dci.setHideOnLastValuesView(checkHideOnLastValues.getSelection());
      dci.setCacheMode(AgentCacheMode.getByValue(agentCacheMode.getSelectionIndex()));
      dci.setMultiplier(5 - multiplierDegree.getSelectionIndex());
      dci.setRelatedObject(relatedObject.getObjectId());
		editor.modify();
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performOk()
	 */
	@Override
	public boolean performOk()
	{
		applyChanges(false);
		return true;
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performApply()
	 */
	@Override
	protected void performApply()
	{
		applyChanges(true);
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performDefaults()
	 */
	@Override
	protected void performDefaults()
	{
		super.performDefaults();
		checkShowOnTooltip.setSelection(false);
		checkShowInOverview.setSelection(false);
		checkCalculateStatus.setSelection(false);
		checkHideOnLastValues.setSelection(false);
		agentCacheMode.select(0);
		multiplierDegree.select(0);
		relatedObject.setObjectId(0);
	}
}
