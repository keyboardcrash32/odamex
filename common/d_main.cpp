// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2020 by The Odamex Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Common functions to determine game mode (shareware, registered),
//	parse command line parameters, and handle wad changes.
//
//-----------------------------------------------------------------------------


#include "odamex.h"


#include <sstream>
#include <algorithm>

#include "win32inc.h"
#ifndef _WIN32
    #include <sys/stat.h>
#endif

#ifdef UNIX
#include <unistd.h>
#include <dirent.h>
#endif

#include <stdlib.h>

#include "m_alloc.h"
#include "gstrings.h"
#include "z_zone.h"
#include "m_argv.h"
#include "m_fileio.h"
#include "c_console.h"
#include "i_system.h"
#include "g_game.h"
#include "g_spawninv.h"
#include "r_main.h"
#include "d_main.h"
#include "d_dehacked.h"
#include "s_sound.h"
#include "gi.h"
#include "w_ident.h"
#include "m_resfile.h"

#ifdef GEKKO
#include "i_wii.h"
#endif

#ifdef _XBOX
#include "i_xbox.h"
#endif

#include "resources/res_main.h"
#include "resources/res_filelib.h"
EXTERN_CVAR (waddirs)
EXTERN_CVAR (cl_waddownloaddir)
OWantFiles missingfiles;
bool missingCommercialIWAD = false;

bool lastWadRebootSuccess = true;
extern bool step_mode;

bool capfps = true;
float maxfps = 35.0f;

extern bool step_mode;


//
// D_GetTitleString
//
// Returns the proper name of the game currently loaded into gameinfo & gamemission
//
std::string D_GetTitleString()
{
	if (gamemission == pack_tnt)
		return "DOOM 2: TNT - Evilution";
	if (gamemission == pack_plut)
		return "DOOM 2: Plutonia Experiment";
	if (gamemission == chex)
		return "Chex Quest";
	if (gamemission == retail_freedoom)
		return "Ultimate FreeDoom";
	if (gamemission == commercial_freedoom)
		return "FreeDoom";
	if (gamemission == commercial_hacx)
		return "HACX";

	return gameinfo.titleString;
}


//
// D_PrintIWADIdentity
//
static void D_PrintIWADIdentity()
{
	if (clientside)
	{
		Printf(PRINT_HIGH, "\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
    	                   "\36\36\36\36\36\36\36\36\36\36\36\36\37\n");

		if (gamemode == undetermined)
			Printf_Bold("Game mode indeterminate, no standard WAD found.\n\n");
		else
			Printf_Bold("%s\n\n", D_GetTitleString().c_str());
	}
	else
	{
		if (gamemode == undetermined)
			Printf(PRINT_HIGH, "Game mode indeterminate, no standard WAD found.\n");
		else 
			Printf(PRINT_HIGH, "%s\n", D_GetTitleString().c_str()); 
	}
}


//
// D_LoadResourceFiles
//
// Loads the given set of resource file names.
//
void D_LoadResourceFiles(const std::vector<std::string>& resource_filenames)
{
	std::vector<std::string> new_resource_filenames = resource_filenames;
	std::vector<std::string> missing_resource_filenames;

	// Ensure the list of resource filenames include ODAMEX.PK3, an IWAD, and
	// the full path for every file.
	Res_ValidateResourceFiles(new_resource_filenames, missing_resource_filenames);

	// If the given files are already loaded, bail out early.
	if (new_resource_filenames == Res_GetResourceFileNames())
		return;

	assert(new_resource_filenames.size() >= 2);	// Require ODAMEX.PK3 and an IWAD

	// Now scan the contents of the IWAD to determine which one it is.
	const std::string& iwad_filename = new_resource_filenames[1];
	W_ConfigureGameInfo(iwad_filename);

	// Print info about the IWAD to the console.
	D_PrintIWADIdentity();

	// Set the window title based on which IWAD we're using.
	I_SetTitleString(D_GetTitleString().c_str());
	
	if (W_IsIWADDeprecated(iwad_filename))
		Printf_Bold("WARNING: IWAD %s is outdated. Please update it to the latest version.\n", iwad_filename.c_str());

	// Load the resource files
	Res_OpenResourceFiles(new_resource_filenames);

	// [RH] Initialize localizable strings.
	// [SL] It is necessary to load the strings here since a dehacked patch
	// might change the strings

	::GStrings.loadStrings(false);

	// Load all DeHackEd files
	/*
	const ResourceIdList dehacked_res_ids = Res_GetAllResourceIds(ResourcePath("/GLOBAL/DEHACKED"));
	for (size_t i = 0; i < dehacked_res_ids.size(); i++)
		D_LoadDehLump(dehacked_res_ids[i]);
	*/

	// check for ChexQuest
	if (gamemode == retail_chex)
	{
		bool chex_deh_loaded = false;
		for (size_t i = 0; i < new_resource_filenames.size(); i++)
			if (iequals(Res_CleanseFilename(new_resource_filenames[i]), "CHEX.DEH"))
				chex_deh_loaded = true;
	
		if (!chex_deh_loaded)
			Printf(PRINT_HIGH, "Warning: CHEX.DEH not loaded, experience may differ from the original!\n");
	}

	// get skill / episode / map from parms
	strcpy(startmap, (gameinfo.flags & GI_MAPxx) ? "MAP01" : "E1M1");
}

