/**
 * NetXMS - open source network management system
 * Copyright (C) 2020-2022 Raden Soultions
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
package org.netxms.nxmc.modules.objecttools.widgets;

import java.util.List;
import java.util.Map;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.netxms.client.objecttools.ObjectTool;
import org.netxms.nxmc.localization.LocalizationHelper;
import org.netxms.nxmc.modules.objects.ObjectContext;
import org.xnap.commons.i18n.I18n;

/**
 * Action executor widget to run an action and display it's result 
 */
public class ActionExecutor extends AbstractObjectToolExecutor
{
   private static I18n i18n = LocalizationHelper.getI18n(ActionExecutor.class);

   private String executionString;
   private long alarmId;
   private Map<String, String> inputValues;
   private List<String> maskedFields;
   protected long nodeId;

   /**
    * Constructor for action execution
    * 
    * @param parent parent control
    * @param viewPart parent view
    * @param ctx execution context
    * @param actionSet action set
    * @param tool object tool to execute
    * @param inputValues input values provided by user
    * @param maskedFields list of the fields that should be masked
    */
   public ActionExecutor(Composite parent, ObjectContext ctx, ActionSet actionSet, ObjectTool tool,
         Map<String, String> inputValues, List<String> maskedFields)
   {
      super(parent, ctx, actionSet);
      alarmId = ctx.getAlarmId();
      nodeId = ctx.object.getObjectId();
      this.executionString = tool.getData();
      this.inputValues = inputValues;
      this.maskedFields = maskedFields;
   }

   /**
    * @see org.netxms.nxmc.modules.objecttools.widgets.AbstractObjectToolExecutor#executeInternal(org.eclipse.swt.widgets.Display)
    */
   @Override
   protected void executeInternal(Display display) throws Exception
   {
      session.executeActionWithExpansion(nodeId, alarmId, executionString, true, inputValues, maskedFields, getOutputListener(), null);
      out.write(i18n.tr("\n\n*** TERMINATED ***\n\n\n"));
   }
}
