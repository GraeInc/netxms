/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2020 Victor Kirhenshtein
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
package org.netxms.ui.eclipse.console;

import java.util.Arrays;
import java.util.Comparator;
import org.eclipse.jface.action.Action;
import org.eclipse.jface.action.ActionContributionItem;
import org.eclipse.jface.action.GroupMarker;
import org.eclipse.jface.action.IContributionItem;
import org.eclipse.jface.action.ICoolBarManager;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.MenuManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.jface.action.ToolBarContributionItem;
import org.eclipse.jface.action.ToolBarManager;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.rap.rwt.RWT;
import org.eclipse.rap.rwt.client.service.UrlLauncher;
import org.eclipse.swt.SWT;
import org.eclipse.ui.IWorkbench;
import org.eclipse.ui.IWorkbenchActionConstants;
import org.eclipse.ui.IWorkbenchCommandConstants;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.actions.ActionFactory.IWorkbenchAction;
import org.eclipse.ui.actions.ContributionItemFactory;
import org.eclipse.ui.application.ActionBarAdvisor;
import org.eclipse.ui.application.IActionBarConfigurer;
import org.eclipse.ui.internal.ActionSetContributionItem;
import org.eclipse.ui.internal.actions.CommandAction;
import org.netxms.base.VersionInfo;
import org.netxms.ui.eclipse.console.resources.GroupMarkers;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.tools.MessageDialogHelper;

/**
 * Action bar advisor for management console
 */
@SuppressWarnings("restriction")
public class ApplicationActionBarAdvisor extends ActionBarAdvisor
{
	private IWorkbenchAction actionExit;
	private Action actionAbout;
   private Action actionOpenManual;
	private IWorkbenchAction actionShowPreferences;
	private IWorkbenchAction actionCustomizePerspective;
	private IWorkbenchAction actionSavePerspective;
	private IWorkbenchAction actionResetPerspective;
	private IWorkbenchAction actionClosePerspective;
	private IWorkbenchAction actionCloseAllPerspectives;
	private IWorkbenchAction actionMinimize;
	private IWorkbenchAction actionMaximize;
   private Action actionClose;
	private IWorkbenchAction actionPrevView;
	private IWorkbenchAction actionNextView;
	private IWorkbenchAction actionShowViewMenu;
	private Action actionOpenProgressView;
	private IContributionItem contribItemOpenPerspective;

	/**
	 * @param configurer
	 */
	public ApplicationActionBarAdvisor(IActionBarConfigurer configurer)
	{
		super(configurer);
	}

   /**
    * @see org.eclipse.ui.application.ActionBarAdvisor#makeActions(org.eclipse.ui.IWorkbenchWindow)
    */
	@Override
	protected void makeActions(final IWorkbenchWindow window)
	{
		contribItemOpenPerspective = ContributionItemFactory.PERSPECTIVES_SHORTLIST.create(window);
		
		actionExit = ActionFactory.QUIT.create(window);
		register(actionExit);

		actionAbout = new Action(String.format(Messages.get().ApplicationActionBarAdvisor_AboutActionName, BrandingManager.getInstance().getConsoleProductName())) {
			@Override
			public void run()
			{
				Dialog dlg = BrandingManager.getInstance().getAboutDialog(window.getShell());
				if (dlg != null)
				{
					dlg.open();
				}
				else
				{
					MessageDialogHelper.openInformation(window.getShell(), 
							Messages.get().ApplicationActionBarAdvisor_AboutTitle, 
							String.format(Messages.get().ApplicationActionBarAdvisor_AboutText, VersionInfo.version()));
				}
			}
		};
		
      actionOpenManual = new Action("Open Administrator &Guide") {
         @Override
         public void run()
         {
            final UrlLauncher launcher = RWT.getClient().getService(UrlLauncher.class);
            launcher.openURL("https://netxms.org/documentation/adminguide/");
         }
      };

		actionShowPreferences = ActionFactory.PREFERENCES.create(window);
		register(actionShowPreferences);

		actionCustomizePerspective = ActionFactory.EDIT_ACTION_SETS.create(window);
		register(actionCustomizePerspective);
		
		actionSavePerspective = ActionFactory.SAVE_PERSPECTIVE.create(window);
		register(actionSavePerspective);
		
		actionResetPerspective = ActionFactory.RESET_PERSPECTIVE.create(window);
		register(actionResetPerspective);
		
		actionClosePerspective = ActionFactory.CLOSE_PERSPECTIVE.create(window);
		register(actionClosePerspective);
		
		actionCloseAllPerspectives = ActionFactory.CLOSE_ALL_PERSPECTIVES.create(window);
		register(actionCloseAllPerspectives);
		
		actionMinimize = ActionFactory.MINIMIZE.create(window);
		register(actionMinimize);
		
		actionMaximize = ActionFactory.MAXIMIZE.create(window);
		register(actionMaximize);
		
		actionClose = new CommandAction(window, IWorkbenchCommandConstants.WINDOW_CLOSE_PART);
      register(actionClose);
		
		actionPrevView = ActionFactory.PREVIOUS_PART.create(window);
		register(actionPrevView);
		
		actionNextView = ActionFactory.NEXT_PART.create(window);
		register(actionNextView);
		
		actionShowViewMenu = ActionFactory.SHOW_VIEW_MENU.create(window);
		register(actionShowViewMenu);
		
		actionOpenProgressView = new Action() {
			@Override
			public void run()
			{
				IWorkbench wb = PlatformUI.getWorkbench();
				if (wb != null)
				{
					IWorkbenchWindow win = wb.getActiveWorkbenchWindow();
					if (win != null)
					{
						IWorkbenchPage page = win.getActivePage();
						if (page != null)
						{
							try
							{
								page.showView("org.eclipse.ui.views.ProgressView"); //$NON-NLS-1$
							}
							catch(PartInitException e)
							{
								e.printStackTrace();
							}
						}
					}
				}
			}
		};
		actionOpenProgressView.setText(Messages.get().ApplicationActionBarAdvisor_Progress);
		actionOpenProgressView.setImageDescriptor(Activator.getImageDescriptor("icons/pview.gif")); //$NON-NLS-1$
	}

