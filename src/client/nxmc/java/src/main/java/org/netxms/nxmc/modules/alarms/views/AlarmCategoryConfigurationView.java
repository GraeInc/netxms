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
package org.netxms.nxmc.modules.alarms.views;

import org.eclipse.jface.action.Separator;
import org.eclipse.jface.action.ToolBarManager;
import org.eclipse.jface.viewers.DoubleClickEvent;
import org.eclipse.jface.viewers.IDoubleClickListener;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Composite;
import org.netxms.nxmc.base.views.ConfigurationView;
import org.netxms.nxmc.localization.LocalizationHelper;
import org.netxms.nxmc.modules.alarms.widgets.AlarmCategoryWidget;
import org.netxms.nxmc.resources.ResourceManager;
import org.xnap.commons.i18n.I18n;

/**
 * Configuration view for alarm categories
 */
public class AlarmCategoryConfigurationView extends ConfigurationView
{
   private static I18n i18n = LocalizationHelper.getI18n(AlarmCategoryConfigurationView.class);
   private static final String ID = "AlarmCategoryConfigurator"; //$NON-NLS-1$

   private AlarmCategoryWidget dataView;
   
   /**
    * Constructor
    */
   public AlarmCategoryConfigurationView()
   {
      super(i18n.tr("Alarm categories"), ResourceManager.getImageDescriptor("icons/config-views/alarm_category.png"), ID, true);
   }

   @Override
   protected void createContent(Composite parent)
   {
      parent.setLayout(new FillLayout());

      dataView = new AlarmCategoryWidget(this, parent, SWT.NONE, ID, true);
      setFilterClient(dataView.getViewer(), dataView.getFilter());

      dataView.getViewer().addDoubleClickListener(new IDoubleClickListener() {
         @Override
         public void doubleClick(DoubleClickEvent event)
         {
            dataView.editCategory();
         }
      });
   }

   /**
    * Fill view's local toolbar
    *
    * @param manager toolbar manager
    */
   @Override
   protected void fillLocalToolbar(ToolBarManager manager)
   {
      manager.add(dataView.getAddCategoryAction());
      manager.add(new Separator());
   }
   
   /**
    * @see org.netxms.nxmc.base.views.View#refresh()
    */
   @Override
   public void refresh()
   {
      dataView.refreshView();
   }

   /**
    * @see org.netxms.nxmc.base.views.ConfigurationView#isModified()
    */
   @Override
   public boolean isModified()
   {
      return false;
   }

   /**
    * @see org.netxms.nxmc.base.views.ConfigurationView#save()
    */
   @Override
   public void save()
   {
   }
}
