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
package org.netxms.nxmc.modules.dashboards.widgets;

import org.eclipse.swt.SWT;
import org.netxms.client.dashboards.DashboardElement;
import org.netxms.client.datacollection.ChartConfiguration;
import org.netxms.nxmc.modules.charts.api.ChartType;
import org.netxms.nxmc.modules.charts.widgets.Chart;
import org.netxms.nxmc.modules.dashboards.config.TableBarChartConfig;
import org.netxms.nxmc.modules.dashboards.views.AbstractDashboardView;

/**
 * Bar chart for tables
 */
public class TableBarChartElement extends TableComparisonChartElement
{
	/**
	 * @param parent
	 * @param data
	 */
   public TableBarChartElement(DashboardControl parent, DashboardElement element, AbstractDashboardView view)
	{
      super(parent, element, view);

		try
		{
			config = TableBarChartConfig.createFromXml(element.getData());
		}
		catch(Exception e)
		{
			e.printStackTrace();
			config = new TableBarChartConfig();
		}

      processCommonSettings(config);

      ChartConfiguration chartConfig = new ChartConfiguration();
      chartConfig.setTitleVisible(false);
      chartConfig.setLegendPosition(config.getLegendPosition());
      chartConfig.setLegendVisible(config.isShowLegend());
      chartConfig.setExtendedLegend(config.isExtendedLegend());
      chartConfig.setTransposed(((TableBarChartConfig)config).isTransposed());
      chartConfig.setTranslucent(config.isTranslucent());
      chartConfig.setAutoScale(config.isAutoScale());
      chartConfig.setMinYScaleValue(config.getMinYScaleValue());
      chartConfig.setMaxYScaleValue(config.getMaxYScaleValue());

      chart = new Chart(getContentArea(), SWT.NONE, ChartType.BAR, chartConfig);
		
		startRefreshTimer();
	}
}
