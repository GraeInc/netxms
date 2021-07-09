/**
 * NetXMS - open source network management system
 * Copyright (C) 2021 Raden Solutions
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
package org.netxms.ui.eclipse.logviewer.widgets;

import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.forms.widgets.FormToolkit;
import org.netxms.client.constants.ColumnFilterType;
import org.netxms.client.log.ColumnFilter;

/**
 * Condition editor for notification status columns
 */
public class StatusConditionEditor extends ConditionEditor
{
   private static final String[] OPERATIONS = { "IS", "IS NOT" };
   
   private Combo state;
   
   /**
    * @param parent
    * @param toolkit
    * @param column
    * @param parentElement
    */
   public StatusConditionEditor(Composite parent, FormToolkit toolkit)
   {
      super(parent, toolkit);
   }

   /**
    * @see org.netxms.ui.eclipse.logviewer.widgets.ConditionEditor#getOperations()
    */
   @Override
   protected String[] getOperations()
   {
      return OPERATIONS;
   }

   /**
    * @see org.netxms.ui.eclipse.logviewer.widgets.ConditionEditor#createContent(org.eclipse.swt.widgets.Composite)
    */
   @Override
   protected void createContent(Composite parent, ColumnFilter initialFilter)
   {
      state = new Combo(this, SWT.READ_ONLY | SWT.BORDER);
      toolkit.adapt(state);
      state.add("Failure");
      state.add("Success");
      state.select(0);
      GridData gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      state.setLayoutData(gd);

      if ((initialFilter != null) && (initialFilter.getType() == ColumnFilterType.EQUALS))
      {
         setSelectedOperation(initialFilter.isNegated() ? 1 : 0);
         state.select((int)initialFilter.getNumericValue());
      }
   }

   /**
    * @see org.netxms.ui.eclipse.logviewer.widgets.ConditionEditor#createFilter()
    */
   @Override
   public ColumnFilter createFilter()
   {
      ColumnFilter filter = new ColumnFilter(ColumnFilterType.EQUALS, state.getSelectionIndex());
      filter.setNegated(getSelectedOperation() == 1);
      return filter;
   }
}
