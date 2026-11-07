/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// teamplay_gamerules.cpp
//
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "game.h"
#include "UserMessages.h"
#include "flag.h"

//TODO: these should be members of CHalfLifeTeamplay
static char team_names[MAX_TEAMS][MAX_TEAMNAME_LENGTH];
static int team_scores[MAX_TEAMS];
static int num_teams = 0;

// Rocket Crowbar 1.9 CTF state
int m_blCTF;
float m_flInitCTF;
int num_flags = 0;

// orig 0x220760. Rocket Crowbar turns CTF on here (mp_ctf 1, a team list of 2-6 teams); Think
// places the flags half a second later.
CHalfLifeTeamplay::CHalfLifeTeamplay()
{
	m_DisableDeathMessages = false;
	m_DisableDeathPenalty = false;

	memset(team_names, 0, sizeof(team_names));
	memset(team_scores, 0, sizeof(team_scores));
	num_teams = 0;

	// Copy over the team from the server config
	m_szTeamList[0] = 0;

	// Cache this because the team code doesn't want to deal with changing this in the middle of a game
	strncpy(m_szTeamList, teamlist.string, TEAMPLAY_TEAMLISTLENGTH);

	edict_t* pWorld = CWorld::World->edict();
	if (pWorld && !FStringNull(pWorld->v.team))
	{
		if (0 != teamoverride.value)
		{
			const char* pTeamList = STRING(pWorld->v.team);
			if (pTeamList && 0 != strlen(pTeamList))
			{
				strncpy(m_szTeamList, pTeamList, TEAMPLAY_TEAMLISTLENGTH);
			}
		}
	}
	// Has the server set teams
	if (0 != strlen(m_szTeamList))
		m_teamLimit = true;
	else
		m_teamLimit = false;

	RecountTeams();

	if (CVAR_GET_FLOAT("mp_ctf") == 1 && m_teamLimit && num_teams < 7)
	{
		m_blCTF = 1;
		m_flInitCTF = gpGlobals->time + 0.5;
	}
}

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;

// orig 0x220910. Rocket Crowbar places the CTF flags once m_flInitCTF passes. halflife-updated's
// voice manager update and mp_fragsleft/mp_timeleft cvars are kept (the original has neither).
void CHalfLifeTeamplay::Think()
{
	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining = 0;

	g_VoiceGameMgr.Update(gpGlobals->frametime);

	if (g_fGameOver) // someone else quit the game already
	{
		CHalfLifeMultiplay::Think();
		return;
	}

	float flTimeLimit = CVAR_GET_FLOAT("mp_timelimit") * 60;

	time_remaining = (int)(0 != flTimeLimit ? (flTimeLimit - gpGlobals->time) : 0);

	if (flTimeLimit != 0 && gpGlobals->time >= flTimeLimit)
	{
		GoToIntermission();
		return;
	}

	float flFragLimit = fraglimit.value;
	if (0 != flFragLimit)
	{
		int bestfrags = 9999;
		int remain;

		// check if any team is over the frag limit
		for (int i = 0; i < num_teams; i++)
		{
			if (team_scores[i] >= flFragLimit)
			{
				GoToIntermission();
				return;
			}

			remain = flFragLimit - team_scores[i];
			if (remain < bestfrags)
			{
				bestfrags = remain;
			}
		}
		frags_remaining = bestfrags;
	}

	// place the flags, at the spawns SetFlagSpawns picks; it can turn CTF back off
	if (m_flInitCTF < gpGlobals->time && m_flInitCTF != 0 && m_blCTF == 1)
	{
		RecountTeams();
		SetFlagSpawns(num_flags);
		if (0 != m_blCTF)
		{
			for (int i = 1; i <= num_flags; i++)
				CBaseFlag::CreateFlag(m_vecFlagOrigin[i - 1], m_vecFlagAngles[i - 1], i);
			m_flInitCTF = 0;
		}
	}

	// Updates when frags change
	if (frags_remaining != last_frags)
	{
		g_engfuncs.pfnCvar_DirectSet(&fragsleft, UTIL_VarArgs("%i", frags_remaining));
	}

	// Updates once per second
	if (timeleft.value != last_time)
	{
		g_engfuncs.pfnCvar_DirectSet(&timeleft, UTIL_VarArgs("%i", time_remaining));
	}

	last_frags = frags_remaining;
	last_time = time_remaining;
}

