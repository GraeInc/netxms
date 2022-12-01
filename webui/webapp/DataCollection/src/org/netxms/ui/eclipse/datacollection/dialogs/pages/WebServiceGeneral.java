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
package org.netxms.ui.eclipse.datacollection.dialogs.pages;

import org.eclipse.jface.preference.PreferencePage;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Group;
import org.netxms.client.constants.WebServiceAuthType;
import org.netxms.client.constants.HttpRequestMethod;
import org.netxms.client.datacollection.WebServiceDefinition;
import org.netxms.ui.eclipse.tools.MessageDialogHelper;
import org.netxms.ui.eclipse.tools.WidgetHelper;
import org.netxms.ui.eclipse.widgets.LabeledSpinner;
import org.netxms.ui.eclipse.widgets.LabeledText;
import org.netxms.ui.eclipse.widgets.PasswordInputField;

/**
 * "General" property page for web service definition configuration
 */
public class WebServiceGeneral extends PreferencePage
{
   private WebServiceDefinition definition;
   private LabeledText name;
   private LabeledText url;
   private Button checkVerifyCert;
   private Button checkVerifyHost;
   private Button checkFollowLocation;
   private Button checkTextParsing;
   private Combo httpMethod;
   private LabeledText requestData;
   private Combo authType;
   private LabeledText login;
   private PasswordInputField password;
   private LabeledSpinner retentionTime;
   private LabeledSpinner timeout;
   private LabeledText description;

   /**
    * Constructor
    */
   public WebServiceGeneral(WebServiceDefinition definition)
   {
      super("General");
      noDefaultAndApplyButton();
      this.definition = definition;
   }

   /**
    * @see org.eclipse.jface.preference.PreferencePage#createContents(org.eclipse.swt.widgets.Composite)
    */
   @Override
   protected Control createContents(Composite parent)
   {
      Composite dialogArea = new Composite(parent, SWT.NONE);

      GridLayout layout = new GridLayout();
      layout.marginWidth = WidgetHelper.DIALOG_WIDTH_MARGIN;
      layout.marginHeight = WidgetHelper.DIALOG_HEIGHT_MARGIN;
      layout.verticalSpacing = WidgetHelper.OUTER_SPACING;
      layout.horizontalSpacing = WidgetHelper.OUTER_SPACING;
      layout.numColumns = 2;
      layout.makeColumnsEqualWidth = true;
      dialogArea.setLayout(layout);

      name = new LabeledText(dialogArea, SWT.NONE);
      name.setLabel("Name");
      name.setText(definition.getName());
      GridData gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalSpan = 2;
      name.setLayoutData(gd);

      url = new LabeledText(dialogArea, SWT.NONE);
      url.setLabel("URL");
      url.setText(definition.getUrl());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalSpan = 2;
      url.setLayoutData(gd);
      
      /* method group */
      Group groupMethod = new Group(dialogArea, SWT.NONE);
      groupMethod.setText("HTTP request method");
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.verticalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.grabExcessVerticalSpace = true;
      gd.horizontalSpan = 2;
      groupMethod.setLayoutData(gd);
      layout = new GridLayout();
      groupMethod.setLayout(layout);

      httpMethod = new Combo(groupMethod, SWT.DROP_DOWN | SWT.READ_ONLY);
      for(HttpRequestMethod t : HttpRequestMethod.values())
         httpMethod.add(t.toString());
      httpMethod.select(definition.getHttpRequestMethod().getValue());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      httpMethod.setLayoutData(gd);

      requestData = new LabeledText(groupMethod, SWT.NONE, SWT.BORDER | SWT.MULTI | SWT.H_SCROLL | SWT.V_SCROLL);
      requestData.setLabel("Request data");
      requestData.setText(definition.getRequestData());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.heightHint = 120;
      requestData.setLayoutData(gd);

      /* authentication group */
      Group groupAuth = new Group(dialogArea, SWT.NONE);
      groupAuth.setText("Authentication");
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.verticalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.grabExcessVerticalSpace = true;
      groupAuth.setLayoutData(gd);
      layout = new GridLayout();
      groupAuth.setLayout(layout);

      authType = new Combo(groupAuth, SWT.DROP_DOWN | SWT.READ_ONLY);
      for(WebServiceAuthType t : WebServiceAuthType.values())
         authType.add(t.toString());
      authType.select(definition.getAuthenticationType().getValue());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      authType.setLayoutData(gd);

      login = new LabeledText(groupAuth, SWT.NONE);
      login.setLabel("Login");
      login.setText(definition.getLogin());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      login.setLayoutData(gd);

      password = new PasswordInputField(groupAuth, SWT.NONE);
      password.setLabel("Password");
      password.setText(definition.getPassword());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      password.setLayoutData(gd);

      /* options group */
      Group groupOptions = new Group(dialogArea, SWT.NONE);
      groupOptions.setText("Options");
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.verticalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.grabExcessVerticalSpace = true;
      groupOptions.setLayoutData(gd);
      layout = new GridLayout();
      groupOptions.setLayout(layout);

      retentionTime = new LabeledSpinner(groupOptions, SWT.NONE);
      retentionTime.setLabel("Cache retention time");
      retentionTime.setRange(0, 3600);
      retentionTime.setSelection(definition.getCacheRetentionTime());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      retentionTime.setLayoutData(gd);

      timeout = new LabeledSpinner(groupOptions, SWT.NONE);
      timeout.setLabel("Request timeout");
      timeout.setRange(0, 300);
      timeout.setSelection(definition.getRequestTimeout());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      timeout.setLayoutData(gd);

      checkVerifyCert = new Button(groupOptions, SWT.CHECK);
      checkVerifyCert.setText("Verify &peer's certificate");
      checkVerifyCert.setSelection(definition.isVerifyCertificate());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      checkVerifyCert.setLayoutData(gd);

      checkVerifyHost = new Button(groupOptions, SWT.CHECK);
      checkVerifyHost.setText("Verify &host name in certificate");
      checkVerifyHost.setSelection(definition.isVerifyHost());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      checkVerifyHost.setLayoutData(gd);

      checkFollowLocation = new Button(groupOptions, SWT.CHECK);
      checkFollowLocation.setText("&Follow location header in a 3xx response");
      checkFollowLocation.setSelection(definition.isFollowLocation());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      checkFollowLocation.setLayoutData(gd);

      checkTextParsing = new Button(groupOptions, SWT.CHECK);
      checkTextParsing.setText("Process response as plain &text");
      checkTextParsing.setSelection(definition.isTextParsingUsed());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      checkTextParsing.setLayoutData(gd);

      description = new LabeledText(dialogArea, SWT.NONE, SWT.MULTI | SWT.BORDER | SWT.H_SCROLL | SWT.V_SCROLL);
      description.setLabel("Description");
      description.setText(definition.getDescription());
      gd = new GridData();
      gd.horizontalAlignment = SWT.FILL;
      gd.grabExcessHorizontalSpace = true;
      gd.horizontalSpan = 2;
      gd.heightHint = 120;
      description.setLayoutData(gd);

      return dialogArea;
   }

