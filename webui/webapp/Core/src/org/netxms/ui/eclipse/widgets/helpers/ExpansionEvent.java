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
package org.netxms.ui.eclipse.widgets.helpers;

import org.eclipse.swt.events.TypedEvent;

/**
 * Widget expansion state change event
 */
public class ExpansionEvent extends TypedEvent
{
   /**
    * Creates a new expansion event.
    *
    * @param source event source
    * @param state the new expansion state
    */
   public ExpansionEvent(Object source, boolean state)
   {
      super(source);
      data = state ? Boolean.TRUE : Boolean.FALSE;
   }

   /**
    * Returns the new expansion state of the widget.
    *
    * @return <code>true</code> if the widget is now expaned, <code>false</code> otherwise.
    */
   public boolean getState()
   {
      return data.equals(Boolean.TRUE);
   }
}
