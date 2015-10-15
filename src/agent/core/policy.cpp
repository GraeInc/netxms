/* 
** NetXMS multiplatform core agent
** Copyright (C) 2003-2013 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be usefu,,
** but ITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: policy.cpp
**
**/

#include "nxagentd.h"

#ifdef _WIN32
#define write _write
#define close _close
#endif

#define POLICY_REGISTRY_PATH _T("/policyRegistry")

/**
 * Register policy in persistent storage
 */
static void RegisterPolicy(CommSession *session, UINT32 type, const uuid& guid)
{
	TCHAR path[256], buffer[64];
	int tail;

   _sntprintf(path, 256, POLICY_REGISTRY_PATH _T("/policy-%s/"), guid.toString(buffer));
	tail = (int)_tcslen(path);

	Config *registry = AgentOpenRegistry();

	_tcscpy(&path[tail], _T("type"));
	registry->setValue(path, type);

	_tcscpy(&path[tail], _T("server"));
   registry->setValue(path, session->getServerAddress().toString(buffer));

	AgentCloseRegistry(true);
}

/**
 * Register policy in persistent storage
 */
static void UnregisterPolicy(const uuid& guid)
{
	TCHAR path[256], buffer[64];

   _sntprintf(path, 256, POLICY_REGISTRY_PATH _T("/policy-%s"), guid.toString(buffer));
	Config *registry = AgentOpenRegistry();
	registry->deleteEntry(path);
	AgentCloseRegistry(true);
}

/**
 * Get policy type by GUID
 */
static int GetPolicyType(const uuid& guid)
{
	TCHAR path[256], buffer[64];
	int type;

   _sntprintf(path, 256, POLICY_REGISTRY_PATH _T("/policy-%s/type"), guid.toString(buffer));
	Config *registry = AgentOpenRegistry();
	type = registry->getValueAsInt(path, -1);
	AgentCloseRegistry(false);
	return type;
}

/**
 * Deploy configuration file
 */
static UINT32 DeployConfig(UINT32 session, const uuid& guid, NXCPMessage *msg)
{
	TCHAR path[MAX_PATH], name[64], tail;
	int fh;
	UINT32 rcc;

	tail = g_szConfigIncludeDir[_tcslen(g_szConfigIncludeDir) - 1];
	_sntprintf(path, MAX_PATH, _T("%s%s%s.conf"), g_szConfigIncludeDir,
	           ((tail != '\\') && (tail != '/')) ? FS_PATH_SEPARATOR : _T(""),
              guid.toString(name));

	fh = _topen(path, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, S_IRUSR | S_IWUSR);
	if (fh != -1)
	{
		UINT32 size = msg->getFieldAsBinary(VID_CONFIG_FILE_DATA, NULL, 0);
		BYTE *data = (BYTE *)malloc(size);
		if (data != NULL)
		{
			msg->getFieldAsBinary(VID_CONFIG_FILE_DATA, data, size);
			if (write(fh, data, size) == size)
			{
		      DebugPrintf(session, 3, _T("Configuration file %s saved successfully"), path);
				rcc = ERR_SUCCESS;
			}
			else
			{
				rcc = ERR_IO_FAILURE;
			}
			free(data);
		}
		else
		{
			rcc = ERR_MEM_ALLOC_FAILED;
		}
		close(fh);
	}
	else
	{
		DebugPrintf(session, 2, _T("DeployConfig(): Error opening file %s for writing (%s)"), path, _tcserror(errno));
		rcc = ERR_FILE_OPEN_ERROR;
	}

	return rcc;
}

/**
 * Deploy log parser policy
 */
static UINT32 DeployLogParser(UINT32 session, const uuid& guid, NXCPMessage *msg)
{
	return ERR_NOT_IMPLEMENTED;
}

/**
 * Deploy policy on agent
 */
UINT32 DeployPolicy(CommSession *session, NXCPMessage *request)
{
	UINT32 type, rcc;

	type = request->getFieldAsUInt16(VID_POLICY_TYPE);
	uuid guid = request->getFieldAsGUID(VID_GUID);

	switch(type)
	{
		case AGENT_POLICY_CONFIG:
			rcc = DeployConfig(session->getIndex(), guid, request);
			break;
		case AGENT_POLICY_LOG_PARSER:
			rcc = DeployLogParser(session->getIndex(), guid, request);
			break;
		default:
			rcc = ERR_BAD_ARGUMENTS;
			break;
	}

	if (rcc == RCC_SUCCESS)
		RegisterPolicy(session, type, guid);

	DebugPrintf(session->getIndex(), 3, _T("Policy deployment: TYPE=%d RCC=%d"), type, rcc);
	return rcc;
}

