/**
 * NetXMS - open source network management system
 * Copyright (C) 2019-2020 Raden Solutions
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
package org.netxms.ui.eclipse.datacollection.widgets;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.action.Action;
import org.eclipse.jface.action.IMenuListener;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.IToolBarManager;
import org.eclipse.jface.action.MenuManager;
import org.eclipse.jface.dialogs.IInputValidator;
import org.eclipse.jface.dialogs.InputDialog;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.ui.IViewPart;
import org.netxms.client.AgentPolicy;
import org.netxms.client.NXCSession;
import org.netxms.client.ProgressListener;
import org.netxms.ui.eclipse.console.resources.SharedIcons;
import org.netxms.ui.eclipse.datacollection.Activator;
import org.netxms.ui.eclipse.datacollection.dialogs.FilePermissionsSelectionDialog;
import org.netxms.ui.eclipse.datacollection.widgets.helpers.FileDeliveryPolicy;
import org.netxms.ui.eclipse.datacollection.widgets.helpers.FileDeliveryPolicyComparator;
import org.netxms.ui.eclipse.datacollection.widgets.helpers.FileDeliveryPolicyContentProvider;
import org.netxms.ui.eclipse.datacollection.widgets.helpers.FileDeliveryPolicyLabelProvider;
import org.netxms.ui.eclipse.datacollection.widgets.helpers.PathElement;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.tools.MessageDialogHelper;
import org.netxms.ui.eclipse.widgets.SortableTableViewer;
import org.netxms.ui.eclipse.widgets.SortableTreeViewer;

/**
 * Editor for file delivery policy
 */
public class FileDeliveryPolicyEditor extends AbstractPolicyEditor
{
   public static final int COLUMN_NAME = 0;
   public static final int COLUMN_GUID = 1;
   public static final int COLUMN_DATE = 2;
   public static final int COLUMN_USER = 3;
   public static final int COLUMN_GROUP = 4;
   public static final int COLUMN_PERMISSIONS = 5;
   
   private SortableTreeViewer fileTree;
   private Set<PathElement> rootElements = new HashSet<PathElement>();
   private Action actionAddRoot;
   private Action actionAddDirectory;
   private Action actionAddFile;
   private Action actionDelete;
   private Action actionRename;
   private Action actionUpdate;
   private Action actionChangePermissions;
   private Action actionChangeOwner;
   private Action actionChangeOwnerGroup;
   private Set<String> filesForDeletion = new HashSet<String>();
   private Set<String> notSavedFiles = new HashSet<String>();

   /**
    * @param parent
    * @param style
    */
   public FileDeliveryPolicyEditor(Composite parent, int style, AgentPolicy policy, IViewPart viewPart)
   {
      super(parent, style, policy, viewPart);
      
      setLayout(new FillLayout());
      

      final String[] columnNames = { "Name", "Guid", "Date", "User", "Group", "Permissions" };
      final int[] columnWidths = { 300, 300, 200, 150, 150, 200 };
      fileTree = new SortableTreeViewer(this, columnNames, columnWidths, 0, SWT.UP, SortableTableViewer.DEFAULT_STYLE);
      fileTree.setContentProvider(new FileDeliveryPolicyContentProvider());
      fileTree.setLabelProvider(new FileDeliveryPolicyLabelProvider());
      fileTree.setComparator(new FileDeliveryPolicyComparator());
      
      actionAddRoot = new Action("&Add root directory...", SharedIcons.ADD_OBJECT) {
         @Override
         public void run()
         {
            addElement(null);
         }
      };

      actionAddDirectory = new Action("Add d&irectory...") {
         @Override
         public void run()
         {
            addDirectory();
         }
      };
      
      actionAddFile = new Action("Add &file...") {
         @Override
         public void run()
         {
            addFile();
         }
      };
      
      actionRename = new Action("&Rename...") {
         @Override
         public void run()
         {
            renameElement();
         }
      };
      
      actionDelete = new Action("&Delete") {
         @Override
         public void run()
         {
            deleteElements();
         }
      };
      
      actionUpdate = new Action("&Update...") {
         @Override
         public void run()
         {
            updateFile();
         }
      };
      
      actionChangePermissions = new Action("Change &permissions") {
         @Override
         public void run()
         {
            changePermissions();
         }
      };

      actionChangeOwner = new Action("Change &owner") {
         @Override
         public void run()
         {
            changeOwner();
         }
      };

      actionChangeOwnerGroup = new Action("Change owner &group") {
         @Override
         public void run()
         {
            changeOwnerGroup();
         }
      };

      createPopupMenu();
      
      fileTree.setInput(rootElements);
      updateControlFromPolicy();
   }

