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
package org.netxms.nxmc.modules.worldmap.widgets;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseTrackListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.ToolTip;
import org.netxms.base.GeoLocation;
import org.netxms.client.NXCSession;
import org.netxms.client.TimePeriod;
import org.netxms.client.constants.TimeFrameType;
import org.netxms.client.constants.TimeUnit;
import org.netxms.client.objects.AbstractObject;
import org.netxms.nxmc.Registry;
import org.netxms.nxmc.base.jobs.Job;
import org.netxms.nxmc.base.views.View;
import org.netxms.nxmc.localization.DateFormatFactory;
import org.netxms.nxmc.localization.LocalizationHelper;
import org.netxms.nxmc.modules.worldmap.GeoLocationCache;
import org.netxms.nxmc.modules.worldmap.tools.Area;
import org.netxms.nxmc.modules.worldmap.tools.QuadTree;
import org.netxms.nxmc.resources.ResourceManager;
import org.netxms.nxmc.resources.SharedIcons;
import org.netxms.nxmc.tools.WidgetHelper;
import org.xnap.commons.i18n.I18n;

/**
 * Geolocation viewer for object location history
 */
public class GeoLocationHistoryViewer extends AbstractGeoMapViewer
{
   private static final I18n i18n = LocalizationHelper.getI18n(GeoLocationHistoryViewer.class);

   private static final int START = 1;
   private static final int END = 2;

   private static final Color INNER_BORDER_COLOR = new Color(Display.getCurrent(), 255, 255, 255);
   private static final Color TRACK_COLOR = new Color(Display.getCurrent(), 163, 73, 164);

   private AbstractObject historyObject = null;
   private List<GeoLocation> points = new ArrayList<GeoLocation>();
   private TimePeriod timePeriod = new TimePeriod();
   private ToolTip pointToolTip = null;
   private QuadTree<GeoLocation> locationTree = new QuadTree<GeoLocation>();
   private int selectedPoint = -1;
   private Image imageStart;
   private Image imageFinish;

   /**
    * Create geolocation history viewer.
    *
    * @param parent parent control
    * @param style control style
    * @param view owning view
    * @param object object to show location history for
    */
   public GeoLocationHistoryViewer(Composite parent, int style, View view, AbstractObject object)
   {
      super(parent, style, view);
      this.historyObject = object;
      pointToolTip = new ToolTip(getShell(), SWT.BALLOON);

      WidgetHelper.attachMouseTrackListener(this, new MouseTrackListener() {
         @Override
         public void mouseHover(MouseEvent e)
         {
            selectedPoint = -1;
            pointToolTip.setVisible(false);
            List<GeoLocation> suitablePoints = getAdjacentLocations(e.x, e.y);
            if (suitablePoints.isEmpty())
               return;

            selectedPoint = points.indexOf(suitablePoints.get(0));
            redraw();
         }

         @Override
         public void mouseEnter(MouseEvent e)
         {
         }

         @Override
         public void mouseExit(MouseEvent e)
         {
            selectedPoint = -1;
            pointToolTip.setVisible(false);
            redraw();
         }
      });

      imageStart = ResourceManager.getImage("icons/start.png");
      imageFinish = ResourceManager.getImage("icons/finish.png");
      addDisposeListener(new DisposeListener() {
         @Override
         public void widgetDisposed(DisposeEvent e)
         {
            imageStart.dispose();
            imageFinish.dispose();
         }
      });
   }

   /**
    * @see org.netxms.nxmc.modules.worldmap.widgets.AbstractGeoMapViewer#onMapLoad()
    */
   @Override
   protected void onMapLoad()
   {
      updateHistory();
   }

   /**
    * @see org.netxms.nxmc.modules.worldmap.widgets.AbstractGeoMapViewer#onCacheChange(org.netxms.client.objects.AbstractObject, org.netxms.base.GeoLocation)
    */
   @Override
   protected void onCacheChange(AbstractObject object, GeoLocation prevLocation)
   {
      if (object.getObjectId() == historyObject.getObjectId())
         updateHistory();
   }