   /**
    * @see org.eclipse.ui.application.ActionBarAdvisor#fillMenuBar(org.eclipse.jface.action.IMenuManager)
    */
	@Override
	protected void fillMenuBar(IMenuManager menuBar)
	{
		MenuManager fileMenu = new MenuManager(Messages.get().ApplicationActionBarAdvisor_File, IWorkbenchActionConstants.M_FILE);
		MenuManager viewMenu = new MenuManager(Messages.get().ApplicationActionBarAdvisor_View, GroupMarkers.M_VIEW);
		MenuManager monitorMenu = new MenuManager(Messages.get().ApplicationActionBarAdvisor_Monitor, GroupMarkers.M_MONITOR);
		MenuManager toolsMenu = new MenuManager(Messages.get().ApplicationActionBarAdvisor_Tools, GroupMarkers.M_TOOLS);
		MenuManager windowMenu = new MenuManager(Messages.get().ApplicationActionBarAdvisor_Window, IWorkbenchActionConstants.M_WINDOW);
		MenuManager helpMenu = new MenuManager(Messages.get().ApplicationActionBarAdvisor_Help, IWorkbenchActionConstants.M_HELP);

      MenuManager configMenu = new MenuManager(Messages.get().ApplicationActionBarAdvisor_Configuration, GroupMarkers.M_CONFIG) {
         @Override
         protected void itemAdded(IContributionItem item)
         {
            IContributionItem[] items = getItems();
            Arrays.sort(items, new Comparator<IContributionItem>() {
               @Override
               public int compare(IContributionItem item1, IContributionItem item2)
               {
                  if (item1 instanceof ActionSetContributionItem)
                     item1 = ((ActionSetContributionItem)item1).getInnerItem();
                  if (item2 instanceof ActionSetContributionItem)
                     item2 = ((ActionSetContributionItem)item2).getInnerItem();

                  String n1;
                  if (item1 instanceof ActionContributionItem)
                     n1 = ((ActionContributionItem)item1).getAction().getText().replace("&", "");
                  else
                     n1 = item1.getId();

                  String n2;
                  if (item2 instanceof ActionContributionItem)
                     n2 = ((ActionContributionItem)item2).getAction().getText().replace("&", "");
                  else
                     n2 = item2.getId();

                  return n1.compareToIgnoreCase(n2);
               }
            });
            internalSetItems(items);
            super.itemAdded(item);
         }
      };

		menuBar.add(fileMenu);
		menuBar.add(viewMenu);
		menuBar.add(monitorMenu);
		menuBar.add(configMenu);
		menuBar.add(toolsMenu);

		// Add a group marker indicating where action set menus will appear.
		menuBar.add(new GroupMarker(IWorkbenchActionConstants.MB_ADDITIONS));
		if (!Activator.getDefault().getPreferenceStore().getBoolean("HIDE_WINDOW_MENU")) //$NON-NLS-1$
			menuBar.add(windowMenu);
		menuBar.add(helpMenu);
		
		// File
		fileMenu.add(actionShowPreferences);
		fileMenu.add(new GroupMarker(IWorkbenchActionConstants.MB_ADDITIONS));
		fileMenu.add(new Separator());
		fileMenu.add(actionExit);

		// View
		viewMenu.add(new GroupMarker(GroupMarkers.M_PRODUCT_VIEW));
		viewMenu.add(new Separator());
		viewMenu.add(new GroupMarker(GroupMarkers.M_PRIMARY_VIEW));
		viewMenu.add(new Separator());
		viewMenu.add(new GroupMarker(GroupMarkers.M_LOGS_VIEW));
		viewMenu.add(new Separator());
		viewMenu.add(actionOpenProgressView);
		viewMenu.add(new GroupMarker(GroupMarkers.M_TOOL_VIEW));
		
		// Monitor
		monitorMenu.add(new GroupMarker(IWorkbenchActionConstants.MB_ADDITIONS));

      // Configuration
      configMenu.add(new GroupMarker(IWorkbenchActionConstants.MB_ADDITIONS));
		
		// Tools
		toolsMenu.add(new GroupMarker(IWorkbenchActionConstants.MB_ADDITIONS));
		
		// Window
		MenuManager openPerspectiveMenuMgr = new MenuManager(Messages.get().ApplicationActionBarAdvisor_OpenPerspective, "openPerspective"); //$NON-NLS-1$
		openPerspectiveMenuMgr.add(contribItemOpenPerspective);
		windowMenu.add(openPerspectiveMenuMgr);
		windowMenu.add(new Separator());
		windowMenu.add(actionCustomizePerspective);
		windowMenu.add(actionSavePerspective);
		windowMenu.add(actionResetPerspective);
		windowMenu.add(actionClosePerspective);
		windowMenu.add(actionCloseAllPerspectives);
      windowMenu.add(new Separator());
		
		final MenuManager navMenu = new MenuManager(Messages.get().ApplicationActionBarAdvisor_Navigation, IWorkbenchActionConstants.M_NAVIGATE);
		windowMenu.add(navMenu);
		navMenu.add(actionShowViewMenu);
		navMenu.add(new Separator());
		navMenu.add(actionMaximize);
		navMenu.add(actionMinimize);
      navMenu.add(actionClose);
		navMenu.add(new Separator());
		navMenu.add(actionNextView);
		navMenu.add(actionPrevView);

		// Help
      helpMenu.add(actionOpenManual);
		helpMenu.add(new GroupMarker(IWorkbenchActionConstants.MB_ADDITIONS));
      helpMenu.add(new Separator());
		helpMenu.add(actionAbout);
	}
	
