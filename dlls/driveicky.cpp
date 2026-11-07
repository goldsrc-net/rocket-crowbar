/***
*
*	Rocket Crowbar 1.9: driveicky.cpp. The ichthyosaur a player drives (drive_icky): it swims,
*	bites (attack, attack2 for a double bite) and charges (jump). Players get one by using a
*	pickup_drive_icky, which drive_icky_spawner places at the map's info_drive_icky_spawn points.
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

const char* g_pDIIdleSounds[] = {
	"ichy/ichy_idle1.wav",
	"ichy/ichy_idle2.wav",
	"ichy/ichy_idle3.wav",
	"ichy/ichy_idle4.wav",
};

const char* g_pDIAlertSounds[] = {
	"ichy/ichy_alert2.wav",
	"ichy/ichy_alert3.wav",
};

const char* g_pDIAttackSounds[] = {
	"ichy/ichy_attack1.wav",
	"ichy/ichy_attack2.wav",
};

const char* g_pDIBiteSounds[] = {
	"ichy/ichy_bite1.wav",
	"ichy/ichy_bite2.wav",
};

const char* g_pDIPainSounds[] = {
	"ichy/ichy_pain2.wav",
	"ichy/ichy_pain3.wav",
	"ichy/ichy_pain5.wav",
};

const char* g_pDIDieSounds[] = {
	"ichy/ichy_die2.wav",
	"ichy/ichy_die4.wav",
};

class CDriveIcky : public CBaseDriveEnt
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

	void EXPORT DIThink();
	void DoBite();
	void DoMovement();
	void DoCharge();
	void DoAngles();
	void DoEye();
	void UpdateOwnerHUD();
	void EXPORT DeathTouch(CBaseEntity* pOther);
	void EXPORT DyingThink();
	void DoBiteDamage();

	// Unsaved (no save table in the original). m_pOwner and m_fFrameTime as the other drive
	// monsters' save tables name these slots.
	// m_pOwner (orig +0x27c, the driving player) is CBaseDriveEnt's
	float m_fFrameTime;		 // orig +0x280
	float m_fNextPainSound;	 // orig +0x284
	bool m_bCanIdle;		 // orig +0x288, the idle animation may play. VERIFY name
	bool m_bCanSwim;		 // orig +0x289, forward/back swimming allowed (not charging). VERIFY name
	bool m_bFastMove;		 // orig +0x28a, moving fast: slower turns, no turn animation. VERIFY name
	float m_fNextIdleSound;	 // orig +0x28c, idle and charge sounds. VERIFY name
	float m_fNextBlink;		 // orig +0x290. VERIFY name
	bool m_bInBite;			 // orig +0x298
	float m_fNextBite;		 // orig +0x29c
	bool m_bInCharge;		 // orig +0x2a0
	float m_fChargeOffTime;	 // orig +0x2a4, the charge ends this long after jump is let go. VERIFY name
	float m_fNextCharge;	 // orig +0x2a8
	int m_iClientHealth;	 // orig +0x2ac
	Vector m_vecEnterOrigin; // orig +0x2b0, where it was when the driver got in; never read. VERIFY name
	float m_fEnterTime;		 // orig +0x2bc, when the driver got in; never read. VERIFY name
};

LINK_ENTITY_TO_CLASS(drive_icky, CDriveIcky); // orig 0x16a690

// orig 0x16a6c0
void CDriveIcky::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_icky");
	SET_MODEL(ENT(pev), "models/icky.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-24, -24, -24), Vector(24, 24, 24));
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_AIM;
	pev->health = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyHealth", 320.0);
	pev->movetype = MOVETYPE_BOUNCE;
	pev->gravity = 0.0001;
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	pev->skin = 1;
	InitBoneControllers();
	pev->sequence = LookupSequence("idle");
	ResetSequenceInfo();
	pev->frame = 0;
	m_fNextPainSound = gpGlobals->time;
	pev->framerate = 1.0;
	m_bCanIdle = true;
	m_bInBite = false;
	m_fNextIdleSound = RANDOM_FLOAT(0.8, 2) + gpGlobals->time;
	m_fNextBlink = RANDOM_FLOAT(3, 4) + gpGlobals->time;
	pev->skin = 1;
	m_bInCharge = false;
	m_fChargeOffTime = gpGlobals->time;
	m_fNextCharge = gpGlobals->time;
	m_fNextBite = gpGlobals->time;

	SetThink(&CDriveIcky::DIThink);
	pev->nextthink = 0.1 + gpGlobals->time;
}

// orig 0x16a920
void CDriveIcky::Precache()
{
	PRECACHE_MODEL("models/icky.mdl");
	for (const char* psz : g_pDIIdleSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_pDIAlertSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_pDIAttackSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_pDIBiteSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_pDIPainSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_pDIDieSounds)
		PRECACHE_SOUND(psz);
}

// orig 0x16aa10
const char* CDriveIcky::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;
	return STRING(m_pOwner->pev->netname);
}

// orig 0x16aa80
void CDriveIcky::DIThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.01;

	if (m_pOwner)
	{
		bool bGone = FNullEnt(m_pOwner->pev);
		if (!bGone && m_pOwner->pev && m_pOwner->pev->pContainingEntity && m_pOwner->pev->pContainingEntity->pvPrivateData && !m_pOwner->pev->pContainingEntity->free && UTIL_PlayerByIndex(ENTINDEX(m_pOwner->pev->pContainingEntity)))
		{
			DispatchAnimEvents(m_fFrameTime);
			m_pOwner->pev->rendermode = kRenderTransColor;
			m_pOwner->pev->renderamt = 0;
			m_pOwner->pev->effects |= EF_NODRAW;
			m_bCanIdle = true;
			m_bCanSwim = true;
			m_bFastMove = false;
			UpdateOwnerHUD();

			if ((pev->flags & FL_FROZEN) != 0)
			{
				pev->velocity = g_vecZero;
				pev->velocity.z = CVAR_GET_FLOAT("sv_gravity") * pev->gravity * m_fFrameTime + pev->velocity.z;
				return;
			}

			DoBite();
			DoMovement();
			DoCharge();
			DoAngles();
			DoEye();

			if (!m_bCanIdle)
				return;

			if (m_fNextIdleSound < gpGlobals->time)
			{
				m_fNextIdleSound = RANDOM_FLOAT(3, 4.8) + gpGlobals->time;
				int iPitch = RANDOM_LONG(95, 105);
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDIIdleSounds[RANDOM_LONG(0, 3)], 1.0, 0.6, 0, iPitch);
			}

			if (pev->sequence == LookupSequence("idle"))
				return;
			if (!m_fSequenceFinished && !m_fSequenceLoops)
				return;
			pev->sequence = LookupSequence("idle");
			ResetSequenceInfo();
			return;
		}
	}

	// the driver is gone
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("ichthyosaur");
	g_pevLastInflictor = pev;
	Killed(pev, GIB_NORMAL);
}

// orig 0x16ad50: attack bites left or right, attack2 bites twice. The damage is done on the
// bite animation's events (HandleAnimEvent).
void CDriveIcky::DoBite()
{
	if (m_bInBite)
	{
		if (!m_fSequenceFinished)
			m_bCanIdle = false;
		else
			m_bInBite = false;
		return;
	}

	if ((m_pOwner->pev->button & (IN_ATTACK | IN_ATTACK2)) == 0 || m_fNextBite > gpGlobals->time)
		return;

	m_fNextBite = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyBiteRefire", 0.4) + gpGlobals->time;
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 2.0;
	if (!m_bInCharge)
	{
		int iPitch = RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDIAttackSounds[RANDOM_LONG(0, 1)], 1.0, 0.6, 0, iPitch);
	}

	if ((m_pOwner->pev->button & IN_ATTACK2) == 0)
	{
		float flRate = 1.0;
		switch (RANDOM_LONG(1, 2))
		{
		case 1:
			pev->sequence = LookupSequence("bite_l");
			flRate = 1.2;
			break;
		case 2:
			pev->sequence = LookupSequence("bite_r");
			flRate = 1.3;
			break;
		}
		ResetSequenceInfo();
		pev->frame = 0;
		pev->framerate = flRate;
	}
	else
	{
		m_fNextBite = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyDoubleBiteRefire", 1.2) + gpGlobals->time;
		pev->sequence = LookupSequence("srattack1");
		ResetSequenceInfo();
		pev->frame = 0;
		pev->framerate = 1.0;
	}
	m_bCanIdle = false;
	m_bInBite = true;
}

// orig 0x16af90: friction, gravity, and forward (or back) swimming
void CDriveIcky::DoMovement()
{
	float flFriction = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyFriction", 2.4);
	if (pev->waterlevel > 1)
		flFriction = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyWaterFriction", 3.8);
	if (m_bInBite)
		flFriction = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyBitingFriction", 3.2);

	pev->velocity.z = CVAR_GET_FLOAT("sv_gravity") * pev->gravity * m_fFrameTime + pev->velocity.z;

	float flSpeed = pev->velocity.Length();
	if (flSpeed > 0.1)
	{
		float flControl = flSpeed;
		if (flSpeed < 6.0)
			flControl = 6.0;
		float flNewSpeed = flSpeed - flControl * flFriction * m_fFrameTime;
		if (flNewSpeed < 0)
			flNewSpeed = 0;
		pev->velocity = pev->velocity * (flNewSpeed / flSpeed);
	}

	if (pev->velocity.Length() > 80)
	{
		m_bFastMove = true;
		m_bCanIdle = false;
	}

	if (!m_bCanSwim || m_bInCharge || (m_pOwner->pev->button & (IN_FORWARD | IN_BACK)) == 0)
		return;

	m_bCanIdle = false;
	m_bFastMove = true;
	if (m_fNextIdleSound < gpGlobals->time)
	{
		m_fNextIdleSound = RANDOM_FLOAT(2, 3) + gpGlobals->time;
		int iPitch = RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDIIdleSounds[RANDOM_LONG(0, 3)], 1.0, 0.6, 0, iPitch);
	}

	if (pev->sequence != LookupSequence("swim") && !m_bInBite)
	{
		pev->sequence = LookupSequence("swim");
		ResetSequenceInfo();
	}

	UTIL_MakeAimVectors(pev->angles);
	float flFrac = 1.0;
	if (pev->waterlevel > 1)
		flFrac = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyWaterSpeedFrac", 0.75);
	if (m_bInBite)
		flFrac = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyBiteSpeedFrac", 0.6) * flFrac;

	float flMaxSpeed, flAccel;
	Vector vecDir;
	if ((m_pOwner->pev->button & IN_FORWARD) == 0)
	{
		flMaxSpeed = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyBackSpeed", 220.0) * flFrac;
		flAccel = flFrac * PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyBackAccel", 300.0);
		pev->framerate = 1.0;
		vecDir = -gpGlobals->v_forward;
	}
	else
	{
		flMaxSpeed = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyForwardSpeed", 380.0) * flFrac;
		flAccel = flFrac * PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyForwardAccel", 450.0);
		pev->framerate = 1.0;
		vecDir = gpGlobals->v_forward;
	}

	float flAdd = flMaxSpeed - DotProduct(pev->velocity, vecDir);
	if (flAdd > 0)
	{
		float flAccelSpeed = flAccel * m_fFrameTime * flMaxSpeed;
		if (flAccelSpeed > flAdd)
			flAccelSpeed = flAdd;
		pev->velocity = pev->velocity + vecDir * flAccelSpeed;
	}
}

// orig 0x16b430: hold jump to charge forward; it lasts until a moment after jump is let go
void CDriveIcky::DoCharge()
{
	if (!m_bInCharge)
	{
		if ((m_pOwner->pev->button & IN_JUMP) == 0)
			return;
		if (gpGlobals->time < m_fNextCharge)
			return;
		if (m_bInBite)
			return;

		m_fChargeOffTime = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyChargeOffDelay", 0.3) + gpGlobals->time;
		m_bInCharge = true;
		int iPitch = RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDIAttackSounds[RANDOM_LONG(0, 1)], 1.0, 0.6, 0, iPitch);
	}

	if ((m_pOwner->pev->button & IN_JUMP) != 0)
		m_fChargeOffTime = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyChargeOffDelay", 0.3) + gpGlobals->time;

	if (m_fChargeOffTime < gpGlobals->time)
	{
		m_bInCharge = false;
		return;
	}

	m_bCanIdle = false;
	m_bCanSwim = false;
	m_bFastMove = true;
	if (!m_bInBite && pev->sequence != LookupSequence("thrust"))
	{
		pev->sequence = LookupSequence("thrust");
		ResetSequenceInfo();
	}
	m_fNextCharge = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyChargeRefire", 1.2) + gpGlobals->time;

	if (m_fNextIdleSound < gpGlobals->time)
	{
		m_fNextIdleSound = RANDOM_FLOAT(0.4, 0.8) + gpGlobals->time;
		int iPitch = RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDIAttackSounds[RANDOM_LONG(0, 1)], 1.0, 0.6, 0, iPitch);
	}

	UTIL_MakeAimVectors(pev->angles);
	float flMaxSpeed = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyChargeSpeed", 680.0);
	float flAccel = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyChargeAccel", 500.0);
	if (m_bInBite)
	{
		flMaxSpeed = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyBitingChargeSpeed", 490.0);
		flAccel = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyBitingChargeAccel", 350.0);
	}
	pev->framerate = 1.0;

	Vector vecDir = gpGlobals->v_forward;
	float flAdd = flMaxSpeed - DotProduct(pev->velocity, vecDir);
	if (flAdd > 0)
	{
		float flAccelSpeed = flAccel * m_fFrameTime * flMaxSpeed;
		if (flAccelSpeed > flAdd)
			flAccelSpeed = flAdd;
		pev->velocity = pev->velocity + vecDir * flAccelSpeed;
	}
}

// orig 0x16b810: turn toward the driver's aim; a sharp turn while slow plays a turn animation
void CDriveIcky::DoAngles()
{
	float flYawSpeed = 1300;
	float flPitchSpeed = m_fFrameTime * 400;
	bool bTurnAnim = false;
	if (m_bInBite)
		flYawSpeed = 1100;
	flYawSpeed = flYawSpeed * m_fFrameTime;
	if (m_bFastMove)
	{
		flYawSpeed = m_fFrameTime * 900;
		flPitchSpeed = m_fFrameTime * 350;
	}
	if (m_bInCharge)
	{
		flYawSpeed = m_fFrameTime * 240;
		flPitchSpeed = m_fFrameTime * 200;
	}

	pev->angles.x = UTIL_Approach(pev->idealpitch, pev->angles.x, flPitchSpeed);
	float flPitch = -80;
	if (pev->angles.x >= -80)
	{
		flPitch = 80;
		if (!(pev->angles.x > 80))
			flPitch = pev->angles.x;
	}
	pev->angles.x = flPitch;

	// how far it is from the ideal yaw (ChangeYaw with a huge rate, then undone)
	float flYaw = pev->angles.y;
	float flDelta = fabs(ChangeYaw(0x2000));
	pev->angles.y = flYaw;
	if (flDelta < 0.001)
		return;

	if (flDelta > 120 && !m_bFastMove && !m_bInBite)
	{
		flYawSpeed = m_fFrameTime * 2000;
		bTurnAnim = true;
	}

	float flTurned = ChangeYaw((int)flYawSpeed);
	if (bTurnAnim && !m_bFastMove && !m_bInBite)
	{
		pev->sequence = LookupSequence(flTurned > 0 ? "rturn" : "lturn");
		ResetSequenceInfo();
		pev->frame = 0;
	}
}

// orig 0x16ba10: skin 1 open eye, 2 blink, 0 charging, 3 biting
void CDriveIcky::DoEye()
{
	if (!m_bInBite)
	{
		if (!m_bInCharge)
		{
			if (pev->skin == 1)
			{
				if (!(m_fNextBlink < gpGlobals->time))
					return;
				m_fNextBlink = gpGlobals->time + 0.2;
				pev->skin = 2;
				return;
			}
			if (!(m_fNextBlink < gpGlobals->time))
				return;
			m_fNextBlink = RANDOM_FLOAT(3, 4) + gpGlobals->time;
			pev->skin = 1;
			return;
		}
		pev->skin = 0;
	}
	else
	{
		pev->skin = 3;
	}
	m_fNextBlink = gpGlobals->time - 1.0;
}

// orig 0x16bb20: the driver's HUD shows the ichthyosaur's health in percent
void CDriveIcky::UpdateOwnerHUD()
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
	WRITE_BYTE(iHealth);
	MESSAGE_END();
	m_iClientHealth = (int)pev->health;
}

// orig 0x16bc40
bool CDriveIcky::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_pOwner)
	{
		if (CBaseEntity::Instance(pevAttacker->pContainingEntity) && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, CBaseEntity::Instance(pevAttacker->pContainingEntity)))
			return false;
		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	if ((bitsDamageType & DMG_DROWN) != 0)
		return false;
	if ((bitsDamageType & DMG_MORTAR) != 0)
		flDamage = flDamage * 0.15;
	if ((bitsDamageType & DMG_BURN) != 0)
		flDamage = 0.66 * flDamage;

	// VERIFY: the original writes this through m_pOwner without checking it for NULL
	if (m_pOwner)
		m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 2.0;

	bool bResult = CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
	if (!bResult)
		return false;

	if (m_fNextPainSound < gpGlobals->time)
	{
		int iPitch = RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDIPainSounds[RANDOM_LONG(0, 2)], 1.0, 0.6, 0, iPitch);
		m_fNextPainSound = RANDOM_FLOAT(0.3, 0.6) + gpGlobals->time;
	}
	return bResult;
}

// orig 0x16be30: the corpse touched something: let it fade now
void CDriveIcky::DeathTouch(CBaseEntity* pOther)
{
	pev->dmgtime = gpGlobals->time - 1.0;
	SetTouch(NULL);
}

// orig 0x16be70: float belly up to the surface, then fade
void CDriveIcky::DyingThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->sequence == LookupSequence("bellyup") || m_fSequenceFinished)
	{
		pev->deadflag = DEAD_DEAD;
		if (pev->sequence != LookupSequence("bellyup"))
		{
			pev->sequence = LookupSequence("bellyup");
			ResetSequenceInfo();
			pev->frame = 0;
		}
		pev->angles.x = UTIL_ApproachAngle(0, pev->angles.x, 20);
		pev->velocity = pev->velocity * 0.8;
		if (pev->velocity.z < 128)
			pev->velocity.z += 8;

		if (m_fSequenceFinished)
		{
			SetTouch(&CDriveIcky::DeathTouch);
			if (m_fSequenceFinished)
			{
				if (gpGlobals->time < pev->dmgtime)
					return;
				FadeMonster();
				return;
			}
		}
	}

	DispatchAnimEvents(StudioFrameAdvance());
}

// orig 0x16c010
void CDriveIcky::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->deadflag != DEAD_NO)
		return;

	pev->sequence = LookupSequence(RANDOM_FLOAT(0, 1) >= 0.5 ? "die2" : "die1");
	ResetSequenceInfo();
	pev->frame = 0;
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_FLY;
	pev->gravity = 3.0;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	if (pev->owner)
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	int iPitch = RANDOM_LONG(95, 105);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDIDieSounds[RANDOM_LONG(0, 1)], 1.0, 0.6, 0, iPitch);

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_BYTE(HIDEHUD_HEALTH);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_BYTE(0);
		MESSAGE_END();
	}

	pev->classname = MAKE_STRING("ichthyosaur");
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

	pev->dmgtime = gpGlobals->time + 14.0;
	SetThink(&CDriveIcky::DyingThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x16c3a0: the bite lands on the best target in front of the jaws
void CDriveIcky::DoBiteDamage()
{
	float flDamage;
	if (!m_bInCharge)
	{
		pev->classname = MAKE_STRING("ichthyosaur_bite");
		if (pev->sequence == LookupSequence("srattack1"))
			flDamage = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyDoubleBiteDamage", 45.0);
		else
			flDamage = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyBiteDamage", 70.0);
	}
	else
	{
		if (pev->sequence == LookupSequence("srattack1"))
			flDamage = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyChargeDoubleBiteDamage", 55.0);
		else
			flDamage = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyChargeBiteDamage", 95.0);
		pev->classname = MAKE_STRING("ichthyosaur_charge_bite");
	}
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 3.0;

	UTIL_MakeAimVectors(pev->angles);
	Vector vecSrc = pev->origin + gpGlobals->v_forward * 20;
	CBaseEntity* pBest = NULL;
	float flBestDist = 9999;
	TraceResult trBest;
	TraceResult tr;

	// the nearest damageable thing within 72 units, in a cone ahead, in sight
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, 72)) != NULL)
	{
		if (pEntity->pev->takedamage == DAMAGE_NO)
			continue;

		Vector vecCenter = pEntity->Center();
		Vector vecDelta = vecCenter - vecSrc;
		float flDist = vecDelta.Length();
		if (flDist > flBestDist)
			continue;

		UTIL_MakeAimVectors(pev->angles);
		if (!(DotProduct(vecDelta.Normalize(), gpGlobals->v_forward) > 0.6))
			continue;

		UTIL_TraceLine(vecSrc, vecCenter, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.pHit == pEntity->pev->pContainingEntity || tr.flFraction >= 1.0)
		{
			trBest = tr;
			flBestDist = flDist;
			pBest = pEntity;
		}
	}

	if (!pBest)
	{
		// nothing in the cone: whatever a big hull swept 72 units ahead hits
		UTIL_MakeAimVectors(pev->angles);
		Vector vecEnd = vecSrc + gpGlobals->v_forward * 72;
		UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, large_hull, ENT(pev), &tr);
		if (!tr.pHit)
			goto done;
		pBest = CBaseEntity::Instance(tr.pHit);
		if (!pBest)
			goto done;
		if (!strcmp(STRING(pBest->pev->classname), "worldspawn") || pBest->pev->takedamage == DAMAGE_NO)
			goto done;

		Vector vecDelta = pBest->Center() - vecSrc;
		if (!pBest->IsBSPModel())
		{
			UTIL_MakeAimVectors(pev->angles);
			if (DotProduct(vecDelta.Normalize(), gpGlobals->v_forward) <= 0.6)
				goto done;
		}
		trBest = tr;
		if (!pBest)
			return;
	}

	{
		entvars_t* pevAttacker = m_pOwner ? m_pOwner->pev : pev;
		UTIL_MakeAimVectors(pev->angles);

		if (!pBest->IsBSPModel())
		{
			pBest->pev->punchangle.z = -19;
			pBest->pev->punchangle.x = 6;

			// it throws what it bites aside (forward too when charging)
			int iCap = 0x40000; // VERIFY: name of RC cap bit 18 (can be knocked about)
			if (pBest->HasRCCap(iCap))
			{
				if (!m_bInCharge)
					pBest->pev->velocity = pBest->pev->velocity - gpGlobals->v_right * 330;
				else
					pBest->pev->velocity = pBest->pev->velocity - gpGlobals->v_right * 330 + gpGlobals->v_forward * 300;

				if (pBest->IsPlayer())
				{
					pBest->pev->angles.x = RANDOM_FLOAT(-35, 35) + pBest->pev->angles.x;
					pBest->pev->angles.y = RANDOM_FLOAT(-90, 90) + pBest->pev->angles.y;
					pBest->pev->angles.z = 0;
					pBest->pev->fixangle = 1;
				}
			}

			// biting something living heals it
			int iHealCap = 0x800; // VERIFY: name of RC cap bit 11 (living, gives health when bitten)
			if (pBest->HasRCCap(iHealCap))
			{
				pev->health = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyHealthForABite", 35.0) + pev->health;
				if (pev->health > pev->max_health)
					pev->health = pev->max_health;
			}

			UTIL_BloodDrips(vecSrc, gpGlobals->v_forward, pBest->BloodColor(), (int)(flDamage * 1.5));
			ClearMultiDamage();
			pBest->TraceAttack(pevAttacker, flDamage, gpGlobals->v_forward, &trBest, DMG_CRUSH | DMG_SLASH);
			ApplyMultiDamage(pev, pevAttacker);
			int iPitch = RANDOM_LONG(95, 105);
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_pDIBiteSounds[RANDOM_LONG(0, 1)], 1.0, 0.6, 0, iPitch);
		}
		else
		{
			ClearMultiDamage();
			pBest->TraceAttack(pevAttacker, flDamage, gpGlobals->v_forward, &trBest, DMG_CRUSH | DMG_SLASH);
			ApplyMultiDamage(pev, pevAttacker);
			TEXTURETYPE_PlaySound(&trBest, vecSrc, vecSrc + gpGlobals->v_forward * 72, BULLET_NONE);
		}
	}

done:
	pev->classname = MAKE_STRING("drive_icky");
}

// orig 0x16d0b0: events 1 and 2 are the bites
void CDriveIcky::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	if (pEvent->event < 3 && pEvent->event > 0)
		DoBiteDamage();
}

// orig 0x16d0e0: the driver's camera, behind the ichthyosaur
void CDriveIcky::DEClientPreThink()
{
	// holding reload frees the camera from steering
	if ((m_pOwner->pev->button & IN_RELOAD) == 0)
	{
		pev->ideal_yaw = UTIL_AngleMod(m_pOwner->pev->v_angle.y);
		pev->idealpitch = -m_pOwner->pev->v_angle.x;
	}

	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;
	UTIL_MakeVectors(m_pOwner->pev->v_angle);

	Vector vecOrg = GetCamRotOrg();
	Vector vecEnd = vecOrg - gpGlobals->v_forward * 200;
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

	// cl_de_alpha: the driver can see the ichthyosaur translucent
	float flAlpha = m_pOwner->m_fDrvEntAlpha;
	if (flAlpha >= 1.0 || !(flAlpha > 0))
	{
		m_pOwner->pev->euser3 = NULL; // VERIFY: euser3 here, euser2 everywhere else (literal)
		m_pOwner->pev->iuser3 = 0;
		m_pOwner->pev->fuser1 = 0;
		m_pOwner->pev->fuser2 = 0;
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
	}
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
}

// orig 0x16d570
void CDriveIcky::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x16d590: the player becomes an invisible camera; the ichthyosaur takes its input
void CDriveIcky::DEClientEnter(CBasePlayer* pPlayer)
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
	m_vecEnterOrigin = pev->origin;
	m_fEnterTime = gpGlobals->time;

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

// orig 0x16d970: give the player back its body
void CDriveIcky::DEClientLeave()
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
	if (m_pOwner->m_pActiveItem)
		m_pOwner->m_pActiveItem->Deploy();
	m_pOwner->pev->euser2 = NULL;
	m_pOwner->pev->iuser3 = 0;
	m_pOwner->pev->fuser1 = 0;
	m_pOwner->pev->fuser2 = 0;
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
	m_pOwner->m_iHideHUD &= ~(HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT);
	SET_VIEW(m_pOwner->pev->pContainingEntity, m_pOwner->pev->pContainingEntity);
	SetupVisibility(m_pOwner->pev->pContainingEntity, m_pOwner->pev->pContainingEntity, NULL, NULL);
}

// orig 0x16dc80: the driver asks to get out; the ichthyosaur gibs and throws the driver back
void CDriveIcky::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVEICKY", "AllowEject", false))
	{
		if (CVAR_GET_FLOAT("sv_allowglobaleject") != 1 && g_pGameRules->IsMultiplayer() && !g_pGameRules->IsCoOp())
			return;
	}

	if (!m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev || !(m_pOwner->m_pCurDriveEnt->pev->health > 0))
		return;
	if (!(m_pOwner->m_fDrvEntEjectTime < gpGlobals->time))
		return;

	UTIL_MakeAimVectors(m_pOwner->m_pCurDriveEnt->pev->angles);
	Vector vecSrc = pev->origin + Vector(0, 0, 32);
	Vector vecEnd = vecSrc - gpGlobals->v_forward * 128;
	TraceResult tr;
	UTIL_TraceLine(pev->origin + Vector(0, 0, 32), vecEnd, ignore_monsters, ENT(pev), &tr);

	DEClientLeave();
	m_pOwner->UpdateClientData();
	m_pOwner->m_bInDriveEnt = false;
	Killed(pev, GIB_ALWAYS);

	Vector vecPos = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecPos = tr.vecEndPos + tr.vecPlaneNormal * 64;
	UTIL_SetOrigin(m_pOwner->pev, vecPos);

	m_pOwner->pev->velocity = gpGlobals->v_forward * -900 + Vector(0, 0, 300);
	m_pOwner->pev->v_angle = pev->angles;
	m_pOwner->pev->angles = m_pOwner->pev->v_angle;
	float flPitch = m_pOwner->pev->v_angle.x;
	m_pOwner->pev->v_angle.x = -flPitch;
	m_pOwner->pev->angles.x = -flPitch;
	m_pOwner->pev->v_angle.z = 0;
	m_pOwner->pev->angles.z = 0;
	m_pOwner->pev->fixangle = 1;
	m_pOwner->m_pCurDriveEnt = NULL;
	EMIT_SOUND_DYN(m_pOwner->pev->pContainingEntity, CHAN_BODY, "ambience/biggun3.wav", 1.0, 0.8, 0, 100);
}

// The pickup a player uses to get in an ichthyosaur. It hides while the ichthyosaur it gave
// out is alive, kills it after IckyMaxDriveTime, and comes back after it dies.
class CDriveIckyPickup : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	int ObjectCaps() override;
	int Classify() override;
	void DeathNotice(entvars_t* pevChild) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	int BloodColor() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int GetRCCaps() override;
	virtual bool IsTeleFraggabe();

	CBaseEntity* GetSpawnOrg();
	void EXPORT PAnimThink();
	void EXPORT DoRespawn();

	static TYPEDESCRIPTION m_SaveData[];

	CBaseEntity* m_pGiven; // orig +0x27c, the drive_icky it gave out
	bool m_bTakeable;	   // orig +0x280
};

LINK_ENTITY_TO_CLASS(pickup_drive_icky, CDriveIckyPickup); // orig 0x16e090

TYPEDESCRIPTION CDriveIckyPickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveIckyPickup, m_pGiven, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveIckyPickup, m_bTakeable, FIELD_BOOLEAN),
};

// orig 0x16e0c0
bool CDriveIckyPickup::Save(CSave& save)
{
	if (!CBaseMonster::Save(save))
		return false;
	return save.WriteFields("CDriveIckyPickup", this, m_SaveData, ARRAYSIZE(m_SaveData));
}

// orig 0x16e110
bool CDriveIckyPickup::Restore(CRestore& restore)
{
	if (!CBaseMonster::Restore(restore))
		return false;
	return restore.ReadFields("CDriveIckyPickup", this, m_SaveData, ARRAYSIZE(m_SaveData));
}

// orig 0x16e160
void CDriveIckyPickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), "models/icky.mdl");
	UTIL_SetSize(pev, Vector(-24, -24, -24), Vector(24, 24, 24));
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->sequence = LookupSequence("idle");
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);
	InitBoneControllers();
	SetUse(&CDriveIckyPickup::Use);
	SetThink(&CDriveIckyPickup::PAnimThink);
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

// orig 0x16e390
void CDriveIckyPickup::Precache()
{
	PRECACHE_MODEL("models/icky.mdl");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_icky");
}

// orig 0x16e3e0: the first info_drive_icky_spawn still there
CBaseEntity* CDriveIckyPickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_icky_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (FNullEnt(pSpot->pev))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x16e460
bool CDriveIckyPickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return false;
}

// orig 0x16e470: fade in; while given out, end the drive at its time limit
void CDriveIckyPickup::PAnimThink()
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

// orig 0x16e610: a player that isn't driving (or just stopped) gets in a new ichthyosaur
void CDriveIckyPickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!m_bTakeable)
		return;
	if (!pCaller->IsPlayer() || !pCaller)
		return;

	CBasePlayer* pPlayer = (CBasePlayer*)pCaller;
	if (!(pPlayer->m_fDrcEntTransTime <= gpGlobals->time) || pPlayer->m_bInDriveEnt)
		return;
	if (pPlayer->Classify() != CLASS_PLAYER)
		return;
	if (strcmp(STRING(pPlayer->pev->classname), "player"))
		return;

	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_icky", pev->origin, pev->angles, ENT(pev));
	pPlayer->pev->angles = Vector(0, pev->angles.y, 0);
	pPlayer->pev->v_angle = pPlayer->pev->angles;
	pPlayer->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGiven = pPlayer->m_pCurDriveEnt;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDriveIckyPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyMaxDriveTime", 420.0) + gpGlobals->time;
}

// orig 0x16e810
void CDriveIckyPickup::DoRespawn()
{
	m_bTakeable = true;
	m_pGiven = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);
	SetThink(&CDriveIckyPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x16e8d0: the ichthyosaur it gave out died
void CDriveIckyPickup::DeathNotice(entvars_t* pevChild)
{
	if (m_bTakeable)
		return;
	pev->nextthink = PREFS_KeyLoopUpFloat("DRIVEICKY", "IckyRespawnDelayAfterDeath", 5.0) + gpGlobals->time;
	SetThink(&CDriveIckyPickup::DoRespawn);
}

// orig 0x16e940: one drive_icky_spawner in multiplayer
void CreateDrvIckySpawner()
{
	if (!g_pGameRules->IsMultiplayer())
		return;
	if (!UTIL_FindEntityByClassname(NULL, "drive_icky_spawner"))
		CBaseEntity::Create("drive_icky_spawner", g_vecZero, g_vecZero);
}

// Makes mp_driveicky pickups 15 seconds into the map, then removes itself.
class CDrvIckySpawner : public CBaseEntity
{
public:
	void Spawn() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	int ObjectCaps() override;

	void EXPORT DrvAThink();

	static TYPEDESCRIPTION m_SaveData[];

	bool m_bDummy; // orig +0x34, never used
};

LINK_ENTITY_TO_CLASS(drive_icky_spawner, CDrvIckySpawner); // orig 0x16e9a0

TYPEDESCRIPTION CDrvIckySpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvIckySpawner, m_bDummy, FIELD_BOOLEAN),
};

// orig 0x16e9d0
bool CDrvIckySpawner::Save(CSave& save)
{
	if (!CBaseEntity::Save(save))
		return false;
	return save.WriteFields("CDrvIckySpawner", this, m_SaveData, ARRAYSIZE(m_SaveData));
}

// orig 0x16ea20
bool CDrvIckySpawner::Restore(CRestore& restore)
{
	if (!CBaseEntity::Restore(restore))
		return false;
	return restore.ReadFields("CDrvIckySpawner", this, m_SaveData, ARRAYSIZE(m_SaveData));
}

// orig 0x16ea70
void CDrvIckySpawner::DrvAThink()
{
	int iCount = (int)CVAR_GET_FLOAT("mp_driveicky");
	for (int i = 0; i < iCount; i++)
		CBaseEntity::Create("pickup_drive_icky", g_vecZero, g_vecZero);
	UTIL_Remove(this);
}

// orig 0x16eb00
void CDrvIckySpawner::Spawn()
{
	pev->classname = MAKE_STRING("drive_icky_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvIckySpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15.0;
}

int CDrvIckySpawner::ObjectCaps() { return FCAP_DONT_SAVE; } // orig 0x16eba0

// orig 0x16ec00
int CDriveIckyPickup::ObjectCaps()
{
	if (m_bTakeable)
		return FCAP_IMPULSE_USE | FCAP_ACROSS_TRANSITION;
	return FCAP_ACROSS_TRANSITION;
}

int CDriveIckyPickup::GetRCCaps() { return 0; }			  // orig 0x16ec20
bool CDriveIckyPickup::IsTeleFraggabe() { return false; } // orig 0x16ec30
int CDriveIckyPickup::BloodColor() { return DONT_BLEED; } // orig 0x16ec40
int CDriveIckyPickup::Classify() { return CLASS_HUMAN_MILITARY; } // orig 0x16ec50

// orig 0x16ecb0
void CDriveIcky::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;
	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

int CDriveIcky::GetRCCaps() { return 0x1aa2dfd; }	 // orig 0x16ed30
int CDriveIcky::GetGuassDistOut() { return 64; }	 // orig 0x16ed40
int CDriveIcky::GetGuassHull() { return large_hull; } // orig 0x16ed50

// orig 0x16ed60
Vector CDriveIcky::GetCamRotOrg()
{
	return Center();
}

int CDriveIcky::BloodColor() { return BLOOD_COLOR_YELLOW; } // orig 0x16eda0
int CDriveIcky::Classify() { return CLASS_ALIEN_MONSTER; }	// orig 0x16edb0
