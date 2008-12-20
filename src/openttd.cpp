/* $Id$ */

/** @file openttd.cpp Functions related to starting OpenTTD. */

#include "stdafx.h"

#define VARDEF
#include "variables.h"
#undef VARDEF

#include "openttd.h"

#include "driver.h"
#include "blitter/factory.hpp"
#include "sound/sound_driver.hpp"
#include "music/music_driver.hpp"
#include "video/video_driver.hpp"

#include "fontcache.h"
#include "gfxinit.h"
#include "gfx_func.h"
#include "gui.h"
#include "mixer.h"
#include "sound_func.h"
#include "window_func.h"

#include "debug.h"
#include "saveload.h"
#include "landscape.h"
#include "company_func.h"
#include "company_base.h"
#include "command_func.h"
#include "town.h"
#include "industry.h"
#include "news_func.h"
#include "fileio_func.h"
#include "fios.h"
#include "airport.h"
#include "aircraft.h"
#include "console_func.h"
#include "screenshot.h"
#include "network/network.h"
#include "network/network_func.h"
#include "signs_base.h"
#include "signs_func.h"
#include "waypoint.h"
#include "ai/ai.h"
#include "train.h"
#include "yapf/yapf.h"
#include "settings_func.h"
#include "genworld.h"
#include "company_manager_face.h"
#include "group.h"
#include "strings_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "gamelog.h"
#include "cheat_func.h"
#include "animated_tile_func.h"
#include "functions.h"
#include "elrail_func.h"
#include "rev.h"

#include "newgrf.h"
#include "newgrf_config.h"
#include "newgrf_house.h"
#include "newgrf_commons.h"
#include "newgrf_station.h"

#include "clear_map.h"
#include "tree_map.h"
#include "rail_map.h"
#include "road_map.h"
#include "road_cmd.h"
#include "station_map.h"
#include "town_map.h"
#include "industry_map.h"
#include "unmovable_map.h"
#include "tunnel_map.h"
#include "bridge_map.h"
#include "water_map.h"
#include "tunnelbridge_map.h"
#include "void_map.h"
#include "water.h"

#include <stdarg.h>

#include "table/strings.h"

StringID _switch_mode_errorstr;

void CallLandscapeTick();
void IncreaseDate();
void DoPaletteAnimations();
void MusicLoop();
void ResetMusic();
void ResetOldNames();
void ProcessAsyncSaveFinish();
void CallWindowTickEvent();

extern void SetDifficultyLevel(int mode, DifficultySettings *gm_opt);
extern Company *DoStartupNewCompany(bool is_ai);
extern void ShowOSErrorBox(const char *buf, bool system);
extern void InitializeRailGUI();

/**
 * Error handling for fatal user errors.
 * @param s the string to print.
 * @note Does NEVER return.
 */
void CDECL usererror(const char *s, ...)
{
	va_list va;
	char buf[512];

	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);

	ShowOSErrorBox(buf, false);
	if (_video_driver != NULL) _video_driver->Stop();

	exit(1);
}

/**
 * Error handling for fatal non-user errors.
 * @param s the string to print.
 * @note Does NEVER return.
 */
void CDECL error(const char *s, ...)
{
	va_list va;
	char buf[512];

	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);

	ShowOSErrorBox(buf, true);
	if (_video_driver != NULL) _video_driver->Stop();

	assert(0);
	exit(1);
}

/**
 * Shows some information on the console/a popup box depending on the OS.
 * @param str the text to show.
 */
void CDECL ShowInfoF(const char *str, ...)
{
	va_list va;
	char buf[1024];
	va_start(va, str);
	vsnprintf(buf, lengthof(buf), str, va);
	va_end(va);
	ShowInfo(buf);
}

/**
 * Show the help message when someone passed a wrong parameter.
 */
static void ShowHelp()
{
	char buf[4096];
	char *p = buf;

	p += seprintf(p, lastof(buf), "OpenTTD %s\n", _openttd_revision);
	p = strecpy(p,
		"\n"
		"\n"
		"Command line options:\n"
		"  -v drv              = Set video driver (see below)\n"
		"  -s drv              = Set sound driver (see below) (param bufsize,hz)\n"
		"  -m drv              = Set music driver (see below)\n"
		"  -b drv              = Set the blitter to use (see below)\n"
		"  -r res              = Set resolution (for instance 800x600)\n"
		"  -h                  = Display this help text\n"
		"  -t year             = Set starting year\n"
		"  -d [[fac=]lvl[,...]]= Debug mode\n"
		"  -e                  = Start Editor\n"
		"  -g [savegame]       = Start new/save game immediately\n"
		"  -G seed             = Set random seed\n"
#if defined(ENABLE_NETWORK)
		"  -n [ip:port#company]= Start networkgame\n"
		"  -D [ip][:port]      = Start dedicated server\n"
		"  -l ip[:port]        = Redirect DEBUG()\n"
#if !defined(__MORPHOS__) && !defined(__AMIGA__) && !defined(WIN32)
		"  -f                  = Fork into the background (dedicated only)\n"
#endif
#endif /* ENABLE_NETWORK */
		"  -i palette          = Force to use the DOS (0) or Windows (1) palette\n"
		"                          (defines default setting when adding newgrfs)\n"
		"                        Default value (2) lets OpenTTD use the palette\n"
		"                          specified in graphics set file (see below)\n"
		"  -I graphics_set     = Force the graphics set (see below)\n"
		"  -c config_file      = Use 'config_file' instead of 'openttd.cfg'\n"
		"  -x                  = Do not automatically save to config file on exit\n"
		"\n",
		lastof(buf)
	);

	/* List the graphics packs */
	p = GetGraphicsSetsList(p, lastof(buf));

	/* List the drivers */
	p = VideoDriverFactoryBase::GetDriversInfo(p, lastof(buf));

	/* List the blitters */
	p = BlitterFactoryBase::GetBlittersInfo(p, lastof(buf));

	/* ShowInfo put output to stderr, but version information should go
	 * to stdout; this is the only exception */
#if !defined(WIN32) && !defined(WIN64)
	printf("%s\n", buf);
#else
	ShowInfo(buf);
#endif
}


struct MyGetOptData {
	char *opt;
	int numleft;
	char **argv;
	const char *options;
	const char *cont;

	MyGetOptData(int argc, char **argv, const char *options)
	{
		opt = NULL;
		numleft = argc;
		this->argv = argv;
		this->options = options;
		cont = NULL;
	}
};

static int MyGetOpt(MyGetOptData *md)
{
	const char *s,*r,*t;

	s = md->cont;
	if (s != NULL)
		goto md_continue_here;

	for (;;) {
		if (--md->numleft < 0) return -1;

		s = *md->argv++;
		if (*s == '-') {
md_continue_here:;
			s++;
			if (*s != 0) {
				/* Found argument, try to locate it in options. */
				if (*s == ':' || (r = strchr(md->options, *s)) == NULL) {
					/* ERROR! */
					return -2;
				}
				if (r[1] == ':') {
					/* Item wants an argument. Check if the argument follows, or if it comes as a separate arg. */
					if (!*(t = s + 1)) {
						/* It comes as a separate arg. Check if out of args? */
						if (--md->numleft < 0 || *(t = *md->argv) == '-') {
							/* Check if item is optional? */
							if (r[2] != ':')
								return -2;
							md->numleft++;
							t = NULL;
						} else {
							md->argv++;
						}
					}
					md->opt = (char*)t;
					md->cont = NULL;
					return *s;
				}
				md->opt = NULL;
				md->cont = s;
				return *s;
			}
		} else {
			/* This is currently not supported. */
			return -2;
		}
	}
}

/**
 * Extract the resolution from the given string and store
 * it in the 'res' parameter.
 * @param res variable to store the resolution in.
 * @param s   the string to decompose.
 */
static void ParseResolution(Dimension *res, const char *s)
{
	const char *t = strchr(s, 'x');
	if (t == NULL) {
		ShowInfoF("Invalid resolution '%s'", s);
		return;
	}

	res->width  = max(strtoul(s, NULL, 0), 64UL);
	res->height = max(strtoul(t + 1, NULL, 0), 64UL);
}

static void InitializeDynamicVariables()
{
	/* Dynamic stuff needs to be initialized somewhere... */
	_industry_mngr.ResetMapping();
	_industile_mngr.ResetMapping();
	_Company_pool.AddBlockToPool();
}


/** Unitializes drivers, frees allocated memory, cleans pools, ...
 * Generally, prepares the game for shutting down
 */
static void ShutdownGame()
{
	/* stop the AI */
	AI_Uninitialize();

	IConsoleFree();

	if (_network_available) NetworkShutDown(); // Shut down the network and close any open connections

	DriverFactoryBase::ShutdownDrivers();

	UnInitWindowSystem();

	/* Uninitialize airport state machines */
	UnInitializeAirports();

	/* Uninitialize variables that are allocated dynamically */
	GamelogReset();
	_Town_pool.CleanPool();
	_Industry_pool.CleanPool();
	_Station_pool.CleanPool();
	_Vehicle_pool.CleanPool();
	_Sign_pool.CleanPool();
	_Order_pool.CleanPool();
	_Group_pool.CleanPool();
	_CargoPacket_pool.CleanPool();
	_Engine_pool.CleanPool();
	_Company_pool.CleanPool();

	free(_config_file);

	/* Close all and any open filehandles */
	FioCloseAll();
}

static void LoadIntroGame()
{
	_game_mode = GM_MENU;

	ResetGRFConfig(false);

	/* Setup main window */
	ResetWindowSystem();
	SetupColorsAndInitialWindow();

	/* Load the default opening screen savegame */
	if (SaveOrLoad("opntitle.dat", SL_LOAD, DATA_DIR) != SL_OK) {
		GenerateWorld(GW_EMPTY, 64, 64); // if failed loading, make empty world.
		WaitTillGeneratedWorld();
		SetLocalCompany(COMPANY_SPECTATOR);
	} else {
		SetLocalCompany(COMPANY_FIRST);
	}

	_pause_game = 0;
	_cursor.fix_at = false;
	MarkWholeScreenDirty();

	CheckForMissingGlyphsInLoadedLanguagePack();

	/* Play main theme */
	if (_music_driver->IsSongPlaying()) ResetMusic();
}

byte _savegame_sort_order;
#if defined(UNIX) && !defined(__MORPHOS__)
extern void DedicatedFork();
#endif

