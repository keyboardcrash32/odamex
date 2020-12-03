// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
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
//   Common gametype-related functionality.
//
//-----------------------------------------------------------------------------

#include "g_gametype.h"

#include "c_dispatch.h"
#include "cmdlib.h"
#include "g_levelstate.h"
#include "m_wdlstats.h"
#include "msg_server.h"

EXTERN_CVAR(g_lives)
EXTERN_CVAR(g_sides)
EXTERN_CVAR(g_roundlimit)
EXTERN_CVAR(g_rounds)
EXTERN_CVAR(g_winlimit)
EXTERN_CVAR(sv_fraglimit)
EXTERN_CVAR(sv_gametype)
EXTERN_CVAR(sv_maxplayers)
EXTERN_CVAR(sv_maxplayersperteam)
EXTERN_CVAR(sv_nomonsters)
EXTERN_CVAR(sv_scorelimit)
EXTERN_CVAR(sv_teamsinplay)
EXTERN_CVAR(sv_timelimit)

/**
 * @brief Returns a string containing the name of the gametype.
 *
 * @return String with the gametype name.
 */
const std::string& G_GametypeName()
{
	static std::string name;
	if (sv_gametype == GM_COOP && g_lives)
		name = "Survival";
	else if (sv_gametype == GM_COOP && ::multiplayer)
		name = "Cooperative";
	else if (sv_gametype == GM_COOP)
		name = "Single-player";
	else if (sv_gametype == GM_DM && g_lives)
		name = "Last Marine Standing";
	else if (sv_gametype == GM_DM && sv_maxplayers <= 2)
		name = "Duel";
	else if (sv_gametype == GM_DM)
		name = "Deathmatch";
	else if (sv_gametype == GM_TEAMDM && g_lives)
		name = "Team Last Marine Standing";
	else if (sv_gametype == GM_TEAMDM)
		name = "Team Deathmatch";
	else if (sv_gametype == GM_CTF && g_lives)
		name = "LMS Capture The Flag";
	else if (sv_gametype == GM_CTF)
		name = "Capture The Flag";
	return name;
}

/**
 * @brief Check if the round should be allowed to end.
 */
bool G_CanEndGame()
{
	return ::levelstate.getState() == LevelState::INGAME;
}

/**
 * @brief Check if the player should be able to fire their weapon.
 */
bool G_CanFireWeapon()
{
	return ::levelstate.getState() == LevelState::INGAME ||
	       ::levelstate.getState() == LevelState::WARMUP;
}

/**
 * @brief Check if a player should be allowed to join the game in the middle
 *        of play.
 */
JoinResult G_CanJoinGame()
{
	// Make sure our usual conditions are satisfied.
	JoinResult join = G_CanJoinGameStart();
	if (join != JOIN_OK)
		return join;

	// Join timer checks.
	if (g_lives && ::levelstate.getState() == LevelState::INGAME)
	{
		if (::levelstate.getJoinTimeLeft() <= 0)
			return JOIN_JOINTIMER;
	}

	return JOIN_OK;
}

/**
 * @brief Check if a player should be allowed to join the game.
 *
 *        This function ignores the join timer which is usually a bad idea,
 *        but is vital for the join queue to work.
 */
JoinResult G_CanJoinGameStart()
{
	// Can't join anytime that's not ingame.
	if (::gamestate != GS_LEVEL)
		return JOIN_ENDGAME;

	// Can't join during the endgame.
	if (::levelstate.getState() == LevelState::ENDGAME_COUNTDOWN)
		return JOIN_ENDGAME;

	// Too many people in the game.
	if (P_NumPlayersInGame() >= sv_maxplayers)
		return JOIN_GAMEFULL;

	// Too many people on either team.
	if (G_IsTeamGame() && sv_maxplayersperteam)
	{
		int teamplayers = sv_maxplayersperteam * sv_teamsinplay;
		if (P_NumPlayersInGame() >= teamplayers)
			return JOIN_GAMEFULL;
	}

	return JOIN_OK;
}

/**
 * @brief Check if a player's lives should be allowed to change.
 */
bool G_CanLivesChange()
{
	return ::levelstate.getState() == LevelState::INGAME;
}

/**
 * @brief Check to see if we're allowed to pick up an objective.
 *
 * @param team Team that the objective belongs to.
 */
