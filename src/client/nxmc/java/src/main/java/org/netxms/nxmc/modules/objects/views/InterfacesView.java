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
package org.netxms.nxmc.modules.objects.views;

import java.util.List;
import java.util.Set;
import org.eclipse.jface.action.Action;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.widgets.Composite;
import org.netxms.base.MacAddress;
import org.netxms.client.objects.AbstractNode;
import org.netxms.client.objects.AbstractObject;
import org.netxms.client.objects.Interface;
import org.netxms.nxmc.PreferenceStore;
import org.netxms.nxmc.base.widgets.SortableTableViewer;
import org.netxms.nxmc.localization.LocalizationHelper;
import org.netxms.nxmc.modules.objects.views.helpers.InterfaceListComparator;
import org.netxms.nxmc.modules.objects.views.helpers.InterfaceListFilter;
import org.netxms.nxmc.modules.objects.views.helpers.InterfaceListLabelProvider;
import org.netxms.nxmc.resources.ResourceManager;
import org.netxms.nxmc.tools.WidgetHelper;
import org.xnap.commons.i18n.I18n;

/**
 * "Interfaces" view
 */
public class InterfacesView extends NodeSubObjectTableView
{
   private static final I18n i18n = LocalizationHelper.getI18n(InterfacesView.class);

   public static final int COLUMN_ID = 0;
   public static final int COLUMN_NAME = 1;
   public static final int COLUMN_ALIAS = 2;
   public static final int COLUMN_TYPE = 3;
   public static final int COLUMN_INDEX = 4;
   public static final int COLUMN_PHYSICAL_LOCATION = 5;
   public static final int COLUMN_MTU = 6;
   public static final int COLUMN_SPEED = 7;
   public static final int COLUMN_DESCRIPTION = 8;
   public static final int COLUMN_MAC_ADDRESS = 9;
   public static final int COLUMN_VENDOR = 10;
   public static final int COLUMN_IP_ADDRESS = 11;
   public static final int COLUMN_VLAN = 12;
   public static final int COLUMN_OSPF_AREA = 13;
   public static final int COLUMN_OSPF_TYPE = 14;
   public static final int COLUMN_OSPF_STATE = 15;
   public static final int COLUMN_PEER_NODE = 16;
   public static final int COLUMN_PEER_INTERFACE = 17;
   public static final int COLUMN_PEER_MAC_ADDRESS = 18;
   public static final int COLUMN_PEER_IP_ADDRESS = 19;
   public static final int COLUMN_PEER_PROTOCOL = 20;
   public static final int COLUMN_ADMIN_STATE = 21;
   public static final int COLUMN_OPER_STATE = 22;
   public static final int COLUMN_EXPECTED_STATE = 23;
   public static final int COLUMN_STATUS = 24;
   public static final int COLUMN_8021X_PAE_STATE = 25;
   public static final int COLUMN_8021X_BACKEND_STATE = 26;

   private InterfaceListLabelProvider labelProvider;
   private Action actionCopyMacAddressToClipboard;
   private Action actionCopyIpAddressToClipboard;
   private Action actionCopyPeerNameToClipboard;
   private Action actionCopyPeerMacToClipboard;
   private Action actionCopyPeerIpToClipboard;

   private boolean hideSubInterfaces = false;

   /**
    * @param name
    * @param image
    * @param id
    */
   public InterfacesView()
   {
      super(i18n.tr("Interfaces"), ResourceManager.getImageDescriptor("icons/object-views/interfaces.png"), "Interfaces", true);
   }

   /**
    * @see org.netxms.nxmc.base.views.View#getPriority()
    */
   @Override
   public int getPriority()
   {
      return 50;
   }

   /**
    * @see org.netxms.nxmc.modules.objects.views.NodeSubObjectTableView#createContent(org.eclipse.swt.widgets.Composite)
    */
   @Override
   protected void createContent(Composite parent)
   {
      super.createContent(parent);
      hideSubInterfaces = PreferenceStore.getInstance().getAsBoolean("InterfacesView.HideSubInterfaces", hideSubInterfaces);
   }

   /**
    * @see org.netxms.nxmc.modules.objects.views.NodeSubObjectTableView#createViewer()
    */
   @Override
   protected void createViewer()
   {
      final String[] names = { 
         i18n.tr("ID"),
         i18n.tr("Name"),
         i18n.tr("Alias"),
         i18n.tr("Type"),
         i18n.tr("Index"),
         i18n.tr("Location"),
         i18n.tr("MTU"),
         i18n.tr("Speed"),
         i18n.tr("Description"),
         i18n.tr("MAC address"),
         i18n.tr("NIC Vendor"),
         i18n.tr("IP addresses"),
         i18n.tr("VLAN"),
         i18n.tr("OSPF area"),
         i18n.tr("OSPF type"),
         i18n.tr("OSPF state"),
         i18n.tr("Peer node"),
         i18n.tr("Peer interface"),
         i18n.tr("Peer MAC"),
         i18n.tr("Peer IP"),
         i18n.tr("Peer discovery protocol"),
         i18n.tr("Administrative state"),
         i18n.tr("Operational state"),
         i18n.tr("Expected state"),
         i18n.tr("Status"),
         i18n.tr("802.1x PAE"),
         i18n.tr("802.1x Backend")
      };
      final int[] widths = { 60, 150, 150, 150, 70, 100, 70, 90, 150, 100, 90, 200, 80, 80, 80, 80, 150, 150, 100, 90, 80, 80, 80, 80, 80, 80, 80 };
      viewer = new SortableTableViewer(mainArea, names, widths, COLUMN_NAME, SWT.UP, SWT.FULL_SELECTION | SWT.MULTI);
      labelProvider = new InterfaceListLabelProvider(viewer);
      viewer.setLabelProvider(labelProvider);
      viewer.setContentProvider(new ArrayContentProvider());
      viewer.setComparator(new InterfaceListComparator());
      filter = new InterfaceListFilter();
      ((InterfaceListFilter)filter).setHideSubInterfaces(hideSubInterfaces);
      setFilterClient(viewer, filter);
      viewer.addFilter(filter);
      WidgetHelper.restoreTableViewerSettings(viewer, "InterfacesView.TableViewer");
      viewer.getTable().addDisposeListener(new DisposeListener() {
         @Override
         public void widgetDisposed(DisposeEvent e)
         {
            WidgetHelper.saveColumnSettings(viewer.getTable(), "InterfacesView.TableViewer");
            PreferenceStore.getInstance().set("InterfacesView.HideSubInterfaces", hideSubInterfaces);
         }
      });
   }

