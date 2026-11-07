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
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include <algorithm>
#include <string>
#include <vector>

#include "extdll.h"
#include "util.h"
#include "filesystem_utils.h"
#include "cbase.h"
#include "com_model.h"
#include "saverestore.h"
#include "player.h"
#include "spectator.h"
#include "client.h"
#include "soundent.h"
#include "gamerules.h"
#include "game.h"
#include "customentity.h"
#include "weapons.h"
#include "weaponinfo.h"
#include "usercmd.h"
#include "netadr.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "UserMessages.h"
#include "flag.h"
#include "Preff.h"
#include "vote.h"
#include "pregame.h"
#include "nm.h"
#include "wog.h"
#include "driveent.h"
#include "monsters.h"
#include "skill.h"

DLL_GLOBAL unsigned int g_ulFrameCount;

extern void CopyToBodyQue(entvars_t* pev);

void LinkUserMessages();

extern const char* GetTeamNameForIndex(int iTeam); // teamplay_gamerules.cpp
extern void ThrowFakeGibs(entvars_t* pevVictim, bool bHuman); // combat.cpp
extern bool g_bUnsafeCreate;						// cbase.cpp: CBaseEntity::Create may make anything
extern float g_flWeaponCheat;						// world.cpp: the "fcheats" level, read by give and fov

// Rocket Crowbar 1.9: client.cpp's own globals.
float g_flLastTime;			 // orig 0x280bf0, gpGlobals->time at the last StartFrame. Nothing reads it.
int g_iRndHack;				 // orig 0x280be8, never used in the original
int g_iEMCountUsedPCs;		 // orig 0x280be0, reset by CWorld::Precache, never used otherwise
int g_iEMCountUsedSCStrs;	 // orig 0x280be4, how many of g_szEmanipSCStrs are in use
char g_szEmanipSCStrs[256][64]; // orig 0x307280, the strings EManipMakeSCStrConst hands out

// orig 0x280bc0: the use types "useent" takes by name
sEManipEnum g_esUseTypes[4] = {
	{"off", USE_OFF},
	{"on", USE_ON},
	{"set", USE_SET},
	{"toggle", USE_TOGGLE},
};

/*
 * used by kill command and disconnect command
 * ROBIN: Moved here from player.cpp, to allow multiple player models
 */
// orig 0xfd3e0
void set_suicide_frame(entvars_t* pev)
{
	if (!FStrEq(STRING(pev->model), "models/player.mdl"))
		return; // allready gibbed

	//	pev->frame		= $deatha11;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->deadflag = DEAD_DEAD;
	pev->nextthink = -1;
}


/*
===========
ClientConnect

called when a player connects to a server
============
*/
// orig 0xfd450
qboolean ClientConnect(edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128])
{
	return static_cast<qboolean>(g_pGameRules->ClientConnected(pEntity, pszName, pszAddress, szRejectReason));

	// a client connecting during an intermission can cause problems
	//	if (intermission_running)
	//		ExitIntermission ();
}


/*
===========
ClientDisconnect

called when a player disconnects from a server

GLOBALS ASSUMED SET:  g_fGameOver
============
*/
// orig 0xfd490. Rocket Crowbar drops the leaver's CTF flag, tells every entity (the drive
// monsters let go of their driver) and the kick vote, and puts the client's speed cvars back.
void ClientDisconnect(edict_t* pEntity)
{
	if (g_fGameOver)
		return;

	char text[256];
	snprintf(text, sizeof(text), "- %s has left the game\n", STRING(pEntity->v.netname));
	MESSAGE_BEGIN(MSG_ALL, gmsgSayText, NULL);
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	CBaseEntity* pPlayer = CBaseEntity::Instance(pEntity);
	CBaseFlag* pFlag;
	if (pPlayer && (pFlag = DoesEntHaveFlag(pPlayer)) != NULL)
	{
		pFlag->FlagDrop(pPlayer->pev);
		snprintf(text, sizeof(text), "* The %s (%s) flag was dropped!\n", GetTeamNameForIndex(pFlag->m_iTeam), GetFlagTeamColor(pFlag->m_iTeam));
		UTIL_SayTextAll(text, pPlayer);
	}

	DispatchDissconnect(pEntity);
	VK_VoteDissconnect(pEntity);

	if (0 == pEntity->free)
		CLIENT_COMMAND(pEntity, "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");

	CSound* pSound;
	pSound = CSoundEnt::SoundPointerForIndex(CSoundEnt::ClientSoundIndex(pEntity));
	{
		// since this client isn't around to think anymore, reset their sound.
		if (pSound)
		{
			pSound->Reset();
		}
	}

	// since the edict doesn't get deleted, fix it so it doesn't interfere.
	pEntity->v.takedamage = DAMAGE_NO; // don't attract autoaim
	pEntity->v.solid = SOLID_NOT;	   // nonsolid
	UTIL_SetOrigin(&pEntity->v, pEntity->v.origin);

	// halflife-updated's fix, kept: a func_tank the player was using is let go, so it isn't left
	// controlled by a client that is gone (halflife #2594); the original doesn't
	auto pTankUser = reinterpret_cast<CBasePlayer*>(GET_PRIVATE(pEntity));

	if (pTankUser)
	{
		if (pTankUser->m_pTank != NULL)
		{
			pTankUser->m_pTank->Use(pTankUser, pTankUser, USE_OFF, 0);
			pTankUser->m_pTank = NULL;
		}
	}

	g_pGameRules->ClientDisconnected(pEntity);
}


// called by ClientKill and DeadThink
// orig 0xfd670
void respawn(entvars_t* pev, bool fCopyCorpse)
{
	if (0 != gpGlobals->coop || 0 != gpGlobals->deathmatch)
	{
		if (fCopyCorpse)
		{
			// make a copy of the dead body for appearances sake
			CopyToBodyQue(pev);
		}

		// respawn player
		GetClassPtr((CBasePlayer*)pev)->Spawn();
	}
	else
	{ // restart the entire server
		SERVER_COMMAND("reload\n");
	}
}

/*
============
ClientKill

Player entered the suicide command

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
============
*/
// orig 0xfd6f0
void ClientKill(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;

	CBasePlayer* pl = (CBasePlayer*)CBasePlayer::Instance(pev);

	if (pl->m_fNextSuicideTime > gpGlobals->time)
		return; // prevent suiciding too ofter

	pl->m_fNextSuicideTime = gpGlobals->time + 1; // don't let them suicide for 5 seconds after suiciding

	// have the player kill themself
	pev->health = 0;
	pl->Killed(pev, GIB_NEVER);

	//	pev->modelindex = g_ulModelIndexPlayer;
	//	pev->frags -= 2;		// extra penalty
	//	respawn( pev );
}

/*
===========
ClientPutInServer

called each time a player is spawned
============
*/
// orig 0xfd780
void ClientPutInServer(edict_t* pEntity)
{
	CBasePlayer* pPlayer;

	entvars_t* pev = &pEntity->v;

	pPlayer = GetClassPtr((CBasePlayer*)pev);
	pPlayer->SetCustomDecalFrames(-1); // Assume none;

	// the original clears this here, so the game HUD is sent again (orig +0x444)
	if (pPlayer)
		pPlayer->m_fGameHUDInitialized = false;

	// Allocate a CBasePlayer for pev, and call spawn
	pPlayer->Spawn();

	// Reset interpolation during first frame
	pPlayer->pev->effects |= EF_NOINTERP;
}

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;



#if defined(_MSC_VER) || defined(WIN32)
typedef wchar_t uchar16;
typedef unsigned int uchar32;
#else
typedef unsigned short uchar16;
typedef wchar_t uchar32;
#endif

//-----------------------------------------------------------------------------
// Purpose: determine if a uchar32 represents a valid Unicode code point
//-----------------------------------------------------------------------------
bool Q_IsValidUChar32(uchar32 uVal)
{
	// Values > 0x10FFFF are explicitly invalid; ditto for UTF-16 surrogate halves,
	// values ending in FFFE or FFFF, or values in the 0x00FDD0-0x00FDEF reserved range
	return (uVal < 0x110000u) && ((uVal - 0x00D800u) > 0x7FFu) && ((uVal & 0xFFFFu) < 0xFFFEu) && ((uVal - 0x00FDD0u) > 0x1Fu);
}


// Decode one character from a UTF-8 encoded string. Treats 6-byte CESU-8 sequences
// as a single character, as if they were a correctly-encoded 4-byte UTF-8 sequence.
int Q_UTF8ToUChar32(const char* pUTF8_, uchar32& uValueOut, bool& bErrorOut)
{
	const uint8* pUTF8 = (const uint8*)pUTF8_;

	int nBytes = 1;
	uint32 uValue = pUTF8[0];
	uint32 uMinValue = 0;

	// 0....... single byte
	if (uValue < 0x80)
		goto decodeFinishedNoCheck;

	// Expecting at least a two-byte sequence with 0xC0 <= first <= 0xF7 (110...... and 11110...)
	if ((uValue - 0xC0u) > 0x37u || (pUTF8[1] & 0xC0) != 0x80)
		goto decodeError;

	uValue = (uValue << 6) - (0xC0 << 6) + pUTF8[1] - 0x80;
	nBytes = 2;
	uMinValue = 0x80;

	// 110..... two-byte lead byte
	if ((uValue & (0x20 << 6)) == 0)
		goto decodeFinished;

	// Expecting at least a three-byte sequence
	if ((pUTF8[2] & 0xC0) != 0x80)
		goto decodeError;

	uValue = (uValue << 6) - (0x20 << 12) + pUTF8[2] - 0x80;
	nBytes = 3;
	uMinValue = 0x800;

	// 1110.... three-byte lead byte
	if ((uValue & (0x10 << 12)) == 0)
		goto decodeFinishedMaybeCESU8;

	// Expecting a four-byte sequence, longest permissible in UTF-8
	if ((pUTF8[3] & 0xC0) != 0x80)
		goto decodeError;

	uValue = (uValue << 6) - (0x10 << 18) + pUTF8[3] - 0x80;
	nBytes = 4;
	uMinValue = 0x10000;

	// 11110... four-byte lead byte. fall through to finished.

decodeFinished:
	if (uValue >= uMinValue && Q_IsValidUChar32(uValue))
	{
	decodeFinishedNoCheck:
		uValueOut = uValue;
		bErrorOut = false;
		return nBytes;
	}
decodeError:
	uValueOut = '?';
	bErrorOut = true;
	return nBytes;

decodeFinishedMaybeCESU8:
	// Do we have a full UTF-16 surrogate pair that's been UTF-8 encoded afterwards?
	// That is, do we have 0xD800-0xDBFF followed by 0xDC00-0xDFFF? If so, decode it all.
	if ((uValue - 0xD800u) < 0x400u && pUTF8[3] == 0xED && (uint8)(pUTF8[4] - 0xB0) < 0x10 && (pUTF8[5] & 0xC0) == 0x80)
	{
		uValue = 0x10000 + ((uValue - 0xD800u) << 10) + ((uint8)(pUTF8[4] - 0xB0) << 6) + pUTF8[5] - 0x80;
		nBytes = 6;
		uMinValue = 0x10000;
	}
	goto decodeFinished;
}



//-----------------------------------------------------------------------------
// Purpose: Returns true if UTF-8 string contains invalid sequences.
//-----------------------------------------------------------------------------
bool Q_UnicodeValidate(const char* pUTF8)
{
	bool bError = false;
	while ('\0' != *pUTF8)
	{
		uchar32 uVal;
		// Our UTF-8 decoder silently fixes up 6-byte CESU-8 (improperly re-encoded UTF-16) sequences.
		// However, these are technically not valid UTF-8. So if we eat 6 bytes at once, it's an error.
		int nCharSize = Q_UTF8ToUChar32(pUTF8, uVal, bError);
		if (bError || nCharSize == 6)
			return false;
		pUTF8 += nCharSize;
	}
	return true;
}

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
// orig 0xfd7e0. The 2001 SDK's say: no chat flood timer, no spectator prefix, no voice-mute
// check and no log line (halflife-updated added all four).
void Host_Say(edict_t* pEntity, bool teamonly)
{
	CBasePlayer* client;
	int j;
	char* p;
	char text[192];
	char szTemp[256];
	const char* cpSay = "say";
	const char* cpSayTeam = "say_team";
	const char* pcmd = CMD_ARGV(0);

	// We can get a raw string now, without the "say " prepended
	if (CMD_ARGC() == 0)
		return;

	if (!stricmp(pcmd, cpSay) || !stricmp(pcmd, cpSayTeam))
	{
		if (CMD_ARGC() >= 2)
		{
			p = (char*)CMD_ARGS();
		}
		else
		{
			// say with a blank message, nothing to do
			return;
		}
	}
	else // Raw text, need to prepend argv[0]
	{
		if (CMD_ARGC() >= 2)
		{
			snprintf(szTemp, sizeof(szTemp), "%s %s", (char*)pcmd, (char*)CMD_ARGS());
		}
		else
		{
			// Just a one word command, use the first word...sigh
			snprintf(szTemp, sizeof(szTemp), "%s", (char*)pcmd);
		}
		p = szTemp;
	}

	// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p) - 1] = 0;
	}

	// make sure the text has content
	char* pc;
	for (pc = p; pc != NULL && *pc != 0; pc++)
	{
		if (0 != isprint((unsigned char)*pc) && 0 == isspace((unsigned char)*pc))
		{
			pc = NULL; // we've found an alphanumeric character,  so text is valid
			break;
		}
	}
	if (pc != NULL)
		return; // no character found, so say nothing

	// halflife-updated: drop text the client can't decode
	if (!Q_UnicodeValidate(p))
		return;

	// turn on color set 2  (color on,  no sound)
	if (teamonly)
		sprintf(text, "%c(TEAM) %s: ", 2, STRING(pEntity->v.netname));
	else
		sprintf(text, "%c%s: ", 2, STRING(pEntity->v.netname));

	j = sizeof(text) - 2 - strlen(text); // -2 for /n and null terminator
	if ((int)strlen(p) > j)
		p[j] = 0;

	strcat(text, p);
	strcat(text, "\n");
	text[sizeof(text) - 1] = 0;

	// loop through all players
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		client = (CBasePlayer*)UTIL_PlayerByIndex(i);
		if (!client || !client->pev)
			continue;

		edict_t* pClientEdict = client->edict();
		if (!pClientEdict || !pClientEdict->pvPrivateData || 0 != pClientEdict->free)
			continue;

		if (pClientEdict == pEntity)
			continue;

		if (!(client->IsNetClient())) // Not a client ? (should never be true)
			continue;

		if (teamonly && g_pGameRules->PlayerRelationship(client, CBaseEntity::Instance(pEntity)) != GR_TEAMMATE)
			continue;

		MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, pClientEdict);
		WRITE_BYTE(ENTINDEX(pEntity));
		WRITE_STRING(text);
		MESSAGE_END();
	}

	// print to the sending client
	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, pEntity->v.pContainingEntity);
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	// echo to server console
	g_engfuncs.pfnServerPrint(text);
}

