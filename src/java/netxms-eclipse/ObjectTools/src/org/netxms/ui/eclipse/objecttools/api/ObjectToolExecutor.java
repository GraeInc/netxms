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
package org.netxms.ui.eclipse.objecttools.api;

import java.io.IOException;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.Platform;
import org.eclipse.jface.window.Window;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.netxms.client.AgentFileData;
import org.netxms.client.NXCSession;
import org.netxms.client.ProgressListener;
import org.netxms.client.objecttools.InputField;
import org.netxms.client.objecttools.InputFieldType;
import org.netxms.client.objecttools.ObjectTool;
import org.netxms.ui.eclipse.filemanager.views.AgentFileViewer;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.jobs.ConsoleJobCallingServerJob;
import org.netxms.ui.eclipse.objects.ObjectContext;
import org.netxms.ui.eclipse.objecttools.Activator;
import org.netxms.ui.eclipse.objecttools.Messages;
import org.netxms.ui.eclipse.objecttools.dialogs.ObjectToolInputDialog;
import org.netxms.ui.eclipse.objecttools.views.AgentActionResults;
import org.netxms.ui.eclipse.objecttools.views.BrowserView;
import org.netxms.ui.eclipse.objecttools.views.LocalCommandResults;
import org.netxms.ui.eclipse.objecttools.views.MultiNodeCommandExecutor;
import org.netxms.ui.eclipse.objecttools.views.ServerCommandResults;
import org.netxms.ui.eclipse.objecttools.views.ServerScriptResults;
import org.netxms.ui.eclipse.objecttools.views.TableToolResults;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.tools.MessageDialogHelper;

/**
 * Executor for object tool
 */
public final class ObjectToolExecutor
{   
   /**
    * Private constructor to forbid instantiation 
    */
   private ObjectToolExecutor()
   {
   }

   /**
    * Check if tool is allowed for execution on at least one node from set
    * 
    * @param tool
    * @param nodes
    * @return
    */
   public static boolean isToolAllowed(ObjectTool tool, Set<ObjectContext> nodes)
   {
      if (tool.getToolType() != ObjectTool.TYPE_INTERNAL)
         return true;
      
      ObjectToolHandler handler = ObjectToolsCache.findHandler(tool.getData());
      if (handler != null)
      {
         for(ObjectContext n : nodes)
            if (handler.canExecuteOnNode(n.object, tool))
               return true;
         return false;
      }
      else
      {
         return false;
      }
   }

   /**
    * Check if given tool is applicable for at least one nodes in set
    * 
    * @param tool
    * @param nodes
    * @return
    */
   public static boolean isToolApplicable(ObjectTool tool, Set<ObjectContext> nodes)
   {
      for(ObjectContext n : nodes)
         if (tool.isApplicableForNode(n.object))
            return true;
      return false;
   }

