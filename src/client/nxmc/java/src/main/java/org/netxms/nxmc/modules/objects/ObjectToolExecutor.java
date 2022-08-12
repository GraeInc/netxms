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
package org.netxms.nxmc.modules.objects;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.apache.commons.lang3.SystemUtils;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.window.Window;
import org.netxms.client.InputField;
import org.netxms.client.NXCSession;
import org.netxms.client.constants.InputFieldType;
import org.netxms.client.objecttools.ObjectTool;
import org.netxms.nxmc.Registry;
import org.netxms.nxmc.base.jobs.Job;
import org.netxms.nxmc.base.views.ViewPlacement;
import org.netxms.nxmc.base.widgets.MessageArea;
import org.netxms.nxmc.localization.LocalizationHelper;
import org.netxms.nxmc.modules.objects.dialogs.ObjectToolInputDialog;
import org.netxms.nxmc.tools.MessageDialogHelper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.xnap.commons.i18n.I18n;

/**
 * Executor for object tool
 */
public final class ObjectToolExecutor
{
   private static final I18n i18n = LocalizationHelper.getI18n(ObjectToolExecutor.class);
   private static final Logger logger = LoggerFactory.getLogger(ObjectToolExecutor.class);

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
    * @param allNodes nodes to execution tool on
    * @param tool Object tool
    * @param viewPlacement view placement information
    */
   public static void execute(final Set<ObjectContext> allNodes, final ObjectTool tool, final ViewPlacement viewPlacement)
   {
      // Filter allowed and applicable nodes for execution
      final Set<ObjectContext> nodes = new HashSet<ObjectContext>();
      ObjectToolHandler handler = ObjectToolsCache.findHandler(tool.getData());
      if ((tool.getToolType() != ObjectTool.TYPE_INTERNAL) || handler != null)
      {
         for(ObjectContext n : allNodes)
            if (((tool.getToolType() != ObjectTool.TYPE_INTERNAL) || handler.canExecuteOnNode(n.object, tool)) && tool.isApplicableForNode(n.object))
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
         inputValues = readInputFields(tool.getDisplayName(), fields);
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
      final NXCSession session = Registry.getSession();
      new Job(i18n.tr("Execute object tool"), null) {
         @Override
         protected void run(IProgressMonitor monitor) throws Exception
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
                  message = node.substituteMacrosForMultipleNodes(message, inputValues, getDisplay());
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
               if (fields[i].isPasswordValidationNeeded())
               {
                  validationNeeded = true;
                  break;
               }

            if (validationNeeded)
            {
               for(int i = 0; i < fields.length; i++)
               {
                  if ((fields[i].getType() == InputFieldType.PASSWORD) && fields[i].isPasswordValidationNeeded())
                  {
                     boolean valid = session.validateUserPassword(inputValues.get(fields[i].getName()));
                     if (!valid)
                     {
                        final String fieldName = fields[i].getDisplayName();
                        getDisplay().syncExec(new Runnable() {
                           @Override
                           public void run()
                           {
                              MessageDialogHelper.openError(null, i18n.tr("Error"),
                                    String.format(i18n.tr("Password entered in input field \"%s\" is not valid"), fieldName));
                           }
                        });
                        return;
                     }
                  }
               }
            }

            int i = 0;
            if ((nodes.size() > 1) &&
                  (tool.getToolType() == ObjectTool.TYPE_LOCAL_COMMAND || tool.getToolType() == ObjectTool.TYPE_SERVER_COMMAND || tool.getToolType() == ObjectTool.TYPE_SSH_COMMAND ||
                  tool.getToolType() == ObjectTool.TYPE_ACTION || tool.getToolType() == ObjectTool.TYPE_SERVER_SCRIPT) && ((tool.getFlags() & ObjectTool.GENERATES_OUTPUT) != 0))
            {
               final List<String> finalExpandedText = expandedText;
               getDisplay().syncExec(new Runnable() {
                  @Override
                  public void run()
                  {
                     executeOnMultipleNodes(nodes, tool, inputValues, maskedFields, finalExpandedText, viewPlacement);
                  }
               });
            }
            else
            {
               for(final ObjectContext n : nodes)
               {
                  if (tool.getToolType() == ObjectTool.TYPE_URL || tool.getToolType() == ObjectTool.TYPE_LOCAL_COMMAND)
                  {
                     final String data = expandedText.get(i++);
                     getDisplay().syncExec(new Runnable() {
                        @Override
                        public void run()
                        {
                           executeOnNode(n, tool, inputValues, maskedFields, data, viewPlacement);
                        }
                     });
                  }
                  else
                  {
                     getDisplay().syncExec(new Runnable() {
                        @Override
                        public void run()
                        {
                           executeOnNode(n, tool, inputValues, maskedFields, null, viewPlacement);
                        }
                     });                  
                  }
               }
            }
         }

         @Override
         protected String getErrorMessage()
         {
            return i18n.tr("Object tool execution failed");
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
               confirmed = MessageDialogHelper.openQuestion(Registry.getMainWindow().getShell(), i18n.tr("Confirm Tool Execution"), message);
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
    * @param title Input dialog title
    * @param fields Input fields to read
    * @return values for input fields
    */
   private static Map<String, String> readInputFields(String title, InputField[] fields)
   {
      ObjectToolInputDialog dlg = new ObjectToolInputDialog(Registry.getMainWindow().getShell(), title, fields);
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
    * @param winbdow owning window
    * @param perspective owning perspective
    */
   private static void executeOnNode(final ObjectContext node, final ObjectTool tool, Map<String, String> inputValues,
         List<String> maskedFields, String expandedToolData, final ViewPlacement viewPlacement)
   {
      switch(tool.getToolType())
      {
         case ObjectTool.TYPE_ACTION:
            executeAgentAction(node, tool, inputValues, maskedFields, viewPlacement);
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
         case ObjectTool.TYPE_SSH_COMMAND:
            executeSshCommand(node, tool, inputValues);
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
    * @param viewPlacement view placement information
    */
   private static void executeOnMultipleNodes(Set<ObjectContext> nodes, ObjectTool tool, Map<String, String> inputValues,
         List<String> maskedFields, List<String> expandedToolData, ViewPlacement viewPlacement)
   {
      /*
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
      */
   }

   /**
    * Execute table tool
    * 
    * @param node
    * @param tool
    */
   private static void executeTableTool(final ObjectContext node, final ObjectTool tool)
   {
      /*
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
      */
   }
   
   /**
    * Execute agent action.
    *
    * @param node
    * @param tool
    * @param inputValues
    * @param view current view
    */
   private static void executeAgentAction(final ObjectContext node, final ObjectTool tool, final Map<String, String> inputValues, final List<String> maskedFields, final ViewPlacement viewPlacement)
   {
      final NXCSession session = Registry.getSession();

      if ((tool.getFlags() & ObjectTool.GENERATES_OUTPUT) == 0)
      {
         new Job(String.format(i18n.tr("Execute command on node %s"), node.object.getObjectName()), null, viewPlacement.getMessageAreaHolder()) {
            @Override
            protected String getErrorMessage()
            {
               return String.format(i18n.tr("Cannot execute command on node %s"), node.object.getObjectName());
            }
   
            @Override
            protected void run(IProgressMonitor monitor) throws Exception
            {
               final String action = session.executeActionWithExpansion(node.object.getObjectId(), node.getAlarmId(), tool.getData(), inputValues, maskedFields);
               runInUIThread(new Runnable() {
                  @Override
                  public void run()
                  {
                     String message = String.format(i18n.tr("Action %s executed successfully on node %s"), action, node.object.getObjectName());
                     viewPlacement.getMessageAreaHolder().addMessage(MessageArea.SUCCESS, message);
                  }
               });
            }
         }.start();
      }
      else
      {
         /*
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
         */
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
      final NXCSession session = Registry.getSession();
      if ((tool.getFlags() & ObjectTool.GENERATES_OUTPUT) == 0)
      {      
         new Job(i18n.tr("Execute server command"), null) {
            @Override
            protected void run(IProgressMonitor monitor) throws Exception
            {
               session.executeServerCommand(node.object.getObjectId(), tool.getData(), inputValues, maskedFields);
               runInUIThread(new Runnable() {
                  @Override
                  public void run()
                  {
                     MessageDialogHelper.openInformation(Registry.getMainWindow().getShell(), i18n.tr("Information"),
                           i18n.tr("Server command executed successfully"));
                  }
               });
            }
            
            @Override
            protected String getErrorMessage()
            {
               return i18n.tr("Cannot execute command on server");
            }
         }.start();
      }
      else
      {
         /*
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
         */
      }
   }

   /**
    * Execute SSH command
    * 
    * @param node target node
    * @param tool tool information
    * @param inputValues input values provided by user
    */
   private static void executeSshCommand(final ObjectContext node, final ObjectTool tool, final Map<String, String> inputValues)
   {
      final NXCSession session = Registry.getSession();

      if ((tool.getFlags() & ObjectTool.GENERATES_OUTPUT) == 0)
      {
         new Job(String.format(i18n.tr("Executing SSH command on node %s"), node.object.getObjectName()), null) {
            @Override
            protected void run(IProgressMonitor monitor) throws Exception
            {
               try
               {
                  session.executeSshCommand(node.object.getObjectId(), tool.getData(), false, null, null);
                  /*
                  if (statusDialog != null)
                  {
                     statusDialog.updateExecutionStatus(node.object.getObjectId(), null);
                  }
                  else
                  {
                     runInUIThread(new Runnable() {
                        @Override
                        public void run()
                        {
                           MessageDialogHelper.openInformation(null, Messages.get().ObjectToolsDynamicMenu_ToolExecution,
                                 String.format(Messages.get().ObjectToolsDynamicMenu_ExecSuccess, tool.getData(), node.object.getObjectName()));
                        }
                     });
                  }
                  */
               }
               catch(Exception e)
               {
                  /*
                  if (statusDialog != null)
                  {
                     statusDialog.updateExecutionStatus(node.object.getObjectId(), e.getLocalizedMessage());
                  }
                  else
                  {
                     throw e;
                  }
                  */
               }
            }

            @Override
            protected String getErrorMessage()
            {
               return String.format(i18n.tr("Cannot execute SSH command on node %s"), node.object.getObjectName());
            }
         }.start();
      }
      else
      {
         /*
         final String secondaryId = Long.toString(node.object.getObjectId()) + "&" + Long.toString(tool.getId()); //$NON-NLS-1$
         final IWorkbenchWindow window = PlatformUI.getWorkbench().getActiveWorkbenchWindow();
         try
         {
            SSHCommandResults view = (SSHCommandResults)window.getActivePage().showView(SSHCommandResults.ID, secondaryId, IWorkbenchPage.VIEW_ACTIVATE);
            view.executeSshCommand(tool.getData());
         }
         catch(Exception e)
         {
            MessageDialogHelper.openError(window.getShell(), Messages.get().ObjectToolsDynamicMenu_Error, String.format(Messages.get().ObjectToolsDynamicMenu_ErrorOpeningView, e.getLocalizedMessage()));
         }
         */
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
      final NXCSession session = Registry.getSession();
      if ((tool.getFlags() & ObjectTool.GENERATES_OUTPUT) == 0)
      {      
         new Job(i18n.tr("Execute server script"), null) {
            @Override
            protected void run(IProgressMonitor monitor) throws Exception
            {
               session.executeLibraryScript(node.object.getObjectId(), node.getAlarmId(), tool.getData(), inputValues, null);
               runInUIThread(new Runnable() {
                  @Override
                  public void run()
                  {
                     MessageDialogHelper.openInformation(Registry.getMainWindow().getShell(), i18n.tr("Information"), i18n.tr("Server script executed successfully"));
                  }
               });
            }
            
            @Override
            protected String getErrorMessage()
            {
               return i18n.tr("Cannot execute script on server");
            }
         }.start();
      }
      else
      {
         /*
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
         */
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
         try
         {
            if (SystemUtils.IS_OS_WINDOWS)
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
            logger.error("Exception while executing local command", e);
         }
      }
      else
      {
         /*
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
         */
      }
   }

   /**
    * @param node
    * @param tool
    * @param inputValues 
    */
   private static void executeFileDownload(final ObjectContext node, final ObjectTool tool, final Map<String, String> inputValues)
   {
      /*
      final NXCSession session = Registry.getSession();
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
      */
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
         MessageDialogHelper.openError(Registry.getMainWindow().getShell(), i18n.tr("Error"), i18n.tr("Cannot execute object tool: handler not defined"));
      }
   }

   /**
    * @param node
    * @param tool
    * @param inputValues 
    */
   private static void openURL(final ObjectContext node, final ObjectTool tool, Map<String, String> inputValues, String url)
   {
      /*
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
      */
   }
}