int ttd_main(int argc, char *argv[])
{
	int i;
	const char *optformat;
	char musicdriver[32], sounddriver[32], videodriver[32], blitter[32], graphics_set[32];
	Dimension resolution = {0, 0};
	Year startyear = INVALID_YEAR;
	uint generation_seed = GENERATE_NEW_SEED;
	bool save_config = true;
#if defined(ENABLE_NETWORK)
	bool dedicated = false;
	bool network   = false;
	char *network_conn = NULL;
	char *debuglog_conn = NULL;
	char *dedicated_host = NULL;
	uint16 dedicated_port = 0;
#endif /* ENABLE_NETWORK */

	musicdriver[0] = sounddriver[0] = videodriver[0] = blitter[0] = graphics_set[0] = '\0';

	_game_mode = GM_MENU;
	_switch_mode = SM_MENU;
	_switch_mode_errorstr = INVALID_STRING_ID;
	_dedicated_forks = false;
	_config_file = NULL;

	/* The last param of the following function means this:
	 *   a letter means: it accepts that param (e.g.: -h)
	 *   a ':' behind it means: it need a param (e.g.: -m<driver>)
	 *   a '::' behind it means: it can optional have a param (e.g.: -d<debug>) */
	optformat = "m:s:v:b:hD::n::ei::I:t:d::r:g::G:c:xl:"
#if !defined(__MORPHOS__) && !defined(__AMIGA__) && !defined(WIN32)
		"f"
#endif
	;

	MyGetOptData mgo(argc - 1, argv + 1, optformat);

	while ((i = MyGetOpt(&mgo)) != -1) {
		switch (i) {
		case 'I': strecpy(graphics_set, mgo.opt, lastof(graphics_set)); break;
		case 'm': strecpy(musicdriver, mgo.opt, lastof(musicdriver)); break;
		case 's': strecpy(sounddriver, mgo.opt, lastof(sounddriver)); break;
		case 'v': strecpy(videodriver, mgo.opt, lastof(videodriver)); break;
		case 'b': strecpy(blitter, mgo.opt, lastof(blitter)); break;
#if defined(ENABLE_NETWORK)
		case 'D':
			strcpy(musicdriver, "null");
			strcpy(sounddriver, "null");
			strcpy(videodriver, "dedicated");
			strcpy(blitter, "null");
			dedicated = true;
			if (mgo.opt != NULL) {
				/* Use the existing method for parsing (openttd -n).
				 * However, we do ignore the #company part. */
				const char *temp = NULL;
				const char *port = NULL;
				ParseConnectionString(&temp, &port, mgo.opt);
				if (!StrEmpty(mgo.opt)) dedicated_host = mgo.opt;
				if (port != NULL) dedicated_port = atoi(port);
			}
			break;
		case 'f': _dedicated_forks = true; break;
		case 'n':
			network = true;
			network_conn = mgo.opt; // optional IP parameter, NULL if unset
			break;
		case 'l':
			debuglog_conn = mgo.opt;
			break;
#endif /* ENABLE_NETWORK */
		case 'r': ParseResolution(&resolution, mgo.opt); break;
		case 't': startyear = atoi(mgo.opt); break;
		case 'd': {
#if defined(WIN32)
				CreateConsole();
#endif
				if (mgo.opt != NULL) SetDebugString(mgo.opt);
			} break;
		case 'e': _switch_mode = SM_EDITOR; break;
		case 'i':
			/* there is an argument, it is not empty, and it is exactly 1 char long */
			if (!StrEmpty(mgo.opt) && mgo.opt[1] == '\0') {
				_use_palette = (PaletteType)(mgo.opt[0] - '0');
				if (_use_palette <= MAX_PAL) break;
			}
			usererror("Valid value for '-i' is 0, 1 or 2");
		case 'g':
			if (mgo.opt != NULL) {
				strecpy(_file_to_saveload.name, mgo.opt, lastof(_file_to_saveload.name));
				_switch_mode = SM_LOAD;
				_file_to_saveload.mode = SL_LOAD;

				/* if the file doesn't exist or it is not a valid savegame, let the saveload code show an error */
				const char *t = strrchr(_file_to_saveload.name, '.');
				if (t != NULL) {
					FiosType ft = FiosGetSavegameListCallback(SLD_LOAD_GAME, _file_to_saveload.name, t, NULL);
					if (ft != FIOS_TYPE_INVALID) SetFiosType(ft);
				}

				break;
			}

			_switch_mode = SM_NEWGAME;
			/* Give a random map if no seed has been given */
			if (generation_seed == GENERATE_NEW_SEED) {
				generation_seed = InteractiveRandom();
			}
			break;
		case 'G': generation_seed = atoi(mgo.opt); break;
		case 'c': _config_file = strdup(mgo.opt); break;
		case 'x': save_config = false; break;
		case -2:
		case 'h':
			/* The next two functions are needed to list the graphics sets.
			 * We can't do them earlier because then we can't show it on
			 * the debug console as that hasn't been configured yet. */
			DeterminePaths(argv[0]);
			FindGraphicsSets();
			ShowHelp();
			return 0;
		}
	}

#if defined(WINCE) && defined(_DEBUG)
	/* Switch on debug lvl 4 for WinCE if Debug release, as you can't give params, and you most likely do want this information */
	SetDebugString("4");
#endif

	DeterminePaths(argv[0]);
	FindGraphicsSets();

#if defined(UNIX) && !defined(__MORPHOS__)
	/* We must fork here, or we'll end up without some resources we need (like sockets) */
	if (_dedicated_forks)
		DedicatedFork();
#endif

	LoadFromConfig();
	CheckConfig();
	LoadFromHighScore();


	/* override config? */
	if (!StrEmpty(graphics_set)) strecpy(_ini_graphics_set, graphics_set, lastof(_ini_graphics_set));
	if (!StrEmpty(musicdriver)) strecpy(_ini_musicdriver, musicdriver, lastof(_ini_musicdriver));
	if (!StrEmpty(sounddriver)) strecpy(_ini_sounddriver, sounddriver, lastof(_ini_sounddriver));
	if (!StrEmpty(videodriver)) strecpy(_ini_videodriver, videodriver, lastof(_ini_videodriver));
	if (!StrEmpty(blitter))     strecpy(_ini_blitter, blitter, lastof(_ini_blitter));
	if (resolution.width != 0) { _cur_resolution = resolution; }
	if (startyear != INVALID_YEAR) _settings_newgame.game_creation.starting_year = startyear;
	if (generation_seed != GENERATE_NEW_SEED) _settings_newgame.game_creation.generation_seed = generation_seed;

	/* The width and height must be at least 1 pixel, this
	 * way all internal drawing routines work correctly. */
	if (_cur_resolution.width  <= 0) _cur_resolution.width  = 1;
	if (_cur_resolution.height <= 0) _cur_resolution.height = 1;

#if defined(ENABLE_NETWORK)
	if (dedicated_host) snprintf(_settings_client.network.server_bind_ip, sizeof(_settings_client.network.server_bind_ip), "%s", dedicated_host);
	if (dedicated_port) _settings_client.network.server_port = dedicated_port;
	if (_dedicated_forks && !dedicated) _dedicated_forks = false;
#endif /* ENABLE_NETWORK */

	/* enumerate language files */
	InitializeLanguagePacks();

	/* initialize screenshot formats */
	InitializeScreenshotFormats();

	/* initialize airport state machines */
	InitializeAirports();

	/* initialize all variables that are allocated dynamically */
	InitializeDynamicVariables();

	/* Sample catalogue */
	DEBUG(misc, 1, "Loading sound effects...");
	MxInitialize(11025);
	SoundInitialize("sample.cat");

	/* Initialize FreeType */
	InitFreeType();

	/* This must be done early, since functions use the InvalidateWindow* calls */
	InitWindowSystem();

	if (!SetGraphicsSet(_ini_graphics_set)) {
		StrEmpty(_ini_graphics_set) ?
			usererror("Failed to find a graphics set. Please acquire a graphics set for OpenTTD.") :
			usererror("Failed to select requested graphics set '%s'", _ini_graphics_set);
	}

	/* Initialize game palette */
	GfxInitPalettes();

	DEBUG(misc, 1, "Loading blitter...");
	if (BlitterFactoryBase::SelectBlitter(_ini_blitter) == NULL)
		StrEmpty(_ini_blitter) ?
			usererror("Failed to autoprobe blitter") :
			usererror("Failed to select requested blitter '%s'; does it exist?", _ini_blitter);

	DEBUG(driver, 1, "Loading drivers...");

	_sound_driver = (SoundDriver*)SoundDriverFactoryBase::SelectDriver(_ini_sounddriver, Driver::DT_SOUND);
	if (_sound_driver == NULL) {
		StrEmpty(_ini_sounddriver) ?
			usererror("Failed to autoprobe sound driver") :
			usererror("Failed to select requested sound driver '%s'", _ini_sounddriver);
	}

	_music_driver = (MusicDriver*)MusicDriverFactoryBase::SelectDriver(_ini_musicdriver, Driver::DT_MUSIC);
	if (_music_driver == NULL) {
		StrEmpty(_ini_musicdriver) ?
			usererror("Failed to autoprobe music driver") :
			usererror("Failed to select requested music driver '%s'", _ini_musicdriver);
	}

	_video_driver = (VideoDriver*)VideoDriverFactoryBase::SelectDriver(_ini_videodriver, Driver::DT_VIDEO);
	if (_video_driver == NULL) {
		StrEmpty(_ini_videodriver) ?
			usererror("Failed to autoprobe video driver") :
			usererror("Failed to select requested video driver '%s'", _ini_videodriver);
	}

	_savegame_sort_order = SORT_BY_DATE | SORT_DESCENDING;
	/* Initialize the zoom level of the screen to normal */
	_screen.zoom = ZOOM_LVL_NORMAL;

	/* restore saved music volume */
	_music_driver->SetVolume(msf.music_vol);

	NetworkStartUp(); // initialize network-core

#if defined(ENABLE_NETWORK)
	if (debuglog_conn != NULL && _network_available) {
		const char *not_used = NULL;
		const char *port = NULL;
		uint16 rport;

		rport = NETWORK_DEFAULT_DEBUGLOG_PORT;

		ParseConnectionString(&not_used, &port, debuglog_conn);
		if (port != NULL) rport = atoi(port);

		NetworkStartDebugLog(debuglog_conn, rport);
	}
#endif /* ENABLE_NETWORK */

	ScanNewGRFFiles();

	ResetGRFConfig(false);

	/* XXX - ugly hack, if diff_level is 9, it means we got no setting from the config file */
	if (_settings_newgame.difficulty.diff_level == 9) SetDifficultyLevel(0, &_settings_newgame.difficulty);

	/* Make sure _settings is filled with _settings_newgame if we switch to a game directly */
	if (_switch_mode != SM_NONE) _settings_game = _settings_newgame;

	/* initialize the ingame console */
	IConsoleInit();
	_cursor.in_window = true;
	InitializeGUI();
	IConsoleCmdExec("exec scripts/autoexec.scr 0");

	GenerateWorld(GW_EMPTY, 64, 64); // Make the viewport initialization happy
	WaitTillGeneratedWorld();

#ifdef ENABLE_NETWORK
	if (network && _network_available) {
		if (network_conn != NULL) {
			const char *port = NULL;
			const char *company = NULL;
			uint16 rport;

			rport = NETWORK_DEFAULT_PORT;
			_network_playas = COMPANY_NEW_COMPANY;

			ParseConnectionString(&company, &port, network_conn);

			if (company != NULL) {
				_network_playas = (CompanyID)atoi(company);

				if (_network_playas != COMPANY_SPECTATOR) {
					_network_playas--;
					if (_network_playas >= MAX_COMPANIES) return false;
				}
			}
			if (port != NULL) rport = atoi(port);

			LoadIntroGame();
			_switch_mode = SM_NONE;
			NetworkClientConnectGame(network_conn, rport);
		}
	}
#endif /* ENABLE_NETWORK */

	_video_driver->MainLoop();

	WaitTillSaved();

	/* only save config if we have to */
	if (save_config) {
		SaveToConfig();
		SaveToHighScore();
	}

	/* Reset windowing system, stop drivers, free used memory, ... */
	ShutdownGame();

	return 0;
}

void HandleExitGameRequest()
{
	if (_game_mode == GM_MENU) { // do not ask to quit on the main screen
		_exit_game = true;
	} else if (_settings_client.gui.autosave_on_exit) {
		DoExitSave();
		_exit_game = true;
	} else {
		AskExitGame();
	}
}

static void ShowScreenshotResult(bool b)
{
	if (b) {
		SetDParamStr(0, _screenshot_name);
		ShowErrorMessage(INVALID_STRING_ID, STR_031B_SCREENSHOT_SUCCESSFULLY, 0, 0);
	} else {
		ShowErrorMessage(INVALID_STRING_ID, STR_031C_SCREENSHOT_FAILED, 0, 0);
	}

}