   /**
    * Execute object tool on node set
    * 
    * @param tool Object tool
    */
   public static void execute(final Set<ObjectContext> allNodes, final ObjectTool tool)
   {
      //Filter allowed and applicable nodes for execution
      final Set<ObjectContext> nodes = new HashSet<ObjectContext>();
      ObjectToolHandler handler = ObjectToolsCache.findHandler(tool.getData());
      if ((tool.getToolType() != ObjectTool.TYPE_INTERNAL) || handler != null)
      {
         for(ObjectContext n : allNodes)
            if (((tool.getToolType() != ObjectTool.TYPE_INTERNAL) || handler.canExecuteOnNode(n.object, tool)) 
                  && tool.isApplicableForNode(n.object))
               nodes.add(n);
      }
      else
      {
         return;
      }

      final List<String> maskedFields = new ArrayList<String>();
      final Map<String, String> inputValues;
      final InputField[] fields = tool.getInputFields();
      if (fields.length > 0)
      {
         Arrays.sort(fields, new Comparator<InputField>() {
            @Override
            public int compare(InputField f1, InputField f2)
            {
               return f1.getSequence() - f2.getSequence();
            }
         });
         inputValues = readInputFields(fields);
         if (inputValues == null)
            return;  // cancelled
         for (int i = 0; i < fields.length; i++)
         {
            if (fields[i].getType() == InputFieldType.PASSWORD)  
            {
               maskedFields.add(fields[i].getName());
            }
         }
      }
      else
      {
         inputValues = new HashMap<String, String>(0);
      }
      final NXCSession session = ConsoleSharedData.getSession();
      new ConsoleJob(Messages.get().ObjectToolExecutor_JobName, null, Activator.PLUGIN_ID, null) {
         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {      
            List<String> expandedText = null;

            if ((tool.getFlags() & ObjectTool.ASK_CONFIRMATION) != 0)
            {
               String message = tool.getConfirmationText();
               if (nodes.size() == 1)
               {
                  // Expand message and action for 1 node, otherwise expansion occurs after confirmation
                  List<String> textToExpand = new ArrayList<String>();
                  textToExpand.add(tool.getConfirmationText());
                  if ((tool.getToolType() == ObjectTool.TYPE_URL) || (tool.getToolType() == ObjectTool.TYPE_LOCAL_COMMAND))
                  {
                     textToExpand.add(tool.getData());
                  }
                  ObjectContext node = nodes.iterator().next();
                  expandedText = session.substituteMacros(node, textToExpand, inputValues);
                  message = expandedText.remove(0);                  
               }
               else
               {
                  ObjectContext node = nodes.iterator().next();
                  message = node.substituteMacrosForMultipleNodes(message, inputValues);
               }

               ConfirmationRunnable runnable = new ConfirmationRunnable(message);
               getDisplay().syncExec(runnable);
               if (!runnable.isConfirmed())
                  return;

               if ((tool.getToolType() == ObjectTool.TYPE_URL) || (tool.getToolType() == ObjectTool.TYPE_LOCAL_COMMAND))
               {
                  expandedText = session.substituteMacros(nodes.toArray(new ObjectContext[nodes.size()]), tool.getData(), inputValues);
               }
            }
            else
            {
               if ((tool.getToolType() == ObjectTool.TYPE_URL) || (tool.getToolType() == ObjectTool.TYPE_LOCAL_COMMAND))
               {
                  expandedText = session.substituteMacros(nodes.toArray(new ObjectContext[nodes.size()]), tool.getData(), inputValues);
               }
            }

            // Check if password validation needed
            boolean validationNeeded = false;
            for(int i = 0; i < fields.length; i++)
               if (fields[i].getOptions().validatePassword)
               {
                  validationNeeded = true;
                  break;
               }

            if (validationNeeded)
            {
               for(int i = 0; i < fields.length; i++)
               {
                  if ((fields[i].getType() == InputFieldType.PASSWORD) && (fields[i].getOptions().validatePassword))
                  {
                     boolean valid = session.validateUserPassword(inputValues.get(fields[i].getName()));
                     if (!valid)
                     {
                        final String fieldName = fields[i].getDisplayName();
                        getDisplay().syncExec(new Runnable() {
                           @Override
                           public void run()
                           {
                              MessageDialogHelper.openError(null, Messages.get().ObjectToolExecutor_ErrorTitle, 
                                    String.format(Messages.get().ObjectToolExecutor_ErrorText, fieldName));
                           }
                        });
                        return;
                     }
                  }
               }
            }

            int i = 0;               
            if (nodes.size() != 1 && (tool.getToolType() == ObjectTool.TYPE_LOCAL_COMMAND || tool.getToolType() == ObjectTool.TYPE_SERVER_COMMAND ||
                  tool.getToolType() == ObjectTool.TYPE_ACTION || tool.getToolType() == ObjectTool.TYPE_SERVER_SCRIPT) && ((tool.getFlags() & ObjectTool.GENERATES_OUTPUT) != 0))
            {
               final List<String> finalExpandedText = expandedText;
               getDisplay().syncExec(new Runnable() {
                  @Override
                  public void run()
                  {
                     executeOnMultipleNodes(nodes, tool, inputValues, maskedFields, finalExpandedText);
                  }
               });
            }
            else
            {
               for(final ObjectContext n : nodes)
               {
                  if (tool.getToolType() == ObjectTool.TYPE_URL || tool.getToolType() == ObjectTool.TYPE_LOCAL_COMMAND)
                  {
                     final String tmp = expandedText.get(i++);
                     getDisplay().syncExec(new Runnable() {
                        @Override
                        public void run()
                        {
                           executeOnNode(n, tool, inputValues, maskedFields, tmp);
                        }
                     });
                  }
                  else
                  {
                     getDisplay().syncExec(new Runnable() {
                        @Override
                        public void run()
                        {
                           executeOnNode(n, tool, inputValues, maskedFields, null);
                        }
                     });                  
                  }
               }
            }
         }

         @Override
         protected String getErrorMessage()
         {
            return Messages.get().ObjectToolExecutor_PasswordValidationFailed;
         }
         
         class ConfirmationRunnable implements Runnable
         {
            private boolean confirmed;
            private String message;

            public ConfirmationRunnable(String message)
            {
               this.message = message;
            }

            @Override
            public void run()
            {
               confirmed = MessageDialogHelper.openQuestion(PlatformUI.getWorkbench().getActiveWorkbenchWindow().getShell(), 
                     Messages.get().ObjectToolsDynamicMenu_ConfirmExec, message);
            }
            
            boolean isConfirmed()
            {
               return confirmed;
            }
         }         
         
      }.start();
   }
   
