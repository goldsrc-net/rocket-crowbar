/***
*
*	Rocket Crowbar 1.9: pregame.cpp, the observer (pregame.h).
*
****/

#include <string.h>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "driveent.h"
#include "pregame.h"

// The help and mode messages share one look: light blue, centred.
static void PREGAME_SetMsgColor(hudtextparms_t& params)
{
	params.r1 = 0x80;
	params.g1 = 0x80;
	params.b1 = 0xff;
	params.a1 = 200;
	params.r2 = 0x80;
	params.g2 = 0x80;
	params.b2 = 0xff;
	params.a2 = 200;
}

// orig 0x1f7490. From UpdateClientData.
void PrintRocketCrowbarMsg(CBaseEntity* pPlayer)
{
	char szMsg[128];
	memset(szMsg, 0, sizeof(szMsg));
	sprintf(szMsg, "Rocket Crowbar (version: %s)", "19");

	hudtextparms_t params;
	params.x = 0;
	params.y = 0;
	params.effect = 0;
	PREGAME_SetMsgColor(params);
	params.fadeinTime = 1.0;
	params.fadeoutTime = 2.0;
	params.holdTime = 9.0;
	params.fxTime = 0;
	params.channel = 3;
	UTIL_HudMessage(pPlayer, params, szMsg);
}

// orig 0x1f75b0. The help text, at the bottom of the screen.
void PrintObserverMessage(CBaseEntity* pPlayer)
{
	hudtextparms_t params;
	params.x = -1;
	params.y = 1.0;
	params.effect = 0;
	PREGAME_SetMsgColor(params);
	params.fadeinTime = 0;
	params.fadeoutTime = 0;
	params.holdTime = 1.0;
	params.fxTime = 0;
	params.channel = 2;
	UTIL_HudMessage(pPlayer, params,
		"In SPECTATOR mode, press JUMP to play\n"
		"Press PRIMARY FIRE to cycle observer modes\n"
		"Press SECONDARY FIRE to cycle through players when casing,\n"
		"It will move you to a random postion when not\n"
		"Press RELOAD to change target drawing mode");
}

// orig 0x1f7660. The current mode and target, at the top; also sets the client's move speeds
// (full speed when roaming, none when chasing).
void PrintModeMessage(CBasePlayer* pPlayer)
{
	const char* szName = NULL;
	CBaseEntity* pTarget = pPlayer->m_pObserverTarget;
	if (pTarget)
	{
		if (pTarget->IsPlayer())
			szName = STRING(pTarget->pev->netname);
		else if (pTarget->GetControllerName())
			szName = pTarget->GetControllerName();
		else
			szName = STRING(pTarget->pev->classname);
	}

	char szMsg[384];
	sprintf(szMsg, "Follow mode, no target");
	if (pPlayer->m_iObserverMode == 0)
		sprintf(szMsg, "Free mode");
	else if (szName)
	{
		if (pPlayer->m_iObserverMode == 1)
			sprintf(szMsg, "Free look, following: \"%s\"", szName);
		else if (pPlayer->m_iObserverMode == 2 && szName)
			sprintf(szMsg, "Target look, following: \"%s\"", szName);
	}

	if (pPlayer->m_iObserverMode == 0)
		CLIENT_COMMAND(pPlayer->edict(), "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");
	else
		CLIENT_COMMAND(pPlayer->edict(), "cl_forwardspeed 0\ncl_backspeed 0\ncl_sidespeed 0\n");

	hudtextparms_t params;
	params.x = -1;
	params.y = 0;
	params.effect = 0;
	PREGAME_SetMsgColor(params);
	params.fadeinTime = 0;
	params.fadeoutTime = 0;
	params.holdTime = 1.0;
	params.fxTime = 0;
	params.channel = 4;
	UTIL_HudMessage(pPlayer, params, szMsg);
}

