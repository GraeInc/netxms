/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2021 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: layer2.cpp
**
**/

#include "nxcore.h"

/**
 * Build layer 2 topology for switch
 */
void BuildL2Topology(NetworkMapObjectList &topology, Node *root, int depth, bool includeEndNodes, bool useL1Topology)
{
	if (topology.isObjectExist(root->getId()))
		return;  // loop in object connections

	topology.addObject(root->getId());

	if (depth <= 0)
	   return;

	shared_ptr<LinkLayerNeighbors> nbs = root->getLinkLayerNeighbors();
	if (nbs == nullptr)
		return;

	for(int i = 0; i < nbs->size(); i++)
	{
		LL_NEIGHBOR_INFO *info = nbs->getConnection(i);
		if (info != nullptr)
		{
			shared_ptr<Node> node = static_pointer_cast<Node>(FindObjectById(info->objectId, OBJECT_NODE));
			if ((node != nullptr) && (node->isBridge() || includeEndNodes))
			{
				BuildL2Topology(topology, node.get(), depth - 1, includeEndNodes, useL1Topology);
				shared_ptr<Interface> ifLocal = root->findInterfaceByIndex(info->ifLocal);
				shared_ptr<Interface> ifRemote = node->findInterfaceByIndex(info->ifRemote);
				nxlog_debug_tag(_T("topo.layer2"), 5, _T("BuildL2Topology: root=%s [%d], node=%s [%d], ifLocal=%d %s, ifRemote=%d %s"),
                  root->getName(), root->getId(), node->getName(), node->getId(), info->ifLocal,
                  (ifLocal != nullptr) ? ifLocal->getName() : _T("(null)"), info->ifRemote,
                  (ifRemote != nullptr) ? ifRemote->getName() : _T("(null)"));
				topology.linkObjectsEx(root->getId(), node->getId(),
                  (ifLocal != nullptr) ? ifLocal->getName() : _T("N/A"),
                  (ifRemote != nullptr) ? ifRemote->getName() : _T("N/A"),
                  info->ifLocal, info->ifRemote);
			}
		}
	}

	if(!useL1Topology)
	   return;

	const ObjectArray<L1_NEIGHBOR_INFO> &l1Neibors = GetL1Neighbors(root);
   for(int i = 0; i < l1Neibors.size(); i++)
   {
      L1_NEIGHBOR_INFO *info = l1Neibors.get(i);
      shared_ptr<Node> node = static_pointer_cast<Node>(FindObjectById(info->objectId, OBJECT_NODE));
      if ((node != nullptr) && (node->isBridge() || includeEndNodes))
      {
         BuildL2Topology(topology, node.get(), depth - 1, includeEndNodes, useL1Topology);
         shared_ptr<Interface> ifLocal = static_pointer_cast<Interface>(FindObjectById(info->ifLocal, OBJECT_INTERFACE));
         shared_ptr<Interface> ifRemote = static_pointer_cast<Interface>(FindObjectById(info->ifRemote, OBJECT_INTERFACE));
         uint32_t ifLocalIndex = ifLocal->getIfIndex();
         uint32_t ifRemoteIndex = ifRemote->getIfIndex();
         nxlog_debug_tag(_T("topo.layer2"), 5, _T("BuildL1Topology: root=%s [%d], node=%s [%d], ifLocal=%d %s, ifRemote=%d %s"),
               root->getName(), root->getId(), node->getName(), node->getId(), ifLocalIndex,
               (ifLocal != nullptr) ? ifLocal->getName() : _T("(null)"), ifRemoteIndex,
               (ifRemote != nullptr) ? ifRemote->getName() : _T("(null)"));
         topology.linkObjectsEx(root->getId(), node->getId(),
               (ifLocal != nullptr) ? ifLocal->getName() : _T("N/A"),
               (ifRemote != nullptr) ? ifRemote->getName() : _T("N/A"),
                     ifLocalIndex, ifRemoteIndex, info->routeInfo);
      }
   }
}

/**
 * Find connection point for interface
 */