   /**
    * Read input fields
    * 
    * @param fields
    * @return
    */
   private static Map<String, String> readInputFields(InputField[] fields)
   {
      ObjectToolInputDialog dlg = new ObjectToolInputDialog(PlatformUI.getWorkbench().getActiveWorkbenchWindow().getShell(), fields);
      if (dlg.open() != Window.OK)
         return null;
      return dlg.getValues();
   }

   /**
    * Execute object tool on single node
    * 
    * @param node node to execute at
    * @param tool object tool
    * @param inputValues input values
    * @param maskedFields list of input fields to be masked
    * @param expandedToolData expanded tool data
    */
   private static void executeOnNode(final ObjectContext node, final ObjectTool tool, Map<String, String> inputValues,
         List<String> maskedFields, String expandedToolData)
   {
      switch(tool.getToolType())
      {
         case ObjectTool.TYPE_ACTION:
            executeAgentAction(node, tool, inputValues, maskedFields);
            break;
         case ObjectTool.TYPE_FILE_DOWNLOAD:
            executeFileDownload(node, tool, inputValues);
            break;
         case ObjectTool.TYPE_INTERNAL:
            executeInternalTool(node, tool);
            break;
         case ObjectTool.TYPE_LOCAL_COMMAND:
            executeLocalCommand(node, tool, inputValues, expandedToolData);
            break;
         case ObjectTool.TYPE_SERVER_COMMAND:
            executeServerCommand(node, tool, inputValues, maskedFields);
            break;
         case ObjectTool.TYPE_SERVER_SCRIPT:
            executeServerScript(node, tool, inputValues);
            break;
         case ObjectTool.TYPE_AGENT_LIST:
         case ObjectTool.TYPE_AGENT_TABLE:
         case ObjectTool.TYPE_SNMP_TABLE:
            executeTableTool(node, tool);
            break;
         case ObjectTool.TYPE_URL:
            openURL(node, tool, inputValues, expandedToolData);
            break;
      }
   }

   /**
    * Execute object tool on set of nodes
    *
    * @param nodes set of nodes
    * @param tool tool to execute
    * @param inputValues input values
    * @param maskedFields list of input fields to be masked
    * @param expandedToolData expanded tool data
    */
   private static void executeOnMultipleNodes(Set<ObjectContext> nodes, ObjectTool tool, Map<String, String> inputValues,
         List<String> maskedFields, List<String> expandedToolData)
   {
      final IWorkbenchWindow window = PlatformUI.getWorkbench().getActiveWorkbenchWindow();
      try
      {
         MultiNodeCommandExecutor view = (MultiNodeCommandExecutor)window.getActivePage().showView(MultiNodeCommandExecutor.ID, tool.getDisplayName() + nodes.toString(), IWorkbenchPage.VIEW_ACTIVATE);
         view.execute(tool, nodes, inputValues, maskedFields, expandedToolData);
      }
      catch(Exception e)
      {
         e.printStackTrace();
         MessageDialogHelper.openError(window.getShell(), Messages.get().ObjectToolsDynamicMenu_Error, String.format(Messages.get().ObjectToolsDynamicMenu_ErrorOpeningView, e.getLocalizedMessage()));
      }
   }