//=========================================================
// ClientCommand
// the user has typed a command which is unrecognized by everything else;
// this check to see if the gamerules knows anything about the command
//=========================================================
// orig 0x220ac0. Rocket Crowbar adds "flagstatus" in CTF.
bool CHalfLifeTeamplay::ClientCommand(CBasePlayer* pPlayer, const char* pcmd)
{
	// halflife-updated: the voice manager's commands (vban, VModEnable)
	if (g_VoiceGameMgr.ClientCommand(pPlayer, pcmd))
		return true;

	if (FStrEq(pcmd, "menuselect"))
	{
		if (CMD_ARGC() < 2)
			return true;

		int slot = atoi(CMD_ARGV(1));

		// select the item from the current menu

		return true;
	}
	else if (FStrEq(pcmd, "flagstatus") && 0 != m_blCTF)
	{
		ReportFlagStatus(pPlayer);
		return true;
	}

	return false;
}

// orig 0x220c40
void CHalfLifeTeamplay::UpdateGameMode(CBasePlayer* pPlayer)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgGameMode, NULL, pPlayer->edict());
	WRITE_BYTE(1); // game mode teamplay
	MESSAGE_END();
}


// orig 0x220ca0
const char* CHalfLifeTeamplay::SetDefaultPlayerTeam(CBasePlayer* pPlayer)
{
	// copy out the team name from the model
	char* mdls = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model");
	strncpy(pPlayer->m_szTeamName, mdls, TEAM_NAME_LENGTH);

	RecountTeams();

	// update the current player of the team he is joining
	if (pPlayer->m_szTeamName[0] == '\0' || !IsValidTeam(pPlayer->m_szTeamName) || 0 != defaultteam.value)
	{
		const char* pTeamName = NULL;

		if (0 != defaultteam.value)
		{
			pTeamName = team_names[0];
		}
		else
		{
			pTeamName = TeamWithFewestPlayers();
		}
		strncpy(pPlayer->m_szTeamName, pTeamName, TEAM_NAME_LENGTH);
	}

	return pPlayer->m_szTeamName;
}


//=========================================================
// InitHUD
//=========================================================
// orig 0x220d90. The original sends no TeamNames message (halflife-updated does); Rocket
// Crowbar names the team's flag colour in CTF and sends the CTF scores.
void CHalfLifeTeamplay::InitHUD(CBasePlayer* pPlayer)
{
	int i;

	SetDefaultPlayerTeam(pPlayer);
	CHalfLifeMultiplay::InitHUD(pPlayer);

	RecountTeams();

	char* mdls = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model");
	// update the current player of the team he is joining
	char text[1024];
	if (0 == strcmp(mdls, pPlayer->m_szTeamName))
	{
		if (0 != m_blCTF)
			sprintf(text, "* you are on team \'%s\' (%s)\n", pPlayer->m_szTeamName, GetFlagTeamColor(GetTeamIndex(pPlayer->m_szTeamName) + 1));
		else
			sprintf(text, "* you are on team \'%s\'\n", pPlayer->m_szTeamName);
	}
	else
	{
		if (0 != m_blCTF)
			sprintf(text, "* assigned to team %s (%s)\n", pPlayer->m_szTeamName, GetFlagTeamColor(GetTeamIndex(pPlayer->m_szTeamName) + 1));
		else
			sprintf(text, "* assigned to team %s\n", pPlayer->m_szTeamName);
	}

	ChangePlayerTeam(pPlayer, pPlayer->m_szTeamName, false, false);
	UTIL_SayText(text, pPlayer);
	int clientIndex = pPlayer->entindex();
	RecountTeams();
	// update this player with all the other players team info
	// loop through all active players and send their team info to the new client
	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* plr = UTIL_PlayerByIndex(i);
		if (plr && IsValidTeam(plr->TeamID()))
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgTeamInfo, NULL, pPlayer->edict());
			WRITE_BYTE(plr->entindex());
			WRITE_STRING(plr->TeamID());
			MESSAGE_END();
		}
	}

	if (0 != m_blCTF)
		UpdateTeamScores();
}


