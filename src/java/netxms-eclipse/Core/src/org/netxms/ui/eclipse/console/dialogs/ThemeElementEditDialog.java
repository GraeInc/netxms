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
package org.netxms.ui.eclipse.console.dialogs;

import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Shell;
import org.netxms.ui.eclipse.console.resources.ThemeElement;
import org.netxms.ui.eclipse.widgets.ExtendedColorSelector;

/**
 * Dialog for editing theme elements
 */
public class ThemeElementEditDialog extends Dialog
{
   private ThemeElement element;
   private ExtendedColorSelector foreground;
   private ExtendedColorSelector background;

   /**
    * @param parentShell
    */
   public ThemeElementEditDialog(Shell parentShell, ThemeElement element)
   {
      super(parentShell);
      this.element = element;
   }

   /**
    * @see org.eclipse.jface.window.Window#configureShell(org.eclipse.swt.widgets.Shell)
    */
   @Override
   protected void configureShell(Shell newShell)
   {
      super.configureShell(newShell);
      newShell.setText("Edit Theme Element");
   }

   /**
    * @see org.eclipse.jface.dialogs.Dialog#createDialogArea(org.eclipse.swt.widgets.Composite)
    */
   @Override
   protected Control createDialogArea(Composite parent)
   {
      Composite dialogArea = (Composite)super.createDialogArea(parent);

      GridLayout layout = new GridLayout();
      layout.numColumns = 2;
      layout.makeColumnsEqualWidth = true;
      dialogArea.setLayout(layout);

      foreground = new ExtendedColorSelector(dialogArea);
      foreground.setLabels("Foreground color", null, null);
      foreground.setColorValue(element.foreground);

      background = new ExtendedColorSelector(dialogArea);
      background.setLabels("Background color", null, null);
      background.setColorValue(element.background);

      return dialogArea;
   }

   /**
    * @see org.eclipse.jface.dialogs.Dialog#okPressed()
    */
   @Override
   protected void okPressed()
   {
      element.foreground = foreground.getColorValue();
      element.background = background.getColorValue();
      super.okPressed();
   }
}