/**
 * @brief Print a warning that occurrs when the user has an IWAD that's a
 *        different version than the one we want.
 * 
 * @param wanted The IWAD that we wanted.
 * @return True if we emitted an commercial IWAD warning.
 */
static bool CommercialIWADWarning(const OWantFile& wanted)
{
	const OMD5Hash& hash = wanted.getWantedMD5();
	if (hash.empty())
	{
		// No MD5 means there is no error we can reasonably display.
		return false;
	}

	const FileIdentifier* info = W_GameInfo(wanted.getWantedMD5());
	if (!info)
	{
		// No GameInfo means that we're not dealing with a WAD we recognize.
		return false;
	}

	if (!info->mIsCommercial)
	{
		// Not commercial means that we should treat the IWAD like any other
		// WAD, with no special callout.
		return false;
	}

	Printf("Odamex attempted to load\n> %s.\n\n", info->mIdName.c_str());

	// Try to find an IWAD file with a matching name in the user's directories.
	OWantFile sameNameWant;
	OWantFile::make(sameNameWant, wanted.getBasename(), OFILE_WAD);
	OResFile sameNameRes;
	const bool resolved = M_ResolveWantedFile(sameNameRes, sameNameWant);
	if (!resolved)
	{
		Printf(
		    "Odamex could not find the data file for this game in any of the locations "
		    "it searches for WAD files.  If you know you have %s on your hard drive, you "
		    "can add that path to the 'waddirs' cvar so Odamex can find it.\n\n",
		    wanted.getBasename().c_str());
	}
	else
	{
		const FileIdentifier* curInfo = W_GameInfo(sameNameRes.getMD5());
		if (curInfo)
		{
			// Found a file, but it's the wrong version.
			Printf("Odamex found a possible data file, but it's the wrong version.\n> "
			       "%s\n> %s\n\n",
			       curInfo->mIdName.c_str(), sameNameRes.getFullpath().c_str());
		}
		else
		{
			// Found a file, but it's not recognized at all.
			Printf("Odamex found a possible data file, but Odamex does not recognize "
			       "it.\n> %s\n\n",
			       sameNameRes.getFullpath().c_str());
		}

#ifdef _WIN32
		Printf("You can use a tool such as Omniscient "
		       "<https://drinkybird.net/doom/omniscient> to patch your way to the "
		       "correct version of the data file.\n");
#else
		Printf("You can use a tool such as xdelta3 <http://xdelta.org/> paried with IWAD "
		       "patches located on Github <https://github.com/Doom-Utils/iwad-patches> "
		       "to patch your way to the correct version of the data file.\n");
#endif
	}

	Printf("If you do not own this game, consider purchasing it on Steam, GOG, or other "
	       "digital storefront.\n\n");
	return true;
}

//
// D_ReloadResourceFiles
//
// Loads a new set of resource files if they are not currently loaded.
//
void D_ReloadResourceFiles(const std::vector<std::string>& new_resource_filenames)
{
	const std::vector<std::string>& resource_filenames = Res_GetResourceFileNames();
	bool reload = false;

	if (new_resource_filenames.size() != resource_filenames.size())
	{
		reload = true;
	}
	else
	{
		for (size_t i = 0; i < new_resource_filenames.size(); i++)
		{
			if (!iequals(Res_CleanseFilename(new_resource_filenames[i]), Res_CleanseFilename(resource_filenames[i])))
			{
				reload = true;
				break;
			}
		}
	}

	if (reload)
	{
		D_Shutdown();
		D_Init(new_resource_filenames);
	}
}


//
// D_UnloadResourceFiles
//
void D_UnloadResourceFiles()
{
	Res_CloseAllResourceFiles();
}


// ============================================================================
//
// TaskScheduler class
//
// ============================================================================
//
// Attempts to schedule a task (indicated by the function pointer passed to
// the concrete constructor) at a specified interval. For uncapped rates, that
// interval is simply as often as possible. For capped rates, the interval is
// specified by the rate parameter.
//

class TaskScheduler
{
public:
	virtual ~TaskScheduler() { }
	virtual void run() = 0;
	virtual dtime_t getNextTime() const = 0;
	virtual float getRemainder() const = 0;
};

class UncappedTaskScheduler : public TaskScheduler
{
public:
	UncappedTaskScheduler(void (*task)()) :
		mTask(task)
	{ }

	virtual ~UncappedTaskScheduler() { }

	virtual void run()
	{
		mTask();
	}

	virtual dtime_t getNextTime() const
	{
		return I_GetTime();
	}

	virtual float getRemainder() const
	{
		return 0.0f;
	}

private:
	void				(*mTask)();
};

