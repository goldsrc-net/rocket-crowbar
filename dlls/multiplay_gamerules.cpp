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

#include "skill.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"
#include "hltv.h"
#include "trains.h"
#include "UserMessages.h"
#include "Preff.h"

#define ITEM_RESPAWN_TIME 30
#define WEAPON_RESPAWN_TIME 20
#define AMMO_RESPAWN_TIME 20

// longest the intermission can last, in seconds
#define MAX_INTERMISSION_TIME 120

CVoiceGameMgr g_VoiceGameMgr;

class CMultiplayGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	// The original has no voice manager, so the engine let everyone hear everyone; halflife-updated
	// limits teamplay voice to teammates. Ours keeps the manager (vban muting) but hears everyone.
	bool CanPlayerHearPlayer(CBasePlayer* pListener, CBasePlayer* pTalker) override
	{
		return true;
	}
};
static CMultiplayGameMgrHelper g_GameMgrHelper;

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

// orig 0x1ce930. The original runs no config here (CWorld::Precache runs game.cfg and the server
// config once); the voice manager is halflife-updated's.
CHalfLifeMultiplay::CHalfLifeMultiplay()
{
	g_VoiceGameMgr.Init(&g_GameMgrHelper, gpGlobals->maxClients);

	RefreshSkillData();
}

bool CHalfLifeMultiplay::ClientCommand(CBasePlayer* pPlayer, const char* pcmd)
{
	if (g_VoiceGameMgr.ClientCommand(pPlayer, pcmd))
		return true;

	return CGameRules::ClientCommand(pPlayer, pcmd);
}

void CHalfLifeMultiplay::ClientUserInfoChanged(CBasePlayer* pPlayer, char* infobuffer)
{
	pPlayer->SetPrefsFromUserinfo(infobuffer);
}

//=========================================================
//=========================================================
// orig 0x1ce970
void CHalfLifeMultiplay::RefreshSkillData()
{
	// load all default values
	CGameRules::RefreshSkillData();

	// override some values for multiplay.

	// suitcharger
	gSkillData.suitchargerCapacity = 30;

	// Crowbar whack
	gSkillData.plrDmgCrowbar = 25;

	// Glock Round
	gSkillData.plrDmg9MM = 12;

	// 357 Round
	gSkillData.plrDmg357 = 40; // RC 1.9's (as SDK 2.1); halflife-updated raised it to 50

	// MP5 Round
	gSkillData.plrDmgMP5 = 12;

	// M203 grenade
	gSkillData.plrDmgM203Grenade = 100;

	// Shotgun buckshot
	gSkillData.plrDmgBuckshot = 20; // fewer pellets in deathmatch

	// Crossbow
	gSkillData.plrDmgCrossbowClient = 20;

	// RPG
	gSkillData.plrDmgRPG = 120;

	// Egon
	gSkillData.plrDmgEgonWide = 20;
	gSkillData.plrDmgEgonNarrow = 10;

	// Hand Grendade
	gSkillData.plrDmgHandGrenade = 100;

	// Satchel Charge
	gSkillData.plrDmgSatchel = 120;

	// Tripmine
	gSkillData.plrDmgTripmine = 150;

	// hornet
	gSkillData.plrDmgHornet = 10;
}

//=========================================================
//=========================================================
// orig 0x1cea30. The original's intermission lasts 6 seconds (GoToIntermission), then waits for a
// button or 120 more seconds; halflife-updated's mp_chattime isn't used. The voice manager update
// and the mp_fragsleft/mp_timeleft cvars are halflife-updated's, kept.
void CHalfLifeMultiplay::Think()
{
	g_VoiceGameMgr.Update(gpGlobals->frametime);

	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining = 0;

	if (g_fGameOver) // someone else quit the game already
	{
		// check to see if we should change levels now
		if (m_flIntermissionEndTime < gpGlobals->time)
		{
			if (m_iEndIntermissionButtonHit // check that someone has pressed a key, or the max intermission time is over
				|| ((m_flIntermissionEndTime + MAX_INTERMISSION_TIME) < gpGlobals->time))
				ChangeLevel(); // intermission is over
		}

		return;
	}

	float flTimeLimit = timelimit.value * 60;
	float flFragLimit = fraglimit.value;

	time_remaining = (int)(0 != flTimeLimit ? (flTimeLimit - gpGlobals->time) : 0);

	if (flTimeLimit != 0 && gpGlobals->time >= flTimeLimit)
	{
		GoToIntermission();
		return;
	}

	if (0 != flFragLimit)
	{
		int bestfrags = 9999;
		int remain;

		// check if any player is over the frag limit
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);

			if (pPlayer && pPlayer->pev->frags >= flFragLimit)
			{
				GoToIntermission();
				return;
			}


			if (pPlayer)
			{
				remain = flFragLimit - pPlayer->pev->frags;
				if (remain < bestfrags)
				{
					bestfrags = remain;
				}
			}
		}
		frags_remaining = bestfrags;
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
//=========================================================
// orig 0x1ceb50
bool CHalfLifeMultiplay::IsMultiplayer()
{
	return true;
}

//=========================================================
//=========================================================
// orig 0x1ceb60
bool CHalfLifeMultiplay::IsDeathmatch()
{
	return true;
}

//=========================================================
//=========================================================
// orig 0x1ceb70
bool CHalfLifeMultiplay::IsCoOp()
{
	return 0 != gpGlobals->coop;
}

//=========================================================
//=========================================================
// orig 0x1cebb0. The original has no cl_autowepswitch preference (halflife-updated's).
bool CHalfLifeMultiplay::FShouldSwitchWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pWeapon)
{
	if (!pWeapon->CanDeploy())
	{
		// that weapon can't deploy anyway.
		return false;
	}

	if (!pPlayer->m_pActiveItem)
	{
		// player doesn't have an active item!
		return true;
	}

	if (!pPlayer->m_pActiveItem->CanHolster())
	{
		// can't put away the active item.
		return false;
	}

	if (pWeapon->iWeight() > pPlayer->m_pActiveItem->iWeight())
	{
		return true;
	}

	return false;
}

