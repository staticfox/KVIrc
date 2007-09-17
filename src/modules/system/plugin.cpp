//=============================================================================
//
//   File : plugin.cpp
//   Creation date : Wed Apr 11 04 2007 00:54:00 GMT+1 by TheXception
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2007 Szymon Stefanek (pragma at kvirc dot net)
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your opinion) any later version.
//
//   This program is distributed in the HOPE that it will be USEFUL,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, write to the Free Software Foundation,
//   Inc. ,59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//=============================================================================

#include "plugin.h"

#include "kvi_module.h"
#include "kvi_string.h"
#include "kvi_library.h"
#include "kvi_thread.h"
#include "kvi_locale.h"
#include "kvi_app.h"
#include "kvi_fileutils.h"

#include <qdir.h>
#include <qfileinfo.h>
/*
	@doc: easyplugins
	@type:
		generic
	@keyterms:
		easyplugins
	@title:
		Easyplugins
	@short:
		Small plugins which can be called in scripts
	@body:
		If you want to know how to call easyplugins please have a look at: $system.call()[br]
		This part of the documentation handles only the way how to write an easyplugin. An easyplugin is simply a dll/so. You can create one like you normally make such so/dll files. The important thing is that these so/dll-files export some of the following functions.
		[br][br]
		[b]Exported functions by easyplugin (C/C++-Examples):[/b][br]
		[br][b]_free function[/b] [i] (needed)[/i][br]
		This function is important! Since KVIrc can not free directly the memory of the dll, the plugins need the _free function so that the memory can be freed by the plugin to prevent memory-leaks.[br]
		[example]
		int _free(void * p)[br]
		{[br]
			// Always free the memory here![br]
			free(p);[br]
			return 0;[br]
		}[br]
		[/example]
		
		[br][b]_load function[/b] [i](optional)[/i][br]
		After the plugin has be loaded, KVIrc will call the _load-function. Here you can prepare your plugin stuff.
		[example]
		int _load()[br]
		{[br]
			return 0;[br]
		}[br]
		[/example]		
		
		[br][b]_unload function[/b] [i]((optional)[/i][br]
		This function will be called before the plugins is unloaded. In this function you can clean up memory or other things. 
		After this call there is no guarantee that the plugin will be kept in memory.[br]
		[example]
		int _unload()[br]
		{[br]
			return 0;[br]
		}[br]
		[/example]	

		[br][b]_canunload function[/b] [i](optional)[/i][br]
		The _canunload-function will be called by KVIrc to check if it may unload the plugin. 
		If return value is true KVIrc will unload the plugin, false means he will try unloading it at the next check.[br]
		Important: KVIrc will ignore this if unload of plugins will be forced! So you have to be sure that the _unload function of your plugins cleans up![br]
		[example]
		int _canunload()[br]
		{[br]
			return 0; [br]
		}[br]
		[/example]
		
		[br][b]user function[/b][br]
		This is the general structure of a user function call.[br]
		The important thing here is the handling of return values. To return a value to KVIrc you have to allocate memory and write the pointer to it into pBuffer. Have a look at the example for more details.[br]
		[example]	
		int about(int argc, char * argv[], char ** pBuffer)[br]
		{[br]
		    *pBuffer = (char*)malloc(1024);[br]
		    sprintf((char*)*pBuffer, "Hello World"); [br]
		    return 1;[br]
		}[br]
		[/example]
*/

KviPlugin::KviPlugin(kvi_library_t pLib, const QString& name)
{
	m_Plugin = pLib;
	m_szName = name;
}

KviPlugin::~KviPlugin()
{
}

KviPlugin* KviPlugin::load(const QString& szFileName)
{
	kvi_library_t pLibrary = kvi_library_open(szFileName.local8Bit());
	if (!pLibrary)
	{
		return 0;
	} 

	KviPlugin* pPlugin = new KviPlugin(pLibrary,KviFileUtils::extractFileName(szFileName));
	
	plugin_load function_load;
	
	function_load = (plugin_unload)kvi_library_symbol(pLibrary,"_load");
	if (function_load)
	{
		//TODO: THREAD
		function_load();
	}
	return pPlugin;
}

bool KviPlugin::pfree(char * pBuffer)
{
	plugin_free function_free;
	
	function_free = (plugin_free)kvi_library_symbol(m_Plugin,"_free");
	if (function_free)
	{
		//TODO: THREAD
		if(pBuffer) function_free(pBuffer);
		return true;
	}
	return false;
}

bool KviPlugin::unload()
{
	plugin_unload function_unload;
	
	function_unload = (plugin_unload)kvi_library_symbol(m_Plugin,"_unload");
	if (function_unload)
	{
		//TODO: THREAD
		function_unload();
	}
	
	if(m_Plugin)
	{
		kvi_library_close(m_Plugin);
	}
	
	return true;
}

bool KviPlugin::canunload()
{
	plugin_canunload function_canunload;

	function_canunload = (plugin_canunload)kvi_library_symbol(m_Plugin,"_canunload");
	if (function_canunload)
	{
		//TODO: THREAD
		if(!function_canunload()) 
		{
			return false;
		}
	}
	return true;
}

int KviPlugin::call(const QString& pszFunctionName, int argc, char * argv[], char ** pBuffer)
{
	int r;
	plugin_function function_call;
	function_call = (plugin_function)kvi_library_symbol(m_Plugin,pszFunctionName.local8Bit());
	if (!function_call)
	{
		return -1;
	} else {
		//TODO: THREAD
		r = function_call(argc,argv,pBuffer);
	}
	if (r < 0) r = 0; // negative numbers are for error handling.
	return r;
}

QString KviPlugin::name()
{
	return m_szName;
}

void KviPlugin::setName(const QString& Name)
{
	m_szName = Name;
}


