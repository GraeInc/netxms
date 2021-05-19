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
package org.netxms.ui.eclipse.alarmviewer.views;

import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.action.Action;
import org.eclipse.jface.action.IMenuManager;
import org.eclipse.jface.action.IToolBarManager;
import org.eclipse.jface.dialogs.IDialogSettings;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.CLabel;
import org.eclipse.swt.custom.ScrolledComposite;
import org.eclipse.swt.events.ControlAdapter;
import org.eclipse.swt.events.ControlEvent;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.TableColumn;
import org.eclipse.swt.widgets.Text;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IViewSite;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.forms.events.ExpansionEvent;
import org.eclipse.ui.forms.events.HyperlinkAdapter;
import org.eclipse.ui.forms.events.HyperlinkEvent;
import org.eclipse.ui.forms.events.IExpansionListener;
import org.eclipse.ui.forms.widgets.Form;
import org.eclipse.ui.forms.widgets.FormToolkit;
import org.eclipse.ui.forms.widgets.ImageHyperlink;
import org.eclipse.ui.forms.widgets.Section;
import org.eclipse.ui.model.WorkbenchLabelProvider;
import org.eclipse.ui.part.ViewPart;
import org.netxms.client.NXCException;
import org.netxms.client.NXCSession;
import org.netxms.client.constants.DataOrigin;
import org.netxms.client.constants.DataType;
import org.netxms.client.constants.HistoricalDataType;
import org.netxms.client.constants.RCC;
import org.netxms.client.constants.Severity;
import org.netxms.client.datacollection.DciData;
import org.netxms.client.datacollection.DciValue;
import org.netxms.client.datacollection.GraphItem;
import org.netxms.client.datacollection.GraphItemStyle;
import org.netxms.client.datacollection.GraphSettings;
import org.netxms.client.datacollection.Threshold;
import org.netxms.client.events.Alarm;
import org.netxms.client.events.AlarmComment;
import org.netxms.client.events.EventInfo;
import org.netxms.client.objects.AbstractObject;
import org.netxms.ui.eclipse.actions.RefreshAction;
import org.netxms.ui.eclipse.alarmviewer.Activator;
import org.netxms.ui.eclipse.alarmviewer.Messages;
import org.netxms.ui.eclipse.alarmviewer.dialogs.EditCommentDialog;
import org.netxms.ui.eclipse.alarmviewer.views.helpers.EventTreeComparator;
import org.netxms.ui.eclipse.alarmviewer.views.helpers.EventTreeContentProvider;
import org.netxms.ui.eclipse.alarmviewer.views.helpers.EventTreeLabelProvider;
import org.netxms.ui.eclipse.alarmviewer.views.helpers.HistoricalDataLabelProvider;
import org.netxms.ui.eclipse.alarmviewer.widgets.AlarmCommentsEditor;
import org.netxms.ui.eclipse.charts.api.ChartFactory;
import org.netxms.ui.eclipse.charts.api.HistoricalDataChart;
import org.netxms.ui.eclipse.console.resources.SharedIcons;
import org.netxms.ui.eclipse.console.resources.StatusDisplayInfo;
import org.netxms.ui.eclipse.jobs.ConsoleJob;
import org.netxms.ui.eclipse.layout.DashboardLayout;
import org.netxms.ui.eclipse.layout.DashboardLayoutData;
import org.netxms.ui.eclipse.shared.ConsoleSharedData;
import org.netxms.ui.eclipse.tools.ColorConverter;
import org.netxms.ui.eclipse.tools.ImageCache;
import org.netxms.ui.eclipse.tools.MessageDialogHelper;
import org.netxms.ui.eclipse.tools.ViewRefreshController;
import org.netxms.ui.eclipse.tools.WidgetHelper;
import org.netxms.ui.eclipse.widgets.SortableTreeViewer;

/**
 * Alarm comments
 */
public class AlarmDetails extends ViewPart
{
	public static final String ID = "org.netxms.ui.eclipse.alarmviewer.views.AlarmDetails"; //$NON-NLS-1$
	
	public static final int EV_COLUMN_SEVERITY = 0;
	public static final int EV_COLUMN_SOURCE = 1;
	public static final int EV_COLUMN_NAME = 2;
	public static final int EV_COLUMN_MESSAGE = 3;
	public static final int EV_COLUMN_TIMESTAMP = 4;
	