static void MakeNewGameDone()
{
	SettingsDisableElrail(_settings_game.vehicle.disable_elrails);

	/* In a dedicated server, the server does not play */
	if (_network_dedicated) {
		SetLocalCompany(COMPANY_SPECTATOR);
		return;
	}

	/* Create a single company */
	DoStartupNewCompany(false);

	SetLocalCompany(COMPANY_FIRST);
	_current_company = _local_company;
	DoCommandP(0, (_settings_client.gui.autorenew << 15 ) | (_settings_client.gui.autorenew_months << 16) | 4, _settings_client.gui.autorenew_money, NULL, CMD_SET_AUTOREPLACE);

	InitializeRailGUI();

#ifdef ENABLE_NETWORK
	/* We are the server, we start a new company (not dedicated),
	 * so set the default password *if* needed. */
	if (_network_server && !StrEmpty(_settings_client.network.default_company_pass)) {
		char *password = _settings_client.network.default_company_pass;
		NetworkChangeCompanyPassword(1, &password);
	}
#endif /* ENABLE_NETWORK */

	MarkWholeScreenDirty();
}

static void MakeNewGame(bool from_heightmap)
{
	_game_mode = GM_NORMAL;

	ResetGRFConfig(true);
	_house_mngr.ResetMapping();
	_industile_mngr.ResetMapping();
	_industry_mngr.ResetMapping();

	GenerateWorldSetCallback(&MakeNewGameDone);
	GenerateWorld(from_heightmap ? GW_HEIGHTMAP : GW_NEWGAME, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
}

static void MakeNewEditorWorldDone()
{
	SetLocalCompany(OWNER_NONE);

	MarkWholeScreenDirty();
}

static void MakeNewEditorWorld()
{
	_game_mode = GM_EDITOR;

	ResetGRFConfig(true);

	GenerateWorldSetCallback(&MakeNewEditorWorldDone);
	GenerateWorld(GW_EMPTY, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
}

void StartupCompanies();
void StartupDisasters();
extern void StartupEconomy();

/**
 * Start Scenario starts a new game based on a scenario.
 * Eg 'New Game' --> select a preset scenario
 * This starts a scenario based on your current difficulty settings
 */
static void StartScenario()
{
	_game_mode = GM_NORMAL;

	/* invalid type */
	if (_file_to_saveload.mode == SL_INVALID) {
		DEBUG(sl, 0, "Savegame is obsolete or invalid format: '%s'", _file_to_saveload.name);
		SetDParam(0, STR_JUST_RAW_STRING);
		SetDParamStr(1, GetSaveLoadErrorString());
		ShowErrorMessage(INVALID_STRING_ID, STR_012D, 0, 0);
		_game_mode = GM_MENU;
		return;
	}

	/* Reinitialize windows */
	ResetWindowSystem();

	SetupColorsAndInitialWindow();

	ResetGRFConfig(true);

	/* Load game */
	if (SaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode, SCENARIO_DIR) != SL_OK) {
		LoadIntroGame();
		SetDParam(0, STR_JUST_RAW_STRING);
		SetDParamStr(1, GetSaveLoadErrorString());
		ShowErrorMessage(INVALID_STRING_ID, STR_012D, 0, 0);
	}

	_settings_game.difficulty = _settings_newgame.difficulty;

	/* Inititalize data */
	StartupEconomy();
	StartupCompanies();
	StartupEngines();
	StartupDisasters();

	SetLocalCompany(COMPANY_FIRST);
	_current_company = _local_company;
	DoCommandP(0, (_settings_client.gui.autorenew << 15 ) | (_settings_client.gui.autorenew_months << 16) | 4, _settings_client.gui.autorenew_money, NULL, CMD_SET_AUTOREPLACE);

	MarkWholeScreenDirty();
}

/** Load the specified savegame but on error do different things.
 * If loading fails due to corrupt savegame, bad version, etc. go back to
 * a previous correct state. In the menu for example load the intro game again.
 * @param filename file to be loaded
 * @param mode mode of loading, either SL_LOAD or SL_OLD_LOAD
 * @param newgm switch to this mode of loading fails due to some unknown error
 * @param subdir default directory to look for filename, set to 0 if not needed
 */
bool SafeSaveOrLoad(const char *filename, int mode, int newgm, Subdirectory subdir)
{
	byte ogm = _game_mode;

	_game_mode = newgm;
	assert(mode == SL_LOAD || mode == SL_OLD_LOAD);
	switch (SaveOrLoad(filename, mode, subdir)) {
		case SL_OK: return true;

		case SL_REINIT:
			switch (ogm) {
				default:
				case GM_MENU:   LoadIntroGame();      break;
				case GM_EDITOR: MakeNewEditorWorld(); break;
			}
			return false;

		default:
			_game_mode = ogm;
			return false;
	}
}

void SwitchMode(int new_mode)
{
#ifdef ENABLE_NETWORK
	/* If we are saving something, the network stays in his current state */
	if (new_mode != SM_SAVE) {
		/* If the network is active, make it not-active */
		if (_networking) {
			if (_network_server && (new_mode == SM_LOAD || new_mode == SM_NEWGAME)) {
				NetworkReboot();
			} else {
				NetworkDisconnect();
			}
		}

		/* If we are a server, we restart the server */
		if (_is_network_server) {
			/* But not if we are going to the menu */
			if (new_mode != SM_MENU) {
				/* check if we should reload the config */
				if (_settings_client.network.reload_cfg) {
					LoadFromConfig();
					_settings_game = _settings_newgame;
					ResetGRFConfig(false);
				}
				NetworkServerStart();
			} else {
				/* This client no longer wants to be a network-server */
				_is_network_server = false;
			}
		}
	}
#endif /* ENABLE_NETWORK */

	switch (new_mode) {
		case SM_EDITOR: /* Switch to scenario editor */
			MakeNewEditorWorld();
			break;

		case SM_NEWGAME: /* New Game --> 'Random game' */
#ifdef ENABLE_NETWORK
			if (_network_server) {
				snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "Random Map");
			}
#endif /* ENABLE_NETWORK */
			MakeNewGame(false);
			break;

		case SM_START_SCENARIO: /* New Game --> Choose one of the preset scenarios */
#ifdef ENABLE_NETWORK
			if (_network_server) {
				snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "%s (Loaded scenario)", _file_to_saveload.title);
			}
#endif /* ENABLE_NETWORK */
			StartScenario();
			break;

		case SM_LOAD: { /* Load game, Play Scenario */
			ResetGRFConfig(true);
			ResetWindowSystem();

			if (!SafeSaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_NORMAL, NO_DIRECTORY)) {
				LoadIntroGame();
				SetDParam(0, STR_JUST_RAW_STRING);
				SetDParamStr(1, GetSaveLoadErrorString());
				ShowErrorMessage(INVALID_STRING_ID, STR_012D, 0, 0);
			} else {
				if (_saveload_mode == SLD_LOAD_SCENARIO) {
					StartupEngines();
				}
				/* Update the local company for a loaded game. It is either always
				* company #1 (eg 0) or in the case of a dedicated server a spectator */
				SetLocalCompany(_network_dedicated ? COMPANY_SPECTATOR : COMPANY_FIRST);
				/* Decrease pause counter (was increased from opening load dialog) */
				DoCommandP(0, 0, 0, NULL, CMD_PAUSE);
#ifdef ENABLE_NETWORK
				if (_network_server) {
					snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "%s (Loaded game)", _file_to_saveload.title);
				}
#endif /* ENABLE_NETWORK */
			}
			break;
		}

		case SM_START_HEIGHTMAP: /* Load a heightmap and start a new game from it */
#ifdef ENABLE_NETWORK
			if (_network_server) {
				snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "%s (Heightmap)", _file_to_saveload.title);
			}
#endif /* ENABLE_NETWORK */
			MakeNewGame(true);
			break;

		case SM_LOAD_HEIGHTMAP: /* Load heightmap from scenario editor */
			SetLocalCompany(OWNER_NONE);

			GenerateWorld(GW_HEIGHTMAP, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
			MarkWholeScreenDirty();
			break;

		case SM_LOAD_SCENARIO: { /* Load scenario from scenario editor */
			if (SafeSaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_EDITOR, NO_DIRECTORY)) {
				SetLocalCompany(OWNER_NONE);
				_settings_newgame.game_creation.starting_year = _cur_year;
			} else {
				SetDParam(0, STR_JUST_RAW_STRING);
				SetDParamStr(1, GetSaveLoadErrorString());
				ShowErrorMessage(INVALID_STRING_ID, STR_012D, 0, 0);
			}
			break;
		}

		case SM_MENU: /* Switch to game intro menu */
			LoadIntroGame();
			break;

		case SM_SAVE: /* Save game */
			/* Make network saved games on pause compatible to singleplayer */
			if (_networking && _pause_game == 1) _pause_game = 2;
			if (SaveOrLoad(_file_to_saveload.name, SL_SAVE, NO_DIRECTORY) != SL_OK) {
				SetDParam(0, STR_JUST_RAW_STRING);
				SetDParamStr(1, GetSaveLoadErrorString());
				ShowErrorMessage(INVALID_STRING_ID, STR_012D, 0, 0);
			} else {
				DeleteWindowById(WC_SAVELOAD, 0);
			}
			if (_networking && _pause_game == 2) _pause_game = 1;
			break;

		case SM_GENRANDLAND: /* Generate random land within scenario editor */
			SetLocalCompany(OWNER_NONE);
			GenerateWorld(GW_RANDOM, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
			/* XXX: set date */
			MarkWholeScreenDirty();
			break;
	}

	if (_switch_mode_errorstr != INVALID_STRING_ID) {
		ShowErrorMessage(INVALID_STRING_ID, _switch_mode_errorstr, 0, 0);
	}
}


/**
 * State controlling game loop.
 * The state must not be changed from anywhere but here.
 * That check is enforced in DoCommand.
 */
void StateGameLoop()
{
	/* dont execute the state loop during pause */
	if (_pause_game) {
		CallWindowTickEvent();
		return;
	}
	if (IsGeneratingWorld()) return;

	ClearStorageChanges(false);

	if (_game_mode == GM_EDITOR) {
		RunTileLoop();
		CallVehicleTicks();
		CallLandscapeTick();
		ClearStorageChanges(true);

		CallWindowTickEvent();
		NewsLoop();
	} else {
#ifdef DEBUG_DUMP_COMMANDS
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v != v->First()) continue;

			switch (v->type) {
				case VEH_ROAD: {
					extern byte GetRoadVehLength(const Vehicle *v);
					if (GetRoadVehLength(v) != v->u.road.cached_veh_length) {
						printf("cache mismatch: vehicle %i, company %i, unit number %i\n", v->index, (int)v->owner, v->unitnumber);
					}
				} break;

				case VEH_TRAIN: {
					uint length = 0;
					for (Vehicle *u = v; u != NULL; u = u->Next()) length++;

					VehicleRail *wagons = MallocT<VehicleRail>(length);
					length = 0;
					for (Vehicle *u = v; u != NULL; u = u->Next()) wagons[length++] = u->u.rail;

					TrainConsistChanged(v, true);

					length = 0;
					for (Vehicle *u = v; u != NULL; u = u->Next()) {
						if (memcmp(&wagons[length], &u->u.rail, sizeof(VehicleRail)) != 0) {
							printf("cache mismatch: vehicle %i, company %i, unit number %i, wagon %i\n", v->index, (int)v->owner, v->unitnumber, length);
						}
						length++;
					}

					free(wagons);
				} break;

				case VEH_AIRCRAFT: {
					uint speed = v->u.air.cached_max_speed;
					UpdateAircraftCache(v);
					if (speed != v->u.air.cached_max_speed) {
						printf("cache mismatch: vehicle %i, company %i, unit number %i\n", v->index, (int)v->owner, v->unitnumber);
					}
				} break;

				default:
					break;
			}
		}