   /**
    * Add toolbar.
    *
    * @param coolBar
    * @param id
    */
	private void addToolBar(ICoolBarManager coolBar, String id)
	{
	   if (coolBar.find(id) != null)
	      return;
	   
      ToolBarManager toolbar = new ToolBarManager(SWT.FLAT | SWT.TRAIL);
      toolbar.add(new GroupMarker(IWorkbenchActionConstants.MB_ADDITIONS));
      coolBar.add(new ToolBarContributionItem(toolbar, id));
      coolBar.setLockLayout(true);
	}

   /**
    * @see org.eclipse.ui.application.ActionBarAdvisor#fillCoolBar(org.eclipse.jface.action.ICoolBarManager)
    */
	@Override
	protected void fillCoolBar(ICoolBarManager coolBar)
	{
		addToolBar(coolBar, "product"); //$NON-NLS-1$
		addToolBar(coolBar, "view"); //$NON-NLS-1$
		addToolBar(coolBar, "logs"); //$NON-NLS-1$
		addToolBar(coolBar, "tools"); //$NON-NLS-1$
		addToolBar(coolBar, "config"); //$NON-NLS-1$

		if (Activator.getDefault().getPreferenceStore().getBoolean("SHOW_SERVER_CLOCK")) //$NON-NLS-1$
		{
		   if (coolBar.find(ServerClockContributionItem.ID) == null)
		      coolBar.add(new ServerClockContributionItem());
		}
		else
		{
		   coolBar.remove(ServerClockContributionItem.ID);
		}
		
		ConsoleSharedData.setProperty("CoolBarManager", coolBar); //$NON-NLS-1$
	}
}
