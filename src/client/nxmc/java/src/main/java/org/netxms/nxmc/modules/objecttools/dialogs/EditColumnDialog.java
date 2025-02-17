/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2015 Victor Kirhenshtein
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
package org.netxms.nxmc.modules.objecttools.dialogs;

import org.eclipse.jface.dialogs.Dialog;
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
import org.eclipse.swt.widgets.Shell;
import org.netxms.client.objecttools.ObjectToolTableColumn;
import org.netxms.client.snmp.SnmpObjectId;
import org.netxms.client.snmp.SnmpObjectIdFormatException;
import org.netxms.nxmc.base.widgets.LabeledText;
import org.netxms.nxmc.localization.LocalizationHelper;
import org.netxms.nxmc.modules.snmp.dialogs.MibSelectionDialog;
import org.netxms.nxmc.tools.MessageDialogHelper;
import org.netxms.nxmc.tools.WidgetHelper;
import org.xnap.commons.i18n.I18n;

/**
 * Dialog for creating or editing object tool column
 *
 */
public class EditColumnDialog extends Dialog
{
   private static I18n i18n = LocalizationHelper.getI18n(EditColumnDialog.class);
	private final String[] formatNames = { i18n.tr("String"), i18n.tr("Integer"), i18n.tr("Float"), i18n.tr("IP Address"), i18n.tr("MAC Address"), i18n.tr("Interface Index") };

	private boolean create;
	private boolean snmpColumn;
	private ObjectToolTableColumn columnObject;
	private LabeledText name;
	private Combo format;
	private LabeledText data;
	private Button selectButton;
	
	/**
	 * 
	 * @param parentShell
	 * @param create
	 * @param snmpColumn
	 */
	public EditColumnDialog(Shell parentShell, boolean create, boolean snmpColumn, ObjectToolTableColumn columnObject)
	{
		super(parentShell);
		this.create = create;
		this.snmpColumn = snmpColumn;
		this.columnObject = columnObject;
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.window.Window#configureShell(org.eclipse.swt.widgets.Shell)
	 */
	@Override
	protected void configureShell(Shell newShell)
	{
		super.configureShell(newShell);
		newShell.setText(create ? i18n.tr("Create Column") : i18n.tr("Edit Column"));
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.dialogs.Dialog#createDialogArea(org.eclipse.swt.widgets.Composite)
	 */
	@Override
	protected Control createDialogArea(Composite parent)
	{
		Composite dialogArea = (Composite)super.createDialogArea(parent);
		
		GridLayout layout = new GridLayout();
		layout.marginWidth = WidgetHelper.DIALOG_WIDTH_MARGIN;
		layout.marginHeight = WidgetHelper.DIALOG_HEIGHT_MARGIN;
		layout.verticalSpacing = WidgetHelper.DIALOG_SPACING;
		dialogArea.setLayout(layout);
		
		name = new LabeledText(dialogArea, SWT.NONE);
		name.setLabel(i18n.tr("Name"));
		name.setText(columnObject.getName());
		GridData gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		gd.widthHint = 350;
		name.setLayoutData(gd);
		
		format = WidgetHelper.createLabeledCombo(dialogArea, SWT.READ_ONLY, i18n.tr("Format"), WidgetHelper.DEFAULT_LAYOUT_DATA);
		for(int i = 0; i < formatNames.length; i++)
			format.add(formatNames[i]);
		format.select(columnObject.getFormat());
		
		Composite dataGroup = null;
		if (snmpColumn)
		{
			dataGroup = new Composite(dialogArea, SWT.NONE);
			layout = new GridLayout();
			layout.marginHeight = 0;
			layout.marginWidth = 0;
			layout.horizontalSpacing = WidgetHelper.INNER_SPACING;
			layout.numColumns = 2;
			dataGroup.setLayout(layout);
			gd = new GridData();
			gd.horizontalAlignment = SWT.FILL;
			gd.grabExcessHorizontalSpace = true;
			dataGroup.setLayoutData(gd);
		}
		
		data = new LabeledText(snmpColumn ? dataGroup : dialogArea, SWT.NONE);
		if (snmpColumn)
		{
			data.setLabel(i18n.tr("SNMP Object Identifier (OID)"));
			data.setText(columnObject.getSnmpOid());
		}
		else
		{
			data.setLabel(i18n.tr("Substring index (starting from 1)"));
			data.setText(Integer.toString(columnObject.getSubstringIndex()));
		}
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		gd.widthHint = 350;
		data.setLayoutData(gd);
		
		if (snmpColumn)
		{
			selectButton = new Button(dataGroup, SWT.PUSH);
			selectButton.setText("..."); //$NON-NLS-1$
			gd = new GridData();
			gd.verticalAlignment = SWT.BOTTOM;
			selectButton.setLayoutData(gd);
			selectButton.addSelectionListener(new SelectionListener() {
				@Override
				public void widgetSelected(SelectionEvent e)
				{
					SnmpObjectId initial;
					try
					{
						initial = SnmpObjectId.parseSnmpObjectId(data.getText());
					}
					catch(SnmpObjectIdFormatException ex)
					{
						initial = null;
					}
					MibSelectionDialog dlg = new MibSelectionDialog(getShell(), initial, 0);
					if (dlg.open() == Window.OK)
					{
						data.setText(dlg.getSelectedObjectId().toString());
					}
				}
				
				@Override
				public void widgetDefaultSelected(SelectionEvent e)
				{
					widgetSelected(e);
				}
			});
		}
		
		return dialogArea;
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.dialogs.Dialog#okPressed()
	 */
	@Override
	protected void okPressed()
	{
		if (snmpColumn)
		{
			/* TODO: add OID validation */
			columnObject.setSnmpOid(data.getText());
		}
		else
		{
			try
			{
				int n = Integer.parseInt(data.getText());
				columnObject.setSubstringIndex(n);
			}
			catch(NumberFormatException e)
			{
				MessageDialogHelper.openWarning(getShell(), i18n.tr("Warning"), i18n.tr("Please enter valid substring number"));
				return;
			}
		}
		
		columnObject.setFormat(format.getSelectionIndex());
		columnObject.setName(name.getText().trim());
		
		super.okPressed();
	}
}