// orig 0x1cec40: the 2001 SDK's (halflife-updated moved it to CGameRules and added alwaysSearch,
// which the original's callers never pass). The same weight first, then the heaviest that deploys.
bool CHalfLifeMultiplay::GetNextBestWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pCurrentWeapon, bool alwaysSearch)
{
	CBasePlayerItem* pCheck;
	CBasePlayerItem* pBest = NULL; // this will be used in the event that we don't find a weapon in the same category.
	int iBestWeight = -1;		   // no weapon lower than -1 can be autoswitched to

	if (!pCurrentWeapon->CanHolster())
	{
		// can't put this gun away right now, so can't switch.
		return false;
	}

	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		pCheck = pPlayer->m_rgpPlayerItems[i];

		while (pCheck)
		{
			if (pCheck->iWeight() > -1 && pCheck->iWeight() == pCurrentWeapon->iWeight() && pCheck != pCurrentWeapon)
			{
				// this weapon is from the same category.
				if (pCheck->CanDeploy())
				{
					if (pPlayer->SwitchWeapon(pCheck))
					{
						return true;
					}
				}
			}
			else if (pCheck->iWeight() > iBestWeight && pCheck != pCurrentWeapon) // don't reselect the weapon we're trying to get rid of
			{
				//ALERT ( at_console, "Considering %s\n", STRING( pCheck->pev->classname ) );
				// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
				// that the player was using. This will end up leaving the player with his heaviest-weighted
				// weapon.
				if (pCheck->CanDeploy())
				{
					// if this weapon is useable, flag it as the best
					iBestWeight = pCheck->iWeight();
					pBest = pCheck;
				}
			}

			pCheck = pCheck->m_pNext;
		}
	}

	// if we make it here, we've checked all the weapons and found no useable
	// weapon in the same catagory as the current weapon.

	// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always
	// at least get the crowbar, but ya never know.
	if (!pBest)
	{
		return false;
	}

	pPlayer->SwitchWeapon(pBest);

	return true;
}

//=========================================================
//=========================================================
// orig 0x1ced90
bool CHalfLifeMultiplay::ClientConnected(edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128])
{
	g_VoiceGameMgr.ClientConnected(pEntity);
	return true;
}

// orig 0x1ceda0
void CHalfLifeMultiplay::UpdateGameMode(CBasePlayer* pPlayer)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgGameMode, NULL, pPlayer->edict());
	WRITE_BYTE(0); // game mode none
	MESSAGE_END();
}