   /**
    * Create pop-up menu for user list
    */
   private void createPopupMenu()
   {
      // Create menu manager
      MenuManager menuMgr = new MenuManager();
      menuMgr.setRemoveAllWhenShown(true);
      menuMgr.addMenuListener(new IMenuListener() {
         public void menuAboutToShow(IMenuManager manager)
         {
            fillContextMenu(manager);
         }
      });

      // Create menu
      Menu menu = menuMgr.createContextMenu(fileTree.getControl());
      fileTree.getControl().setMenu(menu);
   }

   /**
    * Fill context menu
    * 
    * @param manager Menu manager
    */
   protected void fillContextMenu(final IMenuManager manager)
   {
      IStructuredSelection selection = fileTree.getStructuredSelection();
      if (selection.isEmpty())
      {
         manager.add(actionAddRoot);
      }
      else if (selection.size() == 1)
      {
         PathElement e = (PathElement)selection.getFirstElement();
         if (e.isFile())
         {
            manager.add(actionUpdate);
         }
         else
         {
            manager.add(actionAddDirectory);
            manager.add(actionAddFile);
         }
         manager.add(actionRename);
         manager.add(actionDelete);
         manager.add(actionChangePermissions);
         manager.add(actionChangeOwner);
         manager.add(actionChangeOwnerGroup);
      }
      else
      {
         manager.add(actionDelete);
      }
   }

   /**
    * @see org.netxms.ui.eclipse.datacollection.widgets.AbstractPolicyEditor#updateControlFromPolicy()
    */
   @Override
   public void updateControlFromPolicy()
   {
      Set<PathElement> newElementSet = new HashSet<PathElement>();
      try
      {
         FileDeliveryPolicy policyData = FileDeliveryPolicy.createFromXml(getPolicy().getContent());
         newElementSet.addAll(Arrays.asList(policyData.elements));
      }
      catch(Exception e)
      {
         Activator.logError("Cannot parse file delivery policy XML", e);
      }

      checkForMissingElements(rootElements, newElementSet, false); 
      checkForMissingElements(newElementSet, rootElements, true); 
      fileTree.refresh(true);
   }

   /**
    * Check for missing elements
    *
    * @param newElements new element set
    * @param originalElements original element set
    * @param createMissing true if missing elements should be created
    */
   private void checkForMissingElements(Set<PathElement> newElements, Set<PathElement> originalElements, boolean createMissing)
   {
      Iterator<PathElement> iter = newElements.iterator();
      while (iter.hasNext())
      {
         PathElement newElement = iter.next();
         PathElement originalElement = null;
         for (PathElement e : originalElements)
         {
            if (newElement.getName().equals(e.getName()))
            {
               originalElement = e;
               break;
            }
         }
         if (originalElement == null)
         {
            if (createMissing)
            {
               fileTree.refresh(newElement, true);
               originalElements.add(newElement);
            }
            else
            {
               iter.remove();
            }
         }
         else if (newElement.isFile() != originalElement.isFile() && createMissing)
         {
            originalElements.add(newElement);
         }
         else if (!newElement.isFile())
         {            
            checkForMissingElements(newElement.getChildrenSet(), originalElement.getChildrenSet(), createMissing);
         }
      }
   }

