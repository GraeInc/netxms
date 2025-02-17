/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2023 Raden Solutions
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
package org.netxms.nxmc.modules.datacollection.propertypages;

import java.util.ArrayList;
import java.util.Collections;
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
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.TableColumn;
import org.netxms.client.datacollection.ColumnDefinition;
import org.netxms.client.datacollection.DataCollectionTable;
import org.netxms.client.datacollection.TableThreshold;
import org.netxms.nxmc.localization.LocalizationHelper;
import org.netxms.nxmc.modules.datacollection.DataCollectionObjectEditor;
import org.netxms.nxmc.modules.datacollection.TableColumnEnumerator;
import org.netxms.nxmc.modules.datacollection.dialogs.EditTableThresholdDialog;
import org.netxms.nxmc.modules.datacollection.propertypages.helpers.TableThresholdLabelProvider;
import org.netxms.nxmc.tools.WidgetHelper;
import org.xnap.commons.i18n.I18n;

/**
 * "Thresholds" property page for table DCI
 */
public class TableThresholds extends AbstractDCIPropertyPage
{
   private static final I18n i18n = LocalizationHelper.getI18n(TableThresholds.class);
	private static final String COLUMN_SETTINGS_PREFIX = "TableThresholds.ColumnList"; //$NON-NLS-1$
	
	private DataCollectionTable dci;
	private List<TableThreshold> thresholds;
	private TableViewer thresholdList;
	private Button addButton;
	private Button modifyButton;
	private Button deleteButton;
	private Button upButton;
	private Button downButton;
   private Button duplicateButton;
   
   /**
    * Constructor
    * 
    * @param editor
    */
   public TableThresholds(DataCollectionObjectEditor editor)
   {
      super(i18n.tr("Table Thresholds"), editor);
   }

   /**
    * @see org.netxms.nxmc.modules.datacollection.propertypages.AbstractDCIPropertyPage#createContents(org.eclipse.swt.widgets.Composite)
    */
	@Override
	protected Control createContents(Composite parent)
	{
	   Composite dialogArea = (Composite)super.createContents(parent);
		dci = editor.getObjectAsTable();
		
		if (editor.getTableColumnEnumerator() == null)
		{
   	   final List<ColumnDefinition> columns = new ArrayList<ColumnDefinition>();
         for(ColumnDefinition c : dci.getColumns())
            columns.add(new ColumnDefinition(c));
         editor.setTableColumnEnumerator(new TableColumnEnumerator() {
            @Override
            public List<String> getColumns()
            {
               List<String> list = new ArrayList<String>();
               for(int i = 0; i < columns.size(); i++)
                  list.add(columns.get(i).getName());
               return list;
            }
         });
		}
		
		thresholds = new ArrayList<TableThreshold>();
		for(TableThreshold t : dci.getThresholds())
			thresholds.add(new TableThreshold(t));

		GridLayout layout = new GridLayout();
		layout.verticalSpacing = WidgetHelper.OUTER_SPACING;
		layout.marginWidth = 0;
		layout.marginHeight = 0;
      dialogArea.setLayout(layout);

      Composite thresholdListArea = new Composite(dialogArea, SWT.NONE);
      GridData gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.verticalAlignment = SWT.FILL;
      gd.grabExcessVerticalSpace = true;
      gd.horizontalSpan = 2;
      thresholdListArea.setLayoutData(gd);
      layout = new GridLayout();
		layout.verticalSpacing = WidgetHelper.INNER_SPACING;
		layout.marginWidth = 0;
		layout.marginHeight = 0;
		layout.numColumns = 2;
      thresholdListArea.setLayout(layout);
	
      new Label(thresholdListArea, SWT.NONE).setText(i18n.tr("Thresholds"));
      
      thresholdList = new TableViewer(thresholdListArea, SWT.BORDER | SWT.MULTI | SWT.FULL_SELECTION);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.verticalAlignment = SWT.FILL;
      gd.grabExcessVerticalSpace = true;
      gd.horizontalSpan = 2;
      thresholdList.getControl().setLayoutData(gd);
      setupThresholdList();
      thresholdList.setInput(thresholds.toArray());
      
      Composite leftButtons = new Composite(thresholdListArea, SWT.NONE);
      gd = new GridData();
      gd.horizontalAlignment = SWT.LEFT;
      leftButtons.setLayoutData(gd);
      RowLayout buttonsLayout = new RowLayout(SWT.HORIZONTAL);
      buttonsLayout.marginBottom = 0;
      buttonsLayout.marginLeft = 0;
      buttonsLayout.marginRight = 0;
      buttonsLayout.marginTop = 0;
      buttonsLayout.spacing = WidgetHelper.OUTER_SPACING;
      buttonsLayout.fill = true;
      buttonsLayout.pack = false;
      leftButtons.setLayout(buttonsLayout);
      
      upButton = new Button(leftButtons, SWT.PUSH);
      upButton.setText(i18n.tr("&Up"));
      RowData rd = new RowData();
      rd.width = WidgetHelper.BUTTON_WIDTH_HINT;
      upButton.setLayoutData(rd);
      upButton.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}
			