// orig 0x1cee00. VERIFY: the original's ScoreInfo messages carry only index, frags and deaths
// (the 2001 format); ours keep halflife-updated's five fields, which a current client reads.
void CHalfLifeMultiplay::InitHUD(CBasePlayer* pl)
{
	// notify other clients of player joining the game
	UTIL_ClientPrintAll(HUD_PRINTNOTIFY, UTIL_VarArgs("%s has joined the game\n",
											 (!FStringNull(pl->pev->netname) && STRING(pl->pev->netname)[0] != 0) ? STRING(pl->pev->netname) : "unconnected"));

	UTIL_LogPrintf("\"%s<%i>\" has entered the game\n", STRING(pl->pev->netname), GETPLAYERUSERID(pl->edict()));

	UpdateGameMode(pl);

	// sending just one score makes the hud scoreboard active;  otherwise
	// it is just disabled for single play
	MESSAGE_BEGIN(MSG_ONE, gmsgScoreInfo, NULL, pl->edict());
	WRITE_BYTE(ENTINDEX(pl->edict()));
	WRITE_SHORT(0);
	WRITE_SHORT(0);
	WRITE_SHORT(0);
	WRITE_SHORT(0);
	MESSAGE_END();

	// Rocket Crowbar names itself as the server (SendMOTDToClient sends no hostname in the original)
	MESSAGE_BEGIN(MSG_ONE, gmsgServerName, NULL, pl->edict());
	WRITE_STRING(UTIL_VarArgs("Rocket Crowbar v%s", "19"));
	MESSAGE_END();

	SendMOTDToClient(pl->edict());

	// loop through all active players and send their score info to the new client
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		// FIXME:  Probably don't need to cast this just to read m_iDeaths
		CBasePlayer* plr = (CBasePlayer*)UTIL_PlayerByIndex(i);

		if (plr)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgScoreInfo, NULL, pl->edict());
			WRITE_BYTE(i); // client number
			WRITE_SHORT(plr->pev->frags);
			WRITE_SHORT(plr->m_iDeaths);
			WRITE_SHORT(0);
			WRITE_SHORT(GetTeamIndex(plr->m_szTeamName) + 1);
			MESSAGE_END();
		}
	}

	if (g_fGameOver)
	{
		MESSAGE_BEGIN(MSG_ONE, SVC_INTERMISSION, NULL, pl->edict());
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
// orig 0x1cf0c0
void CHalfLifeMultiplay::ClientDisconnected(edict_t* pClient)
{
	if (pClient)
	{
		CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pClient);

		if (pPlayer)
		{
			FireTargets("game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0);

			UTIL_LogPrintf("\"%s<%i>\" disconnected\n", STRING(pPlayer->pev->netname), GETPLAYERUSERID(pPlayer->edict()));

			pPlayer->RemoveAllItems(true); // destroy all of the players weapons and items
		}
	}
}

//=========================================================
//=========================================================
// orig 0x1cf140: Rocket Crowbar has no fall damage in multiplayer (halflife-updated's mp_falldamage
// is unused).
float CHalfLifeMultiplay::FlPlayerFallDamage(CBasePlayer* pPlayer)
{
	return 0;
}

//=========================================================
//=========================================================
// orig 0x1cf150
bool CHalfLifeMultiplay::FPlayerCanTakeDamage(CBasePlayer* pPlayer, CBaseEntity* pAttacker)
{
	return true;
}

//=========================================================
//=========================================================
// orig 0x1cf160
void CHalfLifeMultiplay::PlayerThink(CBasePlayer* pPlayer)
{
	if (g_fGameOver)
	{
		// check for button presses
		if ((pPlayer->m_afButtonPressed & (IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP)) != 0)
			m_iEndIntermissionButtonHit = true;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}
}

//=========================================================
//=========================================================
// orig 0x1cf1c0. Rocket Crowbar's starting weapons and ammo come from rc.ini ([PLAYER]).
void CHalfLifeMultiplay::PlayerSpawn(CBasePlayer* pPlayer)
{
	if (!pPlayer)
		return;

	bool addDefault;
	CBaseEntity* pWeaponEntity = NULL;

	pPlayer->SetHasSuit(true); // the original sets WEAPON_SUIT in pev->weapons

	addDefault = true;

	while (pWeaponEntity = UTIL_FindEntityByClassname(pWeaponEntity, "game_player_equip"))
	{
		pWeaponEntity->Touch(pPlayer);
		addDefault = false;
	}

	if (addDefault)
	{
		// the weapon list is split on tab, newline, space and comma
		char szWeapons[1024];
		strncpy(szWeapons, PREFS_KeyLoopUpString("PLAYER", "StartingWeapons", "weapon_crowbar weapon_9mmhandgun"), 1023);
		szWeapons[1023] = 0;
		for (char* pTok = strtok(szWeapons, " \t\n,"); pTok && *pTok; pTok = strtok(NULL, " \t\n,"))
			pPlayer->GiveNamedItem(pTok);

		// the max is looked up before the count, as in the original (both create their rc.ini keys)
		int iMax, iCount;
		iMax = PREFS_KeyLoopUpIntClamp("ITEMS", "MaxBullets", 250, 0, 250);
		iCount = PREFS_KeyLoopUpInt("PLAYER", "StartingBullets", 80);
		pPlayer->GiveAmmo(iCount, "9mm", iMax);
		iMax = PREFS_KeyLoopUpIntClamp("CROSSBOW", "MaxAmmo", 80, 0, 250);
		iCount = PREFS_KeyLoopUpInt("PLAYER", "StartingCrossbowAmmo", 0);
		pPlayer->GiveAmmo(iCount, "bolts", iMax);
		iMax = PREFS_KeyLoopUpIntClamp("ITEMS", "CellsMax", 200, 0, 250);
		iCount = PREFS_KeyLoopUpInt("PLAYER", "StartingCells", 0);
		pPlayer->GiveAmmo(iCount, "uranium", iMax);
		iMax = PREFS_KeyLoopUpIntClamp("MP5", "MaxGrenades", 15, 0, 250);
		iCount = PREFS_KeyLoopUpInt("PLAYER", "StartingMP5Grenades", 0);
		pPlayer->GiveAmmo(iCount, "ARgrenades", iMax);
		iMax = PREFS_KeyLoopUpIntClamp("PYTHON", "MaxAmmo", 48, 0, 250);
		iCount = PREFS_KeyLoopUpInt("PLAYER", "StartingPythonAmmo", 0);
		pPlayer->GiveAmmo(iCount, "357", iMax);
		iMax = PREFS_KeyLoopUpIntClamp("SHOTGUN", "MaxAmmo", 15, 0, 250);
		iCount = PREFS_KeyLoopUpInt("PLAYER", "StartingShotgunAmmo", 0);
		pPlayer->GiveAmmo(iCount, "buckshot", iMax);
		iMax = PREFS_KeyLoopUpIntClamp("RPG", "MaxAmmo", 10, 0, 250);
		iCount = PREFS_KeyLoopUpInt("PLAYER", "StartingRPGAmmo", 0);
		pPlayer->GiveAmmo(iCount, "rockets", iMax);
	}
}

//=========================================================
//=========================================================
// orig 0x1cf4d0
bool CHalfLifeMultiplay::FPlayerCanRespawn(CBasePlayer* pPlayer)
{
	return true;
}

//=========================================================
//=========================================================
// orig 0x1cf4e0
float CHalfLifeMultiplay::FlPlayerSpawnTime(CBasePlayer* pPlayer)
{
	return gpGlobals->time; //now!
}

// orig 0x1cf510
bool CHalfLifeMultiplay::AllowAutoTargetCrosshair()
{
	return (aimcrosshair.value != 0);
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
// orig 0x1cf560
int CHalfLifeMultiplay::IPointsForKill(CBasePlayer* pAttacker, CBasePlayer* pKilled)
{
	return 1;
}


//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
// orig 0x1cf570
void CHalfLifeMultiplay::PlayerKilled(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor)
{
	// orig 0x1cf570: the 2001 SDK's (no func_vehicle driver credit, which halflife-updated added)
	CBasePlayer* peKiller = NULL;
	CBaseEntity* ktmp = CBaseEntity::Instance(pKiller);
	if (ktmp && ktmp->IsPlayer())
		peKiller = (CBasePlayer*)ktmp;

	DeathNotice(pVictim, pKiller, pInflictor);

	pVictim->m_iDeaths += 1;

	FireTargets("game_playerdie", pVictim, pVictim, USE_TOGGLE, 0);

	if (pVictim->pev == pKiller)
	{ // killed self
		pKiller->frags -= 1;
	}
	else if (ktmp && ktmp->IsPlayer())
	{
		// if a player dies in a deathmatch game and the killer is a client, award the killer some points
		pKiller->frags += IPointsForKill(peKiller, pVictim);

		FireTargets("game_playerkill", ktmp, ktmp, USE_TOGGLE, 0);
	}
	else
	{ // killed by the world
		pKiller->frags -= 1;
	}

	// update the scores
	// killed scores
	MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
	WRITE_BYTE(ENTINDEX(pVictim->edict()));
	WRITE_SHORT(pVictim->pev->frags);
	WRITE_SHORT(pVictim->m_iDeaths);
	WRITE_SHORT(0);
	WRITE_SHORT(GetTeamIndex(pVictim->m_szTeamName) + 1);
	MESSAGE_END();

	// killers score, if it's a player
	CBaseEntity* ep = CBaseEntity::Instance(pKiller);
	if (ep && ep->IsPlayer())
	{
		CBasePlayer* PK = (CBasePlayer*)ep;

		MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
		WRITE_BYTE(ENTINDEX(PK->edict()));
		WRITE_SHORT(PK->pev->frags);
		WRITE_SHORT(PK->m_iDeaths);
		WRITE_SHORT(0);
		WRITE_SHORT(GetTeamIndex(PK->m_szTeamName) + 1);
		MESSAGE_END();

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}

	if (pVictim->HasNamedPlayerItem("weapon_satchel"))
	{
		DeactivateSatchels(pVictim);
	}
}

//=========================================================
// Deathnotice.
//=========================================================
// orig 0x1cf840. Rocket Crowbar names a monster victim "a <name>" in the log, logs in its own
// formats and echoes the line to a dedicated server's console. No HLTV director message (the
// original predates HLTV).
void CHalfLifeMultiplay::DeathNotice(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pevInflictor)
{
	const char* killer_weapon_name = "world"; // by default, the player is killed by the world
	char szVictim[128] = "nobody";
	int killer_index = 0;

	// Hack to fix name change
	const char* tau = "tau_cannon";
	const char* gluon = "gluon gun";

	if ((pKiller->flags & FL_CLIENT) != 0)
	{
		killer_index = ENTINDEX(ENT(pKiller));

		if (pevInflictor)
		{
			if (pevInflictor == pKiller)
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pKiller);

				if (pPlayer->m_pActiveItem)
				{
					killer_weapon_name = pPlayer->m_pActiveItem->pszName();
				}
			}
			else
			{
				killer_weapon_name = STRING(pevInflictor->classname); // it's just that easy
			}
		}
	}
	else if (pevInflictor)
	{
		killer_weapon_name = STRING(pevInflictor->classname);
	}

	if (pVictim)
	{
		if ((pVictim->pev->flags & FL_CLIENT) != 0)
		{
			strncpy(szVictim, STRING(pVictim->pev->netname), sizeof(szVictim) - 1); // the original strcpy's
			szVictim[sizeof(szVictim) - 1] = 0;
		}
		else
		{
			const char* pszClass = STRING(pVictim->pev->classname);
			if (0 == strncmp(pszClass, "monster_", 8))
				pszClass += 8;
			snprintf(szVictim, sizeof(szVictim), "a %s", pszClass);
		}
	}

	// strip the monster_* or weapon_* from the inflictor's classname
	if (strncmp(killer_weapon_name, "weapon_", 7) == 0)
		killer_weapon_name += 7;
	else if (strncmp(killer_weapon_name, "monster_", 8) == 0)
		killer_weapon_name += 8;
	else if (strncmp(killer_weapon_name, "func_", 5) == 0)
		killer_weapon_name += 5;

	MESSAGE_BEGIN(MSG_ALL, gmsgDeathMsg);
	WRITE_BYTE(killer_index);				// the killer
	WRITE_BYTE(ENTINDEX(pVictim->edict())); // the victim
	WRITE_STRING(killer_weapon_name);		// what they were killed by (should this be a string?)
	MESSAGE_END();

	// replace the code names with the 'real' names
	if (0 == strcmp(killer_weapon_name, "egon"))
		killer_weapon_name = gluon;
	else if (0 == strcmp(killer_weapon_name, "gauss"))
		killer_weapon_name = tau;

	char szText[256];
	if (pVictim->pev == pKiller)
	{
		// killed self
		UTIL_LogPrintf("\"%s<%i>\" killed self with %s\n", szVictim, GETPLAYERUSERID(ENT(pKiller)), killer_weapon_name);
		if (!IS_DEDICATED_SERVER())
			return;
		snprintf(szText, sizeof(szText), "%s killed self with %s\n", szVictim, killer_weapon_name);
	}
	else if ((pKiller->flags & FL_CLIENT) != 0)
	{
		UTIL_LogPrintf("\"%s<%i>\" killed \"%s<%i>\" with %s\n", STRING(pKiller->netname), GETPLAYERUSERID(ENT(pKiller)), szVictim, GETPLAYERUSERID(pVictim->edict()), killer_weapon_name);
		if (!IS_DEDICATED_SERVER())
			return;
		snprintf(szText, sizeof(szText), "%s killed %s with %s\n", STRING(pKiller->netname), szVictim, killer_weapon_name);
	}
	else
	{
		// killed by the world
		UTIL_LogPrintf("\"%s<%i>\" killed by world with %s\n", szVictim, GETPLAYERUSERID(pVictim->edict()), killer_weapon_name);
		if (!IS_DEDICATED_SERVER())
			return;
		snprintf(szText, sizeof(szText), "%s killed by world with %s\n", szVictim, killer_weapon_name);
	}
	g_engfuncs.pfnServerPrint(szText);
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
// orig 0x1cfd10
void CHalfLifeMultiplay::PlayerGotWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pWeapon)
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
// orig 0x1cfd20
float CHalfLifeMultiplay::FlWeaponRespawnTime(CBasePlayerItem* pWeapon)
{
	if (weaponstay.value > 0)
	{
		// make sure it's only certain weapons
		if ((pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) == 0)
		{
			return gpGlobals->time + 0; // weapon respawns almost instantly
		}
	}

	return gpGlobals->time + WEAPON_RESPAWN_TIME;
}

