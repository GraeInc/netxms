/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2014 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: netxms-version.h
**
**/

#ifndef _netxms_version_h_
#define _netxms_version_h_

#include "build.h"

/**
 * Version constants 
 */
#define NETXMS_VERSION_MAJOR        1
#define NETXMS_VERSION_MINOR        2
#define NETXMS_VERSION_RELEASE      14
#define NETXMS_VERSION_STRING       _T("1.2.14")
#define NETXMS_VERSION_STRING_A     "1.2.14"

#ifdef UNICODE
#define IS_UNICODE_BUILD_STRING     _T(" (UNICODE)")
#else
#define IS_UNICODE_BUILD_STRING     _T(" (NON-UNICODE)")
#endif

/**
 * Current client-server protocol version
 */
#define CLIENT_PROTOCOL_VERSION           42

/**
 * Current mobile device protocol version
 */
#define MOBILE_DEVICE_PROTOCOL_VERSION    1

#endif
