/***
*
*	Rocket Crowbar 1.9: drivebullsquid.cpp. The drivable bullsquid (drive_bullsquid), the pickup
*	a player uses to get in (pickup_drive_bullsquid) and the spawner that makes the pickups
*	(drive_bullsquid_spawner). Reconstructed from rc_i386.so; addresses are file addresses
*	(reference/funcfile.tsv).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"
#include "effects.h"
#include "decals.h"
#include "gamerules.h"
#include "client.h"
#include "UserMessages.h"
#include "drivebullsquid.h"

// Preff.cpp (prefs-votes package)
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
extern bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault);

// drivechum.cpp (chum-icky-hc package): turns what the chumtoadifier beam hits into a chumtoad
extern void DCTChumtoadifyEnt(edict_t* pentTarget, edict_t* pentOwner);

const char* g_sDBSMeleeStartSounds[] =
	{
		"bullchicken/bc_attackgrowl.wav",
		"bullchicken/bc_attackgrowl2.wav",
		"bullchicken/bc_attackgrowl3.wav",
};

const char* g_sDBSMeleeMissSounds[] =
	{
		"zombie/claw_miss1.wav",
		"zombie/claw_miss2.wav",
};

const char* g_sDBSBiteSounds[] =
	{
		"bullchicken/bc_bite2.wav",
		"bullchicken/bc_bite3.wav",
};

const char* g_sDBSDieSounds[] =
	{
		"bullchicken/bc_die1.wav",
		"bullchicken/bc_die2.wav",
		"bullchicken/bc_die3.wav",
};

const char* g_sDBSPainSounds[] =
	{
		"bullchicken/bc_pain1.wav",
		"bullchicken/bc_pain2.wav",
		"bullchicken/bc_pain3.wav",
		"bullchicken/bc_pain4.wav",
};

const char* g_sDBSChumtoadifierSounds[] =
	{
		"bullchicken/bc_attack2.wav",
		"bullchicken/bc_attack3.wav",
};

const char* g_sDBSIdleSounds[] =
	{
		"bullchicken/bc_idle1.wav",
		"bullchicken/bc_idle2.wav",
		"bullchicken/bc_idle3.wav",
		"bullchicken/bc_idle4.wav",
		"bullchicken/bc_idle5.wav",
};

// util.cpp
unsigned short FixedUnsigned16(float value, float scale);

// orig 0x141bc0
LINK_ENTITY_TO_CLASS(drive_bullsquid, CDriveBullsquid);

// orig 0x141bf0
void CDriveBullsquid::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_bullsquid");
	SET_MODEL(ENT(pev), "models/bullsquid.mdl");
	UTIL_SetSize(pev, Vector(-20, -20, 0), Vector(20, 20, 64));
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->effects = 0;
	pev->friction = 1;
	m_HackedGunPos = Vector(0, 24, 48);
	pev->health = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidHealth", 250);
	pev->max_health = pev->health;
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	pev->sequence = LookupSequence("idle");
	ResetSequenceInfo();
	InitBoneControllers();
	pev->frame = 0;

	m_bInBite = false;
	m_fNextBite = gpGlobals->time;
	m_bInWhip = false;
	m_fNextWhip = gpGlobals->time;
	m_bInChumtoadifier = false;
	m_fNextChumtoadifier = gpGlobals->time;
	pev->framerate = 1;
	SetEyePosition();

	m_fIdleSoundTime = RANDOM_LONG(6, 8) + gpGlobals->time;
	m_fNextPainSound = gpGlobals->time;
	m_bCanIdle = true;
	m_bMoved = false;
	m_bCanMove = true;
	m_vecSudoVel = g_vecZero;
	m_fNextEHopCheck = gpGlobals->time;
	m_fNextDoorCheck = gpGlobals->time;

	SetThink(&CDriveBullsquid::DBSThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x141ec0
void CDriveBullsquid::Precache()
{
	unsigned int i;

	PRECACHE_MODEL("models/bullsquid.mdl");
	PRECACHE_MODEL("sprites/xbeam5.spr");
	PRECACHE_MODEL("sprites/xbeam2.spr");
	PRECACHE_MODEL("sprites/lgtning.spr");
	for (i = 0; i < ARRAYSIZE(g_sDBSMeleeStartSounds); i++)
		PRECACHE_SOUND(g_sDBSMeleeStartSounds[i]);
	for (i = 0; i < ARRAYSIZE(g_sDBSMeleeMissSounds); i++)
		PRECACHE_SOUND(g_sDBSMeleeMissSounds[i]);
	for (i = 0; i < ARRAYSIZE(g_sDBSDieSounds); i++)
		PRECACHE_SOUND(g_sDBSDieSounds[i]);
	for (i = 0; i < ARRAYSIZE(g_sDBSPainSounds); i++)
		PRECACHE_SOUND(g_sDBSPainSounds[i]);
	for (i = 0; i < ARRAYSIZE(g_sDBSIdleSounds); i++)
		PRECACHE_SOUND(g_sDBSIdleSounds[i]);
	for (i = 0; i < ARRAYSIZE(g_sDBSChumtoadifierSounds); i++)
		PRECACHE_SOUND(g_sDBSChumtoadifierSounds[i]);
	for (i = 0; i < ARRAYSIZE(g_sDBSBiteSounds); i++)
		PRECACHE_SOUND(g_sDBSBiteSounds[i]);
	PRECACHE_SOUND("debris/beamstart9.wav");
	PRECACHE_SOUND("weapons/electro5.wav");
}

// orig 0x142030
const char* CDriveBullsquid::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;

	return STRING(m_pOwner->pev->netname);
}

// orig 0x1420a0
void CDriveBullsquid::DBSThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;
	if (m_fFrameTime < 0.001)
		m_fFrameTime = 0.001;

	if (m_pOwner && !FNullEnt(m_pOwner->pev) && m_pOwner->pev->pContainingEntity && m_pOwner->pev->pContainingEntity->pvPrivateData && !m_pOwner->pev->pContainingEntity->free && UTIL_PlayerByIndex(ENTINDEX(m_pOwner->pev->pContainingEntity)))
	{
		if (pev->waterlevel > 1)
			pev->flags &= ~FL_ONGROUND;

		DispatchAnimEvents(m_fFrameTime);

		// keep the driver hidden
		m_pOwner->pev->rendermode = kRenderTransColor;
		m_pOwner->pev->renderamt = 0;
		m_pOwner->pev->effects |= EF_NODRAW;

		UpdateOwnerHUD();

		if ((pev->flags & FL_FROZEN) != 0)
		{
			m_vecSudoVel = g_vecZero;
			m_fLastMoveTime = gpGlobals->time;
			return;
		}

		m_bCanIdle = true;
		m_bMoved = false;
		m_bCanMove = true;
		m_bCanTurnAnim = true;
		m_fYawSpeedScale = 1;

		DoBite();
		DoWhip();
		DoChumtoadifier();
		DoMovement();
		DoYaw();

		pev->fuser4 = m_vecSudoVel.Length();

		if (m_bCanIdle)
		{
			if (pev->sequence != LookupSequence("idle") && m_fSequenceFinished)
			{
				pev->sequence = LookupSequence("idle");
				ResetSequenceInfo();
				pev->frame = 0;
			}

			if (!(m_fIdleSoundTime < gpGlobals->time))
				return;

			int iPitch = RANDOM_LONG(95, 105);
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_sDBSIdleSounds[RANDOM_LONG(0, 4)], 1.0, 1.2, 0, iPitch);
		}

		m_fIdleSoundTime = RANDOM_LONG(6, 8) + gpGlobals->time;
		return;
	}

	// the driver is gone
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("bullsquid");
	g_pevLastInflictor = pev;
	Killed(pev, GIB_NORMAL);
}

// orig 0x1423f0: primary attack
void CDriveBullsquid::DoBite()
{
	if (m_bInWhip || m_bInChumtoadifier)
		return;

	if (!m_bInBite)
	{
		if (m_fNextBite <= gpGlobals->time && (m_pOwner->pev->button & IN_ATTACK) != 0)
		{
			m_bInBite = true;
			m_bCanMove = false;
			m_bCanIdle = false;
			m_bCanTurnAnim = false;
			m_fYawSpeedScale = 0.8;
			pev->sequence = LookupSequence("bite");
			ResetSequenceInfo();
			pev->frame = 0;

			int iPitch = RANDOM_LONG(95, 105);
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_sDBSMeleeStartSounds[RANDOM_LONG(0, 2)], 1.0, 1.0, 0, iPitch);
		}
	}
	else if (!m_fSequenceFinished)
	{
		m_bCanMove = false;
		m_bCanIdle = false;
		m_bCanTurnAnim = false;
		m_fYawSpeedScale = 0.8;
	}
	else
	{
		m_bInBite = false;
		m_fNextBite = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidBiteRefire", 0.25) + gpGlobals->time;
	}
}

// orig 0x142570: secondary attack
void CDriveBullsquid::DoWhip()
{
	if (m_bInBite || m_bInChumtoadifier)
		return;

	if (!m_bInWhip)
	{
		if (m_fNextWhip <= gpGlobals->time && (m_pOwner->pev->button & IN_ATTACK2) != 0)
		{
			m_bInWhip = true;
			m_bCanMove = false;
			m_bCanIdle = false;
			m_bCanTurnAnim = false;
			m_fYawSpeedScale = 0.8;
			pev->sequence = LookupSequence("whip");
			ResetSequenceInfo();
			pev->frame = 0;
		}
	}
	else if (!m_fSequenceFinished)
	{
		m_bCanMove = false;
		m_bCanIdle = false;
		m_bCanTurnAnim = false;
		m_fYawSpeedScale = 0.8;
	}
	else
	{
		m_bInWhip = false;
		m_fNextWhip = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidWhipRefire", 0.25) + gpGlobals->time;
	}
}

// orig 0x142690: jump fires the chumtoadifier
void CDriveBullsquid::DoChumtoadifier()
{
	if (m_bInWhip || m_bInBite)
		return;

	if (!m_bInChumtoadifier)
	{
		if (m_fNextChumtoadifier <= gpGlobals->time && (m_pOwner->pev->button & IN_JUMP) != 0)
		{
			m_bInChumtoadifier = true;
			m_bCanMove = false;
			m_bCanIdle = false;
			m_bCanTurnAnim = false;
			m_fYawSpeedScale = 0.8;
			pev->sequence = LookupSequence("range");
			ResetSequenceInfo();
			pev->frame = 0;
		}
	}
	else if (!m_fSequenceFinished)
	{
		m_bCanMove = false;
		m_bCanIdle = false;
		m_bCanTurnAnim = false;
		m_fYawSpeedScale = 0.8;
	}
	else
	{
		m_bInChumtoadifier = false;
		m_fNextChumtoadifier = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidChumtoadifierRefire", 5) + gpGlobals->time;
	}
}

// orig 0x1427b0: walking into a door opens it
void CDriveBullsquid::CheckDoor()
{
	if (gpGlobals->time < m_fNextDoorCheck)
		return;
	if ((pev->flags & FL_ONGROUND) == 0)
		return;
	if (!m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecSrc = pev->origin + Vector(0, 0, 32);
	Vector vecEnd = vecSrc + gpGlobals->v_forward * (m_fFrameTime * pev->framerate * m_flGroundSpeed);

	TraceResult tr;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

	if (tr.pHit && (FClassnameIs(tr.pHit, "func_door") || FClassnameIs(tr.pHit, "func_door_rotating")))
	{
		CBaseEntity* pDoor = CBaseEntity::Instance(tr.pHit);
		if (pDoor)
			pDoor->Touch(this);
	}

	m_fNextDoorCheck = gpGlobals->time + 0.3;
}

// orig 0x142a10: hop a small ledge the walk could not take. Read from the disassembly (the
// decompile is a stub). Unlike the other drive monsters' it never pushes m_fNextEHopCheck on.
void CDriveBullsquid::CheckEHop()
{
	if (m_fNextEHopCheck > gpGlobals->time)
		return;
	if (pev->velocity.z > 16)
		return;
	if ((pev->flags & FL_ONGROUND) == 0)
		return;
	if (!m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecFront = pev->origin + Vector(0, 0, 4) + gpGlobals->v_forward * 48;
	Vector vecDown = vecFront - Vector(0, 0, 28);
	Vector vecSrc = pev->origin + Vector(0, 0, 4);

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecFront, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction < 1.0 || tr.fAllSolid || tr.fStartSolid)
		return;

	UTIL_TraceLine(vecFront, vecDown, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction < 1.0 || tr.fAllSolid || tr.fStartSolid)
		return;

	pev->velocity.z += 50;
	pev->velocity = pev->velocity + gpGlobals->v_forward * 100;
	pev->flags &= ~FL_ONGROUND;
	UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
}

// orig 0x142d10: swimming
void CDriveBullsquid::RunWaterPhysics()
{
	float flFriction = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidWaterFriction", 8);
	m_fYawSpeedScale = 0.7;

	float flSpeed = pev->velocity.Length();
	if (flSpeed > 0.1)
	{
		float flControl = (flSpeed < 6) ? 6 : flSpeed;
		float flNewSpeed = flSpeed - flControl * flFriction * m_fFrameTime;
		if (flNewSpeed < 0)
			flNewSpeed = 0;
		flNewSpeed /= flSpeed;
		pev->velocity = pev->velocity * flNewSpeed;
	}

	if (pev->waterlevel > 1)
	{
		float flGravity = 1.1 * CVAR_GET_FLOAT("sv_gravity");
		if (pev->gravity > 0)
			flGravity *= pev->gravity;
		pev->velocity.z = flGravity * m_fFrameTime + pev->velocity.z; // VERIFY: adds, so it floats up
	}

	m_bCanIdle = false;

	if (!m_bCanMove)
		return;

	if (pev->sequence != LookupSequence("run"))
	{
		pev->sequence = LookupSequence("run");
		ResetSequenceInfo();
	}

	float flRate;
	if (pev->velocity.Length2D() > 0.1)
		flRate = pev->velocity.Length2D() / 30;
	else
		flRate = 0.6;

	if (flRate < 0.6)
		pev->framerate = 0.6;
	else if (flRate > 1.0)
		pev->framerate = 1.0;
	else
		pev->framerate = flRate;

	if ((m_pOwner->pev->button & IN_FORWARD) != 0)
	{
		UTIL_MakeAimVectors(pev->angles);
		float flWishSpeed = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidWaterSpeed", 180);
		float flAccel = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidWaterAccel", 170);
		Vector vecForward = gpGlobals->v_forward;

		float flAdd = flWishSpeed - DotProduct(pev->velocity, vecForward);
		if (flAdd > 0)
		{
			float flAccelSpeed = flAccel * m_fFrameTime * flWishSpeed;
			if (flAdd < flAccelSpeed)
				flAccelSpeed = flAdd;
			pev->velocity = pev->velocity + vecForward * flAccelSpeed;
		}
	}
}

// orig 0x143080: the driver holds forward; walk along the facing
void CDriveBullsquid::DoMovement()
{
	if ((pev->flags & FL_ONGROUND) != 0)
	{
		if (!m_bCanMove)
		{
			m_vecSudoVel = g_vecZero;
			m_fLastMoveTime = gpGlobals->time;
			return;
		}

		if ((m_pOwner->pev->button & IN_FORWARD) == 0)
		{
			if (pev->sequence == LookupSequence("run") || pev->sequence == LookupSequence("walk"))
				m_fSequenceFinished = true;
			m_vecSudoVel = g_vecZero;
			m_fLastMoveTime = gpGlobals->time;
			return;
		}

		bool bWalk = (m_pOwner->pev->button & (IN_RUN | IN_USE)) != 0;
		float flOldFrame = pev->frame;
		int iOldSequence = pev->sequence;
		float flOldFramerate = pev->framerate;

		// VERIFY: literal, true when the old sequence is neither run nor walk (the panther's
		// version of this test is the other way round)
		bool bRestoreAnim = false;
		if (pev->sequence != LookupSequence("run"))
			bRestoreAnim = pev->sequence != LookupSequence("walk");

		const char* pszSeq = bWalk ? "walk" : "run";
		if (pev->sequence != LookupSequence(pszSeq))
		{
			pev->sequence = LookupSequence(pszSeq);
			ResetSequenceInfo();
			pev->frame = 0;
		}
		pev->framerate = 1;

		CheckDoor();
		m_bCanIdle = false;
		m_bMoved = true;

		UTIL_MakeVectors(pev->angles);
		Vector vecGoal = Center() + gpGlobals->v_forward * 512;
		float flMoved = 0;
		Vector vecOldOrigin = pev->origin;

		// how far the walk can go this think, in steps of at most 16
		float flDist = m_flGroundSpeed * pev->framerate * m_fFrameTime;
		while (flDist > 0.001)
		{
			float flStep = flDist;
			if (flStep > 16)
				flStep = 16;

			Vector vecStart = pev->origin;
			for (; !WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY) && flStep > 2; flStep -= 2)
			{
			}
			UTIL_SetOrigin(pev, vecStart);

			if (!WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY))
			{
				if (flMoved < 0.01)
				{
					if (bRestoreAnim)
					{
						pev->sequence = iOldSequence;
						ResetSequenceInfo();
						pev->frame = flOldFrame;
					}
					else
					{
						m_fSequenceFinished = true;
						pev->framerate = flOldFramerate;
					}
					m_bCanIdle = true;
					m_bMoved = false;
					m_vecSudoVel = g_vecZero;
					UTIL_SetOrigin(pev, vecOldOrigin);
					if (!bWalk)
						CheckEHop();
					return;
				}
				break;
			}

			flDist -= flStep;
			flMoved += flStep;
		}

		// now move that far for real
		UTIL_SetOrigin(pev, vecOldOrigin);
		Vector vecPrev = pev->origin;
		while (flMoved > 0.001)
		{
			float flStep = flMoved;
			if (flStep > 16)
				flStep = 16;
			UTIL_MoveToOrigin(ENT(pev), vecGoal, flStep, MOVE_NORMAL);
			flMoved -= flStep;
		}

		Vector vecDelta = pev->origin - vecPrev;
		if (vecDelta.Length() > 0.01 && m_fFrameTime > 0.001)
			m_vecSudoVel = vecDelta / m_fFrameTime;
		else
			m_vecSudoVel = g_vecZero;

		m_fLastMoveTime = gpGlobals->time;
		m_fYawSpeedScale = 0.8;
		return;
	}

	if (pev->waterlevel != 0)
	{
		RunWaterPhysics();
	}
	else if (m_bCanIdle)
	{
		float flSpeed = pev->velocity.Length2D();
		if (flSpeed > 10 && flSpeed < 70)
		{
			if (pev->sequence != LookupSequence("walk"))
			{
				pev->sequence = LookupSequence("walk");
				ResetSequenceInfo();
			}
			m_bCanIdle = false;
		}
		else if (flSpeed >= 70)
		{
			if (pev->sequence != LookupSequence("run"))
			{
				pev->sequence = LookupSequence("run");
				ResetSequenceInfo();
			}
			m_bCanIdle = false;
		}
	}

	m_vecSudoVel = g_vecZero;
	m_fLastMoveTime = gpGlobals->time;
}

// orig 0x143700: turn toward ideal_yaw
void CDriveBullsquid::DoYaw()
{
	float flSpeed = m_fFrameTime * 1900 * m_fYawSpeedScale;
	bool bTurnAnim = false;
	float flDist = UTIL_AngleDistance(pev->ideal_yaw, pev->angles.y);

	if (fabs(flDist) < 0.001)
		return;

	if (fabs(flDist) > 70 && m_bCanTurnAnim && (pev->flags & FL_ONGROUND) != 0)
	{
		flSpeed = m_fFrameTime * 2800 * m_fYawSpeedScale;
		bTurnAnim = true;
	}

	if (bTurnAnim && pev->sequence == LookupSequence("idle"))
	{
		pev->sequence = LookupSequence(flDist > 0 ? "turnleft" : "turnright");
		ResetSequenceInfo();
		pev->frame = 0;
		m_bCanIdle = false;
	}

	ChangeYaw((int)flSpeed);
}

// orig 0x143850: the driver's HUD shows the bullsquid's health in percent
void CDriveBullsquid::UpdateOwnerHUD()
{
	if (!m_pOwner)
		return;

	if (pev->health == (float)m_iClientHealth)
		return;

	float flPercent = (pev->health / pev->max_health) * 100;
	int iHealth = (flPercent > 0) ? (int)flPercent : 0;
	if (iHealth < 1)
		iHealth = 1;
	else if (iHealth > 100)
		iHealth = 100;

	MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_BYTE(iHealth);
	MESSAGE_END();

	m_iClientHealth = (int)pev->health;
}

// orig 0x143970: the bite (bBite) or the tail whip lands
void CDriveBullsquid::DoMeleeDamage(bool bBite)
{
	if (!m_pOwner)
		return;

	int bitsDamage = DMG_SLASH;
	float flDamage;
	float flDist;
	if (bBite)
	{
		flDamage = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidBiteDamage", 45);
		flDist = 72;
	}
	else
	{
		flDamage = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidWhipDamage", 35);
		bitsDamage = DMG_CLUB | DMG_ALWAYSGIB;
		flDist = 88;
	}

	Vector vecSrc = pev->origin + Vector(0, 0, 36.5);
	UTIL_MakeVectors(pev->angles);
	Vector vecEnd = vecSrc + gpGlobals->v_forward * flDist;

	TraceResult tr;
	UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, human_hull, ENT(pev), &tr);

	if (!bBite)
	{
		int iPitch = RANDOM_LONG(90, 110);
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_sDBSMeleeMissSounds[RANDOM_LONG(0, 1)], 1.0, 0.9, 0, iPitch);
	}

	// nothing hurtable: try a little higher, then lower with the bigger hull
	if (!tr.pHit || VARS(tr.pHit)->takedamage == 0)
	{
		vecSrc = pev->origin + Vector(0, 0, 38);
		vecEnd = vecSrc + gpGlobals->v_forward * flDist;
		UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, human_hull, ENT(pev), &tr);

		if (!tr.pHit || VARS(tr.pHit)->takedamage == 0)
		{
			vecSrc = pev->origin + Vector(0, 0, 32);
			vecEnd = vecSrc + gpGlobals->v_forward * flDist;
			UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);

			if (!tr.pHit || VARS(tr.pHit)->takedamage == 0)
			{
				vecSrc = pev->origin + Vector(0, 0, 18);
				vecEnd = vecSrc + gpGlobals->v_forward * flDist;
				UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);

				if (!tr.pHit)
					return;
				if (VARS(tr.pHit)->takedamage == 0)
					return;
			}
		}
	}

	if (bBite)
	{
		int iPitch = RANDOM_LONG(90, 110);
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_sDBSBiteSounds[RANDOM_LONG(0, 1)], 1.0, 0.9, 0, iPitch);
	}

	CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
	if (!pHit)
		return;

	pev->classname = MAKE_STRING(bBite ? "bullsquid_bite" : "bullsquid_whip");
	ClearMultiDamage();
	pHit->TraceAttack(m_pOwner->pev, flDamage, gpGlobals->v_forward, &tr, bitsDamage);
	ApplyMultiDamage(pev, m_pOwner->pev);
	pev->classname = MAKE_STRING("drive_bullsquid");

	int iCap = 0x4000000; // VERIFY: RC cap bit, name unknown (takes bite/whip knockback)
	if (!pHit->HasRCCap(iCap))
		return;

	if (bBite)
	{
		// toss it up, and feed on it
		pHit->pev->velocity = pHit->pev->velocity + gpGlobals->v_forward * 250 + gpGlobals->v_up * 250;
		pev->health = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidHealthPreBite", 25) + pev->health;
		if (pev->health > pev->max_health)
			pev->health = pev->max_health;
	}
	else
	{
		pHit->pev->velocity = pHit->pev->velocity + gpGlobals->v_forward * 100 + gpGlobals->v_right * 100;
	}

	pHit->pev->flags &= ~FL_ONGROUND;
	UTIL_SetOrigin(pHit->pev, pHit->pev->origin + Vector(0, 0, 1));

	if (pHit->IsNetClient())
	{
		if (bBite)
		{
			unsigned short amplitude = FixedUnsigned16(25, 1 << 12);
			unsigned short duration = FixedUnsigned16(0.7, 1 << 12);
			unsigned short frequency = FixedUnsigned16(1.5, 1 << 8);
			MESSAGE_BEGIN(MSG_ONE, gmsgShake, NULL, pHit->pev->pContainingEntity);
			WRITE_SHORT(amplitude);
			WRITE_SHORT(duration);
			WRITE_SHORT(frequency);
			MESSAGE_END();
		}
		else
		{
			pHit->pev->punchangle.z = pHit->pev->punchangle.z - 20;
			pHit->pev->punchangle.x = pHit->pev->punchangle.x + 20;
		}
	}
}

// orig 0x144240. Read from the disassembly; the decompile loses the event switch.
void CDriveBullsquid::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
	{
		// the chumtoadifier beam, aimed by the driver's pitch
		Vector vecAngles;
		if (pev->idealpitch < -70)
			vecAngles.x = -70;
		else if (pev->idealpitch > 10)
			vecAngles.x = 10;
		else
			vecAngles.x = pev->idealpitch;
		vecAngles.y = pev->angles.y;
		vecAngles.z = 0;
		UTIL_MakeVectors(vecAngles);

		Vector vecSrc = pev->origin + gpGlobals->v_right * 8 + gpGlobals->v_forward * 37 + gpGlobals->v_up * 23;
		if (m_pOwner)
			DBSFireChumtoadifer(ENT(pev), m_pOwner->pev->pContainingEntity, vecSrc, gpGlobals->v_forward);
		else
			DBSFireChumtoadifer(ENT(pev), ENT(pev), vecSrc, gpGlobals->v_forward);
		break;
	}

	case 4:
		DoMeleeDamage(false);
		break;

	case 6:
		DoMeleeDamage(true);
		break;

	case 2001:
		if ((pev->flags & FL_ONGROUND) == 0)
			break;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, RANDOM_LONG(0, 1) == 0 ? "common/bodydrop3.wav" : "common/bodydrop4.wav", 1.0, 0.8, 0, 100);
		break;

	case 2002:
		if ((pev->flags & FL_ONGROUND) == 0)
			break;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, RANDOM_LONG(0, 1) == 0 ? "common/bodydrop3.wav" : "common/bodydrop4.wav", 1.0, 0.8, 0, 90);
		break;

	case 2010:
	{
		int iPitch = RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_sDBSMeleeMissSounds[RANDOM_LONG(0, 1)], 1.0, 1.0, 0, iPitch);
		break;
	}
	}
}

// orig 0x1446f0
void CDriveBullsquid::DyingThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_fSequenceFinished && gpGlobals->time < pev->dmgtime)
	{
		DispatchAnimEvents(StudioFrameAdvance());
		return;
	}

	pev->deadflag = DEAD_DEAD;
	if (pev->dmgtime <= gpGlobals->time || (pev->flags & FL_ONGROUND) != 0)
		FadeMonster();
}

// orig 0x1447b0: "die" or the sequence after it
void CDriveBullsquid::SetDeathAnim()
{
	int iSeq = LookupSequence("die");
	pev->sequence = iSeq + RANDOM_LONG(0, 1);
	ResetSequenceInfo();
	pev->frame = 0;
}

// orig 0x144810
bool CDriveBullsquid::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_pOwner)
	{
		CBaseEntity* pAttacker = CBaseEntity::Instance(pevAttacker->pContainingEntity);
		if (pAttacker && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, pAttacker))
			return false;
		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	if ((bitsDamageType & DMG_SHOCK) != 0)
		flDamage *= 0.5;
	if ((bitsDamageType & DMG_ACID) != 0)
		flDamage *= 0.5;
	if ((bitsDamageType & DMG_POISON) != 0)
		flDamage *= 0.5;
	if ((bitsDamageType & DMG_RADIATION) != 0)
		flDamage *= 0.5;

	if (m_fNextPainSound < gpGlobals->time)
	{
		m_fNextPainSound = RANDOM_FLOAT(3, 4) + gpGlobals->time;
		int iPitch = RANDOM_LONG(85, 120);
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_sDBSPainSounds[RANDOM_LONG(0, 3)], 1.0, 0.8, 0, iPitch);
	}

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x1449f0: the bullsquid dies, and the driver with it
void CDriveBullsquid::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->deadflag != DEAD_NO)
		return;

	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 0, 0, 0, 100);

	pev->renderamt = 0;
	pev->renderfx = kRenderFxNone;

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_BYTE(4);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_BYTE(0);
		MESSAGE_END();
	}

	// tell the pickup it can respawn
	if (pev->owner)
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	pev->classname = MAKE_STRING("Bullsquid"); // sic, capital B in the original
	DEClientLeave();

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		UTIL_SetOrigin(m_pOwner->pev, m_pOwner->m_pCurDriveEnt->pev->origin + Vector(0, 0, 64));
		m_pOwner->pev->effects |= EF_NODRAW;
		m_pOwner->pev->health = -1;
		m_pOwner->Killed(pevAttacker, GIB_NEVER);
	}

	if ((pev->health < -30 && iGib != GIB_NEVER) || iGib == GIB_ALWAYS)
	{
		GibMonster();
	}
	else
	{
		SetDeathAnim();
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_sDBSDieSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, 100);
		SetThink(&CDriveBullsquid::DyingThink);
		pev->nextthink = gpGlobals->time + 0.1;
		pev->dmgtime = gpGlobals->time + 5;
	}
}

// orig 0x144da0: from the driver's PreThink: steer, and put his camera behind the bullsquid
void CDriveBullsquid::DEClientPreThink()
{
	if ((m_pOwner->pev->button & IN_RELOAD) == 0)
	{
		pev->ideal_yaw = UTIL_AngleMod(m_pOwner->pev->v_angle.y);
		pev->idealpitch = m_pOwner->pev->v_angle.x;
	}

	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;

	SetEyePosition();
	UTIL_MakeVectors(m_pOwner->pev->v_angle);

	Vector vecOrg = GetCamRotOrg();
	if (m_vecSudoVel.Length() > 0.01 && m_fLastMoveTime < gpGlobals->time)
		vecOrg = GetCamRotOrg() + m_vecSudoVel * (gpGlobals->time - m_fLastMoveTime);

	Vector vecEnd = vecOrg - gpGlobals->v_forward * 180;
	TraceResult tr;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine(vecOrg, vecEnd, ignore_monsters, ENT(pev), &tr);

	Vector vecCam = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;

	// out of water, walk the camera forward until it is in open air
	int i = 0;
	if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
	{
		for (i = 1; i < 48; i++)
		{
			if (UTIL_PointContents(vecCam) == CONTENTS_EMPTY)
				break;
			vecCam = tr.vecEndPos + gpGlobals->v_forward * (i * 8.0f);
		}
	}

	if (i == 48)
	{
		if (tr.flFraction >= 1.0)
			vecCam = tr.vecEndPos;
		else
			vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;
	}

	if (UTIL_PointContents(vecCam) == CONTENTS_SOLID)
		vecCam = vecOrg;

	m_pOwner->m_afPhysicsFlags = 0;
	UTIL_SetOrigin(m_pOwner->pev, vecCam);

	// the client's driveentalpha: the bullsquid drawn see-through for its driver
	float flAlpha = m_pOwner->m_fDrvEntAlpha;
	if (flAlpha >= 1 || !(flAlpha > 0))
	{
		m_pOwner->pev->euser2 = NULL;
		m_pOwner->pev->iuser3 = 0;
		m_pOwner->pev->fuser1 = 0;
		m_pOwner->pev->fuser2 = 0;
	}
	else
	{
		m_pOwner->pev->euser2 = ENT(pev);
		m_pOwner->pev->iuser3 = 4;
		m_pOwner->pev->fuser1 = 2;

		float flAmt = 0;
		if (m_pOwner->m_fDrvEntAlpha * 255 >= 0)
		{
			flAmt = 255;
			if (m_pOwner->m_fDrvEntAlpha * 255 <= 255)
				flAmt = m_pOwner->m_fDrvEntAlpha * 255;
		}
		m_pOwner->pev->fuser2 = flAmt;
	}
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
}

// orig 0x145310
void CDriveBullsquid::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x145330: the player gets in
void CDriveBullsquid::DEClientEnter(CBasePlayer* pPlayer)
{
	m_pOwner = pPlayer;

	pPlayer->pev->deadflag = DEAD_DEAD;
	m_pOwner->pev->health = 1;
	m_pOwner->pev->takedamage = DAMAGE_NO;
	m_pOwner->pev->movetype = MOVETYPE_NOCLIP;
	m_pOwner->pev->solid = SOLID_NOT;
	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;
	m_pOwner->pev->flags = FL_SPECTATOR | FL_NOTARGET | FL_CLIENT | FL_FLY;
	m_pOwner->pev->classname = MAKE_STRING("client_in_cam");
	m_pOwner->pev->view_ofs = g_vecZero;
	UTIL_SetSize(m_pOwner->pev, g_vecZero, g_vecZero);
	m_pOwner->pev->armorvalue = 0;
	m_pOwner->RemoveAllItems(false);

	m_pOwner->m_bInDriveEnt = true;
	m_pOwner->m_pCurDriveEnt = this;
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 5;

	if (m_pOwner->m_pActiveItem)
	{
		m_pOwner->m_pActiveItem->Holster();
		m_pOwner->pev->weaponmodel = 0;
	}

	m_pOwner->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT;
	m_pOwner->m_afPhysicsFlags = 0;
	m_iClientHealth = -1;

	MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_BYTE(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_BYTE(m_pOwner->m_iHideHUD);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgBattery, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_SHORT(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgFlashBattery, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_BYTE(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgTrain, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_BYTE(0);
	MESSAGE_END();

	pev->ideal_yaw = m_pOwner->pev->v_angle.y;
	pev->idealpitch = m_pOwner->pev->v_angle.x;

	CLIENT_COMMAND(m_pOwner->pev->pContainingEntity, "cl_forwardspeed 0\ncl_backspeed 0\ncl_sidespeed 0\n");
}

// orig 0x1456e0: the player gets out (or dies with the bullsquid): make him a player again
void CDriveBullsquid::DEClientLeave()
{
	if (!m_pOwner || !m_pOwner->pev || !m_pOwner->pev->pContainingEntity || m_pOwner->pev->pContainingEntity->free || !m_pOwner->m_bInDriveEnt)
		return;

	CLIENT_COMMAND(m_pOwner->pev->pContainingEntity, "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");

	m_pOwner->pev->deadflag = DEAD_NO;
	m_pOwner->pev->health = 100;
	m_pOwner->pev->takedamage = DAMAGE_AIM;
	m_pOwner->pev->movetype = MOVETYPE_WALK;
	m_pOwner->pev->solid = SOLID_SLIDEBOX;
	m_pOwner->pev->rendermode = kRenderNormal;
	m_pOwner->pev->renderamt = 255;
	m_pOwner->pev->effects &= ~EF_NODRAW;
	m_pOwner->pev->flags = FL_CLIENT;
	m_pOwner->pev->classname = MAKE_STRING("player");
	m_pOwner->pev->view_ofs = Vector(0, 0, 28);
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 20;
	m_pOwner->m_fDrcEntTransTime = gpGlobals->time + 15;
	UTIL_SetSize(m_pOwner->pev, Vector(-16, -16, -36), Vector(16, 16, 36));

	m_pOwner->pev->euser2 = NULL;
	m_pOwner->pev->iuser3 = 0;
	m_pOwner->pev->fuser1 = 0;
	m_pOwner->pev->fuser2 = 0;
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;

	if (m_pOwner->m_pActiveItem)
		m_pOwner->m_pActiveItem->Deploy();

	m_pOwner->m_iHideHUD &= ~(HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT);

	SET_VIEW(m_pOwner->pev->pContainingEntity, m_pOwner->pev->pContainingEntity);
	SetupVisibility(m_pOwner->pev->pContainingEntity, m_pOwner->pev->pContainingEntity, NULL, NULL);
}

// orig 0x1459f0: the driver asks out; he lands on top and the bullsquid dies
void CDriveBullsquid::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVEBULLSQUID", "AllowEject", false))
	{
		if (CVAR_GET_FLOAT("sv_allowglobaleject") != 1 && g_pGameRules->IsMultiplayer() && !g_pGameRules->IsCoOp())
			return;
	}

	CBasePlayer* pOwner = m_pOwner;
	if (!pOwner->m_bInDriveEnt || !pOwner->m_pCurDriveEnt || pOwner->m_pCurDriveEnt->pev != pev || pOwner->m_pCurDriveEnt->pev->health <= 0)
		return;

	if (pOwner->m_fDrvEntEjectTime >= gpGlobals->time)
		return;

	DEClientLeave();
	m_pOwner->UpdateClientData();
	m_pOwner->m_bInDriveEnt = false;
	Killed(pev, GIB_ALWAYS);

	UTIL_SetOrigin(m_pOwner->pev, pev->origin + Vector(0, 0, 48));
	m_pOwner->pev->v_angle = pev->angles;
	m_pOwner->pev->angles = m_pOwner->pev->v_angle;
	m_pOwner->pev->v_angle.x = 0;
	m_pOwner->pev->angles.x = 0;
	m_pOwner->pev->v_angle.z = 0;
	m_pOwner->pev->angles.z = 0;
	m_pOwner->pev->fixangle = 1;
	m_pOwner->m_pCurDriveEnt = NULL;
}

// orig 0x145c00
LINK_ENTITY_TO_CLASS(pickup_drive_bullsquid, CDriveBullsquidPickup);

TYPEDESCRIPTION CDriveBullsquidPickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveBullsquidPickup, m_pGiven, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveBullsquidPickup, m_bTakeable, FIELD_BOOLEAN),
};

// orig 0x145c30 (Save), 0x145c80 (Restore)
IMPLEMENT_SAVERESTORE(CDriveBullsquidPickup, CBaseMonster);

// orig 0x145cd0: sits on an info_drive_bullsquid_spawn, which it removes
void CDriveBullsquidPickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), "models/bullsquid.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->sequence = LookupSequence("idle1");
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);
	InitBoneControllers();

	SetUse(&CDriveBullsquidPickup::Use);
	SetThink(&CDriveBullsquidPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_bTakeable = true;
	m_pGiven = NULL;

	CBaseEntity* pSpot = GetSpawnOrg();
	if (!pSpot)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	UTIL_SetOrigin(pev, pSpot->Center());
	pev->angles = Vector(0, pSpot->pev->angles.y, 0);
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	pev->renderfx = kRenderFxDistort;
	DoRespawn();
	UTIL_Remove(pSpot);
}

// orig 0x145f10
void CDriveBullsquidPickup::Precache()
{
	PRECACHE_MODEL("models/bullsquid.mdl");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_bullsquid");
}

// orig 0x145f60: the first spawn spot not already being removed
CBaseEntity* CDriveBullsquidPickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_bullsquid_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (FNullEnt(pSpot->pev))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x145fe0
bool CDriveBullsquidPickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return false;
}

// orig 0x145ff0: fades the ghost in; while given out, ends the drive when its time is up
void CDriveBullsquidPickup::PAnimThink()
{
	float flInterval = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_bTakeable)
	{
		if (m_pGiven && m_pGiven->pev->deadflag == DEAD_NO && pev->dmgtime < gpGlobals->time)
		{
			g_pevLastInflictor = pev;
			m_pGiven->Killed(pev, GIB_NORMAL);
			pev->dmgtime = gpGlobals->time + 1e+06;
		}
		if (!m_bTakeable)
			return;
	}

	if (pev->rendermode != kRenderTransTexture || pev->renderamt >= 150)
	{
		pev->rendermode = kRenderTransTexture;
		pev->renderamt = 150;
		pev->renderfx = kRenderFxDistort;
	}
	else
	{
		pev->renderamt = flInterval * 56.689342 + pev->renderamt;
		if (pev->renderamt >= 150)
		{
			pev->rendermode = kRenderTransTexture;
			pev->renderamt = 150;
			pev->renderfx = kRenderFxDistort;
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 0, 0, SND_STOP, 100);
		}
	}
}

// orig 0x146190: a player uses the ghost: make a drive_bullsquid and put him in it
void CDriveBullsquidPickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!m_bTakeable)
		return;

	// VERIFY: the original tests pCaller (not pActivator), and calls IsPlayer before the null test
	if (!pCaller->IsPlayer() || !pCaller)
		return;

	CBasePlayer* pPlayer = (CBasePlayer*)pCaller;
	if (pPlayer->m_fDrcEntTransTime > gpGlobals->time || pPlayer->m_bInDriveEnt)
		return;
	if (pPlayer->Classify() != CLASS_PLAYER)
		return;
	if (strcmp(STRING(pPlayer->pev->classname), "player") != 0)
		return;

	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_bullsquid", pev->origin, pev->angles, ENT(pev));
	pPlayer->pev->angles = Vector(0, pev->angles.y, 0);
	pPlayer->pev->v_angle = pPlayer->pev->angles;
	pPlayer->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGiven = pPlayer->m_pCurDriveEnt;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDriveBullsquidPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidMaxDriveTime", 600) + gpGlobals->time;
}

// orig 0x146390
void CDriveBullsquidPickup::DoRespawn()
{
	m_bTakeable = true;
	m_pGiven = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);
	SetThink(&CDriveBullsquidPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x146450: the drive_bullsquid it made has died
void CDriveBullsquidPickup::DeathNotice(entvars_t* pevChild)
{
	if (m_bTakeable)
		return;

	pev->nextthink = PREFS_KeyLoopUpFloat("DRIVEBULLSQUID", "BullsquidRespawnDelayAfterDeath", 5) + gpGlobals->time;
	SetThink(&CDriveBullsquidPickup::DoRespawn);
}

// orig 0x1464c0: in multiplayer, one drive_bullsquid_spawner per map
void CreateDrvBullsquidSpawner()
{
	if (!g_pGameRules->IsMultiplayer())
		return;

	if (!UTIL_FindEntityByClassname(NULL, "drive_bullsquid_spawner"))
		CBaseEntity::Create("drive_bullsquid_spawner", g_vecZero, g_vecZero, NULL);
}

// orig 0x146520
LINK_ENTITY_TO_CLASS(drive_bullsquid_spawner, CDrvBullsquidSpawner);

TYPEDESCRIPTION CDrvBullsquidSpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvBullsquidSpawner, m_bDummy, FIELD_BOOLEAN),
};

// orig 0x146550 (Save), 0x1465a0 (Restore)
IMPLEMENT_SAVERESTORE(CDrvBullsquidSpawner, CBaseEntity);

// orig 0x1465f0: makes mp_drivebullsquid pickups, then goes
void CDrvBullsquidSpawner::DrvAThink()
{
	int iCount = (int)CVAR_GET_FLOAT("mp_drivebullsquid");
	for (int i = 0; i < iCount; i++)
		CBaseEntity::Create("pickup_drive_bullsquid", g_vecZero, g_vecZero, NULL);

	UTIL_Remove(this);
}

// orig 0x146680
void CDrvBullsquidSpawner::Spawn()
{
	pev->classname = MAKE_STRING("drive_bullsquid_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvBullsquidSpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15;
}

// orig 0x146720: the chumtoadifier: a green beam from vecSrc; whatever non-BSP thing it hits
// becomes a chumtoad (drivechum.cpp)
void DBSFireChumtoadifer(edict_t* pentShooter, edict_t* pentOwner, Vector vecSrc, Vector vecDir)
{
	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, pentShooter, &tr);

	CBeam* pBeam = CBeam::BeamCreate("sprites/xbeam5.spr", 50);
	pBeam->PointsInit(vecSrc, tr.vecEndPos);
	pBeam->pev->rendercolor = Vector(64, 255, 64);
	pBeam->pev->renderamt = 255;
	pBeam->pev->body = 0;
	pBeam->pev->animtime = 40;
	// CBeam::FadeThink's parameters. VERIFY: their meaning is effects.cpp's (fade rate,
	// start time, delay, a flag), not settled here
	pBeam->pev->armorvalue = -255;
	pBeam->pev->teleport_time = gpGlobals->time;
	pBeam->pev->armortype = 0.1;
	pBeam->pev->team = 0;
	pBeam->SetThink(&CBeam::FadeThink);
	pBeam->pev->nextthink = gpGlobals->time + 0.01;
	pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;

	pBeam = CBeam::BeamCreate("sprites/lgtning.spr", 50);
	pBeam->PointsInit(vecSrc, tr.vecEndPos);
	pBeam->pev->rendercolor = Vector(128, 128, 255);
	pBeam->pev->renderamt = 255;
	pBeam->pev->body = 140;
	pBeam->pev->animtime = 40;
	pBeam->pev->armorvalue = -318.75;
	pBeam->pev->teleport_time = gpGlobals->time;
	pBeam->pev->armortype = 0.1;
	pBeam->pev->team = 0;
	pBeam->SetThink(&CBeam::FadeThink);
	pBeam->pev->nextthink = gpGlobals->time + 0.01;
	pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;

	EMIT_SOUND_DYN(pentShooter, CHAN_WEAPON, "debris/beamstart9.wav", 1.0, 0.8, 0, 140);
	EMIT_SOUND_DYN(pentShooter, CHAN_ITEM, "weapons/electro5.wav", 1.0, 0.8, 0, 80);

	if (!tr.pHit)
		return;

	CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
	if (pHit && CBaseEntity::Instance(tr.pHit)->IsBSPModel())
	{
		UTIL_DecalTrace(&tr, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 1));
		return;
	}

	if (tr.pHit)
		DCTChumtoadifyEnt(tr.pHit, pentOwner);
}

// orig 0x146a50
int CDrvBullsquidSpawner::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}

// orig 0x146ab0
int CDriveBullsquidPickup::ObjectCaps()
{
	return m_bTakeable ? (FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE) : FCAP_ACROSS_TRANSITION;
}

// orig 0x146ad0
int CDriveBullsquidPickup::GetRCCaps()
{
	return 0;
}

// orig 0x146ae0
bool CDriveBullsquidPickup::IsTeleFraggabe()
{
	return false;
}

// orig 0x146af0
int CDriveBullsquidPickup::BloodColor()
{
	return DONT_BLEED;
}

// orig 0x146b00
int CDriveBullsquidPickup::Classify()
{
	return CLASS_NONE;
}

// orig 0x146b60
void CDriveBullsquid::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;

	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

// orig 0x146be0
int CDriveBullsquid::GetRCCaps()
{
	return 0x1aa2dfd;
}

// orig 0x146bf0
int CDriveBullsquid::GetGuassDistOut()
{
	return 56;
}

// orig 0x146c00
int CDriveBullsquid::GetGuassHull()
{
	return human_hull;
}

// orig 0x146c10
Vector CDriveBullsquid::GetCamRotOrg()
{
	return pev->origin + Vector(0, 0, 48);
}

// orig 0x146c70
int CDriveBullsquid::BloodColor()
{
	return BLOOD_COLOR_YELLOW;
}

// orig 0x146c80
int CDriveBullsquid::Classify()
{
	return CLASS_ALIEN_PREDATOR;
}