   /**
    * @see org.netxms.ui.eclipse.datacollection.widgets.AbstractPolicyEditor#updatePolicyFromControl()
    */
   @Override
   public AgentPolicy updatePolicyFromControl()
   {
      FileDeliveryPolicy data = new FileDeliveryPolicy();
      data.elements = rootElements.toArray(new PathElement[rootElements.size()]);
      try
      {
         getPolicy().setContent(data.createXml());
      }
      catch(Exception e)
      {
         Activator.logError("Error serializing file delivery policy", e);
      }
      return getPolicy();
   }

   /**
    * Add new directory to currently selected element
    */
   private void addDirectory()
   {
      IStructuredSelection selection = fileTree.getStructuredSelection();
      if ((selection.size() != 1) || ((PathElement)selection.getFirstElement()).isFile())
         return;
      
      addElement((PathElement)selection.getFirstElement());
   }

   /**
    * Add file
    */
   private void addFile()
   {
      IStructuredSelection selection = fileTree.getStructuredSelection();
      if ((selection.size() != 1) || ((PathElement)selection.getFirstElement()).isFile())
         return;

      FileDialog dlg = new FileDialog(getShell(), SWT.OPEN | SWT.MULTI);
      if (dlg.open() == null)
         return;

      final List<PathElement> uploadList = new ArrayList<PathElement>();
      for(String name : dlg.getFileNames())
      {
         File f = new File(dlg.getFilterPath(), name);
         if (f.exists())
         {
            PathElement e = ((PathElement)selection.getFirstElement()).findChild(f.getName());
            if (e != null)
            {
               if (!MessageDialogHelper.openQuestion(getShell(), "File overwrite confirmation", "File with " + f.getName() + " already exist. Do you want to overwrite it?"))
                  continue;
               e.setFile(f);
            }
            else
            {
               e = new PathElement((PathElement)selection.getFirstElement(), f.getName(), f, UUID.randomUUID(), new Date());  
               notSavedFiles.add(e.getGuid().toString());             
            }
            uploadList.add(e);
         }
         else
         {
            Activator.logInfo("File does not exist: " + name);            
         }
      }

      if (!uploadList.isEmpty())
      {
         fileTree.refresh();
         fireModifyListeners();
         
         Activator.logInfo("FileDeliveryPolicyEditor: " + uploadList.size() + " files to upload");
         final NXCSession session = ConsoleSharedData.getSession();
         new ConsoleJob("Upload files", getViewPart(), Activator.PLUGIN_ID, null) {
            @Override
            protected void runInternal(IProgressMonitor monitor) throws Exception
            {
               monitor.beginTask("Upload files", uploadList.size());
               for(PathElement e : uploadList)
               {
                  Activator.logInfo("FileDeliveryPolicyEditor: uploading file " + e.getName() + " from " + e.getLocalFile());
                  monitor.subTask(e.getName());
                  session.uploadFileToServer(e.getLocalFile(), "FileDelivery-" + e.getGuid().toString(), null);
                  monitor.worked(1);
               }
               monitor.done();
            }
            
            @Override
            protected String getErrorMessage()
            {
               return "Cannot upload file";
            }
         }.start();
      }
   }

   /**
    * Delete selected file
    */
   private void deleteFile(final String name)
   {
      final NXCSession session = ConsoleSharedData.getSession();

      new ConsoleJob("Delete file", getViewPart(), Activator.PLUGIN_ID) {
         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {
            session.deleteServerFile("FileDelivery-" + name);
         }
         
         @Override
         protected String getErrorMessage()
         {
            return "Cannot delete file";
         }
      }.start();
   }