   /**
    * @see org.netxms.ui.eclipse.osm.widgets.AbstractGeoMapViewer#mouseMove(org.eclipse.swt.events.MouseEvent)
    */
   @Override
   public void mouseMove(MouseEvent e)
   {
      super.mouseMove(e);
      if (selectedPoint != -1)
      {
         selectedPoint = -1;
         pointToolTip.setVisible(false);
         redraw();
      }
   }

   /**
    * Get geolocations adjacent to given screen coordinates ordered by distance from that point
    * 
    * @param p
    * @return
    */
   public List<GeoLocation> getAdjacentLocations(Point p)
   {
      return getAdjacentLocations(p.x, p.y);
   }
   
   /**
    * Get geolocations adjacent to given screen coordinates ordered by distance from that point
    * 
    * @param x
    * @param y
    * @return
    */
   public List<GeoLocation> getAdjacentLocations(int x, int y)
   {
      Point p = new Point(x, y);
      final GeoLocation center = getLocationAtPoint(p);

      p.x -= 5;
      p.y -= 5;
      GeoLocation topLeft = getLocationAtPoint(p);
      p.x += 10;
      p.y += 10;
      GeoLocation bottomRight = getLocationAtPoint(p);
      Area area = new Area(topLeft.getLatitude(), topLeft.getLongitude(), bottomRight.getLatitude(), bottomRight.getLongitude());

      List<GeoLocation> locations = locationTree.query(area);
      Collections.sort(locations, new Comparator<GeoLocation>() {
         @Override
         public int compare(GeoLocation l1, GeoLocation l2)
         {
            double d1 = Math.pow(Math.pow(l1.getLatitude() - center.getLatitude(), 2) + Math.pow(l1.getLongitude() - center.getLongitude(), 2), 0.5);  
            double d2 = Math.pow(Math.pow(l2.getLatitude() - center.getLatitude(), 2) + Math.pow(l2.getLongitude() - center.getLongitude(), 2), 0.5);  
            return (int)Math.signum(d1 - d2);
         }
      });

      return locations;
   }

   /**
    * Updates points for historical view
    */
   private void updateHistory()
   {
      final NXCSession session = Registry.getSession();
      Job job = new Job(String.format(i18n.tr("Reading location history for %s"), historyObject.getObjectName()), view) {
         @Override
         protected void run(IProgressMonitor monitor) throws Exception
         {
            final List<GeoLocation> pl = session.getLocationHistory(historyObject.getObjectId(), timePeriod.getPeriodStart(), timePeriod.getPeriodEnd());
            runInUIThread(new Runnable() {
               @Override
               public void run()
               {
                  points = pl;
                  locationTree.removeAll();
                  for(int i = 0; i < points.size(); i++)
                     locationTree.insert(points.get(i).getLatitude(), points.get(i).getLongitude(), points.get(i));
                  redraw();
               }
            });
         }

         @Override
         protected String getErrorMessage()
         {
            return String.format(i18n.tr("Cannot read location history for %s"), historyObject.getObjectName());
         }
      };
      job.setUser(false);
      job.start();
   }

   /**
    * Sets new time period
    */
   public void setTimePeriod(TimePeriod timePeriod)
   {
      this.timePeriod = timePeriod;
      updateHistory();      
   }

   /**
    * Gets time period
    */
   public TimePeriod getTimePeriod()
   {
      return timePeriod;
   }

   /**
    * @param value
    * @param unit
    */
   public void changeTimePeriod(int value, TimeUnit unit)
   {
      timePeriod.setTimeFrameType(TimeFrameType.BACK_FROM_NOW);
      timePeriod.setTimeRange(value);
      timePeriod.setTimeUnit(unit);
      updateHistory();
   }

