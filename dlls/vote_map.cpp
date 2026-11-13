/***
*
*	Rocket Crowbar 1.9: vote_map.cpp, the map vote (vote.h).
*
****/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "vote.h"

// util.cpp
short FixedSigned16(float value, float scale);
unsigned short FixedUnsigned16(float value, float scale);

bool g_blInVoteMap = false;
float g_flFractForMap = 0.51;
float g_flVMEndTime = -1;
float g_flVMChangeTime = -1; // -1, or when the changelevel to g_szMapName happens
float g_flVMUpdateTime = -1;
int g_iVMFor = 0;
int g_iVMAgainst = 0;
int g_iVMTotal = 0;
int g_iVMMSGColor[4] = {64, 64, 255, 160};
char g_szMapName[65];
EHANDLE g_pVMInit;
EHANDLE g_pVMVoters[VOTE_MAX_VOTERS];

// orig 0x237e70. Starts a vote to change to szMap. Every client votes, the initiator included.
// When allowedmaps.txt exists (more than one byte) the map must be listed in it.
const char* VM_InitVote(CBaseEntity* pInit, const char* szMap)
{
	// VERIFY: the original tests the kick vote here and says "Vote map"; a running map vote is
	// caught below.
	if (g_blInVoteKick)
		return "Vote map in progress, cannot vote map\n";

	if (g_flVMChangeTime != -1)
		return "Changelevel in progress, cant intiate new\n";

	if (strlen(szMap) > 63)
		return "Hey, what are you trying to pull here...";

	if (g_blInVoteMap)
		return UTIL_VarArgs("Vote in progress for %s, %i seconds remaining\n", g_szMapName, (int)(g_flVMEndTime - gpGlobals->time));

	if (pInit && ((CBasePlayer*)pInit)->m_flNextVoteTime > gpGlobals->time)
		return UTIL_VarArgs("Not allowed to call another vote for %i seconds\n", (int)(((CBasePlayer*)pInit)->m_flNextVoteTime - gpGlobals->time));

	if (!IS_MAP_VALID((char*)szMap))
		return UTIL_VarArgs("Map %s not found on server\n", szMap);

	int iLength;
	char* pFileList = (char*)LOAD_FILE_FOR_ME("allowedmaps.txt", &iLength);
	if (pFileList && iLength > 1)
	{
		bool bFound = false;
		char* p = pFileList;
		// The original's buffer is 64 bytes, which " %64s" can overrun by its terminator.
		char szName[65];
		while (*p)
		{
			while (*p && isspace(*p))
				p++;
			if (!*p)
				break;
			if (sscanf(p, " %64s", szName) != 1)
				break;
			// VERIFY: as the original, a name starting with a character <= 12 ends the list.
			if (szName[0] <= 12)
				break;
			if (!stricmp(szName, szMap))
			{
				bFound = true;
				break;
			}
			p += strlen(szName);
		}
		FREE_FILE(pFileList);

		if (!bFound)
			return "Sorry. map is not in server's allowedmaps.txt";
	}

	g_iVMTotal = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer && pPlayer->edict()->pvPrivateData && pPlayer->IsNetClient())
		{
			g_pVMVoters[g_iVMTotal] = pPlayer;
			g_iVMTotal++;
		}
	}

	if (g_iVMTotal < 1)
		return UTIL_VarArgs("Not enough players to vote kick");

	g_flFractForMap = CVAR_GET_FLOAT("sv_mapfract");
	g_iVMFor = 0;
	g_iVMAgainst = 0;
	g_pVMInit = pInit;
	strncpy(g_szMapName, szMap, 64);
	g_szMapName[64] = '\0';

	if (!IS_MAP_VALID(g_szMapName))
		return "The map was sucked into xen, prehapes? (impossible error)";

	g_blInVoteMap = true;
	g_flVMEndTime = gpGlobals->time + 45.0;
	g_flVMUpdateTime = gpGlobals->time - 1.0;
	g_flVMChangeTime = -1;
	if (pInit)
		((CBasePlayer*)pInit)->m_flNextVoteTime = CVAR_GET_FLOAT("sv_votefreq") + gpGlobals->time;

	// VERIFY: guard, the original crashes here: the messages below read a NULL pInit's name. Ours
	// uses the world entity, as VM_ValidatePTRS does for a gone initiator.
	if (!pInit)
		g_pVMInit = pInit = CBaseEntity::Instance(ENT(0));
	UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* Votemap initiated for %s by %s, need %0.1f precent to succed\n", g_szMapName, STRING(g_pVMInit->pev->netname), 100.0 * g_flFractForMap));

	if (IS_DEDICATED_SERVER())
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("* Votemap initiated for %s by %s\n", g_szMapName, STRING(g_pVMInit->pev->netname)));

	UTIL_LogPrintf("\"%s<%i>\" initiated votemap for %s\n", STRING(pInit->pev->netname), GETPLAYERUSERID(pInit->edict()), g_szMapName);

	return UTIL_VarArgs("Vote map sucsessfully initated for %s\n", g_szMapName);
}

