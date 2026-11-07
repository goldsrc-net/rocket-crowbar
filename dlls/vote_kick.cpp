/***
*
*	Rocket Crowbar 1.9: vote_kick.cpp, the kick vote (vote.h).
*
****/

#include <ctype.h>
#include <string.h>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "vote.h"

// util.cpp (external in the original; the SDK had them static)
short FixedSigned16(float value, float scale);
unsigned short FixedUnsigned16(float value, float scale);

// client.cpp
bool IsClientListenServer(edict_t* pEntity);

bool g_blInVoteKick = false;
float g_flVKFractForKick = 0.51;
float g_flVKEndTime = -1;
float g_flVKUpdateTime = -1;
int g_iVKFor = 0;
int g_iVKAgainst = 0;
int g_iVKTotal = 0;
int g_iVKMSGColor[4] = {255, 64, 64, 160};
EHANDLE g_pVKBoot;
EHANDLE g_pVKInit;
EHANDLE g_pVKVoters[VOTE_MAX_VOTERS];

// orig 0x236760. Starts a vote to kick player iIndex; pInit (may be NULL, the server) does not
// vote, nor does the target. Returns the message for the caller.
const char* VK_InitVote(CBaseEntity* pInit, int iIndex)
{
	if (g_blInVoteKick)
		return UTIL_VarArgs("Vote in progress for %s, %i seconds remaining\n", STRING(g_pVKBoot->pev->netname), (int)(g_flVKEndTime - gpGlobals->time));

	if (g_blInVoteMap)
		return "Vote map in progress, cannot vote kick\n";

	if (pInit && ((CBasePlayer*)pInit)->m_flNextVoteTime > gpGlobals->time)
		return UTIL_VarArgs("Not allowed to call another vote for %i seconds\n", (int)(((CBasePlayer*)pInit)->m_flNextVoteTime - gpGlobals->time));

	g_pVKBoot = UTIL_PlayerByIndex(iIndex);
	if (!g_pVKBoot)
		return UTIL_VarArgs("Cannot find client %i to kick\n", iIndex);

	if (IsClientListenServer(g_pVKBoot->edict()))
		return "Cannot kick the host\n";

	g_iVKTotal = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		// VERIFY: as the original, pInit is dereferenced here even though it may be NULL above.
		if (pPlayer && pPlayer->edict()->pvPrivateData && pPlayer->IsNetClient() && pPlayer->edict() != pInit->edict() && pPlayer->edict() != g_pVKBoot->edict())
		{
			g_pVKVoters[g_iVKTotal] = pPlayer;
			g_iVKTotal++;
		}
	}

	if (g_iVKTotal < 1)
		return UTIL_VarArgs("Not enough players to vote kick");

	g_flVKFractForKick = CVAR_GET_FLOAT("sv_kickfract");
	g_iVKFor = 0;
	g_iVKAgainst = 0;
	g_pVKInit = pInit;
	g_blInVoteKick = true;
	g_flVKEndTime = gpGlobals->time + 45.0;
	g_flVKUpdateTime = gpGlobals->time - 1.0;
	if (pInit)
		((CBasePlayer*)pInit)->m_flNextVoteTime = CVAR_GET_FLOAT("sv_votefreq") + gpGlobals->time;

	// The original's arguments: the target's name first, so it reads "for <target> against <initiator>".
	UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* Votekick initiated for %s against %s, need %0.1f precent to succed\n", STRING(g_pVKBoot->pev->netname), STRING(pInit->pev->netname), 100.0 * g_flVKFractForKick));

	if (IS_DEDICATED_SERVER())
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("* Votekick initiated against %s by %s\n", STRING(g_pVKBoot->pev->netname), STRING(pInit->pev->netname)));

	UTIL_LogPrintf("\"%s<%i>\" initiated votekick against \"%s<%i>\"\n", STRING(pInit->pev->netname), GETPLAYERUSERID(pInit->edict()), STRING(g_pVKBoot->pev->netname), GETPLAYERUSERID(g_pVKBoot->edict()));

	return UTIL_VarArgs("Vote sucsessfully initiated for %s\n", STRING(g_pVKBoot->pev->netname));
}