   /**
    * Execute table tool
    * 
    * @param node
    * @param tool
    */
   private static void executeTableTool(final ObjectContext node, final ObjectTool tool)
   {
      final IWorkbenchWindow window = PlatformUI.getWorkbench().getActiveWorkbenchWindow();
      try
      {
         final IWorkbenchPage page = window.getActivePage();
         final TableToolResults view = (TableToolResults)page.showView(TableToolResults.ID,
               Long.toString(tool.getId()) + "&" + Long.toString(node.object.getObjectId()), IWorkbenchPage.VIEW_ACTIVATE); //$NON-NLS-1$
         view.refreshTable();
      }
      catch(PartInitException e)
      {
         MessageDialogHelper.openError(window.getShell(), Messages.get().ObjectToolsDynamicMenu_Error, String.format(Messages.get().ObjectToolsDynamicMenu_ErrorOpeningView, e.getLocalizedMessage()));
      }
   }
   
   /**
    * @param node
    * @param tool
    * @param inputValues 
    */
   private static void executeAgentAction(final ObjectContext node, final ObjectTool tool, final Map<String, String> inputValues, final List<String> maskedFields)
   {
      final NXCSession session = (NXCSession)ConsoleSharedData.getSession();
      
      if ((tool.getFlags() & ObjectTool.GENERATES_OUTPUT) == 0)
      {      
         new ConsoleJob(String.format(Messages.get().ObjectToolsDynamicMenu_ExecuteOnNode, node.object.getObjectName()), null, Activator.PLUGIN_ID, null) {
            @Override
            protected String getErrorMessage()
            {
               return String.format(Messages.get().ObjectToolsDynamicMenu_CannotExecuteOnNode, node.object.getObjectName());
            }
   
            @Override
            protected void runInternal(IProgressMonitor monitor) throws Exception
            {
               final String action = session.executeActionWithExpansion(node.object.getObjectId(), node.getAlarmId(), tool.getData(), inputValues, maskedFields);
               runInUIThread(new Runnable() {
                  @Override
                  public void run()
                  {
                     MessageDialogHelper.openInformation(null, Messages.get().ObjectToolsDynamicMenu_ToolExecution, String.format(Messages.get().ObjectToolsDynamicMenu_ExecSuccess, action, node.object.getObjectName()));
                  }
               });
            }
         }.start();
      }
      else
      {
         final String secondaryId = Long.toString(node.object.getObjectId()) + "&" + Long.toString(tool.getId()); //$NON-NLS-1$
         final IWorkbenchWindow window = PlatformUI.getWorkbench().getActiveWorkbenchWindow();
         try
         {
            AgentActionResults view = (AgentActionResults)window.getActivePage().showView(AgentActionResults.ID, secondaryId, IWorkbenchPage.VIEW_ACTIVATE);
            view.executeAction(tool.getData(), node.getAlarmId(), inputValues, maskedFields);
         }
         catch(Exception e)
         {
            MessageDialogHelper.openError(window.getShell(), Messages.get().ObjectToolsDynamicMenu_Error, String.format(Messages.get().ObjectToolsDynamicMenu_ErrorOpeningView, e.getLocalizedMessage()));
         }
      }
   }

   /**
    * Execute server command
    * 
    * @param node
    * @param tool
    * @param inputValues 
    */
   private static void executeServerCommand(final ObjectContext node, final ObjectTool tool, final Map<String, String> inputValues, final List<String> maskedFields)
   {
      final NXCSession session = (NXCSession)ConsoleSharedData.getSession();
      if ((tool.getFlags() & ObjectTool.GENERATES_OUTPUT) == 0)
      {      
         new ConsoleJob(Messages.get().ObjectToolsDynamicMenu_ExecuteServerCmd, null, Activator.PLUGIN_ID, null) {
            @Override
            protected void runInternal(IProgressMonitor monitor) throws Exception
            {
               session.executeServerCommand(node.object.getObjectId(), tool.getData(), inputValues, maskedFields);
               runInUIThread(new Runnable() {
                  @Override
                  public void run()
                  {
                     MessageDialogHelper.openInformation(null, Messages.get().ObjectToolsDynamicMenu_Information, Messages.get().ObjectToolsDynamicMenu_ServerCommandExecuted);
                  }
               });
            }
            
            @Override
            protected String getErrorMessage()
            {
               return Messages.get().ObjectToolsDynamicMenu_ServerCmdExecError;
            }
         }.start();
      }
      else
      {
         final String secondaryId = Long.toString(node.object.getObjectId()) + "&" + Long.toString(tool.getId()); //$NON-NLS-1$
         final IWorkbenchWindow window = PlatformUI.getWorkbench().getActiveWorkbenchWindow();
         try
         {
            ServerCommandResults view = (ServerCommandResults)window.getActivePage().showView(ServerCommandResults.ID, secondaryId, IWorkbenchPage.VIEW_ACTIVATE);
            view.executeCommand(tool.getData(), inputValues, maskedFields);
         }
         catch(Exception e)
         {
            MessageDialogHelper.openError(window.getShell(), Messages.get().ObjectToolsDynamicMenu_Error, String.format(Messages.get().ObjectToolsDynamicMenu_ErrorOpeningView, e.getLocalizedMessage()));
         }
      }
   }
   
