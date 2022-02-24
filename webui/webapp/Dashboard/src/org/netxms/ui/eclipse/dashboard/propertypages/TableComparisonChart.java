/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2022 Victor Kirhenshtein
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
package org.netxms.ui.eclipse.dashboard.propertypages;

import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Group;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.ui.dialogs.PropertyPage;
import org.netxms.client.datacollection.ChartConfiguration;
import org.netxms.client.objects.AbstractObject;
import org.netxms.ui.eclipse.dashboard.Messages;
import org.netxms.ui.eclipse.dashboard.widgets.TitleConfigurator;
import org.netxms.ui.eclipse.dashboard.widgets.internal.TableBarChartConfig;
import org.netxms.ui.eclipse.dashboard.widgets.internal.TableComparisonChartConfig;
import org.netxms.ui.eclipse.dashboard.widgets.internal.TablePieChartConfig;
import org.netxms.ui.eclipse.dashboard.widgets.internal.TableTubeChartConfig;
import org.netxms.ui.eclipse.objectbrowser.dialogs.ObjectSelectionDialog;
import org.netxms.ui.eclipse.objectbrowser.widgets.ObjectSelector;
import org.netxms.ui.eclipse.perfview.widgets.YAxisRangeEditor;
import org.netxms.ui.eclipse.tools.WidgetHelper;

/**
 * Table comparison chart properties
 */
public class TableComparisonChart extends PropertyPage
{
	private TableComparisonChartConfig config;
   private TitleConfigurator title;
	private Spinner refreshRate;
	private Combo legendPosition;
	private Button checkShowLegend;
   private Button checkExtendedLegend;
	private Button checkTranslucent;
	private Button checkTransposed;
   private YAxisRangeEditor yAxisRange;
   private ObjectSelector drillDownObject;