// Rocket Crowbar 1.9. orig 0xfdbf0: "say_private <name or index> <text>", a message to one
// player (and an echo to the sender).
void SayPrivate(edict_t* pEntity)
{
	if (CMD_ARGC() < 3)
		return;

	CBaseEntity* pTarget = NULL;

	// an all-digits first argument is a client index
	bool bDigits = false;
	const char* pc;
	for (pc = CMD_ARGV(1); *pc != 0; pc++)
	{
		if (0 == isdigit((unsigned char)*pc))
			break;
		bDigits = true;
	}

	if (*pc == 0 && bDigits)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(atoi(CMD_ARGV(1)));
		if (pPlayer && pPlayer->IsNetClient() && pPlayer->edict() != pEntity)
			pTarget = pPlayer;
	}

	if (!pTarget)
	{
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
			if (!pPlayer || !pPlayer->pev)
				continue;
			edict_t* pEdict = pPlayer->edict();
			if (!pEdict || !pEdict->pvPrivateData || 0 != pEdict->free || pEdict == pEntity)
				continue;
			if (!pPlayer->IsNetClient())
				continue;
			if (!stricmp(STRING(pPlayer->pev->netname), CMD_ARGV(1)))
			{
				pTarget = pPlayer;
				break;
			}
		}
	}

	if (!pTarget || !pTarget->IsNetClient())
		return;

	// VERIFY: guard, the original can overrun its stack here: it strncat's each word (up to 192
	// chars at a time) into a 256-byte buffer, then cuts it at 191. Ours builds it in a
	// std::string and cuts it the same way.
	char prefix[256];
	snprintf(prefix, sizeof(prefix), "%c(PRIVATE <%s>) %s: ", 2, STRING(pTarget->pev->netname), STRING(pEntity->v.netname));
	std::string strText = prefix;
	for (int i = 2; i < CMD_ARGC(); i++)
	{
		strText.append(CMD_ARGV(i), 0, 192);
		strText += (i + 1 < CMD_ARGC()) ? " " : "\n";
	}
	if (strText.size() > 191)
		strText.resize(191);
	const char* text = strText.c_str();

	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, pTarget->edict());
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgSayText, NULL, pEntity->v.pContainingEntity);
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	g_engfuncs.pfnServerPrint(text);
}

// Rocket Crowbar 1.9. orig 0xfe080: is this client the listen server's own player (client 1 of
// a non-dedicated server)?
bool IsClientListenServer(edict_t* pEntity)
{
	CBaseEntity* pPlayer = CBaseEntity::Instance(pEntity);
	if (!pPlayer)
		return false;
	if (!pPlayer->pev || 0 == OFFSET(pPlayer->edict()))
		return false;
	if (!pPlayer->IsNetClient())
		return false;
	if (0 != IS_DEDICATED_SERVER())
		return false;
	CBaseEntity* pHost = UTIL_PlayerByIndex(1);
	return pHost && pHost->pev == pPlayer->pev;
}

// The two admin lists: a cvar of client indices separated by ";: \t\n", plus the listen server's
// own player.
static bool IsClientInIndexList(edict_t* pEntity, const char* szCvar)
{
	CBaseEntity* pPlayer = CBaseEntity::Instance(pEntity);
	if (!pPlayer)
		return false;
	if (!pPlayer->pev || 0 == OFFSET(pPlayer->edict()))
		return false;
	if (!pPlayer->IsNetClient())
		return false;

	char szList[512];
	strncpy(szList, CVAR_GET_STRING(szCvar), sizeof(szList) - 1); // the original strcpy's it
	szList[sizeof(szList) - 1] = 0;
	for (char* pTok = strtok(szList, ";: \t\n"); pTok; pTok = strtok(NULL, ";: \t\n"))
	{
		CBaseEntity* pIndexed = UTIL_PlayerByIndex(atoi(pTok));
		if (pIndexed && pIndexed->pev == pPlayer->pev)
			return true;
	}
	return IsClientListenServer(pEntity);
}

// Rocket Crowbar 1.9. orig 0xfe130: may this client use the admin commands (sv_serverclient)?
bool IsClientServer(edict_t* pEntity)
{
	return IsClientInIndexList(pEntity, "sv_serverclient");
}

// Rocket Crowbar 1.9. orig 0xfe350: may this client use "useent"? The cvar, sv_kersplatis, is
// registered nowhere (in the original too), so only sv_serverclient admins and the listen
// server's player can.
bool IsClientAllowedToUse(edict_t* pEntity)
{
	return IsClientInIndexList(pEntity, "sv_kersplatis");
}

// Rocket Crowbar 1.9. orig 0xfe570: echo an admin command to the server console and log.
void LogClientServerCommand(edict_t* pEntity)
{
	if (IsClientListenServer(pEntity))
		return;

	char szCmd[384];
	if (CMD_ARGC() < 2)
		snprintf(szCmd, sizeof(szCmd), "%s", CMD_ARGV(0));
	else
		snprintf(szCmd, sizeof(szCmd), "%s %s", CMD_ARGV(0), CMD_ARGS());

	g_engfuncs.pfnServerPrint(UTIL_VarArgs("Server Cmd (%s): %s\n", STRING(pEntity->v.netname), szCmd));
	UTIL_LogPrintf("\"%s<%i>\" Server Cmd: %s\n", STRING(pEntity->v.netname), GETPLAYERUSERID(pEntity), szCmd);
}

// Rocket Crowbar 1.9. orig 0xfe690: what the level-custom (.lec) commands act on. The entity the
// player looks at (vecEnd gets the trace end), or if the trace hits nothing within 8192 units,
// the first entity within 32 units of the player's feet (vecEnd is then left as it was).
edict_t* LCSFindEntityForward(entvars_t* pev, Vector& vecEnd)
{
	TraceResult tr;

	UTIL_MakeVectors(pev->v_angle);
	Vector vecSrc = pev->origin + pev->view_ofs;
	UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, pev->pContainingEntity, &tr);

	if (tr.flFraction == 1.0 && (!tr.pHit || 0 == OFFSET(tr.pHit)))
	{
		CBaseEntity* pEntity = NULL;
		while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin + Vector(0, 0, pev->mins.z), 32)) != NULL)
		{
			edict_t* pEdict = pEntity->pev->pContainingEntity;
			if (!pEdict || 0 == OFFSET(pEdict))
				continue;
			if (0 == ENTINDEX(pEntity->pev->pContainingEntity) || pEntity->pev == pev)
				continue;
			return pEntity->pev->pContainingEntity;
		}
		return NULL;
	}

	vecEnd = tr.vecEndPos;
	return tr.pHit;
}

// Rocket Crowbar 1.9. orig 0xfe8b0: the entity the entity-manipulation commands act on, the one
// the player looks at (never the world).
CBaseEntity* EManipFindEntForward(entvars_t* pev)
{
	TraceResult tr;

	UTIL_MakeVectors(pev->v_angle);
	Vector vecSrc = pev->origin + pev->view_ofs;
	UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, pev->pContainingEntity, &tr);

	if (tr.flFraction == 1.0 && (!tr.pHit || 0 == OFFSET(tr.pHit)))
		return NULL;

	CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
	if (!pEntity || !pEntity->pev || 0 == OFFSET(pEntity->edict()))
		return NULL;

	edict_t* pEdict = pEntity->pev->pContainingEntity;
	if (0 == pEdict->free && pEdict->pvPrivateData && strcmp(STRING(pEntity->pev->classname), "worldspawn"))
		return pEntity;
	return NULL;
}

// Rocket Crowbar 1.9. orig 0xfea70: a bare CBaseEntity with this classname, flagged to be
// removed. Nothing calls it.
edict_t* EManipGetTempEntWithClassName(char* szClassname)
{
	CBaseEntity* pEntity = GetClassPtr((CBaseEntity*)NULL);
	pEntity->Spawn();
	// VERIFY: the classname points at the caller's buffer, as in the original (not ALLOC_STRING)
	if (strlen(szClassname) < 64)
		pEntity->pev->classname = MAKE_STRING(szClassname);
	else
		pEntity->pev->classname = MAKE_STRING("worldspawn");
	pEntity->pev->flags |= FL_KILLME;
	return pEntity->edict();
}

// Rocket Crowbar 1.9. orig 0xfeb00: the value of the named entry, or the first entry's value.
int EManipGetEnumArrayValue(char* szName, sEManipEnum* pEnums, int iCount)
{
	for (int i = 0; i < iCount; i++)
	{
		if (!stricmp(pEnums[i].szName, szName))
			return pEnums[i].iValue;
	}
	return pEnums[0].iValue;
}

// Rocket Crowbar 1.9. orig 0xfeb60: print each entry's bit of iBits to the client. Nothing calls it.
void EManipPrintBits(int iBits, sEManipEnum* pEnums, int iCount, edict_t* pEntity)
{
	for (int i = 0; i < iCount; i++)
		CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("  %s  %i\n", pEnums[i].szName, iBits & pEnums[i].iValue));
}

// Rocket Crowbar 1.9. orig 0xfebd0: toggle the named entry's bits in iBits. Nothing calls it.
void EManipSetBits(int& iBits, char* szName, sEManipEnum* pEnums, int iCount)
{
	int iValue = -1;
	for (int i = 0; i < iCount; i++)
	{
		if (!stricmp(pEnums[i].szName, szName))
		{
			iValue = pEnums[i].iValue;
			break;
		}
	}
	if (iValue == -1)
		return;
	if ((iValue & iBits) == 0)
		iBits |= iValue;
	else
		iBits &= ~iValue;
}

// Rocket Crowbar 1.9. orig 0xfec40: a string_t for this text, kept in g_szEmanipSCStrs (256 of
// 64 chars, the same text reuses its slot, the last slot is reused once they run out).
int EManipMakeSCStrConst(const char* szString)
{
	int i;
	for (i = 0; i < g_iEMCountUsedSCStrs; i++)
	{
		if (!strcmp(szString, g_szEmanipSCStrs[i]))
			return MAKE_STRING(g_szEmanipSCStrs[i]);
	}

	if (g_iEMCountUsedSCStrs < 256)
		i = g_iEMCountUsedSCStrs++;
	else
		i = 255;
	// VERIFY: guard, the original crashes here (or corrupts the next slots): it strcpy's with no
	// length check. Ours cuts the text at 63.
	strncpy(g_szEmanipSCStrs[i], szString, sizeof(g_szEmanipSCStrs[i]) - 1);
	g_szEmanipSCStrs[i][sizeof(g_szEmanipSCStrs[i]) - 1] = 0;
	return MAKE_STRING(g_szEmanipSCStrs[i]);
}

// Rocket Crowbar 1.9. orig 0xfed00: the string_t of the text after "=" in "key=value", or of " ".
int EManipSCGetEqualTok(char* szString)
{
	char* pTok = strtok(szString, "=");
	if (pTok && (pTok = strtok(NULL, "=")) != NULL)
		return EManipMakeSCStrConst(pTok);
	return MAKE_STRING(" ");
}

// Rocket Crowbar 1.9. orig 0xfed60: the text after "=" in "key=value", or "".
char* EManipSCGetEqualTokTemp(char* szString)
{
	char* pTok = strtok(szString, "=");
	if (pTok && (pTok = strtok(NULL, "=")) != NULL)
		return pTok;
	return (char*)"";
}

// Rocket Crowbar 1.9. orig 0xfedb0: show text in the client's MOTD window, 64 characters a
// message, at most 1024 characters. Nothing calls it.
void SendTextAsMOTD(const char* szText, edict_t* pEntity)
{
	int iSent = 0;
	while (*szText != 0)
	{
		if (iSent > 1023)
			return;

		char chunk[65]; // 64 characters a message
		strncpy(chunk, szText, 64);
		chunk[64] = 0;
		szText += strlen(chunk);
		iSent += strlen(chunk);

		MESSAGE_BEGIN(MSG_ONE, gmsgMOTD, NULL, pEntity);
		WRITE_BYTE((*szText == 0 || iSent > 1023) ? 1 : 0);
		WRITE_STRING(chunk);
		MESSAGE_END();
	}
}

// Rocket Crowbar 1.9. orig 0xfeec0: does szItem start with szPrefix (no case)?
bool CheckItemStrEqu(char* szItem, char* szPrefix)
{
	return 0 == strnicmp(szItem, szPrefix, strlen(szPrefix));
}

// Rocket Crowbar 1.9. CPopOMatic (client.cpp): squeezes a victim's view (FOV up to 160) and
// glows it for a quarter second, then gibs it. Nothing in the original creates one.
class CPopOMatic : public CBaseEntity
{
public:
	static CPopOMatic* CreatePOM(edict_t* pVictim, edict_t* pOwner);
	void EXPORT POMThink();
};

