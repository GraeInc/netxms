/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2013 Victor Kirhenshtein
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
package org.netxms.ui.eclipse.views;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.action.Action;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.IToolBarManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.part.ViewPart;
import org.netxms.client.NXCSession;
import org.netxms.ui.eclipse.console.Activator;
import org.netxms.ui.eclipse.console.Messages;
import org.netxms.ui.eclipse.console.resources.SharedIcons;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.widgets.AbstractTraceWidget;

/**
 * Abstract trace view (like event trace)
 */
public abstract class AbstractTraceView extends ViewPart
{
   protected NXCSession session = ConsoleSharedData.getSession();
   
	private AbstractTraceWidget traceWidget;
	private Action actionClear;
	
	/* (non-Javadoc)
	 * @see org.eclipse.ui.part.WorkbenchPart#createPartControl(org.eclipse.swt.widgets.Composite)
	 */
	@Override
	public void createPartControl(Composite parent)
	{
		traceWidget = createTraceWidget(parent);
		
		createActions();
		contributeToActionBars();
		
		activateContext();
	}
	
	/**
	 * @param parent
	 * @return trace widget to be used
	 */
	protected abstract AbstractTraceWidget createTraceWidget(Composite parent);
	
	/**
	 * @return
	 */
	protected AbstractTraceWidget getTraceWidget()
	{
		return traceWidget;
	}

	/**
	 * Activate context
	 */
	protected void activateContext()
	{
		IContextService contextService = (IContextService)getSite().getService(IContextService.class);
		if (contextService != null)
		{
			contextService.activateContext("org.netxms.ui.eclipse.library.context.AbstractTraceView"); //$NON-NLS-1$
		}
	}
	
	/**
	 * Create actions
	 */
	protected void createActions()
	{
		actionClear = new Action(Messages.get().AbstractTraceView_Clear, SharedIcons.CLEAR_LOG) {
			@Override
			public void run()
			{
				traceWidget.clear();
			}
		};
	}
	
	/**
	 * Contribute actions to action bar
	 */
	private void contributeToActionBars()
	{
		IActionBars bars = getViewSite().getActionBars();
		fillLocalPullDown(bars.getMenuManager());
		fillLocalToolBar(bars.getToolBarManager());
	}

	/**
	 * Fill local pull-down menu
	 * 
	 * @param manager
	 *           Menu manager for pull-down menu
	 */
	protected void fillLocalPullDown(IMenuManager manager)
	{
		manager.add(traceWidget.getActionShowFilter());
		manager.add(new Separator());
		manager.add(traceWidget.getActionPause());
		manager.add(actionClear);
		manager.add(new Separator());
		manager.add(traceWidget.getActionCopy());
	}

	/**
	 * Fill local tool bar
	 * 
	 * @param manager
	 *           Menu manager for local toolbar
	 */
	protected void fillLocalToolBar(IToolBarManager manager)
	{
		manager.add(traceWidget.getActionPause());
		manager.add(actionClear);
	}

	/* (non-Javadoc)
	 * @see org.eclipse.ui.part.WorkbenchPart#setFocus()
	 */
	@Override
	public void setFocus()
	{
		traceWidget.setFocus();
	}
	
	/**
	 * Subscribe to channel
	 * 
	 * @param channel
	 */
	protected void subscribe(final String channel)
	{
      new ConsoleJob(String.format("Subscribing to channel %s", channel), this, Activator.PLUGIN_ID, null) {
         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {
            session.subscribe(channel);
         }
         
         @Override
         protected String getErrorMessage()
         {
            return String.format("Cannot subscribe to channel %s", channel);
         }
      }.start();
	}
	
	/**
	 * Unsubscribe from channel
	 * 
	 * @param channel
	 */
	protected void unsubscribe(final String channel)
	{
      ConsoleJob job = new ConsoleJob(String.format("Unsubscribing from channel %s", channel), null, Activator.PLUGIN_ID, null) {
         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {
            session.unsubscribe(channel);
         }
         
         @Override
         protected String getErrorMessage()
         {
            return String.format("Cannot unsubscribe from channel %s", channel);
         }
      };
      job.setUser(false);
      job.setSystem(true);
      job.start();
	}
}