#endif

		/* All these actions has to be done from OWNER_NONE
		 *  for multiplayer compatibility */
		CompanyID old_company = _current_company;
		_current_company = OWNER_NONE;

		AnimateAnimatedTiles();
		IncreaseDate();
		RunTileLoop();
		CallVehicleTicks();
		CallLandscapeTick();
		ClearStorageChanges(true);

		AI_RunGameLoop();

		CallWindowTickEvent();
		NewsLoop();
		_current_company = old_company;
	}
}

/** Create an autosave. The default name is "autosave#.sav". However with
 * the patch setting 'keep_all_autosave' the name defaults to company-name + date */
static void DoAutosave()
{
	char buf[MAX_PATH];

#if defined(PSP)
	/* Autosaving in networking is too time expensive for the PSP */
	if (_networking) return;
#endif /* PSP */

	if (_settings_client.gui.keep_all_autosave && _local_company != COMPANY_SPECTATOR) {
		SetDParam(0, _local_company);
		SetDParam(1, _date);
		GetString(buf, STR_4004, lastof(buf));
		strecat(buf, ".sav", lastof(buf));
	} else {
		/* generate a savegame name and number according to _settings_client.gui.max_num_autosaves */
		snprintf(buf, sizeof(buf), "autosave%d.sav", _autosave_ctr);

		if (++_autosave_ctr >= _settings_client.gui.max_num_autosaves) _autosave_ctr = 0;
	}

	DEBUG(sl, 2, "Autosaving to '%s'", buf);
	if (SaveOrLoad(buf, SL_SAVE, AUTOSAVE_DIR) != SL_OK) {
		ShowErrorMessage(INVALID_STRING_ID, STR_AUTOSAVE_FAILED, 0, 0);
	}
}

void GameLoop()
{
	ProcessAsyncSaveFinish();

	/* autosave game? */
	if (_do_autosave) {
		_do_autosave = false;
		DoAutosave();
		RedrawAutosave();
	}

	/* make a screenshot? */
	if (IsScreenshotRequested()) ShowScreenshotResult(MakeScreenshot());

	/* switch game mode? */
	if (_switch_mode != SM_NONE) {
		SwitchMode(_switch_mode);
		_switch_mode = SM_NONE;
	}

	IncreaseSpriteLRU();
	InteractiveRandom();

	_caret_timer += 3;
	_palette_animation_counter += 8;
	CursorTick();

#ifdef ENABLE_NETWORK
	/* Check for UDP stuff */
	if (_network_available) NetworkUDPGameLoop();

	if (_networking && !IsGeneratingWorld()) {
		/* Multiplayer */
		NetworkGameLoop();
	} else {
		if (_network_reconnect > 0 && --_network_reconnect == 0) {
			/* This means that we want to reconnect to the last host
			 * We do this here, because it means that the network is really closed */
			NetworkClientConnectGame(_settings_client.network.last_host, _settings_client.network.last_port);
		}
		/* Singleplayer */
		StateGameLoop();
	}
#else
	StateGameLoop();
#endif /* ENABLE_NETWORK */

	if (!_pause_game && HasBit(_display_opt, DO_FULL_ANIMATION)) DoPaletteAnimations();

	if (!_pause_game || _cheats.build_in_pause.value) MoveAllTextEffects();

	InputLoop();

	_sound_driver->MainLoop();
	MusicLoop();
}

static void ConvertTownOwner()
{
	for (TileIndex tile = 0; tile != MapSize(); tile++) {
		switch (GetTileType(tile)) {
			case MP_ROAD:
				if (GB(_m[tile].m5, 4, 2) == ROAD_TILE_CROSSING && HasBit(_m[tile].m3, 7)) {
					_m[tile].m3 = OWNER_TOWN;
				}
				/* FALLTHROUGH */

			case MP_TUNNELBRIDGE:
				if (GetTileOwner(tile) & 0x80) SetTileOwner(tile, OWNER_TOWN);
				break;

			default: break;
		}
	}
}

/* since savegame version 4.1, exclusive transport rights are stored at towns */
static void UpdateExclusiveRights()
{
	Town *t;

	FOR_ALL_TOWNS(t) {
		t->exclusivity = INVALID_COMPANY;
	}

	/* FIXME old exclusive rights status is not being imported (stored in s->blocked_months_obsolete)
	 *   could be implemented this way:
	 * 1.) Go through all stations
	 *     Build an array town_blocked[ town_id ][ company_id ]
	 *     that stores if at least one station in that town is blocked for a company
	 * 2.) Go through that array, if you find a town that is not blocked for
	 *     one company, but for all others, then give him exclusivity.
	 */
}

static const byte convert_currency[] = {
	 0,  1, 12,  8,  3,
	10, 14, 19,  4,  5,
	 9, 11, 13,  6, 17,
	16, 22, 21,  7, 15,
	18,  2, 20, };

/* since savegame version 4.2 the currencies are arranged differently */
static void UpdateCurrencies()
{
	_settings_game.locale.currency = convert_currency[_settings_game.locale.currency];
}

/* Up to revision 1413 the invisible tiles at the southern border have not been
 * MP_VOID, even though they should have. This is fixed by this function
 */
static void UpdateVoidTiles()
{
	uint i;

	for (i = 0; i < MapMaxY(); ++i) MakeVoid(i * MapSizeX() + MapMaxX());
	for (i = 0; i < MapSizeX(); ++i) MakeVoid(MapSizeX() * MapMaxY() + i);
}

/* since savegame version 6.0 each sign has an "owner", signs without owner (from old games are set to 255) */
static void UpdateSignOwner()
{
	Sign *si;

	FOR_ALL_SIGNS(si) si->owner = OWNER_NONE;
}

extern void UpdateOldAircraft();


static inline RailType UpdateRailType(RailType rt, RailType min)
{
	return rt >= min ? (RailType)(rt + 1): rt;
}

/**
 * Initialization of the windows and several kinds of caches.
 * This is not done directly in AfterLoadGame because these
 * functions require that all saveload conversions have been
 * done. As people tend to add savegame conversion stuff after
 * the intialization of the windows and caches quite some bugs
 * had been made.
 * Moving this out of there is both cleaner and less bug-prone.
 *
 * @return true if everything went according to plan, otherwise false.
 */
static bool InitializeWindowsAndCaches()
{
	/* Initialize windows */
	ResetWindowSystem();
	SetupColorsAndInitialWindow();

	extern void ResetViewportAfterLoadGame();
	ResetViewportAfterLoadGame();

	/* Update coordinates of the signs. */
	UpdateAllStationVirtCoord();
	UpdateAllSignVirtCoords();
	UpdateAllTownVirtCoords();
	UpdateAllWaypointSigns();

	Company *c;
	FOR_ALL_COMPANIES(c) {
		/* For each company, verify (while loading a scenario) that the inauguration date is the current year and set it
		 * accordingly if it is not the case.  No need to set it on companies that are not been used already,
		 * thus the MIN_YEAR (which is really nothing more than Zero, initialized value) test */
		if (_file_to_saveload.filetype == FT_SCENARIO && c->inaugurated_year != MIN_YEAR) {
			c->inaugurated_year = _cur_year;
		}
	}

	SetCachedEngineCounts();

	/* Towns have a noise controlled number of airports system
	 * So each airport's noise value must be added to the town->noise_reached value
	 * Reset each town's noise_reached value to '0' before. */
	UpdateAirportsNoise();

	CheckTrainsLengths();

	return true;
}