// orig 0xfef10
CPopOMatic* CPopOMatic::CreatePOM(edict_t* pVictim, edict_t* pOwner)
{
	CPopOMatic* pPOM = GetClassPtr((CPopOMatic*)NULL);
	pPOM->SetThink(&CPopOMatic::POMThink);
	pPOM->pev->nextthink = gpGlobals->time + 0.01;
	pPOM->pev->animtime = gpGlobals->time;
	pPOM->pev->dmg = 0.25;
	pPOM->pev->dmgtime = gpGlobals->time + pPOM->pev->dmg;
	pPOM->pev->classname = MAKE_STRING("pop-o-matic");
	pPOM->pev->armorvalue = gpGlobals->time; // the last think

	// keep the victim's render settings to put back, and glow it
	entvars_t* pevVictim = VARS(pVictim);
	pPOM->pev->renderfx = pevVictim->renderfx;
	pPOM->pev->rendercolor = pevVictim->rendercolor;
	pevVictim->renderfx = kRenderFxExplode;
	pevVictim->rendercolor = Vector(255, 255, 0);

	pPOM->pev->owner = pOwner;
	pPOM->pev->enemy = pVictim;
	return pPOM;
}

// orig 0xff0a0
void CPopOMatic::POMThink()
{
	float flInterval = gpGlobals->time - pev->armorvalue;
	pev->nextthink = gpGlobals->time + 0.01;
	pev->armorvalue = gpGlobals->time;

	if (pev->owner && !UTIL_IsValidEntity(pev->owner))
		pev->owner = pev->pContainingEntity;

	if (UTIL_IsValidEntity(pev->enemy))
	{
		CBaseEntity* pVictim = CBaseEntity::Instance(pev->enemy);
		if (!pVictim)
			return;

		if (!pVictim->IsAlive())
		{
			SetThink(&CPopOMatic::SUB_Remove);
			return;
		}

		if (pVictim->IsPlayer())
		{
			CBasePlayer* pPlayer = (CBasePlayer*)pVictim;
			if (pPlayer->m_iFOV < 90)
				pPlayer->m_iFOV = 90;
			int iFOV = pPlayer->m_iFOV;
			float flAdd = ((pev->dmgtime - gpGlobals->time) / pev->dmg) * (160 - pPlayer->m_iFOV) * flInterval;
			if (flAdd < 1.0)
				flAdd = 1.0;
			pPlayer->m_iFOV = (int)(flAdd + iFOV);
			if (pPlayer->m_iFOV > 160)
				pPlayer->m_iFOV = 160;
		}

		if (!(pev->dmgtime < gpGlobals->time))
			return;

		pVictim->pev->renderfx = pev->renderfx;
		pVictim->pev->rendercolor = pev->rendercolor;

		// VERIFY: HasHumanGibs is a CBaseMonster virtual; the original calls it through whatever
		// the victim is (only monsters and players are alive).
		CBaseMonster* pMonster = static_cast<CBaseMonster*>(pVictim);
		CGib::SpawnRandomGibs(pVictim->pev, 10, pMonster->HasHumanGibs());
		ThrowFakeGibs(pVictim->pev, pMonster->HasHumanGibs());

		pVictim->TakeDamage(pev, VARS(pev->owner), 9999, DMG_ALWAYSGIB);

		if (pVictim && pVictim->pev && UTIL_IsValidEntity(pVictim->edict()) && pVictim->IsAlive())
			pVictim->Killed(VARS(pev->owner), GIB_NEVER);
	}

	SetThink(&CPopOMatic::SUB_Remove);
}


/*
===========
ClientCommand
called each time a player uses a "cmd" command
============
*/

// The .lec file of the current map, gamedir/maps/<map>.lec (the level-custom commands append to it).
static void LCSMapFilePath(char* szPath, int iSize)
{
	char szGameDir[512];
	GET_GAME_DIR(szGameDir);
	snprintf(szPath, iSize, "%s/maps/%s.lec", szGameDir, STRING(gpGlobals->mapname));
}

// "give"/"giveitem"/"givefor": allowed with sv_allowgives 1, and then either for an admin or,
// with fcheats on, for a living, unfrozen, non-observer player who named something.
static bool CanClientGive(edict_t* pEntity, entvars_t* pev, bool bObserver, bool bAlive)
{
	if (g_flWeaponCheat != 0 && CMD_ARGC() > 1 && (pev->flags & FL_FROZEN) == 0 && !bObserver && bAlive)
		return true;
	return IsClientServer(pEntity);
}

// "lcputent"/"lcrespawnent": the coordinates written, the player's origin and either its view
// angles ("<cmd> classname") or its yaw ("<cmd> classname anything").
static void LCSPutCoords(entvars_t* pev, float* flVals)
{
	// VERIFY: with any other argument count the original leaves these six uninitialized; ours writes zeros.
	if (CMD_ARGC() == 2)
	{
		flVals[0] = pev->origin.x;
		flVals[1] = pev->origin.y;
		flVals[2] = pev->origin.z;
		flVals[3] = pev->v_angle.x;
		flVals[4] = pev->v_angle.y;
		flVals[5] = pev->v_angle.z;
	}
	else if (CMD_ARGC() == 3)
	{
		flVals[0] = pev->origin.x;
		flVals[1] = pev->origin.y;
		flVals[2] = pev->origin.z;
		flVals[3] = 0;
		flVals[4] = pev->v_angle.y;
		flVals[5] = 0;
	}
}

// "lcrement"/"lckillent"/"lcuseent": write "<szWord> classname x y z" for the entity looked at
// and tell the player. All three say "Using" (lcrement says "Removing") as in the original.
static void LCSEntCommand(edict_t* pEntity, entvars_t* pev, const char* szWord, const char* szMsg)
{
	LogClientServerCommand(pEntity);

	char szPath[512];
	LCSMapFilePath(szPath, sizeof(szPath));
	FILE* pFile = fopen(szPath, "a");
	if (!pFile)
	{
		CLIENT_PRINTF(pEntity, print_console, "Cant append to file, does directory exist?\n");
		return;
	}

	// VERIFY: guard: when the entity was found by the sphere search, the original prints this
	// vector unfilled (stack garbage); ours prints zeros.
	Vector vecEnd = g_vecZero;
	edict_t* pFound = LCSFindEntityForward(pev, vecEnd);
	if (pFound)
	{
		fprintf(pFile, "%s %s %f %f %f\n", szWord, STRING(pFound->v.classname), vecEnd.x, vecEnd.y, vecEnd.z);
		CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs(szMsg, STRING(VARS(pFound)->classname), vecEnd.x, vecEnd.y, vecEnd.z));
	}
	fclose(pFile);
}

