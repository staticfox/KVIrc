//=============================================================================
//
//   File : kvi_kvs_corefunctions_sz.cpp
//   Created on Fri 31 Oct 2003 01:52:04 by Szymon Stefanek
//
//   This file is part of the KVIrc IRC client distribution
//   Copyright (C) 2003 Szymon Stefanek <pragma at kvirc dot net>
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



#include "kvi_kvs_corefunctions.h"

#include "kvi_kvs_kernel.h"
#include "kvi_kvs_arraycast.h"
#include "kvi_kvs_object_controller.h"

#include "kvi_window.h"
#include "kvi_out.h"
#include "kvi_locale.h"
#include "kvi_app.h"
#include "kvi_options.h"
#include "kvi_channel.h"
#include "kvi_console.h"
#include "kvi_time.h"
#include "kvi_ircuserdb.h"
#include "kvi_modulemanager.h"
#include "kvi_mirccntrl.h"
#include "kvi_sourcesdate.h"
#include "kvi_socket.h"

#include <stdlib.h> // rand & srand

/*
	Data types:

	<variant>
		<nothing>
		<scalar>
			<boolean>
			<string>
			<numeric>
				<integer>
				<real>
		<array>
		<hash>
		<object>

	$isEmpty(<nothing>) == $true

	<nothing> == <empty string>
	<null> == <null object>


*/

namespace KviKvsCoreFunctions
{
	/////////////////////////////////////////////////////////////////////////////////////////

	// FIXME: #warning "THIS HAS TO WORK FOR QUERIES TOO!"
	/*
		@doc: selected
		@type:
			function
		@title:
			$selected
		@short:
			Returns the list of selected nicknames in the channel
		@syntax:
			<array> $selected
			<array> $selected(<window id:string>)
		@description:
			The form with the <window id> parameter returns an array of the selected
			nicknames in the channel designated by <window id>.
			The form without parameters returns an array of the selected nicknames
			in the current window (assuming that it is a channel),
			thus it is equivalent to calling $selected([fnc]$window[/fnc])
			The returned value may be assigned to a dictionary too: it will be used to simulate an array.[br]
			In a non-array/dictionary context it returns the selected nicknames as a comma separated list.
		@examples:
			[example]
				[cmd]echo[/cmd] $selected
				[cmd]foreach[/cmd](%i,$selected)[cmd]echo[/cmd] %i
			[/example]
		@seealso:
			[fnc]$window[/fnc],
			[fnc]$channel[/fnc],
			[doc:window_naming_conventions]Window naming conventions[/doc]
	*/

	KVSCF(selected)
	{
		QString winId;
		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("winId",KVS_PT_NONEMPTYSTRING,KVS_PF_OPTIONAL,winId)
		KVSCF_PARAMETERS_END

		KviWindow * wnd;
		if(KVSCF_pParams->count() > 0)
		{
			wnd = g_pApp->findWindow(winId.utf8().data());
			if(!wnd)
			{
				KVSCF_pContext->warning(__tr2qs("Window with ID '%s' not found, returning empty string"),winId.utf8().data());
				KVSCF_pRetBuffer->setNothing();
				return true;
			}
		} else {
			wnd = KVSCF_pContext->window();
		}

		if(wnd->type() != KVI_WINDOW_TYPE_CHANNEL)
		{
			KVSCF_pContext->warning(__tr2qs("The specified window is not a channel"));
			KVSCF_pRetBuffer->setNothing();
			return true;
		}

		KviKvsArray * a = new KviKvsArray();

		kvs_int_t i = 0;
		for(QString * s = ((KviChannel *)wnd)->firstSelectedNickname();s;s = ((KviChannel *)wnd)->nextSelectedNickname())
		{
			a->set(i,new KviKvsVariant(*s));
			i++;
		}

		KVSCF_pRetBuffer->setArray(a);
		return true;
	}


		/*
		@doc: sentBytes
		@type:
			function
		@title:
			$sentBytes
		@short:
			Returns total sent bytes
		@syntax:
			<uint> $sentBytes()
		@description:
			Returns total sent bytes
		@seealso:
			[fnc]$receivedBytes[/fnc]
	*/

