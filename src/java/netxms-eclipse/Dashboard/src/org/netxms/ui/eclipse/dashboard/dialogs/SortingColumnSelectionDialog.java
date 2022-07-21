/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2022 Raden Solutions
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
package org.netxms.ui.eclipse.dashboard.dialogs;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Shell;
import org.netxms.client.NXCSession;
import org.netxms.client.datacollection.DciSummaryTable;
import org.netxms.ui.eclipse.dashboard.Activator;
import org.netxms.ui.eclipse.dashboard.widgets.TableColumnSelector;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.widgets.AbstractSelector;

/**
 * Dialog for selecting sorting column in DCI summary table widget
 */
public class SortingColumnSelectionDialog extends Dialog
{
   private TableColumnSelector columnSelector;
   private Button checkDescending;
   private DciSummaryTable sourceSummaryTable;
   private int summaryTableId;
   private String columnName = "";
   private boolean descending;

   /**
    * @param parentShell
    * @param columnName
    * @param descending
    * @param summaryTableId
    */
   public SortingColumnSelectionDialog(Shell parentShell, String columnName, boolean descending, int summaryTableId)
   {
      super(parentShell);
      this.columnName = columnName;
      this.descending = descending;   
      this.summaryTableId = summaryTableId;
   }

   /**
    * @see org.eclipse.jface.window.Window#configureShell(org.eclipse.swt.widgets.Shell)
    */
   @Override
   protected void configureShell(Shell newShell)
   {
      super.configureShell(newShell);
      newShell.setText("Select Sorting Column");
   }

   /**
    * @see org.eclipse.jface.dialogs.Dialog#createDialogArea(org.eclipse.swt.widgets.Composite)
    */
   @Override
   protected Control createDialogArea(Composite parent)
   {
      Composite dialogArea = (Composite)super.createDialogArea(parent);

      final NXCSession session = ConsoleSharedData.getSession();
      new ConsoleJob("Reading summary table configuration", null, Activator.PLUGIN_ID) {
         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {
            sourceSummaryTable = session.getDciSummaryTable(summaryTableId);
            runInUIThread(new Runnable() {
               @Override
               public void run()
               {
                  columnSelector.setSummaryTable(sourceSummaryTable);
               }
            });
         }

         @Override
         protected String getErrorMessage()
         {
            return "Unable to get summary table configuration";
         }
      }.start();

      GridLayout layout = new GridLayout(); 
      dialogArea.setLayout(layout);

      columnSelector = new TableColumnSelector(dialogArea, SWT.NONE, AbstractSelector.SHOW_CLEAR_BUTTON, columnName, null, sourceSummaryTable);
      columnSelector.setLabel("Column");
      GridData gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.widthHint = 500;
      columnSelector.setLayoutData(gd);

      checkDescending = new Button(dialogArea, SWT.CHECK);
      checkDescending.setText("Descending order");
      checkDescending.setSelection(descending);

      return dialogArea;
   }

   /**
    * @see org.eclipse.jface.dialogs.Dialog#okPressed()
    */
   @Override
   protected void okPressed()
   {
      columnName = (checkDescending.getSelection() ? ">" : "<") + columnSelector.getColumnName();
      super.okPressed();
   }

   /**
    * @return the sortingColumn
    */
   public String getColumnName()
   {
      return columnName;
   }
}