   /**
    * Execute server script
    * 
    * @param node
    * @param tool
    * @param inputValues 
    */
   private static void executeServerScript(final ObjectContext node, final ObjectTool tool, final Map<String, String> inputValues)
   {
      final NXCSession session = (NXCSession)ConsoleSharedData.getSession();
      if ((tool.getFlags() & ObjectTool.GENERATES_OUTPUT) == 0)
      {      
         new ConsoleJob("Execute server script", null, Activator.PLUGIN_ID, null) {
            @Override
            protected void runInternal(IProgressMonitor monitor) throws Exception
            {
               session.executeLibraryScript(node.object.getObjectId(), node.getAlarmId(), tool.getData(), inputValues, null);
               runInUIThread(new Runnable() {
                  @Override
                  public void run()
                  {
                     MessageDialogHelper.openInformation(null, Messages.get().ObjectToolsDynamicMenu_Information, Messages.get().ObjectToolsDynamicMenu_ServerScriptExecuted);
                  }
               });
            }
            
            @Override
            protected String getErrorMessage()
            {
               return Messages.get().ObjectToolsDynamicMenu_ServerScriptExecError;
            }
         }.start();
      }
      else
      {
         final String secondaryId = Long.toString(node.object.getObjectId()) + "&" + Long.toString(tool.getId()); //$NON-NLS-1$
         final IWorkbenchWindow window = PlatformUI.getWorkbench().getActiveWorkbenchWindow();
         try
         {
            ServerScriptResults view = (ServerScriptResults)window.getActivePage().showView(ServerScriptResults.ID, secondaryId, IWorkbenchPage.VIEW_ACTIVATE);
            view.executeScript(tool.getData(), node.getAlarmId(), inputValues);
         }
         catch(Exception e)
         {
            MessageDialogHelper.openError(window.getShell(), Messages.get().ObjectToolsDynamicMenu_Error, String.format(Messages.get().ObjectToolsDynamicMenu_ErrorOpeningView, e.getLocalizedMessage()));
         }
      }
   }
   
   /**
    * Execute local command
    * 
    * @param node node to execute at
    * @param tool object tool
    * @param inputValues input values
    * @param command command to execute
    */
   private static void executeLocalCommand(final ObjectContext node, final ObjectTool tool, Map<String, String> inputValues, String command)
   {      
      if ((tool.getFlags() & ObjectTool.GENERATES_OUTPUT) == 0)
      {
         final String os = Platform.getOS();
         
         try
         {
            if (os.equals(Platform.OS_WIN32))
            {
               command = "CMD.EXE /C START \"NetXMS\" " + command; //$NON-NLS-1$
               Runtime.getRuntime().exec(command);
            }
            else
            {
               Runtime.getRuntime().exec(new String[] { "/bin/sh", "-c", command }); //$NON-NLS-1$ //$NON-NLS-2$
            }
         }
         catch(IOException e)
         {
            // TODO Auto-generated catch block
            e.printStackTrace();
         }
      }
      else
      {
         final String secondaryId = Long.toString(node.object.getObjectId()) + "&" + Long.toString(tool.getId()); //$NON-NLS-1$
         final IWorkbenchWindow window = PlatformUI.getWorkbench().getActiveWorkbenchWindow();
         try
         {
            LocalCommandResults view = (LocalCommandResults)window.getActivePage().showView(LocalCommandResults.ID, secondaryId, IWorkbenchPage.VIEW_ACTIVATE);
            view.runCommand(command);
         }
         catch(Exception e)
         {
            MessageDialogHelper.openError(window.getShell(), Messages.get().ObjectToolsDynamicMenu_Error, String.format(Messages.get().ObjectToolsDynamicMenu_ErrorOpeningView, e.getLocalizedMessage()));
         }
      }
   }