   private static final String[] dciStatusImage = { "icons/active.gif", "icons/disabled.gif", "icons/unsupported.gif" }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
   
	private static final String[] stateImage = { "icons/outstanding.png", "icons/acknowledged.png", "icons/resolved.png", "icons/terminated.png", "icons/acknowledged_sticky.png" }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$ //$NON-NLS-5$
	private final String[] stateText = { Messages.get().AlarmListLabelProvider_AlarmState_Outstanding, Messages.get().AlarmListLabelProvider_AlarmState_Acknowledged, Messages.get().AlarmListLabelProvider_AlarmState_Resolved, Messages.get().AlarmListLabelProvider_AlarmState_Terminated };
	
	private NXCSession session;
	private long alarmId;
	private ImageCache imageCache;
	private WorkbenchLabelProvider wbLabelProvider;
	private FormToolkit toolkit;
	private Form form;
	private CLabel alarmSeverity;
	private CLabel alarmState;
	private CLabel alarmSource;
   private CLabel alarmDCI;
   private CLabel alarmKey;
   private CLabel alarmRule;
	private Text alarmText;
	private Composite editorsArea;
	private ImageHyperlink linkAddComment;
	private Map<Long, AlarmCommentsEditor> editors = new HashMap<Long, AlarmCommentsEditor>();
	private Composite dataArea;
	private SortableTreeViewer eventViewer;
	private Action actionRefresh;
	private CLabel labelAccessDenied = null;
	private boolean dataSectionCreated = false;
	private Section dataSection;
	private HistoricalDataChart chart;
	private TableViewer dataViewer;
	private Control dataViewControl;
	private long nodeId;
	private long dciId;
	private ViewRefreshController refreshController = null;
	private boolean updateInProgress = false;

   /**
    * @see org.eclipse.ui.part.ViewPart#init(org.eclipse.ui.IViewSite)
    */
	@Override
	public void init(IViewSite site) throws PartInitException
	{
		super.init(site);
      session = ConsoleSharedData.getSession();
		wbLabelProvider = new WorkbenchLabelProvider();

		try
		{
			alarmId = Long.parseLong(site.getSecondaryId());
		}
		catch(NumberFormatException e)
		{
			throw new PartInitException(Messages.get().AlarmComments_InternalError, e);
		}
		
		setPartName(getPartName() + " [" + Long.toString(alarmId) + "]"); //$NON-NLS-1$ //$NON-NLS-2$
	}

   /**
    * @see org.eclipse.ui.part.WorkbenchPart#createPartControl(org.eclipse.swt.widgets.Composite)
    */
	@Override
	public void createPartControl(Composite parent)
	{
		imageCache = new ImageCache();
		
		toolkit = new FormToolkit(parent.getDisplay());
		form = toolkit.createForm(parent);

		DashboardLayout layout = new DashboardLayout();
		layout.marginWidth = WidgetHelper.OUTER_SPACING;
      layout.marginHeight = WidgetHelper.OUTER_SPACING;
		form.getBody().setLayout(layout);
		
		createAlarmDetailsSection();
		createEventsSection();
		createCommentsSection();
		createDataSection();
		
		createActions();
		contributeToActionBars();

		refresh();
	}

	/**
	 * Create actions
	 */
	private void createActions()
	{
		actionRefresh = new RefreshAction(this) {
			@Override
			public void run()
			{
				refresh();
			}
		};
	}
	
	/**
	 * Contribute actions to action bar
	 */
	private void contributeToActionBars()
	{
		IActionBars bars = getViewSite().getActionBars();
		fillLocalPullDown(bars.getMenuManager());
		fillLocalToolBar(bars.getToolBarManager());
	}

	/**
	 * Fill local pull-down menu
	 * 
	 * @param manager
	 *           Menu manager for pull-down menu
	 */
	private void fillLocalPullDown(IMenuManager manager)
	{
		manager.add(actionRefresh);
	}

	/**
	 * Fill local tool bar
	 * 
	 * @param manager
	 *           Menu manager for local toolbar
	 */
	private void fillLocalToolBar(IToolBarManager manager)
	{
		manager.add(actionRefresh);
	}