			@Override
			public void widgetSelected(SelectionEvent e)
			{
				moveSelectionUp();
			}
		});

      downButton = new Button(leftButtons, SWT.PUSH);
      downButton.setText(i18n.tr("Dow&n"));
      rd = new RowData();
      rd.width = WidgetHelper.BUTTON_WIDTH_HINT;
      downButton.setLayoutData(rd);
      downButton.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}
			
			@Override
			public void widgetSelected(SelectionEvent e)
			{
				moveSelectionDown();
			}
		});

      Composite buttons = new Composite(thresholdListArea, SWT.NONE);
      gd = new GridData();
      gd.horizontalAlignment = SWT.RIGHT;
      buttons.setLayoutData(gd);
      buttonsLayout = new RowLayout(SWT.HORIZONTAL);
      buttonsLayout.marginBottom = 0;
      buttonsLayout.marginLeft = 0;
      buttonsLayout.marginRight = 0;
      buttonsLayout.marginTop = 0;
      buttonsLayout.spacing = WidgetHelper.OUTER_SPACING;
      buttonsLayout.fill = true;
      buttonsLayout.pack = false;
      buttons.setLayout(buttonsLayout);
      
      addButton = new Button(buttons, SWT.PUSH);
      addButton.setText(i18n.tr("&Add..."));
      rd = new RowData();
      rd.width = WidgetHelper.BUTTON_WIDTH_HINT;
      addButton.setLayoutData(rd);
      addButton.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}
			
			@Override
			public void widgetSelected(SelectionEvent e)
			{
				addThreshold();
			}
		});
      
      duplicateButton = new Button(buttons, SWT.PUSH);
      duplicateButton.setText("Duplicate");
      rd = new RowData();
      rd.width = WidgetHelper.BUTTON_WIDTH_HINT;
      duplicateButton.setLayoutData(rd);
      duplicateButton.setEnabled(false);
      duplicateButton.addSelectionListener(new SelectionListener() {
         @Override
         public void widgetDefaultSelected(SelectionEvent e)
         {
            widgetSelected(e);
         }

         @Override
         public void widgetSelected(SelectionEvent e)
         {
            duplicateThreshold();
         }
      });
      
      modifyButton = new Button(buttons, SWT.PUSH);
      modifyButton.setText(i18n.tr("&Edit..."));
      rd = new RowData();
      rd.width = WidgetHelper.BUTTON_WIDTH_HINT;
      modifyButton.setLayoutData(rd);
      modifyButton.setEnabled(false);
      modifyButton.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}

			@Override
			public void widgetSelected(SelectionEvent e)
			{
				editThreshold();
			}
      });
      
      deleteButton = new Button(buttons, SWT.PUSH);
      deleteButton.setText(i18n.tr("&Delete"));
      rd = new RowData();
      rd.width = WidgetHelper.BUTTON_WIDTH_HINT;
      deleteButton.setLayoutData(rd);
      deleteButton.setEnabled(false);
      deleteButton.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}

			@Override
			public void widgetSelected(SelectionEvent e)
			{
				deleteThresholds();
			}
      });
      
      /*** Selection change listener for thresholds list ***/
      thresholdList.addSelectionChangedListener(new ISelectionChangedListener() {
			@Override
			public void selectionChanged(SelectionChangedEvent event)
			{
				IStructuredSelection selection = (IStructuredSelection)event.getSelection();
				deleteButton.setEnabled(selection.size() > 0);
            duplicateButton.setEnabled(selection.size() > 0);
				if (selection.size() == 1)
				{
					modifyButton.setEnabled(true);
					upButton.setEnabled(thresholds.indexOf(selection.getFirstElement()) > 0);
					downButton.setEnabled(thresholds.indexOf(selection.getFirstElement()) < thresholds.size() - 1);
				}
				else
				{
					modifyButton.setEnabled(false);
					upButton.setEnabled(false);
					downButton.setEnabled(false);
				}
			}
      });

      thresholdList.addDoubleClickListener(new IDoubleClickListener() {
			@Override
			public void doubleClick(DoubleClickEvent event)
			{
				editThreshold();
			}
      });
      
      return dialogArea;
	}

	/**
	 * Setup threshold list control
	 */
	private void setupThresholdList()
	{
		Table table = thresholdList.getTable();
		table.setLinesVisible(true);
		table.setHeaderVisible(true);
		
		TableColumn column = new TableColumn(table, SWT.LEFT);
		column.setText(i18n.tr("Condition"));
		column.setWidth(200);
		
      column = new TableColumn(table, SWT.LEFT);
      column.setText("Samples");
      column.setWidth(90);
      
		column = new TableColumn(table, SWT.LEFT);
		column.setText(i18n.tr("Activation Event"));
		column.setWidth(140);
		
		column = new TableColumn(table, SWT.LEFT);
		column.setText(i18n.tr("Deactivation Event"));
		column.setWidth(140);
		
		WidgetHelper.restoreColumnSettings(table, COLUMN_SETTINGS_PREFIX);
		
		thresholdList.setContentProvider(new ArrayContentProvider());
		thresholdList.setLabelProvider(new TableThresholdLabelProvider());
	}

	/**
	 * Delete selected thresholds
	 */
	private void deleteThresholds()
	{
		final IStructuredSelection selection = (IStructuredSelection)thresholdList.getSelection();
		if (!selection.isEmpty())
		{
			Iterator<?> it = selection.iterator();
			while(it.hasNext())
			{
				thresholds.remove(it.next());
			}
	      thresholdList.setInput(thresholds.toArray());
		}
	}
	
	/**
	 * Edit selected threshold
	 */
	private void editThreshold()
	{
		final IStructuredSelection selection = (IStructuredSelection)thresholdList.getSelection();
		if (selection.size() == 1)
		{
			final TableThreshold t = (TableThreshold)selection.getFirstElement();
			EditTableThresholdDialog dlg = new EditTableThresholdDialog(getShell(), t, editor.getTableColumnEnumerator());
			if (dlg.open() == Window.OK)
			{
				thresholdList.update(t, null);
			}
		}
	}
	
	/**	
	 * Add new threshold
	 */
	private void addThreshold()
	{
		final TableThreshold t = new TableThreshold();
		final EditTableThresholdDialog dlg = new EditTableThresholdDialog(getShell(), t, editor.getTableColumnEnumerator());
		if (dlg.open() == Window.OK)
		{
			thresholds.add(t);
	      thresholdList.setInput(thresholds.toArray());
	      thresholdList.setSelection(new StructuredSelection(t));
		}
	}
	
	/**
    * Duplicate selected threshold
    */
   @SuppressWarnings("unchecked")
   private void duplicateThreshold()
   {
      final IStructuredSelection selection = (IStructuredSelection)thresholdList.getSelection();
      if (selection.size() > 0)
      {
         List<TableThreshold> list = selection.toList();
         for(TableThreshold t : list)
         {               
            thresholds.add(thresholds.indexOf(t) + 1, t.duplicate());
            thresholdList.setInput(thresholds.toArray());
         }
      }
   }
	
	/**
	 * Move selected element up 
	 */
	private void moveSelectionUp()
	{
		final IStructuredSelection selection = (IStructuredSelection)thresholdList.getSelection();
		if (selection.size() != 1)
			return;
		
		final TableThreshold t = (TableThreshold)selection.getFirstElement();
		int index = thresholds.indexOf(t);
		if (index > 0)
		{
			Collections.swap(thresholds, index, index - 1);
			thresholdList.setInput(thresholds.toArray());
			thresholdList.setSelection(new StructuredSelection(t));
		}
	}
	
	/**
	 * Move selected element down
	 */
	private void moveSelectionDown()
	{
		final IStructuredSelection selection = (IStructuredSelection)thresholdList.getSelection();
		if (selection.size() != 1)
			return;
		
		final TableThreshold t = (TableThreshold)selection.getFirstElement();
		int index = thresholds.indexOf(t);
		if (index < thresholds.size() - 1)
		{
			Collections.swap(thresholds, index, index + 1);
			thresholdList.setInput(thresholds.toArray());
			thresholdList.setSelection(new StructuredSelection(t));
		}
	}
	
	/**
	 * Apply changes
	 * 
	 * @param isApply true if update operation caused by "Apply" button
	 */
   @Override
	protected boolean applyChanges(final boolean isApply)
	{
      saveSettings();
      
		dci.getThresholds().clear();
		dci.getThresholds().addAll(thresholds);
		editor.modify();		
		return true;
	}
	
   /**
    * @see org.eclipse.jface.preference.PreferencePage#performCancel()
    */
	@Override
	public boolean performCancel()
	{
		saveSettings();
		return true;
	}
	
	/**
	 * Save settings
	 */
	private void saveSettings()
	{
		WidgetHelper.saveColumnSettings(thresholdList.getTable(), COLUMN_SETTINGS_PREFIX);
	}
}
