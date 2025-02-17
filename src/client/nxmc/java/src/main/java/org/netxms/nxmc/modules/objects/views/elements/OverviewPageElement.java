/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2023 Victor Kirhenshtein
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
package org.netxms.nxmc.modules.objects.views.elements;

import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.netxms.client.objects.AbstractObject;
import org.netxms.nxmc.base.widgets.Card;
import org.netxms.nxmc.modules.objects.views.ObjectView;

/**
 * Abstract element of object's "Overview" view
 */
public abstract class OverviewPageElement
{
	private AbstractObject object = null;
	private Composite parent;
	private Card widget = null;
	private OverviewPageElement anchor;
   private ObjectView objectView;
	
	/**
    * Create element.
    *
    * @param parent parent composite
    * @param anchor anchor element
    * @param objectView owning view
    */
   public OverviewPageElement(Composite parent, OverviewPageElement anchor, ObjectView objectView)
	{
		this.parent = parent;
		this.anchor = anchor;
      this.objectView = objectView;
	}
	
	/**
	 * @return the object
	 */
	public AbstractObject getObject()
	{
		return object;
	}

	/**
	 * @param object the object to set
	 */
	public void setObject(AbstractObject object)
	{
		this.object = object;
		if ((widget == null) || widget.isDisposed())
		{
			widget = new Card(parent, getTitle()) {
				@Override
				protected Control createClientArea(Composite parent)
				{
					return OverviewPageElement.this.createClientArea(parent);
				}
			};
			GridData gd = new GridData();
			gd.horizontalAlignment = SWT.FILL;
			gd.grabExcessHorizontalSpace = true;
			gd.verticalAlignment = SWT.TOP;
			widget.setLayoutData(gd);
		}
		onObjectChange();
	}
	
	/**
	 * Dispose widget associated with this element
	 */
	public void dispose()
	{
		if ((widget != null) && !widget.isDisposed())
			widget.dispose();
	}
	
	/**
	 * Check if element is applicable for given object. Default implementation
	 * always returns true.
	 * 
	 * @param object object to test
	 * @return true if element is applicable
	 */
	public boolean isApplicableForObject(AbstractObject object)
	{
		return true;
	}
	
	/**
	 * Fix control placement
	 */
	public void fixPlacement()
	{
		if ((widget != null) && !widget.isDisposed())
		{
			if (anchor == null)
			{
				widget.moveAbove(null);
			}
			else
			{
				widget.moveBelow(anchor.getAnchorControl());
			}
		}
	}
	
	/**
	 * @return
	 */
	private Control getAnchorControl()
	{
		if ((widget != null) && !widget.isDisposed())
			return widget;
		if (anchor != null)
			return anchor.getAnchorControl();
		return null;
	}
	
	/**
	 * @return
	 */
	protected Display getDisplay()
	{
		return ((widget != null) && !widget.isDisposed()) ? widget.getDisplay() : Display.getCurrent();
	}
	
	/**
	 * @return
	 */
   protected ObjectView getObjectView()
	{
      return objectView;
	}

	/**
	 * Create client area.
	 * 
	 * @param parent
	 * @return
	 */
	protected abstract Control createClientArea(Composite parent);
	
	/**
	 * Handler for object change.
	 */
	protected abstract void onObjectChange();
	
	/**
	 * Get element's title
	 * 
	 * @return element's title
	 */
	protected abstract String getTitle();
}