// when we are within this close to running out of entities,  items
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE 100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
// orig 0x1cfda0
float CHalfLifeMultiplay::FlWeaponTryRespawn(CBasePlayerItem* pWeapon)
{
	if (pWeapon && WEAPON_NONE != pWeapon->m_iId && (pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) != 0)
	{
		if (NUMBER_OF_ENTITIES() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE))
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime(pWeapon);
	}

	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
// orig 0x1cfe20
Vector CHalfLifeMultiplay::VecWeaponRespawnSpot(CBasePlayerItem* pWeapon)
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
// orig 0x1cfe50
int CHalfLifeMultiplay::WeaponShouldRespawn(CBasePlayerItem* pWeapon)
{
	if ((pWeapon->pev->spawnflags & SF_NORESPAWN) != 0)
	{
		return GR_WEAPON_RESPAWN_NO;
	}

	return GR_WEAPON_RESPAWN_YES;
}

//=========================================================
// CanHaveWeapon - returns false if the player is not allowed
// to pick up this weapon
//=========================================================
// orig 0x1cfe80
bool CHalfLifeMultiplay::CanHavePlayerItem(CBasePlayer* pPlayer, CBasePlayerItem* pItem)
{
	if (weaponstay.value > 0)
	{
		if ((pItem->iFlags() & ITEM_FLAG_LIMITINWORLD) != 0)
			return CGameRules::CanHavePlayerItem(pPlayer, pItem);

		// check if the player already has this weapon
		for (int i = 0; i < MAX_ITEM_TYPES; i++)
		{
			CBasePlayerItem* it = pPlayer->m_rgpPlayerItems[i];

			while (it != NULL)
			{
				if (it->m_iId == pItem->m_iId)
				{
					return false;
				}

				it = it->m_pNext;
			}
		}
	}

	return CGameRules::CanHavePlayerItem(pPlayer, pItem);
}