bool G_CanPickupObjective(team_t team)
{
	if (g_sides && team != ::levelstate.getDefendingTeam())
	{
		return false;
	}
	return ::levelstate.getState() == LevelState::INGAME;
}

/**
 * @brief Check to see if we should allow players to toggle their ready state.
 */
bool G_CanReadyToggle()
{
	return ::levelstate.getState() != LevelState::WARMUP_FORCED_COUNTDOWN &&
	       ::levelstate.getState() != LevelState::ENDGAME_COUNTDOWN;
}

/**
 * @brief Check if a non-win score should be allowed to change.
 */
bool G_CanScoreChange()
{
	return ::levelstate.getState() == LevelState::INGAME;
}

/**
 * @brief Check if obituaries are allowed to be shown.
 */
bool G_CanShowObituary()
{
	return ::levelstate.getState() == LevelState::INGAME;
}

/**
 * @brief Check if we're allowed to "tick" gameplay systems.
 */
bool G_CanTickGameplay()
{
	return ::levelstate.getState() == LevelState::WARMUP ||
	       ::levelstate.getState() == LevelState::INGAME;
}

/**
 * @brief Check if the passed team is on defense.
 * 
 * @param team Team to check.
 * @return True if the team is on defense, or if sides aren't enabled.
 */
bool G_IsDefendingTeam(team_t team)
{
	return g_sides == false || ::levelstate.getDefendingTeam() == team;
}

/**
 * @brief Check if the gametype is purely player versus environment, where
 *        the players win and lose as a whole.
 */
bool G_IsCoopGame()
{
	return sv_gametype == GM_COOP;
}

/**
 * @brief Check if the gametype typically has a single winner.
 */
bool G_IsFFAGame()
{
	return sv_gametype == GM_DM;
}

/**
 * @brief Check if the gametype has teams and players can win as a team.
 */
bool G_IsTeamGame()
{
	return sv_gametype == GM_TEAMDM || sv_gametype == GM_CTF;
}

/**
 * @brief Check if the game consists of multiple rounds.
 */
bool G_IsRoundsGame()
{
	return (sv_gametype == GM_DM || sv_gametype == GM_TEAMDM || sv_gametype == GM_CTF) && g_rounds == true;
}

/**
 * @brief Check if the game uses lives.
*/
bool G_IsLivesGame()
{
	return g_lives > 0;
}

/**
 * @brief Check if the gametype uses winlimit.
 */
bool G_UsesWinlimit()
{
	return g_rounds;
}

/**
 * @brief Check if the gametype uses roundlimit.
 */
bool G_UsesRoundlimit()
{
	return g_rounds;
}

/**
 * @brief Check if the gametype uses scorelimit.
 */
bool G_UsesScorelimit()
{
	return sv_gametype == GM_CTF;
}

/**
 * @brief Check if the gametype uses fraglimit.
 */
bool G_UsesFraglimit()
{
	return sv_gametype == GM_DM || sv_gametype == GM_TEAMDM;
}

/**
 * @brief Calculate the tic that the level ends on.
 */
int G_EndingTic()
{
	return sv_timelimit * 60 * TICRATE + 1;
}

/**
 * @brief Assert that we have enough players to continue the game, otherwise
 *        end the game or reset it.
 */