// orig 0x1f78b0. VERIFY: as the original, it skips RANDOM_LONG(0, n - 1) spots from the start,
// so the first deathmatch spot comes up for both 0 and 1 and the last one never.
CBaseEntity* GetRandomObserverSpawn()
{
	CBaseEntity* pSpot = NULL;
	int iCount = 0;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch")) != NULL)
		iCount++;

	int iPick = RANDOM_LONG(0, iCount - 1);
	pSpot = NULL;
	for (int i = 0; i < iPick; i++)
		pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch");

	if (!pSpot)
	{
		pSpot = UTIL_FindEntityByClassname(NULL, "info_player_deathmatch");
		if (!pSpot)
			pSpot = UTIL_FindEntityByClassname(NULL, "info_player_start");
	}
	return pSpot;
}

// orig 0x1f7950
void MoveObserverToRandomSpot(CBaseEntity* pPlayer)
{
	CBaseEntity* pSpot = GetRandomObserverSpawn();
	if (!pSpot)
		return;

	pPlayer->pev->origin = pSpot->pev->origin;
	UTIL_SetOrigin(pPlayer->pev, pSpot->pev->origin);
	pPlayer->pev->v_angle = g_vecZero;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->angles = pSpot->pev->angles;
	pPlayer->pev->punchangle = g_vecZero;
	pPlayer->pev->fixangle = 1;
}

// orig 0x1f7a10. From CBasePlayer::Spawn and the console commands.
void InitObserver(CBasePlayer* pPlayer)
{
	pPlayer->pev->health = 1;
	pPlayer->pev->deadflag = DEAD_DEAD;
	pPlayer->pev->takedamage = DAMAGE_NO;
	pPlayer->pev->movetype = MOVETYPE_NOCLIP;
	pPlayer->pev->solid = SOLID_NOT;
	SET_MODEL(pPlayer->edict(), "models/player.mdl");
	pPlayer->pev->rendermode = kRenderTransColor;
	pPlayer->pev->renderamt = 0;
	pPlayer->pev->flags = FL_CLIENT | FL_FLY | FL_NOTARGET;
	pPlayer->pev->classname = MAKE_STRING("spectator");
	UTIL_SetSize(pPlayer->pev, g_vecZero, g_vecZero);
	pPlayer->pev->weaponmodel = 0;
	pPlayer->m_afPhysicsFlags = 0;
	// The original clears pev->weapons, its weapon bits; this SDK keeps them in m_WeaponBits.
	pPlayer->m_WeaponBits = 0ULL;
	pPlayer->pev->weapons = 0;
	pPlayer->m_iHideHUD = HIDEHUD_ALL;
	CLIENT_COMMAND(pPlayer->edict(), "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");

	pPlayer->m_flObserverMsgTime = -1;
	pPlayer->m_iObserverMode = 0;
	pPlayer->m_pObserverTarget = NULL;
	pPlayer->m_iObserverDrawMode = 0;
	pPlayer->m_flObserverChaseDist = 256;
	pPlayer->pev->animtime = gpGlobals->time; // ObserverChaseCamThink's last-think time
	MoveObserverToRandomSpot(pPlayer);

	pPlayer->m_blWasObserver = true;
	pPlayer->m_blIsObserver = true;
	pPlayer->m_blObserverRespawn = true;
}

// orig 0x1f7b80. Steps through the players, then the client_in_cam cameras, then the
// deathballs, and wraps. pPlayer is not used (the observer can pick itself).
CBaseEntity* GetNextObserverEnt(CBaseEntity* pCurrent, CBasePlayer* pPlayer)
{
	if (!pCurrent || !strcmp(STRING(pCurrent->pev->classname), "player"))
		pCurrent = UTIL_FindEntityByClassname(pCurrent, "player");

	if (!pCurrent || !strcmp(STRING(pCurrent->pev->classname), "client_in_cam"))
		pCurrent = UTIL_FindEntityByClassname(pCurrent, "client_in_cam");

	if (!pCurrent || !strcmp(STRING(pCurrent->pev->classname), "deathball"))
		pCurrent = UTIL_FindEntityByClassname(pCurrent, "deathball");

	if (!pCurrent)
	{
		pCurrent = UTIL_FindEntityByClassname(NULL, "player");
		if (!pCurrent)
		{
			pCurrent = UTIL_FindEntityByClassname(NULL, "client_in_cam");
			if (!pCurrent)
				pCurrent = UTIL_FindEntityByClassname(NULL, "deathball");
		}
	}
	return pCurrent;
}