   /**
    * @param node
    * @param tool
    * @param inputValues 
    */
   private static void executeFileDownload(final ObjectContext node, final ObjectTool tool, final Map<String, String> inputValues)
   {
      final NXCSession session = (NXCSession)ConsoleSharedData.getSession();
      String[] parameters = tool.getData().split("\u007F"); //$NON-NLS-1$
      
      final String fileName = parameters[0];
      final int maxFileSize = (parameters.length > 0) ? Integer.parseInt(parameters[1]) : 0;
      final boolean follow = (parameters.length > 1) ? parameters[2].equals("true") : false; //$NON-NLS-1$
      
      ConsoleJobCallingServerJob job = new ConsoleJobCallingServerJob(Messages.get().ObjectToolsDynamicMenu_DownloadFromAgent, null, Activator.PLUGIN_ID, null) {
         @Override
         protected String getErrorMessage()
         {
            return String.format(Messages.get().ObjectToolsDynamicMenu_DownloadError, fileName, node.object.getObjectName());
         }

         @Override
         protected void runInternal(final IProgressMonitor monitor) throws Exception
         {
            final AgentFileData file = session.downloadFileFromAgent(node.object.getObjectId(), fileName, true, node.getAlarmId(), inputValues, maxFileSize, follow, new ProgressListener() {
               @Override
               public void setTotalWorkAmount(long workTotal)
               {
                  monitor.beginTask("Download file " + fileName, (int)workTotal);
               }

               @Override
               public void markProgress(long workDone)
               {
                  monitor.worked((int)workDone);
               }
            }, this);
            runInUIThread(new Runnable() {
               @Override
               public void run()
               {
                  try
                  {
                     String secondaryId = Long.toString(node.object.getObjectId()) + "&" + URLEncoder.encode(fileName, "UTF-8"); //$NON-NLS-1$ //$NON-NLS-2$
                     AgentFileViewer.createView(secondaryId, node.object.getObjectId(), file, follow);
                  }
                  catch(Exception e)
                  {
                     final IWorkbenchWindow window = PlatformUI.getWorkbench().getActiveWorkbenchWindow();
                     MessageDialogHelper.openError(window.getShell(), Messages.get().ObjectToolsDynamicMenu_Error, String.format(Messages.get().ObjectToolsDynamicMenu_ErrorOpeningView, e.getLocalizedMessage()));
                     Activator.logError("Cannot open agent file viewer", e);
                  }
               }
            });
         }
      };
      job.start();
   }

   /**
    * @param node
    * @param tool
    */
   private static void executeInternalTool(final ObjectContext node, final ObjectTool tool)
   {
      ObjectToolHandler handler = ObjectToolsCache.findHandler(tool.getData());
      if (handler != null)
      {
         handler.execute(node.object, tool);
      }
      else
      {
         MessageDialogHelper.openError(PlatformUI.getWorkbench().getActiveWorkbenchWindow().getShell(), Messages.get().ObjectToolsDynamicMenu_Error, Messages.get().ObjectToolsDynamicMenu_HandlerNotDefined);
      }
   }

   /**
    * @param node
    * @param tool
    * @param inputValues 
    */
   private static void openURL(final ObjectContext node, final ObjectTool tool, Map<String, String> inputValues, String url)
   {
      
      final String sid = Long.toString(node.object.getObjectId()) + "&" + Long.toString(tool.getId()); //$NON-NLS-1$
      final IWorkbenchWindow window = PlatformUI.getWorkbench().getActiveWorkbenchWindow();
      try
      {
         BrowserView view = (BrowserView)window.getActivePage().showView(BrowserView.ID, sid, IWorkbenchPage.VIEW_ACTIVATE);
         view.openUrl(url);
      }
      catch(PartInitException e)
      {
         MessageDialogHelper.openError(window.getShell(), Messages.get().ObjectToolsDynamicMenu_Error, Messages.get().ObjectToolsDynamicMenu_CannotOpenWebBrowser + e.getLocalizedMessage());
      }
   }
}
