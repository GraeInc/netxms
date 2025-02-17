/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2020 Victor Kirhenshtein
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
package org.netxms.nxmc.modules.objecttools.propertypages;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.DoubleClickEvent;
import org.eclipse.jface.viewers.IDoubleClickListener;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.layout.RowData;
import org.eclipse.swt.layout.RowLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.TableColumn;
import org.netxms.client.objecttools.ObjectTool;
import org.netxms.client.objecttools.ObjectToolDetails;
import org.netxms.client.objecttools.ObjectToolTableColumn;
import org.netxms.nxmc.base.propertypages.PropertyPage;
import org.netxms.nxmc.localization.LocalizationHelper;
import org.netxms.nxmc.modules.objecttools.dialogs.EditColumnDialog;
import org.netxms.nxmc.modules.objecttools.propertypages.helpers.ToolColumnLabelProvider;
import org.netxms.nxmc.tools.WidgetHelper;
import org.xnap.commons.i18n.I18n;

/**
 * "Columns" property page for object tool
 */
public class Columns extends PropertyPage
{
   private static final I18n i18n = LocalizationHelper.getI18n(Columns.class);
   
	private ObjectToolDetails objectTool;
	private List<ObjectToolTableColumn> columns = new ArrayList<ObjectToolTableColumn>();
	private TableViewer viewer;
	private Button buttonAdd;
	private Button buttonEdit;
	private Button buttonRemove;
   
	
   /**
    * Constructor
    * 
    * @param toolDetails
    */
   public Columns(ObjectToolDetails toolDetails)
   {
      super("Columns");
      noDefaultAndApplyButton();
      objectTool = toolDetails;
   }
	
	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#createContents(org.eclipse.swt.widgets.Composite)
	 */
	@Override
	protected Control createContents(Composite parent)
	{
		for(ObjectToolTableColumn tc : objectTool.getColumns())
			columns.add(new ObjectToolTableColumn(tc));

		Composite dialogArea = new Composite(parent, SWT.NONE);
		
		GridLayout layout = new GridLayout();
		layout.verticalSpacing = WidgetHelper.DIALOG_SPACING;
		layout.marginWidth = 0;
		layout.marginHeight = 0;
		dialogArea.setLayout(layout);
		
		viewer = new TableViewer(dialogArea, SWT.BORDER | SWT.MULTI | SWT.FULL_SELECTION);
		GridData gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		gd.verticalAlignment = SWT.FILL;
		gd.grabExcessVerticalSpace = true;
		viewer.getTable().setLayoutData(gd);
		viewer.setContentProvider(new ArrayContentProvider());
		viewer.setLabelProvider(new ToolColumnLabelProvider(objectTool.getToolType() == ObjectTool.TYPE_SNMP_TABLE));
		setupTableColumns();
		viewer.addSelectionChangedListener(new ISelectionChangedListener() {
			@Override
			public void selectionChanged(SelectionChangedEvent event)
			{
				IStructuredSelection selection = (IStructuredSelection)viewer.getSelection();
				buttonEdit.setEnabled(selection.size() == 1);
				buttonRemove.setEnabled(selection.size() > 0);
			}
		});
		viewer.addDoubleClickListener(new IDoubleClickListener() {
			@Override
			public void doubleClick(DoubleClickEvent event)
			{
				editColumn();
			}
		});
		viewer.setInput(columns.toArray());
		
      Composite buttons = new Composite(dialogArea, SWT.NONE);
      RowLayout buttonLayout = new RowLayout();
      buttonLayout.type = SWT.HORIZONTAL;
      buttonLayout.pack = false;
      buttonLayout.marginWidth = 0;
      buttons.setLayout(buttonLayout);
      gd = new GridData();
      gd.horizontalAlignment = SWT.RIGHT;
      gd.verticalIndent = WidgetHelper.OUTER_SPACING - WidgetHelper.INNER_SPACING;
      buttons.setLayoutData(gd);

      buttonAdd = new Button(buttons, SWT.PUSH);
      buttonAdd.setText(i18n.tr("&Add..."));
      buttonAdd.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}