void G_AssertValidPlayerCount()
{
	if (!::serverside)
		return;

	// In warmup player counts don't matter, and at the end of the game the
	// check is useless.
	if (::levelstate.getState() == LevelState::WARMUP ||
	    ::levelstate.getState() == LevelState::ENDGAME_COUNTDOWN)
		return;

	// Cooperative game modes have slightly different logic.
	if (::sv_gametype == GM_COOP && P_NumPlayersInGame() == 0)
	{
		if (::g_lives)
		{
			// Survival modes cannot function with no players in the gamne,
			// so the level must be reset at this point.
			::levelstate.reset();
		}
		else
		{
			// Normal coop is pretty casual so in this case we leave it to
			// sv_emptyreset to reset the level for us.
		}
		return;
	}

	bool valid = true;

	if (P_NumPlayersInGame() == 0)
	{
		// Literally nobody is in the game.
		::levelstate.setWinner(WinInfo::WIN_UNKNOWN, 0);
		valid = false;
	}
	else if (sv_gametype == GM_DM)
	{
		// End the game if the player is by themselves.
		PlayerResults pr = PlayerQuery().execute();
		if (pr.count == 1)
		{
			::levelstate.setWinner(WinInfo::WIN_PLAYER, pr.players.front()->id);
			valid = false;
		}
	}
	else if (G_IsTeamGame())
	{
		// End the game if there's only one team with players in it.
		int hasplayers = TEAM_NONE;
		PlayerResults pr = PlayerQuery().execute();
		for (int i = 0; i < NUMTEAMS; i++)
		{
			if (pr.teamTotal[i] > 0)
			{
				// Does more than one team has players?  If so, the game continues.
				if (hasplayers != TEAM_NONE)
					return;
				hasplayers = i;
			}
		}

		if (hasplayers != TEAM_NONE)
			::levelstate.setWinner(WinInfo::WIN_TEAM, hasplayers);
		else
			::levelstate.setWinner(WinInfo::WIN_UNKNOWN, 0);
		valid = false;
	}

	// If we haven't signaled an invalid state by now, we're cool.
	if (valid == true)
		return;

	// If we're in warmup, back out before we start.  Otherwise, end the game.
	if (::levelstate.getState() == LevelState::WARMUP_COUNTDOWN ||
	    ::levelstate.getState() == LevelState::WARMUP_FORCED_COUNTDOWN)
		::levelstate.reset();
	else
		::levelstate.endGame();
}

static void GiveWins(player_t& player, int wins)
{
	player.roundwins += wins;

	// If we're not a server, we're done.
	if (!::serverside || ::clientside)
		return;

	// Send information about the new round wins to all players.
	for (Players::iterator it = ::players.begin(); it != ::players.end(); ++it)
	{
		if (!it->ingame())
			continue;
		SVC_PlayerMembers(it->client.netbuf, player, SVC_PM_SCORE);
	}
}

static void GiveTeamWins(team_t team, int wins)
{
	TeamInfo* info = GetTeamInfo(team);
	if (info->Team >= NUMTEAMS)
		return;
	info->RoundWins += wins;

	// If we're not a server, we're done.
	if (!::serverside || ::clientside)
		return;

	// Send information about the new team round wins to all players.
	for (Players::iterator it = ::players.begin(); it != ::players.end(); ++it)
	{
		if (!it->ingame())
			continue;
		SVC_TeamMembers(it->client.netbuf, team);
	}
}

/**
 * @brief Check if timelimit should end the game.
 */
void G_TimeCheckEndGame()
{
	if (!::serverside || !G_CanEndGame())
		return;

	if (sv_timelimit <= 0.0 || level.flags & LEVEL_LOBBYSPECIAL) // no time limit in lobby
		return;

	// Check to see if we have any time left.
	if (G_EndingTic() > level.time)
		return;

	// If nobody is in the game, just end the game and move on.
	if (P_NumPlayersInGame() == 0)
	{
		::levelstate.setWinner(WinInfo::WIN_UNKNOWN, 0);
		::levelstate.endRound();
		return;
	}

	if (sv_gametype == GM_DM)
	{
		PlayerResults pr = PlayerQuery().sortFrags().filterSortMax().execute();
		if (pr.count == 0)
		{
			// Something has seriously gone sideways...
			::levelstate.setWinner(WinInfo::WIN_UNKNOWN, 0);
			::levelstate.endRound();
			return;
		}
		else if (pr.count >= 2)
		{
			SV_BroadcastPrintf(PRINT_HIGH, "Time limit hit. Game is a draw!\n");
			::levelstate.setWinner(WinInfo::WIN_DRAW, 0);
		}
		else
		{
			SV_BroadcastPrintf(PRINT_HIGH, "Time limit hit. Game won by %s!\n",
			                   pr.players.front()->userinfo.netname.c_str());
			::levelstate.setWinner(WinInfo::WIN_PLAYER, pr.players.front()->id);
		}
	}
	else if (G_IsTeamGame())
	{
		if (g_sides)
		{
			// Defense always wins in the event of a timeout.
			const TeamInfo& ti = *GetTeamInfo(::levelstate.getDefendingTeam());
			SV_BroadcastPrintf(PRINT_HIGH, "Time limit hit. %s team wins!\n",
			                   ti.ColorStringUpper.c_str());
			::levelstate.setWinner(WinInfo::WIN_TEAM, ti.Team);
		}
		else
		{
			TeamsView tv = TeamQuery().sortScore().filterSortMax().execute();

			if (tv.size() != 1)
			{
				SV_BroadcastPrintf(PRINT_HIGH, "Time limit hit. Game is a draw!\n");
				::levelstate.setWinner(WinInfo::WIN_DRAW, 0);
			}
			else
			{
				SV_BroadcastPrintf(PRINT_HIGH, "Time limit hit. %s team wins!\n",
				                   tv.front()->ColorStringUpper.c_str());
				::levelstate.setWinner(WinInfo::WIN_TEAM, tv.front()->Team);
			}
		}
	}

	::levelstate.endRound();
	M_CommitWDLLog();
}

