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
package org.netxms.tests.mobileagent;

import org.netxms.mobile.agent.Session;
import org.netxms.tests.TestConstants;
import junit.framework.TestCase;

/**
 * Base class for NetXMS client library testing.
 * 
 * Please note that all tests expects that NetXMS server is running 
 * on local machine, with user admin and no password.
 * Change appropriate constants if needed.
 */
public abstract class SessionTest extends TestCase
{
	protected Session connect(boolean useEncryption) throws Exception
	{
      Session session = new Session(TestConstants.serverAddress, TestConstants.serverPortMobileAgent, TestConstants.MOBILE_DEVICE_ID, TestConstants.loginName, TestConstants.password, useEncryption);
		session.connect();
		return session;
	}

	protected Session connect() throws Exception
	{
		return connect(false);
	}
}
