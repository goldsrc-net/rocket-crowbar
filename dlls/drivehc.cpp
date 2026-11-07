/***
*
*	Rocket Crowbar 1.9: drivehc.cpp. The headcrab a player drives (drive_headcrab): it runs,
*	and leaps at things with a jump charged by holding attack or jump. Also the "headcrabify"
*	hit of the gonarch's headcrabifier (DHCHeadcrabifyEnt) and the effects both it and the
*	chumtoadifier use.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "client.h"
#include "UserMessages.h"
#include "driveent.h"

// Preff.cpp (rc.ini server prefs)
extern float PREFS_KeyLoopUpFloat(const char* pszHeading, const char* pszKey, float flDefault);
extern bool PREFS_KeyLoopUpBool(const char* pszHeading, const char* pszKey, bool bDefault);

const char* g_szDHCDieSounds[] = {
	"headcrab/hc_die1.wav",
	"headcrab/hc_die2.wav",
};

const char* g_szDHCIdleSounds[] = {
	"headcrab/hc_idle1.wav",
	"headcrab/hc_idle2.wav",
	"headcrab/hc_idle3.wav",
};

const char* g_szDHCPainSounds[] = {
	"headcrab/hc_pain1.wav",
	"headcrab/hc_pain2.wav",
	"headcrab/hc_pain3.wav",
};

const char* g_szDHCAttackSounds[] = {
	"headcrab/hc_attack1.wav",
	"headcrab/hc_attack2.wav",
	"headcrab/hc_attack3.wav",
};

// An idle animation: picked with probability flWeight, plays again after flMin to flMax seconds.
struct sDHCAnimT
{
	const char* pszSequence;
	float flWeight;
	float flMin;
	float flMax;
};

sDHCAnimT g_sDHCIdleAnims[] = {
	{"idle2", 0.4, 8, 16},
	{"idle3", 0.25, 10, 20},
};

// sprites for the headcrabify effect, precached by CDriveHC
int g_iHCTransFlare;
int g_iHCTransBalls;

class CDriveHC : public CBaseDriveEnt
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int BloodColor() override;
	int GetGuassHull() override;
	int GetGuassDistOut() override;
	const char* GetControllerName() override;
	void EntIsBeingRemoved(edict_t* pEdict) override;
	int GetRCCaps() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;

	void DEClientPreThink() override;
	void DEClientPostThink() override;
	void DEClientEnter(CBasePlayer* pPlayer) override;
	void DEClientLeave() override;
	void DEClientEject() override;
	Vector GetCamRotOrg() override;

	void EXPORT DHCThink();
	void EXPORT JumpTouch(CBaseEntity* pOther);
	void DoJump();
	void CheckDoor();
	void CheckEHop();
	void RunWaterPhysics();
	void DoMovement();
	void DoYaw();
	void UpdateOwnerHUD();
	float SetRandomAnim(sDHCAnimT* pAnims, int iCount);
	void EXPORT DyingThink();
	void SetDeathAnim();

	// Unsaved (no save table in the original). m_pOwner, m_fFrameTime and the CDriveAGrunt-style
	// names are what the other drive monsters' save tables call the same things.
	// m_pOwner (orig +0x27c, the driving player) is CBaseDriveEnt's
	float m_fFrameTime;		  // orig +0x280
	Vector m_vecSudoVel;	  // orig +0x284, how fast the last run moved (camera lead)
	float m_fLastMoveTime;	  // orig +0x290
	float m_fNextEHopCheck;	  // orig +0x294
	float m_fNextDoorCheck;	  // orig +0x298
	bool m_bCanIdle;		  // orig +0x29c, idle animations may play. VERIFY name
	bool m_bCanTurnAnim;	  // orig +0x29d, a fast turn may play 180_left/right. VERIFY name
	bool m_bCanMove;		  // orig +0x29e. VERIFY name
	float m_fNextIdleAnim;	  // orig +0x2a0. VERIFY name
	float m_fNextPainSound;	  // orig +0x2a4
	float m_fNextIdleSound;	  // orig +0x2a8. VERIFY name
	float m_fYawSpeedScale;	  // orig +0x2ac. VERIFY name
	bool m_bInJump;			  // orig +0x2b0
	float m_fJumpTimeout;	  // orig +0x2b4, a jump ends by this time. VERIFY name
	float m_fJumpChargeStart; // orig +0x2b8, when the jump button went down, -1 when not charging. VERIFY name
	float m_fJumpVel;		  // orig +0x2bc, the charged leap speed. VERIFY name
	float m_fNextJump;		  // orig +0x2c0. VERIFY name
	float m_fJumpStartTime;	  // orig +0x2c4. VERIFY name
	int m_iClientHealth;	  // orig +0x2c8
};

LINK_ENTITY_TO_CLASS(drive_headcrab, CDriveHC); // orig 0x15d4d0

// orig 0x15d500
void CDriveHC::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_headcrab");
	SET_MODEL(ENT(pev), "models/headcrab.mdl");
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 24));
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->effects = 0;
	pev->friction = 1.0;
	pev->health = PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabHealth", 60.0);
	pev->max_health = pev->health;
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	pev->sequence = LookupSequence("idle1");
	ResetSequenceInfo();
	pev->frame = 0;
	pev->framerate = 1.0;
	SetEyePosition();

	m_bInJump = false;
	m_fJumpChargeStart = -1;
	m_fNextJump = gpGlobals->time;
	m_fLastMoveTime = gpGlobals->time;
	m_vecSudoVel = g_vecZero;
	m_fNextIdleAnim = RANDOM_FLOAT(3, 8) + gpGlobals->time;
	m_fNextIdleSound = RANDOM_FLOAT(4, 7) + gpGlobals->time;
	m_fNextPainSound = gpGlobals->time;
	m_fNextEHopCheck = gpGlobals->time;
	m_fNextDoorCheck = gpGlobals->time;

	SetThink(&CDriveHC::DHCThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x15d790
void CDriveHC::Precache()
{
	PRECACHE_MODEL("models/headcrab.mdl");
	for (const char* psz : g_szDHCDieSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_szDHCIdleSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_szDHCPainSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_szDHCAttackSounds)
		PRECACHE_SOUND(psz);
	PRECACHE_SOUND("headcrab/hc_headbite.wav");
	g_iHCTransFlare = PRECACHE_MODEL("sprites/c-tele1.spr");
	g_iHCTransBalls = PRECACHE_MODEL("sprites/hotglow.spr");
	PRECACHE_SOUND("debris/beamstart7.wav");
}

// orig 0x15d8a0
const char* CDriveHC::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;
	return STRING(m_pOwner->pev->netname);
}

// orig 0x15d910
void CDriveHC::DHCThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_pOwner)
	{
		bool bGone = FNullEnt(m_pOwner->pev);
		if (!bGone && m_pOwner->pev && m_pOwner->pev->pContainingEntity && m_pOwner->pev->pContainingEntity->pvPrivateData && !m_pOwner->pev->pContainingEntity->free && UTIL_PlayerByIndex(ENTINDEX(m_pOwner->pev->pContainingEntity)))
		{
			if (pev->waterlevel > 1)
				pev->flags &= ~FL_ONGROUND;
			DispatchAnimEvents(m_fFrameTime);

			if ((pev->flags & FL_FROZEN) != 0)
			{
				pev->sequence = LookupSequence("idle1");
				ResetSequenceInfo();
				pev->frame = 0;
				m_vecSudoVel = g_vecZero;
				m_fLastMoveTime = gpGlobals->time;
				return;
			}

			m_pOwner->pev->rendermode = kRenderTransColor;
			m_pOwner->pev->renderamt = 0;
			m_pOwner->pev->effects |= EF_NODRAW;
			m_bCanIdle = true;
			m_bCanMove = true;
			m_bCanTurnAnim = true;
			m_fYawSpeedScale = 1.0;
			UpdateOwnerHUD();
			DoJump();
			DoMovement();
			DoYaw();
			pev->fuser4 = m_vecSudoVel.Length();

			if (!m_bCanIdle)
			{
				m_fNextIdleAnim = RANDOM_FLOAT(3, 7) + gpGlobals->time;
				m_fNextIdleSound = RANDOM_FLOAT(4, 7) + gpGlobals->time;
				return;
			}

			if (m_fNextIdleSound < gpGlobals->time)
			{
				m_fNextIdleSound = RANDOM_FLOAT(4, 7) + gpGlobals->time;
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_szDHCIdleSounds[RANDOM_LONG(0, 2)], 0.7, 1.1, 0, 100);
			}

			if (!m_fSequenceFinished)
				return;

			if (m_fNextIdleAnim < gpGlobals->time)
			{
				m_fNextIdleAnim = SetRandomAnim(g_sDHCIdleAnims, 2) + gpGlobals->time;
				return;
			}

			if (pev->sequence == LookupSequence("idle1"))
				return;
			pev->sequence = LookupSequence("idle1");
			ResetSequenceInfo();
			pev->frame = 0;
			return;
		}
	}

	// the driver is gone
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("headcrab");
	g_pevLastInflictor = pev;
	Killed(pev, GIB_NORMAL);
}

// orig 0x15dcd0: the leap hits something
void CDriveHC::JumpTouch(CBaseEntity* pOther)
{
	TraceResult tr = UTIL_GetGlobalTrace();
	if (tr.pHit != pOther->pev->pContainingEntity)
		return;

	SetTouch(NULL);
	m_bInJump = false;
	m_fNextJump = PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabJumpRefire", 0.7) + gpGlobals->time;

	if (pOther->pev->takedamage == DAMAGE_NO)
		return;
	if (!strcmp(STRING(pOther->pev->classname), "drive_bm"))
		return;

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "headcrab/hc_headbite.wav", 1.0, 0.9, 0, 100);

	// kills by the leap read "headcrab_leap"
	pev->classname = MAKE_STRING("headcrab_leap");
	if (!m_pOwner)
		pOther->TakeDamage(pev, pev, PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabJumpDamage", 40.0), DMG_SLASH);
	else
		pOther->TakeDamage(pev, m_pOwner->pev, PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabJumpDamage", 40.0), DMG_SLASH);
	pev->classname = MAKE_STRING("drive_headcrab");

	// a bite heals it
	pev->health = PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabHealthPerBite", 20.0) + pev->health;
	if (pev->health > pev->max_health)
		pev->health = pev->max_health;
}

// orig 0x15dec0: hold attack or jump to charge a leap, release to leap. The leap itself starts
// on the jump animation's event (HandleAnimEvent).
void CDriveHC::DoJump()
{
	if (m_bInJump)
	{
		if (!(m_fJumpTimeout < gpGlobals->time))
		{
			if (!(m_fJumpStartTime + 0.4 < gpGlobals->time) || pev->waterlevel == 0)
			{
				m_bCanIdle = false;
				m_bCanMove = false;
				m_bCanTurnAnim = false;
				if ((pev->flags & FL_ONGROUND) != 0)
					m_fYawSpeedScale = 0.8;
				else
					m_fYawSpeedScale = 0.4;
				return;
			}
		}

		m_bInJump = false;
		m_fJumpChargeStart = -1;
		m_fNextJump = PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabJumpRefire", 0.7) + gpGlobals->time;
		SetTouch(NULL);
		return;
	}

	if ((m_pOwner->pev->button & (IN_ATTACK | IN_JUMP)) != 0)
	{
		if (m_fJumpChargeStart == -1)
		{
			m_fJumpChargeStart = gpGlobals->time;
			return;
		}
		// the original checks the buttons again here; still held, so it keeps charging
		if ((m_pOwner->pev->button & (IN_ATTACK | IN_JUMP)) != 0)
			return;
	}

	if (((pev->flags & FL_ONGROUND) == 0 && pev->waterlevel == 0) || gpGlobals->time < m_fNextJump)
		m_fJumpChargeStart = -1;

	if (m_fJumpChargeStart == -1)
		return;

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		pev->sequence = LookupSequence("jump");
		break;
	case 1:
		pev->sequence = LookupSequence("jump_variation1");
		break;
	case 2:
		pev->sequence = LookupSequence("jump_variation2");
		break;
	}
	ResetSequenceInfo();
	pev->frame = 0;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_szDHCAttackSounds[RANDOM_LONG(0, 2)], 1.0, 0.9, 0, 100);

	// the longer the charge, the faster the leap (not capped at the max)
	float flMaxVel = PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabMaxJumpVel", 1200.0);
	float flMinVel = PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabMinJumpVel", 600.0);
	float flChargeTime = PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabMinJumpChargeTime", 4.0);
	m_fJumpVel = (flMaxVel - flMinVel) * ((gpGlobals->time - m_fJumpChargeStart) / flChargeTime) + flMinVel;

	m_bInJump = true;
	m_bCanIdle = false;
	m_bCanMove = false;
	m_bCanTurnAnim = false;
	m_fJumpTimeout = gpGlobals->time + 4.0;
	m_fJumpChargeStart = -1;
	m_fJumpStartTime = gpGlobals->time;
}

// orig 0x15e1d0: running into a door opens it (touches it)
void CDriveHC::CheckDoor()
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

	if (tr.pHit && (!strcmp(STRING(VARS(tr.pHit)->classname), "func_door") || !strcmp(STRING(VARS(tr.pHit)->classname), "func_door_rotating")))
	{
		CBaseEntity* pDoor = CBaseEntity::Instance(tr.pHit);
		if (pDoor)
			pDoor->Touch(this);
	}

	m_fNextDoorCheck = 0.3 + gpGlobals->time;
}

// orig 0x15e420: hop up a small ledge in front
void CDriveHC::CheckEHop()
{
	if (m_fNextEHopCheck > gpGlobals->time || !m_bCanMove)
		return;
	if (pev->velocity.z > 16)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecFwd = pev->origin + Vector(0, 0, 4) + gpGlobals->v_forward * 28;
	Vector vecDown = vecFwd - Vector(0, 0, 24);
	TraceResult tr;
	UTIL_TraceLine(pev->origin + Vector(0, 0, 4), vecFwd, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
	{
		UTIL_TraceLine(vecFwd, vecDown, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
		{
			pev->velocity.z = pev->velocity.z + 50;
			pev->velocity = pev->velocity + gpGlobals->v_forward * 100;
			pev->flags &= ~FL_ONGROUND;
			UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
		}
	}
	m_fNextEHopCheck = gpGlobals->time + 0.2;
}

// orig 0x15e730
void CDriveHC::RunWaterPhysics()
{
	float flFriction = PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabWaterFriction", 8.0);
	float flControl = 6.0;
	m_fYawSpeedScale = 0.7;

	float flSpeed = pev->velocity.Length();
	if (flSpeed > 0.1)
	{
		if (flSpeed >= flControl)
			flControl = flSpeed;
		float flNewSpeed = flSpeed - flControl * flFriction * m_fFrameTime;
		if (flNewSpeed < 0)
			flNewSpeed = 0;
		pev->velocity = pev->velocity * (flNewSpeed / flSpeed);
	}

	// it floats up
	if (pev->waterlevel > 1)
	{
		float flGravity = 1.1 * CVAR_GET_FLOAT("sv_gravity");
		if (pev->gravity > 0)
			flGravity = flGravity * pev->gravity;
		pev->velocity.z = flGravity * m_fFrameTime + pev->velocity.z;
	}

	if (!m_bCanMove)
		return;

	if (pev->sequence != LookupSequence("walk"))
	{
		pev->sequence = LookupSequence("walk");
		ResetSequenceInfo();
	}

	float flRate;
	if (pev->velocity.Length2D() > 0.1)
		flRate = pev->velocity.Length2D() / 30;
	else
		flRate = 1.5;
	if (flRate >= 1.5)
	{
		if (flRate > 1.8)
			pev->framerate = 1.8;
		else
			pev->framerate = flRate;
	}
	else
	{
		pev->framerate = 1.5;
	}

	if ((m_pOwner->pev->button & IN_FORWARD) != 0)
	{
		UTIL_MakeAimVectors(pev->angles);
		float flMaxSpeed = PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabWaterSpeed", 100.0);
		float flAccel = PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabWaterAccel", 90.0);
		Vector vecDir = gpGlobals->v_forward;
		float flAdd = flMaxSpeed - DotProduct(pev->velocity, vecDir);
		if (flAdd > 0)
		{
			float flAccelSpeed = flAccel * m_fFrameTime * flMaxSpeed;
			if (flAccelSpeed > flAdd)
				flAccelSpeed = flAdd;
			pev->velocity = pev->velocity + vecDir * flAccelSpeed;
			m_fJumpChargeStart = -1;
		}
	}
}

// orig 0x15eaa0
void CDriveHC::DoMovement()
{
	if ((pev->flags & FL_ONGROUND) == 0)
	{
		if (pev->waterlevel != 0)
		{
			RunWaterPhysics();
		}
		else if (m_bCanIdle)
		{
			// in the air: VERIFY (literal) runs between 30 and 70 units/s, walks from 70 up
			float flSpeed = pev->velocity.Length2D();
			const char* pszSequence = NULL;
			if (flSpeed > 30 && flSpeed < 70)
			{
				if (pev->sequence != LookupSequence("run"))
					pszSequence = "run";
			}
			else if (flSpeed >= 70)
			{
				if (pev->sequence != LookupSequence("walk"))
					pszSequence = "walk";
			}
			if (pszSequence)
			{
				pev->sequence = LookupSequence(pszSequence);
				ResetSequenceInfo();
				pev->frame = 0;
			}
		}
	}
	else if (m_bCanMove)
	{
		if ((m_pOwner->pev->button & IN_FORWARD) != 0)
		{
			bool bWalk = (m_pOwner->pev->button & (IN_USE | IN_RUN)) != 0;
			float flOldFrame = pev->frame;
			int iOldSequence = pev->sequence;
			bool bWasOther = false; // neither running nor walking before
			if (pev->sequence != LookupSequence("run"))
				bWasOther = pev->sequence != LookupSequence("walk");

			if (bWalk)
			{
				if (pev->sequence != LookupSequence("walk"))
				{
					pev->sequence = LookupSequence("walk");
					ResetSequenceInfo();
					pev->frame = 0;
				}
				pev->framerate = 1.5;
			}
			else
			{
				if (pev->sequence != LookupSequence("run"))
				{
					pev->sequence = LookupSequence("run");
					ResetSequenceInfo();
					pev->frame = 0;
				}
				pev->framerate = 3.0;
			}

			CheckDoor();
			m_bCanIdle = false;

			UTIL_MakeVectors(pev->angles);
			Vector vecGoal = Center() + gpGlobals->v_forward * 512;
			float flMoved = 0;
			Vector vecStart = pev->origin;
			float flDist = m_flGroundSpeed * pev->framerate * m_fFrameTime;

			// how far can it run this frame (WALKMOVE_CHECKONLY, then put back)
			while (flDist > 0.001)
			{
				float flStep = flDist;
				if (flStep > 16)
					flStep = 16;
				Vector vecPrev = pev->origin;
				while (!WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY) && flStep > 2)
					flStep -= 2;
				UTIL_SetOrigin(pev, vecPrev);

				if (!WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY))
				{
					if (flMoved < 0.01)
					{
						// blocked: stop
						if (bWasOther)
						{
							pev->sequence = iOldSequence;
							ResetSequenceInfo();
							pev->frame = flOldFrame;
						}
						else
						{
							m_fSequenceFinished = true;
						}
						m_bCanIdle = true;
						m_vecSudoVel = g_vecZero;
						UTIL_SetOrigin(pev, vecStart);
						if (!bWalk)
							CheckEHop();
						return;
					}
					break;
				}
				flDist -= flStep;
				flMoved += flStep;
			}

			UTIL_SetOrigin(pev, vecStart);
			Vector vecOld = pev->origin;
			float flIdealYaw = pev->ideal_yaw;
			pev->ideal_yaw = pev->angles.y;
			while (flMoved > 0.001)
			{
				float flStep = flMoved;
				if (flStep > 16)
					flStep = 16;
				UTIL_MoveToOrigin(ENT(pev), vecGoal, flStep, MOVE_NORMAL);
				flMoved -= flStep;
			}

			Vector vecDelta = pev->origin - vecOld;
			if (vecDelta.Length() > 0.01 && m_fFrameTime > 0.001)
				m_vecSudoVel = vecDelta / m_fFrameTime;
			else
				m_vecSudoVel = g_vecZero;
			pev->ideal_yaw = flIdealYaw;
			m_bCanTurnAnim = false;
			m_fYawSpeedScale = 0.8;
			m_fLastMoveTime = gpGlobals->time;
			m_fJumpChargeStart = -1;
			return;
		}

		if (pev->sequence == LookupSequence("run") || pev->sequence == LookupSequence("walk"))
			m_fSequenceFinished = true;
	}

	m_vecSudoVel = g_vecZero;
	m_fLastMoveTime = gpGlobals->time;
}

// orig 0x15f150: a sharp turn standing still plays the 180 animations
void CDriveHC::DoYaw()
{
	float flSpeed = m_fFrameTime * 1900 * m_fYawSpeedScale;
	bool bTurnAnim = false;
	float flDelta = UTIL_AngleDistance(pev->ideal_yaw, pev->angles.y);
	if (fabs(flDelta) < 0.001)
		return;

	if (fabs(flDelta) > 70 && m_bCanTurnAnim && (pev->flags & FL_ONGROUND) != 0)
	{
		flSpeed = m_fFrameTime * 2800 * m_fYawSpeedScale;
		bTurnAnim = true;
	}

	if (bTurnAnim && pev->sequence == LookupSequence("idle1"))
	{
		// the original adds the time twice
		m_fNextIdleAnim = gpGlobals->time + gpGlobals->time + RANDOM_FLOAT(3, 8);
		pev->sequence = LookupSequence(flDelta > 0 ? "180_left" : "180_right");
		ResetSequenceInfo();
		pev->frame = 0;
		m_bCanIdle = false;
	}

	ChangeYaw((int)flSpeed);
}

// orig 0x15f2e0: the driver's HUD shows the headcrab's health in percent
void CDriveHC::UpdateOwnerHUD()
{
	if (!m_pOwner)
		return;
	if (pev->health == (float)m_iClientHealth)
		return;

	float flPercent = (pev->health / pev->max_health) * 100;
	int iHealth = flPercent > 0 ? (int)flPercent : 0;
	if (iHealth < 1)
		iHealth = 1;
	else if (iHealth > 100)
		iHealth = 100;

	MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_SHORT(iHealth);
	MESSAGE_END();
	m_iClientHealth = (int)pev->health;
}

// orig 0x15f400: play one of iCount weighted animations; returns how long until the next
float CDriveHC::SetRandomAnim(sDHCAnimT* pAnims, int iCount)
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
	return RANDOM_FLOAT(pAnims[i].flMin, pAnims[i].flMax);
}

// orig 0x15f4e0
void CDriveHC::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 2001:
		if ((pev->flags & FL_ONGROUND) == 0)
			return;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, RANDOM_LONG(0, 1) == 0 ? "common/bodydrop3.wav" : "common/bodydrop4.wav", 1.0, 0.8, 0, 100);
		break;

	case 2002:
		if ((pev->flags & FL_ONGROUND) == 0)
			return;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, RANDOM_LONG(0, 1) == 0 ? "common/bodydrop3.wav" : "common/bodydrop4.wav", 1.0, 0.8, 0, 90);
		break;

	case 2010:
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "zombie/claw_miss2.wav", 1.0, 0.8, 0, 100);
		break;

	case 2: // the leap
	{
		if (!m_pOwner)
			return;
		if ((pev->flags & FL_ONGROUND) == 0 && pev->waterlevel == 0)
		{
			pev->sequence = LookupSequence("idle1");
			ResetSequenceInfo();
			return;
		}

		pev->flags &= ~FL_ONGROUND;
		UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));

		// leap along the driver's aim, pitched up 10 to 80 degrees
		Vector vecAngles = Vector(0, pev->angles.y, pev->angles.z);
		float flPitch = -fabs(m_pOwner->pev->v_angle.x);
		if (flPitch >= -80)
		{
			if (flPitch > -10)
				vecAngles.x = -10;
			else
				vecAngles.x = -fabs(m_pOwner->pev->v_angle.x);
		}
		else
		{
			vecAngles.x = -80;
		}
		UTIL_MakeVectors(vecAngles);

		if (pev->waterlevel == 0)
			pev->velocity = pev->velocity + gpGlobals->v_forward * m_fJumpVel;
		else
			pev->velocity = pev->velocity + (gpGlobals->v_forward * m_fJumpVel) * PREFS_KeyLoopUpFloat("DRIVEHEADCRAB", "CrabJumpWaterScale", 0.5);

		SetTouch(&CDriveHC::JumpTouch);
		break;
	}
	}
}

// orig 0x15f880
void CDriveHC::DyingThink()
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

// orig 0x15f940
void CDriveHC::SetDeathAnim()
{
	pev->sequence = LookupSequence("dieback");
	ResetSequenceInfo();
	pev->frame = 0;
}

// orig 0x15f990
bool CDriveHC::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_pOwner)
	{
		if (CBaseEntity::Instance(pevAttacker->pContainingEntity) && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, CBaseEntity::Instance(pevAttacker->pContainingEntity)))
			return false;
		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	if ((bitsDamageType & DMG_ACID) != 0 || (bitsDamageType & DMG_RADIATION) != 0)
		return false;

	if (m_fNextPainSound < gpGlobals->time && flDamage < pev->health)
	{
		m_fNextPainSound = RANDOM_FLOAT(0.6, 1.2) + gpGlobals->time;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_szDHCPainSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, 100);
	}

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x15fb30
void CDriveHC::Killed(entvars_t* pevAttacker, int iGib)
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
		WRITE_BYTE(HIDEHUD_HEALTH);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_SHORT(0);
		MESSAGE_END();
	}

	if (pev->owner)
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	pev->classname = MAKE_STRING("headcrabeye");
	DEClientLeave();

	// the driver dies with it
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
		return;
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_szDHCDieSounds[RANDOM_LONG(0, 1)], 1.0, 0.8, 0, 100);
	SetDeathAnim();
	SetThink(&CDriveHC::DyingThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = 5.0 + gpGlobals->time;
}

// orig 0x15fec0: the driver's camera, behind the headcrab
void CDriveHC::DEClientPreThink()
{
	// holding reload frees the camera from steering
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

	Vector vecEnd = vecOrg - gpGlobals->v_forward * 120;
	TraceResult tr;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine(vecOrg, vecEnd, ignore_monsters, ENT(pev), &tr);

	Vector vecCam = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;

	// out of water, pull the camera in until it is in open air
	int i = 0;
	if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
	{
		for (i = 1; i < 48; i++)
		{
			if (UTIL_PointContents(vecCam) == CONTENTS_EMPTY)
				break;
			vecCam = tr.vecEndPos + gpGlobals->v_forward * (i * 8);
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

	// cl_de_alpha: the driver can see the headcrab translucent. Unlike the chumtoad's, the
	// opaque case keeps euser2 set and leaves fuser2/fuser3/vuser1 alone.
	float flAlpha = m_pOwner->m_fDrvEntAlpha;
	if (flAlpha >= 1.0 || !(flAlpha > 0))
	{
		m_pOwner->pev->euser2 = ENT(pev);
		m_pOwner->pev->iuser3 = 0;
		m_pOwner->pev->fuser1 = 0;
	}
	else
	{
		m_pOwner->pev->euser2 = ENT(pev);
		m_pOwner->pev->iuser3 = 4;
		m_pOwner->pev->fuser1 = 2.0;
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

// orig 0x160420
void CDriveHC::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x160440: the player becomes an invisible camera; the headcrab takes its input
void CDriveHC::DEClientEnter(CBasePlayer* pPlayer)
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
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 5.0;
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

// orig 0x1607f0: give the player back its body
void CDriveHC::DEClientLeave()
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
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 20.0;
	m_pOwner->m_fDrcEntTransTime = gpGlobals->time + 15.0;
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

// orig 0x160b00: the driver asks to get out; the headcrab gibs
void CDriveHC::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVEHEADCRAB", "AllowEject", false))
	{
		if (CVAR_GET_FLOAT("sv_allowglobaleject") != 1 && g_pGameRules->IsMultiplayer() && !g_pGameRules->IsCoOp())
			return;
	}

	if (!m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev || !(m_pOwner->m_pCurDriveEnt->pev->health > 0))
		return;
	if (!(m_pOwner->m_fDrvEntEjectTime < gpGlobals->time))
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

// orig 0x160d10: the flash and sparks of a headcrabify (or chumtoadify)
void DHHeadcrabifyEffects(Vector vecOrigin)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecOrigin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(vecOrigin.x);
	WRITE_COORD(vecOrigin.y);
	WRITE_COORD(vecOrigin.z);
	WRITE_SHORT(g_iHCTransFlare);
	WRITE_BYTE(16); // scale
	WRITE_BYTE(30); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecOrigin);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(vecOrigin.x);
	WRITE_COORD(vecOrigin.y);
	WRITE_COORD(vecOrigin.z);
	WRITE_COORD(vecOrigin.x);
	WRITE_COORD(vecOrigin.y);
	WRITE_COORD(vecOrigin.z);
	WRITE_SHORT(g_iHCTransBalls);
	WRITE_BYTE(9);	// count
	WRITE_BYTE(4);	// life
	WRITE_BYTE(2);	// scale
	WRITE_BYTE(12); // velocity
	WRITE_BYTE(30); // randomness
	MESSAGE_END();
}

// orig 0x160f20: flash the screens of pPlayer and of every client that can see vecOrigin
void DHHeadcrabifyFade(Vector vecOrigin, CBaseEntity* pPlayer)
{
	if (pPlayer)
		UTIL_ScreenFade(pPlayer, Vector(190, 190, 255), 0.9, 0.2, 240, 0);

	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecOrigin, 512)) != NULL)
	{
		if (!pEntity->pev || !UTIL_IsValidEntity(pEntity->pev->pContainingEntity))
			continue;
		if (!pEntity->IsNetClient())
			continue;
		if (pPlayer && pEntity->pev->pContainingEntity == pPlayer->pev->pContainingEntity)
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecOrigin, pEntity->pev->origin + pEntity->pev->view_ofs, ignore_monsters, NULL, &tr);
		if (tr.flFraction >= 1.0 || tr.pHit == pEntity->pev->pContainingEntity)
			UTIL_ScreenFade(pEntity, Vector(190, 190, 255), 0.3, 0.0, 240, 0);
	}
}

// orig 0x161090: the drivable gonarch's headcrabifier hit pent. A monster becomes a
// monster_headcrab; a player, or the driver of a drive monster, is put in a new drive_headcrab.
// pentAttacker gets the kill.
void DHCHeadcrabifyEnt(edict_t* pent, edict_t* pentAttacker)
{
	if (!UTIL_IsValidEntity(pent))
		return;
	if ((pent->v.flags & (FL_CLIENT | FL_MONSTER)) == 0)
		return;

	CBaseEntity* pEntity = CBaseEntity::Instance(pent);
	if (!pEntity || !pEntity->IsAlive() || pEntity->IsBSPModel())
		return;
	int iCap = 0x1000000; // VERIFY: name of RC cap bit 24 (can be chumtoadified or headcrabified)
	if (!pEntity->HasRCCap(iCap))
		return;

	// the inflictor the kill is credited to
	CBaseEntity* pInflictor = GetClassPtr((CBaseEntity*)NULL);
	pInflictor->Spawn();
	pInflictor->pev->classname = MAKE_STRING("gonarch_headcrabifier");
	pInflictor->pev->flags |= FL_KILLME;

	EMIT_SOUND_DYN(pEntity->pev->pContainingEntity, CHAN_BODY, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(pEntity->pev->pContainingEntity, CHAN_WEAPON, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(pEntity->pev->pContainingEntity, CHAN_VOICE, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(pEntity->pev->pContainingEntity, CHAN_ITEM, "common/null.wav", 0, 0, 0, 100);

	if (pEntity->IsPlayer() && !((CBasePlayer*)pEntity)->m_bInDriveEnt)
	{
		CBasePlayer* pPlayer = (CBasePlayer*)pEntity;
		DHHeadcrabifyEffects(pPlayer->Center());
		DHHeadcrabifyFade(pPlayer->Center(), pPlayer);
		Vector vecAngles = Vector(0, pPlayer->pev->v_angle.y, 0);
		pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_headcrab", pPlayer->Center(), vecAngles);
		if (pPlayer->m_pCurDriveEnt)
		{
			pPlayer->m_pCurDriveEnt->pev->angles = Vector(0, pPlayer->pev->v_angle.y, 0);
			pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
			pPlayer->m_bInDriveEnt = true;
			pPlayer->m_pCurDriveEnt->pev->enemy = pentAttacker;
			EMIT_SOUND_DYN(pPlayer->m_pCurDriveEnt->pev->pContainingEntity, CHAN_ITEM, "debris/beamstart7.wav", 1.0, 0.9, 0, 120);
		}
		if (pPlayer->pev->pContainingEntity != pentAttacker)
			g_pGameRules->PlayerKilled(pPlayer, pentAttacker ? VARS(pentAttacker) : NULL, pInflictor->pev);
		return;
	}

	if (!pEntity->IsPlayer() && !pEntity->IsDriveent())
	{
		// a monster turns into a headcrab
		Vector vecOrigin = pEntity->pev->origin + Vector(0, 0, 1);
		DHHeadcrabifyEffects(pEntity->Center());
		DHHeadcrabifyFade(pEntity->Center(), NULL);
		pEntity->Killed(pEntity->pev, GIB_NORMAL);
		pEntity->pev->flags |= FL_KILLME;
		CBaseEntity* pCrab = CBaseEntity::Create("monster_headcrab", vecOrigin, pEntity->pev->angles);
		// VERIFY: the original doesn't check pCrab for NULL
		EMIT_SOUND_DYN(pCrab->pev->pContainingEntity, CHAN_ITEM, "debris/beamstart7.wav", 1.0, 0.9, 0, 120);
		return;
	}

	// a drive monster: its driver gets a headcrab instead (the driver is CBaseDriveEnt::m_pOwner)
	if (!pEntity)
		return;
	CBaseDriveEnt* pDrive = static_cast<CBaseDriveEnt*>(pEntity);
	if (!pDrive->m_pOwner)
		return;

	CBasePlayer* pPlayer = pDrive->m_pOwner;
	Vector vecCenter = pDrive->Center();
	pDrive->DEClientLeave();
	pDrive->m_pOwner = NULL;
	pDrive->Killed(pDrive->pev, GIB_NORMAL);
	pDrive->pev->flags |= FL_KILLME;
	pPlayer->m_pCurDriveEnt = NULL;
	pPlayer->m_bInDriveEnt = false;

	DHHeadcrabifyEffects(vecCenter);
	DHHeadcrabifyFade(vecCenter, pPlayer);
	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_headcrab", pDrive->pev->origin, Vector(0, pPlayer->pev->v_angle.y, 0));
	if (pPlayer->m_pCurDriveEnt)
	{
		pPlayer->m_pCurDriveEnt->pev->angles = Vector(0, pPlayer->pev->v_angle.y, 0);
		pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
		pPlayer->m_bInDriveEnt = true;
		pPlayer->m_pCurDriveEnt->pev->enemy = pentAttacker;
		EMIT_SOUND_DYN(pPlayer->m_pCurDriveEnt->pev->pContainingEntity, CHAN_ITEM, "debris/beamstart7.wav", 1.0, 0.9, 0, 120);
	}
	if (pPlayer->pev->pContainingEntity != pentAttacker)
		g_pGameRules->PlayerKilled(pPlayer, pentAttacker ? VARS(pentAttacker) : NULL, pInflictor->pev);
}

// orig 0x161630
void CDriveHC::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;
	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

int CDriveHC::GetRCCaps() { return 0x6fe3d5f; }	   // orig 0x1616b0
int CDriveHC::GetGuassDistOut() { return 56; }	   // orig 0x1616c0
int CDriveHC::GetGuassHull() { return human_hull; } // orig 0x1616d0

// orig 0x1616e0
Vector CDriveHC::GetCamRotOrg()
{
	return pev->origin + Vector(0, 0, 32);
}

int CDriveHC::BloodColor() { return BLOOD_COLOR_YELLOW; } // orig 0x161740
int CDriveHC::Classify() { return CLASS_ALIEN_PREY; }	  // orig 0x161750