//=========================================================
//=========================================================
// orig 0x1cff30
bool CHalfLifeMultiplay::CanHaveItem(CBasePlayer* pPlayer, CItem* pItem)
{
	return true;
}

//=========================================================
//=========================================================
// orig 0x1cff40
void CHalfLifeMultiplay::PlayerGotItem(CBasePlayer* pPlayer, CItem* pItem)
{
}

//=========================================================
//=========================================================
// orig 0x1cff50
int CHalfLifeMultiplay::ItemShouldRespawn(CItem* pItem)
{
	if ((pItem->pev->spawnflags & SF_NORESPAWN) != 0)
	{
		return GR_ITEM_RESPAWN_NO;
	}

	return GR_ITEM_RESPAWN_YES;
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
// orig 0x1cff80
float CHalfLifeMultiplay::FlItemRespawnTime(CItem* pItem)
{
	return gpGlobals->time + ITEM_RESPAWN_TIME;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
// orig 0x1cffb0
Vector CHalfLifeMultiplay::VecItemRespawnSpot(CItem* pItem)
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
// orig 0x1cffe0
void CHalfLifeMultiplay::PlayerGotAmmo(CBasePlayer* pPlayer, char* szName, int iCount)
{
}

//=========================================================
//=========================================================
// orig 0x1cfff0
bool CHalfLifeMultiplay::IsAllowedToSpawn(CBaseEntity* pEntity)
{
	//	if ( pEntity->pev->flags & FL_MONSTER )
	//		return false;

	return true;
}

//=========================================================
//=========================================================
// orig 0x1d0000
int CHalfLifeMultiplay::AmmoShouldRespawn(CBasePlayerAmmo* pAmmo)
{
	if ((pAmmo->pev->spawnflags & SF_NORESPAWN) != 0)
	{
		return GR_AMMO_RESPAWN_NO;
	}

	return GR_AMMO_RESPAWN_YES;
}

//=========================================================
//=========================================================
// orig 0x1d0030
float CHalfLifeMultiplay::FlAmmoRespawnTime(CBasePlayerAmmo* pAmmo)
{
	return gpGlobals->time + AMMO_RESPAWN_TIME;
}

//=========================================================
//=========================================================
// orig 0x1d0060
Vector CHalfLifeMultiplay::VecAmmoRespawnSpot(CBasePlayerAmmo* pAmmo)
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
// orig 0x1d0090: rc.ini [ITEMS] WallHealthBackOnTime
float CHalfLifeMultiplay::FlHealthChargerRechargeTime()
{
	return PREFS_KeyLoopUpFloat("ITEMS", "WallHealthBackOnTime", 60);
}

// orig 0x1d00c0: rc.ini [ITEMS] WallChargerBackOnTime
float CHalfLifeMultiplay::FlHEVChargerRechargeTime()
{
	return PREFS_KeyLoopUpFloat("ITEMS", "WallChargerBackOnTime", 30);
}

//=========================================================
//=========================================================
// orig 0x1d00f0
int CHalfLifeMultiplay::DeadPlayerWeapons(CBasePlayer* pPlayer)
{
	return GR_PLR_DROP_GUN_ACTIVE;
}

//=========================================================
//=========================================================
// orig 0x1d0100
int CHalfLifeMultiplay::DeadPlayerAmmo(CBasePlayer* pPlayer)
{
	return GR_PLR_DROP_AMMO_ACTIVE;
}

// orig 0x1d0110
edict_t* CHalfLifeMultiplay::GetPlayerSpawnSpot(CBasePlayer* pPlayer)
{
	edict_t* pentSpawnSpot = CGameRules::GetPlayerSpawnSpot(pPlayer);
	if (IsMultiplayer() && !FStringNull(pentSpawnSpot->v.target))
	{
		FireTargets(STRING(pentSpawnSpot->v.target), pPlayer, pPlayer, USE_TOGGLE, 0);
	}

	return pentSpawnSpot;
}


//=========================================================
//=========================================================
// orig 0x1d0180
int CHalfLifeMultiplay::PlayerRelationship(CBaseEntity* pPlayer, CBaseEntity* pTarget)
{
	// half life deathmatch has only enemies
	return GR_NOTTEAMMATE;
}

// orig 0x1d0190
bool CHalfLifeMultiplay::PlayFootstepSounds(CBasePlayer* pl, float fvol)
{
	if (g_footsteps && g_footsteps->value == 0)
		return false;

	if (pl->IsOnLadder() || pl->pev->velocity.Length2D() > 220)
		return true; // only make step sounds in multiplayer if the player is moving fast enough

	return false;
}

// orig 0x1d0220
bool CHalfLifeMultiplay::FAllowFlashlight()
{
	return flashlight.value != 0;
}

//=========================================================
//=========================================================
// orig 0x1d0270
bool CHalfLifeMultiplay::FAllowMonsters()
{
	return (allowmonsters.value != 0);
}

//=========================================================
//======== CHalfLifeMultiplay private functions ===========
#define INTERMISSION_TIME 6

// orig 0x1d02c0: a 6-second intermission (no mp_chattime in the original)
void CHalfLifeMultiplay::GoToIntermission()
{
	if (g_fGameOver)
		return; // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();

	m_flIntermissionEndTime = gpGlobals->time + 6;
	m_flIntermissionStartTime = gpGlobals->time;

	g_fGameOver = true;
	m_iEndIntermissionButtonHit = false;
}

#define MAX_RULE_BUFFER 1024

typedef struct mapcycle_item_s
{
	struct mapcycle_item_s* next;

	char mapname[32];
	int minplayers, maxplayers;
	char rulebuffer[MAX_RULE_BUFFER];
} mapcycle_item_t;

typedef struct mapcycle_s
{
	struct mapcycle_item_s* items;
	struct mapcycle_item_s* next_item;
} mapcycle_t;

/*
==============
DestroyMapCycle

Clean up memory used by mapcycle when switching it
==============
*/
void DestroyMapCycle(mapcycle_t* cycle)
{
	mapcycle_item_t *p, *n, *start;
	p = cycle->items;
	if (p)
	{
		start = p;
		p = p->next;
		while (p != start)
		{
			n = p->next;
			delete p;
			p = n;
		}

		delete cycle->items;
	}
	cycle->items = NULL;
	cycle->next_item = NULL;
}

static char com_token[1500];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char* COM_Parse(char* data)
{
	int c;
	int len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
			return NULL; // end of file;
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while ('\0' != *data && *data != '\n')
			data++;
		goto skipwhite;
	}


	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (true)
		{
			c = *data++;
			if (c == '\"' || '\0' == c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',')
			break;
	} while (c > 32);

	com_token[len] = 0;
	return data;
}

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
bool COM_TokenWaiting(char* buffer)
{
	char* p;

	p = buffer;
	while ('\0' != *p && *p != '\n')
	{
		if (0 == isspace(*p) || 0 != isalnum(*p))
			return true;

		p++;
	}

	return false;
}



/*
==============
ReloadMapCycleFile


Parses mapcycle.txt file into mapcycle_t structure
==============
*/
bool ReloadMapCycleFile(char* filename, mapcycle_t* cycle)
{
	char szBuffer[MAX_RULE_BUFFER];
	char szMap[32];
	int length;
	char* pFileList;
	char* aFileList = pFileList = (char*)LOAD_FILE_FOR_ME(filename, &length);
	bool hasbuffer;
	mapcycle_item_s *item, *newlist = NULL, *next;

	if (pFileList && 0 != length)
	{
		// the first map name in the file becomes the default
		while (true)
		{
			hasbuffer = false;
			memset(szBuffer, 0, MAX_RULE_BUFFER);

			pFileList = COM_Parse(pFileList);
			if (strlen(com_token) <= 0)
				break;

			strncpy(szMap, com_token, sizeof(szMap));
			szMap[sizeof(szMap) - 1] = '\0';

			// Any more tokens on this line?
			if (COM_TokenWaiting(pFileList))
			{
				pFileList = COM_Parse(pFileList);
				if (strlen(com_token) > 0)
				{
					hasbuffer = true;
					strncpy(szBuffer, com_token, sizeof(szBuffer));
					szBuffer[sizeof(szBuffer) - 1] = '\0';
				}
			}

			// Check map
			if (IS_MAP_VALID(szMap))
			{
				// Create entry
				char* s;

				item = new mapcycle_item_s;

				strcpy(item->mapname, szMap);

				item->minplayers = 0;
				item->maxplayers = 0;

				memset(item->rulebuffer, 0, MAX_RULE_BUFFER);

				if (hasbuffer)
				{
					s = g_engfuncs.pfnInfoKeyValue(szBuffer, "minplayers");
					if (s && '\0' != s[0])
					{
						item->minplayers = atoi(s);
						item->minplayers = V_max(item->minplayers, 0);
						item->minplayers = V_min(item->minplayers, gpGlobals->maxClients);
					}
					s = g_engfuncs.pfnInfoKeyValue(szBuffer, "maxplayers");
					if (s && '\0' != s[0])
					{
						item->maxplayers = atoi(s);
						item->maxplayers = V_max(item->maxplayers, 0);
						item->maxplayers = V_min(item->maxplayers, gpGlobals->maxClients);
					}

					// Remove keys
					//
					g_engfuncs.pfnInfo_RemoveKey(szBuffer, "minplayers");
					g_engfuncs.pfnInfo_RemoveKey(szBuffer, "maxplayers");

					strcpy(item->rulebuffer, szBuffer);
				}

				item->next = cycle->items;
				cycle->items = item;
			}
			else
			{
				ALERT(at_console, "Skipping %s from mapcycle, not a valid map\n", szMap);
			}
		}

		FREE_FILE(aFileList);
	}

	// Fixup circular list pointer
	item = cycle->items;

	// Reverse it to get original order
	while (item)
	{
		next = item->next;
		item->next = newlist;
		newlist = item;
		item = next;
	}
	cycle->items = newlist;
	item = cycle->items;

	// Didn't parse anything
	if (!item)
	{
		return false;
	}

	while (item->next)
	{
		item = item->next;
	}
	item->next = cycle->items;

	cycle->next_item = item->next;

	return true;
}

/*
==============
CountPlayers

Determine the current # of active players on the server for map cycling logic
==============
*/
int CountPlayers()
{
	int num = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pEnt = UTIL_PlayerByIndex(i);

		if (pEnt)
		{
			num = num + 1;
		}
	}

	return num;
}