/**
 * @brief Check for an endgame condition on individual frags.
 */
void G_FragsCheckEndGame()
{
	if (!::serverside || !G_CanEndGame())
		return;

	if (sv_fraglimit <= 0.0)
		return;

	PlayerResults pr = PlayerQuery().sortFrags().filterSortMax().execute();
	if (pr.count > 0)
	{
		player_t* top = pr.players.front();
		if (top->fragcount >= sv_fraglimit)
		{
			GiveWins(*top, 1);

			// [ML] 04/4/06: Added !sv_fragexitswitch
			SV_BroadcastPrintf(PRINT_HIGH, "Frag limit hit. Game won by %s!\n",
			                   top->userinfo.netname.c_str());
			::levelstate.setWinner(WinInfo::WIN_PLAYER, top->id);
			::levelstate.endRound();
		}
	}
}

/**
 * @brief Check for an endgame condition on team frags.
 */
void G_TeamFragsCheckEndGame()
{
	if (!::serverside || !G_CanEndGame())
		return;

	if (sv_fraglimit <= 0.0)
		return;

	TeamsView tv = TeamQuery().sortScore().filterSortMax().execute();
	if (!tv.empty())
	{
		TeamInfo* team = tv.front();
		if (team->Points >= sv_fraglimit)
		{
			GiveTeamWins(team->Team, 1);
			SV_BroadcastPrintf(PRINT_HIGH, "Frag limit hit. %s team wins!\n",
			                   team->ColorString.c_str());
			::levelstate.setWinner(WinInfo::WIN_TEAM, team->Team);
			::levelstate.endRound();
			return;
		}
	}
}

/**
 * @brief Check for an endgame condition on team score.
 */
void G_TeamScoreCheckEndGame()
{
	if (!::serverside || !G_CanEndGame())
		return;

	if (sv_scorelimit <= 0.0)
		return;

	TeamsView tv = TeamQuery().sortScore().filterSortMax().execute();
	if (!tv.empty())
	{
		TeamInfo* team = tv.front();
		if (team->Points >= sv_scorelimit)
		{
			GiveTeamWins(team->Team, 1);
			SV_BroadcastPrintf(PRINT_HIGH, "Score limit hit. %s team wins!\n",
			                   team->ColorString.c_str());
			::levelstate.setWinner(WinInfo::WIN_TEAM, team->Team);
			::levelstate.endRound();
			M_CommitWDLLog();
			return;
		}
	}
}

/**
 * @brief Check to see if we should end the game on lives.
 */
