/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2016 Victor Kirhenshtein
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
package org.netxms.ui.eclipse.agentmanager.views;

import java.io.ByteArrayInputStream;
import java.text.SimpleDateFormat;
import java.util.Date;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.action.Action;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.IToolBarManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.jface.commands.ActionHandler;
import org.eclipse.jface.resource.JFaceResources;
import org.eclipse.rap.rwt.RWT;
import org.eclipse.rap.rwt.client.service.JavaScriptExecutor;
import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.ScrolledComposite;
import org.eclipse.swt.events.ControlAdapter;
import org.eclipse.swt.events.ControlEvent;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IPartListener;
import org.eclipse.ui.IViewSite;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.handlers.IHandlerService;
import org.eclipse.ui.part.ViewPart;
import org.netxms.client.NXCSession;
import org.netxms.client.Table;
import org.netxms.ui.eclipse.actions.RefreshAction;
import org.netxms.ui.eclipse.agentmanager.Activator;
import org.netxms.ui.eclipse.agentmanager.Messages;
import org.netxms.ui.eclipse.console.DownloadServiceHandler;
import org.netxms.ui.eclipse.console.resources.SharedIcons;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.tools.WidgetHelper;

/**
 * Screenshot view
 */
public class ScreenshotView extends ViewPart implements IPartListener
{
   public static final String ID = "org.netxms.ui.eclipse.agentmanager.views.ScreenshotView"; //$NON-NLS-1$

   private NXCSession session;
   private long nodeId;
   private String userSession;
   private String userName;
   private Image image;
   private String errorMessage;
   private String imageInfo;
   private ScrolledComposite scroller;
   private Canvas canvas;
   private byte[] byteImage;
   private Action actionRefresh;
   private Action actionAutoRefresh;
   private Action actionSave;
   private boolean autoRefresh;
   private long lastRequestTime;

   /**
    * @see org.eclipse.ui.part.ViewPart#init(org.eclipse.ui.IViewSite)
    */
   @Override
   public void init(IViewSite site) throws PartInitException
   {
      super.init(site);

      session = ConsoleSharedData.getSession();
      String[] params = site.getSecondaryId().split("&");
      nodeId = Long.parseLong(params[0]);
      if(params.length > 1)
      {
         userSession = params[1];
         if(params.length > 2)
            userName = params[2];
         else
            userName = params[1];     
      }

      setPartName(String.format(Messages.get().ScreenshotView_PartTitle, session.getObjectName(nodeId)));
      autoRefresh = false;
   }

   /**
    * @see org.eclipse.ui.part.WorkbenchPart#createPartControl(org.eclipse.swt.widgets.Composite)
    */
   @Override
   public void createPartControl(Composite parent)
   {
      parent.setLayout(new FillLayout());
      
      scroller = new ScrolledComposite(parent, SWT.H_SCROLL | SWT.V_SCROLL);
      
      canvas = new Canvas(scroller, SWT.NONE);
      canvas.addPaintListener(new PaintListener() {
         public void paintControl(PaintEvent e)
         {
            GC gc = e.gc;
            if (image != null)
            {
               gc.drawImage(image, 0, 0);
               gc.drawText(imageInfo, 10, image.getImageData().height + 10);
            }
            else if (errorMessage != null)
            {
               gc.setForeground(canvas.getDisplay().getSystemColor(SWT.COLOR_DARK_RED));
               gc.setFont(JFaceResources.getBannerFont());
               gc.drawText(errorMessage, 10, 10, true);
            }
         }
      });
      
      scroller.setContent(canvas);
      scroller.setExpandVertical(true);
      scroller.setExpandHorizontal(true);
      WidgetHelper.setScrollBarIncrement(scroller, SWT.VERTICAL, 20);
      WidgetHelper.setScrollBarIncrement(scroller, SWT.HORIZONTAL, 20);
      scroller.addControlListener(new ControlAdapter() {
         public void controlResized(ControlEvent e)
         {
            updateScrollerSize();
         }
      });
      getSite().getPage().addPartListener(this);
      
      activateContext();
      createActions();
      contributeToActionBars();
      refresh();
   }

   /**
    * Update scroller size 
    */
   private void updateScrollerSize()
   {
      if (image != null)
      {
         ImageData d = image.getImageData();
         Point pt = WidgetHelper.getTextExtent(canvas, imageInfo);
         scroller.setMinSize(new Point(Math.max(d.width, pt.x + 20), d.height + pt.y + 15));
      }
      else if (errorMessage != null)
      {
         Point pt = WidgetHelper.getTextExtent(canvas, errorMessage);
         scroller.setMinSize(new Point(pt.x + 20, pt.y + 20));
      }
      else
      {
         scroller.setMinSize(new Point(0, 0));
      }
   }
   
