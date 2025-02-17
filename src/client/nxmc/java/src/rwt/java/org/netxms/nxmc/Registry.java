/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2023 Raden Solutions
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
package org.netxms.nxmc;

import java.io.File;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.TimeZone;
import org.eclipse.rap.rwt.RWT;
import org.eclipse.swt.widgets.Display;
import org.netxms.client.NXCSession;
import org.netxms.nxmc.base.views.ConfigurationPerspective;
import org.netxms.nxmc.base.views.MonitorPerspective;
import org.netxms.nxmc.base.views.Perspective;
import org.netxms.nxmc.base.views.PinLocation;
import org.netxms.nxmc.base.views.PinboardPerspective;
import org.netxms.nxmc.base.views.ToolsPerspective;
import org.netxms.nxmc.base.windows.MainWindow;
import org.netxms.nxmc.modules.alarms.AlarmsPerspective;
import org.netxms.nxmc.modules.businessservice.BusinessServicesPerspective;
import org.netxms.nxmc.modules.datacollection.GraphsPerspective;
import org.netxms.nxmc.modules.logviewer.views.LogViewerPerspective;
import org.netxms.nxmc.modules.objects.DashboardsPerspective;
import org.netxms.nxmc.modules.objects.InfrastructurePerspective;
import org.netxms.nxmc.modules.objects.MapsPerspective;
import org.netxms.nxmc.modules.objects.NetworkPerspective;
import org.netxms.nxmc.modules.objects.TemplatesPerspective;
import org.netxms.nxmc.modules.objects.views.helpers.PollManager;
import org.netxms.nxmc.modules.worldmap.WorldMapPerspective;

/**
 * Global registry
 */
public final class Registry
{
   public static final boolean IS_WEB_CLIENT = true;

   /**
    * Get registry instance.
    *
    * @return registry instance
    */
   private static Registry getInstance()
   {
      return getInstance(Display.getCurrent());
   }

   /**
    * Get registry instance.
    *
    * @param display display to use for registry access
    * @return registry instance
    */
   private static Registry getInstance(Display display)
   {
      Registry instance = (Registry)RWT.getUISession(display).getAttribute("netxms.registry");
      if (instance == null)
      {
         instance = new Registry();
         RWT.getUISession(display).setAttribute("netxms.registry", instance);
      }
      return instance;
   }

   /**
    * Get current NetXMS client library session
    * 
    * @return Current session
    */
   public static NXCSession getSession()
   {
      return getInstance().session;
   }

   /**
    * Get current NetXMS client library session
    *
    * @param display display to use
    * @return Current session
    */
   public static NXCSession getSession(Display display)
   {
      return getInstance(display).session;
   }

   /**
    * Get client timezone
    * 
    * @return
    */
   public static TimeZone getTimeZone()
   {
      return getInstance().timeZone;
   }

   /**
    * Get application's state directory.
    * 
    * @return application's state directory
    */
   public static File getStateDir()
   {
      return getInstance().stateDir;
   }

   /**
    * Get application's state directory.
    * 
    * @param display display to use
    * @return application's state directory
    */
   public static File getStateDir(Display display)
   {
      return getInstance(display).stateDir;
   }

   /**
    * Get application's main window.
    * 
    * @return application's main window
    */
   public static MainWindow getMainWindow()
   {
      return getInstance().mainWindow;
   }

   /**
    * Get singleton object of given class.
    *
    * @param singletonClass class of singleton object
    * @return singleton object or null
    */
   @SuppressWarnings("unchecked")
   public static <T> T getSingleton(Class<T> singletonClass)
   {
      return (T)RWT.getUISession().getAttribute("netxms.singleton." + singletonClass.getName());
   }

   /**
    * Get singleton object of given class.
    *
    * @param singletonClass class of singleton object
    * @param display display object
    * @return singleton object or null
    */
   @SuppressWarnings("unchecked")
   public static <T> T getSingleton(Class<T> singletonClass, Display display)
   {
      return (T)RWT.getUISession(display).getAttribute("netxms.singleton." + singletonClass.getName());
   }

   /**
    * Set singleton object of given class.
    *
    * @param singletonClass class of singleton object
    * @param singleton singleton object
    */
   public static void setSingleton(Class<?> singletonClass, Object singleton)
   {
      RWT.getUISession().setAttribute("netxms.singleton." + singletonClass.getName(), singleton);
   }