// Use CMD_ARGV,  CMD_ARGV, and CMD_ARGC to get pointers the character string command.
// orig 0xff3f0. Rocket Crowbar's version: its gives, nuke, eject, observer mode, the level-custom
// (lc*) and entity commands, client settings, votes, prefs and the admin commands.
void ClientCommand(edict_t* pEntity)
{
	const char* pcmd = CMD_ARGV(0);
	const char* pstr;

	// Is the client spawned yet?
	if (!pEntity->pvPrivateData)
		return;

	CBasePlayer* pPlayerEnt = (CBasePlayer*)CBaseEntity::Instance(pEntity);
	const bool bObserver = pPlayerEnt->m_blIsObserver;
	const bool bAlive = pPlayerEnt->IsAlive() && pEntity->v.health > 0 && pEntity->v.deadflag == DEAD_NO;

	entvars_t* pev = &pEntity->v;

	if (FStrEq(pcmd, "say"))
	{
		Host_Say(pEntity, false);
	}
	else if (FStrEq(pcmd, "say_team"))
	{
		Host_Say(pEntity, true);
	}
	else if (FStrEq(pcmd, "say_private") || FStrEq(pcmd, "sayprivate"))
	{
		SayPrivate(pEntity);
	}
	else if (FStrEq(pcmd, "showmotd") || FStrEq(pcmd, "serverhelp"))
	{
		if (g_pGameRules->IsMultiplayer())
			((CHalfLifeMultiplay*)g_pGameRules)->SendMOTDToClient(pEntity);
	}
	else if ((FStrEq(pcmd, "give") || FStrEq(pcmd, "giveperm")) && CVAR_GET_FLOAT("sv_allowgives") == 1)
	{
		g_bUnsafeCreate = true;
		if (CanClientGive(pEntity, pev, bObserver, bAlive))
		{
			if (CheckItemStrEqu((char*)CMD_ARGV(1), (char*)"item_") || CheckItemStrEqu((char*)CMD_ARGV(1), (char*)"weapon_") || CheckItemStrEqu((char*)CMD_ARGV(1), (char*)"ammo_"))
			{
				// spawn it on the player, let the player touch it, and remove what's left
				CBaseEntity* pItem = CBaseEntity::Create(CMD_ARGV(1), pev->origin, Vector(0, pev->v_angle.y, 0), NULL);
				if (pItem)
				{
					pItem->pev->spawnflags |= SF_NORESPAWN;
					DispatchTouch(pItem->edict(), pEntity);
					if (pItem->pev && 0 != OFFSET(pItem->edict()))
					{
						edict_t* pItemEdict = pItem->pev->pContainingEntity;
						if (pItemEdict && pItemEdict->pvPrivateData && 0 == pItemEdict->free)
						{
							pItem->pev->effects |= EF_NODRAW;
							pItem->pev->flags |= FL_KILLME;
						}
					}
				}
			}
			else
			{
				GetClassPtr((CBasePlayer*)pev)->GiveNamedItem(CMD_ARGV(1));
			}
		}
		g_bUnsafeCreate = false;
	}
	else if (FStrEq(pcmd, "giveitem") && CVAR_GET_FLOAT("sv_allowgives") == 1)
	{
		g_bUnsafeCreate = true;
		if (CanClientGive(pEntity, pev, bObserver, bAlive))
		{
			CBaseEntity* pItem = CBaseEntity::Create(CMD_ARGV(1), pev->origin, Vector(0, pev->v_angle.y, 0), NULL);
			if (pItem)
			{
				pItem->pev->spawnflags |= SF_NORESPAWN;
				DispatchTouch(pItem->edict(), pEntity);
				if (pItem->pev && 0 != OFFSET(pItem->edict()))
				{
					edict_t* pItemEdict = pItem->pev->pContainingEntity;
					if (pItemEdict && pItemEdict->pvPrivateData && 0 == pItemEdict->free)
					{
						pItem->pev->effects |= EF_NODRAW;
						pItem->pev->flags |= FL_KILLME;
					}
				}
			}
		}
		g_bUnsafeCreate = false;
	}
	else if (FStrEq(pcmd, "givefor") && CVAR_GET_FLOAT("sv_allowgives") == 1)
	{
		// spawn it 256 units in front of the player
		g_bUnsafeCreate = true;
		if (CanClientGive(pEntity, pev, bObserver, bAlive))
		{
			UTIL_MakeVectors(pev->v_angle);
			Vector vecOrigin = pev->origin + gpGlobals->v_forward * 256;
			CBaseEntity::Create(CMD_ARGV(1), vecOrigin, Vector(0, pev->v_angle.y, 0), NULL);
		}
		g_bUnsafeCreate = false;
	}
	else if (FStrEq(pcmd, "nuke"))
	{
		CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pEntity);
		if (!pPlayer)
			return;
		if ((pev->flags & FL_FROZEN) != 0 || bObserver || !bAlive)
			return;
		if (!(pPlayer->m_fNukeTime < gpGlobals->time))
			return;
		if (CVAR_GET_FLOAT("mp_allownukes") != 1)
			return;
		if (FireNuke(pPlayer))
			pPlayer->m_fNukeTime = gpGlobals->time + PREFS_KeyLoopUpFloat("NUKE", "RefireTime", 90);
	}
	else if (FStrEq(pcmd, "eject"))
	{
		CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pev->pContainingEntity);
		if (!pPlayer)
			return;
		if (pPlayer->m_bInDriveEnt && pPlayer->m_pCurDriveEnt)
		{
			pPlayer->m_pCurDriveEnt->DEClientEject();
		}
		else
		{
			pPlayer->m_pCurDriveEnt = NULL;
			pPlayer->m_bInDriveEnt = false;
		}
	}
	else if (FStrEq(pcmd, "spectate") || FStrEq(pcmd, "observe"))
	{
		// Rocket Crowbar's observer mode (pregame.cpp), entered by a suicide that costs no frag
		if (CVAR_GET_FLOAT("sv_ingameobservers") == 1 || IsClientServer(pEntity))
		{
			CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pev->pContainingEntity);
			if (!pPlayer || pPlayer->m_blIsObserver || pPlayer->m_bInDriveEnt)
				return;

			g_pevLastInflictor = VARS(INDEXENT(0));
			pPlayer->pev->iuser4 = 1; // VERIFY: read by the player's Killed (player.cpp) to tell an observer suicide
			pPlayer->Killed(g_pevLastInflictor, GIB_NEVER);
			pPlayer->pev->iuser4 = 0;
			pPlayer->m_blWasObserver = true;
			pPlayer->m_blIsObserver = true;
			pPlayer->m_blObserverRespawn = true;
			pPlayer->RemoveAllItems(false);
			InitObserver(pPlayer);
		}
		else
		{
			CLIENT_PRINTF(pEntity, print_console, "Server has disabled in-game spectation\n");
		}
	}
	else if (FStrEq(pcmd, "lcputent") && IsClientServer(pEntity))
	{
		// write a "create" line for the .lec file: the entity is made at map start where the player stands
		LogClientServerCommand(pEntity);
		char szPath[512];
		LCSMapFilePath(szPath, sizeof(szPath));
		FILE* pFile = fopen(szPath, "a");
		if (!pFile)
		{
			CLIENT_PRINTF(pEntity, print_console, "Cant append to file, does directory exist?\n");
			return;
		}
		float v[6] = {0, 0, 0, 0, 0, 0};
		LCSPutCoords(pev, v);
		fprintf(pFile, "create %s %f %f %f %f %f %f\n", CMD_ARGV(1), v[0], v[1], v[2], v[3], v[4], v[5]);
		fclose(pFile);
		CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("%s added at %0.2f, %0.2f, %0.2f facing %0.2f %0.2f %0.2f\n", CMD_ARGV(1), v[0], v[1], v[2], v[3], v[4], v[5]));
	}
	else if (FStrEq(pcmd, "lcrespawnent") && IsClientServer(pEntity))
	{
		LogClientServerCommand(pEntity);
		char szPath[512];
		LCSMapFilePath(szPath, sizeof(szPath));
		FILE* pFile = fopen(szPath, "a");
		if (!pFile)
		{
			CLIENT_PRINTF(pEntity, print_console, "Cant append to file, does directory exist?\n");
			return;
		}
		float v[6] = {0, 0, 0, 0, 0, 0};
		LCSPutCoords(pev, v);
		fprintf(pFile, "respawncreate %s %f %f %f %f %f %f\n", CMD_ARGV(1), v[0], v[1], v[2], v[3], v[4], v[5]);
		fclose(pFile);
		CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("Respawning %s added at %0.2f, %0.2f, %0.2f facing %0.2f %0.2f %0.2f\n", CMD_ARGV(1), v[0], v[1], v[2], v[3], v[4], v[5]));
	}
	else if (FStrEq(pcmd, "lcrement") && IsClientServer(pEntity))
	{
		LCSEntCommand(pEntity, pev, "entremove", "Removing %s at %0.2f, %0.2f, %0.2f\n");
	}
	else if (FStrEq(pcmd, "lckillent") && IsClientServer(pEntity))
	{
		LCSEntCommand(pEntity, pev, "entkill", "Using %s at %0.2f, %0.2f, %0.2f\n");
	}
	else if (FStrEq(pcmd, "lcuseent") && IsClientServer(pEntity))
	{
		LCSEntCommand(pEntity, pev, "useent", "Using %s at %0.2f, %0.2f, %0.2f\n");
	}
	else if (FStrEq(pcmd, "lcaddcommand") && IsClientServer(pEntity))
	{
		LogClientServerCommand(pEntity);
		char szPath[512];
		LCSMapFilePath(szPath, sizeof(szPath));
		FILE* pFile = fopen(szPath, "a");
		if (!pFile)
		{
			CLIENT_PRINTF(pEntity, print_console, "Cant append to file, does directory exist?\n");
			return;
		}
		if (CMD_ARGC() > 1)
			fprintf(pFile, "%s\n", CMD_ARGV(1));
		fclose(pFile);
	}
	else if (FStrEq(pcmd, "lccleanfile") && IsClientServer(pEntity))
	{
		LogClientServerCommand(pEntity);
		char szPath[512];
		LCSMapFilePath(szPath, sizeof(szPath));
		FILE* pFile = fopen(szPath, "w");
		if (pFile)
			fclose(pFile);
		else
			CLIENT_PRINTF(pEntity, print_console, "Cant write to file, does directory exist?\n");
	}
	else if (FStrEq(pcmd, "fcheats") && IsClientServer(pEntity))
	{
		LogClientServerCommand(pEntity);
		if (CMD_ARGC() == 2)
		{
			g_flWeaponCheat = atof(CMD_ARGV(1));
			CVAR_SET_FLOAT("sv_cheats", g_flWeaponCheat);
		}
	}
	else if (FStrEq(pcmd, "drop"))
	{
		// player is dropping an item.
		GetClassPtr((CBasePlayer*)pev)->DropPlayerItem((char*)CMD_ARGV(1));
	}
	else if (FStrEq(pcmd, "fov"))
	{
		if (g_flWeaponCheat != 0 && CMD_ARGC() > 1)
		{
			GetClassPtr((CBasePlayer*)pev)->m_iFOV = atoi(CMD_ARGV(1));
		}
		else
		{
			CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("\"fov\" is \"%d\"\n", (int)GetClassPtr((CBasePlayer*)pev)->m_iFOV));
		}
	}
	else if (FStrEq(pcmd, "cl_gibcount"))
	{
		// extra gibs this client wants, 4-259; kept in its "extragibs" userinfo key
		if (CMD_ARGC() > 1)
		{
			int iCount = atoi(CMD_ARGV(1));
			if ((unsigned int)(iCount - 4) > 255)
			{
				CLIENT_PRINTF(pEntity, print_console, "\"cl_extragibs\" is limited to 4-259\n");
				return;
			}
			CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pev->pContainingEntity);
			int iExtra = iCount - 4;
			if (iExtra < 0)
				pPlayer->m_iGibExtra = 0;
			else if (iExtra > 255)
				pPlayer->m_iGibExtra = 255;
			else
				pPlayer->m_iGibExtra = iExtra;

			char szValue[32];
			snprintf(szValue, sizeof(szValue), "%i", pPlayer->m_iGibExtra);
			g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), g_engfuncs.pfnGetInfoKeyBuffer(pEntity), "extragibs", szValue);
		}
		else
		{
			CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("\"cl_gibcount\" is \"%d\"\n", GetClassPtr((CBasePlayer*)pev)->m_iGibExtra + 4));
		}
	}
	else if (FStrEq(pcmd, "cl_de_alpha"))
	{
		// how see-through the driven monster is for its driver, 0-1; kept in "driveentalpha"
		if (CMD_ARGC() > 1)
		{
			float flAlpha = atof(CMD_ARGV(1));
			if (flAlpha > 1 || flAlpha < 0)
			{
				CLIENT_PRINTF(pEntity, print_console, "\"cl_de_alpha\" is limited to 0.0-1.0\n");
				return;
			}
			CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pev->pContainingEntity);
			if (flAlpha < 0)
				pPlayer->m_fDrvEntAlpha = 0;
			else if (flAlpha > 1)
				pPlayer->m_fDrvEntAlpha = 1;
			else
				pPlayer->m_fDrvEntAlpha = flAlpha;

			char szValue[32];
			snprintf(szValue, sizeof(szValue), "%f", pPlayer->m_fDrvEntAlpha);
			g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), g_engfuncs.pfnGetInfoKeyBuffer(pEntity), "driveentalpha", szValue);
		}
		else
		{
			CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("\"cl_de_alpha\" is \"%f\"\n", GetClassPtr((CBasePlayer*)pev)->m_fDrvEntAlpha));
		}
	}
	else if (FStrEq(pcmd, "use"))
	{
		if ((pev->flags & FL_FROZEN) != 0)
			return;
		GetClassPtr((CBasePlayer*)pev)->SelectItem((char*)CMD_ARGV(1));
	}
	else if (((pstr = strstr(pcmd, "weapon_")) != NULL) && (pstr == pcmd))
	{
		if ((pev->flags & FL_FROZEN) != 0)
			return;
		GetClassPtr((CBasePlayer*)pev)->SelectItem(pcmd);
	}
	else if (FStrEq(pcmd, "lastinv"))
	{
		if ((pev->flags & FL_FROZEN) != 0)
			return;
		GetClassPtr((CBasePlayer*)pev)->SelectLastItem();
	}
	else if (FStrEq(pcmd, "RCBV") || FStrEq(pcmd, "RCV"))
	{
		CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("Rocket Crowbar Version Is: %s\n", "19"));
	}
	else if (FStrEq(pcmd, "listclients"))
	{
		CLIENT_PRINTF(pEntity, print_console, "-------\n");
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
			if (!pPlayer || !pPlayer->pev || 0 == OFFSET(pPlayer->edict()) || !pPlayer->IsNetClient())
				continue;
			CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("  Client %i (uid %i) is %s\n", i, GETPLAYERUSERID(pPlayer->edict()), STRING(pPlayer->pev->netname)));
		}
		CLIENT_PRINTF(pEntity, print_console, "-------\n");
	}
	else if (FStrEq(pcmd, "votekick"))
	{
		if (CMD_ARGC() != 2)
		{
			CLIENT_PRINTF(pEntity, print_console, "ussage: votekick [client index]\n");
			CLIENT_PRINTF(pEntity, print_console, "Valid client indices are are\n-------\n");
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
				if (!pPlayer || !pPlayer->pev || 0 == OFFSET(pPlayer->edict()))
					continue;
				CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("  Index %i is %s\n", i, STRING(pPlayer->pev->netname)));
			}
			CLIENT_PRINTF(pEntity, print_console, "-------\n");
		}
		else
		{
			CLIENT_PRINTF(pEntity, print_console, VK_InitVote(CBaseEntity::Instance(pev), atoi(CMD_ARGV(1))));
		}
	}
	else if (FStrEq(pcmd, "vkyes"))
	{
		VK_RegisterVote(CBaseEntity::Instance(pev), true);
	}
	else if (FStrEq(pcmd, "vkno"))
	{
		VK_RegisterVote(CBaseEntity::Instance(pev), false);
	}
	else if (FStrEq(pcmd, "votemap"))
	{
		if (CMD_ARGC() != 2)
			CLIENT_PRINTF(pEntity, print_console, "ussage: votemap [map name]\n");
		else
			CLIENT_PRINTF(pEntity, print_console, VM_InitVote(CBaseEntity::Instance(pev), CMD_ARGV(1)));
	}
	else if (FStrEq(pcmd, "vmyes"))
	{
		VM_RegisterVote(CBaseEntity::Instance(pev), true);
	}
	else if (FStrEq(pcmd, "vmno"))
	{
		VM_RegisterVote(CBaseEntity::Instance(pev), false);
	}
	else if (FStrEq(pcmd, "showmaps"))
	{
		// the maps in allowedmaps.txt that exist, about 48 characters a line
		int iLength;
		char* pFileList = (char*)LOAD_FILE_FOR_ME("allowedmaps.txt", &iLength);
		char* pFile = pFileList;
		if (!pFileList || iLength <= 1)
		{
			// VERIFY: a file of one byte or less is never freed, as in the original
			CLIENT_PRINTF(pEntity, print_console, "No map limits active, all defaults are valid\n");
			return;
		}

		char szShow[64 + 256];
		char* szBuf = szShow + 64;
		strcpy(szBuf, "Available maps are:\n ");
		int iBufLen = strlen(szBuf);
		int iLineLen = 0;
		while (*pFileList != 0)
		{
			while (*pFileList != 0 && 0 != isspace((unsigned char)*pFileList))
				pFileList++;
			if (*pFileList == 0)
				break;

			// the original's map buffer is 64 bytes right before szBuf, so a 64-character name's
			// terminator lands in szBuf[0]; szShow keeps that layout
			char* szMap = szShow;
			if (sscanf(pFileList, " %64s", szMap) != 1)
				break;
			if ((signed char)szMap[0] <= 12) // VERIFY: the original's test, a first byte of 12 or less ends the list
				break;
			pFileList += strlen(szMap);

			if (!IS_MAP_VALID(szMap))
				continue;

			if (iBufLen + (int)strlen(szMap) > 254)
			{
				CLIENT_PRINTF(pEntity, print_console, szBuf);
				szBuf[0] = 0;
			}
			iLineLen += strlen(szMap);
			if (iLineLen > 48)
			{
				strncat(szBuf, "\n", 255 - strlen(szBuf));
				iLineLen = 0;
			}
			else
			{
				strncat(szBuf, " ", 255 - strlen(szBuf));
			}
			szBuf[255] = 0;
			strncat(szBuf, szMap, 255 - strlen(szBuf));
			szBuf[255] = 0;
			iBufLen = strlen(szBuf);
		}
		strncat(szBuf, "\n", 255 - strlen(szBuf));
		szBuf[255] = 0;
		CLIENT_PRINTF(pEntity, print_console, szBuf);
		FREE_FILE(pFile);
	}
	else if (FStrEq(pcmd, "sndcmd") && IsClientServer(pEntity))
	{
		// "sndcmd <index> <cmd> [args]": run a console command on one client (or on all, for
		// an index that isn't a client); the listen server's player only takes it from itself
		LogClientServerCommand(pEntity);
		if (CMD_ARGC() <= 2)
		{
			CLIENT_PRINTF(pEntity, print_console, "Ussage: sndcmd # cmd\n");
			CLIENT_PRINTF(pEntity, print_console, "Valid client indices are are\n-------\n");
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
				if (!pPlayer || !pPlayer->pev || 0 == OFFSET(pPlayer->edict()))
					continue;
				CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("  Index %i is %s\n", i, STRING(pPlayer->pev->netname)));
			}
			CLIENT_PRINTF(pEntity, print_console, "-------\n");
			return;
		}

		// the command, then each argument in quotes
		char szCmd[512];
		szCmd[0] = 0;
		for (int i = 2; i < CMD_ARGC(); i++)
		{
			if (i != 2)
				strncat(szCmd, "\"", sizeof(szCmd) - 1 - strlen(szCmd));
			strncat(szCmd, CMD_ARGV(i), sizeof(szCmd) - 1 - strlen(szCmd));
			strncat(szCmd, (i != 2) ? "\" " : " ", sizeof(szCmd) - 1 - strlen(szCmd));
		}
		strncat(szCmd, "\n", sizeof(szCmd) - 1 - strlen(szCmd));
		szCmd[511] = 0;

		int iIndex = atoi(CMD_ARGV(1));
		if (iIndex <= 0 || iIndex > gpGlobals->maxClients)
		{
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
				if (!pPlayer || !pPlayer->IsNetClient())
					continue;
				if (!IsClientListenServer(pPlayer->edict()) || IsClientListenServer(pEntity))
					CLIENT_COMMAND(pPlayer->edict(), "%s", szCmd);
			}
		}
		else
		{
			CBaseEntity* pPlayer = UTIL_PlayerByIndex(atoi(CMD_ARGV(1)));
			if (pPlayer && pPlayer->IsNetClient() && !IsClientListenServer(pPlayer->edict()))
				CLIENT_COMMAND(pPlayer->edict(), "%s", szCmd);
		}
	}
	else if (FStrEq(pcmd, "wog"))
	{
		// a "wog" with the wrong password is reported as an unknown command
		if (!WOGCommand(pEntity))
			ClientPrint(&pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs("Unknown command: %s\n", pcmd));
	}
	else if (FStrEq(pcmd, "remall") && IsClientServer(pEntity))
	{
		// flag every non-player of a classname for removal
		LogClientServerCommand(pEntity);
		if (CMD_ARGC() > 1)
		{
			CBaseEntity* pRemove = NULL;
			while ((pRemove = UTIL_FindEntityByClassname(pRemove, CMD_ARGV(1))) != NULL)
			{
				if (!pRemove->IsPlayer())
					pRemove->pev->flags |= FL_KILLME;
			}
		}
	}
	else if (FStrEq(pcmd, "useent") && (IsClientServer(pEntity) || IsClientAllowedToUse(pEntity)))
	{
		// "useent [off|on|set|toggle] [value]": use the entity looked at
		LogClientServerCommand(pEntity);
		CBaseEntity* pTarget = EManipFindEntForward(pev);
		if (!pTarget)
			return;

		CBaseEntity* pPlayer = CBaseEntity::Instance(pEntity);
		if (CMD_ARGC() > 1)
		{
			USE_TYPE useType = (USE_TYPE)EManipGetEnumArrayValue((char*)CMD_ARGV(1), g_esUseTypes, 4);
			if (CMD_ARGC() > 2)
				pTarget->Use(pPlayer, pPlayer, useType, atof(CMD_ARGV(2)));
			else
				pTarget->Use(CBaseEntity::Instance(ENT(0)), pPlayer, useType, 0); // VERIFY: the world is the activator here in the original
		}
		else
		{
			pTarget->Use(pPlayer, pPlayer, USE_OFF, 0);
		}
	}
	else if (FStrEq(pcmd, "rement") && IsClientServer(pEntity))
	{
		LogClientServerCommand(pEntity);
		CBaseEntity* pTarget = EManipFindEntForward(pev);
		if (pTarget && !pTarget->IsPlayer())
			pTarget->pev->flags |= FL_KILLME;
	}
	else if (FStrEq(pcmd, "addspawn") && IsClientServer(pEntity))
	{
		LogClientServerCommand(pEntity);
		CLIENT_PRINTF(pEntity, print_console, "Spawn added\n");
		CBaseEntity::Create("info_player_deathmatch", pev->origin, Vector(0, pev->v_angle.y, 0), pev->pContainingEntity);
	}
	else if ((FStrEq(pcmd, "setpax") || FStrEq(pcmd, "setpay") || FStrEq(pcmd, "setpaz")) && IsClientServer(pEntity))
	{
		// set the punch angle of the entity looked at
		LogClientServerCommand(pEntity);
		CBaseEntity* pTarget = EManipFindEntForward(pev);
		if (!pTarget || CMD_ARGC() <= 1)
			return;
		float flValue = atof(CMD_ARGV(1));
		if (FStrEq(pcmd, "setpax"))
			pTarget->pev->punchangle.x = flValue;
		else if (FStrEq(pcmd, "setpay"))
			pTarget->pev->punchangle.y = flValue;
		else
			pTarget->pev->punchangle.z = flValue;
	}
	else if (FStrEq(pcmd, "saveprefs") && IsClientServer(pEntity))
	{
		LogClientServerCommand(pEntity);
		char szGameDir[512];
		char szPath[512];
		GET_GAME_DIR(szGameDir);
		snprintf(szPath, sizeof(szPath), "%s/%s", szGameDir, CVAR_GET_STRING("sv_prefsfile"));
		PREFS_WriteFile(szPath);
	}
	else if (FStrEq(pcmd, "loadprefs") && IsClientServer(pEntity))
	{
		LogClientServerCommand(pEntity);
		char szGameDir[512];
		char szPath[512];
		GET_GAME_DIR(szGameDir);
		snprintf(szPath, sizeof(szPath), "%s/%s", szGameDir, CVAR_GET_STRING("sv_prefsfile"));
		PREFS_LoadFile(szPath);
	}
	else if (FStrEq(pcmd, "setpref") && IsClientServer(pEntity))
	{
		LogClientServerCommand(pEntity);
		if (CMD_ARGC() != 4)
		{
			CLIENT_PRINTF(pEntity, print_console, "Ussage: setpref heading keyname keyvalue\n");
			return;
		}
		int iHeading = PREFS_BinSearchHead(CMD_ARGV(1));
		if (iHeading < 0)
		{
			CLIENT_PRINTF(pEntity, print_console, "Heading not found\n");
			return;
		}
		int iKey = PREFS_BinSearchKey(CMD_ARGV(2), iHeading);
		if (iKey < 0)
		{
			CLIENT_PRINTF(pEntity, print_console, "Key not found\n");
			return;
		}
		PREFS_SetKey(iHeading, iKey, CMD_ARGV(3));
		UTIL_ClientPrintAll(HUD_PRINTCONSOLE, UTIL_VarArgs("Prefs key changed: [%s] %s=%s\n", CMD_ARGV(1), CMD_ARGV(2), CMD_ARGV(3)));
	}
	else if (FStrEq(pcmd, "dumpheads"))
	{
		CLIENT_PRINTF(pEntity, print_console, "-------\n");
		for (int i = 0; i < g_iCountHeadings; i++)
			CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("  %s\n", g_pHeadings[i].szName));
		CLIENT_PRINTF(pEntity, print_console, "-------\n");
	}
	else if (FStrEq(pcmd, "dumpprefs"))
	{
		if (CMD_ARGC() != 2)
		{
			CLIENT_PRINTF(pEntity, print_console, "Ussage: dumpprefs [heading name]\n");
			return;
		}
		int iHeading = PREFS_BinSearchHead(CMD_ARGV(1));
		if (iHeading < 0)
		{
			CLIENT_PRINTF(pEntity, print_console, "Heading not found\n");
			return;
		}
		CLIENT_PRINTF(pEntity, print_console, "-------\n");
		for (int i = 0; i < g_pHeadings[iHeading].iCount; i++)
		{
			sPrefKey& key = g_pHeadings[iHeading].pKeys[i];
			if (key.szKey && key.szValue)
				CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("  %s=%s\n", key.szKey, key.szValue));
		}
		CLIENT_PRINTF(pEntity, print_console, "-------\n");
	}
	else if (FStrEq(pcmd, "prefsfind") || FStrEq(pcmd, "findpref") || FStrEq(pcmd, "searchprefs") || FStrEq(pcmd, "prefslookup"))
	{
		if (CMD_ARGC() != 3)
		{
			CLIENT_PRINTF(pEntity, print_console, "Ussage: prefsfind [heading name] [key name]\n");
			return;
		}
		const char* szValue = PREFS_LookUpKey(CMD_ARGV(1), CMD_ARGV(2));
		if (szValue)
			CLIENT_PRINTF(pEntity, print_console, UTIL_VarArgs("[%s] %s = %s\n", CMD_ARGV(1), CMD_ARGV(2), szValue));
		else
			CLIENT_PRINTF(pEntity, print_console, "Heading or Key not found\n");
	}
	else if (FStrEq(pcmd, "fullupdate"))
	{
		// halflife-updated: a newer client asks for this to re-send its HUD
		GetClassPtr((CBasePlayer*)pev)->ForceClientDllUpdate();
	}
	else if (FStrEq(pcmd, "closemenus"))
	{
		// halflife-updated: a newer client sends this when it closes a menu; just ignore it
	}
	else if (g_pGameRules->ClientCommand(GetClassPtr((CBasePlayer*)pev), pcmd))
	{
		// MenuSelect returns true only if the command is properly handled,  so don't print a warning
	}
	else
	{
		// tell the user they entered an unknown command
		char command[128];

		// check the length of the command (prevents crash)
		// max total length is 192 ...and we're adding a string below ("Unknown command: %s\n")
		strncpy(command, pcmd, 127);
		command[127] = '\0';
		// First parse the name and remove any %'s
		for (char* pApersand = command; pApersand != NULL && *pApersand != 0; pApersand++)
		{
			// Replace it with a space
			if (*pApersand == '%')
				*pApersand = ' ';
		}

		// tell the user they entered an unknown command
		ClientPrint(&pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs("Unknown command: %s\n", command));
	}
}