// orig 0x236c30. A disconnecting target fails the vote.
void VK_VoteDissconnect(edict_t* pEntity)
{
	if (!g_blInVoteKick)
		return;
	if (!g_pVKBoot || !pEntity)
		return;
	if (ENTINDEX(pEntity) == ENTINDEX(g_pVKBoot->edict()))
		VK_VoteFail();
}

// orig 0x236cc0. A target or initiator that is gone is replaced by the world entity
// (CBaseEntity::Instance of entity offset 0, never NULL), so the messages after can still read a
// name; a gone target also fails the vote.
void VK_ValidatePTRS()
{
	if (!g_pVKBoot || FNullEnt(g_pVKBoot->pev) || !g_pVKBoot->edict()->pvPrivateData || g_pVKBoot->edict()->free)
	{
		g_pVKBoot = CBaseEntity::Instance(ENT(0));
		VK_VoteFail();
	}

	if (!g_pVKInit || FNullEnt(g_pVKInit->pev) || !g_pVKInit->edict()->pvPrivateData || g_pVKInit->edict()->free)
		g_pVKInit = CBaseEntity::Instance(ENT(0));
}

// orig 0x236e80
void VK_VoteFail()
{
	g_blInVoteKick = false;
	VK_ValidatePTRS();

	UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* Votekick against %s by %s has FAILED, final fraction was %0.1f for kick\n", STRING(g_pVKBoot->pev->netname), STRING(g_pVKInit->pev->netname), (double)g_iVKFor / g_iVKTotal * 100.0));

	for (int i = 0; i < g_iVKTotal; i++)
	{
		if (g_pVKVoters[i])
			CLIENT_COMMAND(g_pVKVoters[i]->edict(), "bind \"1\" \"slot1\"\nbind \"2\" \"slot2\"\n");
	}

	if (IS_DEDICATED_SERVER())
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("* Votekick against %s by %s has FAILED\n", STRING(g_pVKBoot->pev->netname), STRING(g_pVKInit->pev->netname)));

	UTIL_LogPrintf("Votekick initiated by \"%s<%i>\" against \"%s<%i>\" has failed\n", STRING(g_pVKInit->pev->netname), GETPLAYERUSERID(g_pVKInit->edict()), STRING(g_pVKBoot->pev->netname), GETPLAYERUSERID(g_pVKBoot->edict()));
}

// orig 0x2370d0. Nothing in the game library calls it.
// VERIFY: as the original, it wants exactly 4 dots (a.b.c.d. passes, a.b.c.d does not) and 4 to
// 12 digits in the first 63 characters, and range-checks the first number once per field
// (atoi of the whole copy, not of the field).
bool VK_IsValidIp(char* szIp)
{
	int iDots = 0;
	int iDigits = 0;
	char* p = szIp;
	for (int i = 0; i < 63 && *p; i++, p++)
	{
		if (*p == '.')
			iDots++;
		else if (!isdigit(*p))
			return false;
		else
			iDigits++;
	}

	if (iDots != 4 || (unsigned int)(iDigits - 4) > 8)
		return false;

	char szCopy[64];
	strncpy(szCopy, szIp, 64);
	szCopy[63] = '\0';
	for (char* pTok = strtok(szCopy, "."); pTok; pTok = strtok(NULL, "."))
	{
		if ((unsigned int)atoi(szCopy) > 255)
			return false;
	}
	return true;
}