	/**
	 * Create alarm details section
	 */
	private void createAlarmDetailsSection()
	{
		final Section section = toolkit.createSection(form.getBody(), Section.TITLE_BAR | Section.EXPANDED | Section.TWISTIE | Section.COMPACT);
		section.setText(Messages.get().AlarmDetails_Overview);
		DashboardLayoutData dd = new DashboardLayoutData();
		dd.fill = false;
		section.setLayoutData(dd);

		final Composite clientArea = toolkit.createComposite(section);
		GridLayout layout = new GridLayout();
		layout.numColumns = 3;
		clientArea.setLayout(layout);
		section.setClient(clientArea);

		alarmSeverity = new CLabel(clientArea, SWT.NONE);
		toolkit.adapt(alarmSeverity);
		GridData gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.verticalAlignment = SWT.TOP;
		alarmSeverity.setLayoutData(gd);

		Label sep = new Label(clientArea, SWT.VERTICAL | SWT.SEPARATOR);
		gd = new GridData();
		gd.verticalAlignment = SWT.FILL;
		gd.grabExcessVerticalSpace = true;
      gd.verticalSpan = 4;
		sep.setLayoutData(gd);

		final ScrolledComposite textContainer = new ScrolledComposite(clientArea, SWT.H_SCROLL | SWT.V_SCROLL) {
			@Override
			public Point computeSize(int wHint, int hHint, boolean changed)
			{
				Point size = super.computeSize(wHint, hHint, changed);
				if (size.y > 200)
					size.y = 200;
				return size;
			}
		};
		textContainer.setExpandHorizontal(true);
		textContainer.setExpandVertical(true);
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		gd.verticalAlignment = SWT.FILL;
      gd.verticalSpan = 4;
		textContainer.setLayoutData(gd);
		textContainer.addControlListener(new ControlAdapter() {
			@Override
			public void controlResized(ControlEvent e)
			{
				Point size = alarmText.computeSize(SWT.DEFAULT, SWT.DEFAULT);
				alarmText.setSize(size.x, size.y);
				textContainer.setMinWidth(size.x);
				textContainer.setMinHeight(size.y);
			}
		});

		int bs = toolkit.getBorderStyle();
		toolkit.setBorderStyle(SWT.NONE);
		alarmText = toolkit.createText(textContainer, "", SWT.MULTI); //$NON-NLS-1$
		toolkit.setBorderStyle(bs);
		alarmText.setEditable(false);
		textContainer.setContent(alarmText);

		alarmState = new CLabel(clientArea, SWT.NONE);
		toolkit.adapt(alarmState);
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.verticalAlignment = SWT.TOP;
		alarmState.setLayoutData(gd);

		alarmSource = new CLabel(clientArea, SWT.NONE);
		toolkit.adapt(alarmSource);
		gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.verticalAlignment = SWT.TOP;
		alarmSource.setLayoutData(gd);

      alarmKey = new CLabel(clientArea, SWT.NONE);
      toolkit.adapt(alarmKey);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.verticalAlignment = SWT.TOP;
      alarmKey.setLayoutData(gd);

      final Image keyImage = Activator.getImageDescriptor("icons/key.png").createImage();
      alarmKey.setImage(keyImage);
      alarmKey.addDisposeListener(new DisposeListener() {
         @Override
         public void widgetDisposed(DisposeEvent e)
         {
            keyImage.dispose();
         }
      });

      alarmRule = new CLabel(clientArea, SWT.NONE);
      toolkit.adapt(alarmRule);
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.verticalAlignment = SWT.BOTTOM;
      gd.horizontalSpan = 3;
      alarmRule.setLayoutData(gd);

      final Image eppImage = Activator.getImageDescriptor("icons/epp.png").createImage();
      alarmRule.setImage(eppImage);
      alarmRule.addDisposeListener(new DisposeListener() {
         @Override
         public void widgetDisposed(DisposeEvent e)
         {
            eppImage.dispose();
         }
      });
	}

