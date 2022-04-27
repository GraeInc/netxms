package org.netxms.ui.eclipse.widgets;

import java.util.HashSet;
import java.util.Set;
import org.eclipse.jface.action.Action;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.netxms.ui.eclipse.console.resources.SharedIcons;
import org.netxms.ui.eclipse.widgets.helpers.DashboardElementButton;
import org.netxms.ui.eclipse.widgets.helpers.ExpansionEvent;
import org.netxms.ui.eclipse.widgets.helpers.ExpansionListener;

/**
 * Section widget - extension of Card widget that contains externally accessible client area and can be collapsed/expanded.
 */
public class Section extends Card
{
   private Composite clientArea;
   private DashboardElementButton expandButton;
   private boolean expanded = true;
   private Set<ExpansionListener> expansionListeners = new HashSet<>();

   /**
    * Create section widget.
    *
    * @param parent parent composite
    * @param title section title
    * @param collapsible true to allow section collapse/expand
    */
   public Section(Composite parent, String title, boolean collapsible)
   {
      super(parent, title);

      if (collapsible)
      {
         expandButton = new DashboardElementButton("Collapse", SharedIcons.IMG_COLLAPSE, new Action() {
            @Override
            public void run()
            {
               setExpanded(!expanded);
            }
         });
         addButton(expandButton);
      }
   }

   /**
    * @see org.netxms.ui.eclipse.widgets.Card#createClientArea(org.eclipse.swt.widgets.Composite)
    */
   @Override
   protected Control createClientArea(Composite parent)
   {
      clientArea = new Composite(parent, SWT.NONE);
      clientArea.setLayout(new FillLayout());
      return clientArea;
   }

   /**
    * Set expanded state for this section.
    *
    * @param expanded true to make section expanded
    */
   public void setExpanded(boolean expanded)
   {
      if (this.expanded == expanded)
         return;

      this.expanded = expanded;
      expandButton.setImage(expanded ? SharedIcons.IMG_COLLAPSE : SharedIcons.IMG_EXPAND);
      expandButton.setName(expanded ? "Collapse" : "Expand");
      updateButtons();
      showClientArea(expanded);

      ExpansionEvent e = new ExpansionEvent(this, expanded);
      for(ExpansionListener l : expansionListeners)
         l.expansionStateChanged(e);

      getParent().layout(true, true);
   }

   /**
    * Add expansion listener. Has no effect if same listener already added.
    *
    * @param listener listener to add
    */
   public void addExpansionListener(ExpansionListener listener)
   {
      expansionListeners.add(listener);
   }

   /**
    * Remove expansion listener.
    *
    * @param listener listener to remove
    */
   public void removeExpansionListener(ExpansionListener listener)
   {
      expansionListeners.remove(listener);
   }

   /**
    * @return the clientArea
    */
   public Composite getClient()
   {
      return clientArea;
   }
}
