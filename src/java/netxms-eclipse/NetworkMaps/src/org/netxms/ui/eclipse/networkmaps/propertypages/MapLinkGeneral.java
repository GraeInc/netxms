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
package org.netxms.ui.eclipse.networkmaps.propertypages;

import org.eclipse.jface.preference.ColorSelector;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Group;
import org.eclipse.swt.widgets.List;
import org.eclipse.ui.dialogs.PropertyPage;
import org.netxms.client.NXCSession;
import org.netxms.client.maps.NetworkMapLink;
import org.netxms.client.objects.AbstractObject;
import org.netxms.ui.eclipse.networkmaps.Messages;
import org.netxms.ui.eclipse.networkmaps.views.helpers.LinkEditor;
import org.netxms.ui.eclipse.nxsl.widgets.ScriptSelector;
import org.netxms.ui.eclipse.objectbrowser.dialogs.ObjectSelectionDialog;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.tools.ColorConverter;
import org.netxms.ui.eclipse.tools.WidgetHelper;
import org.netxms.ui.eclipse.widgets.LabeledText;

/**
 * "General" property page for map link
 */
public class MapLinkGeneral extends PropertyPage
{
	private LinkEditor object;
	private LabeledText name;
	private LabeledText connector1;
	private LabeledText connector2;
	private Button radioColorDefault;
	private Button radioColorObject;
   private Button radioColorScript;
	private Button radioColorCustom;
   private ScriptSelector script;
	private ColorSelector color;
	private List list;
	private Button add;
	private Button remove;
	private Combo routingAlgorithm;
	private Button checkUseThresholds;
	
   /**
    * @see org.eclipse.jface.preference.PreferencePage#createContents(org.eclipse.swt.widgets.Composite)
    */
	@Override
	protected Control createContents(Composite parent)
	{
		object = (LinkEditor)getElement().getAdapter(LinkEditor.class);

		Composite dialogArea = new Composite(parent, SWT.NONE);
		
		GridLayout layout = new GridLayout();
		layout.numColumns = 2;
		dialogArea.setLayout(layout);
		
		name = new LabeledText(dialogArea, SWT.NONE);
		name.setLabel(Messages.get().MapLinkGeneral_Name);
		name.setText(object.getName());
		GridData gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		gd.horizontalSpan = 2;
		name.setLayoutData(gd);
		
		connector1 = new LabeledText(dialogArea, SWT.NONE);
		connector1.setLabel(Messages.get().MapLinkGeneral_NameConn1);
		connector1.setText(object.getConnectorName1());
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		gd.horizontalSpan = 2;
		connector1.setLayoutData(gd);
		
		connector2 = new LabeledText(dialogArea, SWT.NONE);
		connector2.setLabel(Messages.get().MapLinkGeneral_NameConn2);
		connector2.setText(object.getConnectorName2());
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		gd.horizontalSpan = 2;
		connector2.setLayoutData(gd);
		
		final Group colorGroup = new Group(dialogArea, SWT.NONE);
		colorGroup.setText(Messages.get().MapLinkGeneral_Color);
		layout = new GridLayout();
		colorGroup.setLayout(layout);
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		gd.horizontalSpan = 2;
		colorGroup.setLayoutData(gd);
		
		final SelectionListener listener = new SelectionListener() {
			@Override
			public void widgetSelected(SelectionEvent e)
			{
				color.setEnabled(radioColorCustom.getSelection());
            script.setEnabled(radioColorScript.getSelection());
				list.setEnabled(radioColorObject.getSelection()); 
				add.setEnabled(radioColorObject.getSelection());
				remove.setEnabled(radioColorObject.getSelection());
				checkUseThresholds.setEnabled(radioColorObject.getSelection());
			}

			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}
		};

		radioColorDefault = new Button(colorGroup, SWT.RADIO);
		radioColorDefault.setText(Messages.get().MapLinkGeneral_DefColor);
      radioColorDefault.setSelection(object.getColorSource() == NetworkMapLink.COLOR_SOURCE_DEFAULT);
		radioColorDefault.addSelectionListener(listener);

		radioColorObject = new Button(colorGroup, SWT.RADIO);
		radioColorObject.setText(Messages.get().MapLinkGeneral_BasedOnObjStatus);
      radioColorObject.setSelection(object.getColorSource() == NetworkMapLink.COLOR_SOURCE_OBJECT_STATUS);
		radioColorObject.addSelectionListener(listener);

		final Composite nodeSelectionGroup = new Composite(colorGroup, SWT.NONE);
      layout = new GridLayout();
      layout.numColumns = 2;
      layout.marginHeight = 0;
      layout.marginWidth = 0;
      nodeSelectionGroup.setLayout(layout);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalSpan = 2;
      nodeSelectionGroup.setLayoutData(gd);
		
		list = new List(nodeSelectionGroup, SWT.BORDER | SWT.SINGLE | SWT.V_SCROLL | SWT.H_SCROLL );
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.verticalSpan = 2;
      gd.verticalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalIndent = 20;
      list.setLayoutData(gd);
      if (object.getStatusObjects() != null)
		{
         NXCSession session = ConsoleSharedData.getSession();
         for(Long id : object.getStatusObjects())
		   {
            list.add(session.getObjectName(id));
		   }
		}
      list.setEnabled(radioColorObject.getSelection());

      add = new Button(nodeSelectionGroup, SWT.PUSH);
      add.setText(Messages.get().MapLinkGeneral_Add);
      gd = new GridData();
      gd.widthHint = WidgetHelper.BUTTON_WIDTH_HINT;
      gd.verticalAlignment = SWT.TOP;
      add.setLayoutData(gd);
      add.addSelectionListener(new SelectionListener() {
         @Override
         public void widgetDefaultSelected(SelectionEvent e)
         {
            widgetSelected(e);
         }

         @Override
         public void widgetSelected(SelectionEvent e)
         {
            addObject();
         }
      });
      add.setEnabled(radioColorObject.getSelection());

