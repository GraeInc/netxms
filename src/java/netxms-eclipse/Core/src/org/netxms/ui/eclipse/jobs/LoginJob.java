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
package org.netxms.ui.eclipse.jobs;

import java.lang.reflect.InvocationTargetException;
import java.security.Signature;
import java.security.cert.Certificate;
import java.util.List;
import java.util.Locale;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IConfigurationElement;
import org.eclipse.core.runtime.IExtensionRegistry;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.Platform;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.util.IPropertyChangeListener;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.widgets.Display;
import org.netxms.base.VersionInfo;
import org.netxms.client.NXCException;
import org.netxms.client.NXCSession;
import org.netxms.client.ProtocolVersion;
import org.netxms.client.SessionListener;
import org.netxms.client.SessionNotification;
import org.netxms.client.TwoFactorAuthenticationCallback;
import org.netxms.client.constants.AuthenticationType;
import org.netxms.client.constants.RCC;
import org.netxms.ui.eclipse.console.Activator;
import org.netxms.ui.eclipse.console.KeepAliveTimer;
import org.netxms.ui.eclipse.console.Messages;
import org.netxms.ui.eclipse.console.SourceProvider;
import org.netxms.ui.eclipse.console.TweakletManager;
import org.netxms.ui.eclipse.console.api.ConsoleLoginListener;
import org.netxms.ui.eclipse.console.api.SessionProvider;
import org.netxms.ui.eclipse.console.dialogs.TwoFactorMetodSelectionDialog;
import org.netxms.ui.eclipse.console.dialogs.TwoFactorResponseDialog;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;

/**
 * Login job
 */
public class LoginJob implements IRunnableWithProgress
{
   private Display display;
   private String server;
   private String loginName;
   private boolean encryptSession;
   private boolean ignoreProtocolVersion;
   private AuthenticationType authMethod;
   private String password;
   private Certificate certificate;
   private Signature signature;

   /**
    * @param display
    * @param server
    * @param loginName
    * @param encryptSession
    */
   public LoginJob(Display display, String server, String loginName, boolean encryptSession, boolean ignoreProtocolVersion)
   {
      this.display = display;
      this.server = server;
      this.loginName = loginName;
      this.encryptSession = encryptSession;
      this.ignoreProtocolVersion = ignoreProtocolVersion;
      authMethod = AuthenticationType.PASSWORD;
   }

   /**
    * @see org.eclipse.jface.operation.IRunnableWithProgress#run(org.eclipse.core.runtime.IProgressMonitor)
    */
   @Override
   public void run(IProgressMonitor monitor) throws InvocationTargetException, InterruptedException
   {
      monitor.beginTask(Messages.get().LoginJob_connecting, 100);
      try
      {
         final String hostName;
         int port = NXCSession.DEFAULT_CONN_PORT;
         final String[] split = server.split(":"); //$NON-NLS-1$
         if (split.length == 2)
         {
            hostName = split[0];
            try
            {
               port = Integer.valueOf(split[1]);
            }
            catch(NumberFormatException e)
            {
               // ignore
            }
         }
         else
         {
            hostName = server;
         }
         
         Activator.logInfo("Connecting to " + hostName + " port " + port);

         final NXCSession session = createSession(hostName, port);
         session.setClientLanguage(Locale.getDefault().getLanguage());

         session.setClientInfo("nxmc/" + VersionInfo.version()); //$NON-NLS-1$
         session.setIgnoreProtocolVersion(ignoreProtocolVersion);
         monitor.worked(10);

         session.connect(new int[] { ProtocolVersion.INDEX_FULL });
         session.login(authMethod, loginName, password, certificate, signature, new TwoFactorAuthenticationCallback() {
            @Override
            public int selectMethod(final List<String> methods)
            {
               if (methods.size() == 1)
                  return 0; // Skip selection dialog if only one method is available

               final int[] selection = new int[1];
               selection[0] = -1;
               display.syncExec(new Runnable() {
                  @Override
                  public void run()
                  {
                     TwoFactorMetodSelectionDialog dlg = new TwoFactorMetodSelectionDialog(null, methods);
                     if (dlg.open() == Window.OK)
                        selection[0] = dlg.getSelectedMethod();
                  }
               });
               return selection[0];
            }

            @Override
            public String getUserResponse(final String challenge, final String qrLabel)
            {
               final String[] response = new String[1];
               display.syncExec(new Runnable() {
                  @Override
                  public void run()
                  {
                     TwoFactorResponseDialog dlg = new TwoFactorResponseDialog(null, challenge, qrLabel);
                     if (dlg.open() == Window.OK)
                        response[0] = dlg.getResponse();
                  }
               });
               return response[0];
            }
         });
         monitor.worked(40);

         monitor.setTaskName(Messages.get().LoginJob_sync_objects);
         final boolean[] objectsFullSync = new boolean[1];
         display.syncExec(new Runnable() {
            @Override
            public void run()
            {
               IPreferenceStore store = ConsoleSharedData.getSettings();
               objectsFullSync[0] = store.getBoolean("ObjectsFullSync");
               store.addPropertyChangeListener(new IPropertyChangeListener() {
                  @Override
                  public void propertyChange(PropertyChangeEvent event)
                  {
                     if (event.getProperty().equals("ObjectsFullSync"))
                     {
                        Object value = event.getNewValue();
                        boolean doFullSync;
                        if (value instanceof Boolean)
                           doFullSync = ((Boolean)value).booleanValue();
                        else if (value instanceof String)
                           doFullSync = Boolean.valueOf((String)value);
                        else
                           doFullSync = false;
                        if (doFullSync)
                        {
                           Activator.logInfo("Full object synchronization triggered by preference change");
                           ConsoleJob job = new ConsoleJob("Synchronize all objects", null, Activator.PLUGIN_ID, null) {
                              @Override
                              protected void runInternal(IProgressMonitor monitor) throws Exception
                              {
                                 if (!session.areObjectsSynchronized())
                                    session.syncObjects();
                              }
   
                              @Override
                              protected String getErrorMessage()
                              {
                                 return "Failed to synchronize all objects";
                              }
                           };
                           job.setUser(false);
                           job.start();
                        }
                     }
                  }
               });
            }
         });
         session.syncObjects(objectsFullSync[0]);
         monitor.worked(25);

         monitor.setTaskName(Messages.get().LoginJob_sync_users);
         session.subscribeToUserDBUpdates();
         monitor.worked(5);

         monitor.setTaskName(Messages.get().LoginJob_sync_event_db);
         try
         {
            session.syncEventTemplates();
         }
         catch(NXCException e)
         {
            if (e.getErrorCode() != RCC.ACCESS_DENIED)
               throw e;
         }
         try
         {
            session.syncAlarmCategories();
         }
         catch(NXCException e)
         {
            if (e.getErrorCode() != RCC.ACCESS_DENIED)
               throw e;
         }
         monitor.worked(5);

         monitor.setTaskName(Messages.get().LoginJob_subscribe);
         session.subscribe(NXCSession.CHANNEL_ALARMS);
         session.subscribe(NXCSession.CHANNEL_GEO_AREAS);
         monitor.worked(5);

         ConsoleSharedData.setSession(session);

         display.syncExec(new Runnable() {
            @Override
            public void run()
            {
               SourceProvider.getInstance().updateAccessRights(session.getUserSystemRights());
            }
         });

         monitor.setTaskName(Messages.get().LoginJob_init_extensions);
         TweakletManager.postLogin(session);
         callLoginListeners(session);
         monitor.worked(5);
         
         setupSessionListener(session, display);

         Activator.logInfo("Creating keepalive timer");
         new KeepAliveTimer(session).start();
      }
      catch(Exception e)
      {
         throw new InvocationTargetException(e);
      }
      finally
      {
         monitor.setTaskName(""); //$NON-NLS-1$
         monitor.done();
      }

      Activator.logInfo("Login job completed");
   }