// orig 0x221020. The original sends no ScoreInfo here (halflife-updated does).
void CHalfLifeTeamplay::ChangePlayerTeam(CBasePlayer* pPlayer, const char* pTeamName, bool bKill, bool bGib)
{
	int damageFlags = DMG_GENERIC;
	int clientIndex = pPlayer->entindex();

	if (!bGib)
	{
		damageFlags |= DMG_NEVERGIB;
	}
	else
	{
		damageFlags |= DMG_ALWAYSGIB;
	}

	if (bKill)
	{
		// kill the player,  remove a death,  and let them start on the new team
		m_DisableDeathMessages = true;
		m_DisableDeathPenalty = true;

		pPlayer->TakeDamage(VARS(INDEXENT(0)), VARS(INDEXENT(0)), 900, damageFlags);

		m_DisableDeathMessages = false;
		m_DisableDeathPenalty = false;
	}

	// copy out the team name from the model
	strncpy(pPlayer->m_szTeamName, pTeamName, TEAM_NAME_LENGTH);

	g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", pPlayer->m_szTeamName);
	g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "team", pPlayer->m_szTeamName);

	// notify everyone's HUD of the team change
	MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
	WRITE_BYTE(clientIndex);
	WRITE_STRING(pPlayer->m_szTeamName);
	MESSAGE_END();
}


//=========================================================
// ClientUserInfoChanged
//=========================================================
// orig 0x2211b0. Rocket Crowbar names the flag colour in CTF and logs in its own format.
void CHalfLifeTeamplay::ClientUserInfoChanged(CBasePlayer* pPlayer, char* infobuffer)
{
	char text[1024];

	// prevent skin/color/model changes
	char* mdls = g_engfuncs.pfnInfoKeyValue(infobuffer, "model");

	if (!stricmp(mdls, pPlayer->m_szTeamName))
		return;

	if (0 != defaultteam.value)
	{
		int clientIndex = pPlayer->entindex();

		g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", pPlayer->m_szTeamName);
		g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "team", pPlayer->m_szTeamName);
		sprintf(text, "* Not allowed to change teams in this game!\n");
		UTIL_SayText(text, pPlayer);
		return;
	}

	if (0 != defaultteam.value || !IsValidTeam(mdls))
	{
		int clientIndex = pPlayer->entindex();

		g_engfuncs.pfnSetClientKeyValue(clientIndex, g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", pPlayer->m_szTeamName);
		sprintf(text, "* Can't change team to \'%s\'\n", mdls);
		UTIL_SayText(text, pPlayer);
		sprintf(text, "* Server limits teams to \'%s\'\n", m_szTeamList);
		UTIL_SayText(text, pPlayer);
		return;
	}
	// notify everyone of the team change
	if (0 != m_blCTF)
		sprintf(text, "* %s has changed to team \'%s\' (%s)\n", STRING(pPlayer->pev->netname), mdls, GetFlagTeamColor(GetTeamIndex(mdls) + 1));
	else
		sprintf(text, "* %s has changed to team \'%s\'\n", STRING(pPlayer->pev->netname), mdls);
	UTIL_SayTextAll(text, pPlayer);

	if (0 != m_blCTF)
		UTIL_LogPrintf("\"%s<%i>\" changed to team %s (%s)\n", STRING(pPlayer->pev->netname), GETPLAYERUSERID(pPlayer->edict()), mdls, GetFlagTeamColor(GetTeamIndex(mdls) + 1));
	else
		UTIL_LogPrintf("\"%s<%i>\" changed to team %s\n", STRING(pPlayer->pev->netname), GETPLAYERUSERID(pPlayer->edict()), mdls);

	ChangePlayerTeam(pPlayer, mdls, true, true);
	// recound stuff
	RecountTeams();
}

//=========================================================
// Deathnotice.
//=========================================================
// orig 0x2215b0
void CHalfLifeTeamplay::DeathNotice(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pevInflictor)
{
	if (m_DisableDeathMessages)
		return;

	if (pVictim && pKiller && (pKiller->flags & FL_CLIENT) != 0)
	{
		CBasePlayer* pk = (CBasePlayer*)CBaseEntity::Instance(pKiller);

		if (pk)
		{
			if ((pk != pVictim) && (PlayerRelationship(pVictim, pk) == GR_TEAMMATE))
			{
				MESSAGE_BEGIN(MSG_ALL, gmsgDeathMsg);
				WRITE_BYTE(ENTINDEX(ENT(pKiller)));		// the killer
				WRITE_BYTE(ENTINDEX(pVictim->edict())); // the victim
				WRITE_STRING("teammate");				// flag this as a teammate kill
				MESSAGE_END();
				return;
			}
		}
	}

	CHalfLifeMultiplay::DeathNotice(pVictim, pKiller, pevInflictor);
}