/*
==============
ExtractCommandString

Parse commands/key value pairs to issue right after map xxx command is issued on server
 level transition
==============
*/
void ExtractCommandString(char* s, char* szCommand)
{
	// Now make rules happen
	char pkey[512];
	char value[512]; // use two buffers so compares
					 // work without stomping on each other
	char* o;

	if (*s == '\\')
		s++;

	while (true)
	{
		o = pkey;
		while (*s != '\\')
		{
			if ('\0' == *s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && '\0' != *s)
		{
			if ('\0' == *s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		strcat(szCommand, pkey);
		if (strlen(value) > 0)
		{
			strcat(szCommand, " ");
			strcat(szCommand, value);
		}
		strcat(szCommand, "\n");

		if ('\0' == *s)
			return;
		s++;
	}
}

/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
// orig 0x1d0340: the 2001 SDK's map cycle (the map after this one in mapcyclefile, else the
// first; no player counts or per-map rules, which halflife-updated added). Rocket Crowbar runs
// <next map>.cfg first with sv_mapcfg.
void CHalfLifeMultiplay::ChangeLevel()
{
	char szNextMap[33]; // VERIFY: the original's are 32 bytes and " %32s" can write 33
	char szFirstMapInList[33];
	strcpy(szFirstMapInList, "bounce"); // the original's default, overwritten at once

	const char* mapcfile = CVAR_GET_STRING("mapcyclefile");

	strncpy(szNextMap, STRING(gpGlobals->mapname), 32);
	szNextMap[32] = 0;
	strncpy(szFirstMapInList, STRING(gpGlobals->mapname), 32);
	szFirstMapInList[32] = 0;

	int length;
	char* pFileList;
	char* aFileList = pFileList = (char*)LOAD_FILE_FOR_ME(mapcfile, &length);
	if (pFileList && 0 != length)
	{
		// the first map in the file
		sscanf(pFileList, " %32s", szNextMap);
		if (IS_MAP_VALID(szNextMap))
			strcpy(szFirstMapInList, szNextMap);

		// keep pulling mapnames out of the list until the current mapname
		// if the current mapname isn't found, load the first map in the list
		bool keeplooking = false;
		while ('\0' != *pFileList)
		{
			while ('\0' != *pFileList && 0 != isspace((unsigned char)*pFileList))
				pFileList++;

			if ('\0' == *pFileList)
				break;

			char cBuf[33];
			int ret = sscanf(pFileList, " %32s", cBuf);
			// Check the map name is valid
			if (ret != 1 || (signed char)cBuf[0] <= 12)
				break;

			if (keeplooking && IS_MAP_VALID(cBuf))
			{
				// the map is valid, so use it
				strcpy(szNextMap, cBuf);
				break;
			}

			if (FStrEq(cBuf, STRING(gpGlobals->mapname)))
				keeplooking = true;

			pFileList += strlen(cBuf);
		}

		FREE_FILE(aFileList);
	}

	if (!IS_MAP_VALID(szNextMap))
		strcpy(szNextMap, szFirstMapInList);

	g_fGameOver = true;

	ALERT(at_console, "CHANGE LEVEL: %s\n", szNextMap);

	if (CVAR_GET_FLOAT("sv_mapcfg") != 0)
	{
		char szCommand[64];
		snprintf(szCommand, sizeof(szCommand), "exec %s.cfg\n", szNextMap);
		SERVER_COMMAND(szCommand);
	}

	CHANGE_LEVEL(szNextMap, NULL);
}

#define MAX_MOTD_CHUNK 64	 // the original's (halflife-updated: 60)
#define MAX_MOTD_LENGTH 1024 // the original's (halflife-updated: 1536)

// orig 0x1d05f0: motd.txt (no motdfile cvar), 64 characters a message, 1024 at most, and no
// ServerName message (InitHUD sends "Rocket Crowbar v19" as the name).
void CHalfLifeMultiplay::SendMOTDToClient(edict_t* client)
{
	// read from the MOTD.txt file
	int length, char_count = 0;
	char* pFileList;
	char* aFileList = pFileList = (char*)LOAD_FILE_FOR_ME("motd.txt", &length);

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	if (pFileList && '\0' != *pFileList)
	{
		while (char_count < MAX_MOTD_LENGTH)
		{
			char chunk[MAX_MOTD_CHUNK + 1];

			if (strlen(pFileList) < MAX_MOTD_CHUNK)
			{
				strcpy(chunk, pFileList);
			}
			else
			{
				strncpy(chunk, pFileList, MAX_MOTD_CHUNK);
				chunk[MAX_MOTD_CHUNK] = 0; // strncpy doesn't always append the null terminator
			}

			char_count += strlen(chunk);
			if (char_count < MAX_MOTD_LENGTH)
				pFileList = aFileList + char_count;
			else
				*pFileList = 0;

			MESSAGE_BEGIN(MSG_ONE, gmsgMOTD, NULL, client);
			WRITE_BYTE(static_cast<int>('\0' == *pFileList)); // false means there is still more message to come
			WRITE_STRING(chunk);
			MESSAGE_END();

			if (!pFileList || '\0' == *pFileList)
				break;
		}
	}

	FREE_FILE(aFileList);
}

//=========================================================
//=========================================================
// Busters Gamerules
//=========================================================
//=========================================================

#define EGON_BUSTING_TIME 10

bool IsBustingGame()
{
	return sv_busters.value == 1;
}

bool IsPlayerBusting(CBaseEntity* pPlayer)
{
	if (!pPlayer || !pPlayer->IsPlayer() || !IsBustingGame())
		return false;

	return ((CBasePlayer*)pPlayer)->HasPlayerItemFromID(WEAPON_EGON);
}

bool BustingCanHaveItem(CBasePlayer* pPlayer, CBaseEntity* pItem)
{
	bool bIsWeaponOrAmmo = false;

	if (strstr(STRING(pItem->pev->classname), "weapon_") || strstr(STRING(pItem->pev->classname), "ammo_"))
	{
		bIsWeaponOrAmmo = true;
	}

	// Busting players can't have ammo nor weapons
	if (IsPlayerBusting(pPlayer) && bIsWeaponOrAmmo)
		return false;

	return true;
}

//=========================================================
CMultiplayBusters::CMultiplayBusters()
{
	m_flEgonBustingCheckTime = -1;
}

//=========================================================
void CMultiplayBusters::Think()
{
	CheckForEgons();

	CHalfLifeMultiplay::Think();
}

//=========================================================
int CMultiplayBusters::IPointsForKill(CBasePlayer* pAttacker, CBasePlayer* pKilled)
{
	// If the attacker is busting, they get a point per kill
	if (IsPlayerBusting(pAttacker))
		return 1;

	// If the victim is busting, then the attacker gets a point
	if (IsPlayerBusting(pKilled))
		return 2;

	return 0;
}

//=========================================================
void CMultiplayBusters::PlayerKilled(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor)
{
	if (IsPlayerBusting(pVictim))
	{
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "The Buster is dead!!");

		// Reset egon check time
		m_flEgonBustingCheckTime = -1;

		CBasePlayer* peKiller = NULL;
		CBaseEntity* ktmp = CBaseEntity::Instance(pKiller);

		if (ktmp && (ktmp->Classify() == CLASS_PLAYER))
		{
			peKiller = (CBasePlayer*)ktmp;
		}
		else if (ktmp && (ktmp->Classify() == CLASS_VEHICLE))
		{
			CBasePlayer* pDriver = ((CFuncVehicle*)ktmp)->m_pDriver;

			if (pDriver != NULL)
			{
				peKiller = pDriver;
				ktmp = pDriver;
				pKiller = pDriver->pev;
			}
		}

		if (peKiller)
		{
			UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("%s has has killed the Buster!\n", STRING(peKiller->pev->netname)));
		}

		pVictim->pev->renderfx = kRenderFxNone;
		pVictim->pev->rendercolor = g_vecZero;
		// pVictim->pev->effects &= ~EF_BRIGHTFIELD;
	}

	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);
}

