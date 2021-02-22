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
package org.netxms.ui.eclipse.nxsl.widgets;

import java.util.Collection;
import java.util.Set;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.rap.rwt.RWT;
import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.CLabel;
import org.eclipse.swt.events.MouseAdapter;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Text;
import org.netxms.client.NXCSession;
import org.netxms.client.ScriptCompilationResult;
import org.netxms.ui.eclipse.console.resources.SharedIcons;
import org.netxms.ui.eclipse.console.resources.ThemeEngine;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.nxsl.Activator;
import org.netxms.ui.eclipse.nxsl.Messages;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.tools.MessageDialogHelper;

/**
 * NXSL script editor
 *
 */
public class ScriptEditor extends Composite
{
	private Text editor;
	private String hintText;
	private Composite hintArea;
	private Text hintTextControl = null;
	private Label hintsExpandButton = null;
	private Button compileButton;
   
   private static final Color ERROR_COLOR = new Color(Display.getDefault(), 255, 0, 0);
	
   /**
    * @param parent
    * @param style
    * @param editorStyle
    */
   public ScriptEditor(Composite parent, int style, int editorStyle)
   {
      this(parent, style, editorStyle, true, null, true);
   }

   /**
    * @param parent
    * @param style
    * @param editorStyle
    * @param showLineNumbers
    */
   public ScriptEditor(Composite parent, int style, int editorStyle, boolean showLineNumbers)
   {
      this(parent, style, editorStyle, showLineNumbers, null, true);
   }
   
   /**
    * 
    * @param parent
    * @param style
    * @param editorStyle
    * @param showLineNumbers
    * @param hints
    * @param showCompileButton
    */
   public ScriptEditor(Composite parent, int style, int editorStyle, boolean showLineNumbers, String hints)
   {
      this(parent, style, editorStyle, showLineNumbers, null, true);
   }

   /**
    * 
    * @param parent
    * @param style
    * @param editorStyle
    * @param showLineNumbers
    * @param showCompileButton
    */
   public ScriptEditor(Composite parent, int style, int editorStyle, boolean showLineNumbers, boolean showCompileButton)
   {
      this(parent, style, editorStyle, showLineNumbers, null, showCompileButton);
   }
   