	/**
	 * Create comment section
	 */
	private void createCommentsSection()
	{
		final Section section = toolkit.createSection(form.getBody(), Section.TITLE_BAR | Section.EXPANDED | Section.TWISTIE | Section.COMPACT);
		section.setText(Messages.get().AlarmComments_Comments);
      final DashboardLayoutData dd = new DashboardLayoutData();
      dd.fill = true;
      section.setLayoutData(dd);
      section.addExpansionListener(new IExpansionListener() {
         @Override
         public void expansionStateChanging(ExpansionEvent e)
         {
            dd.fill = e.getState();
         }
         
         @Override
         public void expansionStateChanged(ExpansionEvent e)
         {
         }
      });

		editorsArea = toolkit.createComposite(section);
		GridLayout layout = new GridLayout();
		editorsArea.setLayout(layout);
		section.setClient(editorsArea);
		
		linkAddComment = toolkit.createImageHyperlink(editorsArea, SWT.NONE);
		linkAddComment.setImage(imageCache.add(Activator.getImageDescriptor("icons/new_comment.png"))); //$NON-NLS-1$
		linkAddComment.setText(Messages.get().AlarmComments_AddCommentLink);
		linkAddComment.addHyperlinkListener(new HyperlinkAdapter() {
			@Override
			public void linkActivated(HyperlinkEvent e)
			{
				addComment();
			}
		});
	}