			@Override
			public void widgetSelected(SelectionEvent e)
			{
				addColumn();
			}
      });
      RowData rd = new RowData();
      rd.width = WidgetHelper.BUTTON_WIDTH_HINT;
      buttonAdd.setLayoutData(rd);
		
      buttonEdit = new Button(buttons, SWT.PUSH);
      buttonEdit.setText(i18n.tr("&Edit..."));
      buttonEdit.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}

			@Override
			public void widgetSelected(SelectionEvent e)
			{
				editColumn();
			}
      });
      rd = new RowData();
      rd.width = WidgetHelper.BUTTON_WIDTH_HINT;
      buttonEdit.setLayoutData(rd);

      buttonRemove = new Button(buttons, SWT.PUSH);
      buttonRemove.setText(i18n.tr("&Delete"));
      buttonRemove.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}

			@Override
			public void widgetSelected(SelectionEvent e)
			{
				removeColumn();
			}
      });
      rd = new RowData();
      rd.width = WidgetHelper.BUTTON_WIDTH_HINT;
      buttonRemove.setLayoutData(rd);

      return dialogArea;
	}
	
	/**
	 * Setup table viewer columns
	 */
	private void setupTableColumns()
	{
		TableColumn column = new TableColumn(viewer.getTable(), SWT.LEFT);
		column.setText(i18n.tr("Name"));
		column.setWidth(200);
		
		column = new TableColumn(viewer.getTable(), SWT.LEFT);
		column.setText(i18n.tr("Format"));
		column.setWidth(90);
		
		column = new TableColumn(viewer.getTable(), SWT.LEFT);
		column.setText(objectTool.getToolType() == ObjectTool.TYPE_SNMP_TABLE ? i18n.tr("OID") : i18n.tr("Index"));
		column.setWidth(200);
		
		viewer.getTable().setHeaderVisible(true);
		
		WidgetHelper.restoreColumnSettings(viewer.getTable(), "ColumnsPropertyPage"); //$NON-NLS-1$
	}

	/**
	 * Add new column
	 */
	private void addColumn()
	{
		ObjectToolTableColumn tc = new ObjectToolTableColumn(i18n.tr("Column ") + Integer.toString(columns.size() + 1));
		EditColumnDialog dlg = new EditColumnDialog(getShell(), true, objectTool.getToolType() == ObjectTool.TYPE_SNMP_TABLE, tc);
		if (dlg.open() == Window.OK)
		{
			columns.add(tc);
			viewer.setInput(columns.toArray());
			viewer.setSelection(new StructuredSelection(tc));
		}
	}
	
	/**
	 * Edit column
	 */
	private void editColumn()
	{
		IStructuredSelection selection = (IStructuredSelection)viewer.getSelection();
		if (selection.size() != 1)
			return;
		
		EditColumnDialog dlg = new EditColumnDialog(getShell(), false,
				objectTool.getToolType() == ObjectTool.TYPE_SNMP_TABLE, (ObjectToolTableColumn)selection.getFirstElement());
		dlg.open();
		viewer.update(selection.getFirstElement(), null);
	}
	
	/**
	 * Remove selected column(s)
	 */
	@SuppressWarnings("unchecked")
	private void removeColumn()
	{
		IStructuredSelection selection = (IStructuredSelection)viewer.getSelection();
		Iterator<ObjectToolTableColumn> it = selection.iterator();
		while(it.hasNext())
		{
			columns.remove(it.next());
		}
		viewer.setInput(columns.toArray());
	}

   /**
    * @see org.netxms.nxmc.base.propertypages.PropertyPage#applyChanges(boolean)
    */
   @Override
   protected boolean applyChanges(boolean isApply)
	{
      objectTool.setColumns(columns);
      WidgetHelper.saveColumnSettings(viewer.getTable(), "ColumnsPropertyPage"); //$NON-NLS-1$
      return true;
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performCancel()
	 */
	@Override
	public boolean performCancel()
	{
      if (isControlCreated())
      {
   		WidgetHelper.saveColumnSettings(viewer.getTable(), "ColumnsPropertyPage"); //$NON-NLS-1$
      }
      return super.performCancel();
	}
}