	/**
	 * 
	 * @param parent
	 * @param style
	 * @param editorStyle
	 * @param showLineNumbers
	 * @param hints
	 * @param showCompileButton
	 */
	public ScriptEditor(Composite parent, int style, int editorStyle, boolean showLineNumbers, String hints, boolean showCompileButton)
	{
		super(parent, style);

      hintText = hints;

      GridLayout layout = new GridLayout();
      layout.marginWidth = 0;
      layout.marginHeight = 0;
      layout.verticalSpacing = 0;
      setLayout(layout);
      
      if (hints != null)
      {
         createHintsArea();
      }
		
		editor = new Text(this, editorStyle | SWT.MULTI);
		editor.setData(RWT.CUSTOM_VARIANT, "monospace");
		editor.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, true));

		
      if (showCompileButton)
      {
         compileButton = new Button(this, SWT.PUSH);
         GridData gridData = new GridData();
         gridData.verticalAlignment = GridData.END;
         gridData.horizontalAlignment = GridData.END;
         compileButton.setLayoutData(gridData);
         compileButton.setText("Compile");
         compileButton.addSelectionListener(new SelectionListener() {
            @Override
            public void widgetDefaultSelected(SelectionEvent e)
            {
               widgetSelected(e);
            }

            @Override
            public void widgetSelected(SelectionEvent e)
            {
               compileScript();
            }
         });
      }
	}
	
	/**
	 * Create hints area
	 */
	private void createHintsArea()
	{
      hintArea = new Composite(this, SWT.NONE);
      GridLayout layout = new GridLayout();
      layout.marginWidth = 0;
      layout.marginHeight = 0;
      layout.verticalSpacing = 0;
      layout.numColumns = 2;
      hintArea.setLayout(layout);
      hintArea.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, false));
      hintArea.setBackground(ThemeEngine.getBackgroundColor("MessageBar"));
      
      CLabel hintsTitle = new CLabel(hintArea, SWT.NONE);
      hintsTitle.setBackground(hintArea.getBackground());
      hintsTitle.setForeground(ThemeEngine.getForegroundColor("MessageBar"));
      hintsTitle.setImage(SharedIcons.IMG_INFORMATION);
      hintsTitle.setText(Messages.get().ScriptEditor_Hints);
      hintsTitle.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, false));
      hintsTitle.addMouseListener(new MouseAdapter() {
         @Override
         public void mouseDoubleClick(MouseEvent e)
         {
            if (e.button == 1)
               toggleHints();
         }
      });
      
      hintsExpandButton = new Label(hintArea, SWT.NONE);
      hintsExpandButton.setBackground(hintArea.getBackground());
      hintsExpandButton.setCursor(getDisplay().getSystemCursor(SWT.CURSOR_HAND));
      hintsExpandButton.setImage(SharedIcons.IMG_EXPAND);
      hintsExpandButton.setToolTipText(Messages.get().ScriptEditor_HideMessage);
      GridData gd = new GridData();
      gd.verticalAlignment = SWT.CENTER;
      hintsExpandButton.setLayoutData(gd);
      hintsExpandButton.addMouseListener(new MouseListener() {
         private boolean doAction = false;
         
         @Override
         public void mouseDoubleClick(MouseEvent e)
         {
            if (e.button == 1)
               doAction = false;
         }

         @Override
         public void mouseDown(MouseEvent e)
         {
            if (e.button == 1)
               doAction = true;
         }

         @Override
         public void mouseUp(MouseEvent e)
         {
            if ((e.button == 1) && doAction)
               toggleHints();
         }
      });
      
      Label separator = new Label(this, SWT.SEPARATOR | SWT.HORIZONTAL);
      separator.setLayoutData(new GridData(SWT.FILL, SWT.TOP, true, false));
   }
	
	/**
	 * Get underlying text widget
	 * @return text widget
	 */
	public Text getTextWidget()
	{
		return editor;
	}
	
	/**
	 * Set text for editing
	 * @param text
	 */
	public void setText(String text)
	{
		editor.setText((text != null) ? text : "");
	}
	
	/**
	 * Get editor's content
	 * @return
	 */
	public String getText()
	{
		return editor.getText();
	}

	/**
	 * @param functions the functions to set
	 */
	public void setFunctions(Set<String> functions)
	{
	}
	
	/**
	 * Add functions
	 * 
	 * @param fc
	 */
	public void addFunctions(Collection<String> fc)
	{
	}

	/**
	 * @param variables the variables to set
	 */
	public void setVariables(Set<String> variables)
	{
	}

	/**
	 * Add variables
	 * 
	 * @param vc
	 */
	public void addVariables(Collection<String> vc)
	{
	}

   /**
    * @param constants new constant set
    */
   public void setConstants(Set<String> constants)
   {
   }

   /**
    * Add constants
    * 
    * @param cc constants to add
    */
   public void addConstants(Collection<String> cc)
   {
   }

	/**
	 * @return the functionsCache
	 */
	public String[] getFunctions()
	{
		return new String[0];
	}

	/**
	 * @return the variablesCache
	 */
	public String[] getVariables()
	{
      return new String[0];
	}

   /**
    * @return constants cache
    */
   public String[] getConstants()
   {
      return new String[0];
   }

	/**
	 * Get icon for given autocompletion proposal type. Proposal types defined in NXSLProposalProcessor.
	 * 
	 * @param type proposal type
	 * @return image for given proposal type or null
	 */
	public Image getProposalIcon(int type)
	{
		return null;
	}

	/* (non-Javadoc)
	 * @see org.eclipse.swt.widgets.Composite#computeSize(int, int, boolean)
	 */
	@Override
	public Point computeSize(int wHint, int hHint, boolean changed)
	{
		Point p = editor.computeSize(wHint, hHint, changed);
		p.y += 4;
		return p;
	}
   
   /**
    * Toggle hints area
    */
   private void toggleHints()
   {
      if (hintTextControl != null)
      {
         hintTextControl.dispose();
         hintTextControl = null;
         hintsExpandButton.setImage(SharedIcons.IMG_EXPAND);
      }
      else
      {
         hintTextControl = new Text(hintArea, SWT.MULTI | SWT.WRAP);
         hintTextControl.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, false, 2, 1));
         hintTextControl.setEditable(false);
         hintTextControl.setText(hintText);
         hintTextControl.setBackground(ThemeEngine.getBackgroundColor("MessageBar"));
         hintTextControl.setForeground(ThemeEngine.getForegroundColor("MessageBar"));
         hintsExpandButton.setImage(SharedIcons.IMG_COLLAPSE);
      }
      layout(true, true);
	}
   
   /**
    * Compile script
    */
   private void compileScript()
   {
      final String source = getText();
      getTextWidget().setEditable(false);
      NXCSession session = ConsoleSharedData.getSession();
      new ConsoleJob("Compile script", null, Activator.PLUGIN_ID, null) {
         @Override
         protected String getErrorMessage()
         {
            return "Cannot compile script";
         }

         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {
            final ScriptCompilationResult result = session.compileScript(source, false);
            runInUIThread(new Runnable() {
               @Override
               public void run()
               {
                  if (result.success)
                  {
                     MessageDialogHelper.openInformation(getShell(), "Info", "Script compiled successfully");
                  }
                  else
                  {
                     MessageDialogHelper.openInformation(getShell(), "Warning", result.errorMessage);
                  }
                  getTextWidget().setEditable(true);
               }
            });
         }
      }.start();
   }
}