   /**
    * Set singleton object of given class using specific display.
    *
    * @param display display
    * @param singletonClass class of singleton object
    * @param singleton singleton object
    */
   public static void setSingleton(Display display, Class<?> singletonClass, Object singleton)
   {
      RWT.getUISession(display).setAttribute("netxms.singleton." + singletonClass.getName(), singleton);
   }

   /**
    * Get named property.
    *
    * @param name property name
    * @return value of property or null
    */
   public static Object getProperty(String name)
   {
      return RWT.getUISession().getAttribute("netxms." + name);
   }

   /**
    * Get named property using given display.
    *
    * @param name property name
    * @param display display to use
    * @return value of property or null
    */
   public static Object getProperty(String name, Display display)
   {
      return RWT.getUISession(display).getAttribute("netxms." + name);
   }

   /**
    * Set named property.
    *
    * @param name property name
    * @param value property vakue
    */
   public static void setProperty(String name, Object value)
   {
      RWT.getUISession().setAttribute("netxms." + name, value);
   }

   /**
    * Set named property.
    *
    * @param name property name
    * @param value property vakue
    */
   public static void setProperty(Display display, String name, Object value)
   {
      RWT.getUISession(display).setAttribute("netxms." + name, value);
   }

   /**
    * Get application's main window.
    * 
    * @return application's main window
    */
   public static PollManager getPollManager()
   {
      return getInstance().pollManager;
   }

   private NXCSession session = null;
   private TimeZone timeZone = null;
   private File stateDir = null;
   private MainWindow mainWindow = null;
   private PollManager pollManager = null;
   private PinLocation lastViewPinLocation = PinLocation.PINBOARD;
   private Set<Perspective> perspectives = new HashSet<Perspective>();

   /**
    * Default constructor
    */
   private Registry()
   {
      pollManager = new PollManager();

      perspectives.add(new AlarmsPerspective());
      perspectives.add(new BusinessServicesPerspective());
      perspectives.add(new ConfigurationPerspective());
      perspectives.add(new DashboardsPerspective());
      perspectives.add(new GraphsPerspective());
      perspectives.add(new InfrastructurePerspective());
      perspectives.add(new LogViewerPerspective());
      perspectives.add(new MapsPerspective());
      perspectives.add(new MonitorPerspective());
      perspectives.add(new NetworkPerspective());
      perspectives.add(new PinboardPerspective());
      perspectives.add(new TemplatesPerspective());
      perspectives.add(new ToolsPerspective());
      perspectives.add(new WorldMapPerspective());
   }

   /**
    * Get registered perspectives ordered by priority
    *
    * @return the perspectives
    */
   public static List<Perspective> getPerspectives()
   {
      List<Perspective> result = new ArrayList<Perspective>(getInstance().perspectives);
      result.sort(new Comparator<Perspective>() {
         @Override
         public int compare(Perspective p1, Perspective p2)
         {
            return p1.getPriority() - p2.getPriority();
         }
      });
      return result;
   }

   /**
    * Set current NetXMS client library session
    * 
    * @param display current display
    * @param session current session
    */
   public static void setSession(Display display, NXCSession session)
   {
      getInstance(display).session = session;
   }

   /**
    * Set server time zone
    */
   public static void setServerTimeZone()
   {
      Registry instance = getInstance();
      if (instance.session != null)
      {
         String tz = instance.session.getServerTimeZone();
         instance.timeZone = TimeZone.getTimeZone(tz.replaceAll("[A-Za-z]+([\\+\\-][0-9]+).*", "GMT$1"));
      }
   }

   /**
    * Reset time zone to default
    */
   public static void resetTimeZone()
   {
      getInstance().timeZone = null;
   }

   /**
    * Set application state directory (should be called only by startup code).
    *
    * @param stateDir application state directory
    */
   protected static void setStateDir(File stateDir)
   {
      Registry instance = getInstance();
      if (instance.stateDir == null)
         instance.stateDir = stateDir;
   }

   /**
    * Set application's main window (should be called only by startup code).
    *
    * @param window application's main window
    */
   protected static void setMainWindow(MainWindow window)
   {
      Registry instance = getInstance();
      if (instance.mainWindow == null)
         instance.mainWindow = window;
   }

   /**
    * Get last used view pin location
    *
    * @return last used view pin location
    */
   public static PinLocation getLastViewPinLocation()
   {
      return getInstance().lastViewPinLocation;
   }

   /**
    * Set last used view pin location
    * 
    * @param lastViewPinLocation last used view pin location
    */
   public static void setLastViewPinLocation(PinLocation lastViewPinLocation)
   {
      getInstance().lastViewPinLocation = lastViewPinLocation;
   }
}
