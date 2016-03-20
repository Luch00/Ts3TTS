/*
* TeamSpeak 3 Japanese To Balabolka plugin
*
* Copyright (c) 2014 Luch
*/
using namespace std;
#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <assert.h>
#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_rare_definitions.h"
#include "ts3_functions.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <codecvt>
#include <locale>
#include <vector>
#include <map>
#include <iterator>
#include "plugin.h"
static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 20

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128

static char* pluginID = NULL;

std::map<std::string, std::string> options; // global?
bool jpn;

#ifdef _WIN32
/* Helper function to convert wchar_T to Utf-8 encoded strings on Windows */
static int wcharToUtf8(const wchar_t* str, char** result) {
	int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
	*result = (char*)malloc(outlen);
	if (WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
		*result = NULL;
		return -1;
	}
	return 0;
}
#endif

/*********************************** Required functions ************************************/
/*
* If any of these required functions is not implemented, TS3 will refuse to load the plugin
*/

/* Unique name identifying this plugin */
const char* ts3plugin_name() {
#ifdef _WIN32
	/* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
	static char* result = NULL;  /* Static variable so it's allocated only once */
	if (!result) {
		const wchar_t* name = L"Ts3TTS";
		if (wcharToUtf8(name, &result) == -1) {  /* Convert name into UTF-8 encoded result */
			result = "Ts3TTS";  /* Conversion failed, fallback here */
		}
	}
	return result;
#else
	return "Ts3TTS";
#endif
}

/* Plugin version */
const char* ts3plugin_version() {
	return "1.1";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
	return "Luch";
}

/* Plugin description */
const char* ts3plugin_description() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
	return "Configuration in the plugin folder";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
	ts3Functions = funcs;
}

/*
* Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
* If the function returns 1 on failure, the plugin will be unloaded again.
*/
int ts3plugin_init() {
	setlocale(LC_ALL, "");
	char pluginPath[PATH_BUFSIZE];

	/* Your plugin init code here */
	//printf("PLUGIN: init\n");

	/* Example on how to query application, resources and configuration paths from client */
	/* Note: Console client returns empty string for app and resources path */
	ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE);

	std::string cfg = std::string(pluginPath) + "Ts3TTS\\settings.ini";
	std::string line;
	std::ifstream infile(cfg.c_str());
	while (std::getline(infile, line)) {
		if (line[0] != '#') {
			std::istringstream iss(line);
			std::string key;
			if (std::getline(iss, key, '=')) {
				printf("PLUGIN: CFG: KEY %s\n", key.c_str());
				if (key != "path" && key != "cmd" && key != "jpn") {
					return 1;
				}
				std::string value;
				if (std::getline(iss, value)) {
					printf("PLUGIN: CFG: VALUE %s\n", value.c_str());
					if (key != "jpn") {
						options[key] = value;
					}
					else {
						if (value == "true") {
							jpn = true;
						}
						else {
							jpn = false;
						}
					}
				}
			}
		}
		else {
			printf("PLUGIN: CFG: %s\n", "Comment!");
		}
	}

	//printf("PLUGIN: App path: %s\nResources path: %s\nConfig path: %s\nPlugin path: %s\n", appPath, resourcesPath, configPath, pluginPath);

	return 0;  /* 0 = success, 1 = failure, -2 = failure but client will not show a "failed to load" warning */
			   /* -2 is a very special case and should only be used if a plugin displays a dialog (e.g. overlay) asking the user to disable
			   * the plugin again, avoiding the show another dialog by the client telling the user the plugin failed to load.
			   * For normal case, if a plugin really failed to load because of an error, the correct return value is 1. */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
	/* Your plugin cleanup code here */
	//printf("PLUGIN: shutdown\n");

	/*
	* Note:
	* If your plugin implements a settings dialog, it must be closed and deleted here, else the
	* TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
	*/

	/* Free pluginID if we registered it */
	if (pluginID) {
		free(pluginID);
		pluginID = NULL;
	}
}

/****************************** Optional functions ********************************/
/*
* Following functions are optional, if not needed you don't need to implement them.
*/

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure() {
	/*
	* Return values:
	* PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
	* PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
	* PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
	*/
	return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
}