shared_ptr<NetObj> FindInterfaceConnectionPoint(const MacAddress& macAddr, int *type)
{
   TCHAR macAddrText[64];
   nxlog_debug(6, _T("Called FindInterfaceConnectionPoint(%s)"), macAddr.toString(macAddrText));

   *type = CP_TYPE_INDIRECT;

   if (!macAddr.isValid() || (macAddr.length() != MAC_ADDR_LENGTH))
      return shared_ptr<NetObj>();

	shared_ptr<NetObj> cp;
	unique_ptr<SharedObjectArray<NetObj>> nodes = g_idxNodeById.getObjects();

	Node *bestMatchNode = nullptr;
	uint32_t bestMatchIfIndex = 0;
	int bestMatchCount = 0x7FFFFFFF;

	for(int i = 0; (i < nodes->size()) && (cp == nullptr); i++)
	{
		Node *node = static_cast<Node*>(nodes->get(i));
		
      shared_ptr<ForwardingDatabase> fdb = node->getSwitchForwardingDatabase();
		if (fdb != nullptr)
		{
			DbgPrintf(6, _T("FindInterfaceConnectionPoint(%s): FDB obtained for node %s [%d]"),
			          macAddrText, node->getName(), (int)node->getId());
         bool isStatic;
         uint32_t ifIndex = fdb->findMacAddress(macAddr.value(), &isStatic);
			if (ifIndex != 0)
			{
			   DbgPrintf(6, _T("FindInterfaceConnectionPoint(%s): MAC address found on interface %d (%s)"),
                      macAddrText, ifIndex, isStatic ? _T("static") : _T("dynamic"));
				int count = fdb->getMacCountOnPort(ifIndex);
				if (count == 1)
				{
               if (isStatic)
               {
                  // keep it as best match and continue search for dynamic connection
   					bestMatchCount = count;
					   bestMatchNode = node;
					   bestMatchIfIndex = ifIndex;
               }
               else
               {
					   shared_ptr<Interface> iface = node->findInterfaceByIndex(ifIndex);
					   if (iface != nullptr)
					   {
						   DbgPrintf(4, _T("FindInterfaceConnectionPoint(%s): found interface %s [%u] on node %s [%u]"), macAddrText,
									    iface->getName(), iface->getId(), iface->getParentNodeName().cstr(), iface->getParentNodeId());
                     cp = iface;
                     *type = CP_TYPE_DIRECT;
					   }
					   else
					   {
						   DbgPrintf(4, _T("FindInterfaceConnectionPoint(%s): cannot find interface object for node %s [%d] ifIndex %d"),
									    macAddrText, node->getName(), node->getId(), ifIndex);
					   }
               }
				}
            else if (count < bestMatchCount)
				{
					bestMatchCount = count;
					bestMatchNode = node;
					bestMatchIfIndex = ifIndex;
					DbgPrintf(4, _T("FindInterfaceConnectionPoint(%s): found potential interface [ifIndex=%d] on node %s [%d], count %d"), 
					          macAddrText, ifIndex, node->getName(), (int)node->getId(), count);
				}
			}
		}

      if (node->isWirelessController())
      {
			DbgPrintf(6, _T("FindInterfaceConnectionPoint(%s): node %s [%d] is a wireless controller, checking associated stations"),
			          macAddrText, node->getName(), (int)node->getId());
         ObjectArray<WirelessStationInfo> *wsList = node->getWirelessStations();
         if (wsList != nullptr)
         {
			   DbgPrintf(6, _T("FindInterfaceConnectionPoint(%s): %d wireless stations registered on node %s [%d]"),
                      macAddrText, wsList->size(), node->getName(), (int)node->getId());

            for(int i = 0; i < wsList->size(); i++)
            {
               WirelessStationInfo *ws = wsList->get(i);
               if (!memcmp(ws->macAddr, macAddr.value(), MAC_ADDR_LENGTH))
               {
                  auto ap = static_pointer_cast<AccessPoint>(FindObjectById(ws->apObjectId, OBJECT_ACCESSPOINT));
                  if (ap != nullptr)
                  {
						   DbgPrintf(4, _T("FindInterfaceConnectionPoint(%s): found matching wireless station on node %s [%d] AP %s"), macAddrText,
									    node->getName(), (int)node->getId(), ap->getName());
                     cp = ap;
                     *type = CP_TYPE_WIRELESS;
                  }
                  else
                  {
                     shared_ptr<Interface> iface = node->findInterfaceByIndex(ws->rfIndex);
                     if (iface != nullptr)
                     {
						      DbgPrintf(4, _T("FindInterfaceConnectionPoint(%s): found matching wireless station on node %s [%d] interface %s"),
                           macAddrText, node->getName(), (int)node->getId(), iface->getName());
                        cp = iface;
                        *type = CP_TYPE_WIRELESS;
                     }
                     else
                     {
						      DbgPrintf(4, _T("FindInterfaceConnectionPoint(%s): found matching wireless station on node %s [%d] but cannot determine AP or interface"), 
                           macAddrText, node->getName(), (int)node->getId());
                     }
                  }
                  break;
               }
            }

            delete wsList;
         }
         else
         {
			   DbgPrintf(6, _T("FindInterfaceConnectionPoint(%s): unable to get wireless stations from node %s [%d]"),
			             macAddrText, node->getName(), (int)node->getId());
         }
      }
	}

	if ((cp == nullptr) && (bestMatchNode != nullptr))
	{
		cp = bestMatchNode->findInterfaceByIndex(bestMatchIfIndex);
      if (bestMatchCount == 1)
      {
         // static best match
         *type = CP_TYPE_DIRECT;
      }
	}

	return cp;
}