   /**
    * @see org.eclipse.jface.preference.PreferencePage#performOk()
    */
   @Override
   public boolean performOk()
   {
      if (!isControlCreated())
         return super.performOk();

      String svcName = name.getText().trim();
      if (svcName.isEmpty())
      {
         MessageDialogHelper.openWarning(getShell(), "Warning", "Web service name cannot be empty!");
         return false;
      }

      if (svcName.contains(":") || svcName.contains("/") || svcName.contains(",") || svcName.contains("(") || svcName.contains(")") || svcName.contains("{") || svcName.contains("}") ||
            svcName.contains("'") || svcName.contains("\""))
      {
         MessageDialogHelper.openWarning(getShell(), "Warning", "Web service name should not contain any of the following characters: / , : ' \" ( ) { }");
         return false;
      }

      definition.setName(svcName);
      definition.setUrl(url.getText().trim());
      definition.setHttpRequestMethod(HttpRequestMethod.getByValue(httpMethod.getSelectionIndex()));
      definition.setRequestData(requestData.getText().trim());
      definition.setVerifyCertificate(checkVerifyCert.getSelection());
      definition.setVerifyHost(checkVerifyHost.getSelection());
      definition.setFollowLocation(checkFollowLocation.getSelection());
      definition.setParseAsText(checkTextParsing.getSelection());
      definition.setAuthenticationType(WebServiceAuthType.getByValue(authType.getSelectionIndex()));
      definition.setLogin(login.getText().trim());
      definition.setPassword(password.getText());
      definition.setCacheRetentionTime(retentionTime.getSelection());
      definition.setRequestTimeout(timeout.getSelection());
      definition.setDescription(description.getText());
      return super.performOk();
   }
}