void G_LivesCheckEndGame()
{
	if (!::serverside)
		return;

	if (!g_lives || !G_CanEndGame())
		return;

	if (sv_gametype == GM_COOP)
	{
		// Everybody losing their lives in coop is a failure.
		PlayerResults pr = PlayerQuery().hasLives().execute();
		if (pr.count == 0)
		{
			SV_BroadcastPrintf(PRINT_HIGH, "All players have run out of lives.\n");
			::levelstate.setWinner(WinInfo::WIN_NOBODY, 0);
			::levelstate.endRound();
		}
	}
	else if (sv_gametype == GM_DM)
	{
		// One person being alive is success, nobody alive is a draw.
		PlayerResults pr = PlayerQuery().hasLives().execute();
		if (pr.count == 0)
		{
			SV_BroadcastPrintf(PRINT_HIGH, "All players have run out of lives.\n");
			::levelstate.setWinner(WinInfo::WIN_DRAW, 0);
			::levelstate.endRound();
		}
		else if (pr.count == 1)
		{
			GiveWins(*pr.players.front(), 1);
			SV_BroadcastPrintf(PRINT_HIGH, "%s wins as the last player standing!\n",
			                   pr.players.front()->userinfo.netname.c_str());
			::levelstate.setWinner(WinInfo::WIN_PLAYER, pr.players.front()->id);
			::levelstate.endRound();
		}

		// Nobody won the game yet - keep going.
	}
	else if (G_IsTeamGame())
	{
		// If someone has configured TeamDM improperly, just don't do anything.
		int teamsinplay = sv_teamsinplay.asInt();
		if (teamsinplay < 1 || teamsinplay > NUMTEAMS)
			return;

		// One person alive on a single team is success, nobody alive is a draw.
		PlayerResults pr = PlayerQuery().hasLives().execute();
		int aliveteams = 0;
		for (int i = 0; i < teamsinplay; i++)
		{
			if (pr.teamCount[i] > 0)
				aliveteams += 1;
		}

		// [AM] This end-of-game logic branch is necessary becuase otherwise
		//      going for objectives in CTF would never be worth it.  However,
		//      side-mode needs a special-case because otherwise in games
		//      with scorelimit > 1 the offense can just score once and
		//      turtle.
		if (aliveteams <= 1 && sv_gametype == GM_CTF && g_sides == false)
		{
			const char* teams = aliveteams == 1 ? "one team" : "no teams";

			TeamsView tv = TeamQuery().filterSortMax().sortScore().execute();
			if (tv.size() == 1)
			{
				GiveTeamWins(tv.front()->Team, 1);
				SV_BroadcastPrintf(
				    PRINT_HIGH,
				    "%s team wins for having the highest score with %s left!\n",
				    tv.front()->ColorString.c_str(), teams);
				::levelstate.setWinner(WinInfo::WIN_TEAM, tv.front()->Team);
				::levelstate.endRound();
			}
			else
			{
				SV_BroadcastPrintf(PRINT_HIGH,
				                   "Score is tied with with %s left. Game is a draw!\n",
				                   teams);
				::levelstate.setWinner(WinInfo::WIN_DRAW, 0);
				::levelstate.endRound();
			}
		}

		if (aliveteams == 0 || pr.count == 0)
		{
			SV_BroadcastPrintf(PRINT_HIGH, "All teams have run out of lives.\n");
			::levelstate.setWinner(WinInfo::WIN_DRAW, 0);
			::levelstate.endRound();
		}
		else if (aliveteams == 1)
		{
			team_t team = pr.players.front()->userinfo.team;
			GiveTeamWins(team, 1);
			SV_BroadcastPrintf(PRINT_HIGH, "%s team wins as the last team standing!\n",
			                   GetTeamInfo(team)->ColorString.c_str());
			::levelstate.setWinner(WinInfo::WIN_TEAM, team);
			::levelstate.endRound();
		}

		// Nobody won the game yet - keep going.
	}
}

/**
 * @brief Check to see if we should end the game on won rounds...or just rounds total.
 *        Note that this function does not actually end the game - that's the job
 *        of the caller.
 */
bool G_RoundsShouldEndGame()
{
	if (!::serverside)
		return false;

	if (!g_roundlimit && !g_winlimit)
		return false;

	// Coop doesn't have rounds to speak of - though perhaps in the future
	// rounds might be used to limit the number of tries a map is attempted.
	if (sv_gametype == GM_COOP)
		return true;

	if (sv_gametype == GM_DM)
	{
		PlayerResults pr = PlayerQuery().sortWins().filterSortMax().execute();
		if (pr.count == 1 && g_winlimit && pr.players.front()->roundwins >= g_winlimit)
		{
			SV_BroadcastPrintf(PRINT_HIGH, "Win limit hit. Match won by %s!\n",
			                   pr.players.front()->userinfo.netname.c_str());
			::levelstate.setWinner(WinInfo::WIN_PLAYER, pr.players.front()->id);
			return true;
		}
		else if (pr.count == 1 && g_roundlimit && ::levelstate.getRound() >= g_roundlimit)
		{
			SV_BroadcastPrintf(PRINT_HIGH, "Round limit hit. Match won by %s!\n",
			                   pr.players.front()->userinfo.netname.c_str());
			::levelstate.setWinner(WinInfo::WIN_PLAYER, pr.players.front()->id);
			return true;
		}
		else if (g_roundlimit && ::levelstate.getRound() >= g_roundlimit)
		{
			SV_BroadcastPrintf(PRINT_HIGH, "Round limit hit. Game is a draw!\n");
			::levelstate.setWinner(WinInfo::WIN_DRAW, 0);
			return true;
		}
	}
	else if (G_IsTeamGame())
	{
		for (size_t i = 0; i < NUMTEAMS; i++)
		{
			TeamInfo* team = GetTeamInfo((team_t)i);
			if (team->RoundWins >= g_winlimit)
			{
				SV_BroadcastPrintf(PRINT_HIGH, "Win limit hit. %s team wins!\n",
				                   team->ColorString.c_str());
				::levelstate.setWinner(WinInfo::WIN_TEAM, team->Team);
				return true;
			}
		}
	}

	return false;
}