// orig 0x2372a0. Kicks and bans the target for sv_kickbantime minutes.
void VK_VoteSucceede()
{
	g_blInVoteKick = false;
	VK_ValidatePTRS();

	UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* Votekick against %s by %s has SUCCEDED, final fraction was %0.1f for kick\n", STRING(g_pVKBoot->pev->netname), STRING(g_pVKInit->pev->netname), (double)g_iVKFor / g_iVKTotal * 100.0));

	if (IS_DEDICATED_SERVER())
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("* Votekick against %s by %s has SUCCEDED\n", STRING(g_pVKBoot->pev->netname), STRING(g_pVKInit->pev->netname)));

	UTIL_LogPrintf("Votekick initiated by \"%s<%i>\" against \"%s<%i>\" has succeded\n", STRING(g_pVKInit->pev->netname), GETPLAYERUSERID(g_pVKInit->edict()), STRING(g_pVKBoot->pev->netname), GETPLAYERUSERID(g_pVKBoot->edict()));

	for (int i = 0; i < g_iVKTotal; i++)
	{
		if (g_pVKVoters[i])
			CLIENT_COMMAND(g_pVKVoters[i]->edict(), "bind \"1\" \"slot1\"\nbind \"2\" \"slot2\"\n");
	}

	if (GETPLAYERUSERID(g_pVKBoot->edict()) != -1)
	{
		// The original also reads (CBaseEntity*)g_pVKBoot here and drops the result.
		char szCommand[512];
		sprintf(szCommand, "banid %i #%i kick\n", (int)CVAR_GET_FLOAT("sv_kickbantime"), GETPLAYERUSERID(g_pVKBoot->edict()));
		SERVER_COMMAND(szCommand);
	}
}

// orig 0x237620. bReset puts keys 1 and 2 back on slot1/slot2, else binds them to vkyes/vkno.
void VK_VKKeysBind(edict_t* pEntity, bool bReset)
{
	if (bReset)
	{
		CLIENT_COMMAND(pEntity, "bind \"1\" \"slot1\"\n");
		CLIENT_COMMAND(pEntity, "bind \"2\" \"slot2\"\n");
	}
	else
	{
		CLIENT_COMMAND(pEntity, "bind \"1\" \"vkyes\"\n");
		CLIENT_COMMAND(pEntity, "bind \"2\" \"vkno\"\n");
	}
}

// orig 0x2376a0. The vote text, as an unreliable TE_TEXTMESSAGE held for half a second (sent
// every half second): the question, centred (channel 5), to a player who has not voted yet, the
// running tally at the right (channel 4) to everyone else.
void VK_SendVKMsg(edict_t* pEntity, bool bVoter)
{
	char szMsg[512];

	if (bVoter)
	{
		sprintf(szMsg, "Kick %s off this server? Press number key:\n  1 - For yes\n  2 - For no ", STRING(g_pVKBoot->pev->netname));

		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, pEntity);
		WRITE_BYTE(TE_TEXTMESSAGE);
		WRITE_BYTE(5); // channel
		WRITE_SHORT(FixedSigned16(-1, 1 << 13)); // x
		WRITE_SHORT(FixedSigned16(-1, 1 << 13)); // y
		WRITE_BYTE(0);							  // effect
		WRITE_BYTE(g_iVKMSGColor[0]);
		WRITE_BYTE(g_iVKMSGColor[1]);
		WRITE_BYTE(g_iVKMSGColor[2]);
		WRITE_BYTE(g_iVKMSGColor[3]);
		WRITE_BYTE(g_iVKMSGColor[0]);
		WRITE_BYTE(g_iVKMSGColor[1]);
		WRITE_BYTE(g_iVKMSGColor[2]);
		WRITE_BYTE(g_iVKMSGColor[3]);
		WRITE_SHORT(0); // fade in
	}
	else
	{
		sprintf(szMsg, "Vote to kick %s\n %0.1f for\n %0.1f against\n %0.1f not yet voted\n %i Seconds left",
			STRING(g_pVKBoot->pev->netname),
			(double)g_iVKFor / g_iVKTotal * 100.0,
			(double)g_iVKAgainst / g_iVKTotal * 100.0,
			(double)(g_iVKTotal - (g_iVKAgainst + g_iVKFor)) / g_iVKTotal * 100.0,
			(int)(g_flVKEndTime - gpGlobals->time));

		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, pEntity);
		WRITE_BYTE(TE_TEXTMESSAGE);
		WRITE_BYTE(4); // channel
		WRITE_SHORT(FixedSigned16(1, 1 << 13)); // x
		WRITE_SHORT(0);							 // y
		WRITE_BYTE(0);							 // effect
		WRITE_BYTE(g_iVKMSGColor[0]);
		WRITE_BYTE(g_iVKMSGColor[1]);
		WRITE_BYTE(g_iVKMSGColor[2]);
		WRITE_BYTE(g_iVKMSGColor[3]);
		WRITE_BYTE(g_iVKMSGColor[0]);
		WRITE_BYTE(g_iVKMSGColor[1]);
		WRITE_BYTE(g_iVKMSGColor[2]);
		WRITE_BYTE(g_iVKMSGColor[3]);
		WRITE_SHORT(0); // fade in
	}

	WRITE_SHORT(0);								  // fade out
	WRITE_SHORT(FixedUnsigned16(0.5, 1 << 8)); // hold time
	WRITE_STRING(szMsg);
	MESSAGE_END();
}