/**
 * Remove configuration file
 */
static UINT32 RemoveConfig(UINT32 session, const uuid& guid, NXCPMessage *msg)
{
	TCHAR path[MAX_PATH], name[64], tail;
	UINT32 rcc;

	tail = g_szConfigIncludeDir[_tcslen(g_szConfigIncludeDir) - 1];
	_sntprintf(path, MAX_PATH, _T("%s%s%s.conf"), g_szConfigIncludeDir,
	           ((tail != '\\') && (tail != '/')) ? FS_PATH_SEPARATOR : _T(""),
              guid.toString(name));

	if (_tremove(path) != 0)
	{
		rcc = (errno == ENOENT) ? ERR_SUCCESS : ERR_IO_FAILURE;
	}
	else
	{
		rcc = ERR_SUCCESS;
	}
	return rcc;
}

/**
 * Remove log parser file
 */
static UINT32 RemoveLogParser(UINT32 session, const uuid& guid,  NXCPMessage *msg)
{
	TCHAR path[MAX_PATH], name[64], tail;
	UINT32 rcc;

	tail = g_szConfigIncludeDir[_tcslen(g_szConfigIncludeDir) - 1];
	_sntprintf(path, MAX_PATH, _T("%s%s%s.conf"), g_szConfigIncludeDir,
	           ((tail != '\\') && (tail != '/')) ? FS_PATH_SEPARATOR : _T(""),
              guid.toString(name));

	if (_tremove(path) != 0)
	{
		rcc = (errno == ENOENT) ? ERR_SUCCESS : ERR_IO_FAILURE;
	}
	else
	{
		rcc = ERR_SUCCESS;
	}
	return rcc;
}

/**
 * Uninstall policy from agent
 */
UINT32 UninstallPolicy(CommSession *session, NXCPMessage *request)
{
	UINT32 rcc;
	int type;
	TCHAR buffer[64];

	uuid guid = request->getFieldAsGUID(VID_GUID);
	type = GetPolicyType(guid);

	switch(type)
	{
		case AGENT_POLICY_CONFIG:
			rcc = RemoveConfig(session->getIndex(), guid, request);
			break;
		case AGENT_POLICY_LOG_PARSER:
			rcc = RemoveLogParser(session->getIndex(), guid, request);
			break;
		default:
			rcc = ERR_BAD_ARGUMENTS;
			break;
	}

	if (rcc == RCC_SUCCESS)
		UnregisterPolicy(guid);

   DebugPrintf(session->getIndex(), 3, _T("Policy uninstall: GUID=%s TYPE=%d RCC=%d"), guid.toString(buffer), type, rcc);
	return rcc;
}

/**
 * Get policy inventory
 */
UINT32 GetPolicyInventory(CommSession *session, NXCPMessage *msg)
{
	Config *registry = AgentOpenRegistry();

	ObjectArray<ConfigEntry> *list = registry->getSubEntries(_T("/policyRegistry"), NULL);
	if (list != NULL)
	{
		msg->setField(VID_NUM_ELEMENTS, (UINT32)list->size());
		UINT32 varId = VID_ELEMENT_LIST_BASE;
		for(int i = 0; i < list->size(); i++, varId += 7)
		{
			ConfigEntry *e = list->get(i);
			uuid_t guid;

			if (MatchString(_T("policy-*"), e->getName(), TRUE))
			{
				_uuid_parse(&(e->getName()[7]), guid);
				msg->setField(varId++, guid, UUID_LENGTH);
				msg->setField(varId++, (WORD)e->getSubEntryValueAsInt(_T("type")));
				msg->setField(varId++, e->getSubEntryValue(_T("server")));
			}
		}
		delete list;
	}
	else
	{
		msg->setField(VID_NUM_ELEMENTS, (UINT32)0);
	}
	
	AgentCloseRegistry(false);
	return RCC_SUCCESS;
}