   /**
    * @see org.eclipse.jface.preference.PreferencePage#createContents(org.eclipse.swt.widgets.Composite)
    */
	@Override
	protected Control createContents(Composite parent)
	{
		config = (TableComparisonChartConfig)getElement().getAdapter(TableComparisonChartConfig.class);
		
		Composite dialogArea = new Composite(parent, SWT.NONE);
		
		GridLayout layout = new GridLayout();
		layout.numColumns = 2;
		layout.makeColumnsEqualWidth = true;
		dialogArea.setLayout(layout);
		
      title = new TitleConfigurator(dialogArea, config);
      GridData gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalSpan = 2;
      title.setLayoutData(gd);

		legendPosition = WidgetHelper.createLabeledCombo(dialogArea, SWT.READ_ONLY, Messages.get().TableComparisonChart_LegendPosition, WidgetHelper.DEFAULT_LAYOUT_DATA);
		legendPosition.add(Messages.get().TableComparisonChart_Left);
		legendPosition.add(Messages.get().TableComparisonChart_Right);
		legendPosition.add(Messages.get().TableComparisonChart_Top);
		legendPosition.add(Messages.get().TableComparisonChart_Bottom);
		legendPosition.select(positionIndexFromValue(config.getLegendPosition()));
		
		Group optionsGroup = new Group(dialogArea, SWT.NONE);
		optionsGroup.setText(Messages.get().TableComparisonChart_Options);
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		gd.verticalSpan = 2;
		optionsGroup.setLayoutData(gd);
		GridLayout optionsLayout = new GridLayout();
		optionsGroup.setLayout(optionsLayout);

		checkShowLegend = new Button(optionsGroup, SWT.CHECK);
		checkShowLegend.setText(Messages.get().TableComparisonChart_ShowLegend);
		checkShowLegend.setSelection(config.isShowLegend());
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		checkShowLegend.setLayoutData(gd);
		
      checkExtendedLegend = new Button(optionsGroup, SWT.CHECK);
      checkExtendedLegend.setText(Messages.get().AbstractChart_ExtendedLegend);
      checkExtendedLegend.setSelection(config.isExtendedLegend());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      checkExtendedLegend.setLayoutData(gd);
		
		checkTranslucent = new Button(optionsGroup, SWT.CHECK);
		checkTranslucent.setText(Messages.get().TableComparisonChart_Translucent);
		checkTranslucent.setSelection(config.isTranslucent());
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		checkTranslucent.setLayoutData(gd);
		
		if ((config instanceof TableBarChartConfig) || (config instanceof TableTubeChartConfig))
		{
			checkTransposed = new Button(optionsGroup, SWT.CHECK);
			checkTransposed.setText(Messages.get().TableComparisonChart_Transposed);
			checkTransposed.setSelection((config instanceof TableBarChartConfig) ? ((TableBarChartConfig)config).isTransposed() : ((TableTubeChartConfig)config).isTransposed());
			gd = new GridData();
			gd.horizontalAlignment = SWT.FILL;
			gd.grabExcessHorizontalSpace = true;
			checkTransposed.setLayoutData(gd);
		}
		
		gd = new GridData();
		gd.verticalAlignment = SWT.TOP;
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		refreshRate = WidgetHelper.createLabeledSpinner(dialogArea, SWT.BORDER, Messages.get().TableComparisonChart_RefreshInterval, 1, 10000, gd);
		refreshRate.setSelection(config.getRefreshRate());
		
      if (!(config instanceof TablePieChartConfig))
      {
         yAxisRange = new YAxisRangeEditor(dialogArea, SWT.NONE);
         gd = new GridData();
         gd.horizontalSpan = layout.numColumns;
         gd.horizontalAlignment = SWT.FILL;
         gd.grabExcessHorizontalSpace = true;
         yAxisRange.setLayoutData(gd);
         yAxisRange.setSelection(config.isAutoScale(), config.modifyYBase(),
                                 config.getMinYScaleValue(), config.getMaxYScaleValue());
      }
      
      drillDownObject = new ObjectSelector(dialogArea, SWT.NONE, true);
      drillDownObject.setLabel("Drill-down object");
      drillDownObject.setObjectClass(AbstractObject.class);
      drillDownObject.setClassFilter(ObjectSelectionDialog.createDashboardAndNetworkMapSelectionFilter());
      drillDownObject.setObjectId(config.getDrillDownObjectId());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalSpan = 2;
      drillDownObject.setLayoutData(gd);
		
		return dialogArea;
	}
	
	/**
	 * @param value
	 * @return
	 */
	private int positionIndexFromValue(int value)
	{
		switch(value)
		{
			case ChartConfiguration.POSITION_BOTTOM:
				return 3;
			case ChartConfiguration.POSITION_LEFT:
				return 0;
			case ChartConfiguration.POSITION_RIGHT:
				return 1;
			case ChartConfiguration.POSITION_TOP:
				return 2;
		}
		return 0;
	}

   /**
    * @see org.eclipse.jface.preference.PreferencePage#performOk()
    */
	@Override
	public boolean performOk()
	{
      title.updateConfiguration(config);
		config.setLegendPosition(1 << legendPosition.getSelectionIndex());
		config.setRefreshRate(refreshRate.getSelection());
		config.setShowLegend(checkShowLegend.getSelection());
      config.setExtendedLegend(checkExtendedLegend.getSelection());
		config.setTranslucent(checkTranslucent.getSelection());	
      config.setDrillDownObjectId(drillDownObject.getObjectId());

      if (!(config instanceof TablePieChartConfig))
      {
	      if (!yAxisRange.validate(true))
	         return false;

         config.setAutoScale(yAxisRange.isAuto());
         config.setMinYScaleValue(yAxisRange.getMinY());
         config.setMaxYScaleValue(yAxisRange.getMaxY());
         config.setModifyYBase(yAxisRange.modifyYBase());
      }

		if (config instanceof TableBarChartConfig)
		{
			((TableBarChartConfig)config).setTransposed(checkTransposed.getSelection());
		}
		else if (config instanceof TableTubeChartConfig)
		{
			((TableTubeChartConfig)config).setTransposed(checkTransposed.getSelection());
		}
		return true;
	}
}