   /**
    * Setup session listener
    * 
    * @param session
    * @param display
    */
   private static void setupSessionListener(final NXCSession session, final Display display)
   {
      session.addListener(new SessionListener() {
         @Override
         public void notificationHandler(SessionNotification n)
         {
            if (n.getCode() == SessionNotification.SYSTEM_ACCESS_CHANGED)
            {
               display.asyncExec(new Runnable() {
                  @Override
                  public void run()
                  {
                     SourceProvider.getInstance().updateAccessRights(session.getUserSystemRights());
                  }
               });
            }
         }
      });
   }

   /**
    * Create new session object.
    * 
    * @param hostName
    * @param port
    * @return
    */
   private NXCSession createSession(String hostName, int port)
   {   
      // Read all registered extensions and create provider with minimal priority
      IConfigurationElement currentElement = null;
      int currentPriotity = 65536;

      final IExtensionRegistry reg = Platform.getExtensionRegistry();
      IConfigurationElement[] elements = reg.getConfigurationElementsFor("org.netxms.ui.eclipse.sessionproviders"); //$NON-NLS-1$
      for(int i = 0; i < elements.length; i++)
      {
         int priority = 65535;
         String value = elements[i].getAttribute("priority"); //$NON-NLS-1$
         if (value != null)
         {
            try
            {
               priority = Integer.parseInt(value);
               if ((priority < 0) || (priority > 65535))
                  priority = 65535;
            }
            catch(NumberFormatException e)
            {
            }
         }
         if (priority < currentPriotity)
         {
            currentElement = elements[i];
            currentPriotity = priority;
         }
      }

      if (currentElement != null)
      {
         try
         {
            SessionProvider p = (SessionProvider)currentElement.createExecutableExtension("class"); //$NON-NLS-1$
            return p.createSession(hostName, port, encryptSession);
         }
         catch(CoreException e)
         {
         }
      }

      return new NXCSession(hostName, port, encryptSession);
   }

   /**
    * Inform all registered login listeners about successful login
    * 
    * @param session new client session
    */
   private void callLoginListeners(NXCSession session)
   {
      // Read all registered extensions and create listeners
      final IExtensionRegistry reg = Platform.getExtensionRegistry();
      IConfigurationElement[] elements = reg.getConfigurationElementsFor("org.netxms.ui.eclipse.loginlisteners"); //$NON-NLS-1$
      for(int i = 0; i < elements.length; i++)
      {
         try
         {
            final ConsoleLoginListener listener = (ConsoleLoginListener)elements[i].createExecutableExtension("class"); //$NON-NLS-1$
            listener.afterLogin(session, display);
         }
         catch(CoreException e)
         {
            Activator.logError("Exception in login listener", e); //$NON-NLS-1$
         }
      }
   }

   /**
    * Set password for this login job
    * 
    * @param password
    */
   public void setPassword(String password)
   {
      this.password = password;
      authMethod = AuthenticationType.PASSWORD;
   }

   /**
    * Set certificate and signature for this login job
    * 
    * @param signature
    */
   public void setCertificate(Certificate certificate, Signature signature)
   {
      this.certificate = certificate;
      this.signature = signature;
      authMethod = AuthenticationType.CERTIFICATE;
   }
}
