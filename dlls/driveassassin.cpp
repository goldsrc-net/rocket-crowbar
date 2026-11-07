/***
*
*	Rocket Crowbar 1.9: driveassassin.cpp. The drivable female assassin: a player who uses a
*	pickup_drive_assassin walks, charges jumps, shoots, kicks, throws grenades and cloaks as a
*	drive_assassin (CBaseDriveEnt), seen from a chase camera. drive_assassin_spawner places
*	the pickups. Tunables come from the DRIVEASSASSIN section of rc.ini (PREFS_KeyLoopUp*).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "decals.h"
#include "client.h"
#include "UserMessages.h"
#include "driveent.h"
#include "driveassassin.h"

// Preff.cpp (rc.ini), declared as dlls/Preff.h has them
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
extern bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault);

#define ASSASSIN_PREFS "DRIVEASSASSIN"

const char* g_sDAMeleeHitSounds[] =
	{
		"zombie/claw_strike1.wav",
		"zombie/claw_strike2.wav",
		"zombie/claw_strike3.wav",
};

const char* g_sDAMeleeMissSounds[] =
	{
		"zombie/claw_miss1.wav",
		"zombie/claw_miss2.wav",
};

//=========================================================
// CDriveAssassin. Nothing of it is saved (the original has no save table), so every member
// below is named from what it does; the original's offsets are in the comments.
//=========================================================
class CDriveAssassin : public CBaseDriveEnt
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

	void EXPORT DAThink();
	void EXPORT DyingThink();

	void DoJump();
	void DoGrenade();
	void DoKick();
	void DoGun();
	void CheckDoor();
	void CheckEHop();
	void RunWaterPhysics();
	void DoMovement();
	void DoCloak();
	void DoHealthRegen();
	void UpdateOwnerHUD();
	void KickDamage();
	void AsFireBullets(Vector vecSrc, Vector vecDirShooting, Vector vecSpread);
	void SetDeathAnim();

	float m_fFrameTime;			// orig +0x280, StudioFrameAdvance interval, at least 0.001
	bool m_bIdle;				// orig +0x284, nothing this think overrode the idle animation. VERIFY name
	bool m_bMoving;				// orig +0x285, walking this think; written, never read. VERIFY name
	bool m_bCanMove;			// orig +0x286. VERIFY name
	bool m_bCanShoot;			// orig +0x287. VERIFY name
	bool m_bCanKickOrThrow;		// orig +0x288, gates DoGrenade and DoKick. VERIFY name
	bool m_bCanAct;				// orig +0x289, cleared with the others; written, never read. VERIFY name
	bool m_bLeftGround;			// orig +0x28a, set when airborne; written, never read. VERIFY name
	bool m_bInAir;				// orig +0x28b, airborne last think (landing starts m_fNextJumpTime). VERIFY name
	bool m_bJumping;			// orig +0x28c, the jump animation is playing. VERIFY name
	bool m_bInAction;			// orig +0x28d, a kick or throw animation blocks everything else. VERIFY name
	float m_fJumpChargeStart;	// orig +0x290, -1 when jump is not held. VERIFY name
	float m_fNextJumpTime;		// orig +0x294, 0.5s after landing. VERIFY name
	float m_fLastActiveTime;	// orig +0x298, last action; health regen and cloak read it. VERIFY name
	float m_fNextHealthRegen;	// orig +0x29c. VERIFY name
	Vector m_vecMoveVel;		// orig +0x2a0, walk velocity (WalkMove/MoveToOrigin moves, not pev->velocity). VERIFY name
	float m_fMoveVelTime;		// orig +0x2ac, when m_vecMoveVel was measured. VERIFY name
	float m_fNextEHopCheck;		// orig +0x2b0. VERIFY name
	float m_fNextDoorCheck;		// orig +0x2b4. VERIFY name
	bool m_bStepSound;			// orig +0x2b8, footstep every other think. VERIFY name
	float m_fLastShotTime;		// orig +0x2bc, -1 before the first. VERIFY name
	float m_fGunSpread;			// orig +0x2c0, 0.08 tightening to 0.01 while firing. VERIFY name
	bool m_bGunReady;			// orig +0x2c4, toggles each think fire is held; fires on the off beat. VERIFY name
	float m_fNextGrenadeTime;	// orig +0x2c8. VERIFY name
	float m_fGrenadeChargeStart; // orig +0x2cc, -1 when duck is not held. VERIFY name
	float m_fGrenadeTime;		// orig +0x2d0, fuse of the grenade being thrown. VERIFY name
	float m_fGrenadeVel;		// orig +0x2d4, speed of the grenade being thrown. VERIFY name
	float m_fNextKickTime;		// orig +0x2d8. VERIFY name
	float m_fKickDamageTime;	// orig +0x2dc, -1 when no kick is pending. VERIFY name
	bool m_bCloaked;			// orig +0x2e0. VERIFY name
	float m_fCloakToggleTime;	// orig +0x2e4, cloak cannot toggle before this. VERIFY name
	float m_fCloakDrainTime;	// orig +0x2e8, next drain (cloaked) or recharge (not). VERIFY name
	int m_iIdleSequence;		// orig +0x2ec. VERIFY name
	float m_fIdleSeqTime;		// orig +0x2f0. VERIFY name
	int m_iShell;				// orig +0x2f4, models/shell.mdl (CHAssassin's name)
	int m_iClientHealth;		// orig +0x2f8 (CDriveApache's name)
	int m_iClientArmor;			// orig +0x2fc. VERIFY name
};

// orig 0x1347f0
LINK_ENTITY_TO_CLASS(drive_assassin, CDriveAssassin);

// orig 0x134820
void CDriveAssassin::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_assassin");

	SET_MODEL(ENT(pev), "models/hassassin.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->effects = 0;
	pev->friction = 1;
	m_HackedGunPos = Vector(0, 24, 48);
	pev->health = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinHealth", 180);
	pev->max_health = pev->health;
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;

	m_iIdleSequence = LookupSequence(RANDOM_FLOAT(0, 1) > 0.5 ? "idle3" : "idle1");
	pev->sequence = m_iIdleSequence;
	ResetSequenceInfo();
	InitBoneControllers();
	pev->frame = 0;
	pev->framerate = 1.0;
	SetEyePosition();

	m_bIdle = true;
	m_bMoving = false;
	m_bCanMove = true;
	m_bCanShoot = true;
	m_bCanKickOrThrow = true;
	m_bCanAct = true;
	m_bInAir = false;
	m_bJumping = false;
	m_bGunReady = true;
	m_bInAction = false;
	m_fGrenadeTime = 0;
	m_fGrenadeVel = 0;
	m_fGunSpread = 0.08;
	m_fJumpChargeStart = -1;
	m_fLastShotTime = -1;
	m_fGrenadeChargeStart = -1;
	m_fKickDamageTime = -1;
	m_fNextJumpTime = m_fNextGrenadeTime = m_fNextKickTime = gpGlobals->time - 1.0;
	m_fIdleSeqTime = m_fMoveVelTime = gpGlobals->time;
	m_vecMoveVel = g_vecZero;
	m_bStepSound = true;
	m_fNextEHopCheck = m_fNextDoorCheck = gpGlobals->time;
	m_fLastActiveTime = m_fNextHealthRegen = gpGlobals->time;
	m_fCloakToggleTime = gpGlobals->time + 1.0;
	m_bCloaked = false;
	m_fCloakToggleTime = gpGlobals->time;
	m_fCloakDrainTime = gpGlobals->time;
	pev->armorvalue = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakStartPower", 50);

	pev->rendermode = kRenderNormal;
	pev->renderamt = 255;

	SetThink(&CDriveAssassin::DAThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x134bf0
void CDriveAssassin::Precache()
{
	PRECACHE_MODEL("models/hassassin.mdl");
	PRECACHE_SOUND("weapons/pl_gun1.wav");
	PRECACHE_SOUND("weapons/pl_gun2.wav");
	PRECACHE_SOUND("debris/beamstart1.wav");
	m_iShell = PRECACHE_MODEL("models/shell.mdl");

	for (unsigned int i = 0; i < ARRAYSIZE(g_sDAMeleeHitSounds); i++)
		PRECACHE_SOUND(g_sDAMeleeHitSounds[i]);
	for (unsigned int i = 0; i < ARRAYSIZE(g_sDAMeleeMissSounds); i++)
		PRECACHE_SOUND(g_sDAMeleeMissSounds[i]);
}

// orig 0x134cb0
const char* CDriveAssassin::GetControllerName()
{
	if (!m_pOwner)
		return NULL;

	CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;
	if (!pOwner->m_bInDriveEnt || !pOwner->m_pCurDriveEnt || pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;

	return STRING(m_pOwner->pev->netname);
}

// orig 0x134d20
void CDriveAssassin::DAThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;
	if (m_fFrameTime < 0.001)
		m_fFrameTime = 0.001;

	if (m_pOwner && !FNullEnt(m_pOwner->pev) && m_pOwner->pev && m_pOwner->pev->pContainingEntity)
	{
		edict_t* pOwnerEdict = m_pOwner->pev->pContainingEntity;
		if (pOwnerEdict->pvPrivateData && !pOwnerEdict->free && UTIL_PlayerByIndex(ENTINDEX(pOwnerEdict)))
		{
			if (pev->waterlevel > 1)
				pev->flags &= ~FL_ONGROUND;

			DispatchAnimEvents(m_fFrameTime);

			m_pOwner->pev->rendermode = kRenderTransColor;
			m_pOwner->pev->renderamt = 0;
			m_pOwner->pev->effects |= EF_NODRAW;

			UpdateOwnerHUD();

			if ((pev->flags & FL_FROZEN) != 0)
			{
				m_vecMoveVel = g_vecZero;
				m_fMoveVelTime = gpGlobals->time;
				pev->rendermode = kRenderNormal;
				return;
			}

			pev->angles.y = pev->ideal_yaw;
			m_bIdle = true;
			m_bMoving = false;
			m_bCanMove = true;
			m_bCanShoot = true;
			m_bCanKickOrThrow = true;
			m_bCanAct = true;
			pev->angles.x = 0;

			if (m_fKickDamageTime != -1 && m_fKickDamageTime < gpGlobals->time)
			{
				KickDamage();
				m_fKickDamageTime = -1;
			}

			if (m_bInAction)
			{
				if (m_fSequenceFinished)
					m_bInAction = false;

				if (m_bInAction && !m_fSequenceFinished)
				{
					m_bIdle = false;
					m_bMoving = false;
					m_bCanMove = false;
					m_bCanShoot = false;
					m_bCanKickOrThrow = false;
					m_bCanAct = false;
					m_fLastActiveTime = gpGlobals->time;
					pev->fuser4 = 0;
					return;
				}
			}

			DoJump();
			DoGrenade();
			DoKick();
			DoGun();
			DoMovement();
			DoCloak();
			DoHealthRegen();

			pev->fuser4 = m_vecMoveVel.Length();

			if (!m_bIdle)
				return;

			if (m_fLastShotTime + 4 < gpGlobals->time)
			{
				if (m_fIdleSeqTime + 5 < gpGlobals->time)
					m_iIdleSequence = LookupSequence(RANDOM_FLOAT(0, 1) > 0.5 ? "idle3" : "idle1");

				if (pev->sequence != m_iIdleSequence && (m_fSequenceFinished || m_fSequenceLoops))
				{
					pev->sequence = m_iIdleSequence;
					ResetSequenceInfo();
					pev->frame = 0;
				}
				m_fIdleSeqTime = gpGlobals->time;
			}
			else
			{
				// gun up for a while after shooting
				if (pev->sequence != LookupSequence("idle2") && (m_fSequenceFinished || m_fSequenceLoops))
				{
					pev->sequence = LookupSequence("idle2");
					ResetSequenceInfo();
					pev->frame = 0;
				}
			}
			m_bJumping = false;
			return;
		}
	}

	// the driver is gone
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("assassin");
	g_pevLastInflictor = pev;
	Killed(pev, GIB_NORMAL);
}

// orig 0x135180: hold jump to charge, release to leap; steer a little in the air
void CDriveAssassin::DoJump()
{
	if (m_bJumping)
	{
		if ((pev->flags & FL_ONGROUND) != 0)
			m_bJumping = false;

		if (m_bJumping)
		{
			if (!m_fSequenceFinished)
			{
				m_bCanMove = false;
				m_bIdle = false;
				m_bCanShoot = false;
				m_bCanKickOrThrow = false;
				m_bCanAct = false;
				m_fLastActiveTime = gpGlobals->time;
				return;
			}
			m_bJumping = false;
		}
		else if (m_fSequenceFinished)
			m_bJumping = false;
	}
	else if (m_fSequenceFinished)
		m_bJumping = false;

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		if (m_bInAir)
		{
			// landed
			m_fNextJumpTime = gpGlobals->time + 0.5;
			m_bInAir = false;
			m_fLastActiveTime = gpGlobals->time;
		}
		m_bInAir = false;

		if ((m_pOwner->pev->button & IN_JUMP) == 0)
		{
			if (m_fJumpChargeStart == -1)
				return;

			// released: jump, higher the longer it was held
			pev->sequence = LookupSequence("jump");
			ResetSequenceInfo();
			pev->frame = 0;
			m_bJumping = true;

			float flMinVel = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinMinJumpVel", 400);
			float flMaxVel = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinMaxJumpVel", 900);
			float flChargeTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinJumpChargeTime", 1.4);
			Vector vecPush = g_vecZero;
			float flHeld = gpGlobals->time - m_fJumpChargeStart;
			if (!(flHeld < flChargeTime))
				flHeld = flChargeTime;

			if (m_vecMoveVel.Length() > 0.1 || pev->velocity.Length() < 0.1)
			{
				vecPush = m_vecMoveVel;
			}
			else
			{
				// VERIFY: v_forward/v_right are whatever the last MakeVectors left (no call here)
				float flStill = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinJumpVelFromStill", 420);
				if ((m_pOwner->pev->button & IN_FORWARD) != 0)
					vecPush = vecPush + gpGlobals->v_forward * flStill;
				if ((m_pOwner->pev->button & IN_BACK) != 0)
					vecPush = vecPush - gpGlobals->v_forward * flStill;
				if ((m_pOwner->pev->button & IN_MOVELEFT) != 0)
					vecPush = vecPush - gpGlobals->v_right * flStill;
				if ((m_pOwner->pev->button & IN_MOVERIGHT) != 0)
					vecPush = vecPush + gpGlobals->v_right * flStill;
			}

			pev->velocity = pev->velocity + vecPush;
			pev->velocity.z = (flMaxVel - flMinVel) * (flHeld / flChargeTime) + flMinVel + pev->velocity.z;
			pev->flags &= ~FL_ONGROUND;

			m_bLeftGround = true;
			m_bCanMove = false;
			m_bIdle = false;
			m_bInAir = true;
			m_bCanShoot = false;
			m_bCanKickOrThrow = false;
			m_bCanAct = false;
			m_fJumpChargeStart = -1;
			m_fLastActiveTime = gpGlobals->time;

			switch (RANDOM_LONG(0, 3))
			{
			case 0:
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_step1.wav", 0.7, 0.8, 0, 100);
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_step3.wav", 0.7, 0.8, 0, 100);
				break;
			case 2:
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_step2.wav", 0.7, 0.8, 0, 100);
				break;
			case 3:
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_step4.wav", 0.7, 0.8, 0, 100);
				break;
			}
			return;
		}

		// held: start charging once the landing delay is over
		if (m_fJumpChargeStart == -1 && m_fNextJumpTime < gpGlobals->time)
		{
			m_fJumpChargeStart = gpGlobals->time;
			m_fLastActiveTime = gpGlobals->time;
		}
		m_fLastActiveTime = gpGlobals->time;
		return;
	}

	// in the air
	m_bLeftGround = true;
	m_bCanMove = false;
	m_bIdle = false;
	m_bCanKickOrThrow = false;
	m_bCanAct = false;
	m_bInAir = true;

	if (pev->velocity.z > 16 && pev->waterlevel == 0)
	{
		m_bCanShoot = false;
		if (pev->sequence != LookupSequence("fly_up"))
		{
			pev->sequence = LookupSequence("fly_up");
			ResetSequenceInfo();
			pev->frame = 0;
		}
	}
	else
	{
		m_bCanShoot = true;
		if (!m_bGunReady || (m_pOwner->pev->button & IN_ATTACK) == 0)
		{
			if (pev->sequence != LookupSequence("fly_down"))
			{
				pev->sequence = LookupSequence("fly_down");
				ResetSequenceInfo();
				pev->frame = 0;
			}
		}
		else
		{
			pev->sequence = LookupSequence("fly_attack");
			ResetSequenceInfo();
			pev->frame = 0;
		}
	}

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	float flAccel = m_fFrameTime * 300;
	Vector vecPush = g_vecZero;
	if (pev->velocity.Length2D() < 800 && pev->waterlevel == 0)
	{
		if ((m_pOwner->pev->button & IN_FORWARD) != 0)
			vecPush = vecPush + flAccel * gpGlobals->v_forward;
		if ((m_pOwner->pev->button & IN_BACK) != 0)
			vecPush = vecPush - flAccel * gpGlobals->v_forward;
		if ((m_pOwner->pev->button & IN_MOVELEFT) != 0)
			vecPush = vecPush - flAccel * gpGlobals->v_right;
		if ((m_pOwner->pev->button & IN_MOVERIGHT) != 0)
			vecPush = vecPush + flAccel * gpGlobals->v_right;
	}
	pev->velocity = pev->velocity + vecPush;

	m_fLastActiveTime = gpGlobals->time;
}

// orig 0x135a70: hold duck to cook a grenade (shorter fuse, faster throw), release to throw
void CDriveAssassin::DoGrenade()
{
	if (!m_bCanKickOrThrow || (pev->flags & FL_ONGROUND) == 0)
		return;

	if (m_fGrenadeChargeStart == -1)
	{
		if ((m_pOwner->pev->button & IN_DUCK) == 0)
			return;
	}
	else if ((m_pOwner->pev->button & IN_DUCK) == 0)
	{
		float flMaxTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinGrenadeMaxTime", 3);
		float flChargeTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinGrenadeChargeTime", 1.5);

		float flHeld = gpGlobals->time - m_fGrenadeChargeStart;
		if (flHeld >= 0)
		{
			if (flHeld <= flMaxTime)
				m_fGrenadeTime = flMaxTime - flHeld;
			else
				m_fGrenadeTime = flMaxTime - flMaxTime;
		}
		else
			m_fGrenadeTime = flMaxTime - 0;

		if (m_fGrenadeTime > 0.1)
		{
			float flMinVel = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinGrenadeMinVel", 300);
			float flMaxVel = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinGrenadeMaxVel", 900);
			float flFrac;
			if (gpGlobals->time - m_fGrenadeChargeStart >= 0)
			{
				if (gpGlobals->time - m_fGrenadeChargeStart <= flChargeTime)
					flFrac = (gpGlobals->time - m_fGrenadeChargeStart) / flChargeTime;
				else
					flFrac = flChargeTime / flChargeTime;
			}
			else
				flFrac = 0 / flChargeTime;
			m_fGrenadeVel = flFrac * (flMaxVel - flMinVel) + flMinVel;
		}
		else
		{
			m_fGrenadeVel = 0;
		}

		m_bCanMove = false;
		m_bIdle = false;
		m_bCanShoot = false;
		m_bCanAct = false;
		m_bInAction = true;

		// the grenade leaves at the animation event (HandleAnimEvent, event 2)
		pev->sequence = LookupSequence("grenadethrow");
		ResetSequenceInfo();
		pev->frame = 0;
		m_fLastActiveTime = gpGlobals->time;
		m_fGrenadeChargeStart = -1;
		m_fNextGrenadeTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinGrenadeRefire", 1.4) + gpGlobals->time;
		return;
	}

	m_fLastActiveTime = gpGlobals->time;
	if (m_fNextGrenadeTime <= gpGlobals->time && m_fGrenadeChargeStart == -1)
		m_fGrenadeChargeStart = gpGlobals->time;
}

// orig 0x135d30: secondary fire kicks; the hit lands 0.24s in (KickDamage)
void CDriveAssassin::DoKick()
{
	if (!m_bCanKickOrThrow || (pev->flags & FL_ONGROUND) == 0 || (m_pOwner->pev->button & IN_ATTACK2) == 0)
		return;

	m_fLastActiveTime = gpGlobals->time;
	if (m_fNextKickTime <= gpGlobals->time)
	{
		m_bCanMove = false;
		m_bIdle = false;
		m_bCanShoot = false;
		pev->sequence = LookupSequence("kickshort");
		ResetSequenceInfo();
		pev->frame = 0;
		m_fNextKickTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinKickRefire", 0.7) + gpGlobals->time;
		m_bInAction = true;
		m_fKickDamageTime = gpGlobals->time + 0.24;
	}
}

// orig 0x135e40: the pistol fires every other think while primary fire is held
void CDriveAssassin::DoGun()
{
	if (!m_bCanShoot || (m_pOwner->pev->button & IN_ATTACK) == 0)
		return;

	m_fLastActiveTime = gpGlobals->time;

	if (!m_bGunReady)
	{
		m_bCanMove = false;
		m_bIdle = false;
		SetBlending(0, -pev->idealpitch);
		m_bGunReady = true;
		return;
	}

	m_bGunReady = false;
	m_bCanMove = false;
	m_bIdle = false;

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		pev->sequence = LookupSequence("shoot");
		ResetSequenceInfo();
		pev->frame = 0;
	}

	UTIL_MakeVectors(pev->angles);
	Vector vecSrc = pev->origin + gpGlobals->v_up * 32;

	// spread tightens with sustained fire
	if (m_fLastShotTime + 2 < gpGlobals->time)
	{
		m_fGunSpread = 0.08;
	}
	else
	{
		m_fGunSpread = m_fGunSpread - 0.01;
		if (m_fGunSpread < 0.01)
			m_fGunSpread = 0.01;
	}
	m_fLastShotTime = gpGlobals->time;

	UTIL_MakeVectors(pev->angles);
	SetBlending(0, -pev->idealpitch);

	Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40, 90) + gpGlobals->v_up * RANDOM_FLOAT(75, 200);
	vecShellVelocity = vecShellVelocity + gpGlobals->v_forward * RANDOM_FLOAT(-40, 40);
	EjectBrass(vecSrc + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL);

	UTIL_MakeVectors(Vector(pev->idealpitch, pev->angles.y, 0));
	Vector vecDir = gpGlobals->v_forward;

	pev->classname = MAKE_STRING("assassin_gun");
	AsFireBullets(vecSrc, vecDir, Vector(m_fGunSpread, m_fGunSpread, m_fGunSpread));
	pev->effects |= EF_MUZZLEFLASH;
	pev->classname = MAKE_STRING("drive_assassin");

	((CBasePlayer*)m_pOwner)->m_fDrvEntEjectTime = gpGlobals->time + 2.0;

	switch (RANDOM_LONG(0, 1))
	{
	case 0:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/pl_gun1.wav", RANDOM_FLOAT(0.6, 0.8), 0.8, 0, 100);
		break;
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/pl_gun2.wav", RANDOM_FLOAT(0.6, 0.8), 0.8, 0, 100);
		break;
	}
}

// orig 0x136350: walking into a door opens it
void CDriveAssassin::CheckDoor()
{
	if (gpGlobals->time < m_fNextDoorCheck)
		return;
	if ((pev->flags & FL_ONGROUND) == 0)
		return;
	if (!m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecSrc = pev->origin + Vector(0, 0, 32);
	Vector vecEnd = vecSrc + m_fFrameTime * pev->framerate * m_flGroundSpeed * gpGlobals->v_forward;

	TraceResult tr;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, edict(), &tr);

	if (tr.pHit && (!strcmp(STRING(VARS(tr.pHit)->classname), "func_door") || !strcmp(STRING(VARS(tr.pHit)->classname), "func_door_rotating")))
	{
		CBaseEntity* pDoor = CBaseEntity::Instance(tr.pHit);
		if (pDoor)
			pDoor->Touch(this);
	}

	m_fNextDoorCheck = gpGlobals->time + 0.3;
}

// orig 0x1365b0: walking off a ledge hops forward instead of stepping down
void CDriveAssassin::CheckEHop()
{
	if (!(m_fNextEHopCheck <= gpGlobals->time) || (pev->flags & FL_ONGROUND) == 0)
		return;
	if (pev->velocity.z > 16 || !m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecFront = pev->origin + Vector(0, 0, 4) + gpGlobals->v_forward * 28;
	Vector vecDown = vecFront - Vector(0, 0, 24);

	TraceResult tr;
	UTIL_TraceLine(pev->origin + Vector(0, 0, 4), vecFront, dont_ignore_monsters, edict(), &tr);
	if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
	{
		UTIL_TraceLine(vecFront, vecDown, dont_ignore_monsters, edict(), &tr);
		if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
		{
			// nothing below the next step
			pev->velocity.z = pev->velocity.z + 50;
			pev->velocity = pev->velocity + gpGlobals->v_forward * 100;
			pev->flags &= ~FL_ONGROUND;
			UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
			m_fLastActiveTime = gpGlobals->time;
		}
	}
	m_fNextEHopCheck = gpGlobals->time + 0.2;
}

// orig 0x1368e0: swimming: friction, buoyancy, and forward to swim along the view
void CDriveAssassin::RunWaterPhysics()
{
	float flFriction = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinWaterFriction", 8);
	float flSpeed = pev->velocity.Length();
	if (flSpeed > 0.1)
	{
		float flControl = (flSpeed < 6) ? 6 : flSpeed;
		float flNewSpeed = flSpeed - flControl * flFriction * m_fFrameTime;
		if (flNewSpeed < 0)
			flNewSpeed = 0;
		flNewSpeed = flNewSpeed / flSpeed;
		pev->velocity = pev->velocity * flNewSpeed;
	}

	if (pev->waterlevel > 1)
	{
		float flLift = 1.1 * CVAR_GET_FLOAT("sv_gravity");
		if (pev->gravity > 0)
			flLift = flLift * pev->gravity;
		pev->velocity.z = flLift * m_fFrameTime + pev->velocity.z;
	}

	if (pev->sequence == LookupSequence("fly_down") || pev->sequence == LookupSequence("fly_attack"))
	{
		float flRate;
		if (pev->velocity.Length() > 0.1)
			flRate = 150 / pev->velocity.Length();
		else
			flRate = 0.1;

		if (flRate >= 0.5)
			pev->framerate = (flRate > 1.1) ? 1.1 : flRate;
		else
			pev->framerate = 0.5;
	}

	if ((m_pOwner->pev->button & IN_FORWARD) != 0)
	{
		UTIL_MakeAimVectors(pev->angles);
		float flWaterSpeed = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinWaterSpeed", 160);
		float flWaterAccel = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinWaterAccel", 140);
		Vector vecForward = gpGlobals->v_forward;
		float flAdd = flWaterSpeed - DotProduct(pev->velocity, vecForward);
		if (flAdd > 0)
		{
			float flAccel = flWaterAccel * m_fFrameTime * flWaterSpeed;
			if (flAdd < flAccel)
				flAccel = flAdd;
			pev->velocity = pev->velocity + vecForward * flAccel;
		}
	}
}

// orig 0x136c10: forward walks the monster with WalkMove (run, or walk while +speed is held),
// measuring the distance first so a blocked step leaves it where it was
void CDriveAssassin::DoMovement()
{
	if ((pev->flags & FL_ONGROUND) == 0)
	{
		if (pev->waterlevel != 0)
			RunWaterPhysics();
	}
	else if (m_bCanMove)
	{
		if ((m_pOwner->pev->button & IN_FORWARD) == 0)
		{
			if (pev->sequence == LookupSequence("run") || pev->sequence == LookupSequence("walk"))
				m_fSequenceFinished = true;
		}
		else
		{
			m_fLastActiveTime = gpGlobals->time;
			const bool bWalk = (m_pOwner->pev->button & IN_RUN) != 0;
			float flOldFrame = pev->frame;
			int iOldSequence = pev->sequence;
			bool bWasStill = false;
			if (pev->sequence != LookupSequence("run"))
				bWasStill = pev->sequence != LookupSequence("walk");

			if (!bWalk)
			{
				if (pev->sequence != LookupSequence("run"))
				{
					pev->sequence = LookupSequence("run");
					ResetSequenceInfo();
					pev->frame = 0;
				}
			}
			else if (pev->sequence != LookupSequence("walk"))
			{
				pev->sequence = LookupSequence("walk");
				ResetSequenceInfo();
				pev->frame = 0;
			}

			CheckDoor();
			m_bIdle = false;
			m_bMoving = true;

			UTIL_MakeVectors(pev->angles);
			Vector vecGoal = Center() + gpGlobals->v_forward * 512;
			float flMoved = 0;
			Vector vecStart = pev->origin;
			float flDist = m_flGroundSpeed * pev->framerate * m_fFrameTime;

			while (flDist > 0.001)
			{
				float flStep = (flDist > 16) ? 16 : flDist;
				Vector vecSave = pev->origin;
				while (!WALK_MOVE(edict(), pev->angles.y, flStep, WALKMOVE_CHECKONLY) && flStep > 2)
					flStep -= 2;
				UTIL_SetOrigin(pev, vecSave);

				if (!WALK_MOVE(edict(), pev->angles.y, flStep, WALKMOVE_CHECKONLY))
				{
					if (flMoved < 0.01)
					{
						// blocked at once: stand
						if (bWasStill)
						{
							pev->sequence = iOldSequence;
							ResetSequenceInfo();
							pev->frame = flOldFrame;
						}
						else
						{
							m_fSequenceFinished = true;
						}
						m_bIdle = true;
						m_bMoving = false;
						m_vecMoveVel = g_vecZero;
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

			if (!bWalk && m_bStepSound)
			{
				m_bStepSound = false;
				switch (RANDOM_LONG(0, 3))
				{
				case 0:
					EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_step1.wav", 0.5, 0.8, 0, 100);
					break;
				case 1:
					EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_step3.wav", 0.5, 0.8, 0, 100);
					break;
				case 2:
					EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_step2.wav", 0.5, 0.8, 0, 100);
					break;
				case 3:
					EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_step4.wav", 0.5, 0.8, 0, 100);
					break;
				}
			}
			else
			{
				m_bStepSound = true;
			}

			Vector vecOld = pev->origin;
			while (flMoved > 0.001)
			{
				float flStep = (flMoved > 16) ? 16 : flMoved;
				UTIL_MoveToOrigin(edict(), vecGoal, flStep, MOVE_NORMAL);
				flMoved -= flStep;
			}

			Vector vecDelta = pev->origin - vecOld;
			if (vecDelta.Length() > 0.01 && m_fFrameTime > 0.001)
			{
				m_vecMoveVel = vecDelta / m_fFrameTime;
				m_fMoveVelTime = gpGlobals->time;
				return;
			}
		}
	}

	m_vecMoveVel = g_vecZero;
	m_fMoveVelTime = gpGlobals->time;
}

// orig 0x137270: +use toggles the cloak, which runs on pev->armorvalue. Cloaked, the alpha
// follows how fast she moves; under 50 she is FL_NOTARGET.
void CDriveAssassin::DoCloak()
{
	float flMaxPower = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakMaxPower", 100);

	if (!m_bCloaked)
	{
		// fade back in
		if (pev->renderamt < 255)
		{
			pev->renderamt = m_fFrameTime * 300 + pev->renderamt;
			pev->rendermode = kRenderTransTexture;
			if (pev->renderamt >= 255)
			{
				pev->rendermode = kRenderNormal;
				pev->renderamt = 255;
			}
		}
		pev->flags &= ~FL_NOTARGET;

		if (gpGlobals->time < m_fCloakToggleTime)
			return;

		if (m_fCloakDrainTime < gpGlobals->time)
		{
			pev->armorvalue = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakOffRegenPerUnit", 1) + pev->armorvalue;
			if (pev->armorvalue > flMaxPower)
				pev->armorvalue = flMaxPower;
			m_fCloakDrainTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakOffRegenDelay", 1) + gpGlobals->time;
		}

		if ((m_pOwner->pev->button & IN_USE) == 0)
			return;

		float flStartupDrain = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakStartupDrain", 5);
		if (pev->armorvalue <= flStartupDrain)
			return;

		m_bCloaked = true;
		m_fCloakToggleTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakMinOnTime", 3) + gpGlobals->time;
		m_fCloakDrainTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakFirstDrainDelay", 0.8) + gpGlobals->time;
		pev->armorvalue = pev->armorvalue - flStartupDrain;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart1.wav", 0.3, 0.8, 0, 100);
	}

	if (m_fCloakToggleTime < gpGlobals->time && (m_pOwner->pev->button & IN_USE) != 0)
	{
		m_bCloaked = false;
		m_fCloakToggleTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakMinOffTime", 2) + gpGlobals->time;
		m_fCloakDrainTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakOffFirstRechargeDelay", 1.2) + gpGlobals->time;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart1.wav", 0.3, 0.8, 0, 100);
	}

	if (m_fCloakDrainTime < gpGlobals->time)
	{
		pev->armorvalue = pev->armorvalue - PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakDrainPerUnit", 1);
		m_fCloakDrainTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakDrainDelay", 0.5) + gpGlobals->time;
	}

	if (pev->armorvalue <= 0)
	{
		// ran dry
		m_bCloaked = false;
		m_fCloakToggleTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakMinForcedOffTime", 3) + gpGlobals->time;
		m_fCloakDrainTime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinCloakForcedOffFirstRechargeDelay", 1.8) + gpGlobals->time;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart1.wav", 0.35, 0.8, 0, 100);
	}

	float flWantAmt;
	if ((pev->flags & FL_ONGROUND) == 0)
	{
		flWantAmt = 210;
	}
	else if ((pev->velocity + m_vecMoveVel).Length() > 200)
	{
		flWantAmt = 180;
	}
	else if ((pev->velocity + m_vecMoveVel).Length() > 20)
	{
		flWantAmt = 120;
	}
	else if ((m_pOwner->pev->button & IN_ATTACK) == 0)
	{
		flWantAmt = (m_fLastActiveTime < gpGlobals->time) ? 30 : 90;
	}
	else
	{
		flWantAmt = 95;
	}

	pev->rendermode = kRenderTransTexture;
	float flRate;
	if (flWantAmt < pev->renderamt)
		flRate = 210 * -m_fFrameTime;
	else
		flRate = 280 * m_fFrameTime;

	// VERIFY: fading out (flRate < 0) never snaps to flWantAmt, so it can overshoot
	if (fabs(pev->renderamt - flWantAmt) <= flRate)
		pev->renderamt = flWantAmt;
	else
		pev->renderamt = flRate + pev->renderamt;

	if (pev->renderamt >= 50)
		pev->flags &= ~FL_NOTARGET;
	else
		pev->flags |= FL_NOTARGET;

	m_fLastActiveTime = gpGlobals->time;
}

// orig 0x137800: heal after AssassinHealthRegenStillnessDelay seconds without action
void CDriveAssassin::DoHealthRegen()
{
	if (!(pev->health < pev->max_health) || !(m_fNextHealthRegen <= gpGlobals->time))
		return;

	if (PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinHealthRegenStillnessDelay", 4) > 0 &&
		PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinHealthRegenUnitDelay", 1.5) > 0 &&
		PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinHealthRegenStillnessDelay", 4) + m_fLastActiveTime <= gpGlobals->time)
	{
		pev->health = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinHealthRegenPerUnit", 1.8) + pev->health;
		if (pev->health > pev->max_health)
			pev->health = pev->max_health;
		m_fNextHealthRegen = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinHealthRegenUnitDelay", 1.5) + gpGlobals->time;
	}
}

// orig 0x137960: health in percent, and cloak power on the armor display
void CDriveAssassin::UpdateOwnerHUD()
{
	if (!m_pOwner)
		return;

	if (pev->health != (float)m_iClientHealth)
	{
		float flPercent = (pev->health / pev->max_health) * 100;
		int iHealth = (flPercent > 0) ? (int)flPercent : 0;
		if (iHealth < 1)
			iHealth = 1;
		else if (iHealth > 100)
			iHealth = 100;

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->edict());
		WRITE_SHORT(iHealth);
		MESSAGE_END();

		m_iClientHealth = (int)pev->health;
	}

	if (pev->armorvalue != (float)m_iClientArmor)
	{
		short iArmor;
		if (pev->armorvalue >= 0)
			iArmor = (pev->armorvalue > 999) ? 999 : (short)(int)pev->armorvalue;
		else
			iArmor = 0;

		MESSAGE_BEGIN(MSG_ONE, gmsgBattery, NULL, m_pOwner->edict());
		WRITE_SHORT(iArmor);
		MESSAGE_END();

		m_iClientArmor = (int)pev->armorvalue;
	}
}

// orig 0x137b70
void CDriveAssassin::KickDamage()
{
	float flDamage = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinKickDamage", 35);

	UTIL_MakeVectors(pev->angles);
	Vector vecStart = pev->origin;
	vecStart.z += pev->size.z * 0.5;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * 90);

	TraceResult tr;
	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);

	((CBasePlayer*)m_pOwner)->m_fDrvEntEjectTime = gpGlobals->time + 3.0;
	pev->classname = MAKE_STRING("assassin_kick");

	CBaseEntity* pHit = tr.pHit ? CBaseEntity::Instance(tr.pHit) : NULL;
	if (pHit && !pHit->IsBSPModel() && pHit->pev->takedamage != DAMAGE_NO)
	{
		const int iPitch = 80 + RANDOM_LONG(0, 20);
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_sDAMeleeHitSounds[RANDOM_LONG(0, ARRAYSIZE(g_sDAMeleeHitSounds) - 1)], 0.8, 0.8, 0, iPitch);

		// RC cap 0x1000: can be knocked around
		int iCap = 0x1000;
		if (pHit->HasRCCap(iCap))
		{
			pHit->pev->punchangle.x = -10;
			pHit->pev->punchangle.z = 5;
			pHit->pev->velocity = pHit->pev->velocity + gpGlobals->v_forward * 140 + Vector(0, 0, 80);
			pHit->pev->flags &= ~FL_ONGROUND;
		}

		pHit->TakeDamage(pev, m_pOwner ? m_pOwner->pev : pev, flDamage, DMG_CLUB | DMG_NEVERGIB);
	}
	else
	{
		const int iPitch = 100 + RANDOM_LONG(0, 20);
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_sDAMeleeMissSounds[RANDOM_LONG(0, ARRAYSIZE(g_sDAMeleeMissSounds) - 1)], 0.6, 0.8, 0, iPitch);
	}

	pev->classname = MAKE_STRING("drive_assassin");
}

// orig 0x137f20: one bullet, FireBullets style, with a tracer, credited to the driver
void CDriveAssassin::AsFireBullets(Vector vecSrc, Vector vecDirShooting, Vector vecSpread)
{
	TraceResult tr;
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;

	ClearMultiDamage();
	// VERIFY: DMG_MORTAR (1 << 23) is in the original's type; RC may give the bit its own meaning
	gMultiDamage.type = DMG_BULLET | DMG_NEVERGIB | DMG_MORTAR;

	float x, y, z;
	do
	{
		x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
		y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
		z = x * x + y * y;
	} while (z > 1);

	Vector vecDir = vecDirShooting + x * vecSpread.x * vecRight + y * vecSpread.y * vecUp;
	Vector vecEnd = vecSrc + vecDir * 8192;

	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_TRACER);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(tr.vecEndPos.x);
	WRITE_COORD(tr.vecEndPos.y);
	WRITE_COORD(tr.vecEndPos.z);
	MESSAGE_END();

	if (tr.flFraction != 1.0)
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
		if (pEntity)
		{
			pEntity->TraceAttack(m_pOwner ? m_pOwner->pev : pev, PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinDamageBullets", 19), vecDir, &tr, DMG_BULLET | DMG_NEVERGIB | DMG_MORTAR);

			if (pEntity->IsBSPModel() && CVAR_GET_FLOAT("sv_lowlag") != 1)
			{
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, BULLET_MONSTER_9MM);
				UTIL_GunshotDecalTrace(&tr, pEntity->DamageDecal(DMG_BULLET));
			}
		}
	}

	UTIL_BubbleTrail(vecSrc, tr.vecEndPos, (int)((8192 * tr.flFraction) / 64.0));

	ApplyMultiDamage(pev, m_pOwner ? m_pOwner->pev : pev);
}

// orig 0x138420
void CDriveAssassin::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
	case 3:
		break;

	case 2: // throw the grenade DoGrenade charged
	{
		UTIL_MakeVectors(pev->angles);
		Vector vecSrc = pev->origin + gpGlobals->v_forward * 34 + Vector(0, 0, 32);

		UTIL_MakeVectors(Vector(pev->idealpitch, pev->ideal_yaw, 0));
		Vector vecVelocity = gpGlobals->v_forward * m_fGrenadeVel;

		if (!m_pOwner)
		{
			CGrenade::ShootTimed(pev, vecSrc, vecVelocity, m_fGrenadeTime);
		}
		else
		{
			CGrenade::ShootDETimed(pev, m_pOwner->pev, vecSrc, vecVelocity, m_fGrenadeTime);
			((CBasePlayer*)m_pOwner)->m_fDrvEntEjectTime = gpGlobals->time + 3.0;
		}
	}
	break;

	case 2001: // landing
		if ((pev->flags & FL_ONGROUND) != 0)
		{
			if (RANDOM_LONG(0, 1) == 0)
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/bodydrop3.wav", 1, ATTN_NORM, 0, 100);
			else
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/bodydrop4.wav", 1, ATTN_NORM, 0, 100);
		}
		break;

	case 2002: // body drop
		if ((pev->flags & FL_ONGROUND) != 0)
		{
			if (RANDOM_LONG(0, 1) == 0)
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/bodydrop3.wav", 1, ATTN_NORM, 0, 90);
			else
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/bodydrop4.wav", 1, ATTN_NORM, 0, 90);
		}
		break;

	case 2010: // kick swing
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "zombie/claw_miss2.wav", 1, ATTN_NORM, 0, 100);
		break;

	default:
		break;
	}
}

// orig 0x138720: the camera pivot, lower while crouched in the shoot animation
Vector CDriveAssassin::GetCamRotOrg()
{
	float flHeight;
	if (pev->sequence == LookupSequence("idle2"))
		flHeight = 56;
	else if (pev->sequence == LookupSequence("fly_up"))
		flHeight = 48;
	else if (pev->sequence == LookupSequence("fly_down") || pev->sequence == LookupSequence("fly_attack"))
		flHeight = 56;
	else if (pev->sequence == LookupSequence("shoot"))
		flHeight = 32;
	else
		flHeight = 64;

	return pev->origin + Vector(0, 0, flHeight);
}

// orig 0x138830
void CDriveAssassin::DyingThink()
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

// orig 0x1388f0
void CDriveAssassin::SetDeathAnim()
{
	UTIL_MakeVectors(pev->angles);
	Vector vecDir = g_vecAttackDir * -1;
	Vector vecForward = gpGlobals->v_forward;

	const char* pszSequence = "die_backwards";
	if ((pev->flags & FL_ONGROUND) != 0)
	{
		if (pev->sequence == LookupSequence("run"))
			pszSequence = "death_during_run";
		else if (DotProduct(vecForward, vecDir) > -0.3)
			pszSequence = "die_simple";
	}

	pev->sequence = LookupSequence(pszSequence);
	ResetSequenceInfo();
	pev->frame = 0;
}

// orig 0x1389e0
bool CDriveAssassin::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_pOwner)
	{
		CBaseEntity* pAttacker = CBaseEntity::Instance(pevAttacker);
		if (pAttacker && !g_pGameRules->FPlayerCanTakeDamage((CBasePlayer*)m_pOwner, pAttacker))
			return false;

		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	m_fLastActiveTime = gpGlobals->time;
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x138af0
void CDriveAssassin::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->deadflag != DEAD_NO)
		return;

	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	if (m_pOwner)
	{
		CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;
		if (pOwner->m_bInDriveEnt && pOwner->m_pCurDriveEnt && pOwner->m_pCurDriveEnt->pev == pev)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->edict());
			WRITE_BYTE(HIDEHUD_ALL);
			MESSAGE_END();

			MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->edict());
			WRITE_SHORT(0);
			MESSAGE_END();
		}
	}

	// the pickup that made us schedules its respawn
	// VERIFY: guard, the original crashes here: it calls through Instance()'s result unchecked
	// (NULL once the pickup's edict is freed)
	if (pev->owner && CBaseEntity::Instance(pev->owner))
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	pev->classname = MAKE_STRING("assassin");
	DEClientLeave();

	if (m_pOwner)
	{
		CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;
		if (pOwner->m_bInDriveEnt && pOwner->m_pCurDriveEnt && pOwner->m_pCurDriveEnt->pev == pev)
		{
			// the driver dies with her
			UTIL_SetOrigin(m_pOwner->pev, pOwner->m_pCurDriveEnt->pev->origin + Vector(0, 0, 64));
			m_pOwner->pev->effects |= EF_NODRAW;
			m_pOwner->pev->health = -1;
			m_pOwner->Killed(pevAttacker, GIB_NEVER);
		}
	}

	if ((pev->health < -30 && iGib != GIB_NEVER) || iGib == GIB_ALWAYS)
	{
		GibMonster();
	}
	else
	{
		SetDeathAnim();
		SetThink(&CDriveAssassin::DyingThink);
		pev->nextthink = gpGlobals->time + 0.1;
		pev->dmgtime = gpGlobals->time + 5.0;
	}
}

// orig 0x138de0: chase camera 180 units behind the pivot along the driver's view (holding
// reload frees the view), or first person with UseFirstPerson
void CDriveAssassin::DEClientPreThink()
{
	CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;

	if ((m_pOwner->pev->button & IN_RELOAD) == 0 || PREFS_KeyLoopUpBool(ASSASSIN_PREFS, "UseFirstPerson", false))
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
	// lead the camera by the walk velocity since it was measured
	if (m_vecMoveVel.Length() > 0.01 && m_fMoveVelTime < gpGlobals->time)
		vecOrg = GetCamRotOrg() + m_vecMoveVel * (gpGlobals->time - m_fMoveVelTime);

	Vector vecEnd = vecOrg - gpGlobals->v_forward * 180;

	if (!PREFS_KeyLoopUpBool(ASSASSIN_PREFS, "UseFirstPerson", false))
	{
		TraceResult tr;
		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine(vecOrg, vecEnd, ignore_monsters, edict(), &tr);

		Vector vecCam = tr.vecEndPos;
		if (tr.flFraction < 1.0)
			vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;

		int i = 0;
		if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
		{
			for (i = 1; i < 48; i++)
			{
				if (UTIL_PointContents(vecCam) == CONTENTS_EMPTY)
					break;
				vecCam = tr.vecEndPos + (float)i * gpGlobals->v_forward * 8;
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

		pOwner->m_afPhysicsFlags = 0;
		UTIL_SetOrigin(m_pOwner->pev, vecCam);

		// cl_de_alpha, unless cloaked
		if (pOwner->m_fDrvEntAlpha < 1 && pOwner->m_fDrvEntAlpha > 0 && !m_bCloaked)
		{
			m_pOwner->pev->euser2 = edict();
			m_pOwner->pev->iuser3 = 4;
			m_pOwner->pev->fuser1 = 2.0;
			float flAmt = 0;
			if (pOwner->m_fDrvEntAlpha * 255 >= 0)
			{
				flAmt = 255;
				if (pOwner->m_fDrvEntAlpha * 255 <= 255)
					flAmt = pOwner->m_fDrvEntAlpha * 255;
			}
			m_pOwner->pev->fuser2 = flAmt;
			m_pOwner->pev->fuser3 = 0;
			m_pOwner->pev->vuser1 = g_vecZero;
			return;
		}

		m_pOwner->pev->euser2 = NULL;
		m_pOwner->pev->iuser3 = 0;
		m_pOwner->pev->fuser1 = 0;
	}
	else
	{
		m_pOwner->pev->view_ofs = g_vecZero;
		UTIL_SetOrigin(m_pOwner->pev, vecOrg);
		m_pOwner->pev->iuser3 = 4;
		m_pOwner->pev->fuser1 = 1.0;
	}

	m_pOwner->pev->fuser2 = 0;
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
}

// orig 0x139410
void CDriveAssassin::DEClientPostThink()
{
	((CBasePlayer*)m_pOwner)->m_afPhysicsFlags = 0;
}

// orig 0x139430: the player becomes an invisible camera following the assassin
void CDriveAssassin::DEClientEnter(CBasePlayer* pPlayer)
{
	m_pOwner = pPlayer;
	pPlayer->pev->deadflag = DEAD_DEAD;

	CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;
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
	pOwner->RemoveAllItems(false);

	pOwner->m_bInDriveEnt = true;
	pOwner->m_pCurDriveEnt = this;
	pOwner->m_fDrvEntEjectTime = gpGlobals->time + 5.0;

	if (pOwner->m_pActiveItem)
	{
		pOwner->m_pActiveItem->Holster();
		m_pOwner->pev->weaponmodel = 0;
	}

	pOwner->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT;
	pOwner->m_afPhysicsFlags = 0;
	m_iClientHealth = -1;
	m_iClientArmor = -1;

	MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, m_pOwner->edict());
	WRITE_BYTE(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->edict());
	WRITE_BYTE(pOwner->m_iHideHUD);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgBattery, NULL, m_pOwner->edict());
	WRITE_SHORT(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgFlashBattery, NULL, m_pOwner->edict());
	WRITE_BYTE(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgTrain, NULL, m_pOwner->edict());
	WRITE_BYTE(0);
	MESSAGE_END();

	pev->ideal_yaw = m_pOwner->pev->v_angle.y;
	pev->idealpitch = m_pOwner->pev->v_angle.x;

	CLIENT_COMMAND(m_pOwner->edict(), "cl_forwardspeed 0\ncl_backspeed 0\ncl_sidespeed 0\n");
}

// orig 0x1397f0: give the driver his body and view back. m_bInDriveEnt and m_pCurDriveEnt stay
// set; the callers clear them.
void CDriveAssassin::DEClientLeave()
{
	if (!m_pOwner || !m_pOwner->pev || !m_pOwner->pev->pContainingEntity || m_pOwner->pev->pContainingEntity->free)
		return;

	CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;
	if (!pOwner->m_bInDriveEnt)
		return;

	CLIENT_COMMAND(m_pOwner->edict(), "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");

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

	pOwner->m_fDrvEntEjectTime = gpGlobals->time + 20.0;
	pOwner->m_fDrcEntTransTime = gpGlobals->time + 15.0;

	UTIL_SetSize(m_pOwner->pev, Vector(-16, -16, -36), Vector(16, 16, 36));

	m_pOwner->pev->euser2 = NULL;
	m_pOwner->pev->iuser3 = 0;
	m_pOwner->pev->fuser1 = 0;
	m_pOwner->pev->fuser2 = 0;
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;

	if (pOwner->m_pActiveItem)
		pOwner->m_pActiveItem->Deploy();

	pOwner->m_iHideHUD &= ~(HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT);

	SET_VIEW(m_pOwner->edict(), m_pOwner->edict());
	SetupVisibility(m_pOwner->edict(), m_pOwner->edict(), NULL, NULL);
}

// orig 0x139b00: step out on top; the assassin gibs
void CDriveAssassin::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool(ASSASSIN_PREFS, "AllowEject", false))
	{
		if (CVAR_GET_FLOAT("sv_allowglobaleject") != 1 && g_pGameRules->IsMultiplayer() && !g_pGameRules->IsCoOp())
			return;
	}

	CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;
	if (!pOwner->m_bInDriveEnt || !pOwner->m_pCurDriveEnt || pOwner->m_pCurDriveEnt->pev != pev)
		return;
	if (!(pOwner->m_pCurDriveEnt->pev->health > 0))
		return;
	if (!(pOwner->m_fDrvEntEjectTime < gpGlobals->time))
		return;

	DEClientLeave();
	pOwner->UpdateClientData();
	pOwner->m_bInDriveEnt = false;
	Killed(pev, GIB_ALWAYS);

	UTIL_SetOrigin(m_pOwner->pev, pev->origin + Vector(0, 0, 48));
	m_pOwner->pev->v_angle = pev->angles;
	m_pOwner->pev->angles = m_pOwner->pev->v_angle;
	m_pOwner->pev->angles.x = m_pOwner->pev->v_angle.x = 0;
	m_pOwner->pev->angles.z = m_pOwner->pev->v_angle.z = 0;
	m_pOwner->pev->fixangle = 1;
	pOwner->m_pCurDriveEnt = NULL;
}

//=========================================================
// CDriveAssassinPickup: the standing, half-transparent assassin a player uses to get in
//=========================================================
class CDriveAssassinPickup : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	int ObjectCaps() override;
	int Classify() override;
	void DeathNotice(entvars_t* pevChild) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	int BloodColor() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int GetRCCaps() override;

	// new in this class (original vtable slot 150); nothing in the original calls it
	virtual bool IsTeleFraggabe();

	CBaseEntity* GetSpawnOrg();
	void EXPORT PAnimThink();
	void EXPORT DoRespawn();

	CBaseEntity* m_pGiven; // 0x27c, the drive_assassin made from this pickup
	bool m_bTakeable;	   // 0x280
};

// orig 0x139d10
LINK_ENTITY_TO_CLASS(pickup_drive_assassin, CDriveAssassinPickup);

TYPEDESCRIPTION CDriveAssassinPickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveAssassinPickup, m_pGiven, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveAssassinPickup, m_bTakeable, FIELD_BOOLEAN),
};

// orig 0x139d40 (Save), 0x139d90 (Restore)
IMPLEMENT_SAVERESTORE(CDriveAssassinPickup, CBaseMonster);

// orig 0x139de0: move to a free info_drive_assassin_spawn (and use it up), else go away
void CDriveAssassinPickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;

	SET_MODEL(ENT(pev), "models/hassassin.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));
	UTIL_SetOrigin(pev, pev->origin);

	pev->takedamage = DAMAGE_NO;
	pev->health = 1;

	pev->sequence = LookupSequence(RANDOM_FLOAT(0, 1) >= 0.5 ? "idle1" : "idle3");
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 0xFF);
	InitBoneControllers();

	SetUse(&CDriveAssassinPickup::Use);
	SetThink(&CDriveAssassinPickup::PAnimThink);
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

// orig 0x13a050
void CDriveAssassinPickup::Precache()
{
	PRECACHE_MODEL("models/hassassin.mdl");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_assassin");
}

// orig 0x13a0a0
CBaseEntity* CDriveAssassinPickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_assassin_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (FNullEnt(pSpot->pev))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x13a120
bool CDriveAssassinPickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return false;
}

// orig 0x13a130: fade in to the hologram look; while taken, end the drive after AssassinMaxDriveTime
void CDriveAssassinPickup::PAnimThink()
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
			STOP_SOUND(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav");
		}
	}
}

// orig 0x13a2d0: a walking player gets in. It reads pCaller, not pActivator (the same for +use).
void CDriveAssassinPickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!m_bTakeable)
		return;
	if (!pCaller->IsPlayer() || !pCaller)
		return;

	CBasePlayer* pPlayer = (CBasePlayer*)pCaller;
	if (pPlayer->m_fDrcEntTransTime > gpGlobals->time)
		return;
	if (pPlayer->m_bInDriveEnt)
		return;
	if (pCaller->Classify() != CLASS_PLAYER)
		return;
	if (strcmp(STRING(pCaller->pev->classname), "player") != 0)
		return;

	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_assassin", pev->origin, pev->angles, edict());
	pCaller->pev->angles = Vector(0, pev->angles.y, 0);
	pCaller->pev->v_angle = pCaller->pev->angles;
	pCaller->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGiven = pPlayer->m_pCurDriveEnt;

	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDriveAssassinPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinMaxDriveTime", 600) + gpGlobals->time;
}

// orig 0x13a4d0
void CDriveAssassinPickup::DoRespawn()
{
	m_bTakeable = true;
	m_pGiven = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;

	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);

	SetThink(&CDriveAssassinPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x13a590: our drive_assassin died (CDriveAssassin::Killed)
void CDriveAssassinPickup::DeathNotice(entvars_t* pevChild)
{
	if (!m_bTakeable)
	{
		pev->nextthink = PREFS_KeyLoopUpFloat(ASSASSIN_PREFS, "AssassinRespawnDelayAfterDeath", 5) + gpGlobals->time;
		SetThink(&CDriveAssassinPickup::DoRespawn);
	}
}

//=========================================================
// CDrvAssSpawner: places mp_driveassassin pickups 15 seconds into the map
//=========================================================
class CDrvAssSpawner : public CBaseEntity
{
public:
	void Spawn() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	int ObjectCaps() override;

	void EXPORT DrvAThink();

	bool m_bDummy; // 0x34
};

// orig 0x13a600
void CreateDrvAssassinSpawner()
{
	if (g_pGameRules->IsMultiplayer() && !UTIL_FindEntityByClassname(NULL, "drive_assassin_spawner"))
		CBaseEntity::Create("drive_assassin_spawner", g_vecZero, g_vecZero, NULL);
}

// orig 0x13a660
LINK_ENTITY_TO_CLASS(drive_assassin_spawner, CDrvAssSpawner);

TYPEDESCRIPTION CDrvAssSpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvAssSpawner, m_bDummy, FIELD_BOOLEAN),
};

// orig 0x13a690 (Save), 0x13a6e0 (Restore)
IMPLEMENT_SAVERESTORE(CDrvAssSpawner, CBaseEntity);

// orig 0x13a730
void CDrvAssSpawner::DrvAThink()
{
	int iCount = (int)CVAR_GET_FLOAT("mp_driveassassin");
	for (int i = 0; i < iCount; i++)
		CBaseEntity::Create("pickup_drive_assassin", g_vecZero, g_vecZero, NULL);

	UTIL_Remove(this);
}

// orig 0x13a7c0
void CDrvAssSpawner::Spawn()
{
	pev->classname = MAKE_STRING("drive_assassin_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvAssSpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15.0;
}

// orig 0x13a860
int CDrvAssSpawner::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}

// orig 0x13a8c0
int CDriveAssassinPickup::ObjectCaps()
{
	if (m_bTakeable)
		return FCAP_IMPULSE_USE | FCAP_ACROSS_TRANSITION;
	return FCAP_ACROSS_TRANSITION;
}

// orig 0x13a8e0
int CDriveAssassinPickup::GetRCCaps()
{
	return 0;
}

// orig 0x13a8f0
bool CDriveAssassinPickup::IsTeleFraggabe()
{
	return false;
}

// orig 0x13a900
int CDriveAssassinPickup::BloodColor()
{
	return DONT_BLEED;
}

// orig 0x13a910
int CDriveAssassinPickup::Classify()
{
	return CLASS_NONE;
}

//=========================================================
// CDriveAssassin's small virtuals (the original emits them after the spawner's)
//=========================================================

// orig 0x13a970
void CDriveAssassin::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;

	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

// orig 0x13a9f0
int CDriveAssassin::GetRCCaps()
{
	return 0x7fe3fff;
}

// orig 0x13aa00
int CDriveAssassin::GetGuassDistOut()
{
	return 56;
}

// orig 0x13aa10
int CDriveAssassin::GetGuassHull()
{
	return human_hull;
}

// orig 0x13aa20
int CDriveAssassin::BloodColor()
{
	return BLOOD_COLOR_RED;
}

// orig 0x13aa30
int CDriveAssassin::Classify()
{
	return CLASS_HUMAN_MILITARY;
}
