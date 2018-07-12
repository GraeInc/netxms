/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2018 Victor Kirhenshtein
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

import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.preference.ColorSelector;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Shell;
import org.netxms.ui.eclipse.dashboard.widgets.internal.ObjectToolsConfig.Tool;
import org.netxms.ui.eclipse.objectbrowser.widgets.ObjectSelector;
import org.netxms.ui.eclipse.objecttools.widgets.ObjectToolSelector;
import org.netxms.ui.eclipse.tools.ColorConverter;
import org.netxms.ui.eclipse.tools.WidgetHelper;
import org.netxms.ui.eclipse.widgets.LabeledText;

/**
 * Edit tool configuration dialog
 */
public class EditToolDialog extends Dialog
{
   private LabeledText name;
   private ObjectSelector objectSelector;
   private ObjectToolSelector toolSelector;
   private ColorSelector colorSelector;
   private Tool tool;

   /**
    * @param parentShell
    */
   public EditToolDialog(Shell parentShell, Tool tool)
   {
      super(parentShell);
      this.tool = tool;
   }

   /* (non-Javadoc)
    * @see org.eclipse.jface.window.Window#configureShell(org.eclipse.swt.widgets.Shell)
    */
   @Override
   protected void configureShell(Shell newShell)
   {
      super.configureShell(newShell);
      newShell.setText("Edit Tool");
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
      layout.numColumns = 2;
      dialogArea.setLayout(layout);
      
      name = new LabeledText(dialogArea, SWT.NONE);
      name.setLabel("Display name");
      name.getTextControl().setTextLimit(63);
      GridData gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.widthHint = 350;
      name.setLayoutData(gd);
      
      Composite colorSelectionArea = new Composite(dialogArea, SWT.NONE);
      layout = new GridLayout();
      layout.marginWidth = 0;
      layout.marginHeight = 0;
      layout.verticalSpacing = WidgetHelper.INNER_SPACING;
      colorSelectionArea.setLayout(layout);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = false;
      colorSelectionArea.setLayoutData(gd);
      
      Label label = new Label(colorSelectionArea, SWT.NONE);
      label.setText("Color");
      
      colorSelector = new ColorSelector(colorSelectionArea);
      colorSelector.setColorValue(ColorConverter.rgbFromInt(tool.color));

      objectSelector = new ObjectSelector(dialogArea, SWT.NONE, false);
      objectSelector.setLabel("Object");
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.widthHint = 350;
      gd.horizontalSpan = 2;
      objectSelector.setLayoutData(gd);
      
      toolSelector = new ObjectToolSelector(dialogArea, SWT.NONE, 0);
      toolSelector.setLabel("Tool");
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.widthHint = 350;
      gd.horizontalSpan = 2;
      toolSelector.setLayoutData(gd);
      
      return dialogArea;
   }

   /* (non-Javadoc)
    * @see org.eclipse.jface.dialogs.Dialog#okPressed()
    */
   @Override
   protected void okPressed()
   {
      tool.name = name.getText().trim();
      tool.objectId = objectSelector.getObjectId();
      tool.toolId = toolSelector.getToolId();
      tool.color = ColorConverter.rgbToInt(colorSelector.getColorValue());
      super.okPressed();
   }

   /**
    * @return the tool
    */
   public Tool getTool()
   {
      return tool;
   }
}