   /**
    * @see org.netxms.nxmc.modules.objects.views.NodeSubObjectTableView#createActions()
    */
   @Override
   protected void createActions()
   {
      super.createActions();

      actionCopyMacAddressToClipboard = new Action(i18n.tr("Copy MAC address to clipboard")) {
         @Override
         public void run()
         {
            copyMacAddress(false);
         }
      };

      actionCopyIpAddressToClipboard = new Action(i18n.tr("Copy IP address to clipboard")) {
         @Override
         public void run()
         {
            copyToClipboard(COLUMN_IP_ADDRESS);
         }
      };

      actionCopyPeerNameToClipboard = new Action(i18n.tr("Copy peer node name to clipboard")) {
         @Override
         public void run()
         {
            copyToClipboard(COLUMN_PEER_NODE);
         }
      };

      actionCopyPeerMacToClipboard = new Action(i18n.tr("Copy peer MAC address to clipboard")) {
         @Override
         public void run()
         {
            copyMacAddress(true);
         }
      };

      actionCopyPeerIpToClipboard = new Action(i18n.tr("Copy peer IP address to clipboard")) {
         @Override
         public void run()
         {
            copyToClipboard(COLUMN_PEER_IP_ADDRESS);
         }
      };
   }

   /**
    * @see org.netxms.nxmc.modules.objects.views.NodeSubObjectTableView#fillContextMenu(org.eclipse.jface.action.IMenuManager)
    */
   @Override
   protected void fillContextMenu(IMenuManager manager)
   {
      manager.add(actionCopyToClipboard);
      manager.add(actionCopyMacAddressToClipboard);
      manager.add(actionCopyIpAddressToClipboard);
      manager.add(actionCopyPeerNameToClipboard);
      manager.add(actionCopyPeerMacToClipboard);
      manager.add(actionCopyPeerIpToClipboard);
      manager.add(actionExportToCsv);
   }

   /**
    * Copy mac address
    */
   private void copyMacAddress(boolean peerMacAddress)
   {
      @SuppressWarnings("unchecked")
      final List<Interface> selection = viewer.getStructuredSelection().toList();
      if (selection.size() > 0)
      {
         final String newLine = WidgetHelper.getNewLineCharacters();
         final StringBuilder sb = new StringBuilder();
         for(int i = 0; i < selection.size(); i++)
         {
            if (i > 0)
               sb.append(newLine);
            
            MacAddress addr = null;
            if (peerMacAddress)
            {
               addr = labelProvider.getPeerMacAddress(selection.get(i));
            }
            else
            {
               addr = selection.get(i).getMacAddress();
            }
            sb.append(addr != null ? addr.toString() : "");
         }
         WidgetHelper.copyToClipboard(sb.toString());
      }      
   }

   /**
    * @see org.netxms.nxmc.modules.objects.views.NodeSubObjectView#refresh()
    */
   @Override
   public void refresh()
   {
      if (getObject() != null)
         viewer.setInput(getObject().getAllChildren(AbstractObject.OBJECT_INTERFACE).toArray());
      else
         viewer.setInput(new Interface[0]);
   }

   /**
    * Show/hide subinterfaces
    *
    * @param hide true to hide subinterfaces
    */
   public void hideSubInterfaces(boolean hide)
   {
      hideSubInterfaces = hide;
      ((InterfaceListFilter)filter).setHideSubInterfaces(hide);
      viewer.refresh();
   }

   /**
    * @see org.netxms.nxmc.modules.objects.views.NodeSubObjectView#needRefreshOnObjectChange(org.netxms.client.objects.AbstractObject)
    */
   @Override
   public boolean needRefreshOnObjectChange(AbstractObject object)
   {
      return (object instanceof Interface) && object.isChildOf(getObjectId());
   }

   /**
    * @see org.netxms.nxmc.modules.objects.views.NodeSubObjectView#onObjectChange(org.netxms.client.objects.AbstractObject)
    */
   @Override
   protected void onObjectChange(AbstractObject object)
   {
      labelProvider.setNode((AbstractNode)object);
      super.onObjectChange(object);
   }

   /**
    * @see org.netxms.nxmc.modules.objects.views.NodeSubObjectView#collectObjectsForChildrenSync(org.netxms.client.objects.AbstractObject, java.util.Set)
    */
   @Override
   protected void collectObjectsForChildrenSync(AbstractObject object, Set<AbstractObject> objectsForSync)
   {
      super.collectObjectsForChildrenSync(object, objectsForSync);

      for(AbstractObject o : object.getAllChildren(AbstractObject.OBJECT_INTERFACE))
      {
         AbstractObject peerNode = session.findObjectById(((Interface)o).getPeerNodeId());
         if ((peerNode != null) && !peerNode.areChildrenSynchronized())
            objectsForSync.add(peerNode);
      }
   }
}
