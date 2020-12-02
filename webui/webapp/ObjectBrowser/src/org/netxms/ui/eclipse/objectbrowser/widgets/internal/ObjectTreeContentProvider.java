/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2020 Raden Solutions
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
package org.netxms.ui.eclipse.objectbrowser.widgets.internal;

import java.util.Iterator;
import org.eclipse.jface.viewers.TreeNodeContentProvider;
import org.eclipse.jface.viewers.Viewer;
import org.netxms.client.NXCSession;
import org.netxms.client.objects.AbstractObject;
import org.netxms.client.objects.LoadingObject;
import org.netxms.client.objects.Node;

/**
 * Content provider for object tree.
 */
public class ObjectTreeContentProvider extends TreeNodeContentProvider
{
	private NXCSession session = null;
	private long[] rootObjects = null;
	private AbstractObject loadingObject;
	private boolean objectFullSync = false;
	
	/**
	 * @param rootObjects
	 * @param objectFullSync 
	 */
	public ObjectTreeContentProvider(long[] rootObjects, boolean objectFullSync)
	{
		super();
		this.objectFullSync = objectFullSync;
		if (rootObjects != null)
		{
			this.rootObjects = new long[rootObjects.length];
			System.arraycopy(rootObjects, 0, this.rootObjects, 0, rootObjects.length);
		}
		loadingObject = new LoadingObject(-1, session);
		
	}

   /**
	 * @see org.eclipse.jface.viewers.TreeNodeContentProvider#getChildren(java.lang.Object)
	 */
	@Override
	public Object[] getChildren(Object parentElement)
	{
      AbstractObject object = (AbstractObject)parentElement;
      if (!objectFullSync && (object instanceof Node) && object.hasChildren() && !session.areChildrenSynchronized(object.getObjectId()))
         return new AbstractObject[] { loadingObject };
      return object.getChildrenAsArray();
	}

   /**
    * @see org.eclipse.jface.viewers.TreeNodeContentProvider#getElements(java.lang.Object)
    */
	@Override
	public Object[] getElements(Object inputElement)
	{
		if (session != null)
		{
			return (rootObjects != null) ? session.findMultipleObjects(rootObjects, false).toArray() : session.getTopLevelObjects();
		}
		return new AbstractObject[0];
	}

   /**
    * @see org.eclipse.jface.viewers.TreeNodeContentProvider#getParent(java.lang.Object)
    */
	@Override
	public Object getParent(Object element)
	{
		if (session == null)
			return null;
		
		Iterator<Long> it = ((AbstractObject)element).getParents();
		return it.hasNext() ? session.findObjectById(it.next()) : null;
	}

	/**
	 * @see org.eclipse.jface.viewers.TreeNodeContentProvider#hasChildren(java.lang.Object)
	 */
	@Override
	public boolean hasChildren(Object element)
	{
		return (element instanceof Node) ? ((AbstractObject)element).hasChildren() : ((AbstractObject)element).hasAccessibleChildren();
	}

	/**
	 * @see org.eclipse.jface.viewers.TreeNodeContentProvider#inputChanged(org.eclipse.jface.viewers.Viewer, java.lang.Object, java.lang.Object)
	 */
	@Override
	public void inputChanged(Viewer viewer, Object oldInput, Object newInput)
	{
		session = (NXCSession)newInput;
	}

	/**
	 * @param rootObjects the rootObjects to set
	 */
	public void setRootObjects(long[] rootObjects)
	{
		if (rootObjects != null)
		{
			this.rootObjects = new long[rootObjects.length];
			System.arraycopy(rootObjects, 0, this.rootObjects, 0, rootObjects.length);
		}
		else
		{
			this.rootObjects = null;
		}
	}
}