// orig 0x238360. A gone initiator is replaced by the world entity, as VK_ValidatePTRS.
void VM_ValidatePTRS()
{
	if (!g_pVMInit || FNullEnt(g_pVMInit->pev) || !g_pVMInit->edict()->pvPrivateData || g_pVMInit->edict()->free)
		g_pVMInit = CBaseEntity::Instance(ENT(0));
}

// orig 0x238450
void VM_VoteFail()
{
	VM_ValidatePTRS();
	g_blInVoteMap = false;

	UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* Votemap for %s by %s has FAILED, final fraction was %0.1f for changelevel\n", g_szMapName, STRING(g_pVMInit->pev->netname), (double)g_iVMFor / g_iVMTotal * 100.0));

	for (int i = 0; i < g_iVMTotal; i++)
	{
		if (g_pVMVoters[i])
			CLIENT_COMMAND(g_pVMVoters[i]->edict(), "bind \"1\" \"slot1\"\nbind \"2\" \"slot2\"\n");
	}

	if (IS_DEDICATED_SERVER())
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("* Votemap for %s by %s has failed\n", g_szMapName, STRING(g_pVMInit->pev->netname)));

	UTIL_LogPrintf("Votemap initiated by \"%s<%i>\" for %s has failed\n", STRING(g_pVMInit->pev->netname), GETPLAYERUSERID(g_pVMInit->edict()), g_szMapName);
}

// orig 0x238620. The changelevel itself happens 5 seconds later, in VM_VoteFrame.
void VM_VoteSucceede()
{
	VM_ValidatePTRS();
	g_blInVoteMap = false;

	UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* Votemap for %s by %s has SUCCEDED, final fraction was %0.1f for changelevel\n", g_szMapName, STRING(g_pVMInit->pev->netname), (double)g_iVMFor / g_iVMTotal * 100.0));
	UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* Changing level to %s in 5 seconds\n", g_szMapName));

	for (int i = 0; i < g_iVMTotal; i++)
	{
		if (g_pVMVoters[i])
			CLIENT_COMMAND(g_pVMVoters[i]->edict(), "bind \"1\" \"slot1\"\nbind \"2\" \"slot2\"\n");
	}

	g_flVMChangeTime = gpGlobals->time + 5.0;

	if (IS_DEDICATED_SERVER())
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("* Votemap for %s by %s has succeded\n", g_szMapName, STRING(g_pVMInit->pev->netname)));

	UTIL_LogPrintf("Votemap initiated by \"%s<%i>\" for %s has succeded\n", STRING(g_pVMInit->pev->netname), GETPLAYERUSERID(g_pVMInit->edict()), g_szMapName);
}

// orig 0x238840. bReset puts keys 1 and 2 back on slot1/slot2, else binds them to vmyes/vmno.
void VM_KeysBind(edict_t* pEntity, bool bReset)
{
	if (bReset)
	{
		CLIENT_COMMAND(pEntity, "bind \"1\" \"slot1\"\n");
		CLIENT_COMMAND(pEntity, "bind \"2\" \"slot2\"\n");
	}
	else
	{
		CLIENT_COMMAND(pEntity, "bind \"1\" \"vmyes\"\n");
		CLIENT_COMMAND(pEntity, "bind \"2\" \"vmno\"\n");
	}
}