   /**
    * Update file content
    */
   private void updateFile()
   {
      IStructuredSelection selection = fileTree.getStructuredSelection();
      if ((selection.size() != 1) || !((PathElement)selection.getFirstElement()).isFile())
         return;

      FileDialog dlg = new FileDialog(getShell(), SWT.OPEN);
      String fileName = dlg.open();
      if (fileName == null)
         return;

      final File file = new File(fileName);
      if (!file.exists())
         return;

      final UUID guid = ((PathElement)selection.getFirstElement()).getGuid();
      final NXCSession session = ConsoleSharedData.getSession();
      new ConsoleJob("Upload file", getViewPart(), Activator.PLUGIN_ID, null) {
         @Override
         protected void runInternal(final IProgressMonitor monitor) throws Exception
         {
            session.uploadFileToServer(file, "FileDelivery-" + guid.toString(), new ProgressListener() {
               @Override
               public void setTotalWorkAmount(long workTotal)
               {
                  monitor.beginTask("Upload file", (int)file.length());
               }
               
               @Override
               public void markProgress(long workDone)
               {
                  monitor.worked((int)workDone);
               }
            });
            monitor.done();
            runInUIThread(new Runnable() {               
               @Override
               public void run()
               {
                  ((PathElement)selection.getFirstElement()).updateCreationTime(); 
                  fileTree.refresh(true);
                  fireModifyListeners();
               }
            });
         }
         
         @Override
         protected String getErrorMessage()
         {
            return "Cannot upload file";
         }
      }.start();
   }

   /**
    * Add new element
    */
   private void addElement(PathElement parent)
   {
      InputDialog dlg = new InputDialog(getShell(), (parent == null) ? "New root directory" : "New directory", "Enter name for new directory", "", new IInputValidator() {
         @Override
         public String isValid(String newText)
         {
            if (newText.isEmpty())
               return "Name cannot be empty";
            if (parent != null)
               for (PathElement el : parent.getChildren())
               {
                  if(newText.equalsIgnoreCase(el.getName()))
                     return "Object with this name already exists";
               }
            return null;
         }
      });
      if (dlg.open() != Window.OK)
         return;

      PathElement e = new PathElement(parent, dlg.getValue());
      if (parent == null)
      {
         rootElements.add(e);
         fileTree.refresh(true);
      }
      else
      {
         fileTree.refresh();
      }

      fireModifyListeners();
   }

   /**
    * Rename selected element
    */
   private void renameElement()
   {
      IStructuredSelection selection = fileTree.getStructuredSelection();
      if (selection.size() != 1)
         return;

      PathElement element = (PathElement)selection.getFirstElement();
      InputDialog dlg = new InputDialog(getShell(), element.isFile() ? "Rename file" : "Rename directory", "Enter new name", element.getName(), new IInputValidator() {
         @Override
         public String isValid(String newText)
         {
            if (newText.isEmpty())
               return "Name cannot be empty";
            if (element.getParent() != null)
               for (PathElement el : element.getParent().getChildren())
               {
                  if(!el.equals(element) && newText.equalsIgnoreCase(el.getName()))
                     return "Object with this name already exists";
               }
            return null;
         }
      });
      if (dlg.open() != Window.OK)
         return;

      ((PathElement)selection.getFirstElement()).updateCreationTime(); 
      if (element.getParent() == null)
      {
         rootElements.remove(element);
         element.setName(dlg.getValue());
         rootElements.add(element);
         fileTree.refresh();
      }
      else
      {
         element.setName(dlg.getValue());
         fileTree.update(element, null);
      }
      
      fireModifyListeners();
   }

   /**
    * Change file permissions
    */
   private void changePermissions()
   {
      IStructuredSelection selection = fileTree.getStructuredSelection();
      if (selection.size() != 1)
         return;

      PathElement element = (PathElement)selection.getFirstElement();
      FilePermissionsSelectionDialog dlg = new FilePermissionsSelectionDialog(getShell(), element.getPermissions());
      if (dlg.open() != Window.OK)
         return;

      element.setPermissions(dlg.getValue());
      fileTree.update(element, null);

      fireModifyListeners();
   }

   /**
    * Change file owner
    */
   private void changeOwner()
   {
      IStructuredSelection selection = fileTree.getStructuredSelection();
      if (selection.size() != 1)
         return;

      PathElement element = (PathElement)selection.getFirstElement();
      InputDialog dlg = new InputDialog(getShell(), "Set new file owner user name", "Enter owner", element.getOwner(), null);
      if (dlg.open() != Window.OK)
         return;

      element.setOwner(dlg.getValue());
      fileTree.update(element, null);

      fireModifyListeners();
   }