class CappedTaskScheduler : public TaskScheduler
{
public:
	CappedTaskScheduler(void (*task)(), float rate, int max_count) :
		mTask(task), mMaxCount(max_count),
		mFrameDuration(I_ConvertTimeFromMs(1000) / rate),
		mAccumulator(mFrameDuration),
		mPreviousFrameStartTime(I_GetTime())
	{
	}

	virtual ~CappedTaskScheduler() { }

	virtual void run()
	{
		mFrameStartTime = I_GetTime();
		mAccumulator += mFrameStartTime - mPreviousFrameStartTime;
		mPreviousFrameStartTime = mFrameStartTime;

		int count = mMaxCount;

		while (mAccumulator >= mFrameDuration && count--)
		{
			mTask();
			mAccumulator -= mFrameDuration;
		}
	}

	virtual dtime_t getNextTime() const
	{
		return mFrameStartTime + mFrameDuration - mAccumulator;
	}

	virtual float getRemainder() const
	{
		// mAccumulator can be greater than mFrameDuration so only get the
		// time remaining until the next frame
		dtime_t remaining_time = mAccumulator % mFrameDuration;
		return (float)(double(remaining_time) / mFrameDuration);
	}

private:
	void				(*mTask)();
	const int			mMaxCount;
	const dtime_t		mFrameDuration;
	dtime_t				mAccumulator;
	dtime_t				mFrameStartTime;
	dtime_t				mPreviousFrameStartTime;
};

static TaskScheduler* simulation_scheduler;
static TaskScheduler* display_scheduler;

//
// D_InitTaskSchedulers
//
// Checks for external changes to the rate for the simulation and display
// tasks and instantiates the appropriate TaskSchedulers.
//
static void D_InitTaskSchedulers(void (*sim_func)(), void(*display_func)())
{
	bool capped_simulation = !timingdemo;
	bool capped_display = !timingdemo && capfps;

	static bool previous_capped_simulation = !capped_simulation;
	static bool previous_capped_display = !capped_display;
	static float previous_maxfps = -1.0f;

	if (capped_simulation != previous_capped_simulation)
	{
		previous_capped_simulation = capped_simulation;

		delete simulation_scheduler;

		if (capped_simulation)
			simulation_scheduler = new CappedTaskScheduler(sim_func, TICRATE, 4);
		else
			simulation_scheduler = new UncappedTaskScheduler(sim_func);
	}

	if (capped_display != previous_capped_display || maxfps != previous_maxfps)
	{
		previous_capped_display = capped_display;
		previous_maxfps = maxfps;

		delete display_scheduler;

		if (capped_display)
			display_scheduler = new CappedTaskScheduler(display_func, maxfps, 1);
		else
			display_scheduler = new UncappedTaskScheduler(display_func);
	}
}

void STACK_ARGS D_ClearTaskSchedulers()
{
	delete simulation_scheduler;
	delete display_scheduler;
	simulation_scheduler = NULL;
	display_scheduler = NULL;
}

//
// D_RunTics
//
// The core of the main game loop.
// This loop allows the game simulation timing to be decoupled from the display
// timing. If the the user selects a capped framerate and isn't using the
// -timedemo parameter, both the simulation and display functions will be called
// TICRATE times a second. If the framerate is uncapped, the simulation function
// will still be called TICRATE times a second but the display function will
// be called as often as possible. After each iteration through the loop,
// the program yields briefly to the operating system.
//
void D_RunTics(void (*sim_func)(), void(*display_func)())
{
	D_InitTaskSchedulers(sim_func, display_func);

	simulation_scheduler->run();

#ifdef CLIENT_APP
	// Use linear interpolation for rendering entities if the display
	// framerate is not synced with the simulation frequency.
	// Ch0wW : if you experience a spinning effect while trying to pause the frame, 
	// don't forget to add your condition here.
	if ((maxfps == TICRATE && capfps)
		|| timingdemo || paused || step_mode
		|| ((menuactive || ConsoleState == c_down || ConsoleState == c_falling) && !network_game && !demoplayback))
		render_lerp_amount = FRACUNIT;
	else
		render_lerp_amount = simulation_scheduler->getRemainder() * FRACUNIT;
#endif

	display_scheduler->run();

	if (timingdemo)
		return;

	// Sleep until the next scheduled task.
	dtime_t simulation_wake_time = simulation_scheduler->getNextTime();
	dtime_t display_wake_time = display_scheduler->getNextTime();
	dtime_t wake_time = std::min<dtime_t>(simulation_wake_time, display_wake_time);

	const dtime_t max_sleep_amount = 1000LL * 1000LL;	// 1ms

	// Sleep in 1ms increments until the next scheduled task
	for (dtime_t now = I_GetTime(); wake_time > now; now = I_GetTime())
	{
		dtime_t sleep_amount = std::min<dtime_t>(max_sleep_amount, wake_time - now);
		I_Sleep(sleep_amount);
	}
}

VERSION_CONTROL (d_main_cpp, "$Id$")