typedef std::vector<std::string> StringList;

static void SurvivalHelp()
{
	Printf(PRINT_HIGH,
	       "survival - Configures some settings for a basic game of Survival\n\n"
	       "Flags:\n"
	       "  lives <LIVES>\n"
	       "    Set number of player lives to LIVES.  Defaults to 3.\n");
}

BEGIN_COMMAND(survival)
{
	if (argc < 2)
	{
		SurvivalHelp();
		return;
	}

	std::string buffer;
	StringList params;

	params.push_back("sv_gametype 0");
	params.push_back("sv_skill 4");
	params.push_back("sv_nomonsters 0");
	params.push_back("sv_forcerespawn 1");
	params.push_back("g_lives_jointimer 30");
	params.push_back("g_rounds 0");

	bool lives = false;
	StringList args = VectorArgs(argc, argv);
	for (;;)
	{
		if (args.empty())
		{
			// Ran out of parameters to munch.
			break;
		}
		else if (args.size() == 1)
		{
			// All params take a second one.
			Printf(PRINT_HIGH, "Missing argument for \"%s\"\n", args.front().c_str());
			SurvivalHelp();
			return;
		}

		const std::string& cmd = args.at(0);
		if (iequals(cmd, "lives"))
		{
			// Set lives.
			const std::string& value = args.at(1);
			StrFormat(buffer, "g_lives %s", value.c_str());
			params.push_back(buffer);
			lives = true;
		}
		else
		{
			// Unknown flag.
			Printf(PRINT_HIGH, "Unknown flag \"%s\"\n", args.front().c_str());
			SurvivalHelp();
			return;
		}

		// Shift param and argument off the front.
		args.erase(args.begin(), args.begin() + 2);
	}

	// Defaults.
	if (lives == false)
	{
		params.push_back("g_lives 3");
	}

	std::string config = JoinStrings(params, "; ");
	Printf(PRINT_HIGH, "Configuring Survival...\n%s\n", config.c_str());
	AddCommandString(config.c_str());
	return;
}
END_COMMAND(survival)

static void LMSHelp()
{
	Printf(
	    PRINT_HIGH,
	    "lms - Configures some settings for a basic game of Last Marine Standing\n\n"
	    "Flags:\n"
	    "  lives <LIVES>\n"
	    "    Set number of player lives per round to LIVES.  Defaults to 1.\n"
	    "  wins <WINS>\n"
	    "    Set number of round wins needed to win the game to WINS.  Defaults to 5.\n");
}

