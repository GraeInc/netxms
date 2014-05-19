/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2013 Victor Kirhenshtein
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
package org.netxms.client;

import org.netxms.client.maps.NetworkMapLink;
import org.netxms.client.maps.NetworkMapPage;
import org.netxms.client.maps.elements.NetworkMapElement;


/**
 * Tests for network topology functions
 */
public class TopologyTest extends SessionTest
{
   private static final long SUBNET_ID = 796;
   private static final long NODE_ID = 603;
   
   public void testAddressMap() throws Exception
   {
      final NXCSession session = connect();

      long[] map = session.getSubnetAddressMap(SUBNET_ID);
      for(int i = 0; i < map.length; i++)
         System.out.println(i + ": " + map[i]);
      
      session.disconnect();
   }

   public void testLinkLayerTopology() throws Exception
   {
      final NXCSession session = connect();

      NetworkMapPage page = session.queryLayer2Topology(NODE_ID);
      for(NetworkMapElement e : page.getElements())
         System.out.println(e.toString());
      for(NetworkMapLink l : page.getLinks())
         System.out.println(l.toString());
      
      session.disconnect();
   }
}