	KVSCF(sentBytes)
	{
		KVSCF_pRetBuffer->setInteger(g_uOutgoingTraffic);
		return true;
	}

	/*
		@doc: serialize
		@type:
			function
		@title:
			<string> $serialize(<data:mixed>)
		@short:
			Encodes variable to JSON string
		@syntax:
			<string> $serialize(<data:mixed>)
		@description:
			Decodes JSON-encoded string
			$serialize() returns a string containing a byte-stream representation of value that can be stored anywhere.
		@seealso:
			[fnc]$serialize[/fnc]
	*/
	KVSCF(serialize)
	{
		KviKvsVariant *pVar = 0;
		QString szBuffer;

		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("data",KVS_PT_VARIANT,0,pVar)
		KVSCF_PARAMETERS_END

		if(!pVar) return false;
		pVar->serialize(szBuffer);
		KVSCF_pRetBuffer->setString(szBuffer);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: server
		@type:
			function
		@title:
			$server
		@short:
			Returns the current server name (if any)
		@syntax:
			<string> $server[(irc_context_id:uint)]
		@description:
			Returns the current server name of the specified IRC context.[br]
			If no <irc_context_id> is specified, the current IRC context is used.[br]
			If you are not connected to a server, this function will return an empty string.[br]
			If the current window does not belong to any IRC context and no irc_context_id
			is specified, this function prints a warning and also returns an empty string.[br]
	*/

	KVSCF(server)
	{
		kvs_uint_t uCntx;

		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("irc_context_id",KVS_PT_UINT,KVS_PF_OPTIONAL,uCntx)
		KVSCF_PARAMETERS_END

		KviConsole * cns;

		if(KVSCF_pParams->count() > 0)
		{
			cns = g_pApp->findConsole(uCntx);
			if(cns)
			{
				if(cns->context()->isConnected() || cns->context()->isLoggingIn())
					KVSCF_pRetBuffer->setString(cns->connection()->currentServerName());
				else
					KVSCF_pRetBuffer->setNothing();
			} else {
				KVSCF_pRetBuffer->setNothing();
			}
		} else {
			if(KVSCF_pContext->window()->console())
			{
				cns = KVSCF_pContext->window()->console();
				if(cns->context()->isConnected() || cns->context()->isLoggingIn())
					KVSCF_pRetBuffer->setString(cns->connection()->currentServerName());
				else
					KVSCF_pRetBuffer->setNothing();
			} else {
				KVSCF_pContext->warning(__tr2qs("This window has no associated IRC context"));
				KVSCF_pRetBuffer->setNothing();
			}
		}
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: sort
		@type:
			function
		@title:
			$sort
		@short:
			Sorts an array
		@syntax:
			<array> $sort(<data:array>)
		@description:
			Sorts an array in ascending order.
		@seealso:
			[fnc]$rsort[/fnc]
	*/

	KVSCF(sort)
	{
		KviKvsArrayCast a;

		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("data",KVS_PT_ARRAYCAST,0,a)
		KVSCF_PARAMETERS_END

		if(a.array())
		{
			KviKvsArray * arry = new KviKvsArray(*(a.array()));
			arry->sort();
			KVSCF_pRetBuffer->setArray(arry);
		} else {
			KVSCF_pRetBuffer->setArray(new KviKvsArray());
		}
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: split
		@type:
			function
		@title:
			$split
		@short:
			DEPRECATED: Use $str.split instead!
		@description:
			This function has been moved to the str module.
			See [fnc]$str.split[/fnc]()
	*/

	KVSCF(split)
	{
		KviModule * m = g_pModuleManager->getModule("str");
		if(!m)
		{
			KVSCF_pContext->error(__tr2qs("Module function call failed: can't load the module 'str'"));
			return false;
		}

		KviKvsModuleFunctionExecRoutine * proc = m->kvsFindFunction("split");
		if(!proc)
		{
			KVSCF_pContext->error(__tr2qs("Module function call failed: the module 'str' doesn't export a function named 'split'"));
			return false;
		}

		KviKvsModuleFunctionCall call(m,KVSCF_pContext,KVSCF_pParams,KVSCF_pRetBuffer);

		return (*proc)(&call);
	}


	/////////////////////////////////////////////////////////////////////////////////////////////
	
	/*
		@doc: string
		@type:
			function
		@title:
			$string
		@short:
			Casts a variable to a string
		@syntax:
			<integer> $string(<data:variant>)
		@description:
			Forces <data> to be a string data type with the following semantics:
			[ul]
				[li]If <data> is a string then <data> itself is returned.[/li]
				[li]If <data> is an integer then its decimal rappresentation is returned.[/li]
				[li]If <data> is a real then its decimal floating-point rappresentation is returned.[/li]
				[li]If <data> is a boolean then the string "1" is returned for a true value and the string "0" for a false value.[/li]
				[li]If <data> is nothing (unset) then an empty string is returned[/li]
				[li]If <data> is an array then a string with all the items converted to strings and separated by commas is returned[/li]
				[li]If <data> is a hash then a string with all the values converted to strings and separated by commas is returned[/li]
				[li]If <data> is a hobject then the string "object" is returned[/li]
			[/ul]
			Note that since KVIrc does most of the casting work automatically
			you shouldn't need to use this function.
		@seealso:
			[fnc]$real()[/fnc]
			[fnc]$integer()[/fnc]
	*/

	KVSCF(string)
	{
		KviKvsVariant * v;
		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("data",KVS_PT_VARIANT,0,v)
		KVSCF_PARAMETERS_END

		QString szVal;
		v->asString(szVal);
		KVSCF_pRetBuffer->setString(szVal);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: sw
		@type:
			function
		@title:
			$sw
		@short:
			Returns the value of a switch for an alias
		@syntax:
			<variant> $sw(<switch_name:string>[,<long_switch_name:string>])
		@description:
			This function is valid and useful only in aliases.
			It allows an alias to handle switches just like any other
			KVIrc command. If a switch in the form -<letter> was
			passed to the current alias then $sw(<letter>)
			returns 1 (true). If a switch in the form -<letter>=<value>
			was passed to the current alias then <value> is returned.
			If the switch was not present at all then this function
			returns an empty string (that evaluates to false in an expression).
			A warning is printed if this function is used non-alias code.
		@examples:
			[example]
				[cmd]alias[/cmd](test){
					if($sw(a,append)) [cmd]echo[/cmd] "Switch -a was passed"
					%x = $sw(x);
					if(%x) [cmd]echo[/cmd] "Switch -x=%x was passed"
				}
				test -a
				test -x
				test --append -x
				test -a -x
				test -a -x=test
				test -a=10 -x=test
			[/example]
	*/

	KVSCF(sw)
	{
		QString szSwitch;
		QString szLongSwitch;

		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("switch_name",KVS_PT_STRING,0,szSwitch)
			KVSCF_PARAMETER("long_name",KVS_PT_STRING | KVS_PF_OPTIONAL ,0,szLongSwitch)
		KVSCF_PARAMETERS_END

		KviKvsSwitchList * sl = KVSCF_pContext->aliasSwitchList();
		if(!sl)
		{
			KVSCF_pContext->warning(__tr2qs("The $sw() function can be used only in aliases"));
			return true;
		}

		KviKvsVariant * v;
		

		if(szSwitch.length() > 1)
		{
			if(szLongSwitch.isEmpty())
				v = sl->find(szSwitch);
			else
				v = sl->find(szSwitch[0].unicode(),szLongSwitch);
		}else {
			if(szLongSwitch.isEmpty())
				v = sl->find(szSwitch[0]);
			else
				v = sl->find(szSwitch[0].unicode(),szLongSwitch);
		}

		if(v)KVSCF_pRetBuffer->copyFrom(*v);
		else KVSCF_pRetBuffer->setNothing();
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: target
		@type:
			function
		@title:
			$target
		@short:
			Returns the target of the current window
		@syntax:
			<string> $target
			<string> $target(<window id>)
		@description:
			The form with the <window id> parameter returns the target
			of the channel,query or dcc that has the specified ID.
			The form without parameters returns the target of the current window,
			thus it is equivalent to calling $target([fnc]$window[/fnc]).
			For channel windows the target is the channel name,
			for query windows it is the list of the "queried" users, for the
			dcc windows it is the remote end of the connection.
			The other windows have an empty target.
		@examples:
			[example]
				[cmd]echo[/cmd] $target
			[/example]
		@seealso:
			[fnc]$window[/fnc],
			[fnc]$console[/fnc],
			[fnc]$channel[/fnc],
			[fnc]$query[/fnc],
			[doc:window_naming_conventions]Window naming conventions[/doc]
	*/

	KVSCF(target)
	{
		QString winId;
		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("winId",KVS_PT_NONEMPTYSTRING,KVS_PF_OPTIONAL,winId)
		KVSCF_PARAMETERS_END

		KviWindow * wnd;
		if(KVSCF_pParams->count() > 0)
		{
			wnd = g_pApp->findWindow(winId.utf8().data());
			if(!wnd)
			{
				KVSCF_pContext->warning(__tr2qs("Window with ID '%s' not found, returning empty string"),winId.utf8().data());
				KVSCF_pRetBuffer->setNothing();
				return true;
			}
		} else {
			wnd = KVSCF_pContext->window();
		}

		//debug("CALLING $target on window %s",wnd->name());
		QString szTa = wnd->target();


		KVSCF_pRetBuffer->setString(wnd->target());
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: this
		@type:
			function
		@title:
			$this
		@short:
			Retrieves the ID of the current object
		@syntax:
			$this
		@description:
			Returns the ID of the current object or ('0') if there is
			none. This function has a "quick" version with syntax:
			[b]$$[/b][br]
	*/

	/*
		@doc: $
		@type:
			function
		@title:
			$$
		@short:
			Retrieves the ID of the current object
		@syntax:
			$$
		@description:
			Returns the ID of the current object or ('0') if there is
			none. This function has equivalent to [fnc]$this[/fnc]
	*/

	KVSCF(thisCKEYWORDWORKAROUND)
	{
		// prologue: parameter handling
		KviKvsObject * o = KVSCF_pContext->thisObject();
		KVSCF_pRetBuffer->setHObject(o ? o->handle() : ((kvs_hobject_t)0));
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: time
		@type:
			function
		@title:
			$time
		@short:
			Returns a formatted date/time string
		@syntax:
			$time(<unixtime>)
			$time
		@description:
			DEPRECATED: use [fnc]$date[/fnc]
	*/

	KVSCF(timeCFUNCTIONWORKAROUND)
	{
		kvs_int_t iTime;
		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("unixtime",KVS_PT_INT,KVS_PF_OPTIONAL,iTime)
		KVSCF_PARAMETERS_END

		kvi_time_t t = (kvi_time_t)iTime;
		KVSCF_pRetBuffer->setString(QString(ctime(&t)));
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: script_localization
		@type:
			generic
		@title:
			Localization of scripts
		@short:
			Explains how to add translation capabilities to your scripts
		@body:
			[p]
			[big]Introduction[/big]
			[/p]
			[p]
			Adding the translated versions of the strings adds a great
			value to your scripts. The process of translating a part of
			software is called localization. KVIrc offers some commands
			and functions for this purpose and this document explains
			briefly how to use them.
			[/p]

			[p]
			[big]The big picture[/big]
			[/p]
			[p]
			All of the strings in your script are written in a "primary language".
			The most common "primary language" is english, but theoretically
			it can be any language of your choice.
			[/p]
			[p]
			By the means of the gettext package programs you extract
			the strings from your script and produce a translation file.
			[/p]
			[p]
			The translation file is then effectively translated in another
			language and later compiled in a binary form.
			[/p]
			[p]
			The binary form translations are then loaded in the KVIrc
			executable at runtime and a function is used to look up the
			translations.
			[/p]

			[p]
			[big]How to translate scripts[/big]
			[/p]
			[p]
			Your strings should be surrounded by the [fnc]$tr[/fnc]() function in the following way:
			[example]
				[fnc]$tr[/fnc]("your default language text")
			[/example]
			[/p]
			[p]
			Then you should run the xgettext command on your script files.
			This is done by a shell commandline similar to the following:[br]
			[pre]xgettext -o myscript.pot -ktr mykvsfile1.kvs mykvsfile2.kvs ...[/pre]
			[/p]
			[p]
			Copy the translation file obtained in the following way:[br]
			[pre]cp myscript.pot myscript_XX.pot[/pre]
			Where the XX is your country/language code. For example, for Italian
			it would be:[br]
			[pre]cp myscript.pot myscript_it.po[/pre]
			[/p]
			[p]
			Translate mytranslation_it.po. The format of the po file is straightforward.
			There are msgid lines with the original english text and immediately
			following msgstr lines that must be filled with the corresponding translation.
			For example in Italian you would translate:[br]
			msgid "your default language text"
			msgstr "il tuo testo in linguaggio predefinito"
			[/p]
			[p]
			Compile your translation to binary form with the following command:[br]
			[pre]msgfmt -o myscript_it.mo myscript_it.po[/pre]
			[/p]
			[p]
			Copy the generated *.mo file to the "locale" subdirectory
			in the KVIrc's local directory (usually $HOME/.kvirc/locale/).
			[/p]
			[p]
			Set the system language to the XX above with the following command:[br]
			[pre]export LANG="XX"[/pre][br]
			For Italian it would be:[br]
			[pre]export LANG="it"[/pre][br]
			[/p]
			[p]
			Start KVIrc and type in the commandline:
			[example]
			[cmd]echo[/cmd] [fnc]$tr[/fnc]("your default language text","myscript")
			[/example]
			If you did everything well, you should see the translated
			text echoed in the window :)[br]
			[/p]
			[p]
			Obviously if you don't set LANG="XX", the same command will
			output the original string unchanged.
			[/p]
			[p]
			You can manage translations in several languages by producing
			several *.mo files all with the proper language/country code appended.
			The right *.mo file will be magically loaded by KVIrc that
			will look up the user's LANG variable. (If you don't want
			to use LANG, you can use KVIRC_LANG instead, it will still work).
			[/p]

			[big]Caveats[/big]
			[/p]
			[p]
			You should NEVER use variables or identifiers inside the $tr() function.
			This because the translation files are generated offline,
			when the string is not evaluated yet (i.e variables ad identifiers
			are not substituted by their actual return values).
			The translation process, instead, happens at runtime, when
			the variables and identifiers have been substituted by their
			actual values. This would lead to a mismatch between the
			string you look up in the translation catalogue and the
			effectively translated one. If you need to include variables
			in your strings you should compose the string with smaller pieces
			[example]
				[cmd]echo[/cmd] [fnc]$tr[/fnc]("On this channel") %number [fnc]$tr[/fnc]("users are operators")
			[/example]
			[/p]
			[p]
			The translation process can be realized only if your
			scripts are written in external files. This makes sense since
			if you're translating the script then you will probably want to
			distribute it and the only way to distribute it is on files.
			But well.. this is a caveat.
			[/p]
	*/

	/*
		@doc: tr
		@type:
			function
		@title:
			$tr
		@short:
			Translates an english string to the current language
		@syntax:
			<string> $tr(<default_language_string:string>[,<catalogue:string>])
		@description:
			This function searches for the translation of <default_language_string>
			in the specified translation <catalogue> or in the main
			KVIrc translation file if <catalogue> is omitted.[br]
			If no translation is found then <english_string> is returned.[br]
			<default_language_string> is a string in your script default
			language (which should probably be english since it is the
			most common language spoken by the translators...).[br]
			If the <catalogue> is not loaded yet, KVIrc will attempt to load it,
			but only the first time that the catalogue is accessed (i.e. a load
			failure will cause the catalogue to be ignored completly until [cmd]trunload[/cmd]
			is explicitly used.[br]
			KVIrc will search the catalogue only in [fnc]$file.localdir[/fnc]/locale/
			and in [fnc]$file.globaldir[/fnc]/locale/. If your catalogues are
			in some other place then you must load them explicitly by the
			means of [cmd]trload[/cmd].
			For more informations see the documentation about [doc:script_localization]script localization[/doc].
		@examples:
			[example]
				[cmd]echo[/cmd] $tr("Hello World!")
			[/example]
		@seealso:
			[cmd]trload[/cmd], [cmd]trunload[/cmd]
	*/

	KVSCF(tr)
	{
		// prologue: parameter handling
		QString szString;
		QString szCatalogue;

		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("default_language_string",KVS_PT_STRING,0,szString)
			KVSCF_PARAMETER("catalogue",KVS_PT_STRING,KVS_PF_OPTIONAL,szCatalogue)
		KVSCF_PARAMETERS_END
		if(szString.isEmpty()) return true;
		// body: the real job
		QString translation;

		if(!szCatalogue.isEmpty())
		{
			KviMessageCatalogue * pCat = KviLocale::getLoadedCatalogue(szCatalogue);
			if(pCat)
			{
				translation = pCat->translateToQString(szString);
			} else {
				// attempt to load it automatically
				QString szDir;
				g_pApp->getLocalKvircDirectory(szDir,KviApp::Locale);
				if(!KviLocale::loadCatalogue(szCatalogue,szDir))
				{
					g_pApp->getGlobalKvircDirectory(szDir,KviApp::Locale);
					KviLocale::loadCatalogue(szCatalogue,szDir);
				}
				// If the code above fails to load the catalogue
				// then __tr2qs_ctx_no_xgettext will place
				// a dummy catalogue in its place
				// This means that the next call to getLoadedCatalogue will
				// not fail unless /trunload is explicitly used
				// This will avoid trashing the user's disk too much
				// when a catalogue for a given language is not available
				translation = __tr2qs_ctx_no_xgettext(szString,szCatalogue);
			}
		} else {
			translation = __tr2qs_no_xgettext(szString);
		}

		// epilogue: set the return value
		KVSCF_pRetBuffer->setString(translation);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: true
		@type:
			function
		@title:
			$true
		@short:
			The boolean true constant
		@syntax:
			<boolean> $true
		@description:
			Evaluates to the true boolean constant. True
			is equivalent to the integer 1 too. This function/constant
			is useful to keep your code readable: when you
			have a variable that can assume boolean values it's
			nicer to use $true and [fnc]$false[/fnc] instead of
			the integer constants 1 and 0. The reader will
			undestand immediately that the variable simply can't
			assume any other value.
		@examples:
			[example]
				%a = $true
				[cmd]echo[/cmd] $typeof(%a)
				[cmd]echo[/cmd] $(%a + 1)
			[/example]
		@seealso:
			[fnc]$false[/fnc]
	*/

	KVSCF(trueCKEYWORDWORKAROUND)
	{
		KVSCF_pRetBuffer->setBoolean(true);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: typeof
		@type:
			function
		@title:
			$typeof
		@short:
			Returns the internal data type of a variable
		@syntax:
			<string> $typeof(<data:variant>)
		@description:
			Returns the internal data type of the <data>.
		@examples:
			[example]
				[cmd]echo[/cmd] $typeof("test")
				%a = "test"
				[cmd]echo[/cmd] $typeof(%a)
				%a = 1
				[cmd]echo[/cmd] $typeof(%a)
				%a = $(1 + 2)
				[cmd]echo[/cmd] $typeof(%a)
				[cmd]echo[/cmd] $typeof($typeof(%a))
			[/example]
	*/

	KVSCF(typeofCKEYWORDWORKAROUND)
	{
		KviKvsVariant * v;

		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("data",KVS_PT_VARIANT,0,v)
		KVSCF_PARAMETERS_END

		QString szType;
		v->getTypeName(szType);
		KVSCF_pRetBuffer->setString(szType);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: u
		@type:
			function
		@title:
			$u
		@short:
			Returns the UNDERLINE mIRC control character
		@syntax:
			<string> $u
		@description:
			Returns the UNDERLINE mIRC control character (CTRL+U).[br]
		@seealso:
			[fnc]$k[/fnc],[fnc]$b[/fnc],[fnc]$r[/fnc],[fnc]$o[/fnc]
	*/

	KVSCF(u)
	{
		KVSCF_pRetBuffer->setString(QString(QChar(KVI_TEXT_UNDERLINE)));
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: unicode
		@type:
			function
		@title:
			$unicode
		@short:
			Returns the UNICODE code of a sets of characters
		@syntax:
			<variant> $unicode(<char:string>)
		@description:
			If <char> is composed only of a single character
			then returns its UNICODE code point as an integer.
			If <char> is composed of more than one character
			then an array of UNICODE code points is returned.
		@seealso:
			[fnc]$cr[/fnc],[fnc]$lf[/fnc],[fnc]$char[/fnc]
	*/

	KVSCF(unicode)
	{
		QString sz;
		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("char",KVS_PT_NONEMPTYSTRING,0,sz)
		KVSCF_PARAMETERS_END

		if(sz.length() > 1)
		{
			KviKvsArray * a = new KviKvsArray();
			for(kvs_int_t i=0;i<sz.length();i++)
				a->set(i,new KviKvsVariant((kvs_int_t)(sz[(int)i].unicode())));
			KVSCF_pRetBuffer->setArray(a);
		} else {
			KVSCF_pRetBuffer->setInteger((kvs_int_t)(sz[0].unicode()));
		}
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: unixtime
		@type:
			function
		@title:
			$unixTime
		@short:
			Returns the current UNIX time
		@syntax:
			<integer> $unixtime
		@description:
			Returns the time since the Epoch (00:00:00 UTC, January 1, 1970),
			measured in seconds.
		@seealso:
			[fnc]$time[/fnc], [fnc]$date[/fnc], [fnc]$hptimestamp[/fnc]
	*/

	KVSCF(unixtime)
	{
		KVSCF_pRetBuffer->setInteger((kvs_int_t)(time(0)));
		return true;
	}

	/*
		@doc: unserialize
		@type:
			function
		@title:
			<mixed> $unserialize(<data:string>)
		@short:
			Decodes JSON-encoded string
		@syntax:
			<mixed> $unserialize(<data:string>)
		@description:
			Decodes JSON-encoded string
		@seealso:
			[fnc]$serialize[/fnc]
	*/

	KVSCF(unserialize)
	{
		QString szData;

		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("data",KVS_PT_NONEMPTYSTRING,0,szData)
		KVSCF_PARAMETERS_END

		KviKvsVariant *pVar = KviKvsVariant::unserialize(szData);
		if(pVar)
			KVSCF_pRetBuffer->copyFrom(pVar);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: username
		@type:
			function
		@title:
			$username
		@short:
			Returns the username of the specified user
		@syntax:
			<string> $username(<nickname:string>)
		@description:
			Returns the username of the specified IRC user IF it is known.[br]
			The username is known if [fnc]$isWellKnown[/fnc] returns 1.[br]
			The username is generally known if the user is on a channel with you
			or has an open query with you.[br]
			Detailed explaination:[br]
			KVIrc has an internal database of users that are currently
			visible by *this client*: this includes users on open channels
			and queries.[br] The other IRC users are NOT in the database:
			this means that KVIrc knows NOTHING about them and can't return
			any information immediately. In this case this function will return
			an EMPTY string.[br]
			If a user is in the database, at least his nickname is known.[br]
			The username and hostname are known only if the server provides that information
			spontaneously or after a KVIrc request.[br]
			KVIrc requests user information for all the users in open queries
			and channels. This information takes some time to be retrieved,
			in this interval of time KVIrc knows only the user's nickname.
			This function will return the string "*" in this case.[br]
		@seealso:
			[fnc]$iswellknown[/fnc], [$fnc]$hostname[/fnc], [cmd]awhois[/cmd]
	*/

	KVSCF(username)
	{
		QString szNick;

		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("nick",KVS_PT_NONEMPTYSTRING,0,szNick)
		KVSCF_PARAMETERS_END

		if(KVSCF_pContext->window()->console())
		{
			if(KVSCF_pContext->window()->console()->isConnected())
			{
				KviIrcUserEntry * e = KVSCF_pContext->window()->connection()->userDataBase()->find(szNick);
				if(e)
				{
					KVSCF_pRetBuffer->setString(e->user());
					return true;
				}
			}
		}

		KVSCF_pRetBuffer->setNothing();
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
		@doc: version
		@type:
			function
		@title:
			$version
		@short:
			Returns informations about the version of KVIrc.
		@syntax:
			<string> $version()
			<string> $version(<type:string>)
		@description:
			Returns informations about the version of the currently running KVIrc.[br]
			Type can be one of:[br]
			[ul]
			[li]v: return the current numeric version[/li]
			[li]s: return the current sources date[/li]
			[li]r: return the release name[/li]
			[li]b: return the build date in human readable form[/li]
			[/ul]
			If <type> is omitted then v is assumed.[br]
		@examples:
			[example]
				[cmd]echo[/cmd] $version $version(r)
			[/example]
		@seealso:
			[fnc]$features[/fnc]
	*/

	KVSCF(version)
	{
		QString szType;
		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("type",KVS_PT_STRING,KVS_PF_OPTIONAL,szType)
		KVSCF_PARAMETERS_END

		if(szType.isEmpty())
			KVSCF_pRetBuffer->setString(KVI_VERSION);
		else {
			if(szType.find('r') != -1)KVSCF_pRetBuffer->setString(KVI_RELEASE_NAME);
			else if(szType.find('s') != -1)KVSCF_pRetBuffer->setString(KVI_SOURCES_DATE);
			else if(szType.find('b') != -1)KVSCF_pRetBuffer->setString(KVI_BUILD_DATE);
			else KVSCF_pRetBuffer->setString(KVI_VERSION);
		}
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	/*
 		@doc: window
 		@type:
 			function
		@title:
			$window
		@short:
			Retrieve the id of a specified window.
		@syntax:
			<string> $window
			<string> $window([<caption_text:hash>[,<context_id:uint>]])
		@description:
			Returns the [b]window id[/b] of the first window that
			has the specified <caption text>.[br]
			If no window matches the specified <caption text>, and invalid
			window id is returned (0).[br]
			If no <caption text> is specified , this function returns the id
			of the current window.[br]
			<context_id> restricts search in only one irc context.
		@examples:
 			[example]
 				[cmd]echo[/cmd] This is the window with id $window
			[/example]
		@seealso:
			[fnc]$channel[/fnc],
 			[fnc]$query[/fnc],
			[fnc]$console[/fnc],
			[doc:window_naming_conventions]Window naming conventions[/doc]
	*/

 	KVSCF(window)
 	{
		//#warning "FIXME: the window identifiers could be numbers!"
		QString szCaption;
		kvs_int_t iContextId; // kvs_int_t is 64bit while int is 32 (and thus KVSCF_PARAMETER() crashes)

		KVSCF_PARAMETERS_BEGIN
			KVSCF_PARAMETER("caption_text",KVS_PT_STRING,KVS_PF_OPTIONAL,szCaption)
			KVSCF_PARAMETER("context_id",KVS_PT_INTEGER,KVS_PF_OPTIONAL,iContextId)
		KVSCF_PARAMETERS_END

		if(KVSCF_pParams->count() < 2)
			iContextId = -1;

		KviWindow * pWnd;
		if(szCaption.isEmpty())
		{
			pWnd = KVSCF_pContext->window();
		} else {
			pWnd = g_pApp->findWindowByCaption(szCaption,iContextId);
			if(!pWnd)
			{
				//follow the documented behaviour
				KVSCF_pRetBuffer->setInteger(0);
				return true;
			}
		}
		KVSCF_pRetBuffer->setInteger(pWnd->numericId());
		return true;
	}
};