/*
========================
ClientUserInfoChanged

called after the player changes
userinfo - gives dll a chance to modify it before
it gets sent into the rest of the engine.
========================
*/
// orig 0x102cc0. Rocket Crowbar also takes the client's "extragibs" and "driveentalpha" keys
// (cl_gibcount, cl_de_alpha) and logs a name change in its own format.
void ClientUserInfoChanged(edict_t* pEntity, char* infobuffer)
{
	// Is the client spawned yet?
	if (!pEntity->pvPrivateData)
		return;

	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pEntity);

	if (atoi(g_engfuncs.pfnInfoKeyValue(infobuffer, "extragibs")) != pPlayer->m_iGibExtra)
	{
		pPlayer->m_iGibExtra = atoi(g_engfuncs.pfnInfoKeyValue(infobuffer, "extragibs"));
		if (pPlayer->m_iGibExtra > 255)
			pPlayer->m_iGibExtra = 255;
		if (pPlayer->m_iGibExtra < 0)
			pPlayer->m_iGibExtra = 0;
	}

	if (pPlayer->m_fDrvEntAlpha != (float)atof(g_engfuncs.pfnInfoKeyValue(infobuffer, "driveentalpha")))
	{
		pPlayer->m_fDrvEntAlpha = atof(g_engfuncs.pfnInfoKeyValue(infobuffer, "driveentalpha"));
		if (pPlayer->m_fDrvEntAlpha > 1)
			pPlayer->m_fDrvEntAlpha = 1;
		if (pPlayer->m_fDrvEntAlpha < 0)
			pPlayer->m_fDrvEntAlpha = 0;
	}

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	if (!FStringNull(pEntity->v.netname) && STRING(pEntity->v.netname)[0] != 0 && !FStrEq(STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue(infobuffer, "name")))
	{
		// halflife-updated's fix, kept: a % in the new name becomes a space, so the name can't act
		// as a format string where clients print it; the original, as SDK 2.1, passes it through
		char sName[256];
		char* pName = g_engfuncs.pfnInfoKeyValue(infobuffer, "name");
		strncpy(sName, pName, sizeof(sName) - 1);
		sName[sizeof(sName) - 1] = '\0';

		// First parse the name and remove any %'s
		for (char* pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++)
		{
			// Replace it with a space
			if (*pApersand == '%')
				*pApersand = ' ';
		}

		// Set the name
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pEntity), infobuffer, "name", sName);

		char text[256];
		snprintf(text, sizeof(text), "* %s changed name to %s\n", STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue(infobuffer, "name"));
		MESSAGE_BEGIN(MSG_ALL, gmsgSayText, NULL);
		WRITE_BYTE(ENTINDEX(pEntity));
		WRITE_STRING(text);
		MESSAGE_END();

		UTIL_LogPrintf("\"%s<%i>\" changed name to \"%s<%i>\"\n",
			STRING(pEntity->v.netname),
			GETPLAYERUSERID(pEntity),
			g_engfuncs.pfnInfoKeyValue(infobuffer, "name"),
			GETPLAYERUSERID(pEntity));
	}

	g_pGameRules->ClientUserInfoChanged(GetClassPtr((CBasePlayer*)&pEntity->v), infobuffer);
}

static int g_serveractive = 0;

// orig 0x103380
void ServerDeactivate()
{
	// It's possible that the engine will call this function more times than is necessary
	//  Therefore, only run it one time for each call to ServerActivate
	if (g_serveractive != 1)
	{
		return;
	}

	g_serveractive = 0;

	// Peform any shutdown operations here...
	//
}

// orig 0x1033b0. The original skips edicts below clientMax (so it skips the world and activates
// the last client slot); halflife-updated skips 1..clientMax. Both leave the world and the players'
// Activate (empty in both) uncalled in practice, so ours keeps halflife-updated's test.
void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax)
{
	int i;
	CBaseEntity* pClass;

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	g_serveractive = 1;

	// Clients have not been initialized yet
	for (i = 0; i < edictCount; i++)
	{
		if (0 != pEdictList[i].free)
			continue;

		// Clients aren't necessarily initialized until ClientPutInServer()
		if ((i > 0 && i <= clientMax) || !pEdictList[i].pvPrivateData)
			continue;

		pClass = CBaseEntity::Instance(&pEdictList[i]);
		// Activate this entity if it's got a class & isn't dormant
		if (pClass && (pClass->pev->flags & FL_DORMANT) == 0)
		{
			pClass->Activate();
		}
		else
		{
			ALERT(at_console, "Can't instance %s\n", STRING(pEdictList[i].v.classname));
		}
	}

	// Link user messages here to make sure first client can get them...
	LinkUserMessages();
}


/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
// orig 0x1034a0
void PlayerPreThink(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PreThink();
}

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
// orig 0x1034e0
void PlayerPostThink(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PostThink();
}



// orig 0x103520
void ParmsNewLevel()
{
}


// orig 0x103530 (the original also does "if (g_fGameOver) g_fGameOver = true;", which changes nothing)
void ParmsChangeLevel()
{
	// retrieve the pointer to the save data
	SAVERESTOREDATA* pSaveData = (SAVERESTOREDATA*)gpGlobals->pSaveData;

	if (pSaveData)
		pSaveData->connectionCount = BuildChangeList(pSaveData->levelList, MAX_LEVEL_CONNECTIONS);
}