   /**
    * @see org.netxms.nxmc.modules.worldmap.widgets.AbstractGeoMapViewer#drawContent(org.eclipse.swt.graphics.GC,
    *      org.netxms.base.GeoLocation, int, int, int)
    */
   @Override
   protected void drawContent(GC gc, GeoLocation currentLocation, int imgW, int imgH, int verticalOffset)
   {
      final Point centerXY = GeoLocationCache.coordinateToDisplay(currentLocation, accessor.getZoom());
      int nextX = 0;
      int nextY = 0;
      for(int i = 0; i < points.size(); i++)
      {
         final Point virtualXY = GeoLocationCache.coordinateToDisplay(points.get(i), accessor.getZoom());
         final int dx = virtualXY.x - centerXY.x;
         final int dy = virtualXY.y - centerXY.y + verticalOffset;

         if (i != points.size() - 1)
         { 
            final Point virtualXY2 = GeoLocationCache.coordinateToDisplay(points.get(i + 1), accessor.getZoom());
            nextX = imgW / 2 + (virtualXY2.x - centerXY.x);
            nextY = imgH / 2 + (virtualXY2.y - centerXY.y) + verticalOffset;
         }

         int color = SWT.COLOR_RED;
         if (i == selectedPoint)
         {
            color = SWT.COLOR_GREEN;
            DateFormat df = DateFormatFactory.getDateTimeFormat();
            pointToolTip.setText(String.format("%s\r\n%s - %s", points.get(i), df.format(points.get(i).getTimestamp()), df.format(points.get(i).getEndTimestamp())));
            pointToolTip.setVisible(true);
         }

         if (i == 0)
         {
            if (i == points.size() - 1)
            {
               nextX = imgW / 2 + dx;
               nextY = imgH / 2 + dy;
            }
            drawPoint(gc, imgW / 2 + dx, imgH / 2 + dy, START, nextX, nextY, color);                  
            continue;
         } 

         if (i == points.size() - 1)
         {    
            drawPoint(gc, imgW / 2 + dx, imgH / 2 + dy, END, nextX, nextY, color);
            continue;
         }

         drawPoint(gc, imgW / 2 + dx, imgH / 2 + dy, 0, nextX, nextY, color);
      }
   }

   /**
    * Draw point
    * 
    * @param gc
    * @param x
    * @param y
    * @param object
    */
   private void drawPoint(GC gc, int x, int y, int flag, int prevX, int prevY, int color) 
   {    
      if (flag == START || flag == END)
      {
         if (flag == START)
         {
            gc.setForeground(TRACK_COLOR);
            gc.setLineWidth(3);
            gc.drawLine(x, y, prevX, prevY);
         }

         gc.setBackground(getDisplay().getSystemColor(color)); 
         gc.fillOval(x - 5, y -5, 10, 10);

         Image image = (flag == START) ? imageStart : imageFinish;
         if (image == null)
            image = SharedIcons.IMG_EMPTY;

         int w = image.getImageData().width + LABEL_X_MARGIN * 2;
         int h = image.getImageData().height + LABEL_Y_MARGIN * 2;
         Rectangle rect = new Rectangle(x - w / 2 - 1, y - LABEL_ARROW_HEIGHT - h, w, h);

         gc.setBackground(TRACK_COLOR);
         gc.fillArc(rect.x, rect.y, rect.width, rect.height, 0, 360);
         gc.setLineWidth(2);
         gc.setForeground(INNER_BORDER_COLOR);
         gc.drawArc(rect.x + 4, rect.y + 4, rect.width - 8, rect.height - 8, 0, 360);
         gc.setLineWidth(1);

         final int[] arrow = new int[] { rect.x + rect.width / 2 - 3, rect.y + rect.height - 1, x, y, rect.x + rect.width / 2 + 4, rect.y + rect.height - 1 };
         gc.fillPolygon(arrow);

         gc.drawImage(image, rect.x + LABEL_X_MARGIN, rect.y + LABEL_Y_MARGIN);
      }
      else 
      {
         gc.setForeground(TRACK_COLOR);
         gc.setLineWidth(3);
         gc.drawLine(x, y, prevX, prevY);
         gc.setBackground(Display.getCurrent().getSystemColor(color)); 
         gc.fillOval(x - 5, y -5, 10, 10);
      }      
   }  

   /**
    * @see org.netxms.ui.eclipse.osm.widgets.AbstractGeoMapViewer#getObjectAtPoint(org.eclipse.swt.graphics.Point)
    */
   @Override
   public AbstractObject getObjectAtPoint(Point p)
   {
      return null;
   }
}