// orig 0x2388c0. As VK_SendVKMsg; the tally is on channel 5 here too.
void VM_SendVMMsg(edict_t* pEntity, bool bVoter)
{
	char szMsg[512];

	if (bVoter)
	{
		sprintf(szMsg, "Change to map %s? PRESS number keys to vote:\n  1 - For yes\n  2 - For no ", g_szMapName);

		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, pEntity);
		WRITE_BYTE(TE_TEXTMESSAGE);
		WRITE_BYTE(5); // channel
		WRITE_SHORT(FixedSigned16(-1, 1 << 13)); // x
		WRITE_SHORT(FixedSigned16(-1, 1 << 13)); // y
		WRITE_BYTE(0);							  // effect
		WRITE_BYTE(g_iVMMSGColor[0]);
		WRITE_BYTE(g_iVMMSGColor[1]);
		WRITE_BYTE(g_iVMMSGColor[2]);
		WRITE_BYTE(g_iVMMSGColor[3]);
		WRITE_BYTE(g_iVMMSGColor[0]);
		WRITE_BYTE(g_iVMMSGColor[1]);
		WRITE_BYTE(g_iVMMSGColor[2]);
		WRITE_BYTE(g_iVMMSGColor[3]);
		WRITE_SHORT(0); // fade in
	}
	else
	{
		sprintf(szMsg, "Vote to change level to %s\n %0.1f for\n %0.1f against\n %0.1f not yet voted\n %i Seconds left",
			g_szMapName,
			(double)g_iVMFor / g_iVMTotal * 100.0,
			(double)g_iVMAgainst / g_iVMTotal * 100.0,
			(double)(g_iVMTotal - (g_iVMAgainst + g_iVMFor)) / g_iVMTotal * 100.0,
			(int)(g_flVMEndTime - gpGlobals->time));

		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, pEntity);
		WRITE_BYTE(TE_TEXTMESSAGE);
		WRITE_BYTE(5); // channel
		WRITE_SHORT(FixedSigned16(1, 1 << 13)); // x
		WRITE_SHORT(0);							 // y
		WRITE_BYTE(0);							 // effect
		WRITE_BYTE(g_iVMMSGColor[0]);
		WRITE_BYTE(g_iVMMSGColor[1]);
		WRITE_BYTE(g_iVMMSGColor[2]);
		WRITE_BYTE(g_iVMMSGColor[3]);
		WRITE_BYTE(g_iVMMSGColor[0]);
		WRITE_BYTE(g_iVMMSGColor[1]);
		WRITE_BYTE(g_iVMMSGColor[2]);
		WRITE_BYTE(g_iVMMSGColor[3]);
		WRITE_SHORT(0); // fade in
	}

	WRITE_SHORT(0);								  // fade out
	WRITE_SHORT(FixedUnsigned16(0.5, 1 << 8)); // hold time
	WRITE_STRING(szMsg);
	MESSAGE_END();
}

// orig 0x238cc0. From StartFrame. Does a pending changelevel (running "exec <map>.cfg" first
// when sv_mapcfg is set), else runs the vote as VK_VoteFrame.
void VM_VoteFrame()
{
	if (g_flVMChangeTime != -1 && g_flVMChangeTime < gpGlobals->time)
	{
		g_flVMChangeTime = -1;
		if (IS_MAP_VALID(g_szMapName))
		{
			if (CVAR_GET_FLOAT("sv_mapcfg") != 0)
			{
				char szCommand[128];
				sprintf(szCommand, "exec %s.cfg\n", g_szMapName);
				SERVER_COMMAND(szCommand);
			}
			CHANGE_LEVEL(g_szMapName, NULL);
		}
		return;
	}

	if (!g_blInVoteMap)
		return;

	if (g_flVMEndTime < gpGlobals->time)
	{
		VM_VoteFail();
		return;
	}

	if (g_flVMUpdateTime > gpGlobals->time)
		return;
	g_flVMUpdateTime = gpGlobals->time + 0.5;

	VM_ValidatePTRS();

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (!pPlayer || !pPlayer->IsNetClient())
			continue;

		bool bVoter = false;
		for (int j = 0; j < g_iVMTotal; j++)
		{
			if (g_pVMVoters[j] && g_pVMVoters[j]->pev == pPlayer->pev)
			{
				bVoter = true;
				break;
			}
		}

		if (bVoter)
		{
			VM_SendVMMsg(pPlayer->edict(), true);
			VM_KeysBind(pPlayer->edict(), false);
		}
		else
			VM_SendVMMsg(pPlayer->edict(), false);
	}
}

// orig 0x238f30. As VK_RegisterVote, except the vote passes at a yes fraction of sv_mapfract
// or more (the kick vote wants more than sv_kickfract).
void VM_RegisterVote(CBaseEntity* pVoter, bool bFor)
{
	if (!g_blInVoteMap || !pVoter)
		return;

	int i;
	for (i = 0; i < g_iVMTotal; i++)
	{
		if (g_pVMVoters[i] && g_pVMVoters[i]->pev == pVoter->pev)
			break;
	}
	if (i >= g_iVMTotal)
	{
		CLIENT_PRINTF(pVoter->edict(), print_console, "Cant re-vote\n");
		return;
	}

	g_pVMVoters[i] = NULL;
	if (bFor)
		g_iVMFor++;
	else
		g_iVMAgainst++;

	int iNotVoted = g_iVMTotal - g_iVMFor - g_iVMAgainst;
	if ((double)g_iVMFor / g_iVMTotal >= g_flFractForMap)
		VM_VoteSucceede();
	else if ((double)g_iVMAgainst / g_iVMTotal > g_flFractForMap || (double)(iNotVoted + g_iVMFor) / g_iVMTotal < g_flFractForMap)
		VM_VoteFail();

	VM_KeysBind(pVoter->edict(), true);
	if (bFor)
		CLIENT_PRINTF(pVoter->edict(), print_console, "Vote to change level registered\n");
	else
		CLIENT_PRINTF(pVoter->edict(), print_console, "Vote NOT to change level registered\n");
}
