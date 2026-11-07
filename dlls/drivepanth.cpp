/***
*
*	Rocket Crowbar 1.9: drivepanth.cpp. The drivable panther (drive_panth), the pickup a player
*	uses to get in (pickup_drive_panth) and the spawner that makes the pickups
*	(drive_panth_spawner). Reconstructed from rc_i386.so; addresses are file addresses
*	(reference/funcfile.tsv).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "client.h"
#include "UserMessages.h"
#include "drivepanth.h"

// Preff.cpp (prefs-votes package)
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
extern bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault);

sDPSoundT g_pDPAttackHitSounds[] =
	{
		{"zombie/claw_strike1.wav", 100, 100},
		{"zombie/claw_strike2.wav", 100, 100},
		{"zombie/claw_strike3.wav", 100, 100},
};

sDPSoundT g_pDPAttackMissSounds[] =
	{
		{"zombie/claw_miss1.wav", 100, 100},
		{"zombie/claw_miss2.wav", 100, 100},
};

sDPSoundT g_pDPStartPounceSounds[] =
	{
		{"tentacle/te_alert1.wav", 85, 95},
		{"tentacle/te_alert2.wav", 85, 95},
};

sDPSoundT g_pDPPainSounds[] =
	{
		{"agrunt/ag_pain1.wav", 90, 100},
		{"agrunt/ag_pain2.wav", 95, 105},
		{"agrunt/ag_pain3.wav", 95, 100},
};

sDPSoundT g_pDPDeathSounds[] =
	{
		{"garg/gar_die2.wav", 100, 110},
		{"agrunt/ag_die2.wav", 70, 80},
};

sDPSoundT g_pDPIdleSounds[] =
	{
		{"agrunt/ag_alert4.wav", 100, 100},
		{"agrunt/ag_alert5.wav", 100, 100},
		{"agrunt/ag_pain2.wav", 100, 100},
		{"agrunt/ag_pain1.wav", 100, 100},
};

sDPSoundT g_pDPChragePounceSounds[] =
	{
		{"agrunt/ag_attack2.wav", 80, 90},
		{"agrunt/ag_attack3.wav", 80, 90},
};

sDPSoundT g_pDPStartClawSounds[] =
	{
		{"agrunt/ag_attack2.wav", 110, 115},
		{"agrunt/ag_attack3.wav", 105, 110},
};

sDPAnimT g_sDPanthIdleAnims[] =
	{
		{"idle_figit", 0.6, 10, 20},
};

// plays one entry of a sound table with a pitch from its range
static void DPEmitSound(CBaseEntity* pEntity, int iChannel, const sDPSoundT& sound, float flVolume, float flAttenuation)
{
	EMIT_SOUND_DYN(pEntity->edict(), iChannel, sound.pszSound, flVolume, flAttenuation, 0, RANDOM_LONG(sound.iMinPitch, sound.iMaxPitch));
}

// orig 0x16ee10
LINK_ENTITY_TO_CLASS(drive_panth, CDrivePanth);

// orig 0x16ee40
void CDrivePanth::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_panth");
	SET_MODEL(ENT(pev), "models/panther.mdl");
	UTIL_SetSize(pev, Vector(-18, -18, 0), Vector(18, 18, 44));
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->effects = 0;
	pev->friction = 1;
	pev->health = PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthHealth", 360);
	pev->max_health = pev->health;
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	pev->sequence = 0;
	ResetSequenceInfo();
	pev->frame = 0;
	m_fSequenceLoops = true;
	InitBoneControllers();
	pev->framerate = 1;
	SetEyePosition();

	m_bInJump = false;
	m_fJumpLandTime = -1;
	m_fNextJump = gpGlobals->time;
	m_bInClaw = false;
	m_fUnknown2cc = gpGlobals->time;
	m_flHeadYaw = 0;
	m_flHeadYawSpeed = 2000;
	m_fLastClaw = gpGlobals->time;
	m_bClawSide = RANDOM_LONG(0, 1) != 0;
	m_fLastActiveTime = gpGlobals->time;
	m_fNextRegen = gpGlobals->time;
	m_fUnknown2e0 = gpGlobals->time;
	m_fNextIdleTime = RANDOM_FLOAT(3, 7) + gpGlobals->time;
	m_fIdleSoundTime = RANDOM_FLOAT(4, 7) + gpGlobals->time;

	SetThink(&CDrivePanth::DPThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x16f100
void CDrivePanth::Precache()
{
	unsigned int i;

	PRECACHE_MODEL("models/panther.mdl");
	for (i = 0; i < ARRAYSIZE(g_pDPAttackHitSounds); i++)
		PRECACHE_SOUND(g_pDPAttackHitSounds[i].pszSound);
	for (i = 0; i < ARRAYSIZE(g_pDPAttackMissSounds); i++)
		PRECACHE_SOUND(g_pDPAttackMissSounds[i].pszSound);
	for (i = 0; i < ARRAYSIZE(g_pDPStartPounceSounds); i++)
		PRECACHE_SOUND(g_pDPStartPounceSounds[i].pszSound);
	for (i = 0; i < ARRAYSIZE(g_pDPPainSounds); i++)
		PRECACHE_SOUND(g_pDPPainSounds[i].pszSound);
	for (i = 0; i < ARRAYSIZE(g_pDPDeathSounds); i++)
		PRECACHE_SOUND(g_pDPDeathSounds[i].pszSound);
	for (i = 0; i < ARRAYSIZE(g_pDPIdleSounds); i++)
		PRECACHE_SOUND(g_pDPIdleSounds[i].pszSound);
	for (i = 0; i < ARRAYSIZE(g_pDPChragePounceSounds); i++)
		PRECACHE_SOUND(g_pDPChragePounceSounds[i].pszSound);
	for (i = 0; i < ARRAYSIZE(g_pDPStartClawSounds); i++)
		PRECACHE_SOUND(g_pDPStartClawSounds[i].pszSound);
}

// orig 0x16f260
const char* CDrivePanth::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;

	return STRING(m_pOwner->pev->netname);
}

// orig 0x16f2d0
void CDrivePanth::DPThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_pOwner && !FNullEnt(m_pOwner->pev) && m_pOwner->pev->pContainingEntity && m_pOwner->pev->pContainingEntity->pvPrivateData && !m_pOwner->pev->pContainingEntity->free && UTIL_PlayerByIndex(ENTINDEX(m_pOwner->pev->pContainingEntity)))
	{
		if (pev->waterlevel < 2)
			CheckGround();
		else
			pev->flags &= ~FL_ONGROUND;

		if ((pev->flags & FL_ONGROUND) == 0)
			m_fLastActiveTime = gpGlobals->time;

		DispatchAnimEvents(m_fFrameTime);
		UpdateOwnerHUD();

		if ((pev->flags & FL_FROZEN) != 0)
		{
			pev->sequence = 0;
			ResetSequenceInfo();
			pev->frame = 0;
			m_vecSudoVel = g_vecZero;
			m_fLastMoveTime = gpGlobals->time;
			return;
		}

		// keep the driver hidden
		m_pOwner->pev->rendermode = kRenderTransColor;
		m_pOwner->pev->renderamt = 0;
		m_pOwner->pev->effects |= EF_NODRAW;

		m_bCanIdle = true;
		m_bCanMove = true;
		m_bCanTurn = true;
		m_bCanTurnAnim = true;
		m_bCanJump = true;
		m_bCanClaw = true;
		m_fYawSpeedScale = 1;
		m_flHeadYawSpeed = 1200;
		m_bHeadTrack = false;

		if (m_bInClaw)
		{
			m_fLastActiveTime = gpGlobals->time;
			if (!m_fSequenceFinished)
			{
				m_bCanIdle = false;
				m_bCanMove = false;
				m_bCanTurn = false;
				m_bCanTurnAnim = false;
				m_bCanJump = false;
				m_bHeadTrack = true;
				m_fYawSpeedScale = 0.7;
			}
			else
			{
				m_bInClaw = false;
			}
		}

		DoJump();
		DoClaw();
		DoMovement();
		DoYaw();
		DoRegen();

		pev->fuser4 = m_vecSudoVel.Length();

		if (m_bCanIdle)
		{
			if (m_fIdleSoundTime < gpGlobals->time)
			{
				m_fIdleSoundTime = RANDOM_FLOAT(6, 10) + gpGlobals->time;
				DPEmitSound(this, CHAN_VOICE, g_pDPIdleSounds[RANDOM_LONG(0, 3)], 0.6, 1.1);
			}

			if (!m_fSequenceFinished)
				return;

			if (m_fNextIdleTime < gpGlobals->time)
			{
				m_fNextIdleTime = SetRandomAnim(g_sDPanthIdleAnims, 1) + gpGlobals->time;
				return;
			}

			if (pev->sequence == 0)
				return;

			pev->sequence = 0;
			ResetSequenceInfo();
			pev->frame = 0;
			m_fSequenceLoops = true;
			return;
		}

		m_fNextIdleTime = RANDOM_FLOAT(3, 7) + gpGlobals->time;
		m_fIdleSoundTime = RANDOM_FLOAT(4, 7) + gpGlobals->time;
		return;
	}

	// the driver is gone
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("alien_slave"); // VERIFY: the original's own string, kept from CDriveASlave
	g_pevLastInflictor = pev;
	Killed(pev, GIB_NORMAL);
}

// orig 0x16f740: empty in the original
void CDrivePanth::CheckGround()
{
}

// orig 0x16f760: the pounce lands on something
void CDrivePanth::JumpTouch(CBaseEntity* pOther)
{
	if (!pOther)
		return;

	SetTouch(NULL);

	if (pOther->pev->takedamage == 0 || (pev->flags & FL_ONGROUND) != 0)
		return;

	if (pev->velocity.Length() <= 500)
		return;

	pev->classname = MAKE_STRING("panther_pounce");
	if (!m_pOwner)
		pOther->TakeDamage(pev, pev, PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthPounceDamage", 95), DMG_SLASH);
	else
		pOther->TakeDamage(pev, m_pOwner->pev, PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthPounceDamage", 95), DMG_SLASH);
	pev->classname = MAKE_STRING("drive_panth");

	int iCap = 0x80000; // VERIFY: RC cap bit, name unknown (takes knockback)
	if (pOther->HasRCCap(iCap))
	{
		pOther->pev->velocity = pOther->pev->velocity + pev->velocity * 0.5;
		pOther->pev->velocity.z += 100;
		pOther->pev->flags &= ~FL_ONGROUND;
	}

	pev->velocity = pev->velocity * 0.2;
}

// orig 0x16f980
void CDrivePanth::DoJump()
{
	if (m_bInJump)
	{
		m_fLastActiveTime = gpGlobals->time;

		if ((pev->flags & FL_ONGROUND) == 0)
		{
			m_bCanTurn = false;
		}
		else
		{
			m_bCanTurn = true;
			m_fYawSpeedScale = 0.4;
		}

		if (((pev->flags & FL_ONGROUND) == 0 && pev->waterlevel == 0) || gpGlobals->time < m_fJumpLandTime)
		{
			m_bCanIdle = false;
			m_bCanMove = false;
			m_bCanClaw = false;
			m_bCanTurnAnim = false;
			return;
		}

		if (m_fJumpLandTime + 0.2 < gpGlobals->time)
			m_bInJump = false;
	}

	if (m_bCanJump && (m_pOwner->pev->button & IN_JUMP) != 0 && (pev->flags & FL_ONGROUND) != 0 && m_fNextJump <= gpGlobals->time)
	{
		m_fNextJump = gpGlobals->time + 1.5;
		m_fJumpLandTime = gpGlobals->time + 0.7333;
		m_bInJump = true;
		pev->sequence = LookupSequence("crouch_to_jump");
		ResetSequenceInfo();
		pev->frame = 0;

		DPEmitSound(this, CHAN_VOICE, g_pDPChragePounceSounds[RANDOM_LONG(0, 1)], 0.8, 0.9);

		m_bCanIdle = false;
		m_bCanMove = false;
		m_bCanTurn = false;
		m_bCanClaw = false;
		m_bCanTurnAnim = false;
		m_fLastActiveTime = gpGlobals->time;
	}
}

// orig 0x16fba0
void CDrivePanth::DoClaw()
{
	if (m_bInClaw)
	{
		m_bCanIdle = false;
		m_bCanMove = false;
		m_bCanClaw = false;
		m_bCanTurnAnim = false;
		m_bHeadTrack = true;
		m_bCanTurn = false;
		return;
	}

	if (m_fLastClaw + 2 < gpGlobals->time)
		m_bClawSide = RANDOM_LONG(0, 1) != 0;

	if (!m_bCanClaw || (m_pOwner->pev->button & (IN_ATTACK | IN_ATTACK2)) == 0)
		return;

	m_bInClaw = true;

	if ((m_pOwner->pev->button & IN_ATTACK) == 0)
	{
		// secondary: the two-claw swipe
		pev->sequence = LookupSequence("attack_primary");
		m_bMainClaw = false;
		DPEmitSound(this, CHAN_VOICE, g_pDPStartClawSounds[RANDOM_LONG(0, 1)], 0.9, 1.0);
	}
	else
	{
		if (!m_bClawSide)
		{
			pev->sequence = LookupSequence("attack_main_claw");
			m_bMainClaw = true;
		}
		else
		{
			pev->sequence = LookupSequence("attack_simple_claw");
			m_bMainClaw = false;
		}
		DPEmitSound(this, CHAN_VOICE, g_pDPStartClawSounds[RANDOM_LONG(0, 1)], 0.8, 1.0);
	}

	ResetSequenceInfo();
	pev->frame = 0;
	m_fLastClaw = gpGlobals->time;
	m_bCanIdle = false;
	m_bCanMove = false;
	m_bCanTurn = false;
	m_bCanClaw = false;
	m_bCanTurnAnim = false;
	m_bHeadTrack = true;
	m_fYawSpeedScale = 0.7;
	m_fLastActiveTime = gpGlobals->time;
}

// orig 0x16fe20: walking into a door opens it
void CDrivePanth::CheckDoor()
{
	if (gpGlobals->time < m_fNextDoorCheck)
		return;
	if (!m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecSrc = pev->origin + Vector(0, 0, 16);
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

// orig 0x170070: hop a small ledge the walk could not take
void CDrivePanth::CheckEHop()
{
	if (gpGlobals->time < m_fNextEHopCheck || !m_bCanMove)
		return;

	if (pev->velocity.z > 16 || (pev->flags & FL_ONGROUND) == 0)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecFront = pev->origin + Vector(0, 0, 4) + gpGlobals->v_forward * 28;
	Vector vecDown = vecFront - Vector(0, 0, 24);
	Vector vecSrc = pev->origin + Vector(0, 0, 4);

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecFront, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
	{
		UTIL_TraceLine(vecFront, vecDown, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
		{
			pev->velocity.z += 50;
			pev->velocity = pev->velocity + gpGlobals->v_forward * 100;
			pev->flags &= ~FL_ONGROUND;
			UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
		}
	}

	m_fNextEHopCheck = gpGlobals->time + 0.2;
}

// orig 0x170390: swimming
void CDrivePanth::RunWaterPhysics()
{
	float flFriction = PREFS_KeyLoopUpFloat("DRIVEPANTH", "SlaveWaterFriction", 8); // VERIFY: the original's key, kept from CDriveASlave

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

	if (!m_bCanMove)
		return;

	if (pev->sequence != LookupSequence("run"))
	{
		pev->sequence = LookupSequence("run");
		ResetSequenceInfo();
	}

	float flRate;
	if (pev->velocity.Length2D() > 0.1)
		flRate = pev->velocity.Length2D() / 60;
	else
		flRate = 0.3;

	if (flRate < 0.3)
		pev->framerate = 0.3;
	else if (flRate > 1.1)
		pev->framerate = 1.1;
	else
		pev->framerate = flRate;

	if ((m_pOwner->pev->button & IN_FORWARD) != 0)
	{
		UTIL_MakeAimVectors(pev->angles);
		float flWishSpeed = PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthWaterSpeed", 250);
		float flAccel = PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthWaterAccel", 220);
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

// orig 0x1706f0: the driver holds forward; walk along the facing
void CDrivePanth::DoMovement()
{
	if ((pev->flags & FL_ONGROUND) == 0)
	{
		if (pev->waterlevel != 0)
		{
			RunWaterPhysics();
		}
		else if (m_bCanIdle)
		{
			float flSpeed = pev->velocity.Length2D();
			const char* pszSeq = NULL;
			// VERIFY: literal reading, run between 30 and 120 and walk above that while airborne
			if (flSpeed > 30 && flSpeed < 120)
			{
				if (pev->sequence != LookupSequence("run"))
					pszSeq = "run";
			}
			else if (flSpeed >= 120 && pev->sequence != LookupSequence("walk"))
			{
				pszSeq = "walk";
			}

			if (pszSeq)
			{
				pev->sequence = LookupSequence(pszSeq);
				ResetSequenceInfo();
				pev->frame = 0;
			}
		}
	}
	else if (m_bCanMove)
	{
		if ((m_pOwner->pev->button & IN_FORWARD) != 0)
		{
			bool bWalk = (m_pOwner->pev->button & (IN_RUN | IN_USE)) != 0;
			float flOldFrame = pev->frame;
			int iOldSequence = pev->sequence;
			float flOldFramerate = pev->framerate;

			// VERIFY: literal, false when the old sequence is run (only a walk gets restored)
			bool bRestoreAnim = false;
			if (pev->sequence != LookupSequence("run"))
				bRestoreAnim = pev->sequence == LookupSequence("walk");

			if (bWalk)
			{
				if (pev->sequence != LookupSequence("walk"))
				{
					pev->sequence = LookupSequence("walk");
					ResetSequenceInfo();
					pev->frame = 0;
				}
			}
			else if (pev->sequence != LookupSequence("run"))
			{
				pev->sequence = LookupSequence("run");
				ResetSequenceInfo();
				pev->frame = 0;
				pev->framerate = 1.3;
			}

			CheckDoor();
			m_bCanIdle = false;

			UTIL_MakeVectors(pev->angles);
			Vector vecGoal = Center() + gpGlobals->v_forward * 512;
			float flMoved = 0;
			Vector vecOldOrigin = pev->origin;

			// something right in front: stand still
			Vector vecSrc = pev->origin + Vector(0, 0, 20);
			Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;
			TraceResult tr;
			UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr);
			if (tr.flFraction < 1.0 && !tr.fStartSolid)
			{
				if (bRestoreAnim)
				{
					pev->sequence = iOldSequence;
					ResetSequenceInfo();
					pev->frame = flOldFrame;
					pev->framerate = flOldFramerate;
				}
				else
				{
					m_fSequenceFinished = true;
				}
				m_bCanIdle = true;
				m_vecSudoVel = g_vecZero;
				UTIL_SetOrigin(pev, vecOldOrigin);
				return;
			}

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
							pev->framerate = flOldFramerate;
						}
						else
						{
							m_fSequenceFinished = true;
						}
						m_bCanIdle = true;
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
			float flOldIdealYaw = pev->ideal_yaw;
			pev->ideal_yaw = pev->angles.y;
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

			pev->ideal_yaw = flOldIdealYaw;
			m_bCanTurnAnim = false;
			m_fLastActiveTime = gpGlobals->time;
			m_fLastMoveTime = gpGlobals->time;
			return;
		}

		if (pev->sequence == LookupSequence("run") || pev->sequence == LookupSequence("walk"))
			m_fSequenceFinished = true;
	}

	m_vecSudoVel = g_vecZero;
	m_fLastMoveTime = gpGlobals->time;
}

// orig 0x170f60: the head follows the aim, the body turns toward ideal_yaw
void CDrivePanth::DoYaw()
{
	float flTarget;
	if (!m_bHeadTrack)
	{
		flTarget = 0;
	}
	else
	{
		float flDiff = UTIL_AngleMod(pev->ideal_yaw - pev->angles.y);
		if (flDiff < -180)
			flDiff += 360;
		else if (flDiff > 180)
			flDiff -= 360;

		if (flDiff < -55)
			flDiff = -55;
		else if (flDiff > 55)
			flDiff = 55;
		flTarget = flDiff;
	}

	m_flHeadYaw = UTIL_ApproachAngle(flTarget, m_flHeadYaw, m_flHeadYawSpeed * m_fFrameTime);
	m_flHeadYaw = SetBoneController(0, m_flHeadYaw);

	if (!m_bCanTurn)
		return;

	float flSpeed = 1900 * m_fFrameTime * m_fYawSpeedScale;
	bool bTurnAnim = false;
	float flDist = UTIL_AngleDistance(pev->ideal_yaw, pev->angles.y);

	if (fabs(flDist) < 0.001)
		return;

	if (fabs(flDist) > 80 && m_bCanTurnAnim && (pev->flags & FL_ONGROUND) != 0)
	{
		flSpeed = m_fFrameTime * 2800 * m_fYawSpeedScale;
		bTurnAnim = true;
	}

	if (bTurnAnim && pev->sequence == 0)
	{
		// VERIFY: literal, the original adds the time twice
		m_fNextIdleTime = gpGlobals->time + gpGlobals->time + RANDOM_FLOAT(3, 8);
		pev->sequence = LookupSequence(flDist > 0 ? "turn_left" : "turn_right");
		ResetSequenceInfo();
		pev->frame = 0;
		m_bCanIdle = false;
	}

	ChangeYaw((int)flSpeed);
}

// orig 0x1711d0: standing still heals
void CDrivePanth::DoRegen()
{
	if (PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthRegenStillnessTime", 6) < 0)
		return;

	if (PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthRegenStillnessTime", 6) + m_fLastActiveTime > gpGlobals->time)
		return;

	if (m_fNextRegen > gpGlobals->time)
		return;

	m_fNextRegen = PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthRegenDelayPerUnit", 1.2) + gpGlobals->time;
	pev->health = PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthRegenHealthPerUnit", pev->max_health / 100.0) + pev->health;
	if (pev->health > pev->max_health)
		pev->health = pev->max_health;
}

// orig 0x1712e0: the driver's HUD shows the panther's health in percent
void CDrivePanth::UpdateOwnerHUD()
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
	WRITE_SHORT(iHealth);
	MESSAGE_END();

	m_iClientHealth = (int)pev->health;
}

// orig 0x171400: a claw swipe lands (bRight: which claw's anim event)
void CDrivePanth::ClawDamage(bool bRight)
{
	float flDamage = PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthClawDamage", 75);
	if (pev->sequence == LookupSequence("attack_primary"))
		flDamage = PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthDoubleClawDamage", 45);

	Vector vecAngles = pev->angles;
	vecAngles.y = UTIL_AngleMod(pev->angles.y - m_flHeadYaw);
	UTIL_MakeAimVectors(vecAngles);

	Vector vecSrc = pev->origin;
	vecSrc.z += (pev->sequence == LookupSequence("attack_primary")) ? 36 : 28;
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 70;

	TraceResult tr;
	UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);

	if ((!tr.pHit || VARS(tr.pHit)->takedamage == 0) && pev->sequence != LookupSequence("attack_primary"))
	{
		// missed level: try again along the driver's pitch
		vecAngles.y = UTIL_AngleMod(pev->angles.y - m_flHeadYaw);
		if (pev->idealpitch < -40)
			vecAngles.x = -40;
		else if (pev->idealpitch > 20)
			vecAngles.x = 20;
		else
			vecAngles.x = pev->idealpitch;
		UTIL_MakeAimVectors(vecAngles);
		vecEnd = vecSrc + gpGlobals->v_forward * 70;
		UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);
	}

	if (tr.pHit && VARS(tr.pHit)->takedamage != 0)
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		entvars_t* pevAttacker = pev;
		if (m_pOwner)
			pevAttacker = m_pOwner->pev;

		pev->classname = MAKE_STRING("panther_claws");
		ClearMultiDamage();
		pHit->TraceAttack(pevAttacker, flDamage, gpGlobals->v_forward, &tr, DMG_SLASH);
		ApplyMultiDamage(pev, pevAttacker);
		pev->classname = MAKE_STRING("drive_panth");

		if (pHit->IsBSPModel())
		{
			TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, BULLET_NONE);
			DPEmitSound(this, CHAN_WEAPON, g_pDPAttackMissSounds[RANDOM_LONG(0, 1)], 1.0, 0.8);
		}
		else
		{
			int iCap = 0x100000; // VERIFY: RC cap bit, name unknown (takes claw knockback)
			if (pHit->HasRCCap(iCap))
			{
				pHit->pev->punchangle.z = -19;
				pHit->pev->punchangle.x = 7;
				float flPush = bRight ? 120 : -120;
				pHit->pev->velocity = pHit->pev->velocity + gpGlobals->v_right * flPush;

				if (pHit->IsPlayer())
				{
					float flYaw = RANDOM_FLOAT(20, 30);
					if (!bRight)
						flYaw = -flYaw;
					pHit->pev->angles.y = pHit->pev->angles.y + flYaw;
					pHit->pev->angles.x = RANDOM_FLOAT(-20, 20) + pHit->pev->angles.x;
					pHit->pev->angles.z = 0;
					pHit->pev->fixangle = 1;
				}
			}

			DPEmitSound(this, CHAN_WEAPON, g_pDPAttackHitSounds[RANDOM_LONG(0, 2)], 1.0, 0.8);
		}
	}
	else
	{
		DPEmitSound(this, CHAN_WEAPON, g_pDPAttackMissSounds[RANDOM_LONG(0, 1)], 1.0, 0.8);
	}

	m_bClawSide = bRight;
}

static const char* DPStepSound()
{
	switch (RANDOM_LONG(0, 3))
	{
	case 0:
		return "player/pl_step1.wav";
	case 1:
		return "player/pl_step3.wav";
	case 2:
		return "player/pl_step2.wav";
	case 3:
		return "player/pl_step4.wav";
	}
	return NULL;
}

// orig 0x171ab0
void CDrivePanth::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
		ClawDamage(true);
		break;

	case 2:
		ClawDamage(false);
		break;

	case 3:
	case 4:
		// footstep
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, DPStepSound(), 0.7, 0.8, 0, 100);
		break;

	case 5:
	case 6:
		// quiet footstep. VERIFY: the first entry of this jump table is misdisassembled; taken
		// to be pl_step1 like the other two tables
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, DPStepSound(), 0.3, 0.8, 0, 100);
		break;

	case 7:
		// the pounce leaves the ground
		UTIL_MakeVectors(pev->angles);
		pev->velocity = pev->velocity + gpGlobals->v_forward * PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthJumpForwardVel", 850);
		pev->velocity.z = PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthJumpZVel", 240) + pev->velocity.z;
		pev->flags &= ~FL_ONGROUND;
		SetTouch(&CDrivePanth::JumpTouch);

		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, DPStepSound(), 1.0, 0.8, 0, 100);
		DPEmitSound(this, CHAN_WEAPON, g_pDPStartPounceSounds[RANDOM_LONG(0, 1)], 1.0, 0.9);
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
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "zombie/claw_miss2.wav", 1.0, 0.8, 0, 100);
		break;
	}
}

// orig 0x171fa0: pick one by weight, play it, and return how long it should hold
float CDrivePanth::SetRandomAnim(sDPAnimT* pAnims, int iCount)
{
	float flTotal = 0;
	int i;
	for (i = 0; i < iCount; i++)
		flTotal += pAnims[i].flWeight;

	float flPick = RANDOM_FLOAT(0, flTotal);
	for (i = 0; i < iCount; i++)
	{
		if (flPick < pAnims[i].flWeight)
			break;
		flPick -= pAnims[i].flWeight;
	}
	if (i >= iCount)
		i = iCount - 1;

	pev->sequence = LookupSequence(pAnims[i].pszSequence);
	ResetSequenceInfo();
	pev->frame = 0;
	return RANDOM_FLOAT(pAnims[i].flMinTime, pAnims[i].flMaxTime);
}

// orig 0x172080
void CDrivePanth::DyingThink()
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

// orig 0x172140
void CDrivePanth::SetDeathAnim()
{
	const char* pszSeq = "death_simple";

	InitBoneControllers();

	if (pev->health < -15 && RANDOM_FLOAT(0, 1) < 0.5)
	{
		// room in front to drop forward
		UTIL_MakeVectors(pev->angles);
		Vector vecSrc = pev->origin + Vector(0, 0, 18);
		Vector vecEnd = vecSrc + gpGlobals->v_forward * 100;
		TraceResult tr;
		UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);
		if (tr.flFraction >= 1.0)
			pszSeq = "death_drop";
	}

	pev->sequence = LookupSequence(pszSeq);
	ResetSequenceInfo();
	pev->frame = 0;
}

// orig 0x1722e0
bool CDrivePanth::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_pOwner)
	{
		CBaseEntity* pAttacker = CBaseEntity::Instance(pevAttacker->pContainingEntity);
		if (pAttacker && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, pAttacker))
			return false;
		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	if ((bitsDamageType & DMG_SONIC) != 0)
		flDamage *= 0.33;
	if ((bitsDamageType & DMG_RADIATION) != 0)
		flDamage *= 0.15;

	if (m_fNextPainSound < gpGlobals->time && flDamage < pev->health)
	{
		m_fNextPainSound = RANDOM_FLOAT(0.9, 1.8) + gpGlobals->time;
		DPEmitSound(this, CHAN_VOICE, g_pDPPainSounds[RANDOM_LONG(0, 2)], 1.0, 0.8);
	}

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x1724c0: the panther dies, and the driver with it
void CDrivePanth::Killed(entvars_t* pevAttacker, int iGib)
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

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_BYTE(4);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_SHORT(0);
		MESSAGE_END();
	}

	// tell the pickup it can respawn
	// VERIFY: guard, the original crashes here: it calls through Instance()'s result unchecked
	// (NULL once the pickup's edict is freed)
	if (pev->owner && CBaseEntity::Instance(pev->owner))
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	pev->classname = MAKE_STRING("alien_slave"); // VERIFY: the original's own string, kept from CDriveASlave
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
		DPEmitSound(this, CHAN_VOICE, g_pDPDeathSounds[RANDOM_LONG(0, 1)], 1.0, 0.8);
		SetDeathAnim();
		SetThink(&CDrivePanth::DyingThink);
		pev->nextthink = gpGlobals->time + 0.1;
		pev->dmgtime = gpGlobals->time + 5;
	}
}

// orig 0x172870: from the driver's PreThink: steer, and put his camera behind the panther
void CDrivePanth::DEClientPreThink()
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

	Vector vecEnd = vecOrg - gpGlobals->v_forward * 160;
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

	// the client's driveentalpha: the panther drawn see-through for its driver
	float flAlpha = m_pOwner->m_fDrvEntAlpha;
	if (flAlpha < 1 && flAlpha > 0)
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
		m_pOwner->pev->fuser3 = 0;
		m_pOwner->pev->vuser1 = g_vecZero;
	}
}

// orig 0x172da0
void CDrivePanth::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x172dc0: the player gets in
void CDrivePanth::DEClientEnter(CBasePlayer* pPlayer)
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

// orig 0x173170: the player gets out (or dies with the panther): make him a player again
void CDrivePanth::DEClientLeave()
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

// orig 0x173480: the driver asks out; he lands on top and the panther dies
void CDrivePanth::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVEPANTH", "AllowEject", false))
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

// orig 0x173690
LINK_ENTITY_TO_CLASS(pickup_drive_panth, CDrivePanthPickup);

TYPEDESCRIPTION CDrivePanthPickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDrivePanthPickup, m_pGiven, FIELD_CLASSPTR),
		DEFINE_FIELD(CDrivePanthPickup, m_bTakeable, FIELD_BOOLEAN),
};

// orig 0x1736c0 (Save), 0x173710 (Restore)
IMPLEMENT_SAVERESTORE(CDrivePanthPickup, CBaseMonster);

// orig 0x173760: sits on an info_drive_panth_spawn, which it removes
void CDrivePanthPickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), "models/panther.mdl");
	UTIL_SetSize(pev, Vector(-18, -18, 0), Vector(18, 18, 44));
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->sequence = 0;
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);
	m_fSequenceLoops = true;
	InitBoneControllers();

	SetUse(&CDrivePanthPickup::Use);
	SetThink(&CDrivePanthPickup::PAnimThink);
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

// orig 0x173990
void CDrivePanthPickup::Precache()
{
	PRECACHE_MODEL("models/panther.mdl");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_panth");
}

// orig 0x1739e0: the first spawn spot not already being removed
CBaseEntity* CDrivePanthPickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_panth_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (FNullEnt(pSpot->pev))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x173a60
bool CDrivePanthPickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return false;
}

// orig 0x173a70: fades the ghost in; while given out, ends the drive when its time is up
void CDrivePanthPickup::PAnimThink()
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

// orig 0x173c10: a player uses the ghost: make a drive_panth and put him in it
void CDrivePanthPickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
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

	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_panth", pev->origin, pev->angles, ENT(pev));
	pPlayer->pev->angles = Vector(0, pev->angles.y, 0);
	pPlayer->pev->v_angle = pPlayer->pev->angles;
	pPlayer->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGiven = pPlayer->m_pCurDriveEnt;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDrivePanthPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthMaxDriveTime", 600) + gpGlobals->time;
}

// orig 0x173e10
void CDrivePanthPickup::DoRespawn()
{
	m_bTakeable = true;
	m_pGiven = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);
	SetThink(&CDrivePanthPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x173ed0: the drive_panth it made has died
void CDrivePanthPickup::DeathNotice(entvars_t* pevChild)
{
	if (m_bTakeable)
		return;

	pev->nextthink = PREFS_KeyLoopUpFloat("DRIVEPANTH", "PanthRespawnDelayAfterDeath", 5) + gpGlobals->time;
	SetThink(&CDrivePanthPickup::DoRespawn);
}

// orig 0x173f40: in multiplayer, one drive_panth_spawner per map
void CreateDrvPanthSpawner()
{
	if (!g_pGameRules->IsMultiplayer())
		return;

	if (!UTIL_FindEntityByClassname(NULL, "drive_panth_spawner"))
		CBaseEntity::Create("drive_panth_spawner", g_vecZero, g_vecZero, NULL);
}

// orig 0x173fa0
LINK_ENTITY_TO_CLASS(drive_panth_spawner, CDrvPanthSpawner);

TYPEDESCRIPTION CDrvPanthSpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvPanthSpawner, m_bDummy, FIELD_BOOLEAN),
};

// orig 0x173fd0 (Save), 0x174020 (Restore)
IMPLEMENT_SAVERESTORE(CDrvPanthSpawner, CBaseEntity);

// orig 0x174070: makes mp_drivepanth pickups, then goes
void CDrvPanthSpawner::DrvAThink()
{
	int iCount = (int)CVAR_GET_FLOAT("mp_drivepanth");
	for (int i = 0; i < iCount; i++)
		CBaseEntity::Create("pickup_drive_panth", g_vecZero, g_vecZero, NULL);

	UTIL_Remove(this);
}

// orig 0x174100
void CDrvPanthSpawner::Spawn()
{
	pev->classname = MAKE_STRING("drive_panth_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvPanthSpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15;
}

// orig 0x1741a0
int CDrvPanthSpawner::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}

// orig 0x174200
int CDrivePanthPickup::ObjectCaps()
{
	return m_bTakeable ? (FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE) : FCAP_ACROSS_TRANSITION;
}

// orig 0x174220
int CDrivePanthPickup::GetRCCaps()
{
	return 0;
}

// orig 0x174230
bool CDrivePanthPickup::IsTeleFraggabe()
{
	return false;
}

// orig 0x174240
int CDrivePanthPickup::BloodColor()
{
	return DONT_BLEED;
}

// orig 0x174250
int CDrivePanthPickup::Classify()
{
	return CLASS_NONE;
}

// orig 0x1742b0
void CDrivePanth::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;

	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

// orig 0x174330
int CDrivePanth::GetRCCaps()
{
	return 0x7fe3dff;
}

// orig 0x174340
int CDrivePanth::GetGuassDistOut()
{
	return 56;
}

// orig 0x174350
int CDrivePanth::GetGuassHull()
{
	return human_hull;
}

// orig 0x174360
Vector CDrivePanth::GetCamRotOrg()
{
	return pev->origin + Vector(0, 0, 64);
}

// orig 0x1743c0
int CDrivePanth::BloodColor()
{
	return BLOOD_COLOR_YELLOW;
}

// orig 0x1743d0
int CDrivePanth::Classify()
{
	return CLASS_ALIEN_MONSTER;
}