// orig 0x1f7ca0. Ghidra lost most of it; read from the disassembly. Puts the observer behind
// its target (or the monster the target drives) at m_flObserverChaseDist, which forward and
// back move between per-case limits at 200 units a second.
void ObserverChaseCamThink(CBasePlayer* pPlayer)
{
	CBaseEntity* pTarget = pPlayer->m_pObserverTarget;
	if (!pTarget || !pTarget->pev || !pTarget->edict() || !pTarget->edict()->pvPrivateData || pTarget->edict()->free || FNullEnt(pTarget->pev))
		pPlayer->m_pObserverTarget = GetNextObserverEnt(NULL, pPlayer);

	if (pPlayer->m_afButtonPressed & IN_ATTACK2)
		pPlayer->m_pObserverTarget = GetNextObserverEnt(pPlayer->m_pObserverTarget, pPlayer);

	if (!pPlayer->m_pObserverTarget)
		return;

	pTarget = pPlayer->m_pObserverTarget;
	if (!pTarget->pev || !pTarget->edict() || !UTIL_IsValidEntity(pTarget->edict()))
		pPlayer->m_pObserverTarget = NULL;

	if (!pPlayer->m_pObserverTarget)
		return;
	pTarget = pPlayer->m_pObserverTarget;

	float flDelta = gpGlobals->time - pPlayer->pev->animtime;
	pPlayer->pev->animtime = gpGlobals->time;
	pPlayer->pev->movetype = MOVETYPE_NOCLIP;
	pPlayer->pev->velocity = pTarget->pev->velocity;

	if (pPlayer->m_iObserverMode == 2)
	{
		pPlayer->pev->angles = pTarget->pev->v_angle;
		pPlayer->pev->v_angle = pPlayer->pev->angles;
		pPlayer->pev->fixangle = 1;
	}

	// The drawing mode goes to the client in iuser3 (4 = draw), euser2 (the entity),
	// fuser1-3 and vuser1 (a colour).
	if (pPlayer->m_iObserverDrawMode == 0)
	{
		pPlayer->pev->iuser3 = 0;
		pPlayer->pev->euser2 = NULL;
	}
	else
	{
		pPlayer->pev->euser2 = pTarget->edict();
		if (pTarget->IsPlayer())
		{
			CBasePlayer* pTargetPlayer = (CBasePlayer*)pTarget;
			if (pTargetPlayer->m_bInDriveEnt && pTargetPlayer->m_pCurDriveEnt && pTargetPlayer->m_pCurDriveEnt->edict() && UTIL_IsValidEntity(pTargetPlayer->m_pCurDriveEnt->edict()))
				pPlayer->pev->euser2 = pTargetPlayer->m_pCurDriveEnt->edict();
		}

		pPlayer->pev->iuser3 = 4;
		pPlayer->pev->fuser1 = 0;
		pPlayer->pev->fuser2 = 0;
		pPlayer->pev->fuser3 = 0;
		pPlayer->pev->vuser1 = Vector(255, 64, 64);
		pPlayer->pev->vuser1 = g_vecZero; // overwritten at once, as in the original

		switch (pPlayer->m_iObserverDrawMode)
		{
		case 1:
			pPlayer->pev->fuser1 = 2;
			pPlayer->pev->fuser2 = 200;
			pPlayer->pev->fuser3 = 0;
			pPlayer->pev->vuser1 = g_vecZero;
			break;
		case 2:
			pPlayer->pev->fuser1 = 2;
			pPlayer->pev->fuser2 = 140;
			pPlayer->pev->fuser3 = 0;
			pPlayer->pev->vuser1 = g_vecZero;
			break;
		case 3:
			pPlayer->pev->fuser1 = 2;
			pPlayer->pev->fuser2 = 100;
			pPlayer->pev->fuser3 = 0;
			pPlayer->pev->vuser1 = g_vecZero;
			break;
		case 4:
			pPlayer->pev->fuser1 = 2;
			pPlayer->pev->fuser2 = 70;
			pPlayer->pev->fuser3 = 0;
			pPlayer->pev->vuser1 = g_vecZero;
			break;
		case 5:
			pPlayer->pev->fuser1 = 0;
			pPlayer->pev->fuser3 = 19;
			pPlayer->pev->fuser2 = 64;
			pPlayer->pev->vuser1 = Vector(255, 0, 0);
			break;
		case 6:
			pPlayer->pev->fuser1 = 0;
			pPlayer->pev->fuser3 = 19;
			pPlayer->pev->fuser2 = 64;
			pPlayer->pev->vuser1 = Vector(0, 255, 0);
			break;
		case 7:
			pPlayer->pev->fuser1 = 0;
			pPlayer->pev->fuser3 = 19;
			pPlayer->pev->fuser2 = 64;
			pPlayer->pev->vuser1 = Vector(0, 0, 255);
			break;
		case 8:
			pPlayer->pev->fuser1 = 2;
			pPlayer->pev->fuser2 = 200;
			pPlayer->pev->fuser3 = 16;
			pPlayer->pev->vuser1 = g_vecZero;
			break;
		default:
			pPlayer->pev->iuser3 = 0;
			pPlayer->pev->euser2 = NULL;
			pPlayer->m_iObserverDrawMode = 0;
			break;
		}
	}

	UTIL_MakeVectors(pPlayer->pev->v_angle);

	Vector vecOffset = Vector(0, 0, 28);
	Vector vecStart = pTarget->pev->origin + vecOffset;

	// Chase distance limits: in toward 256 / out to 734 around a driven monster, 32 / 512
	// around a player on foot, 128 / 734 around anything else.
	float flMinDist = 0, flMaxDist = 0;
	bool bIn = false, bOut = false;
	if (pTarget->IsPlayer())
	{
		CBasePlayer* pTargetPlayer = (CBasePlayer*)pTarget;
		if (pTargetPlayer->m_bInDriveEnt && pTargetPlayer->m_pCurDriveEnt && pTargetPlayer->m_pCurDriveEnt->edict() && UTIL_IsValidEntity(pTargetPlayer->m_pCurDriveEnt->edict()))
		{
			vecStart = pTargetPlayer->m_pCurDriveEnt->Center();
			pPlayer->pev->velocity = pTargetPlayer->m_pCurDriveEnt->pev->velocity;
			flMinDist = 256;
			flMaxDist = 734;
		}
		else
		{
			flMinDist = 32;
			flMaxDist = 512;
		}
	}
	else
	{
		flMinDist = 128;
		flMaxDist = 734;
	}

	if (pPlayer->pev->button & IN_FORWARD)
		bIn = true;
	else if (pPlayer->pev->button & IN_BACK)
		bOut = true;

	if (bIn)
	{
		float flDist = pPlayer->m_flObserverChaseDist - 200 * flDelta;
		pPlayer->m_flObserverChaseDist = (flMinDist > flDist) ? flMinDist : pPlayer->m_flObserverChaseDist - 200 * flDelta;
	}
	else if (bOut)
	{
		float flDist = pPlayer->m_flObserverChaseDist + 200 * flDelta;
		pPlayer->m_flObserverChaseDist = (flMaxDist < flDist) ? flMaxDist : pPlayer->m_flObserverChaseDist + 200 * flDelta;
	}

	Vector vecEnd = vecStart - gpGlobals->v_forward * pPlayer->m_flObserverChaseDist;

	TraceResult tr;
	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, pTarget->edict(), &tr);

	Vector vecPos;
	if (tr.flFraction < 1.0)
		vecPos = tr.vecEndPos + tr.vecPlaneNormal * 8;
	else
		vecPos = tr.vecEndPos;

	if (tr.fAllSolid || tr.fStartSolid || UTIL_PointContents(vecPos) != CONTENTS_EMPTY)
		vecPos = vecStart;

	pPlayer->pev->origin = vecPos;
	UTIL_SetOrigin(pPlayer->pev, vecPos);
	pPlayer->pev->button &= ~(IN_DUCK | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);
}