//=========================================================
void CMultiplayBusters::DeathNotice(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pevInflictor)
{
	// Only death notices that the Buster was involved in in Busting game mode
	if (!IsPlayerBusting(pVictim) && !IsPlayerBusting(CBaseEntity::Instance(pKiller)))
		return;

	CHalfLifeMultiplay::DeathNotice(pVictim, pKiller, pevInflictor);
}

//=========================================================
int CMultiplayBusters::WeaponShouldRespawn(CBasePlayerItem* pWeapon)
{
	if (pWeapon->m_iId == WEAPON_EGON)
		return GR_WEAPON_RESPAWN_NO;

	return CHalfLifeMultiplay::WeaponShouldRespawn(pWeapon);
}


//=========================================================
// CheckForEgons:
// Check to see if any player has an egon
// If they don't then get the lowest player on the scoreboard and give them one
// Then check to see if any weapon boxes out there has an egon, and delete it
//=========================================================
void CMultiplayBusters::CheckForEgons()
{
	if (m_flEgonBustingCheckTime <= 0.0f)
	{
		m_flEgonBustingCheckTime = gpGlobals->time + EGON_BUSTING_TIME;
		return;
	}

	if (m_flEgonBustingCheckTime <= gpGlobals->time)
	{
		m_flEgonBustingCheckTime = -1.0f;

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex(i);

			// Someone is busting, no need to continue
			if (IsPlayerBusting(pPlayer))
				return;
		}

		int bBestFrags = 9999;
		CBasePlayer* pBestPlayer = NULL;

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex(i);

			if (pPlayer && pPlayer->pev->frags <= bBestFrags)
			{
				bBestFrags = pPlayer->pev->frags;
				pBestPlayer = pPlayer;
			}
		}

		if (pBestPlayer)
		{
			pBestPlayer->GiveNamedItem("weapon_egon");

			CBaseEntity* pEntity = NULL;

			// Find a weaponbox that includes an Egon, then destroy it
			while ((pEntity = UTIL_FindEntityByClassname(pEntity, "weaponbox")) != NULL)
			{
				CWeaponBox* pWeaponBox = (CWeaponBox*)pEntity;

				if (pWeaponBox)
				{
					CBasePlayerItem* pWeapon;

					for (int i = 0; i < MAX_ITEM_TYPES; i++)
					{
						pWeapon = pWeaponBox->m_rgpPlayerItems[i];

						while (pWeapon)
						{
							// There you are, bye box
							if (pWeapon->m_iId == WEAPON_EGON)
							{
								pWeaponBox->Kill();
								break;
							}

							pWeapon = pWeapon->m_pNext;
						}
					}
				}
			}
		}
	}
}

