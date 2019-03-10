/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2019 Raden Solutions
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
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.ui.dialogs.PropertyPage;
import org.netxms.client.maps.elements.NetworkMapTextBox;
import org.netxms.client.objects.AbstractObject;
import org.netxms.ui.eclipse.objectbrowser.dialogs.ObjectSelectionDialog;
import org.netxms.ui.eclipse.objectbrowser.widgets.ObjectSelector;
import org.netxms.ui.eclipse.tools.ColorConverter;
import org.netxms.ui.eclipse.tools.WidgetHelper;
import org.netxms.ui.eclipse.widgets.LabeledText;

/**
 * Text box property page for network map element
 */
public class GeneralTextBox extends PropertyPage
{
   private NetworkMapTextBox textBoxElement;
   private LabeledText text;
   private ColorSelector backgroundColor;
   private ColorSelector textColor;
   private ColorSelector borderColor;
   private Button checkShowBorder;
   private Spinner fontSize;
   private ObjectSelector drillDownObject;

   /* (non-Javadoc)
    * @see org.eclipse.jface.preference.PreferencePage#createContents(org.eclipse.swt.widgets.Composite)
    */
   @Override
   protected Control createContents(Composite parent)
   {
      textBoxElement = (NetworkMapTextBox)getElement().getAdapter(NetworkMapTextBox.class);

      Composite dialogArea = new Composite(parent, SWT.NONE);
      
      GridLayout layout = new GridLayout();
      layout.marginWidth = WidgetHelper.DIALOG_WIDTH_MARGIN;
      layout.marginHeight = WidgetHelper.DIALOG_HEIGHT_MARGIN;
      layout.verticalSpacing = WidgetHelper.OUTER_SPACING;
      layout.numColumns = 3;
      dialogArea.setLayout(layout);

      text = new LabeledText(dialogArea, SWT.NONE, SWT.MULTI | SWT.BORDER | SWT.V_SCROLL);
      text.setLabel("Text");
      text.setText(textBoxElement.getText());
      GridData gd = new GridData();
      gd.horizontalSpan = 3;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalAlignment = SWT.FILL;
      gd.heightHint = 200;
      text.setLayoutData(gd);
      
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      backgroundColor = WidgetHelper.createLabeledColorSelector(dialogArea, "Background color", gd);
      backgroundColor.setColorValue(ColorConverter.rgbFromInt(textBoxElement.getBackgroundColor()));
      
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      textColor = WidgetHelper.createLabeledColorSelector(dialogArea, "Text color", gd);
      textColor.setColorValue(ColorConverter.rgbFromInt(textBoxElement.getTextColor()));

      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      borderColor = WidgetHelper.createLabeledColorSelector(dialogArea, "Border color", gd);
      borderColor.setColorValue(ColorConverter.rgbFromInt(textBoxElement.getBorderColor()));

      checkShowBorder = new Button(dialogArea, SWT.CHECK);
      checkShowBorder.setText("Show border");
      checkShowBorder.setSelection(textBoxElement.isBorderRequired());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalSpan = 3;
      checkShowBorder.setLayoutData(gd);
      
      checkShowBorder.addSelectionListener(new SelectionListener() {
         @Override
         public void widgetSelected(SelectionEvent e)
         {
            borderColor.setEnabled(checkShowBorder.getSelection());
         }
         
         @Override
         public void widgetDefaultSelected(SelectionEvent e)
         {
            widgetSelected(e);
         }
      });

      fontSize = WidgetHelper.createLabeledSpinner(dialogArea, SWT.BORDER, "Text size", 1, 100, WidgetHelper.DEFAULT_LAYOUT_DATA);
      fontSize.setSelection(textBoxElement.getFontSize());
      
      drillDownObject = new ObjectSelector(dialogArea, SWT.NONE, true);
      drillDownObject.setLabel("Drill-down object");
      drillDownObject.setObjectClass(AbstractObject.class);
      drillDownObject.setObjectId(textBoxElement.getDrillDownObjectId());
      drillDownObject.setClassFilter(ObjectSelectionDialog.createDashboardAndNetworkMapSelectionFilter());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalSpan = 2;
      drillDownObject.setLayoutData(gd);
      
      return dialogArea;
   }

   /**
    * Apply changes
    * 
    * @param isApply true if update operation caused by "Apply" button
    */
   private boolean applyChanges(final boolean isApply)
   {
      textBoxElement.setText(text.getText());
      textBoxElement.setBackgroundColor(ColorConverter.rgbToInt(backgroundColor.getColorValue()));
      textBoxElement.setTextColor(ColorConverter.rgbToInt(textColor.getColorValue()));
      textBoxElement.setBorderRequired(checkShowBorder.getSelection());
      textBoxElement.setBorderColor(ColorConverter.rgbToInt(borderColor.getColorValue()));
      textBoxElement.setFontSize(fontSize.getSelection());
      textBoxElement.setDrillDownObjectId(drillDownObject.getObjectId());
      return true;
   }

   /* (non-Javadoc)
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
