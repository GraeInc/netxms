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
package org.netxms.ui.eclipse.dashboard.views;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.action.Action;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.IToolBarManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.jface.commands.ActionHandler;
import org.eclipse.jface.util.IPropertyChangeListener;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.ISaveablePart;
import org.eclipse.ui.IViewSite;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.handlers.IHandlerService;
import org.eclipse.ui.part.ViewPart;
import org.netxms.client.NXCSession;
import org.netxms.client.SessionListener;
import org.netxms.client.SessionNotification;
import org.netxms.client.constants.UserAccessRights;
import org.netxms.client.dashboards.DashboardElement;
import org.netxms.client.datacollection.DciDataRow;
import org.netxms.client.objects.Dashboard;
import org.netxms.ui.eclipse.actions.RefreshAction;
import org.netxms.ui.eclipse.console.DownloadServiceHandler;
import org.netxms.ui.eclipse.console.resources.RegionalSettings;
import org.netxms.ui.eclipse.console.resources.SharedIcons;
import org.netxms.ui.eclipse.dashboard.Activator;
import org.netxms.ui.eclipse.dashboard.ElementCreationHandler;
import org.netxms.ui.eclipse.dashboard.ElementCreationMenuManager;
import org.netxms.ui.eclipse.dashboard.Messages;
import org.netxms.ui.eclipse.dashboard.widgets.DashboardControl;
import org.netxms.ui.eclipse.dashboard.widgets.ElementWidget;
import org.netxms.ui.eclipse.dashboard.widgets.LineChartElement;
import org.netxms.ui.eclipse.dashboard.widgets.LineChartElement.DataCacheElement;
import org.netxms.ui.eclipse.dashboard.widgets.internal.DashboardModifyListener;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.tools.IntermediateSelectionProvider;
import org.netxms.ui.eclipse.tools.MessageDialogHelper;

/**
 * Dashboard view
 */
public class DashboardView extends ViewPart implements ISaveablePart
{
	public static final String ID = "org.netxms.ui.eclipse.dashboard.views.DashboardView"; //$NON-NLS-1$

	private NXCSession session;
	private SessionListener clientListener;
	private Dashboard dashboard;
   private long contextObjectId;
	private boolean readOnly = true;
	private IntermediateSelectionProvider selectionProvider;
	private boolean fullScreenDisplay = false;
	private Shell fullScreenDisplayShell;
   private Composite parentComposite;
	private DashboardControl dbc;
	private DashboardModifyListener dbcModifyListener;
	private Action actionRefresh;
	private Action actionEditMode;
   private Action actionAddColumn;
   private Action actionRemoveColumn;
	private Action actionSave;
	private Action actionExportValues;
	private Action actionFullScreenDisplay;

   /**
    * @see org.eclipse.ui.part.ViewPart#init(org.eclipse.ui.IViewSite)
    */
	@Override
	public void init(IViewSite site) throws PartInitException
	{
		super.init(site);
      session = ConsoleSharedData.getSession();
      String[] parts = site.getSecondaryId().split("&");
      dashboard = session.findObjectById(Long.parseLong(parts[0]), Dashboard.class);
		if (dashboard == null)
			throw new PartInitException(Messages.get().DashboardView_InitError);
      contextObjectId = (parts.length > 1) ? Long.parseLong(parts[1]) : 0;
		setPartName(Messages.get().DashboardView_PartNamePrefix + dashboard.getObjectName());
	}