//=========================================================
bool CMultiplayBusters::CanHavePlayerItem(CBasePlayer* pPlayer, CBasePlayerItem* pItem)
{
	// Buster cannot have more weapons nor ammo
	if (BustingCanHaveItem(pPlayer, pItem) == false)
	{
		return false;
	}

	return CHalfLifeMultiplay::CanHavePlayerItem(pPlayer, pItem);
}

//=========================================================
bool CMultiplayBusters::CanHaveItem(CBasePlayer* pPlayer, CItem* pItem)
{
	// Buster cannot have more weapons nor ammo
	if (BustingCanHaveItem(pPlayer, pItem) == false)
	{
		return false;
	}

	return CHalfLifeMultiplay::CanHaveItem(pPlayer, pItem);
}

//=========================================================
void CMultiplayBusters::PlayerGotWeapon(CBasePlayer* pPlayer, CBasePlayerItem* pWeapon)
{
	if (pWeapon->m_iId == WEAPON_EGON)
	{
		pPlayer->RemoveAllItems(false);

		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Long live the new Buster!");
		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("%s is busting!\n", STRING(pPlayer->pev->netname)));

		SetPlayerModel(pPlayer);

		pPlayer->pev->health = pPlayer->pev->max_health;
		pPlayer->pev->armorvalue = 100;

		pPlayer->pev->renderfx = kRenderFxGlowShell;
		pPlayer->pev->renderamt = 25;
		pPlayer->pev->rendercolor = Vector(0, 75, 250);

		CBasePlayerWeapon* pEgon = (CBasePlayerWeapon*)pWeapon;

		pEgon->m_iDefaultAmmo = 100;
		pPlayer->m_rgAmmo[pEgon->m_iPrimaryAmmoType] = pEgon->m_iDefaultAmmo;

		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "ivan");
	}
}

void CMultiplayBusters::ClientUserInfoChanged(CBasePlayer* pPlayer, char* infobuffer)
{
	SetPlayerModel(pPlayer);

	// Set preferences
	pPlayer->SetPrefsFromUserinfo(infobuffer);
}

void CMultiplayBusters::PlayerSpawn(CBasePlayer* pPlayer)
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);
	SetPlayerModel(pPlayer);
}

void CMultiplayBusters::SetPlayerModel(CBasePlayer* pPlayer)
{
	if (IsPlayerBusting(pPlayer))
	{
		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "ivan");
	}
	else
	{
		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "skeleton");
	}
}
