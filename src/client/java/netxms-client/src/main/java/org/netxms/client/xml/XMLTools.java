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
package org.netxms.client.xml;

import java.util.UUID;
import org.simpleframework.xml.Serializer;
import org.simpleframework.xml.convert.AnnotationStrategy;
import org.simpleframework.xml.convert.Registry;
import org.simpleframework.xml.convert.RegistryStrategy;
import org.simpleframework.xml.core.Persister;
import org.simpleframework.xml.filter.Filter;

/**
 * Tools for XML conversion
 */
public final class XMLTools
{
   /**
    * Create serializer with registered converters
    * 
    * @return serializer with registered converters
    * @throws Exception on XML library failures
    */
   public static Serializer createSerializer() throws Exception
   {
      return createSerializer(new Registry());
   }

   /**
    * Create serializer with registered converters using caller provided registry.
    * 
    * @param registry registry to use for custom annotation strategy
    * @return serializer with registered converters
    * @throws Exception on XML library failures
    */
   public static Serializer createSerializer(Registry registry) throws Exception
   {
      registry.bind(UUID.class, UUIDConverter.class);
      // Add dummy filter to prevent expansion of ${name} in XML data
      return new Persister(new AnnotationStrategy(new RegistryStrategy(registry)), new Filter() {
         @Override
         public String replace(String text)
         {
            return null;
         }
      });
   }
}