bool AfterLoadGame()
{
	TileIndex map_size = MapSize();
	Company *c;

	if (CheckSavegameVersion(98)) GamelogOldver();

	GamelogTestRevision();
	GamelogTestMode();

	if (CheckSavegameVersion(98)) GamelogGRFAddList(_grfconfig);

	/* in very old versions, size of train stations was stored differently */
	if (CheckSavegameVersion(2)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->train_tile != 0 && st->trainst_h == 0) {
				extern SavegameType _savegame_type;
				uint n = _savegame_type == SGT_OTTD ? 4 : 3; // OTTD uses 4 bits per dimensions, TTD 3 bits
				uint w = GB(st->trainst_w, n, n);
				uint h = GB(st->trainst_w, 0, n);

				if (GetRailStationAxis(st->train_tile) != AXIS_X) Swap(w, h);

				st->trainst_w = w;
				st->trainst_h = h;

				assert(GetStationIndex(st->train_tile + TileDiffXY(w - 1, h - 1)) == st->index);
			}
		}
	}

	/* in version 2.1 of the savegame, town owner was unified. */
	if (CheckSavegameVersionOldStyle(2, 1)) ConvertTownOwner();

	/* from version 4.1 of the savegame, exclusive rights are stored at towns */
	if (CheckSavegameVersionOldStyle(4, 1)) UpdateExclusiveRights();

	/* from version 4.2 of the savegame, currencies are in a different order */
	if (CheckSavegameVersionOldStyle(4, 2)) UpdateCurrencies();

	/* from version 6.1 of the savegame, signs have an "owner" */
	if (CheckSavegameVersionOldStyle(6, 1)) UpdateSignOwner();

	/* In old version there seems to be a problem that water is owned by
	 * OWNER_NONE, not OWNER_WATER.. I can't replicate it for the current
	 * (4.3) version, so I just check when versions are older, and then
	 * walk through the whole map.. */
	if (CheckSavegameVersionOldStyle(4, 3)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_WATER) && GetTileOwner(t) >= MAX_COMPANIES) {
				SetTileOwner(t, OWNER_WATER);
			}
		}
	}

	if (CheckSavegameVersion(84)) {
		FOR_ALL_COMPANIES(c) {
			c->name = CopyFromOldName(c->name_1);
			if (c->name != NULL) c->name_1 = STR_SV_UNNAMED;
			c->president_name = CopyFromOldName(c->president_name_1);
			if (c->president_name != NULL) c->president_name_1 = SPECSTR_PRESIDENT_NAME;
		}

		Station *st;
		FOR_ALL_STATIONS(st) {
			st->name = CopyFromOldName(st->string_id);
			/* generating new name would be too much work for little effect, use the station name fallback */
			if (st->name != NULL) st->string_id = STR_SV_STNAME_FALLBACK;
		}

		Town *t;
		FOR_ALL_TOWNS(t) {
			t->name = CopyFromOldName(t->townnametype);
			if (t->name != NULL) t->townnametype = SPECSTR_TOWNNAME_START + _settings_game.game_creation.town_name;
		}

		Waypoint *wp;
		FOR_ALL_WAYPOINTS(wp) {
			wp->name = CopyFromOldName(wp->string);
			wp->string = STR_EMPTY;
		}

		for (uint i = 0; i < GetSignPoolSize(); i++) {
			/* invalid signs are determined by si->ower == INVALID_COMPANY now */
			Sign *si = GetSign(i);
			if (!si->IsValid() && si->name != NULL) {
				si->owner = OWNER_NONE;
			}
		}
	}

	/* From this point the old names array is cleared. */
	ResetOldNames();

	/* convert road side to my format. */
	if (_settings_game.vehicle.road_side) _settings_game.vehicle.road_side = 1;

	/* Check if all NewGRFs are present, we are very strict in MP mode */
	GRFListCompatibility gcf_res = IsGoodGRFConfigList();
	if (_networking && gcf_res != GLC_ALL_GOOD) {
		SetSaveLoadError(STR_NETWORK_ERR_CLIENT_NEWGRF_MISMATCH);
		return false;
	}

	switch (gcf_res) {
		case GLC_COMPATIBLE: _switch_mode_errorstr = STR_NEWGRF_COMPATIBLE_LOAD_WARNING; break;
		case GLC_NOT_FOUND: _switch_mode_errorstr = STR_NEWGRF_DISABLED_WARNING; _pause_game = -1; break;
		default: break;
	}

	/* Update current year
	 * must be done before loading sprites as some newgrfs check it */
	SetDate(_date);

	/* Force dynamic engines off when loading older savegames */
	if (CheckSavegameVersion(95)) _settings_game.vehicle.dynamic_engines = 0;

	/* Load the sprites */
	GfxLoadSprites();
	LoadStringWidthTable();

	/* Copy temporary data to Engine pool */
	CopyTempEngineData();

	/* Connect front and rear engines of multiheaded trains and converts
	 * subtype to the new format */
	if (CheckSavegameVersionOldStyle(17, 1)) ConvertOldMultiheadToNew();

	/* Connect front and rear engines of multiheaded trains */
	ConnectMultiheadedTrains();

	/* reinit the landscape variables (landscape might have changed) */
	InitializeLandscapeVariables(true);

	/* Update all vehicles */
	AfterLoadVehicles(true);

	/* Update all waypoints */
	if (CheckSavegameVersion(12)) FixOldWaypoints();

	/* in version 2.2 of the savegame, we have new airports */
	if (CheckSavegameVersionOldStyle(2, 2)) UpdateOldAircraft();

	AfterLoadTown();

	/* make sure there is a town in the game */
	if (_game_mode == GM_NORMAL && !ClosestTownFromTile(0, UINT_MAX)) {
		SetSaveLoadError(STR_NO_TOWN_IN_SCENARIO);
		return false;
	}

	/* The void tiles on the southern border used to belong to a wrong class (pre 4.3).
	 * This problem appears in savegame version 21 too, see r3455. But after loading the
	 * savegame and saving again, the buggy map array could be converted to new savegame
	 * version. It didn't show up before r12070. */
	if (CheckSavegameVersion(87)) UpdateVoidTiles();

	/* If Load Scenario / New (Scenario) Game is used,
	 *  a company does not exist yet. So create one here.
	 * 1 exeption: network-games. Those can have 0 companies
	 *   But this exeption is not true for non dedicated network_servers! */
	if (!IsValidCompanyID(COMPANY_FIRST) && (!_networking || (_networking && _network_server && !_network_dedicated)))
		DoStartupNewCompany(false);

	if (CheckSavegameVersion(72)) {
		/* Locks/shiplifts in very old savegames had OWNER_WATER as owner */
		for (TileIndex t = 0; t < MapSize(); t++) {
			switch (GetTileType(t)) {
				default: break;

				case MP_WATER:
					if (GetWaterTileType(t) == WATER_TILE_LOCK && GetTileOwner(t) == OWNER_WATER) SetTileOwner(t, OWNER_NONE);
					break;

				case MP_STATION: {
					if (HasBit(_m[t].m6, 3)) SetBit(_m[t].m6, 2);
					StationGfx gfx = GetStationGfx(t);
					StationType st;
					if (       IsInsideMM(gfx,   0,   8)) { // Railway station
						st = STATION_RAIL;
						SetStationGfx(t, gfx - 0);
					} else if (IsInsideMM(gfx,   8,  67)) { // Airport
						st = STATION_AIRPORT;
						SetStationGfx(t, gfx - 8);
					} else if (IsInsideMM(gfx,  67,  71)) { // Truck
						st = STATION_TRUCK;
						SetStationGfx(t, gfx - 67);
					} else if (IsInsideMM(gfx,  71,  75)) { // Bus
						st = STATION_BUS;
						SetStationGfx(t, gfx - 71);
					} else if (gfx == 75) {                    // Oil rig
						st = STATION_OILRIG;
						SetStationGfx(t, gfx - 75);
					} else if (IsInsideMM(gfx,  76,  82)) { // Dock
						st = STATION_DOCK;
						SetStationGfx(t, gfx - 76);
					} else if (gfx == 82) {                    // Buoy
						st = STATION_BUOY;
						SetStationGfx(t, gfx - 82);
					} else if (IsInsideMM(gfx,  83, 168)) { // Extended airport
						st = STATION_AIRPORT;
						SetStationGfx(t, gfx - 83 + 67 - 8);
					} else if (IsInsideMM(gfx, 168, 170)) { // Drive through truck
						st = STATION_TRUCK;
						SetStationGfx(t, gfx - 168 + GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET);
					} else if (IsInsideMM(gfx, 170, 172)) { // Drive through bus
						st = STATION_BUS;
						SetStationGfx(t, gfx - 170 + GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET);
					} else {
						return false;
					}
					SB(_m[t].m6, 3, 3, st);
				} break;
			}
		}
	}

	for (TileIndex t = 0; t < map_size; t++) {
		switch (GetTileType(t)) {
			case MP_STATION: {
				Station *st = GetStationByTile(t);

				/* Set up station spread; buoys do not have one */
				if (!IsBuoy(t)) st->rect.BeforeAddTile(t, StationRect::ADD_FORCE);

				switch (GetStationType(t)) {
					case STATION_TRUCK:
					case STATION_BUS:
						if (CheckSavegameVersion(6)) {
							/* From this version on there can be multiple road stops of the
							 * same type per station. Convert the existing stops to the new
							 * internal data structure. */
							RoadStop *rs = new RoadStop(t);
							if (rs == NULL) error("Too many road stops in savegame");

							RoadStop **head =
								IsTruckStop(t) ? &st->truck_stops : &st->bus_stops;
							*head = rs;
						}
						break;

					case STATION_OILRIG: {
						/* Very old savegames sometimes have phantom oil rigs, i.e.
						 * an oil rig which got shut down, but not completly removed from
						 * the map
						 */
						TileIndex t1 = TILE_ADDXY(t, 0, 1);
						if (IsTileType(t1, MP_INDUSTRY) &&
								GetIndustryGfx(t1) == GFX_OILRIG_1) {
							/* The internal encoding of oil rigs was changed twice.
							 * It was 3 (till 2.2) and later 5 (till 5.1).
							 * Setting it unconditionally does not hurt.
							 */
							GetStationByTile(t)->airport_type = AT_OILRIG;
						} else {
							DeleteOilRig(t);
						}
						break;
					}

					default: break;
				}
				break;
			}

			default: break;
		}
	}

	/* In version 6.1 we put the town index in the map-array. To do this, we need
	 *  to use m2 (16bit big), so we need to clean m2, and that is where this is
	 *  all about ;) */
	if (CheckSavegameVersionOldStyle(6, 1)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_HOUSE:
					_m[t].m4 = _m[t].m2;
					SetTownIndex(t, CalcClosestTownFromTile(t, UINT_MAX)->index);
					break;

				case MP_ROAD:
					_m[t].m4 |= (_m[t].m2 << 4);
					if ((GB(_m[t].m5, 4, 2) == ROAD_TILE_CROSSING ? (Owner)_m[t].m3 : GetTileOwner(t)) == OWNER_TOWN) {
						SetTownIndex(t, CalcClosestTownFromTile(t, UINT_MAX)->index);
					} else {
						SetTownIndex(t, 0);
					}
					break;

				default: break;
			}
		}
	}

	/* From version 9.0, we update the max passengers of a town (was sometimes negative
	 *  before that. */
	if (CheckSavegameVersion(9)) {
		Town *t;
		FOR_ALL_TOWNS(t) UpdateTownMaxPass(t);
	}

	/* From version 16.0, we included autorenew on engines, which are now saved, but
	 *  of course, we do need to initialize them for older savegames. */
	if (CheckSavegameVersion(16)) {
		FOR_ALL_COMPANIES(c) {
			c->engine_renew_list   = NULL;
			c->engine_renew        = false;
			c->engine_renew_months = -6;
			c->engine_renew_money  = 100000;
		}

		/* When loading a game, _local_company is not yet set to the correct value.
		 * However, in a dedicated server we are a spectator, so nothing needs to
		 * happen. In case we are not a dedicated server, the local company always
		 * becomes company 0, unless we are in the scenario editor where all the
		 * companies are 'invalid'.
		 */
		if (!_network_dedicated && IsValidCompanyID(COMPANY_FIRST)) {
			c = GetCompany(COMPANY_FIRST);
			c->engine_renew        = _settings_client.gui.autorenew;
			c->engine_renew_months = _settings_client.gui.autorenew_months;
			c->engine_renew_money  = _settings_client.gui.autorenew_money;
		}
	}

	if (CheckSavegameVersion(48)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (IsPlainRailTile(t)) {
						/* Swap ground type and signal type for plain rail tiles, so the
						 * ground type uses the same bits as for depots and waypoints. */
						uint tmp = GB(_m[t].m4, 0, 4);
						SB(_m[t].m4, 0, 4, GB(_m[t].m2, 0, 4));
						SB(_m[t].m2, 0, 4, tmp);
					} else if (HasBit(_m[t].m5, 2)) {
						/* Split waypoint and depot rail type and remove the subtype. */
						ClrBit(_m[t].m5, 2);
						ClrBit(_m[t].m5, 6);
					}
					break;

				case MP_ROAD:
					/* Swap m3 and m4, so the track type for rail crossings is the
					 * same as for normal rail. */
					Swap(_m[t].m3, _m[t].m4);
					break;

				default: break;
			}
		}
	}

	if (CheckSavegameVersion(61)) {
		/* Added the RoadType */
		bool old_bridge = CheckSavegameVersion(42);
		for (TileIndex t = 0; t < map_size; t++) {
			switch(GetTileType(t)) {
				case MP_ROAD:
					SB(_m[t].m5, 6, 2, GB(_m[t].m5, 4, 2));
					switch (GetRoadTileType(t)) {
						default: NOT_REACHED();
						case ROAD_TILE_NORMAL:
							SB(_m[t].m4, 0, 4, GB(_m[t].m5, 0, 4));
							SB(_m[t].m4, 4, 4, 0);
							SB(_m[t].m6, 2, 4, 0);
							break;
						case ROAD_TILE_CROSSING:
							SB(_m[t].m4, 5, 2, GB(_m[t].m5, 2, 2));
							break;
						case ROAD_TILE_DEPOT:    break;
					}
					SetRoadTypes(t, ROADTYPES_ROAD);
					break;

				case MP_STATION:
					if (IsRoadStop(t)) SetRoadTypes(t, ROADTYPES_ROAD);
					break;

				case MP_TUNNELBRIDGE:
					/* Middle part of "old" bridges */
					if (old_bridge && IsBridge(t) && HasBit(_m[t].m5, 6)) break;
					if (((old_bridge && IsBridge(t)) ? (TransportType)GB(_m[t].m5, 1, 2) : GetTunnelBridgeTransportType(t)) == TRANSPORT_ROAD) {
						SetRoadTypes(t, ROADTYPES_ROAD);
					}
					break;

				default: break;
			}
		}
	}

	if (CheckSavegameVersion(42)) {
		Vehicle* v;

		for (TileIndex t = 0; t < map_size; t++) {
			if (MayHaveBridgeAbove(t)) ClearBridgeMiddle(t);
			if (IsBridgeTile(t)) {
				if (HasBit(_m[t].m5, 6)) { // middle part
					Axis axis = (Axis)GB(_m[t].m5, 0, 1);

					if (HasBit(_m[t].m5, 5)) { // transport route under bridge?
						if (GB(_m[t].m5, 3, 2) == TRANSPORT_RAIL) {
							MakeRailNormal(
								t,
								GetTileOwner(t),
								axis == AXIS_X ? TRACK_BIT_Y : TRACK_BIT_X,
								GetRailType(t)
							);
						} else {
							TownID town = IsTileOwner(t, OWNER_TOWN) ? ClosestTownFromTile(t, UINT_MAX)->index : 0;

							MakeRoadNormal(
								t,
								axis == AXIS_X ? ROAD_Y : ROAD_X,
								ROADTYPES_ROAD,
								town,
								GetTileOwner(t), OWNER_NONE, OWNER_NONE
							);
						}
					} else {
						if (GB(_m[t].m5, 3, 2) == 0) {
							MakeClear(t, CLEAR_GRASS, 3);
						} else {
							if (GetTileSlope(t, NULL) != SLOPE_FLAT) {
								MakeShore(t);
							} else {
								if (GetTileOwner(t) == OWNER_WATER) {
									MakeWater(t);
								} else {
									MakeCanal(t, GetTileOwner(t), Random());
								}
							}
						}
					}
					SetBridgeMiddle(t, axis);
				} else { // ramp
					Axis axis = (Axis)GB(_m[t].m5, 0, 1);
					uint north_south = GB(_m[t].m5, 5, 1);
					DiagDirection dir = ReverseDiagDir(XYNSToDiagDir(axis, north_south));
					TransportType type = (TransportType)GB(_m[t].m5, 1, 2);

					_m[t].m5 = 1 << 7 | type << 2 | dir;
				}
			}
		}

		FOR_ALL_VEHICLES(v) {
			if (v->type != VEH_TRAIN && v->type != VEH_ROAD) continue;
			if (IsBridgeTile(v->tile)) {
				DiagDirection dir = GetTunnelBridgeDirection(v->tile);

				if (dir != DirToDiagDir(v->direction)) continue;
				switch (dir) {
					default: NOT_REACHED();
					case DIAGDIR_NE: if ((v->x_pos & 0xF) !=  0)            continue; break;
					case DIAGDIR_SE: if ((v->y_pos & 0xF) != TILE_SIZE - 1) continue; break;
					case DIAGDIR_SW: if ((v->x_pos & 0xF) != TILE_SIZE - 1) continue; break;
					case DIAGDIR_NW: if ((v->y_pos & 0xF) !=  0)            continue; break;
				}
			} else if (v->z_pos > GetSlopeZ(v->x_pos, v->y_pos)) {
				v->tile = GetNorthernBridgeEnd(v->tile);
			} else {
				continue;
			}
			if (v->type == VEH_TRAIN) {
				v->u.rail.track = TRACK_BIT_WORMHOLE;
			} else {
				v->u.road.state = RVSB_WORMHOLE;
			}
		}
	}

	/* Elrails got added in rev 24 */
	if (CheckSavegameVersion(24)) {
		Vehicle *v;
		RailType min_rail = RAILTYPE_ELECTRIC;

		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_TRAIN) {
				RailType rt = RailVehInfo(v->engine_type)->railtype;

				v->u.rail.railtype = rt;
				if (rt == RAILTYPE_ELECTRIC) min_rail = RAILTYPE_RAIL;
			}
		}

		/* .. so we convert the entire map from normal to elrail (so maintain "fairness") */
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					break;

				case MP_ROAD:
					if (IsLevelCrossing(t)) {
						SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					}
					break;

				case MP_STATION:
					if (IsRailwayStation(t)) {
						SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					}
					break;

				case MP_TUNNELBRIDGE:
					if (GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL) {
						SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					}
					break;

				default:
					break;
			}
		}

		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_TRAIN && (IsFrontEngine(v) || IsFreeWagon(v))) TrainConsistChanged(v, true);
		}

	}

	/* In version 16.1 of the savegame a company can decide if trains, which get
	 * replaced, shall keep their old length. In all prior versions, just default
	 * to false */
	if (CheckSavegameVersionOldStyle(16, 1)) {
		FOR_ALL_COMPANIES(c) c->renew_keep_length = false;
	}

	/* In version 17, ground type is moved from m2 to m4 for depots and
	 * waypoints to make way for storing the index in m2. The custom graphics
	 * id which was stored in m4 is now saved as a grf/id reference in the
	 * waypoint struct. */
	if (CheckSavegameVersion(17)) {
		Waypoint *wp;

		FOR_ALL_WAYPOINTS(wp) {
			if (wp->deleted == 0) {
				const StationSpec *statspec = NULL;

				if (HasBit(_m[wp->xy].m3, 4))
					statspec = GetCustomStationSpec(STAT_CLASS_WAYP, _m[wp->xy].m4 + 1);

				if (statspec != NULL) {
					wp->stat_id = _m[wp->xy].m4 + 1;
					wp->grfid = statspec->grffile->grfid;
					wp->localidx = statspec->localidx;
				} else {
					/* No custom graphics set, so set to default. */
					wp->stat_id = 0;
					wp->grfid = 0;
					wp->localidx = 0;
				}

				/* Move ground type bits from m2 to m4. */
				_m[wp->xy].m4 = GB(_m[wp->xy].m2, 0, 4);
				/* Store waypoint index in the tile. */
				_m[wp->xy].m2 = wp->index;
			}
		}
	} else {
		/* As of version 17, we recalculate the custom graphic ID of waypoints
		 * from the GRF ID / station index. */
		AfterLoadWaypoints();
	}

	/* From version 15, we moved a semaphore bit from bit 2 to bit 3 in m4, making
	 *  room for PBS. Now in version 21 move it back :P. */
	if (CheckSavegameVersion(21) && !CheckSavegameVersion(15)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (HasSignals(t)) {
						/* convert PBS signals to combo-signals */
						if (HasBit(_m[t].m2, 2)) SetSignalType(t, TRACK_X, SIGTYPE_COMBO);

						/* move the signal variant back */
						SetSignalVariant(t, TRACK_X, HasBit(_m[t].m2, 3) ? SIG_SEMAPHORE : SIG_ELECTRIC);
						ClrBit(_m[t].m2, 3);
					}

					/* Clear PBS reservation on track */
					if (!IsRailDepotTile(t)) {
						SB(_m[t].m4, 4, 4, 0);
					} else {
						ClrBit(_m[t].m3, 6);
					}
					break;

				case MP_ROAD: /* Clear PBS reservation on crossing */
					if (IsLevelCrossing(t)) ClrBit(_m[t].m5, 0);
					break;

				case MP_STATION: /* Clear PBS reservation on station */
					ClrBit(_m[t].m3, 6);
					break;

				default: break;
			}
		}
	}

	if (CheckSavegameVersion(25)) {
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_ROAD) {
				v->vehstatus &= ~0x40;
				v->u.road.slot = NULL;
				v->u.road.slot_age = 0;
			}
		}
	} else {
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_ROAD && v->u.road.slot != NULL) v->u.road.slot->num_vehicles++;
		}
	}

	if (CheckSavegameVersion(26)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			st->last_vehicle_type = VEH_INVALID;
		}
	}

	YapfNotifyTrackLayoutChange(INVALID_TILE, INVALID_TRACK);

	if (CheckSavegameVersion(34)) FOR_ALL_COMPANIES(c) ResetCompanyLivery(c);

	FOR_ALL_COMPANIES(c) {
		c->avail_railtypes = GetCompanyRailtypes(c->index);
		c->avail_roadtypes = GetCompanyRoadtypes(c->index);
	}

	if (!CheckSavegameVersion(27)) AfterLoadStations();

	/* Time starts at 0 instead of 1920.
	 * Account for this in older games by adding an offset */
	if (CheckSavegameVersion(31)) {
		Station *st;
		Waypoint *wp;
		Engine *e;
		Industry *i;
		Vehicle *v;

		_date += DAYS_TILL_ORIGINAL_BASE_YEAR;
		_cur_year += ORIGINAL_BASE_YEAR;

		FOR_ALL_STATIONS(st)  st->build_date      += DAYS_TILL_ORIGINAL_BASE_YEAR;
		FOR_ALL_WAYPOINTS(wp) wp->build_date      += DAYS_TILL_ORIGINAL_BASE_YEAR;
		FOR_ALL_ENGINES(e)    e->intro_date       += DAYS_TILL_ORIGINAL_BASE_YEAR;
		FOR_ALL_COMPANIES(c)  c->inaugurated_year += ORIGINAL_BASE_YEAR;
		FOR_ALL_INDUSTRIES(i) i->last_prod_year   += ORIGINAL_BASE_YEAR;

		FOR_ALL_VEHICLES(v) {
			v->date_of_last_service += DAYS_TILL_ORIGINAL_BASE_YEAR;
			v->build_year += ORIGINAL_BASE_YEAR;
		}
	}

	/* From 32 on we save the industry who made the farmland.
	 *  To give this prettyness to old savegames, we remove all farmfields and
	 *  plant new ones. */
	if (CheckSavegameVersion(32)) {
		Industry *i;

		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_CLEAR) && IsClearGround(t, CLEAR_FIELDS)) {
				/* remove fields */
				MakeClear(t, CLEAR_GRASS, 3);
			} else if (IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES)) {
				/* remove fences around fields */
				SetFenceSE(t, 0);
				SetFenceSW(t, 0);
			}
		}

		FOR_ALL_INDUSTRIES(i) {
			uint j;

			if (GetIndustrySpec(i->type)->behaviour & INDUSTRYBEH_PLANT_ON_BUILT) {
				for (j = 0; j != 50; j++) PlantRandomFarmField(i);
			}
		}
	}

	/* Setting no refit flags to all orders in savegames from before refit in orders were added */
	if (CheckSavegameVersion(36)) {
		Order *order;
		Vehicle *v;

		FOR_ALL_ORDERS(order) {
			order->SetRefit(CT_NO_REFIT);
		}

		FOR_ALL_VEHICLES(v) {
			v->current_order.SetRefit(CT_NO_REFIT);
		}
	}

	/* from version 38 we have optional elrails, since we cannot know the
	 * preference of a user, let elrails enabled; it can be disabled manually */
	if (CheckSavegameVersion(38)) _settings_game.vehicle.disable_elrails = false;
	/* do the same as when elrails were enabled/disabled manually just now */
	SettingsDisableElrail(_settings_game.vehicle.disable_elrails);
	InitializeRailGUI();

	/* From version 53, the map array was changed for house tiles to allow
	 * space for newhouses grf features. A new byte, m7, was also added. */
	if (CheckSavegameVersion(53)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_HOUSE)) {
				if (GB(_m[t].m3, 6, 2) != TOWN_HOUSE_COMPLETED) {
					/* Move the construction stage from m3[7..6] to m5[5..4].
					 * The construction counter does not have to move. */
					SB(_m[t].m5, 3, 2, GB(_m[t].m3, 6, 2));
					SB(_m[t].m3, 6, 2, 0);

					/* The "house is completed" bit is now in m6[2]. */
					SetHouseCompleted(t, false);
				} else {
					/* The "lift has destination" bit has been moved from
					 * m5[7] to m7[0]. */
					SB(_me[t].m7, 0, 1, HasBit(_m[t].m5, 7));
					ClrBit(_m[t].m5, 7);

					/* The "lift is moving" bit has been removed, as it does
					 * the same job as the "lift has destination" bit. */
					ClrBit(_m[t].m1, 7);

					/* The position of the lift goes from m1[7..0] to m6[7..2],
					 * making m1 totally free, now. The lift position does not
					 * have to be a full byte since the maximum value is 36. */
					SetLiftPosition(t, GB(_m[t].m1, 0, 6 ));

					_m[t].m1 = 0;
					_m[t].m3 = 0;
					SetHouseCompleted(t, true);
				}
			}
		}
	}

	/* Check and update house and town values */
	UpdateHousesAndTowns();

	if (CheckSavegameVersion(43)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_INDUSTRY)) {
				switch (GetIndustryGfx(t)) {
					case GFX_POWERPLANT_SPARKS:
						SetIndustryAnimationState(t, GB(_m[t].m1, 2, 5));
						break;

					case GFX_OILWELL_ANIMATED_1:
					case GFX_OILWELL_ANIMATED_2:
					case GFX_OILWELL_ANIMATED_3:
						SetIndustryAnimationState(t, GB(_m[t].m1, 0, 2));
						break;

					case GFX_COAL_MINE_TOWER_ANIMATED:
					case GFX_COPPER_MINE_TOWER_ANIMATED:
					case GFX_GOLD_MINE_TOWER_ANIMATED:
						 SetIndustryAnimationState(t, _m[t].m1);
						 break;

					default: /* No animation states to change */
						break;
				}
			}
		}
	}

	if (CheckSavegameVersion(44)) {
		Vehicle *v;
		/* If we remove a station while cargo from it is still enroute, payment calculation will assume
		 * 0, 0 to be the source of the cargo, resulting in very high payments usually. v->source_xy
		 * stores the coordinates, preserving them even if the station is removed. However, if a game is loaded
		 * where this situation exists, the cargo-source information is lost. in this case, we set the source
		 * to the current tile of the vehicle to prevent excessive profits
		 */
		FOR_ALL_VEHICLES(v) {
			const CargoList::List *packets = v->cargo.Packets();
			for (CargoList::List::const_iterator it = packets->begin(); it != packets->end(); it++) {
				CargoPacket *cp = *it;
				cp->source_xy = IsValidStationID(cp->source) ? GetStation(cp->source)->xy : v->tile;
				cp->loaded_at_xy = cp->source_xy;
			}
			v->cargo.InvalidateCache();
		}

		/* Store position of the station where the goods come from, so there
		 * are no very high payments when stations get removed. However, if the
		 * station where the goods came from is already removed, the source
		 * information is lost. In that case we set it to the position of this
		 * station */
		Station *st;
		FOR_ALL_STATIONS(st) {
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				GoodsEntry *ge = &st->goods[c];

				const CargoList::List *packets = ge->cargo.Packets();
				for (CargoList::List::const_iterator it = packets->begin(); it != packets->end(); it++) {
					CargoPacket *cp = *it;
					cp->source_xy = IsValidStationID(cp->source) ? GetStation(cp->source)->xy : st->xy;
					cp->loaded_at_xy = cp->source_xy;
				}
			}
		}
	}

	if (CheckSavegameVersion(45)) {
		Vehicle *v;
		/* Originally just the fact that some cargo had been paid for was
		 * stored to stop people cheating and cashing in several times. This
		 * wasn't enough though as it was cleared when the vehicle started
		 * loading again, even if it didn't actually load anything, so now the
		 * amount of cargo that has been paid for is stored. */
		FOR_ALL_VEHICLES(v) {
			const CargoList::List *packets = v->cargo.Packets();
			for (CargoList::List::const_iterator it = packets->begin(); it != packets->end(); it++) {
				CargoPacket *cp = *it;
				cp->paid_for = HasBit(v->vehicle_flags, 2);
			}
			ClrBit(v->vehicle_flags, 2);
			v->cargo.InvalidateCache();
		}
	}

	/* Buoys do now store the owner of the previous water tile, which can never
	 * be OWNER_NONE. So replace OWNER_NONE with OWNER_WATER. */
	if (CheckSavegameVersion(46)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->IsBuoy() && IsTileOwner(st->xy, OWNER_NONE) && TileHeight(st->xy) == 0) SetTileOwner(st->xy, OWNER_WATER);
		}
	}

	if (CheckSavegameVersion(50)) {
		Vehicle *v;
		/* Aircraft units changed from 8 mph to 1 km/h */
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_AIRCRAFT && v->subtype <= AIR_AIRCRAFT) {
				const AircraftVehicleInfo *avi = AircraftVehInfo(v->engine_type);
				v->cur_speed *= 129;
				v->cur_speed /= 10;
				v->max_speed = avi->max_speed;
				v->acceleration = avi->acceleration;
			}
		}
	}

	if (CheckSavegameVersion(49)) FOR_ALL_COMPANIES(c) c->face = ConvertFromOldCompanyManagerFace(c->face);

	if (CheckSavegameVersion(52)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsStatueTile(t)) {
				_m[t].m2 = CalcClosestTownFromTile(t, UINT_MAX)->index;
			}
		}
	}

	/* A patch option containing the proportion of towns that grow twice as
	 * fast was added in version 54. From version 56 this is now saved in the
	 * town as cities can be built specifically in the scenario editor. */
	if (CheckSavegameVersion(56)) {
		Town *t;

		FOR_ALL_TOWNS(t) {
			if (_settings_game.economy.larger_towns != 0 && (t->index % _settings_game.economy.larger_towns) == 0) {
				t->larger_town = true;
			}
		}
	}

	if (CheckSavegameVersion(57)) {
		Vehicle *v;
		/* Added a FIFO queue of vehicles loading at stations */
		FOR_ALL_VEHICLES(v) {
			if ((v->type != VEH_TRAIN || IsFrontEngine(v)) &&  // for all locs
					!(v->vehstatus & (VS_STOPPED | VS_CRASHED)) && // not stopped or crashed
					v->current_order.IsType(OT_LOADING)) {         // loading
				GetStation(v->last_station_visited)->loading_vehicles.push_back(v);

				/* The loading finished flag is *only* set when actually completely
				 * finished. Because the vehicle is loading, it is not finished. */
				ClrBit(v->vehicle_flags, VF_LOADING_FINISHED);
			}
		}
	} else if (CheckSavegameVersion(59)) {
		/* For some reason non-loading vehicles could be in the station's loading vehicle list */

		Station *st;
		FOR_ALL_STATIONS(st) {
			std::list<Vehicle *>::iterator iter;
			for (iter = st->loading_vehicles.begin(); iter != st->loading_vehicles.end();) {
				Vehicle *v = *iter;
				iter++;
				if (!v->current_order.IsType(OT_LOADING)) st->loading_vehicles.remove(v);
			}
		}
	}

	if (CheckSavegameVersion(58)) {
		/* patch difficulty number_industries other than zero get bumped to +1
		 * since a new option (very low at position1) has been added */
		if (_settings_game.difficulty.number_industries > 0) {
			_settings_game.difficulty.number_industries++;
		}

		/* Same goes for number of towns, although no test is needed, just an increment */
		_settings_game.difficulty.number_towns++;
	}

	if (CheckSavegameVersion(64)) {
		/* copy the signal type/variant and move signal states bits */
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_RAILWAY) && HasSignals(t)) {
				SetSignalStates(t, GB(_m[t].m2, 4, 4));
				SetSignalVariant(t, INVALID_TRACK, GetSignalVariant(t, TRACK_X));
				SetSignalType(t, INVALID_TRACK, GetSignalType(t, TRACK_X));
				ClrBit(_m[t].m2, 7);
			}
		}
	}

	if (CheckSavegameVersion(69)) {
		/* In some old savegames a bit was cleared when it should not be cleared */
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_ROAD && (v->u.road.state == 250 || v->u.road.state == 251)) {
				SetBit(v->u.road.state, RVS_IS_STOPPING);
			}
		}
	}

	if (CheckSavegameVersion(70)) {
		/* Added variables to support newindustries */
		Industry *i;
		FOR_ALL_INDUSTRIES(i) i->founder = OWNER_NONE;
	}

	/* From version 82, old style canals (above sealevel (0), WATER owner) are no longer supported.
	    Replace the owner for those by OWNER_NONE. */
	if (CheckSavegameVersion(82)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_WATER) &&
					GetWaterTileType(t) == WATER_TILE_CLEAR &&
					GetTileOwner(t) == OWNER_WATER &&
					TileHeight(t) != 0) {
				SetTileOwner(t, OWNER_NONE);
			}
		}
	}

	/*
	 * Add the 'previous' owner to the ship depots so we can reset it with
	 * the correct values when it gets destroyed. This prevents that
	 * someone can remove canals owned by somebody else and it prevents
	 * making floods using the removal of ship depots.
	 */
	if (CheckSavegameVersion(83)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_WATER) && IsShipDepot(t)) {
				_m[t].m4 = (TileHeight(t) == 0) ? OWNER_WATER : OWNER_NONE;
			}
		}
	}

	if (CheckSavegameVersion(74)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				st->goods[c].last_speed = 0;
				if (st->goods[c].cargo.Count() != 0) SetBit(st->goods[c].acceptance_pickup, GoodsEntry::PICKUP);
			}
		}
	}

	if (CheckSavegameVersion(78)) {
		Industry *i;
		uint j;
		FOR_ALL_INDUSTRIES(i) {
			const IndustrySpec *indsp = GetIndustrySpec(i->type);
			for (j = 0; j < lengthof(i->produced_cargo); j++) {
				i->produced_cargo[j] = indsp->produced_cargo[j];
			}
			for (j = 0; j < lengthof(i->accepts_cargo); j++) {
				i->accepts_cargo[j] = indsp->accepts_cargo[j];
			}
		}
	}

	/* Before version 81, the density of grass was always stored as zero, and
	 * grassy trees were always drawn fully grassy. Furthermore, trees on rough
	 * land used to have zero density, now they have full density. Therefore,
	 * make all grassy/rough land trees have a density of 3. */
	if (CheckSavegameVersion(81)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (GetTileType(t) == MP_TREES) {
				TreeGround groundType = GetTreeGround(t);
				if (groundType != TREE_GROUND_SNOW_DESERT) SetTreeGroundDensity(t, groundType, 3);
			}
		}
	}


	if (CheckSavegameVersion(93)) {
		/* Rework of orders. */
		Order *order;
		FOR_ALL_ORDERS(order) order->ConvertFromOldSavegame();

		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->orders != NULL && !v->orders->IsValid()) v->orders = NULL;

			v->current_order.ConvertFromOldSavegame();
			if (v->type == VEH_ROAD && v->IsPrimaryVehicle() && v->FirstShared() == v) {
				FOR_VEHICLE_ORDERS(v, order) order->SetNonStopType(ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
			}
		}
	} else if (CheckSavegameVersion(94)) {
		/* Unload and transfer are now mutual exclusive. */
		Order *order;
		FOR_ALL_ORDERS(order) {
			if ((order->GetUnloadType() & (OUFB_UNLOAD | OUFB_TRANSFER)) == (OUFB_UNLOAD | OUFB_TRANSFER)) {
				order->SetUnloadType(OUFB_TRANSFER);
				order->SetLoadType(OLFB_NO_LOAD);
			}
		}

		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if ((v->current_order.GetUnloadType() & (OUFB_UNLOAD | OUFB_TRANSFER)) == (OUFB_UNLOAD | OUFB_TRANSFER)) {
				v->current_order.SetUnloadType(OUFB_TRANSFER);
				v->current_order.SetLoadType(OLFB_NO_LOAD);
			}
		}
	}

	if (CheckSavegameVersion(84)) {
		/* Update go to buoy orders because they are just waypoints */
		Order *order;
		FOR_ALL_ORDERS(order) {
			if (order->IsType(OT_GOTO_STATION) && GetStation(order->GetDestination())->IsBuoy()) {
				order->SetLoadType(OLF_LOAD_IF_POSSIBLE);
				order->SetUnloadType(OUF_UNLOAD_IF_POSSIBLE);
			}
		}

		/* Set all share owners to INVALID_COMPANY for
		 * 1) all inactive companies
		 *     (when inactive companies were stored in the savegame - TTD, TTDP and some
		 *      *really* old revisions of OTTD; else it is already set in InitializeCompanies())
		 * 2) shares that are owned by inactive companies or self
		 *     (caused by cheating clients in earlier revisions) */
		FOR_ALL_COMPANIES(c) {
			for (uint i = 0; i < 4; i++) {
				CompanyID company = c->share_owners[i];
				if (company == INVALID_COMPANY) continue;
				if (!IsValidCompanyID(company) || company == c->index) c->share_owners[i] = INVALID_COMPANY;
			}
		}
	}

	if (CheckSavegameVersion(86)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* Move river flag and update canals to use water class */
			if (IsTileType(t, MP_WATER)) {
				if (GetWaterClass(t) != WATER_CLASS_RIVER) {
					if (IsWater(t)) {
						Owner o = GetTileOwner(t);
						if (o == OWNER_WATER) {
							MakeWater(t);
						} else {
							MakeCanal(t, o, Random());
						}
					} else if (IsShipDepot(t)) {
						Owner o = (Owner)_m[t].m4; // Original water owner
						SetWaterClass(t, o == OWNER_WATER ? WATER_CLASS_SEA : WATER_CLASS_CANAL);
					}
				}
			}
		}

		/* Update locks, depots, docks and buoys to have a water class based
		 * on its neighbouring tiles. Done after river and canal updates to
		 * ensure neighbours are correct. */
		for (TileIndex t = 0; t < map_size; t++) {
			if (GetTileSlope(t, NULL) != SLOPE_FLAT) continue;

			if (IsTileType(t, MP_WATER) && IsLock(t)) SetWaterClassDependingOnSurroundings(t, false);
			if (IsTileType(t, MP_STATION) && (IsDock(t) || IsBuoy(t))) SetWaterClassDependingOnSurroundings(t, false);
		}
	}

	if (CheckSavegameVersion(87)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* skip oil rigs at borders! */
			if ((IsTileType(t, MP_WATER) || IsBuoyTile(t)) &&
					(TileX(t) == 0 || TileY(t) == 0 || TileX(t) == MapMaxX() - 1 || TileY(t) == MapMaxY() - 1)) {
				/* Some version 86 savegames have wrong water class at map borders (under buoy, or after removing buoy).
				 * This conversion has to be done before buoys with invalid owner are removed. */
				SetWaterClass(t, WATER_CLASS_SEA);
			}

			if (IsBuoyTile(t) || IsDriveThroughStopTile(t) || IsTileType(t, MP_WATER)) {
				Owner o = GetTileOwner(t);
				if (o < MAX_COMPANIES && !IsValidCompanyID(o)) {
					_current_company = o;
					ChangeTileOwner(t, o, INVALID_OWNER);
				}
				if (IsBuoyTile(t)) {
					/* reset buoy owner to OWNER_NONE in the station struct
					 * (even if it is owned by active company) */
					GetStationByTile(t)->owner = OWNER_NONE;
				}
			} else if (IsTileType(t, MP_ROAD)) {
				/* works for all RoadTileType */
				for (RoadType rt = ROADTYPE_ROAD; rt < ROADTYPE_END; rt++) {
					/* update even non-existing road types to update tile owner too */
					Owner o = GetRoadOwner(t, rt);
					if (o < MAX_COMPANIES && !IsValidCompanyID(o)) SetRoadOwner(t, rt, OWNER_NONE);
				}
				if (IsLevelCrossing(t)) {
					Owner o = GetTileOwner(t);
					if (!IsValidCompanyID(o)) {
						/* remove leftover rail piece from crossing (from very old savegames) */
						_current_company = o;
						DoCommand(t, 0, GetCrossingRailTrack(t), DC_EXEC | DC_BANKRUPT, CMD_REMOVE_SINGLE_RAIL);
					}
				}
			}
		}

		/* Convert old PF settings to new */
		if (_settings_game.pf.yapf.rail_use_yapf || CheckSavegameVersion(28)) {
			_settings_game.pf.pathfinder_for_trains = VPF_YAPF;
		} else {
			_settings_game.pf.pathfinder_for_trains = (_settings_game.pf.new_pathfinding_all ? VPF_NPF : VPF_NTP);
		}

		if (_settings_game.pf.yapf.road_use_yapf || CheckSavegameVersion(28)) {
			_settings_game.pf.pathfinder_for_roadvehs = VPF_YAPF;
		} else {
			_settings_game.pf.pathfinder_for_roadvehs = (_settings_game.pf.new_pathfinding_all ? VPF_NPF : VPF_OPF);
		}

		if (_settings_game.pf.yapf.ship_use_yapf) {
			_settings_game.pf.pathfinder_for_ships = VPF_YAPF;
		} else {
			_settings_game.pf.pathfinder_for_ships = (_settings_game.pf.new_pathfinding_all ? VPF_NPF : VPF_OPF);
		}
	}

	if (CheckSavegameVersion(88)) {
		/* Profits are now with 8 bit fract */
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			v->profit_this_year <<= 8;
			v->profit_last_year <<= 8;
			v->running_ticks = 0;
		}
	}

	if (CheckSavegameVersion(91)) {
		/* Increase HouseAnimationFrame from 5 to 7 bits */
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_HOUSE) && GetHouseType(t) >= NEW_HOUSE_OFFSET) {
				SetHouseAnimationFrame(t, GB(_m[t].m6, 3, 5));
			}
		}
	}

	if (CheckSavegameVersion(62)) {
		/* Remove all trams from savegames without tram support.
		 * There would be trams without tram track under causing crashes sooner or later. */
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_ROAD && v->First() == v &&
					HasBit(EngInfo(v->engine_type)->misc_flags, EF_ROAD_TRAM)) {
				if (_switch_mode_errorstr == INVALID_STRING_ID || _switch_mode_errorstr == STR_NEWGRF_COMPATIBLE_LOAD_WARNING) {
					_switch_mode_errorstr = STR_LOADGAME_REMOVED_TRAMS;
				}
				delete v;
			}
		}
	}

	if (CheckSavegameVersion(99)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* Set newly introduced WaterClass of industry tiles */
			if (IsTileType(t, MP_STATION) && IsOilRig(t)) {
				SetWaterClassDependingOnSurroundings(t, true);
			}
			if (IsTileType(t, MP_INDUSTRY)) {
				if ((GetIndustrySpec(GetIndustryType(t))->behaviour & INDUSTRYBEH_BUILT_ONWATER) != 0) {
					SetWaterClassDependingOnSurroundings(t, true);
				} else {
					SetWaterClass(t, WATER_CLASS_INVALID);
				}
			}

			/* Replace "house construction year" with "house age" */
			if (IsTileType(t, MP_HOUSE) && IsHouseCompleted(t)) {
				_m[t].m5 = Clamp(_cur_year - (_m[t].m5 + ORIGINAL_BASE_YEAR), 0, 0xFF);
			}
		}
	}

	/* Move the signal variant back up one bit for PBS. We don't convert the old PBS
	 * format here, as an old layout wouldn't work properly anyway. To be safe, we
	 * clear any possible PBS reservations as well. */
	if (CheckSavegameVersion(100)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (HasSignals(t)) {
						/* move the signal variant */
						SetSignalVariant(t, TRACK_UPPER, HasBit(_m[t].m2, 2) ? SIG_SEMAPHORE : SIG_ELECTRIC);
						SetSignalVariant(t, TRACK_LOWER, HasBit(_m[t].m2, 6) ? SIG_SEMAPHORE : SIG_ELECTRIC);
						ClrBit(_m[t].m2, 2);
						ClrBit(_m[t].m2, 6);
					}

					/* Clear PBS reservation on track */
					if (IsRailDepot(t) ||IsRailWaypoint(t)) {
						SetDepotWaypointReservation(t, false);
					} else {
						SetTrackReservation(t, TRACK_BIT_NONE);
					}
					break;

				case MP_ROAD: /* Clear PBS reservation on crossing */
					if (IsLevelCrossing(t)) SetCrossingReservation(t, false);
					break;

				case MP_STATION: /* Clear PBS reservation on station */
					if (IsRailwayStation(t)) SetRailwayStationReservation(t, false);
					break;

				case MP_TUNNELBRIDGE: /* Clear PBS reservation on tunnels/birdges */
					if (GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL) SetTunnelBridgeReservation(t, false);
					break;

				default: break;
			}
		}
	}

	/* Reserve all tracks trains are currently on. */
	if (CheckSavegameVersion(101)) {
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_TRAIN) {
				if ((v->u.rail.track & TRACK_BIT_WORMHOLE) == TRACK_BIT_WORMHOLE) {
					TryReserveRailTrack(v->tile, DiagDirToDiagTrack(GetTunnelBridgeDirection(v->tile)));
				} else if ((v->u.rail.track & TRACK_BIT_MASK) != TRACK_BIT_NONE) {
					TryReserveRailTrack(v->tile, TrackBitsToTrack(v->u.rail.track));
				}
			}
		}

		/* Give owners to waypoints, based on rail tracks it is sitting on.
		 * If none is available, specify OWNER_NONE */
		Waypoint *wp;
		FOR_ALL_WAYPOINTS(wp) {
			Owner owner = (IsRailWaypointTile(wp->xy) ? GetTileOwner(wp->xy) : OWNER_NONE);
			wp->owner = IsValidCompanyID(owner) ? owner : OWNER_NONE;
		}
	}

	if (CheckSavegameVersion(102)) {
		for (TileIndex t = 0; t < map_size; t++) {
			/* Now all crossings should be in correct state */
			if (IsLevelCrossingTile(t)) UpdateLevelCrossing(t, false);
		}
	}

	if (CheckSavegameVersion(103)) {
		/* Non-town-owned roads now store the closest town */
		UpdateNearestTownForRoadTiles(false);

		/* signs with invalid owner left from older savegames */
		Sign *si;
		FOR_ALL_SIGNS(si) {
			if (si->owner != OWNER_NONE && !IsValidCompanyID(si->owner)) si->owner = OWNER_NONE;
		}

		/* Station can get named based on an industry type, but the current ones
		 * are not, so mark them as if they are not named by an industry. */
		Station *st;
		FOR_ALL_STATIONS(st) {
			st->indtype = IT_INVALID;
		}
	}

	if (CheckSavegameVersion(104)) {
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			/* Set engine_type of shadow and rotor */
			if (v->type == VEH_AIRCRAFT && !IsNormalAircraft(v)) {
				v->engine_type = v->First()->engine_type;
			}
		}
	}

	GamelogPrintDebug(1);

	return InitializeWindowsAndCaches();
}

/** Reload all NewGRF files during a running game. This is a cut-down
 * version of AfterLoadGame().
 * XXX - We need to reset the vehicle position hash because with a non-empty
 * hash AfterLoadVehicles() will loop infinitely. We need AfterLoadVehicles()
 * to recalculate vehicle data as some NewGRF vehicle sets could have been
 * removed or added and changed statistics */
void ReloadNewGRFData()
{
	/* reload grf data */
	GfxLoadSprites();
	LoadStringWidthTable();
	ResetEconomy();
	/* reload vehicles */
	ResetVehiclePosHash();
	AfterLoadVehicles(false);
	StartupEngines();
	SetCachedEngineCounts();
	/* update station and waypoint graphics */
	AfterLoadWaypoints();
	AfterLoadStations();
	/* Check and update house and town values */
	UpdateHousesAndTowns();
	/* Update livery selection windows */
	for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) InvalidateWindowData(WC_COMPANY_COLOR, i, _loaded_newgrf_features.has_2CC);
	/* redraw the whole screen */
	MarkWholeScreenDirty();
	CheckTrainsLengths();
}