static std::vector<std::string> g_MapsToLoad;

static void LoadNextMap()
{
	const std::string mapName = std::move(g_MapsToLoad.back());
	g_MapsToLoad.pop_back();

	pmove->Con_Printf("Loading map \"%s\" automatically (%d left)\n",
		mapName.c_str(), static_cast<int>(g_MapsToLoad.size() + 1));

	if (g_MapsToLoad.empty())
	{
		pmove->Con_Printf("Loading last map\n");
		g_MapsToLoad.shrink_to_fit();
	}

	SERVER_COMMAND(UTIL_VarArgs("map \"%s\"\n", mapName.c_str()));
}

static void LoadAllMaps()
{
	if (!g_MapsToLoad.empty())
	{
		pmove->Con_Printf("Already loading all maps (%d remaining)\nUse sv_stop_loading_all_maps to stop\n",
			static_cast<int>(g_MapsToLoad.size()));
		return;
	}

	FileFindHandle_t handle = FILESYSTEM_INVALID_FIND_HANDLE;

	const char* fileName = g_pFileSystem->FindFirst("maps/*.bsp", &handle);

	if (fileName != nullptr)
	{
		do
		{
			std::string mapName = fileName;
			mapName.resize(mapName.size() - 4);

			if (std::find_if(g_MapsToLoad.begin(), g_MapsToLoad.end(), [=](const auto& candidate)
					{ return 0 == stricmp(candidate.c_str(), mapName.c_str()); }) == g_MapsToLoad.end())
			{
				g_MapsToLoad.push_back(std::move(mapName));
			}
		} while ((fileName = g_pFileSystem->FindNext(handle)) != nullptr);

		g_pFileSystem->FindClose(handle);

		// Sort in reverse order so the first map in alphabetic order is loaded first.
		std::sort(g_MapsToLoad.begin(), g_MapsToLoad.end(), [](const auto& lhs, const auto& rhs)
			{ return rhs < lhs; });
	}

	if (!g_MapsToLoad.empty())
	{
		if (CMD_ARGC() == 2)
		{
			const char* firstMapToLoad = CMD_ARGV(1);

			// Clear out all maps that would have been loaded before this one.
			if (auto it = std::find(g_MapsToLoad.begin(), g_MapsToLoad.end(), firstMapToLoad);
				it != g_MapsToLoad.end())
			{
				const std::size_t numberOfMapsToSkip = g_MapsToLoad.size() - (it - g_MapsToLoad.begin());

				g_MapsToLoad.erase(it + 1, g_MapsToLoad.end());

				pmove->Con_Printf("Skipping %d maps to start with \"%s\"\n",
					static_cast<int>(numberOfMapsToSkip), g_MapsToLoad.back().c_str());
			}
			else
			{
				pmove->Con_Printf("Unknown map \"%s\", starting from beginning\n", firstMapToLoad);
			}
		}

		pmove->Con_Printf("Loading %d maps one at a time to generate files\n", static_cast<int>(g_MapsToLoad.size()));

		// Load the first map right now.
		LoadNextMap();
	}
	else
	{
		pmove->Con_Printf("No maps to load\n");
	}
}

void InitMapLoadingUtils()
{
	g_engfuncs.pfnAddServerCommand("sv_load_all_maps", &LoadAllMaps);
	// Escape hatch in case the command is executed in error.
	g_engfuncs.pfnAddServerCommand("sv_stop_loading_all_maps", []()
		{ g_MapsToLoad.clear(); });
}

static bool g_LastAllowBunnyHoppingState = false;

//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
// orig 0x103590. Rocket Crowbar runs the kick and map votes here. The original also reads
// "skill" into g_iSkillLevel every frame, and calls each player's IsNetClient() and ignores it.
void StartFrame()
{
	if (g_pGameRules)
		g_pGameRules->Think();

	g_flLastTime = gpGlobals->time;

	if (g_fGameOver)
		return;

	VK_VoteFrame();
	VM_VoteFrame();

	gpGlobals->teamplay = CVAR_GET_FLOAT("teamplay");
	g_iSkillLevel = CVAR_GET_FLOAT("skill");
	g_ulFrameCount++;

	const bool allowBunnyHopping = sv_allowbunnyhopping.value != 0;

	if (allowBunnyHopping != g_LastAllowBunnyHoppingState)
	{
		g_LastAllowBunnyHoppingState = allowBunnyHopping;

		for (int i = 1; i <= gpGlobals->maxClients; ++i)
		{
			auto player = UTIL_PlayerByIndex(i);

			if (!player)
			{
				continue;
			}

			g_engfuncs.pfnSetPhysicsKeyValue(player->edict(), "bj", UTIL_dtos1(allowBunnyHopping ? 1 : 0));
		}
	}

	// If we're loading all maps then change maps after 3 seconds (time starts at 1)
	// to give the game time to generate files.
	if (!g_MapsToLoad.empty() && gpGlobals->time > 4)
	{
		LoadNextMap();
	}
}