BEGIN_COMMAND(lms)
{
	if (argc < 2)
	{
		LMSHelp();
		return;
	}

	std::string buffer;
	StringList params;
	params.push_back("sv_gametype 1");
	params.push_back("sv_skill 5");
	params.push_back("sv_nomonsters 1");
	params.push_back("sv_forcerespawn 1");
	params.push_back("g_lives_jointimer 0");
	params.push_back("g_rounds 1");

	bool lives = false, wins = false;
	StringList args = VectorArgs(argc, argv);
	for (;;)
	{
		if (args.empty())
		{
			// Ran out of parameters to munch.
			break;
		}
		else if (args.size() == 1)
		{
			// All params take a second one.
			Printf(PRINT_HIGH, "Missing argument for \"%s\"\n", args.front().c_str());
			LMSHelp();
			return;
		}

		const std::string& cmd = args.at(0);
		if (iequals(cmd, "lives"))
		{
			// Set lives.
			const std::string& value = args.at(1);
			StrFormat(buffer, "g_lives %s", value.c_str());
			params.push_back(buffer);
			lives = true;
		}
		else if (iequals(cmd, "wins"))
		{
			// Set lives.
			const std::string& value = args.at(1);
			StrFormat(buffer, "g_winlimit %s", value.c_str());
			params.push_back(buffer);
			wins = true;
		}
		else
		{
			// Unknown flag.
			Printf(PRINT_HIGH, "Unknown flag \"%s\"\n", args.front().c_str());
			LMSHelp();
			return;
		}

		// Shift param and argument off the front.
		args.erase(args.begin(), args.begin() + 2);
	}

	// Defaults.
	if (lives == false)
	{
		params.push_back("g_lives 1");
	}
	if (wins == false)
	{
		params.push_back("g_winlimit 5");
	}

	std::string config = JoinStrings(params, "; ");
	Printf(PRINT_HIGH, "Configuring Last Marine Standing...\n%s\n", config.c_str());
	AddCommandString(config.c_str());
	return;
}
END_COMMAND(lms)

static void TLMSHelp()
{
	Printf(
	    PRINT_HIGH,
	    "tlms - Configures some settings for a basic game of Team Last Marine "
	    "Standing\n\n"
	    "Flags:\n"
	    "  lives <LIVES>\n"
	    "    Set number of player lives per round to LIVES.  Defaults to 1.\n"
	    "  teams <TEAMS>\n"
	    "    Set number of teams in play to TEAMS.  Defaults to 2.\n"
	    "  wins <WINS>\n"
	    "    Set number of round wins needed to win the game to WINS.  Defaults to 5.\n");
}

BEGIN_COMMAND(tlms)
{
	if (argc < 2)
	{
		TLMSHelp();
		return;
	}

	std::string buffer;
	StringList params;
	params.push_back("sv_gametype 2");
	params.push_back("sv_skill 5");
	params.push_back("sv_nomonsters 1");
	params.push_back("sv_forcerespawn 1");
	params.push_back("g_lives_jointimer 0");
	params.push_back("g_rounds 1");
	params.push_back("sv_friendlyfire 0");

	bool lives = false, teams = false, wins = false;
	StringList args = VectorArgs(argc, argv);
	for (;;)
	{
		if (args.empty())
		{
			// Ran out of parameters to munch.
			break;
		}
		else if (args.size() == 1)
		{
			// All params take a second one.
			Printf(PRINT_HIGH, "Missing argument for \"%s\"\n", args.front().c_str());
			TLMSHelp();
			return;
		}

		const std::string& cmd = args.at(0);
		if (iequals(cmd, "lives"))
		{
			// Set lives.
			const std::string& value = args.at(1);
			StrFormat(buffer, "g_lives %s", value.c_str());
			params.push_back(buffer);
			lives = true;
		}
		else if (iequals(cmd, "teams"))
		{
			// Set teams in play.
			const std::string& value = args.at(1);
			StrFormat(buffer, "sv_teamsinplay %s", value.c_str());
			params.push_back(buffer);
			teams = true;
		}
		else if (iequals(cmd, "wins"))
		{
			// Set winlimit.
			const std::string& value = args.at(1);
			StrFormat(buffer, "g_winlimit %s", value.c_str());
			params.push_back(buffer);
			wins = true;
		}
		else
		{
			// Unknown flag.
			Printf(PRINT_HIGH, "Unknown flag \"%s\"\n", args.front().c_str());
			TLMSHelp();
			return;
		}

		// Shift param and argument off the front.
		args.erase(args.begin(), args.begin() + 2);
	}

	// Defaults.
	if (lives == false)
	{
		params.push_back("g_lives 1");
	}
	if (teams == false)
	{
		params.push_back("sv_teamsinplay 2");
	}
	if (wins == false)
	{
		params.push_back("g_winlimit 5");
	}

	std::string config = JoinStrings(params, "; ");
	Printf(PRINT_HIGH, "Configuring Team Last Marine Standing...\n%s\n", config.c_str());
	AddCommandString(config.c_str());
	return;
}
END_COMMAND(tlms)
