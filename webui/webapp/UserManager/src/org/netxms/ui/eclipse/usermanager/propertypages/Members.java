/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2021 Victor Kirhenshtein
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
package org.netxms.ui.eclipse.usermanager.propertypages;

import java.util.HashMap;
import java.util.Iterator;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.jface.viewers.ViewerComparator;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Label;
import org.eclipse.ui.dialogs.PropertyPage;
import org.eclipse.ui.model.WorkbenchLabelProvider;
import org.netxms.client.NXCSession;
import org.netxms.client.users.AbstractUserObject;
import org.netxms.client.users.UserGroup;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.tools.WidgetHelper;
import org.netxms.ui.eclipse.usermanager.Activator;
import org.netxms.ui.eclipse.usermanager.Messages;
import org.netxms.ui.eclipse.usermanager.dialogs.SelectUserDialog;

/**
 * "Members" page for user group
 */
public class Members extends PropertyPage
{
   private TableViewer userList;
	private NXCSession session;
	private UserGroup object;
	private HashMap<Long, AbstractUserObject> members = new HashMap<Long, AbstractUserObject>(0);

   /**
    * @see org.eclipse.jface.preference.PreferencePage#createContents(org.eclipse.swt.widgets.Composite)
    */
	@Override
	protected Control createContents(Composite parent)
	{
		session = ConsoleSharedData.getSession();
		
		Composite dialogArea = new Composite(parent, SWT.NONE);
		object = (UserGroup)getElement().getAdapter(UserGroup.class);

      if (object.getId() == AbstractUserObject.WELL_KNOWN_ID_EVERYONE)
      {
         GridLayout layout = new GridLayout();
         dialogArea.setLayout(layout);
         Label label = new Label(dialogArea, SWT.CENTER | SWT.WRAP);
         label.setText("This built-in group contains all the users in the system and\nis populated automatically. You can’t add or remove users here.");
         label.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, true));
         return dialogArea;
      }

		GridLayout layout = new GridLayout();
		layout.marginWidth = 0;
		layout.marginHeight = 0;
		dialogArea.setLayout(layout);

      userList = new TableViewer(dialogArea, SWT.BORDER | SWT.MULTI | SWT.FULL_SELECTION);
		userList.setContentProvider(new ArrayContentProvider());
		userList.setLabelProvider(new WorkbenchLabelProvider());
      userList.setComparator(new ViewerComparator() {
         @Override
         public int compare(Viewer viewer, Object e1, Object e2)
         {
            return ((AbstractUserObject)e1).getName().compareToIgnoreCase(((AbstractUserObject)e2).getName());
         }
      });

      GridData gd = new GridData();
      gd.grabExcessHorizontalSpace = true;
      gd.grabExcessVerticalSpace = true;
      gd.horizontalAlignment = SWT.FILL;
      gd.verticalAlignment = SWT.FILL;
      userList.getControl().setLayoutData(gd);

      Composite buttons = new Composite(dialogArea, SWT.NONE);
      FillLayout buttonsLayout = new FillLayout();
      buttonsLayout.spacing = WidgetHelper.INNER_SPACING;
      buttons.setLayout(buttonsLayout);
      gd = new GridData();
      gd.horizontalAlignment = SWT.RIGHT;
      gd.widthHint = 184;
      buttons.setLayoutData(gd);
      
      final Button addButton = new Button(buttons, SWT.PUSH);
      addButton.setText(Messages.get().Members_Add);
      addButton.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}

			@Override
			public void widgetSelected(SelectionEvent e)
			{
				SelectUserDialog dlg = new SelectUserDialog(Members.this.getShell(), AbstractUserObject.class);
				if (dlg.open() == Window.OK)
				{
					AbstractUserObject[] selection = dlg.getSelection();
					for(AbstractUserObject user : selection)
						members.put(user.getId(), user);
					userList.setInput(members.values().toArray(new AbstractUserObject[members.size()]));
				}
			}
      });

      final Button deleteButton = new Button(buttons, SWT.PUSH);
      deleteButton.setText(Messages.get().Members_Delete);
      deleteButton.setEnabled(false);
      deleteButton.addSelectionListener(new SelectionListener() {
			@Override
			public void widgetDefaultSelected(SelectionEvent e)
			{
				widgetSelected(e);
			}

			@SuppressWarnings("unchecked")
			@Override
			public void widgetSelected(SelectionEvent e)
			{
				IStructuredSelection sel = (IStructuredSelection)userList.getSelection();
				Iterator<AbstractUserObject> it = sel.iterator();
				while(it.hasNext())
				{
					AbstractUserObject element = it.next();
					members.remove(element.getId());
				}
				userList.setInput(members.values().toArray());
			}
      });
		
      userList.addSelectionChangedListener(new ISelectionChangedListener() {
			@Override
			public void selectionChanged(SelectionChangedEvent event)
			{
				deleteButton.setEnabled(!userList.getSelection().isEmpty());
			}
      });

      // Initial data
		for(long userId : object.getMembers())
		{
			final AbstractUserObject user = session.findUserDBObjectById(userId, null);
			if (user != null)
			{
				members.put(user.getId(), user);
			}
		}
		userList.setInput(members.values().toArray());

		return dialogArea;
	}
	
	/**
	 * Apply changes
	 * 
	 * @param isApply true if update operation caused by "Apply" button
	 */
	protected void applyChanges(final boolean isApply)
	{
		if (isApply)
			setValid(false);
		
		long[] memberIds = new long[members.size()];
		int i = 0;
		for(Long id : members.keySet())
			memberIds[i++] = id;
		object.setMembers(memberIds);
		
		new ConsoleJob(Messages.get().Members_JobTitle, null, Activator.PLUGIN_ID, null) {
			@Override
			protected void runInternal(IProgressMonitor monitor) throws Exception
			{
				session.modifyUserDBObject(object, AbstractUserObject.MODIFY_MEMBERS);
			}

			@Override
			protected String getErrorMessage()
			{
				return Messages.get().Members_JobError;
			}

			@Override
			protected void jobFinalize()
			{
				if (isApply)
				{
					runInUIThread(new Runnable() {
						@Override
						public void run()
						{
							Members.this.setValid(true);
						}
					});
				}
			}
		}.start();
	}

	/* (non-Javadoc)
	 * @see org.eclipse.jface.preference.PreferencePage#performOk()
	 */
	@Override
	public boolean performOk()
	{
		applyChanges(false);
		return true;
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