//=========================================================
//=========================================================
// orig 0x2216f0
void CHalfLifeTeamplay::PlayerKilled(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor)
{
	if (!m_DisableDeathPenalty)
	{
		CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);
		RecountTeams();
	}
}


//=========================================================
// IsTeamplay
//=========================================================
// orig 0x221730
bool CHalfLifeTeamplay::IsTeamplay()
{
	return true;
}

// orig 0x221740
bool CHalfLifeTeamplay::FPlayerCanTakeDamage(CBasePlayer* pPlayer, CBaseEntity* pAttacker)
{
	if (pAttacker && PlayerRelationship(pPlayer, pAttacker) == GR_TEAMMATE)
	{
		// my teammate hit me.
		if ((friendlyfire.value == 0) && (pAttacker != pPlayer))
		{
			// friendly fire is off, and this hit came from someone other than myself,  then don't get hurt
			return false;
		}
	}

	return CHalfLifeMultiplay::FPlayerCanTakeDamage(pPlayer, pAttacker);
}

//=========================================================
//=========================================================
// orig 0x2217d0
int CHalfLifeTeamplay::PlayerRelationship(CBaseEntity* pPlayer, CBaseEntity* pTarget)
{
	// half life multiplay has a simple concept of Player Relationships.
	// you are either on another player's team, or you are not.
	if (!pPlayer || !pTarget || !pTarget->IsPlayer())
		return GR_NOTTEAMMATE;

	if ((*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp(GetTeamID(pPlayer), GetTeamID(pTarget)))
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

//=========================================================
//=========================================================
// orig 0x221870
bool CHalfLifeTeamplay::ShouldAutoAim(CBasePlayer* pPlayer, edict_t* target)
{
	// always autoaim, unless target is a teammate
	CBaseEntity* pTgt = CBaseEntity::Instance(target);
	if (pTgt && pTgt->IsPlayer())
	{
		if (PlayerRelationship(pPlayer, pTgt) == GR_TEAMMATE)
			return false; // don't autoaim at teammates
	}

	return CHalfLifeMultiplay::ShouldAutoAim(pPlayer, target);
}

//=========================================================
//=========================================================
// orig 0x2218f0
int CHalfLifeTeamplay::IPointsForKill(CBasePlayer* pAttacker, CBasePlayer* pKilled)
{
	if (!pKilled)
		return 0;

	if (!pAttacker)
		return 1;

	if (pAttacker != pKilled && PlayerRelationship(pAttacker, pKilled) == GR_TEAMMATE)
		return -1;

	return 1;
}

//=========================================================
//=========================================================
// orig 0x221950
const char* CHalfLifeTeamplay::GetTeamID(CBaseEntity* pEntity)
{
	if (pEntity == NULL || pEntity->pev == NULL)
		return "";

	// return their team name
	return pEntity->TeamID();
}


// orig 0x221990
int CHalfLifeTeamplay::GetTeamIndex(const char* pTeamName)
{
	if (pTeamName && *pTeamName != 0)
	{
		// try to find existing team
		for (int tm = 0; tm < num_teams; tm++)
		{
			if (!stricmp(team_names[tm], pTeamName))
				return tm;
		}
	}

	return -1; // No match
}


// orig 0x2219f0
const char* CHalfLifeTeamplay::GetIndexedTeamName(int teamIndex)
{
	if (teamIndex < 0 || teamIndex >= num_teams)
		return "";

	return team_names[teamIndex];
}


// orig 0x221a30
bool CHalfLifeTeamplay::IsValidTeam(const char* pTeamName)
{
	if (!m_teamLimit) // Any team is valid if the teamlist isn't set
		return true;

	return (GetTeamIndex(pTeamName) != -1) ? true : false;
}

// orig 0x221a80
const char* CHalfLifeTeamplay::TeamWithFewestPlayers()
{
	int i;
	int minPlayers = MAX_TEAMS;
	int teamCount[MAX_TEAMS];
	char* pTeamName = NULL;

	memset(teamCount, 0, MAX_TEAMS * sizeof(int));

	// loop through all clients, count number of players on each team
	for (i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* plr = UTIL_PlayerByIndex(i);

		if (plr)
		{
			int team = GetTeamIndex(plr->TeamID());
			if (team >= 0)
				teamCount[team]++;
		}
	}

	// Find team with least players
	for (i = 0; i < num_teams; i++)
	{
		if (teamCount[i] < minPlayers)
		{
			minPlayers = teamCount[i];
			pTeamName = team_names[i];
		}
	}

	return pTeamName;
}


//=========================================================
//=========================================================
// orig 0x221b70. Rocket Crowbar counts a flag for each listed team, and keeps the team scores
// in CTF (where they are flag captures, not frags). The original has no bResendInfo (TeamInfo
// re-send); halflife-updated's ClientUserInfoChanged asked for one.
void CHalfLifeTeamplay::RecountTeams()
{
	char* pName;
	char teamlist[TEAMPLAY_TEAMLISTLENGTH];

	// loop through all teams, recounting everything
	num_teams = 0;
	num_flags = 0;

	// Copy all of the teams from the teamlist
	// make a copy because strtok is destructive
	strcpy(teamlist, m_szTeamList);
	pName = teamlist;
	pName = strtok(pName, ";");
	while (pName != NULL && '\0' != *pName)
	{
		if (GetTeamIndex(pName) < 0)
		{
			strcpy(team_names[num_teams], pName);
			num_teams++;
			num_flags++;
		}
		pName = strtok(NULL, ";");
	}

	if (num_teams < 2)
	{
		num_teams = 0;
		num_flags = 0;
		m_teamLimit = false;
	}

	// Sanity check
	if (0 == m_blCTF)
		memset(team_scores, 0, sizeof(team_scores));

	// loop through all clients
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* plr = UTIL_PlayerByIndex(i);

		if (plr)
		{
			const char* pTeamName = plr->TeamID();
			// try add to existing team
			int tm = GetTeamIndex(pTeamName);

			if (tm < 0) // no team match found
			{
				if (!m_teamLimit)
				{
					// add to new team
					tm = num_teams;
					num_teams++;
					if (0 == m_blCTF)
						team_scores[tm] = 0;
					strncpy(team_names[tm], pTeamName, MAX_TEAMNAME_LENGTH);
				}
			}

			if (0 == m_blCTF && tm >= 0)
			{
				team_scores[tm] += plr->pev->frags;
			}
		}
	}
}

// orig 0x221da0
const char* CHalfLifeTeamplay::GetGameDescription()
{
	if (0 == m_blCTF)
		return "Rocket Crowbar Teamplay";
	return "Rocket Crowbar CTF";
}

// Rocket Crowbar 1.9. orig 0x221dd0: where flags 1..iFlags start. Each info_flagspawn whose
// "button" is 1..iFlags+1 sets flag "button"'s base, if there are at least iFlags of them.
// Otherwise flag 1 takes a random deathmatch spawn and each next one the unused deathmatch spawn
// farthest on average from the flags placed so far; each spawn used is removed. Too few
// deathmatch spawns (iFlags or fewer) turn CTF off.
// orig 0x221dd0
void CHalfLifeTeamplay::SetFlagSpawns(int iFlags)
{
	CBaseEntity* pSpot = NULL;
	int iCount = 0;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_flagspawn")) != NULL)
		iCount++;

	if (iCount >= iFlags)
	{
		pSpot = NULL;
		while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_flagspawn")) != NULL)
		{
			// the original takes button iFlags+1 too, one past the flags (see m_vecFlagSpawns)
			int iButton = pSpot->pev->button;
			if (iButton > 0 && iButton <= iFlags + 1)
			{
				m_vecFlagOrigin[iButton - 1] = pSpot->pev->origin;
				m_vecFlagAngles[iButton - 1] = pSpot->pev->angles;
				pSpot->pev->classname = MAKE_STRING("flag_start");
			}
		}
		return;
	}

	iCount = 0;
	pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch")) != NULL)
		iCount++;

	if (iCount < iFlags + 1)
	{
		m_blCTF = 0;
		return;
	}

	// a random spawn for the first flag (the random number is drawn again each step, as in the original)
	pSpot = NULL;
	for (int i = 1; i < RANDOM_LONG(2, iCount - 1); i++)
		pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch");

	if (!pSpot)
	{
		m_blCTF = 0;
		return;
	}

	m_vecFlagOrigin[0] = pSpot->pev->origin;
	m_vecFlagAngles[0] = pSpot->pev->angles;
	pSpot->SUB_Remove();

	for (int iFlag = 1; iFlag <= iFlags - 1; iFlag++)
	{
		// VERIFY: guard, the original crashes here: it keeps the best spawn from the last flag when
		// it finds none this time (a removed entity, or NULL for the second flag) and reads through it.
		CBaseEntity* pBest = NULL;
		float flBest = 0;
		pSpot = NULL;
		while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch")) != NULL)
		{
			Vector vecSpot = pSpot->Center();

			bool bUsed = false;
			for (int j = 0; j <= iFlag - 1; j++)
			{
				if (m_vecFlagOrigin[j].x == vecSpot.x && m_vecFlagOrigin[j].y == vecSpot.y && m_vecFlagOrigin[j].z == vecSpot.z)
					bUsed = true;
			}
			if (bUsed)
				continue;

			float flTotal = 0;
			for (int j = 0; j <= iFlag - 1; j++)
				flTotal += (vecSpot - m_vecFlagOrigin[j]).Length();

			float flAverage = flTotal / iFlag;
			if (flAverage > flBest)
			{
				pBest = pSpot;
				flBest = flAverage;
			}
		}

		if (!pBest)
			return;

		m_vecFlagOrigin[iFlag] = pBest->pev->origin;
		m_vecFlagAngles[iFlag] = pBest->pev->angles;
		pBest->pev->classname = MAKE_STRING("flag_start");
		pBest->SUB_Remove();
	}
}

// Rocket Crowbar 1.9. orig 0x222180: team iTeam's name (1-based).
const char* GetTeamNameForIndex(int iTeam)
{
	// VERIFY: guard, the original crashes here: it strncpy's into a "" string literal (read-only)
	// and returns that; ours uses a static buffer.
	static char szTeamName[MAX_TEAMNAME_LENGTH + 1];
	// VERIFY: no range check, as in the original; iTeam outside 1..MAX_TEAMS reads past team_names
	strncpy(szTeamName, team_names[iTeam - 1], MAX_TEAMNAME_LENGTH);
	szTeamName[MAX_TEAMNAME_LENGTH] = 0;
	return szTeamName;
}

// Rocket Crowbar 1.9. orig 0x2221c0: add to team iTeam's (1-based) CTF score and tell everyone.
void AddTeamScore(int iTeam, int iScore)
{
	if (0 == m_blCTF)
		return;

	char szName[MAX_TEAMNAME_LENGTH + 1]; // the original sprintf's into 16 bytes
	snprintf(szName, sizeof(szName), "%s", GetTeamNameForIndex(iTeam));
	team_scores[iTeam - 1] += iScore;

	MESSAGE_BEGIN(MSG_ALL, gmsgTeamScore);
	WRITE_STRING(szName);
	WRITE_SHORT(team_scores[iTeam - 1]);
	WRITE_SHORT(0);
	MESSAGE_END();
}

// Rocket Crowbar 1.9. orig 0x222290: send every team's CTF score.
void UpdateTeamScores()
{
	if (0 == m_blCTF)
		return;

	for (int i = 0; i < num_teams; i++)
	{
		char szName[MAX_TEAMNAME_LENGTH + 1]; // the original sprintf's into 16 bytes
		snprintf(szName, sizeof(szName), "%s", GetTeamNameForIndex(i + 1));

		MESSAGE_BEGIN(MSG_ALL, gmsgTeamScore);
		WRITE_STRING(szName);
		WRITE_SHORT(team_scores[i]);
		WRITE_SHORT(0);
		MESSAGE_END();
	}
}

// Rocket Crowbar 1.9. orig 0x222370: called as the game rules are made (InstallGameRules): turn
// CTF on with mp_ctf 1 and 2-6 teams, and clear the scores.
void SendCTFInitMSG()
{
	if (CVAR_GET_FLOAT("mp_ctf") == 1 && num_teams <= 6 && num_teams > 1)
	{
		m_blCTF = 1;
		m_flInitCTF = gpGlobals->time + 0.5;
		memset(team_scores, 0, sizeof(team_scores));
	}
}