/* Plugin might offer a configuration window. If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
void ts3plugin_configure(void* handle, void* qParentWidget) {
}

/*
* If the plugin wants to use error return codes, plugin commands, hotkeys or menu items, it needs to register a command ID. This function will be
* automatically called after the plugin was initialized. This function is optional. If you don't use these features, this function can be omitted.
* Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
*/
void ts3plugin_registerPluginID(const char* id) {
	const size_t sz = strlen(id) + 1;
	pluginID = (char*)malloc(sz * sizeof(char));
	_strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
								//printf("PLUGIN: registerPluginID: %s\n", pluginID);
}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
	return "";
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
	return 0;  /* Plugin handled command */
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
	//printf("PLUGIN: currentServerConnectionChanged %llu (%llu)\n", (long long unsigned int)serverConnectionHandlerID, (long long unsigned int)ts3Functions.getCurrentServerConnectionHandlerID());
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data) {
	free(data);
}

/*
* Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
* the user manually disabled it in the plugin dialog.
* This function is optional. If missing, no autoload is assumed.
*/
int ts3plugin_requestAutoload() {
	return 0;  /* 1 = request autoloaded, 0 = do not request autoload */
}

/************************** TeamSpeak callbacks ***************************/
/*
* Following functions are optional, feel free to remove unused callbacks.
* See the clientlib documentation for details on each function.
*/

/* Clientlib */

int ts3plugin_onTextMessageEvent(uint64 serverConnectionHandlerID, anyID targetMode, anyID toID, anyID fromID, const char* fromName, const char* fromUniqueIdentifier, const char* message, int ffIgnored) {
	//printf("PLUGIN: onTextMessageEvent %llu %d %d %s %s %d\n", (long long unsigned int)serverConnectionHandlerID, targetMode, fromID, fromName, message, ffIgnored);

	/* Friend/Foe manager has ignored the message, so ignore here as well. */
	if (ffIgnored) {
		return 0; /* Client will ignore the message anyways, so return value here doesn't matter */
	}

	std::vector<wchar_t> s;
	if (jpn) {
		s = checkJapanese(toWide(message));
	}
	else {
		std::wstring w = toWide(message);
		std::copy(w.begin(), w.end(), back_inserter(s));
	}
	if (s.size() > 0)
	{
		runVoice(s);
	}

	return 0;  /* 0 = handle normally, 1 = client will ignore the text message */
}

std::wstring toWide(const char* source) {
	int wideLen = MultiByteToWideChar(CP_UTF8, 0, source, (int)strlen(source), NULL, 0);
	std::wstring buffer;
	buffer.resize(wideLen);
	MultiByteToWideChar(CP_UTF8, NULL, source, (int)strlen(source), &buffer[0], wideLen);
	return buffer;
}

std::vector<wchar_t> checkJapanese(std::wstring input) {
	std::vector<wchar_t> buffer;
	for (std::wstring::size_type n = 0; n < input.size(); n++)
	{
		if (isInRange(input[n], 0x3040, 0x309F) || isInRange(input[n], 0x30A0, 0x30FF) || isInRange(input[n], 0x4E00, 0x9FBF) || isInRange(input[n], 0x3000, 0x303F) || isInRange(input[n], 0xFF00, 0xFFEF))
		{
			buffer.push_back(input[n]);
		}
	}
	return buffer;
}

void runVoice(std::vector<wchar_t> s) {

	std::wstringstream stream;
	stream << options["cmd"].c_str();
	stream << " -t \"";
	for (int i = 0; i < s.size(); i++)
	{
		stream << s[i];
	}
	stream << L"\"";

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	CreateProcessW(toWide(options["path"].c_str()).c_str(), (LPWSTR)stream.str().c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
}

bool isInRange(wchar_t c, int min, int max)
{
	if (c >= min && c <= max)
	{
		//printf("PLUGIN: TRUE min: %X char: %X max: %X\n", min, c, max);
		return true;
	}
	else
	{
		//printf("PLUGIN: FALSE min: %X char: %X max: %X\n", min, c, max);
		return false;
	}
}