   /**
    * Activate context
    */
   private void activateContext()
   {
      IContextService contextService = (IContextService)getSite().getService(IContextService.class);
      if (contextService != null)
      {
         contextService.activateContext("org.netxms.ui.eclipse.agentmanager.context.ScreenshotView"); //$NON-NLS-1$
      }
   }
   
   /**
    * Get new screenshot and refresh image
    */
   public void refresh()
   {
      ConsoleJob job = new ConsoleJob(Messages.get().ScreenshotView_JobTitle, this, Activator.PLUGIN_ID) {
         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {
            try
            {
               if (userSession == null)
               {
                  Table sessions = session.queryAgentTable(nodeId, "Agent.SessionAgents"); //$NON-NLS-1$
                  if ((sessions != null) && (sessions.getRowCount() > 0))
                  {
                     int colIndexName = sessions.getColumnIndex("SESSION_NAME"); //$NON-NLS-1$
                     int colIndexUser = sessions.getColumnIndex("USER_NAME"); //$NON-NLS-1$
                     for(int i = 0; i < sessions.getRowCount(); i++)
                     {
                        String n = sessions.getCellValue(i, colIndexName);
                        if ("Console".equalsIgnoreCase(n)) //$NON-NLS-1$
                        {
                           userSession = n;
                           userName = sessions.getCellValue(i, colIndexUser);
                           break;
                        }
                     }
                     
                     if (userSession == null)
                     {
                        // Console session not found, use first available
                        userSession = sessions.getCellValue(0, colIndexName);
                        userName = sessions.getCellValue(0, colIndexUser);
                     }
                  }
               }
               
               if (userSession == null)
               {
                  // Cannot find any connected sessions
                  runInUIThread(new Runnable() {
                     @Override
                     public void run()
                     {
                        if (canvas.isDisposed())
                           return;

                        if (image != null)
                           image.dispose();
                        
                        image = null;
                        errorMessage = Messages.get().ScreenshotView_ErrorNoActiveSessions;
                        canvas.redraw();
                        
                        actionSave.setEnabled(false);
                        
                        updateScrollerSize();
                     }
                  });
                  return;
               }
               
               lastRequestTime = System.currentTimeMillis();
               byteImage = session.takeScreenshot(nodeId, userSession);
               final ImageData data = new ImageData(new ByteArrayInputStream(byteImage));
               runInUIThread(new Runnable() {
                  @Override
                  public void run()
                  {
                     if (canvas.isDisposed())
                        return;

                     if (image != null)
                        image.dispose();
                     
                     image = new Image(getDisplay(), data);
                     imageInfo = userName + "@" + userSession; //$NON-NLS-1$
                     errorMessage = null;
                     canvas.redraw();

                     actionSave.setEnabled(true);

                     updateScrollerSize();
                  }
               });
            }
            catch(Exception e)
            {
               byteImage = null;
               final String emsg = e.getLocalizedMessage();
               runInUIThread(new Runnable() {
                  @Override
                  public void run()
                  {
                     if (canvas.isDisposed())
                        return;

                     if (image != null)
                        image.dispose();
                     
                     image = null;
                     errorMessage = (emsg != null) ? String.format(Messages.get().ScreenshotView_ErrorWithMsg, emsg) : Messages.get().ScreenshotView_ErrorWithoutMsg;
                     canvas.redraw();

                     actionSave.setEnabled(false);

                     updateScrollerSize();
                  }
               });
            }
            if (autoRefresh)
            {
               long diff = System.currentTimeMillis() - lastRequestTime;
               if (diff < 250)
                  Thread.sleep(250 - diff);               
               runInUIThread(new Runnable() {
                  @Override
                  public void run()
                  {                     
                     if (autoRefresh && !canvas.isDisposed() && getSite().getPage().isPartVisible(ScreenshotView.this))
                     {
                        refresh();
                     }
                  }
               });
            }
         }

         @Override
         protected String getErrorMessage()
         {
            return Messages.get().ScreenshotView_JobError;
         }
      };
      job.setUser(false);
      job.start();
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
            refresh();
         }
      };
      
      actionAutoRefresh = new Action("Auto refresh", Action.AS_CHECK_BOX) {
         @Override
         public void run()
         {
            autoRefresh = actionAutoRefresh.isChecked();
            if (autoRefresh)
            {
               refresh();
            }
         }
      };
      actionAutoRefresh.setChecked(autoRefresh);

      actionSave = new Action(Messages.get().ScreenshotView_Save, SharedIcons.SAVE) {
         @Override
         public void run()
         {
            saveImage();
         }
      };
      actionSave.setActionDefinitionId("org.netxms.ui.eclipse.agentmanager.commands.save_screenshot"); //$NON-NLS-1$
      handlerService.activateHandler(actionSave.getActionDefinitionId(), new ActionHandler(actionSave));
      actionSave.setEnabled(false);
   }

   /**
    * Save image to file
    */
   private void saveImage()
   {
      String id = Long.toString(System.currentTimeMillis());
      DownloadServiceHandler.addDownload(id, session.getObjectName(nodeId) + "-screenshot-" + new SimpleDateFormat("yyyyMMdd-HHmmss").format(new Date()) + ".png", byteImage, "application/octet-stream"); //$NON-NLS-1$ //$NON-NLS-2$
      JavaScriptExecutor executor = RWT.getClient().getService(JavaScriptExecutor.class);
      if (executor != null) 
      {
         StringBuilder js = new StringBuilder();
         js.append("var hiddenIFrameID = 'hiddenDownloader',"); //$NON-NLS-1$
         js.append("   iframe = document.getElementById(hiddenIFrameID);"); //$NON-NLS-1$
         js.append("if (iframe === null) {"); //$NON-NLS-1$
         js.append("   iframe = document.createElement('iframe');"); //$NON-NLS-1$
         js.append("   iframe.id = hiddenIFrameID;"); //$NON-NLS-1$
         js.append("   iframe.style.display = 'none';"); //$NON-NLS-1$
         js.append("   document.body.appendChild(iframe);"); //$NON-NLS-1$
         js.append("}"); //$NON-NLS-1$
         js.append("iframe.src = '"); //$NON-NLS-1$
         js.append(DownloadServiceHandler.createDownloadUrl(id));
         js.append("';"); //$NON-NLS-1$
         executor.execute(js.toString());
      }
   }

   /**
    * @see org.eclipse.ui.part.WorkbenchPart#setFocus()
    */
   @Override
   public void setFocus()
   {
      canvas.setFocus();
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
      manager.add(actionSave);
      manager.add(new Separator());
      manager.add(actionRefresh);
      manager.add(actionAutoRefresh);
   }

   /**
    * Fill local tool bar
    * 
    * @param manager
    *           Menu manager for local toolbar
    */
   private void fillLocalToolBar(IToolBarManager manager)
   {
      manager.add(actionSave);
      manager.add(new Separator());
      manager.add(actionRefresh);
   }

   /**
    * @see org.eclipse.ui.part.WorkbenchPart#dispose()
    */
   @Override
   public void dispose()
   {
      getSite().getPage().removePartListener(this);
      if (image != null)
         image.dispose();
      super.dispose();
   }

   /**
    * @see org.eclipse.ui.IPartListener#partActivated(org.eclipse.ui.IWorkbenchPart)
    */
   @Override
   public void partActivated(IWorkbenchPart part)
   {
      if (autoRefresh && getSite().getPage().isPartVisible(ScreenshotView.this))
      {
         refresh();
      }
   }

   /**
    * @see org.eclipse.ui.IPartListener#partBroughtToTop(org.eclipse.ui.IWorkbenchPart)
    */
   @Override
   public void partBroughtToTop(IWorkbenchPart part)
   {
      if (autoRefresh && getSite().getPage().isPartVisible(ScreenshotView.this))
      {
         refresh();
      }
   }

   /**
    * @see org.eclipse.ui.IPartListener#partClosed(org.eclipse.ui.IWorkbenchPart)
    */
   @Override
   public void partClosed(IWorkbenchPart part)
   {
   }

   /**
    * @see org.eclipse.ui.IPartListener#partDeactivated(org.eclipse.ui.IWorkbenchPart)
    */
   @Override
   public void partDeactivated(IWorkbenchPart part)
   {      
   }

   /**
    * @see org.eclipse.ui.IPartListener#partOpened(org.eclipse.ui.IWorkbenchPart)
    */
   @Override
   public void partOpened(IWorkbenchPart part)
   {
   }
}