      remove = new Button(nodeSelectionGroup, SWT.PUSH);
      remove.setText(Messages.get().MapLinkGeneral_Delete);
      gd.widthHint = WidgetHelper.BUTTON_WIDTH_HINT;
      gd.verticalAlignment = SWT.TOP;
      remove.setLayoutData(gd);
      remove.addSelectionListener(new SelectionListener() {
         @Override
         public void widgetDefaultSelected(SelectionEvent e)
         {
            widgetSelected(e);
         }

         @Override
         public void widgetSelected(SelectionEvent e)
         {
            removeObject();
         }
      });
      remove.setEnabled(radioColorObject.getSelection());
      
      checkUseThresholds = new Button(nodeSelectionGroup, SWT.CHECK);
      checkUseThresholds.setText("Include active thresholds into calculation");
      checkUseThresholds.setEnabled(radioColorObject.getSelection());
      checkUseThresholds.setSelection(object.isUseActiveThresholds());
      gd = new GridData();
      gd.horizontalIndent = 17;
      checkUseThresholds.setLayoutData(gd);

      radioColorScript = new Button(colorGroup, SWT.RADIO);
      radioColorScript.setText("Script");
      radioColorScript.setSelection(object.getColorSource() == NetworkMapLink.COLOR_SOURCE_SCRIPT);
      radioColorScript.addSelectionListener(listener);

      script = new ScriptSelector(colorGroup, SWT.NONE, false, false);
      script.setScriptName(object.getColorProvider());
      if (radioColorScript.getSelection())
         script.setScriptName(object.getColorProvider());
      else
         script.setEnabled(false);
      gd = new GridData();
      gd.horizontalIndent = 20;
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalSpan = 2;
      script.setLayoutData(gd);

		radioColorCustom = new Button(colorGroup, SWT.RADIO);
		radioColorCustom.setText(Messages.get().MapLinkGeneral_CustomColor);
      radioColorCustom.setSelection(object.getColorSource() == NetworkMapLink.COLOR_SOURCE_CUSTOM_COLOR);
		radioColorCustom.addSelectionListener(listener);

		color = new ColorSelector(colorGroup);
		if (radioColorCustom.getSelection())
			color.setColorValue(ColorConverter.rgbFromInt(object.getColor()));
		else
			color.setEnabled(false);
		gd = new GridData();
		gd.horizontalIndent = 20;
		color.getButton().setLayoutData(gd);

		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		routingAlgorithm = WidgetHelper.createLabeledCombo(dialogArea, SWT.READ_ONLY, Messages.get().MapLinkGeneral_RoutingAlg, gd);
		routingAlgorithm.add(Messages.get().MapLinkGeneral_MapDefault);
		routingAlgorithm.add(Messages.get().MapLinkGeneral_Direct);
		routingAlgorithm.add(Messages.get().MapLinkGeneral_Manhattan);
		routingAlgorithm.add(Messages.get().MapLinkGeneral_BendPoints);
		routingAlgorithm.select(object.getRoutingAlgorithm());
		
		return dialogArea;
	}

   /**
    * Add object to status source list
    */
   private void addObject()
   {
      ObjectSelectionDialog dlg = new ObjectSelectionDialog(getShell());
      dlg.enableMultiSelection(false);
      if (dlg.open() == Window.OK)
      {
         AbstractObject[] objects = dlg.getSelectedObjects(AbstractObject.class);
         if (objects.length > 0)
         {
            for(AbstractObject obj : objects)
            {
               object.addStatusObject(obj.getObjectId());
               list.add((obj != null) ? obj.getObjectName() : ("<" + Long.toString(obj.getObjectId()) + ">")); //$NON-NLS-1$ //$NON-NLS-2$
            }
         }
      }
   }

   /**
    * Remove object from status source list
    */
   private void removeObject()
   {
      int index = list.getSelectionIndex();
      list.remove(index);
      object.removeStatusObjectByIndex(index);
   }
	  
	/**
	 * Apply changes
	 * 
	 * @param isApply true if update operation caused by "Apply" button
	 */
	private boolean applyChanges(final boolean isApply)
	{
		object.setName(name.getText());
		object.setConnectorName1(connector1.getText());
		object.setConnectorName2(connector2.getText());
		if (radioColorCustom.getSelection())
		{
			object.setColor(ColorConverter.rgbToInt(color.getColorValue()));
         object.setColorSource(NetworkMapLink.COLOR_SOURCE_CUSTOM_COLOR);
		}
		else if (radioColorObject.getSelection())
		{
         object.setColorSource(NetworkMapLink.COLOR_SOURCE_OBJECT_STATUS);
			//status objects already set
		}
      else if (radioColorScript.getSelection())
      {
         object.setColorSource(NetworkMapLink.COLOR_SOURCE_SCRIPT);
         object.setColorProvider(script.getScriptName());
      }
		else
		{
         object.setColorSource(NetworkMapLink.COLOR_SOURCE_DEFAULT);
		}
      object.setUseActiveThresholds(checkUseThresholds.getSelection());
		object.setRoutingAlgorithm(routingAlgorithm.getSelectionIndex());
		object.update();
		return true;
	}

   /**
    * @see org.eclipse.jface.preference.PreferencePage#performOk()
    */
	@Override
	public boolean performOk()
	{
		return applyChanges(false);
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performApply()
	 */
	@Override
	protected void performApply()
	{
		applyChanges(true);
	}
}
