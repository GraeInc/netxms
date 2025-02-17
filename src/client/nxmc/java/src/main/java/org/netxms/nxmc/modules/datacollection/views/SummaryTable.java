/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2015 Victor Kirhenshtein
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
package org.netxms.nxmc.modules.datacollection.views;

import org.eclipse.jface.action.Action;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.IToolBarManager;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;
import org.netxms.client.NXCSession;
import org.netxms.client.Table;
import org.netxms.client.objects.AbstractObject;
import org.netxms.nxmc.Registry;
import org.netxms.nxmc.base.actions.ExportToCsvAction;
import org.netxms.nxmc.base.views.View;
import org.netxms.nxmc.localization.LocalizationHelper;
import org.netxms.nxmc.modules.datacollection.widgets.SummaryTableWidget;
import org.netxms.nxmc.modules.objects.views.AdHocObjectView;
import org.netxms.nxmc.resources.ResourceManager;
import org.xnap.commons.i18n.I18n;

/**
 * Display results of table tool execution
 */
public class SummaryTable extends AdHocObjectView
{
	public static final String ID = "SummaryTable"; //$NON-NLS-1$
   private static I18n i18n = LocalizationHelper.getI18n(PerformanceView.class);
	
	private NXCSession session;
	private int tableId;
	private long baseObjectId;
	private SummaryTableWidget viewer;
	private Action actionExportAllToCsv;

   /**
    * @param name
    * @param image
    */
   public SummaryTable(int tableId, long baseObjectId, long contextId)
   {
      super(i18n.tr("Summary Table"), ResourceManager.getImageDescriptor("icons/config-views/summary_table.png"), "SummaryTable", baseObjectId, contextId,  false);
      session = Registry.getSession();
      this.baseObjectId = baseObjectId;
      this.tableId = tableId;
   }

   /**
    * Create agent configuration editor for cloning.
    */
   protected SummaryTable()
   {
      super(null, null, null, 0, 0, false); 
   }

   /**
    * @see org.netxms.nxmc.base.views.ViewWithContext#cloneView()
    */
   @Override
   public View cloneView()
   {
      SummaryTable view = (SummaryTable)super.cloneView();
      view.tableId = tableId;
      view.baseObjectId = baseObjectId;
      return view;
   }     

   /**
    * Post clone action
    */
   protected void postClone(View origin)
   {    
      super.postClone(origin);
      viewer.refresh();
   }

   /**
    * @see org.netxms.nxmc.base.views.View#createContent(org.eclipse.swt.widgets.Composite)
    */
   @Override
   protected void createContent(Composite parent)
	{
		viewer = new SummaryTableWidget(parent, SWT.NONE, this, tableId, baseObjectId);

		createActions();
	}
	
	/**
	 * Create actions
	 */
	private void createActions()
	{
		actionExportAllToCsv = new ExportToCsvAction(this, viewer.getViewer(), false);
	}	
	
   /**
    * @see org.netxms.nxmc.base.views.View#fillLocalMenu(IMenuManager)
    */
   @Override
   protected void fillLocalMenu(IMenuManager manager)
   {
      manager.add(viewer.getActionUseMultipliers());
      manager.add(actionExportAllToCsv);
   }

   /**
    * @see org.netxms.nxmc.base.views.View#refresh()
    */
   @Override
   public void refresh()
   {
      viewer.refresh();
   }

   /**
    * @see org.netxms.nxmc.base.views.View#fillLocalToolBar(IToolBarManager)
    */
   @Override
   protected void fillLocalToolBar(IToolBarManager manager)
   {
      manager.add(actionExportAllToCsv);
   }
	
	/**
	 * @param table
	 */
	public void setTable(Table table)
	{
		AbstractObject object = session.findObjectById(baseObjectId);
		setName(table.getTitle() + " - " + ((object != null) ? object.getObjectName() : ("[" + baseObjectId + "]")));  //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
		viewer.update(table);
	}
}