   /**
    * @see org.eclipse.ui.part.WorkbenchPart#createPartControl(org.eclipse.swt.widgets.Composite)
    */
	@Override
	public void createPartControl(Composite parent)
	{
	   selectionProvider = new IntermediateSelectionProvider();
	   getSite().setSelectionProvider(selectionProvider);

      parentComposite = parent;

      clientListener = new SessionListener() {         
         @Override
         public void notificationHandler(SessionNotification n)
         {
            if (n.getCode() == SessionNotification.OBJECT_CHANGED && dashboard.getObjectId() == n.getSubCode())
            {
               parentComposite.getDisplay().asyncExec(new Runnable() {
                  @Override
                  public void run()
                  {
                     rebuildDashboard(true);
                  }
               });
            }
         }
      };
      
      dbc = new DashboardControl(parent, SWT.NONE, dashboard, (contextObjectId != 0) ? session.findObjectById(contextObjectId) : null, this, selectionProvider, false);

      activateContext();
      createActions();

	   ConsoleJob job = new ConsoleJob(Messages.get().DashboardView_GetEffectiveRights, this, Activator.PLUGIN_ID) {
         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {
            readOnly = ((dashboard.getEffectiveRights() & UserAccessRights.OBJECT_ACCESS_MODIFY) == 0);
            runInUIThread(new Runnable() {
               @Override
               public void run()
               {
                  if (!readOnly)
                  {
                     dbcModifyListener = new DashboardModifyListener() {
                        @Override
                        public void save()
                        {
                           actionSave.setEnabled(false);
                           firePropertyChange(PROP_DIRTY);
                        }

                        @Override
                        public void modify()
                        {
                           actionSave.setEnabled(true);
                           firePropertyChange(PROP_DIRTY);
                        }
                     };
                     dbc.setModifyListener(dbcModifyListener);
                  }

                  contributeToActionBars();
                  getViewSite().getActionBars().updateActionBars();

                  session.addListener(clientListener);
               }
            });
         }

         @Override
         protected String getErrorMessage()
         {
            return Messages.get().DashboardView_GetEffectiveRightsError;
         }
      };
      job.start();

      addPartPropertyListener(new IPropertyChangeListener() {
         @Override
         public void propertyChange(PropertyChangeEvent event)
         {
            if (event.getProperty().equals("FullScreen"))
            {
               boolean enable = Boolean.parseBoolean(event.getNewValue().toString());
               actionFullScreenDisplay.setChecked(enable);
               enableFullScreenDisplay(enable);
            }
         }
      });
	}

   /**
    * @see org.eclipse.ui.part.WorkbenchPart#dispose()
    */
   @Override
   public void dispose()
   {
      if ((fullScreenDisplayShell != null) && !fullScreenDisplayShell.isDisposed())
      {
         fullScreenDisplayShell.close();
         fullScreenDisplayShell.dispose();
      }
      if ((session != null) && (clientListener != null))
         session.removeListener(clientListener);
      super.dispose();
   }

   /**
    * Activate context
    */
   private void activateContext()
   {
      IContextService contextService = (IContextService)getSite().getService(IContextService.class);
      if (contextService != null)
      {
         contextService.activateContext("org.netxms.ui.eclipse.dashboard.context.DashboardView"); //$NON-NLS-1$
      }
   }