	/**
	 * Create events section
	 */
	private void createEventsSection()
	{
		final Section section = toolkit.createSection(form.getBody(), Section.TITLE_BAR | Section.EXPANDED | Section.TWISTIE | Section.COMPACT);
		section.setText(Messages.get().AlarmDetails_RelatedEvents);
		
		final DashboardLayoutData dd = new DashboardLayoutData();
		dd.fill = true;
		section.setLayoutData(dd);
		section.addExpansionListener(new IExpansionListener() {
			@Override
			public void expansionStateChanging(ExpansionEvent e)
			{
				dd.fill = e.getState();
			}
			
			@Override
			public void expansionStateChanged(ExpansionEvent e)
			{
			}
		});

      final Composite content = toolkit.createComposite(section);
      GridLayout layout = new GridLayout();
      layout.marginWidth = 0;
      layout.marginHeight = 0;
      content.setLayout(layout);
      section.setClient(content);

		final String[] names = { Messages.get().AlarmDetails_Column_Severity, Messages.get().AlarmDetails_Column_Source, Messages.get().AlarmDetails_Column_Name, Messages.get().AlarmDetails_Column_Message, Messages.get().AlarmDetails_Column_Timestamp };
		final int[] widths = { 130, 160, 160, 400, 150 };
		eventViewer = new SortableTreeViewer(content, names, widths, EV_COLUMN_TIMESTAMP, SWT.DOWN, SWT.BORDER | SWT.FULL_SELECTION);
		eventViewer.setContentProvider(new EventTreeContentProvider());
		eventViewer.setLabelProvider(new EventTreeLabelProvider());
		eventViewer.setComparator(new EventTreeComparator());
		eventViewer.getControl().setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, true));

		final IDialogSettings settings = Activator.getDefault().getDialogSettings();
		WidgetHelper.restoreTreeViewerSettings(eventViewer, settings, "AlarmDetails.Events"); //$NON-NLS-1$
		eventViewer.getControl().addDisposeListener(new DisposeListener() {
         @Override
         public void widgetDisposed(DisposeEvent e)
         {
            WidgetHelper.saveTreeViewerSettings(eventViewer, settings, "AlarmDetails.Events"); //$NON-NLS-1$
         }
      });
	}

	/**
	 * Create data section
	 */
	private void createDataSection()
	{
		dataSection = toolkit.createSection(form.getBody(), Section.TITLE_BAR | Section.EXPANDED | Section.TWISTIE | Section.COMPACT);
		dataSection.setText(Messages.get().AlarmDetails_LastValues);
      final DashboardLayoutData dd = new DashboardLayoutData();
      dd.fill = true;
      dataSection.setLayoutData(dd);
      dataSection.addExpansionListener(new IExpansionListener() {
			@Override
			public void expansionStateChanging(ExpansionEvent e)
			{
				dd.fill = e.getState();
			}
			
			@Override
			public void expansionStateChanged(ExpansionEvent e)
			{
			}
		});
		
		dataArea = toolkit.createComposite(dataSection);
		dataSection.setClient(dataArea);
		dataArea.setLayout(new FillLayout());

      final Composite clientArea = toolkit.createComposite(dataSection);
      alarmDCI = new CLabel(clientArea, SWT.NONE);
      toolkit.adapt(alarmDCI);
      GridData gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.verticalAlignment = SWT.TOP;
      alarmDCI.setLayoutData(gd);
	}

   /**
    * @see org.eclipse.ui.part.WorkbenchPart#setFocus()
    */
	@Override
	public void setFocus()
	{
		form.setFocus();
	}
	
	/**
	 * Refresh view
	 */
	private void refresh()
	{
		new ConsoleJob(Messages.get().AlarmDetails_RefreshJobTitle, this, Activator.PLUGIN_ID, null) {
			@Override
			protected void runInternal(IProgressMonitor monitor) throws Exception
			{
				final Alarm alarm = session.getAlarm(alarmId);
				final List<AlarmComment> comments = session.getAlarmComments(alarmId);
				
				final DciValue[] lastValues = session.getLastValues(alarm.getSourceObjectId());
				
            List<EventInfo> _events = null;
            try
            {
               _events = session.getAlarmEvents(alarmId);
            }
            catch(NXCException e)
            {
               if (e.getErrorCode() != RCC.ACCESS_DENIED)
                  throw e;
            }
				final List<EventInfo> events = _events;
				runInUIThread(new Runnable() {
					@Override
					public void run()
					{
						updateAlarmDetails(alarm);
						
						for(AlarmCommentsEditor e : editors.values())
							e.dispose();
						
						for(AlarmComment n : comments)
							editors.put(n.getId(), createEditor(n));
						
						if (events != null)
						{
   						eventViewer.setInput(events);
   						eventViewer.expandAll();
                     if (labelAccessDenied != null)
                     {
                        labelAccessDenied.dispose();
                        labelAccessDenied = null;
                     }
						}
						else if (labelAccessDenied == null)
                  {
                     labelAccessDenied = new CLabel(eventViewer.getControl().getParent(), SWT.NONE);
                     toolkit.adapt(labelAccessDenied);
                     labelAccessDenied.setImage(StatusDisplayInfo.getStatusImage(Severity.CRITICAL));
                     labelAccessDenied.setText(Messages.get().AlarmDetails_RelatedEvents_AccessDenied);
                     labelAccessDenied.moveAbove(null);
                     labelAccessDenied.setLayoutData(new GridData(SWT.FILL, SWT.TOP, true, false));
                  }
						
						if (!dataSectionCreated)
						{
   						if (alarm.getDciId() != 0)
   						{
   						   DciValue dci = null;
   						   for(DciValue d : lastValues)
   						   {
   						      if (d.getId() == alarm.getDciId())
   						      {
   						         dci = d;
   						         break;
   						      }
   						   }
   						   if (dci != null)
   						   {
                           Threshold t = dci.getActiveThreshold();
   						      alarmDCI.setText(dci.getDescription() + ((t != null) ? (" (" + t.getTextualRepresentation() + ")") : " (OK)"));
   						      alarmDCI.setImage(imageCache.add(Activator.getImageDescriptor(dciStatusImage[dci.getStatus()])));

   						      createDataAreaElements(alarm, dci);
      				         refreshData();
   						   }
   						   else
   						   {
   	                     Label label = new Label(dataArea, SWT.NONE);
   	                     label.setText(String.format("DCI with ID %d is not accessible", alarm.getDciId()));
                           alarmDCI.setText("[" + alarm.getDciId() + "]");
   	                     dataSection.setExpanded(false);
   	                     DashboardLayoutData dd = (DashboardLayoutData)dataSection.getLayoutData();
   	                     dd.fill = false;
   						   }
   						}
   						else
   						{
   						   Label label = new Label(dataArea, SWT.NONE);
   						   label.setText("No DCI associated with this alarm");
   						   dataSection.setExpanded(false);
   						   DashboardLayoutData dd = (DashboardLayoutData)dataSection.getLayoutData();
   						   dd.fill = false;
   						   alarmDCI.dispose();
   						}
   						dataSectionCreated = true;
						}						
						else if (dataViewControl != null)
						{
						   refreshData();
						}
						updateLayout();
					}
				});
			}

			@Override
			protected String getErrorMessage()
			{
				return Messages.get().AlarmDetails_RefreshJobError;
			}
		}.start();
	}
	
	/**
	 * Create elements of data area
	 * 
	 * @param alarm current alarm
	 * @param dci current DCI
	 */
	private void createDataAreaElements(Alarm alarm, DciValue dci)
	{
      nodeId = alarm.getSourceObjectId();
      dciId = alarm.getDciId();
      
      if (dci.getDataType() != DataType.STRING)
      {
         chart = ChartFactory.createLineChart(dataArea, SWT.BORDER);
         chart.setZoomEnabled(true);
         chart.setTitleVisible(true);
         chart.setChartTitle(dci.getDescription());
         chart.setLegendVisible(true);
         chart.setLegendPosition(GraphSettings.POSITION_BOTTOM);
         chart.setExtendedLegend(true);
         chart.setGridVisible(true);
         chart.setTranslucent(true);
         chart.addParameter(new GraphItem(nodeId, dciId, DataOrigin.INTERNAL, dci.getDataType(), dci.getName(), dci.getDescription(), "%s"));
         
         List<GraphItemStyle> itemStyles = chart.getItemStyles();
         itemStyles.get(0).setType(GraphItemStyle.AREA);
         itemStyles.get(0).setColor(ColorConverter.rgbToInt(new RGB(127, 154, 72)));
         chart.setItemStyles(itemStyles);
         
         dataViewControl = (Control)chart;
      }
      else
      {
         dataViewer = new TableViewer(dataArea, SWT.BORDER | SWT.FULL_SELECTION);
         dataViewer.getTable().setHeaderVisible(true);
         
         TableColumn tc = new TableColumn(dataViewer.getTable(), SWT.LEFT);
         tc.setText("Timestamp");
         tc = new TableColumn(dataViewer.getTable(), SWT.LEFT);
         tc.setText("Value");
         
         dataViewer.setContentProvider(new ArrayContentProvider());
         dataViewer.setLabelProvider(new HistoricalDataLabelProvider());
         
         dataViewControl = dataViewer.getControl();
      }
      
      refreshController = new ViewRefreshController(AlarmDetails.this, 30, new Runnable() {
         @Override
         public void run()
         {
            if (dataViewControl.isDisposed())
               return;
            
            refreshData();
         }
      });
	}
	
	/**
	 * Update layout after internal change
	 */
	private void updateLayout()
	{
		form.layout(true, true);
	}
	
	/**
	 * Create comment editor widget
	 * 
	 * @param note alarm note associated with this widget
	 * @return
	 */
	private AlarmCommentsEditor createEditor(final AlarmComment note)
	{
	   HyperlinkAdapter editAction = new HyperlinkAdapter()
      {
         @Override
         public void linkActivated(HyperlinkEvent e)
         {
            editComment(note.getId(), note.getText());
         }
      };
      HyperlinkAdapter deleteAction = new HyperlinkAdapter()
      {
         @Override
         public void linkActivated(HyperlinkEvent e)
         {
            deleteComment(note.getId());
         }
      };
      final AlarmCommentsEditor e = new AlarmCommentsEditor(editorsArea, toolkit, imageCache, note, editAction, deleteAction);
		toolkit.adapt(e);
		GridData gd = new GridData();
		gd.horizontalAlignment = SWT.FILL;
		gd.grabExcessHorizontalSpace = true;
		e.setLayoutData(gd);
		e.moveBelow(linkAddComment);
		return e;
	}
	
	/**
    * Add new comment
    */
   private void addComment()
   {
      editComment(0, "");//$NON-NLS-1$
   }
   
   /**
    * Edit comment
    */
   private void editComment(final long noteId, String noteText)
   {
      final EditCommentDialog dlg = new EditCommentDialog(getSite().getShell(), noteId , noteText);
      if (dlg.open() != Window.OK)
         return;
      
      new ConsoleJob(Messages.get().AlarmComments_AddCommentJob, this, Activator.PLUGIN_ID, null) {
         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {
            session.updateAlarmComment(alarmId, noteId, dlg.getText());
            runInUIThread(new Runnable() {
               @Override
               public void run()
               {
                  refresh();
               }
            });
         }
         
         @Override
         protected String getErrorMessage()
         {
            return Messages.get().AlarmComments_AddError;
         }
      }.start();
   }
	
   /**
    * Delete comment
    */
   private void deleteComment(final long noteId)
   {
      if (!MessageDialogHelper.openConfirm(getSite().getShell(), Messages.get().AlarmComments_Confirmation, Messages.get().AlarmComments_AckToDeleteComment))
         return;
      
      new ConsoleJob(Messages.get().AlarmComments_DeleteCommentJob, this, Activator.PLUGIN_ID, null) {
         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {
            session.deleteAlarmComment(alarmId, noteId);
            runInUIThread(new Runnable() {
               @Override
               public void run()
               {
                  refresh();
               }
            });
         }
         
         @Override
         protected String getErrorMessage()
         {
            return Messages.get().AlarmComments_ErrorDeleteAlarmComment;
         }
      }.start();
   }
   
	/**
	 * Update alarm details
	 * 
	 * @param alarm
	 */
	private void updateAlarmDetails(Alarm alarm)
	{
		alarmSeverity.setImage(StatusDisplayInfo.getStatusImage(alarm.getCurrentSeverity()));
		alarmSeverity.setText(StatusDisplayInfo.getStatusText(alarm.getCurrentSeverity()));

		int state = alarm.getState();
		if ((state == Alarm.STATE_ACKNOWLEDGED) && alarm.isSticky())
			state = Alarm.STATE_TERMINATED + 1;
		alarmState.setImage(imageCache.add(Activator.getImageDescriptor(stateImage[state])));
		alarmState.setText(stateText[alarm.getState()]);

		AbstractObject object = session.findObjectById(alarm.getSourceObjectId());
		alarmSource.setImage((object != null) ? wbLabelProvider.getImage(object) : SharedIcons.IMG_UNKNOWN_OBJECT);
		alarmSource.setText((object != null) ? object.getObjectName() : ("[" + Long.toString(alarm.getSourceObjectId()) + "]")); //$NON-NLS-1$ //$NON-NLS-2$

      alarmKey.setText(alarm.getKey());
		alarmText.setText(alarm.getMessage());
      alarmRule.setText(String.format("Created by rule \"%s\" (%s)", alarm.getRuleDescription(), alarm.getRuleId().toString()));
	}

   /**
    * @see org.eclipse.ui.part.WorkbenchPart#dispose()
    */
	@Override
	public void dispose()
	{
	   if (refreshController != null)
	      refreshController.dispose();
		imageCache.dispose();
		wbLabelProvider.dispose();
		super.dispose();
	}

   /**
    * Refresh graph's data
    */
   private void refreshData()
   {
      if (updateInProgress)
         return;
      
      updateInProgress = true;
      
      ConsoleJob job = new ConsoleJob("Update DCI data view", this, Activator.PLUGIN_ID, null) {
         @Override
         protected void runInternal(IProgressMonitor monitor) throws Exception
         {
            if (chart != null)
            {
               final Date from = new Date(System.currentTimeMillis() - 86400000);
               final Date to = new Date(System.currentTimeMillis());
               final DciData data = session.getCollectedData(nodeId, dciId, from, to, 0, HistoricalDataType.PROCESSED);
               runInUIThread(new Runnable() {
                  @Override
                  public void run()
                  {
                     if (!dataViewControl.isDisposed())
                     {
                        chart.setTimeRange(from, to);
                        chart.updateParameter(0, data, true);
                        chart.clearErrors();
                     }
                     updateInProgress = false;
                  }
               });
            }
            else
            {
               final DciData data = session.getCollectedData(nodeId, dciId, null, null, 20, HistoricalDataType.PROCESSED);
               runInUIThread(new Runnable() {
                  @Override
                  public void run()
                  {
                     if (!dataViewControl.isDisposed())
                     {
                        dataViewer.setInput(data.getValues());
                        for(TableColumn tc : dataViewer.getTable().getColumns())
                        {
                           tc.pack();
                           tc.setWidth(tc.getWidth() + 10); // compensate for pack issues on Linux
                        }
                     }
                     updateInProgress = false;
                  }
               });
            }
         }

         @Override
         protected String getErrorMessage()
         {
            return "Cannot read DCI data from server";
         }

         @Override
         protected void jobFailureHandler()
         {
            updateInProgress = false;
            super.jobFailureHandler();
         }

         @Override
         protected IStatus createFailureStatus(final Exception e)
         {
            runInUIThread(new Runnable() {
               @Override
               public void run()
               {
                  if (chart != null)
                     chart.addError(getErrorMessage() + " (" + e.getLocalizedMessage() + ")"); //$NON-NLS-1$ //$NON-NLS-2$
                  else
                     dataViewer.setInput(new Object[0]);
               }
            });
            return Status.OK_STATUS;
         }
      };
      job.setUser(false);
      job.start();
   }
}
