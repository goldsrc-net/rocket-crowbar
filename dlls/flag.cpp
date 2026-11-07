/***
*
*	Rocket Crowbar 1.9: flag.cpp. The capture-the-flag flag (classname "flag"). An enemy touching
*	it picks it up; touching your own dropped flag returns it; touching your own flag at its base
*	while carrying theirs captures. Teams are 1-based (GetTeamIndex + 1).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "shake.h"
#include "flag.h"

// teamplay_gamerules.cpp (RC additions there)
extern const char* GetTeamNameForIndex(int iTeam);
extern void AddTeamScore(int iTeam, int iScore);

LINK_ENTITY_TO_CLASS(flag, CBaseFlag); // orig 0x188490

// orig 0x1884c0
void CBaseFlag::Spawn()
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(edict(), "models/flag.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));
	UTIL_SetOrigin(pev, pev->origin);
	pev->classname = MAKE_STRING("flag");
	SetThink(&CBaseFlag::FlagThink);
	SetTouch(&CBaseFlag::FlagTouch);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = GetFlagBaseColor(m_iTeam);
	pev->renderamt = 64.0;
	AddTeamScore(m_iTeam, 0);
	DROP_TO_FLOOR(edict());
}

// orig 0x188630
void CBaseFlag::Precache()
{
	PRECACHE_MODEL("models/flag.mdl");
	PRECACHE_MODEL("models/flagt.mdl");
}

// orig 0x188670: a flag at its base
CBaseFlag* CBaseFlag::CreateFlag(Vector vecOrigin, Vector vecAngles, int iTeam)
{
	CBaseFlag* pFlag = GetClassPtr((CBaseFlag*)NULL);
	UTIL_SetOrigin(pFlag->pev, vecOrigin);
	pFlag->pev->angles = vecAngles;
	pFlag->m_vecBaseOrigin = vecOrigin;
	pFlag->m_vecBaseAngles = vecAngles;
	pFlag->m_iTeam = iTeam;
	pFlag->m_iAtBase = 1;
	pFlag->m_iCarried = 0;
	pFlag->Spawn();
	return pFlag;
}

// orig 0x188720
void CBaseFlag::FlagTouch(CBaseEntity* pOther)
{
	char szMsg[256];

	if ((pOther->pev->flags & FL_CLIENT) == 0)
		return;

	CBasePlayer* pPlayer = GetClassPtr((CBasePlayer*)pOther->pev);

	if (m_iAtBase != 1)
	{
		// own flag, dropped: return it
		if (g_pGameRules->GetTeamIndex(pPlayer->m_szTeamName) + 1 == m_iTeam)
		{
			FlagReturn();
			sprintf(szMsg, "* %s returned the %s (%s) flag!\n", STRING(pOther->pev->netname), pPlayer->m_szTeamName, GetFlagTeamColor(m_iTeam));
			UTIL_SayTextAll(szMsg, pOther);
			UTIL_LogPrintf("\"%s<%i>\" returned the %s (%s) flag\n", STRING(pOther->pev->netname), GETPLAYERUSERID(pOther->pev->pContainingEntity), pPlayer->m_szTeamName, GetFlagTeamColor(m_iTeam));
		}
	}
	else
	{
		// own flag at its base, carrying theirs: capture
		if (g_pGameRules->GetTeamIndex(pPlayer->m_szTeamName) + 1 == m_iTeam)
		{
			CBaseFlag* pCarried = DoesEntHaveFlag(pOther);
			if (pCarried)
			{
				pPlayer->AddPoints((int)CVAR_GET_FLOAT("mp_points_per_capture"), true);
				AddTeamScore(m_iTeam, (int)CVAR_GET_FLOAT("mp_points_per_capture"));
				sprintf(szMsg, "* %s of team %s (%s) captured the %s (%s) flag!\n", STRING(pOther->pev->netname), pPlayer->m_szTeamName, GetFlagTeamColor(m_iTeam), GetTeamNameForIndex(pCarried->m_iTeam), GetFlagTeamColor(pCarried->m_iTeam));
				UTIL_SayTextAll(szMsg, pOther);
				UTIL_LogPrintf("\"%s<%i>\" of team %s (%s) captured the %s (%s) flag\n", STRING(pOther->pev->netname), GETPLAYERUSERID(pOther->pev->pContainingEntity), pPlayer->m_szTeamName, GetFlagTeamColor(m_iTeam), GetTeamNameForIndex(pCarried->m_iTeam), GetFlagTeamColor(pCarried->m_iTeam));
				pCarried->FlagReturn();

				pOther->pev->renderfx = kRenderFxNone;
				pOther->pev->rendercolor = g_vecZero;
				pOther->pev->renderamt = 0;
				UTIL_ScreenFade(pOther, GetFlagBaseColor(pCarried->m_iTeam), 1.0, 0.0, 32, 0);
			}
		}
	}

	if (g_pGameRules->GetTeamIndex(pPlayer->m_szTeamName) + 1 == m_iTeam)
		return;

	// an enemy flag: pick it up
	if (DoesEntHaveFlag(pOther) == NULL)
	{
		FlagPickUp(pOther);
		sprintf(szMsg, "* %s of team %s (%s) got the %s (%s) flag!\n", STRING(pOther->pev->netname), pPlayer->m_szTeamName, GetFlagTeamColor(g_pGameRules->GetTeamIndex(pPlayer->m_szTeamName) + 1), GetTeamNameForIndex(m_iTeam), GetFlagTeamColor(m_iTeam));
		UTIL_SayTextAll(szMsg, pOther);
		UTIL_LogPrintf("\"%s<%i>\" of team %s (%s) got the %s (%s) flag\n", STRING(pOther->pev->netname), GETPLAYERUSERID(pOther->pev->pContainingEntity), pPlayer->m_szTeamName, GetFlagTeamColor(g_pGameRules->GetTeamIndex(pPlayer->m_szTeamName) + 1), GetTeamNameForIndex(m_iTeam), GetFlagTeamColor(m_iTeam));
		UTIL_ScreenFade(pOther, GetFlagBaseColor(m_iTeam), 1.0, 1.0, 32, FFADE_OUT | FFADE_STAYOUT);
	}
	else if (m_flNextMsgTime < gpGlobals->time)
	{
		sprintf(szMsg, "* You can't carry more than 1 flag!\n");
		UTIL_SayText(szMsg, pPlayer);
		m_flNextMsgTime = gpGlobals->time + 1.0;
	}
}

// orig 0x188d30: returns home when it leaves the world or its return time runs out; a carried
// flag keeps its carrier glowing in the team colour
void CBaseFlag::FlagThink()
{
	char szMsg[256];

	if (!IsInWorld())
	{
		FlagReturn();
		sprintf(szMsg, "* The %s (%s) flag has returned!\n", GetTeamNameForIndex(m_iTeam), GetFlagTeamColor(m_iTeam));
		UTIL_SayTextAll(szMsg, this);
		UTIL_LogPrintf("The %s (%s) flag has returned\n", GetTeamNameForIndex(m_iTeam), GetFlagTeamColor(m_iTeam));
	}

	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	// pev->dmg: when a dropped flag goes home, 0 for never (FlagDrop)
	if (pev->dmg < gpGlobals->time && pev->dmg != 0 && m_iAtBase == 0 && m_iCarried == 0)
	{
		FlagReturn();
		sprintf(szMsg, "* The %s (%s) flag has returned!\n", GetTeamNameForIndex(m_iTeam), GetFlagTeamColor(m_iTeam));
		UTIL_SayTextAll(szMsg, this);
		UTIL_LogPrintf("The %s (%s) flag has returned\n", GetTeamNameForIndex(m_iTeam), GetFlagTeamColor(m_iTeam));
	}

	if (m_iCarried == 0)
		return;

	if (pev->owner)
	{
		pev->owner->v.renderfx = kRenderFxGlowShell;
		pev->owner->v.rendercolor = GetFlagBaseColor(m_iTeam);
		pev->owner->v.renderamt = 64;
	}
}

// orig 0x188f40: the flag rides along invisibly (model "")
void CBaseFlag::FlagPickUp(CBaseEntity* pCarrier)
{
	pev->owner = pCarrier->pev->pContainingEntity;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL(edict(), "");
	m_iAtBase = 0;
	m_iCarried = 1;
}

// orig 0x188fc0: tossed up where the carrier was, facing his way
void CBaseFlag::FlagDrop(entvars_t* pevCarrier)
{
	pev->owner = NULL;
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_TOSS;
	SET_MODEL(edict(), "models/flag.mdl");
	m_iCarried = 0;
	m_iAtBase = 0;

	float flYaw = pevCarrier->v_angle.y;
	UTIL_SetOrigin(pev, pevCarrier->origin);
	pev->angles = Vector(0, flYaw, 0);
	pev->velocity = Vector(0, 0, RANDOM_FLOAT(100, 500));

	if (CVAR_GET_FLOAT("mp_flag_return_time") > 0)
		pev->dmg = CVAR_GET_FLOAT("mp_flag_return_time") + gpGlobals->time;
	else
		pev->dmg = 0;
}

// orig 0x189110
void CBaseFlag::FlagReturn()
{
	pev->owner = NULL;
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_TOSS;
	SET_MODEL(edict(), "models/flag.mdl");
	m_iCarried = 0;
	m_iAtBase = 1;
	UTIL_SetOrigin(pev, m_vecBaseOrigin);
	pev->angles = m_vecBaseAngles;
	DROP_TO_FLOOR(edict());
}

// orig 0x1891d0
CBaseFlag* DoesEntHaveFlag(CBaseEntity* pEntity)
{
	CBaseFlag* pFlag = NULL;

	while ((pFlag = (CBaseFlag*)UTIL_FindEntityByClassname(pFlag, "flag")) != NULL)
	{
		if (pFlag->m_iAtBase == 0 && pFlag->m_iCarried != 0 && pFlag->pev->owner == pEntity->pev->pContainingEntity)
			return pFlag;
	}

	return NULL;
}

// orig 0x189240
char* GetFlagStatusString(CBaseFlag* pFlag, CBasePlayer* pPlayer)
{
	static char szReturnString[256];
	const char* pszFormat;

	if (pFlag->m_iAtBase != 0)
	{
		pszFormat = "is at its base\n";
	}
	else if (pFlag->m_iCarried == 0)
	{
		pszFormat = "has been dropped somewhere\n";
	}
	else
	{
		CBasePlayer* pCarrier = pFlag->pev->owner ? GetClassPtr((CBasePlayer*)VARS(pFlag->pev->owner)) : NULL;

		if (pCarrier == NULL)
		{
			pszFormat = "is being carried by a NULL entity, please report this\n";
		}
		else if (pPlayer != pCarrier)
		{
			sprintf(szReturnString, "is being carried by %s of team %s (%s)\n", STRING(pFlag->pev->owner->v.netname), pCarrier->m_szTeamName, GetFlagTeamColor(g_pGameRules->GetTeamIndex(pCarrier->m_szTeamName) + 1));
			return szReturnString;
		}
		else
		{
			pszFormat = "is being carried by you\n";
		}
	}

	sprintf(szReturnString, "%s", pszFormat); // the original passes pszFormat as the format
	return szReturnString;
}

// orig 0x189340
char* GetFlagNameString(CBaseFlag* pFlag, CBasePlayer* pPlayer)
{
	static char szReturnString[128];

	if (pFlag->m_iTeam == g_pGameRules->GetTeamIndex(pPlayer->m_szTeamName) + 1)
		sprintf(szReturnString, "Your flag ");
	else
		sprintf(szReturnString, "The %s (%s) flag ", GetTeamNameForIndex(pFlag->m_iTeam), GetFlagTeamColor(pFlag->m_iTeam));

	return szReturnString;
}

// orig 0x1893e0: one line per flag, "Your flag is at its base" and the like
char* ReportFlagStatus(CBasePlayer* pPlayer)
{
	static char szReturnString[1024];
	CBaseFlag* pFlag = NULL;

	while ((pFlag = (CBaseFlag*)UTIL_FindEntityByClassname(pFlag, "flag")) != NULL)
	{
		char* pszStatus = GetFlagStatusString(pFlag, pPlayer);
		sprintf(szReturnString, "%s%s", GetFlagNameString(pFlag, pPlayer), pszStatus);
		UTIL_SayText(szReturnString, pPlayer);
	}

	return szReturnString;
}

// orig 0x189460
const char* GetFlagTeamColor(int iTeam)
{
	const char* pszColor = "";

	switch (iTeam)
	{
	case 1:
		pszColor = "Red";
		break;
	case 2:
		pszColor = "Blue";
		break;
	case 3:
		pszColor = "Green";
		break;
	case 4:
		pszColor = "Yellow";
		break;
	case 5:
		pszColor = "Purple";
		break;
	case 6:
		pszColor = "Orange";
		break;
	default:
		// VERIFY: the original sprintf()s "ERROR: Team %d does not exist" into the "" literal it
		// returns, a write to read-only memory. Left out; returns "".
		break;
	}

	return pszColor;
}

// orig 0x189500
Vector GetFlagBaseColor(int iTeam)
{
	switch (iTeam)
	{
	case 1:
		return Vector(255, 0, 0);
	case 2:
		return Vector(0, 0, 255);
	case 3:
		return Vector(0, 255, 0);
	case 4:
		return Vector(255, 255, 0);
	case 5:
		return Vector(255, 0, 255);
	case 6:
		return Vector(255, 128, 0);
	default:
		return g_vecZero;
	}
}