	/**
	 * Create actions
	 */
	private void createActions()
	{
      final IHandlerService handlerService = (IHandlerService)getSite().getService(IHandlerService.class);

		actionRefresh = new RefreshAction(this) {
			@Override
			public void run()
			{
				if (dbc.isModified())
				{
					if (!MessageDialogHelper.openConfirm(getSite().getShell(), Messages.get().DashboardView_Refresh, 
							Messages.get().DashboardView_Confirmation))
						return;
				}
				rebuildDashboard(true);
			}
		};

		actionSave = new Action(Messages.get().DashboardView_Save, SharedIcons.SAVE) {
			@Override
			public void run()
			{
				dbc.saveDashboard(DashboardView.this);
			}
		};
		actionSave.setEnabled(false);

      actionExportValues = new Action(Messages.get().DashboardView_ExportLineChartValues, Activator.getImageDescriptor("icons/export-data.png")) {
         @Override
         public void run()
         {
            exportLineChartValues();
         }
		};
		actionExportValues.setActionDefinitionId("org.netxms.ui.eclipse.dashboard.commands.export_line_chart_values"); //$NON-NLS-1$
      handlerService.activateHandler(actionExportValues.getActionDefinitionId(), new ActionHandler(actionExportValues));

		actionEditMode = new Action(Messages.get().DashboardView_EditMode, Action.AS_CHECK_BOX) {
			@Override
			public void run()
			{
				dbc.setEditMode(!dbc.isEditMode());
				actionEditMode.setChecked(dbc.isEditMode());
				if (!dbc.isEditMode())
					rebuildDashboard(false);
			}
		};
		actionEditMode.setImageDescriptor(SharedIcons.EDIT);
		actionEditMode.setChecked(dbc.isEditMode());
		
      actionAddColumn = new Action("Add &column", Activator.getImageDescriptor("icons/add-column.png")) {
         @Override
         public void run()
         {
            dbc.addColumn();
            if (dbc.getColumnCount() == 128)
               actionAddColumn.setEnabled(false);
            actionRemoveColumn.setEnabled(true);
         }
      };
      actionAddColumn.setEnabled(dashboard.getNumColumns() < 128);

      actionRemoveColumn = new Action("&Remove column", Activator.getImageDescriptor("icons/remove-column.png")) {
         @Override
         public void run()
         {
            dbc.removeColumn();
            if (dbc.getColumnCount() == dbc.getMinimalColumnCount())
               actionRemoveColumn.setEnabled(false);
            actionAddColumn.setEnabled(true);
         }
      };
      actionRemoveColumn.setEnabled(dashboard.getNumColumns() > dbc.getMinimalColumnCount());

		actionFullScreenDisplay = new Action("&Full screen display", Action.AS_CHECK_BOX) {
         @Override
         public void run()
         {
            enableFullScreenDisplay(actionFullScreenDisplay.isChecked());
         }
		};
		actionFullScreenDisplay.setImageDescriptor(Activator.getImageDescriptor("icons/full-screen.png"));
		actionFullScreenDisplay.setActionDefinitionId("org.netxms.ui.eclipse.dashboard.commands.full_screen"); //$NON-NLS-1$
      handlerService.activateHandler(actionFullScreenDisplay.getActionDefinitionId(), new ActionHandler(actionFullScreenDisplay));
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
	private void fillLocalPullDown(IMenuManager manager)
	{
	   if (!readOnly)
	   {
   		manager.add(actionEditMode);
   		manager.add(actionSave);
   		manager.add(new Separator());
         manager.add(new ElementCreationMenuManager("&Add", new ElementCreationHandler() {
            @Override
            public void elementCreated(DashboardElement element)
            {
               dbc.addElement(element);
            }
         }));
   		manager.add(new Separator());
         manager.add(actionAddColumn);
         manager.add(actionRemoveColumn);
         manager.add(new Separator());
	   }
      manager.add(actionExportValues);
      manager.add(new Separator());
      manager.add(actionFullScreenDisplay);
      manager.add(new Separator());
		manager.add(actionRefresh);
	}

	/**
	 * Fill local tool bar
	 * 
	 * @param manager
	 *           Menu manager for local toolbar
	 */
	private void fillLocalToolBar(IToolBarManager manager)
	{
	   if (!readOnly)
	   {
   		manager.add(actionEditMode);
   		manager.add(actionSave);
   		manager.add(new Separator());
         manager.add(actionAddColumn);
         manager.add(actionRemoveColumn);
         manager.add(new Separator());
	   }
	   manager.add(actionExportValues);
      manager.add(new Separator());
      manager.add(actionFullScreenDisplay);
      manager.add(new Separator());
		manager.add(actionRefresh);
	}

   /**
    * @see org.eclipse.ui.part.WorkbenchPart#setFocus()
    */
	@Override
	public void setFocus()
	{
      if (!dbc.isDisposed())
         dbc.setFocus();
	}

   /**
    * @see org.eclipse.ui.ISaveablePart#doSave(org.eclipse.core.runtime.IProgressMonitor)
    */
	@Override
	public void doSave(IProgressMonitor monitor)
	{
		dbc.saveDashboard(this);
	}

   /**
    * @see org.eclipse.ui.ISaveablePart#doSaveAs()
    */
	@Override
	public void doSaveAs()
	{
	}

   /**
    * @see org.eclipse.ui.ISaveablePart#isDirty()
    */
	@Override
	public boolean isDirty()
	{
		return dbc.isModified() && !readOnly;
	}

   /**
    * @see org.eclipse.ui.ISaveablePart#isSaveAsAllowed()
    */
	@Override
	public boolean isSaveAsAllowed()
	{
		return false;
	}

   /**
    * @see org.eclipse.ui.ISaveablePart#isSaveOnCloseNeeded()
    */
	@Override
	public boolean isSaveOnCloseNeeded()
	{
		return dbc.isModified() && !readOnly;
	}

	/**
	 * Rebuild current dashboard
	 * 
	 * @param reload if true, dashboard object will be reloaded and all unsaved changes
	 * will be lost
	 */
	private void rebuildDashboard(boolean reload)
	{
		if (dashboard == null)
			return;

		if (dbc != null)
			dbc.dispose();

		if (reload)
		{
         dashboard = ConsoleSharedData.getSession().findObjectById(dashboard.getObjectId(), Dashboard.class);

			if (dashboard != null)
			{
            dbc = new DashboardControl(fullScreenDisplay ? fullScreenDisplayShell : parentComposite, SWT.NONE, dashboard, (contextObjectId != 0) ? session.findObjectById(contextObjectId) : null, this, selectionProvider, false);
				dbc.getParent().layout(true, true);
				setPartName(Messages.get().DashboardView_PartNamePrefix + dashboard.getObjectName());
				if (!readOnly)
				{
				   dbc.setModifyListener(dbcModifyListener);
				}
			}
			else
			{
				dbc = null;
			}
		}
		else
		{
         dbc = new DashboardControl(fullScreenDisplay ? fullScreenDisplayShell : parentComposite, SWT.NONE, dbc);
			dbc.getParent().layout(true, true);
			if (!readOnly)
			{
			   dbc.setModifyListener(dbcModifyListener);
			}
		}

		actionSave.setEnabled((dbc != null) ? dbc.isModified() : false);
      actionAddColumn.setEnabled(dbc.getColumnCount() < 128);
      actionRemoveColumn.setEnabled(dbc.getColumnCount() > dbc.getMinimalColumnCount());
		firePropertyChange(PROP_DIRTY);
	}

	/**
	 * Export all line chart values as CSV
	 */
	private void exportLineChartValues()
	{
      final List<DataCacheElement> data = new ArrayList<DataCacheElement>();
      for(ElementWidget w : dbc.getElementWidgets())
      {
         if (!(w instanceof LineChartElement))
            continue;

         for(DataCacheElement d : ((LineChartElement)w).getDataCache())
         {
            data.add(d);
         }
      }

      final DateFormat dfDate = RegionalSettings.getDateFormat();
      final DateFormat dfTime = RegionalSettings.getTimeFormat();
      final DateFormat dfDateTime = RegionalSettings.getDateTimeFormat();
	   
	   new ConsoleJob(Messages.get().DashboardView_ExportLineChartData, this, Activator.PLUGIN_ID, null) {
         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {
            boolean doInterpolation = session.getPublicServerVariableAsBoolean("Client.DashboardDataExport.EnableInterpolation"); //$NON-NLS-1$
            
            // Build combined time series
            // Time stamps in series are reversed - latest value first
            List<Date> combinedTimeSeries = new ArrayList<Date>();
            for(DataCacheElement d : data)
            {
               for(DciDataRow r : d.data.getValues())
               {
                  int i;
                  for(i = 0; i < combinedTimeSeries.size(); i++)
                  {
                     if (combinedTimeSeries.get(i).getTime() == r.getTimestamp().getTime())
                        break;                 
                     if (combinedTimeSeries.get(i).getTime() < r.getTimestamp().getTime())
                     {
                        combinedTimeSeries.add(i, r.getTimestamp());
                        break;
                     }
                  }
                  if (i == combinedTimeSeries.size())
                     combinedTimeSeries.add(r.getTimestamp());
               }
            }
            
            List<Double[]> combinedData = new ArrayList<Double[]>(data.size());
            
            // insert missing values
            for(DataCacheElement d : data)
            {
               Double[] ySeries = new Double[combinedTimeSeries.size()];
               int combinedIndex = 0;
               double lastValue = 0;
               long lastTimestamp = 0;
               DciDataRow[] values = d.data.getValues();
               for(int i = 0; i < values.length; i++)
               {
                  Date currentTimestamp = values[i].getTimestamp();
                  double currentValue = values[i].getValueAsDouble();
                  long currentCombinedTimestamp = combinedTimeSeries.get(combinedIndex).getTime();
                  while(currentCombinedTimestamp > currentTimestamp.getTime())
                  {
                     if ((lastTimestamp != 0) && doInterpolation)
                     {
                        // do linear interpolation for missed value
                        ySeries[combinedIndex] = lastValue + (currentValue - lastValue) * ((double)(lastTimestamp - currentCombinedTimestamp) / (double)(lastTimestamp - currentTimestamp.getTime()));
                     }
                     else
                     {
                        ySeries[combinedIndex] = null;
                     }
                     combinedIndex++;
                     currentCombinedTimestamp = combinedTimeSeries.get(combinedIndex).getTime();
                  }
                  ySeries[combinedIndex++] = currentValue;
                  lastTimestamp = currentTimestamp.getTime();
                  lastValue = currentValue;
               }
               combinedData.add(ySeries);
            }

            final File tmpFile = File.createTempFile("ExportDashboardCSV_" + DashboardView.this.hashCode(), "_" + System.currentTimeMillis());
            BufferedWriter writer = new BufferedWriter(new FileWriter(tmpFile));
            try
            {
               writer.write("# " + dashboard.getObjectName() + " " + dfDateTime.format(new Date())); //$NON-NLS-1$ //$NON-NLS-2$
               writer.newLine();
               writer.write("DATE,TIME"); //$NON-NLS-1$
               for(DataCacheElement d : data)
               {
                  writer.write(',');
                  writer.write(d.name);
               }
               writer.newLine();
               
               for(int i = combinedTimeSeries.size() - 1; i >= 0; i--)
               {
                  Date d = combinedTimeSeries.get(i);
                  writer.write(dfDate.format(d));
                  writer.write(',');
                  writer.write(dfTime.format(d));
                  for(Double[] values : combinedData)
                  {
                     writer.write(',');
                     if (values[i] != null)
                     {
                        double v = values[i];
                        if (Math.abs(v) > 0.001)
                           writer.write(String.format("%.3f", v)); //$NON-NLS-1$
                        else
                           writer.write(Double.toString(v));
                     }
                  }  
                  writer.newLine();
               }
            }
            finally
            {
               writer.close();
            }

            DownloadServiceHandler.addDownload(tmpFile.getName(), dashboard.getObjectName() + ".csv", tmpFile, "text/csv");
            runInUIThread(new Runnable() {
               @Override
               public void run()
               {
                  DownloadServiceHandler.startDownload(tmpFile.getName());
               }
            });
         }
         
         @Override
         protected String getErrorMessage()
         {
            return Messages.get().DashboardView_8;
         }
      }.start();
	}
	
	/**
	 * Enable/disable full screen display
	 * 
	 * @param enable true to show dashboard on full screen
	 */
   private void enableFullScreenDisplay(boolean enable)
   {
      if (enable)
      {
         if (fullScreenDisplayShell == null)
         {
            fullScreenDisplayShell = new Shell(Display.getCurrent(), SWT.SHELL_TRIM);
            fullScreenDisplayShell.setLayout(new FillLayout());
            fullScreenDisplayShell.open();
            fullScreenDisplayShell.setFullScreen(true);
            fullScreenDisplayShell.addKeyListener(new KeyListener() {
               @Override
               public void keyReleased(KeyEvent e)
               {
               }

               @Override
               public void keyPressed(KeyEvent e)
               {
                  if ((e.keyCode == SWT.F11) && ((e.stateMask & (SWT.ALT | SWT.COMMAND)) != 0))
                  {
                     actionFullScreenDisplay.setChecked(false);
                     enableFullScreenDisplay(false);
                  }
               }
            });
         }
      }
      else
      {
         if (fullScreenDisplayShell != null)
         {
            if (!fullScreenDisplayShell.isDisposed())
            {
               fullScreenDisplayShell.close();
               fullScreenDisplayShell.dispose();
            }
            fullScreenDisplayShell = null;
         }
      }
      fullScreenDisplay = enable;
      rebuildDashboard(false);
      if (!enable)
         parentComposite.setFocus();
   }
}