// orig 0x237ae0. From StartFrame. Fails the vote at its end time or when the target is gone,
// else every half second shows each player the question (and binds 1 and 2) or the tally.
void VK_VoteFrame()
{
	if (!g_blInVoteKick)
		return;

	if (g_flVKEndTime < gpGlobals->time)
	{
		VK_VoteFail();
		return;
	}

	if (g_flVKUpdateTime > gpGlobals->time)
		return;
	g_flVKUpdateTime = gpGlobals->time + 0.5;

	VK_ValidatePTRS();
	if (g_pVKBoot->edict() == CBaseEntity::Instance(ENT(0))->edict())
	{
		VK_VoteFail();
		return;
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (!pPlayer || !pPlayer->IsNetClient())
			continue;

		bool bVoter = false;
		for (int j = 0; j < g_iVKTotal; j++)
		{
			if (g_pVKVoters[j] && g_pVKVoters[j]->pev == pPlayer->pev)
			{
				bVoter = true;
				break;
			}
		}

		if (bVoter)
		{
			VK_SendVKMsg(pPlayer->edict(), true);
			VK_VKKeysBind(pPlayer->edict(), false);
		}
		else
			VK_SendVKMsg(pPlayer->edict(), false);
	}
}

// orig 0x237cc0. From the vkyes/vkno commands. A voter is struck off the list once counted.
// The vote passes when the yes fraction is over sv_kickfract, and fails when the no fraction is
// over it or yes plus not-yet-voted is under it.
void VK_RegisterVote(CBaseEntity* pVoter, bool bFor)
{
	if (!g_blInVoteKick || !pVoter)
		return;

	int i;
	for (i = 0; i < g_iVKTotal; i++)
	{
		if (g_pVKVoters[i] && g_pVKVoters[i]->pev == pVoter->pev)
			break;
	}
	if (i >= g_iVKTotal)
	{
		CLIENT_PRINTF(pVoter->edict(), print_console, "Cant re-vote\n");
		return;
	}

	g_pVKVoters[i] = NULL;
	if (bFor)
		g_iVKFor++;
	else
		g_iVKAgainst++;

	int iNotVoted = g_iVKTotal - g_iVKFor - g_iVKAgainst;
	if ((double)g_iVKFor / g_iVKTotal > g_flVKFractForKick)
		VK_VoteSucceede();
	else if ((double)g_iVKAgainst / g_iVKTotal > g_flVKFractForKick || (double)(iNotVoted + g_iVKFor) / g_iVKTotal < g_flVKFractForKick)
		VK_VoteFail();

	VK_VKKeysBind(pVoter->edict(), true);
	if (bFor)
		CLIENT_PRINTF(pVoter->edict(), print_console, "Vote to kick registered\n");
	else
		CLIENT_PRINTF(pVoter->edict(), print_console, "Vote NOT to kick registered\n");
}