KviPluginManager::KviPluginManager()
{
	m_pPluginDict = new QHash<QString,KviPlugin*>;
	
	m_bCanUnload = true;
}

KviPluginManager::~KviPluginManager()
{
	delete 	m_pPluginDict;
}

bool KviPluginManager::pluginCall(KviKvsModuleFunctionCall *c)
{
	//   /echo $system.call("traffic.dll",about)
	QString szPluginPath; //contains full path and plugin name like "c:/plugin.dll"
	QString szFunctionName;
	
	KVSM_PARAMETERS_BEGIN(c)
		KVSM_PARAMETER("plugin_path",KVS_PT_NONEMPTYSTRING,0,szPluginPath)
		KVSM_PARAMETER("function",KVS_PT_NONEMPTYSTRING,0,szFunctionName)
	KVSM_PARAMETERS_END(c)
	
	//Check if there is such a plugin
	if(!findPlugin(szPluginPath))
	{
		c->error(__tr2qs("Plugin not found. Please check the plugin-name and path."));
		return true;
	}
	
	//Load plugin or check it in cache
	if(!loadPlugin(szPluginPath))
	{
		c->error(__tr2qs("Error while loading plugin."));
		return true;
	}
	
	//Parsing more Parameters
	int iArgc = 0;
	char ** ppArgv;
	char * pArgvBuffer;
	
	//Preparing argv buffer	
	if(c->parameterCount() > 2)
	{
		iArgc = c->parameterCount() - 2;
	}
		
	if (iArgc > 0)
	{	
		int i = 2;
		QString tmp;
		int iSize = 0;
		
		//Calculate buffer size
		while (i < (iArgc + 2) )
		{
			c->params()->value(i)->asString(tmp);
			iSize += tmp.length()+1; //+1 for the \0 characters
			i++;
		}
		
		//Allocate buffer
		ppArgv = (char**)malloc(iArgc*sizeof(char*));
		pArgvBuffer = (char*)malloc(iSize);
		
		i = 2;
		char * x = 0;
		x = pArgvBuffer;
		while (i < (iArgc + 2) )
		{
			ppArgv[i-2] = x;
			c->params()->value(i)->asString(tmp);
			strcpy(x,tmp.local8Bit());
			x += tmp.length();

			*x = 0;
			x++;
			i++;
		}	

	} else {
		//Avoid using unfilled variables
		ppArgv = 0;
		pArgvBuffer = 0;
		iArgc = 0;
	}
	
	//Preparing return buffer
	char * returnBuffer;
	KviPlugin * plugin;
	
	plugin = getPlugin(szPluginPath);
	int r = plugin->call(szFunctionName,iArgc,ppArgv,&returnBuffer);
	
	if(r == -1)
	{
		c->error(__tr2qs("This plugin does not export the desired function."));
		return true;
	}
	if (r > 0)
	{
		c->returnValue()->setString(QString::fromLocal8Bit(returnBuffer));
	}

	
	//Clean up
	if(pArgvBuffer) free(pArgvBuffer);
	if(ppArgv) free(ppArgv);
	if(returnBuffer)
	{
		if (!plugin->pfree(returnBuffer))
		{
			c->warning(__tr2qs("The plugin has no function to free memory. This can result in Memory Leaks!"));
		}
	}

	

	return true;
}

bool KviPluginManager::checkUnload()
{
	/* 	
	Always called when system module should be unloaded
	Checking here if all small "modules" can be unloaded	
	*/
	QHash<QString,KviPlugin*>::iterator it(m_pPluginDict->begin());
	
	m_bCanUnload = true;
	
	while(it != m_pPluginDict->end())
	{
		if(it.value()->canunload())
		{
			it.value()->unload();
			it = m_pPluginDict->erase(it);
		} else {
			m_bCanUnload = false;
			++it;
		}
		
	}
	
	return m_bCanUnload;
}

void KviPluginManager::unloadAll()
{
	QHash<QString,KviPlugin*>::iterator it(m_pPluginDict->begin());
	
	while(it != m_pPluginDict->end())
	{
			it.value()->unload();
			it = m_pPluginDict->erase(it);
	}
}

bool KviPluginManager::findPlugin(QString& szPath)
{
	QString szFileName(KviFileUtils::extractFileName(szPath));
//	szFileName.detach();
	if(KviFileUtils::isAbsolutePath(szPath) && KviFileUtils::fileExists(szPath))
	{
		// Ok, 
		return true;
	} else {
		//Plugin not found in direct way. Looking in kvirc local dir
		g_pApp->getGlobalKvircDirectory(szPath,KviApp::EasyPlugins,szFileName);
		
		if(!KviFileUtils::fileExists(szPath))
		{
			//Plugin not found in kvirc local dir. Looking in kvirc global dir
			g_pApp->getLocalKvircDirectory(szPath,KviApp::EasyPlugins,szFileName);
			
			if(!KviFileUtils::fileExists(szPath))
			{
				return false;
			}
		}
	}
	return true;
}

bool KviPluginManager::isPluginLoaded(const QString& pSingleName)
{
	KviPlugin * p = m_pPluginDict->value(pSingleName);
	if (!p)
		return false;
	else
		return true;
}

bool KviPluginManager::loadPlugin(const QString& szPluginPath)
{
	if(isPluginLoaded(szPluginPath))
	{
		return getPlugin(szPluginPath)!=0;
	} else {
		KviPlugin * plugin = KviPlugin::load(szPluginPath);
		if(plugin)
		{
			m_pPluginDict->insert(szPluginPath,plugin);
			return true;
		}
	}
	return false;
}

KviPlugin * KviPluginManager::getPlugin(const QString& szPluginPath)
{
	KviPlugin * p = m_pPluginDict->value(szPluginPath);
	return p;
}