// orig 0x103690. Rocket Crowbar adds the player shockwave sprite and the garg stomp sound, and
// precaches the scientist (halflife-updated: the grunt, when a map needs one).
void ClientPrecache()
{
	g_iPlayerShockwave = PRECACHE_MODEL("sprites/shockwave.spr");
	PRECACHE_SOUND("garg/gar_stomp1.wav");

	// setup precaches always needed
	PRECACHE_SOUND("player/sprayer.wav"); // spray paint sound for PreAlpha

	// PRECACHE_SOUND("player/pl_jumpland2.wav");		// UNDONE: play 2x step sound

	PRECACHE_SOUND("player/pl_fallpain2.wav");
	PRECACHE_SOUND("player/pl_fallpain3.wav");

	PRECACHE_SOUND("player/pl_step1.wav"); // walk on concrete
	PRECACHE_SOUND("player/pl_step2.wav");
	PRECACHE_SOUND("player/pl_step3.wav");
	PRECACHE_SOUND("player/pl_step4.wav");

	PRECACHE_SOUND("common/npc_step1.wav"); // NPC walk on concrete
	PRECACHE_SOUND("common/npc_step2.wav");
	PRECACHE_SOUND("common/npc_step3.wav");
	PRECACHE_SOUND("common/npc_step4.wav");

	PRECACHE_SOUND("player/pl_metal1.wav"); // walk on metal
	PRECACHE_SOUND("player/pl_metal2.wav");
	PRECACHE_SOUND("player/pl_metal3.wav");
	PRECACHE_SOUND("player/pl_metal4.wav");

	PRECACHE_SOUND("player/pl_dirt1.wav"); // walk on dirt
	PRECACHE_SOUND("player/pl_dirt2.wav");
	PRECACHE_SOUND("player/pl_dirt3.wav");
	PRECACHE_SOUND("player/pl_dirt4.wav");

	PRECACHE_SOUND("player/pl_duct1.wav"); // walk in duct
	PRECACHE_SOUND("player/pl_duct2.wav");
	PRECACHE_SOUND("player/pl_duct3.wav");
	PRECACHE_SOUND("player/pl_duct4.wav");

	PRECACHE_SOUND("player/pl_grate1.wav"); // walk on grate
	PRECACHE_SOUND("player/pl_grate2.wav");
	PRECACHE_SOUND("player/pl_grate3.wav");
	PRECACHE_SOUND("player/pl_grate4.wav");

	PRECACHE_SOUND("player/pl_slosh1.wav"); // walk in shallow water
	PRECACHE_SOUND("player/pl_slosh2.wav");
	PRECACHE_SOUND("player/pl_slosh3.wav");
	PRECACHE_SOUND("player/pl_slosh4.wav");

	PRECACHE_SOUND("player/pl_tile1.wav"); // walk on tile
	PRECACHE_SOUND("player/pl_tile2.wav");
	PRECACHE_SOUND("player/pl_tile3.wav");
	PRECACHE_SOUND("player/pl_tile4.wav");
	PRECACHE_SOUND("player/pl_tile5.wav");

	PRECACHE_SOUND("player/pl_swim1.wav"); // breathe bubbles
	PRECACHE_SOUND("player/pl_swim2.wav");
	PRECACHE_SOUND("player/pl_swim3.wav");
	PRECACHE_SOUND("player/pl_swim4.wav");

	PRECACHE_SOUND("player/pl_ladder1.wav"); // climb ladder rung
	PRECACHE_SOUND("player/pl_ladder2.wav");
	PRECACHE_SOUND("player/pl_ladder3.wav");
	PRECACHE_SOUND("player/pl_ladder4.wav");

	PRECACHE_SOUND("player/pl_wade1.wav"); // wade in water
	PRECACHE_SOUND("player/pl_wade2.wav");
	PRECACHE_SOUND("player/pl_wade3.wav");
	PRECACHE_SOUND("player/pl_wade4.wav");

	PRECACHE_SOUND("debris/wood1.wav"); // hit wood texture
	PRECACHE_SOUND("debris/wood2.wav");
	PRECACHE_SOUND("debris/wood3.wav");

	PRECACHE_SOUND("plats/train_use1.wav"); // use a train

	PRECACHE_SOUND("buttons/spark5.wav"); // hit computer texture
	PRECACHE_SOUND("buttons/spark6.wav");
	PRECACHE_SOUND("debris/glass1.wav");
	PRECACHE_SOUND("debris/glass2.wav");
	PRECACHE_SOUND("debris/glass3.wav");

	PRECACHE_SOUND("items/flashlight1.wav"); // twice in the original (on and off)
	PRECACHE_SOUND("items/flashlight1.wav");

	// player gib sounds
	PRECACHE_SOUND("common/bodysplat.wav");

	// player pain sounds
	PRECACHE_SOUND("player/pl_pain2.wav");
	PRECACHE_SOUND("player/pl_pain4.wav");
	PRECACHE_SOUND("player/pl_pain5.wav");
	PRECACHE_SOUND("player/pl_pain6.wav");
	PRECACHE_SOUND("player/pl_pain7.wav");

	PRECACHE_MODEL("models/player.mdl");

	// hud sounds

	PRECACHE_SOUND("common/wpn_hudoff.wav");
	PRECACHE_SOUND("common/wpn_hudon.wav");
	PRECACHE_SOUND("common/wpn_moveselect.wav");
	PRECACHE_SOUND("common/wpn_select.wav");
	PRECACHE_SOUND("common/wpn_denyselect.wav");


	// geiger sounds

	PRECACHE_SOUND("player/geiger6.wav");
	PRECACHE_SOUND("player/geiger5.wav");
	PRECACHE_SOUND("player/geiger4.wav");
	PRECACHE_SOUND("player/geiger3.wav");
	PRECACHE_SOUND("player/geiger2.wav");
	PRECACHE_SOUND("player/geiger1.wav");

	UTIL_PrecacheOther("monster_scientist");
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
// orig 0x103c80
const char* GetGameDescription()
{
	if (g_pGameRules) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Rocket Crowbar";
}

/*
================
Sys_Error

Engine is going to shut down, allows setting a breakpoint in game .dll to catch that occasion
================
*/
// orig 0x103c70
void Sys_Error(const char* error_string)
{
	// Default case, do nothing.  MOD AUTHORS:  Add code ( e.g., _asm { int 3 }; here to cause a breakpoint for debugging your game .dlls
}

/*
================
PlayerCustomization

A new player customization has been registered on the server
UNDONE:  This only sets the # of frames of the spray can logo
animation right now.
================
*/
// orig 0x103cc0
void PlayerCustomization(edict_t* pEntity, customization_t* pCust)
{
	entvars_t* pev = &pEntity->v;
	CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(pEntity);

	if (!pPlayer)
	{
		ALERT(at_console, "PlayerCustomization:  Couldn't get player!\n");
		return;
	}

	if (!pCust)
	{
		ALERT(at_console, "PlayerCustomization:  NULL customization!\n");
		return;
	}

	switch (pCust->resource.type)
	{
	case t_decal:
		pPlayer->SetCustomDecalFrames(pCust->nUserData2); // Second int is max # of frames.
		break;
	case t_sound:
	case t_skin:
	case t_model:
		// Ignore for now.
		break;
	default:
		ALERT(at_console, "PlayerCustomization:  Unknown customization type!\n");
		break;
	}
}

/*
================
SpectatorConnect

A spectator has joined the game
================
*/
// orig 0x103d50
void SpectatorConnect(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorConnect();
}

/*
================
SpectatorConnect

A spectator has left the game
================
*/
// orig 0x103d90
void SpectatorDisconnect(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorDisconnect();
}

/*
================
SpectatorConnect

A spectator has sent a usercmd
================
*/
// orig 0x103dd0
void SpectatorThink(edict_t* pEntity)
{
	entvars_t* pev = &pEntity->v;
	CBaseSpectator* pPlayer = (CBaseSpectator*)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorThink();
}

////////////////////////////////////////////////////////
// PAS and PVS routines for client messaging
//

/*
================
SetupVisibility

A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
entity's origin is used.  Either is offset by the view_ofs to get the eye position.

From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
 override the actual PAS or PVS values, or use a different origin.

NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
================
*/
// orig 0x103e10. Rocket Crowbar's takes NULL for pvs and pas (the drive monsters' DEClientLeave
// only want the engine's PVS/PAS set up for the leaving driver), and has no HLTV proxy test; ours
// keeps halflife-updated's proxy test (HLTV, which the original predates).
void SetupVisibility(edict_t* pViewEntity, edict_t* pClient, unsigned char** pvs, unsigned char** pas)
{
	Vector org;
	edict_t* pView = pClient;

	// Find the client's PVS
	if (pViewEntity)
	{
		pView = pViewEntity;
	}

	if ((pClient->v.flags & FL_PROXY) != 0)
	{
		if (pvs)
			*pvs = NULL; // the spectator proxy sees
		if (pas)
			*pas = NULL; // and hears everything
		return;
	}

	org = pView->v.origin + pView->v.view_ofs;
	if ((pView->v.flags & FL_DUCKING) != 0)
	{
		org = org + (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);
	}

	unsigned char* pSet = ENGINE_SET_PVS((float*)&org);
	if (pvs)
		*pvs = pSet;
	pSet = ENGINE_SET_PAS((float*)&org);
	if (pas)
		*pas = pSet;
}

// Rocket Crowbar 1.9. orig 0x103f50: how fast an entity is moving, for a monster that hunts by
// speed: 0 for anything that can't be aimed at or isn't drawn normally, plus fuser4 for a
// non-client. Nothing calls it.
float GetEntHoundeyeVel(edict_t* pEdict)
{
	entvars_t* pev = &pEdict->v;
	if ((pev->flags & (FL_DORMANT | FL_CUSTOMENTITY | FL_SPECTATOR)) != 0 || pev->takedamage != DAMAGE_AIM)
		return 0;
	if (pev->movetype != MOVETYPE_WALK && pev->movetype != MOVETYPE_FLY && pev->movetype != MOVETYPE_STEP)
		return 0;
	if (pev->solid == SOLID_NOT || pev->movetype == MOVETYPE_NONE || (pev->effects & EF_NODRAW) != 0 || pev->rendermode != kRenderNormal || pev->renderfx != kRenderFxNone)
		return 0;

	if ((pev->flags & FL_CLIENT) == 0 && pev->fuser4 > 0)
		return pev->velocity.Length() + pev->fuser4;
	return pev->velocity.Length();
}

#include "entity_state.h"

/*
AddToFullPack

Return 1 if the entity state has been filled in for the ent and the entity will be propagated to the client, 0 otherwise

state is the server maintained copy of the state info that is transmitted to the client
a MOD could alter values copied into state to send the "host" a different look for a particular entity update, etc.
e and ent are the entity that is being added to the update, if 1 is returned
host is the player's edict of the player whom we are sending the update to
player is 1 if the ent/e is a player and 0 otherwise
pSet is either the PAS or PVS that we previous set up.  We can use it to ask the engine to filter the entity against the PAS or PVS.
we could also use the pas/ pvs that we set in SetupVisibility, if we wanted to.  Caching the value is valid in that case, but still only for the current frame
*/
// orig 0x104050. Rocket Crowbar's version reads draw instructions RC code leaves in the pev's
// user fields:
//   ent->iuser3 bit 1: send it even if NODRAW, modelless or a spectator (and bit 2 skips the
//   model and PVS tests);
//   host->iuser3 bit 4: draw host->euser2 with rendermode/renderamt/renderfx = host->fuser1-3
//   and rendercolor = host->vuser1 (the observer's and the drivers' view of their target);
//   host->iuser3 bit 8: hide host->euser2 and euser3 from the host, and glow-shell everything
//   aimable by host->fuser1 brightness;
//   ent->iuser3 bit 0x20: renderamt from fuser2 (min) to fuser1 (max) by how squarely the
//   aiment faces the host, between dots fuser4 and fuser3.
// halflife-updated's FL_FLY slerp, m_EFlags and EFLAG_FLESH_SOUND are not in the original; kept
// as bug fixes (Steven, 2026-11-07).
int AddToFullPack(struct entity_state_s* state, int e, edict_t* ent, edict_t* host, int hostflags, int player, unsigned char* pSet)
{
	// Entities with an index greater than this will corrupt the client's heap because 
	// the index is sent with only 11 bits of precision (2^11 == 2048).
	// So we don't send them, just like having too many entities would result
	// in the entity not being sent.
	if (e >= MAX_EDICTS)
	{
		return 0;
	}

	int i;
	bool bSkipVisCheck = false;

	if ((ent->v.iuser3 & 1) == 0)
	{
		// don't send if flagged for NODRAW and it's not the host getting the message
		if ((ent->v.effects & EF_NODRAW) != 0 && (ent != host))
			return 0;

		// Ignore ents without valid / visible models
		if ((ent->v.iuser3 & 2) == 0 && (0 == ent->v.modelindex || !STRING(ent->v.model)))
			return 0;

		// Don't send spectators to other players
		if ((ent->v.flags & FL_SPECTATOR) != 0)
		{
			if (ent != host)
				return 0;
			bSkipVisCheck = true;
		}
	}

	// Ignore if not the host and not touching a PVS/PAS leaf
	// If pSet is NULL, then the test will always succeed and the entity will be added to the update
	if (!bSkipVisCheck && ent != host && (ent->v.iuser3 & 2) == 0)
	{
		if (!ENGINE_CHECK_VISIBILITY((const struct edict_s*)ent, pSet))
		{
			return 0;
		}
	}


	// Don't send entity to local client if the client says it's predicting the entity itself.
	if ((ent->v.flags & FL_SKIPLOCALHOST) != 0)
	{
		if ((hostflags & 1) != 0 && (ent->v.owner == host))
			return 0;
	}

	if (0 != host->v.groupinfo)
	{
		UTIL_SetGroupTrace(host->v.groupinfo, GROUP_OP_AND);

		// Should always be set, of course
		if (0 != ent->v.groupinfo)
		{
			if (g_groupop == GROUP_OP_AND)
			{
				if ((ent->v.groupinfo & host->v.groupinfo) == 0)
					return 0;
			}
			else if (g_groupop == GROUP_OP_NAND)
			{
				if ((ent->v.groupinfo & host->v.groupinfo) != 0)
					return 0;
			}
		}

		UTIL_UnsetGroupTrace();
	}

	memset(state, 0, sizeof(*state));

	// Assign index so we can track this entity from frame to frame and
	//  delta from it.
	state->number = e;
	state->entityType = ENTITY_NORMAL;

	// Flag custom entities.
	if ((ent->v.flags & FL_CUSTOMENTITY) != 0)
	{
		state->entityType = ENTITY_BEAM;
	}

	//
	// Copy state data
	//

	// Round animtime to nearest millisecond
	state->animtime = (int)(1000.0 * ent->v.animtime) / 1000.0;

	memcpy(state->origin, ent->v.origin, 3 * sizeof(float));
	memcpy(state->angles, ent->v.angles, 3 * sizeof(float));
	memcpy(state->mins, ent->v.mins, 3 * sizeof(float));
	memcpy(state->maxs, ent->v.maxs, 3 * sizeof(float));

	memcpy(state->startpos, ent->v.startpos, 3 * sizeof(float));
	memcpy(state->endpos, ent->v.endpos, 3 * sizeof(float));

	state->impacttime = ent->v.impacttime;
	state->starttime = ent->v.starttime;

	state->modelindex = ent->v.modelindex;

	state->frame = ent->v.frame;

	state->skin = ent->v.skin;
	state->effects = ent->v.effects;

	// halflife-updated's fix, kept (Steven, 2026-11-07; halflife#3228): smooth flying monsters, which
	// their think moves, by FL_FLY. The original's test here (a non-player with an animtime and no
	// velocity) was SDK 2.x's, which halflife-updated replaced. Then the per-entity flags
	// halflife-updated's other fixes set (m_EFlags), and EFLAG_FLESH_SOUND, which the client's
	// bullet impacts read to play flesh sounds on living things.
	if ((ent->v.flags & FL_FLY) != 0)
		state->eflags |= EFLAG_SLERP;
	else
		state->eflags &= ~EFLAG_SLERP;

	CBaseEntity* pFlagsEntity = static_cast<CBaseEntity*>(GET_PRIVATE(ent));
	if (pFlagsEntity)
	{
		state->eflags |= pFlagsEntity->m_EFlags;
		if (pFlagsEntity->Classify() != CLASS_NONE && pFlagsEntity->Classify() != CLASS_MACHINE)
			state->eflags |= EFLAG_FLESH_SOUND;
		else
			state->eflags &= ~EFLAG_FLESH_SOUND;
	}

	state->scale = ent->v.scale;
	state->solid = ent->v.solid;
	state->colormap = ent->v.colormap;

	state->movetype = ent->v.movetype;
	state->sequence = ent->v.sequence;
	state->framerate = ent->v.framerate;
	state->body = ent->v.body;

	for (i = 0; i < 4; i++)
	{
		state->controller[i] = ent->v.controller[i];
	}

	for (i = 0; i < 2; i++)
	{
		state->blending[i] = ent->v.blending[i];
	}

	if ((host->v.iuser3 & 4) != 0 && ent == host->v.euser2)
	{
		state->rendermode = (int)host->v.fuser1;
		state->renderamt = (int)host->v.fuser2;
		state->renderfx = (int)host->v.fuser3;
		state->rendercolor.r = (byte)(int64_t)host->v.vuser1.x;
		state->rendercolor.g = (byte)(int64_t)host->v.vuser1.y;
		state->rendercolor.b = (byte)(int64_t)host->v.vuser1.z;
	}
	else
	{
		state->rendermode = ent->v.rendermode;
		state->renderamt = (int)ent->v.renderamt;
		state->renderfx = ent->v.renderfx;
		state->rendercolor.r = (byte)(int64_t)ent->v.rendercolor.x;
		state->rendercolor.g = (byte)(int64_t)ent->v.rendercolor.y;
		state->rendercolor.b = (byte)(int64_t)ent->v.rendercolor.z;
	}

	if ((host->v.iuser3 & 8) != 0)
	{
		if (ent == host->v.euser2 || ent == host->v.euser3)
		{
			state->rendermode = kRenderTransAdd;
			state->renderamt = 0;
			state->renderfx = kRenderFxNone;
			state->rendercolor.r = 0;
			state->rendercolor.g = 0;
			state->rendercolor.b = 0;
			state->eflags &= ~EFLAG_SLERP;
			state->frame = 0;
			state->sequence = 0;
			state->effects = EF_NODRAW;
			state->animtime = 0;
			for (i = 0; i < 4; i++)
				state->controller[i] = 0;
		}
		else if (ent->v.takedamage == DAMAGE_AIM && (ent->v.flags & FL_NOTARGET) == 0 && host->v.fuser1 > 0)
		{
			state->rendermode = kRenderNormal;
			float flFrac = host->v.fuser1 / 255.0f;
			if (flFrac < 0)
				state->renderamt = 16;
			else if (flFrac > 1)
				state->renderamt = 64;
			else
				state->renderamt = (int)(flFrac * 48.0 + 16.0);
			state->renderfx = kRenderFxGlowShell;
			if (host->v.fuser1 < 1.32)
				state->rendercolor.r = 1;
			else if (host->v.fuser1 > 255.0)
				state->rendercolor.r = 255;
			else
				state->rendercolor.r = (byte)(int64_t)host->v.fuser1;
			state->rendercolor.g = 0;
			state->rendercolor.b = 0;
		}
	}

	state->aiment = 0;
	if (ent->v.aiment)
	{
		state->aiment = ENTINDEX(ent->v.aiment);
	}

	if ((ent->v.iuser3 & 0x20) != 0 && ent->v.aiment && 0 != OFFSET(ent->v.aiment))
	{
		const float flMax = ent->v.fuser1;
		const float flMin = ent->v.fuser2;
		const float flDotHi = ent->v.fuser3;
		const float flDotLo = ent->v.fuser4;

		UTIL_MakeAimVectors(ent->v.aiment->v.angles);
		Vector vecToHost = (host->v.origin - ent->v.aiment->v.origin).Normalize();
		float flDot = DotProduct(-gpGlobals->v_forward, vecToHost);

		float flAmt;
		if (flDot <= flDotLo)
			flAmt = flMin;
		else if (flDot < flDotHi)
			flAmt = (flDot - flDotLo) / (flDotHi - flDotLo) * (flMax - flMin) + flMin;
		else
			flAmt = flMax;

		if (flAmt < flMin)
			state->renderamt = (int)flMin;
		else if (flAmt > flMax)
			state->renderamt = (int)flMax;
		else
			state->renderamt = (int)flAmt;

		if (state->renderamt < 0)
			state->renderamt = 0;
		else if (state->renderamt > 255)
			state->renderamt = 255;
	}

	state->owner = 0;
	if (ent->v.owner)
	{
		int owner = ENTINDEX(ent->v.owner);

		// Only care if owned by a player
		if (owner >= 1 && owner <= gpGlobals->maxClients)
		{
			state->owner = owner;
		}
	}

	// HACK:  Somewhat...
	// Class is overridden for non-players to signify a breakable glass object ( sort of a class? )
	if (0 == player)
	{
		state->playerclass = ent->v.playerclass;
	}

	// Special stuff for players only
	if (0 != player)
	{
		memcpy(state->basevelocity, ent->v.basevelocity, 3 * sizeof(float));

		state->weaponmodel = MODEL_INDEX(STRING(ent->v.weaponmodel));
		state->gaitsequence = ent->v.gaitsequence;
		state->spectator = ent->v.flags & FL_SPECTATOR;
		state->friction = ent->v.friction;

		state->gravity = ent->v.gravity;
		//		state->team			= ent->v.team;
		//
		state->usehull = ((ent->v.flags & FL_DUCKING) != 0 && ent->v.movetype != MOVETYPE_NOCLIP) ? 1 : 0;
		state->health = ent->v.health;
	}

	return 1;
}

/*
===================
CreateBaseline

Creates baselines used for network encoding, especially for player data since players are not spawned until connect time.
===================
*/
// orig 0x104aa0
void CreateBaseline(int player, int eindex, struct entity_state_s* baseline, struct edict_s* entity, int playermodelindex, Vector* player_mins, Vector* player_maxs)
{
	baseline->origin = entity->v.origin;
	baseline->angles = entity->v.angles;
	baseline->frame = entity->v.frame;
	baseline->skin = (short)entity->v.skin;

	// render information
	baseline->rendermode = (byte)entity->v.rendermode;
	baseline->renderamt = (byte)entity->v.renderamt;
	baseline->rendercolor.r = (byte)entity->v.rendercolor.x;
	baseline->rendercolor.g = (byte)entity->v.rendercolor.y;
	baseline->rendercolor.b = (byte)entity->v.rendercolor.z;
	baseline->renderfx = (byte)entity->v.renderfx;

	if (0 != player)
	{
		baseline->mins = *player_mins;
		baseline->maxs = *player_maxs;

		baseline->colormap = eindex;
		baseline->modelindex = playermodelindex;
		baseline->friction = 1.0;
		baseline->movetype = MOVETYPE_WALK;

		baseline->scale = entity->v.scale;
		baseline->solid = SOLID_SLIDEBOX;
		baseline->framerate = 1.0;
		baseline->gravity = 1.0;
	}
	else
	{
		baseline->mins = entity->v.mins;
		baseline->maxs = entity->v.maxs;

		baseline->colormap = 0;
		baseline->modelindex = entity->v.modelindex; //SV_ModelIndex(pr_strings + entity->v.model);
		baseline->movetype = entity->v.movetype;

		baseline->scale = entity->v.scale;
		baseline->solid = entity->v.solid;
		baseline->framerate = entity->v.framerate;
		baseline->gravity = entity->v.gravity;
	}
}

typedef struct
{
	char name[32];
	int field;
} entity_field_alias_t;

#define FIELD_ORIGIN0 0
#define FIELD_ORIGIN1 1
#define FIELD_ORIGIN2 2
#define FIELD_ANGLES0 3
#define FIELD_ANGLES1 4
#define FIELD_ANGLES2 5

static entity_field_alias_t entity_field_alias[] =
	{
		{"origin[0]", 0},
		{"origin[1]", 0},
		{"origin[2]", 0},
		{"angles[0]", 0},
		{"angles[1]", 0},
		{"angles[2]", 0},
};

// orig 0x104cb0
void Entity_FieldInit(struct delta_s* pFields)
{
	entity_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN0].name);
	entity_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN1].name);
	entity_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ORIGIN2].name);
	entity_field_alias[FIELD_ANGLES0].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES0].name);
	entity_field_alias[FIELD_ANGLES1].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES1].name);
	entity_field_alias[FIELD_ANGLES2].field = DELTA_FINDFIELD(pFields, entity_field_alias[FIELD_ANGLES2].name);
}