   /**
    * Change file owner group
    */
   private void changeOwnerGroup()
   {
      IStructuredSelection selection = fileTree.getStructuredSelection();
      if (selection.size() != 1)
         return;

      PathElement element = (PathElement)selection.getFirstElement();
      InputDialog dlg = new InputDialog(getShell(), "Set new file owner group name", "Enter owner group name", element.getOwnerGroup(), null);
      if (dlg.open() != Window.OK)
         return;

      element.setOwnerGroup(dlg.getValue());
      fileTree.update(element, null);

      fireModifyListeners();
   }

   /**
    * Delete selected elements
    */
   private void deleteElements()
   {
      IStructuredSelection selection = fileTree.getStructuredSelection();
      if (selection.isEmpty())
         return;

      if (!MessageDialogHelper.openQuestion(getShell(), "Delete confirmation", "Delete selected files?"))
         return;

      for(Object o : selection.toList())
      {
         PathElement element = (PathElement)o;
         
         if (element.isFile())
            deleteFiles(element);
         
         if (element.getParent() == null)
         {
            rootElements.remove(o);
         }
         else
         {
            element.remove();
         }
      }

      fileTree.refresh(true);
      fireModifyListeners();
   }
   
   /**
    * Delete files under folder
    */
   private void deleteFiles(PathElement element)
   {
      if (element.isFile())
         filesForDeletion.add(element.getGuid().toString());
      else
      {
         for(PathElement el : element.getChildren())
         {
            deleteFiles(el);
         }
      }
   }

   /**
    * @see org.netxms.ui.eclipse.datacollection.widgets.AbstractPolicyEditor#fillLocalPullDown(org.eclipse.jface.action.IMenuManager)
    */
   @Override
   public void fillLocalPullDown(IMenuManager manager)
   {
      manager.add(actionAddRoot);
   }

   /**
    * @see org.netxms.ui.eclipse.datacollection.widgets.AbstractPolicyEditor#fillLocalToolBar(org.eclipse.jface.action.IToolBarManager)
    */
   @Override
   public void fillLocalToolBar(IToolBarManager manager)
   {
      manager.add(actionAddRoot);
   }

   /**
    * @see org.eclipse.jface.text.IFindReplaceTarget#canPerformFind()
    */
   @Override
   public boolean canPerformFind()
   {
      return false;
   }

   /**
    * @see org.eclipse.jface.text.IFindReplaceTarget#findAndSelect(int, java.lang.String, boolean, boolean, boolean)
    */
   @Override
   public int findAndSelect(int widgetOffset, String findString, boolean searchForward, boolean caseSensitive, boolean wholeWord)
   {
      return 0;
   }

   /**
    * @see org.eclipse.jface.text.IFindReplaceTarget#getSelection()
    */
   @Override
   public Point getSelection()
   {
      return null;
   }

   /**
    * @see org.eclipse.jface.text.IFindReplaceTarget#getSelectionText()
    */
   @Override
   public String getSelectionText()
   {
      return null;
   }

   /**
    * @see org.eclipse.jface.text.IFindReplaceTarget#isEditable()
    */
   @Override
   public boolean isEditable()
   {
      return false;
   }

   /**
    * @see org.eclipse.jface.text.IFindReplaceTarget#replaceSelection(java.lang.String)
    */
   @Override
   public void replaceSelection(String text)
   {
   }

    /**
    * @see org.netxms.ui.eclipse.datacollection.widgets.AbstractPolicyEditor#isFindAndReplaceRequired()
    */
   @Override
   public boolean isFindAndReplaceRequired()
   {
      return false;
   }   

   /**
    * Callback that will be called on policy save
    */
   public void onSave()
   {
      notSavedFiles.clear();
      for (String name : filesForDeletion)
      {
         deleteFile(name);
      }
      filesForDeletion.clear();
   }
   
   /**
    * Callback that will be called on save discard
    */
   public void onDiscard()
   {
      for (String name : notSavedFiles)
      {
         deleteFile(name);
      }
      notSavedFiles.clear();
   }
}