// orig 0x1f8480. From CBasePlayer::PreThink while m_blIsObserver is set.
void ObserverFrame(CBasePlayer* pPlayer)
{
	if (pPlayer->m_afButtonPressed & IN_JUMP)
	{
		pPlayer->m_blIsObserver = false;
		pPlayer->m_blObserverRespawn = true;
		pPlayer->m_blWasObserver = true;
		return;
	}

	pPlayer->pev->view_ofs = g_vecZero;
	pPlayer->m_afButtonPressed &= ~IN_DUCK;
	pPlayer->m_afButtonReleased &= ~IN_DUCK;
	pPlayer->pev->button &= ~IN_DUCK;

	if (pPlayer->m_afButtonPressed & IN_ATTACK)
	{
		if (pPlayer->m_iObserverMode == 0)
		{
			pPlayer->m_pObserverTarget = GetNextObserverEnt(NULL, pPlayer);
			pPlayer->m_iObserverMode = 1;
			pPlayer->m_flObserverChaseDist = 256;
			CLIENT_COMMAND(pPlayer->edict(), "cl_forwardspeed 0\ncl_backspeed 0\ncl_sidespeed 0\n");
		}
		else if (pPlayer->m_iObserverMode == 1)
		{
			pPlayer->m_pObserverTarget = GetNextObserverEnt(NULL, pPlayer);
			pPlayer->m_iObserverMode = 2;
			CLIENT_COMMAND(pPlayer->edict(), "cl_forwardspeed 0\ncl_backspeed 0\ncl_sidespeed 0\n");
		}
		else
		{
			pPlayer->m_pObserverTarget = NULL;
			pPlayer->m_iObserverMode = 0;
			pPlayer->pev->velocity = g_vecZero;
			CLIENT_COMMAND(pPlayer->edict(), "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");
			pPlayer->pev->movetype = MOVETYPE_NOCLIP;
			pPlayer->pev->iuser3 = 0;
			pPlayer->pev->euser2 = NULL;
		}
	}

	if (pPlayer->m_afButtonPressed & IN_RELOAD)
	{
		pPlayer->m_iObserverDrawMode++;
		if (pPlayer->m_iObserverDrawMode > 8)
			pPlayer->m_iObserverDrawMode = 0;
	}

	if (pPlayer->m_iObserverMode == 1 || pPlayer->m_iObserverMode == 2)
		ObserverChaseCamThink(pPlayer);
	else
	{
		pPlayer->pev->movetype = MOVETYPE_NOCLIP;
		if (pPlayer->m_afButtonPressed & IN_ATTACK2)
			MoveObserverToRandomSpot(pPlayer);
		pPlayer->pev->iuser3 = 0;
		pPlayer->pev->euser2 = NULL;
	}

	if (pPlayer->m_flObserverMsgTime < gpGlobals->time)
	{
		PrintObserverMessage(pPlayer);
		PrintModeMessage(pPlayer);
		pPlayer->m_flObserverMsgTime = gpGlobals->time + 1.0;
	}
}