/*
==================
Entity_Encode

Callback for sending entity_state_t info over network. 
FIXME:  Move to script
==================
*/
// orig 0x104d70
void Entity_Encode(struct delta_s* pFields, const unsigned char* from, const unsigned char* to)
{
	entity_state_t *f, *t;
	static bool initialized = false;

	if (!initialized)
	{
		Entity_FieldInit(pFields);
		initialized = true;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	const bool localplayer = (t->number - 1) == ENGINE_CURRENT_PLAYER();
	if (localplayer)
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}

	if ((t->impacttime != 0) && (t->starttime != 0))
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);

		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ANGLES2].field);
	}

	if ((t->movetype == MOVETYPE_FOLLOW) &&
		(t->aiment != 0))
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
	else if (t->aiment != f->aiment)
	{
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
}

static entity_field_alias_t player_field_alias[] =
	{
		{"origin[0]", 0},
		{"origin[1]", 0},
		{"origin[2]", 0},
};

// orig 0x104f80
void Player_FieldInit(struct delta_s* pFields)
{
	player_field_alias[FIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN0].name);
	player_field_alias[FIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN1].name);
	player_field_alias[FIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, player_field_alias[FIELD_ORIGIN2].name);
}

/*
==================
Player_Encode

Callback for sending entity_state_t for players info over network. 
==================
*/
// orig 0x104ff0
void Player_Encode(struct delta_s* pFields, const unsigned char* from, const unsigned char* to)
{
	entity_state_t *f, *t;
	static bool initialized = false;

	if (!initialized)
	{
		Player_FieldInit(pFields);
		initialized = true;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	const bool localplayer = (t->number - 1) == ENGINE_CURRENT_PLAYER();
	if (localplayer)
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}

	if ((t->movetype == MOVETYPE_FOLLOW) &&
		(t->aiment != 0))
	{
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
	else if (t->aiment != f->aiment)
	{
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN0].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN1].field);
		DELTA_SETBYINDEX(pFields, entity_field_alias[FIELD_ORIGIN2].field);
	}
}

#define CUSTOMFIELD_ORIGIN0 0
#define CUSTOMFIELD_ORIGIN1 1
#define CUSTOMFIELD_ORIGIN2 2
#define CUSTOMFIELD_ANGLES0 3
#define CUSTOMFIELD_ANGLES1 4
#define CUSTOMFIELD_ANGLES2 5
#define CUSTOMFIELD_SKIN 6
#define CUSTOMFIELD_SEQUENCE 7
#define CUSTOMFIELD_ANIMTIME 8

entity_field_alias_t custom_entity_field_alias[] =
	{
		{"origin[0]", 0},
		{"origin[1]", 0},
		{"origin[2]", 0},
		{"angles[0]", 0},
		{"angles[1]", 0},
		{"angles[2]", 0},
		{"skin", 0},
		{"sequence", 0},
		{"animtime", 0},
};

// orig 0x105150
void Custom_Entity_FieldInit(struct delta_s* pFields)
{
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].name);
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].name);
	custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].name);
	custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].name);
	custom_entity_field_alias[CUSTOMFIELD_SKIN].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].name);
	custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].name);
	custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field = DELTA_FINDFIELD(pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].name);
}

/*
==================
Custom_Encode

Callback for sending entity_state_t info ( for custom entities ) over network. 
FIXME:  Move to script
==================
*/
// orig 0x1052a0
void Custom_Encode(struct delta_s* pFields, const unsigned char* from, const unsigned char* to)
{
	entity_state_t *f, *t;
	int beamType;
	static bool initialized = false;

	if (!initialized)
	{
		Custom_Entity_FieldInit(pFields);
		initialized = true;
	}

	f = (entity_state_t*)from;
	t = (entity_state_t*)to;

	beamType = t->rendermode & 0x0f;

	if (beamType != BEAM_POINTS && beamType != BEAM_ENTPOINT)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN0].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN1].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ORIGIN2].field);
	}

	if (beamType != BEAM_POINTS)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES0].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES1].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANGLES2].field);
	}

	if (beamType != BEAM_ENTS && beamType != BEAM_ENTPOINT)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_SKIN].field);
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_SEQUENCE].field);
	}

	// animtime is compared by rounding first
	// see if we really shouldn't actually send it
	if ((int)f->animtime == (int)t->animtime)
	{
		DELTA_UNSETBYINDEX(pFields, custom_entity_field_alias[CUSTOMFIELD_ANIMTIME].field);
	}
}

/*
=================
RegisterEncoders

Allows game .dll to override network encoding of certain types of entities and tweak values, etc.
=================
*/
// orig 0x105440
void RegisterEncoders()
{
	DELTA_ADDENCODER("Entity_Encode", Entity_Encode);
	DELTA_ADDENCODER("Custom_Encode", Custom_Encode);
	DELTA_ADDENCODER("Player_Encode", Player_Encode);
}

// orig 0x1054b0. Rocket Crowbar sends no weapon data: with this and UpdateClientData's empty
// m_iId, a client predicts no weapon, so RC's changed weapons can't mispredict.
int GetWeaponData(struct edict_s* player, struct weapon_data_s* info)
{
	memset(info, 0, MAX_WEAPONS * sizeof(weapon_data_t));
	return 1;
}

/*
=================
UpdateClientData

Data sent to current client only
engine sets cd to 0 before calling.
=================
*/
// orig 0x1054e0. The original's: no spectator target view (iuser1/iuser2), no weapon prediction
// data, fov from pev->fov (which nothing sets), and a noclipping player sent as not ducking.
// Ours sends m_iFOV (halflife-updated's, below).
void UpdateClientData(const edict_t* ent, int sendweapons, struct clientdata_s* cd)
{
	if (!ent || !ent->pvPrivateData)
		return;
	entvars_t* pev = (entvars_t*)&ent->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));

	cd->flags = pev->flags;
	cd->health = pev->health;

	cd->viewmodel = MODEL_INDEX(STRING(pev->viewmodel));

	cd->waterlevel = pev->waterlevel;
	cd->watertype = pev->watertype;
	// halflife-updated stores the player's weapon bitmask in
	// CBasePlayer::m_WeaponBits (std::uint64_t) and ships the bits to its
	// own cl_dll via the custom gmsgWeapons message rather than the
	// engine's per-frame client_data.iWeaponBits delta. Vanilla HL clients
	// (i.e., a stock anniversary client connecting to this server) don't
	// have the matching cl_dll, so they only see cd->weapons / iWeaponBits.
	// Without this sync, pev->weapons stays 0, the WEAPON_SUIT bit (31)
	// never reaches the vanilla HUD, and the HEV display never appears.
	// Replicate the lower 32 bits of m_WeaponBits into cd->weapons so the
	// legacy channel carries WEAPON_SUIT and weapons 0..30 to vanilla
	// clients. The halflife-updated cl_dll ignores cdata->iWeaponBits
	// (see cl_dll/hud_update.cpp:38), so updated clients are unaffected.
	if (pl)
		cd->weapons = static_cast<int>(pl->m_WeaponBits & 0xFFFFFFFF);
	else
		cd->weapons = pev->weapons;

	// Vectors
	cd->origin = pev->origin;
	cd->velocity = pev->velocity;
	cd->view_ofs = pev->view_ofs;
	cd->punchangle = pev->punchangle;

	if (pev->movetype == MOVETYPE_NOCLIP)
	{
		cd->bInDuck = 0;
		cd->flDuckTime = -1;
	}
	else
	{
		cd->bInDuck = pev->bInDuck;
		cd->flDuckTime = pev->flDuckTime;
	}
	cd->flTimeStepSound = pev->flTimeStepSound;
	cd->flSwimTime = pev->flSwimTime;
	cd->waterjumptime = pev->teleport_time;

	strcpy(cd->physinfo, ENGINE_GETPHYSINFO(ent));

	cd->maxspeed = pev->maxspeed;
	// halflife-updated's fix, kept (Steven 2026-11-07): the zoom is m_iFOV, which RC's zooms set
	// (6f8aac9, halflife #3044); the original sent pev->fov, always 0
	cd->fov = pl ? pl->m_iFOV : 0;
	cd->weaponanim = pev->weaponanim;

	cd->pushmsec = pev->pushmsec;
}

/*
=================
CmdStart

We're about to run this usercmd for the specified player.  We can set up groupinfo and masking here, etc.
This is the time to examine the usercmd for anything extra.  This call happens even if think does not.
=================
*/
// orig 0x105670
void CmdStart(const edict_t* player, const struct usercmd_s* cmd, unsigned int random_seed)
{
	entvars_t* pev = (entvars_t*)&player->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));

	if (!pl)
		return;

	if (pl->pev->groupinfo != 0)
	{
		UTIL_SetGroupTrace(pl->pev->groupinfo, GROUP_OP_AND);
	}

	pl->random_seed = random_seed;
}

/*
=================
CmdEnd

Each cmdstart is exactly matched with a cmd end, clean up any group trace flags, etc. here
=================
*/
// orig 0x1056f0
void CmdEnd(const edict_t* player)
{
	entvars_t* pev = (entvars_t*)&player->v;
	CBasePlayer* pl = dynamic_cast<CBasePlayer*>(CBasePlayer::Instance(pev));

	if (!pl)
		return;
	if (pl->pev->groupinfo != 0)
	{
		UTIL_UnsetGroupTrace();
	}
}

/*
================================
ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
// orig 0x105750
int ConnectionlessPacket(const struct netadr_s* net_from, const char* args, char* response_buffer, int* response_buffer_size)
{
	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

/*
================================
GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
// orig 0x105770. The original copies the hull sizes into locals and never writes mins/maxs
// (the 2001 SDK's bug), returning 1 for hulls 0-2 and 0 otherwise, so the engine kept its own
// default hulls. Ours keeps halflife-updated's, which writes those same standard sizes.
int GetHullBounds(int hullnumber, float* mins, float* maxs)
{
	return static_cast<int>(PM_GetHullBounds(hullnumber, mins, maxs));
}

/*
================================
CreateInstancedBaselines

Create pseudo-baselines for items that aren't placed in the map at spawn time, but which are likely
to be created during play ( e.g., grenades, ammo packs, projectiles, corpses, etc. )
================================
*/
// orig 0x105830
void CreateInstancedBaselines()
{
	int iret = 0;
	entity_state_t state;

	memset(&state, 0, sizeof(state));

	// Create any additional baselines here for things like grendates, etc.
	// iret = ENGINE_INSTANCE_BASELINE( pc->pev->classname, &state );

	// Destroy objects.
	//UTIL_Remove( pc );
}

/*
================================
InconsistentFile

One of the ENGINE_FORCE_UNMODIFIED files failed the consistency check for the specified player
 Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
================================
*/
// orig 0x105870
int InconsistentFile(const edict_t* player, const char* filename, char* disconnect_message)
{
	// Server doesn't care?
	if (CVAR_GET_FLOAT("mp_consistency") != 1)
		return 0;

	// Default behavior is to kick the player
	sprintf(disconnect_message, "Server is enforcing file consistency for %s\n", filename);

	// Kick now with specified disconnect message.
	return 1;
}

/*
================================
AllowLagCompensation

 The game .dll should return 1 if lag compensation should be allowed ( could also just set
  the sv_unlag cvar.
 Most games right now should return 0, until client-side weapon prediction code is written
  and tested for them ( note you can predict weapons, but not do lag compensation, too, 
  if you want.
================================
*/
// orig 0x1058d0: Rocket Crowbar predicts no weapons, so it allows no lag compensation.
int AllowLagCompensation()
{
	return 0;
}



