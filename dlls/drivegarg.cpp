/***
*
*	Rocket Crowbar 1.9: drivegarg.cpp. The drivable Gargantua: drive_garg, the
*	pickup_drive_garg a player uses to get in, the drive_garg_spawner that places the pickups,
*	and dg_streak_spiral, its death spiral. Much of it follows HL's gargantua.cpp (flames, eye,
*	death). Reconstructed from rc_i386.so (addresses are file addresses,
*	reference/funcfile.tsv); see reference/dossiers/bm-garg.md.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"
#include "customentity.h"
#include "effects.h"
#include "explode.h"
#include "func_break.h"
#include "gamerules.h"
#include "client.h"
#include "decals.h"
#include "UserMessages.h"
#include "driveent.h"
#include "drivegarg.h"

// Preff.cpp (prefs-votes package); temporary definitions are in drivebm.cpp
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
extern bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault);
// combat.cpp (RC added it): live entities of a classname
extern int CountEntsInWorld(const char* pszClassname); // combat.cpp
// gargantua.cpp
extern void StreakSplash(const Vector& origin, const Vector& direction, int color, int count, int speed, int velocityRange);

const char* g_sDGMeleeHitSounds[] = {"zombie/claw_strike1.wav", "zombie/claw_strike2.wav", "zombie/claw_strike3.wav"};
const char* g_sDGMeleeMissSounds[] = {"zombie/claw_miss1.wav", "zombie/claw_miss2.wav"};
const char* g_sDGBiteHitSounds[] = {"ichy/ichy_bite1.wav", "ichy/ichy_bite1.wav"};
const char* g_sDGWalkSounds[] = {"garg/gar_step1.wav", "garg/gar_step2.wav"};
const char* g_sDGIdleSounds[] = {"garg/gar_idle1.wav", "garg/gar_idle2.wav", "garg/gar_idle3.wav", "garg/gar_idle4.wav", "garg/gar_idle5.wav"};
const char* g_sDGPainSounds[] = {"garg/gar_pain1.wav", "garg/gar_pain2.wav", "garg/gar_pain3.wav"};
const char* g_sDGBreathSounds[] = {"garg/gar_breathe1.wav", "garg/gar_breathe2.wav", "garg/gar_breathe3.wav"};

// An idle animation SetRandomAnim can pick: its chance weight and how long it holds.
struct sDGAnimT
{
	const char* pszName;
	float flWeight;
	float flMinTime;
	float flMaxTime;
};

sDGAnimT g_sDGIdleAnims[] = {
	{"idle2", 0.3, 16, 24},
	{"idle1", 0.25, 14, 20},
	{"idle4", 0.4, 11, 18},
};

// The damage types that hurt a gargantua (HL's GARG_DAMAGE)
#define DGARG_DAMAGE (DMG_ENERGYBEAM | DMG_CRUSH | DMG_MORTAR | DMG_BLAST)

//=========================================================
// CGDSpiral: the streak spiral of the death effect (HL's CSpiral)
//=========================================================
class CGDSpiral : public CBaseEntity
{
public:
	void Spawn() override;
	void Think() override;
	int ObjectCaps() override;
	static CGDSpiral* Create(const Vector& origin, float height, float radius, float duration);
};

LINK_ENTITY_TO_CLASS(dg_streak_spiral, CGDSpiral);

//=========================================================
// CDriveGarg: the Gargantua a player drives
//=========================================================
class CDriveGarg : public CBaseDriveEnt
{
public:
	void Spawn() override;
	void Precache() override;
	void SetObjectCollisionBox() override;
	int Classify() override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int BloodColor() override;
	float ExtreemeDamageFract() override;
	float VeryHeavyDamageFract() override;
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

	void EXPORT DGThink();
	void EXPORT DyingThink();
	void DoFlameWeapon();
	void DoSlashWeapon();
	void DoBiteWeapon();
	void DoKickWeapon();
	void CheckEHop();
	void DoMovement();
	void DoYaw();
	void UpdateOwnerHUD();
	void EyeUpdate();
	void DoBiteDamage();
	void DoKickDamage();
	void DoFlameDamage(Vector vecStart, Vector vecEnd, float flDamage);
	void UpdateFlames();
	void CreateFlames();
	void DestoryFlames();
	float SetRandomAnim(sDGAnimT* pAnims, int iCount);
	CBaseEntity* GargCheckTraceHullAttack(float flDist, int iDamage, int iDmgType);
	void DeathEffect();

	// Unsaved (the original has no save table for this class). Offsets are the original's.
	float m_flInterval;		  // orig +0x280, this think's StudioFrameAdvance interval
	int m_iLastHealth;		  // orig +0x284, health last sent to the driver's HUD
	Vector m_vecLastOrigin;	  // orig +0x288, for the driver's camera velocity
	float m_flLastOriginTime; // orig +0x294
	bool m_bCanTurn;		  // orig +0x298, false while flaming (aim with the head instead). VERIFY name
	bool m_bCanMove;		  // orig +0x299, VERIFY name
	bool m_bMoving;			  // orig +0x29a, VERIFY name
	bool m_bCanIdle;		  // orig +0x29b, VERIFY name
	bool m_bCanFlame;		  // orig +0x29c, VERIFY name
	bool m_bCanSlash;		  // orig +0x29d, VERIFY name
	bool m_bCanBite;		  // orig +0x29e, VERIFY name
	bool m_bCanKick;		  // orig +0x29f, VERIFY name
	bool m_bCanYaw;			  // orig +0x2a0, VERIFY name
	float m_flNextIdle;		  // orig +0x2a4
	float m_flNextFlameTime;  // orig +0x2a8
	float m_flNextSlash;	  // orig +0x2ac
	float m_flNextBite;		  // orig +0x2b0
	float m_flNextKick;		  // orig +0x2b4
	float m_flBiteDamageTime; // orig +0x2b8, -1 when no bite is pending
	float m_flKickDamageTime; // orig +0x2bc, -1 when no kick is pending
	bool m_bSlashReady;		  // orig +0x2c0, VERIFY name
	bool m_bBiteReady;		  // orig +0x2c1, VERIFY name
	bool m_bKickReady;		  // orig +0x2c2, VERIFY name
	bool m_bInMelee;		  // orig +0x2c3, a slash, bite or kick is playing. VERIFY name
	Vector m_vecMoveVel;	  // orig +0x2c4, velocity of the last walk step
	float m_flMoveTime;		  // orig +0x2d0
	float m_flNextEHop;		  // orig +0x2d4
	float m_flameX;			  // orig +0x2d8, head pitch bone (HL's m_flameX)
	float m_flameY;			  // orig +0x2dc, head yaw bone (HL's m_flameY)
	bool m_bFlameOn;		  // orig +0x2e0, the flame beams exist. VERIFY name
	bool m_bFlaming;		  // orig +0x2e1, VERIFY name
	float m_flFlameMinTime;	  // orig +0x2e4
	float m_streakTime;		  // orig +0x2e8 (HL's name)
	CBeam* m_pFlame[2];		  // orig +0x2ec, the outer flame beams
	CBeam* m_pFlameInner[2];  // orig +0x2f4, the inner (blue) ones
	float m_flDieTime;		  // orig +0x2fc
	float m_flPainTime;		  // orig +0x300
	int m_iGibModel;		  // orig +0x304 (HL's gGargGibModel)
	entvars_t* m_pevKiller;	  // orig +0x308, the driver is killed by this when the garg is
	CSprite* m_pEyeGlow;	  // orig +0x30c (HL's name)
	float m_eyeBrightness;	  // orig +0x310 (HL's name)
};

// orig 0x15a080 (HL's SpawnExplosion)
void DGargSpawnExplosion(Vector center, float randomRange, float flTime, int magnitude)
{
	KeyValueData kvd;
	char buf[128];

	center.x += RANDOM_FLOAT(-randomRange, randomRange);
	center.y += RANDOM_FLOAT(-randomRange, randomRange);

	CBaseEntity* pExplosion = CBaseEntity::Create("env_explosion", center, g_vecZero, NULL);
	sprintf(buf, "%3d", magnitude);
	kvd.szKeyName = "iMagnitude";
	kvd.szValue = buf;
	pExplosion->KeyValue(&kvd);
	pExplosion->pev->spawnflags |= SF_ENVEXPLOSION_NODAMAGE;

	pExplosion->Spawn();
	pExplosion->SetThink(&CBaseEntity::SUB_CallUseToggle);
	pExplosion->pev->nextthink = gpGlobals->time + flTime;
}

LINK_ENTITY_TO_CLASS(drive_garg, CDriveGarg);

// orig 0x156500
void CDriveGarg::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_garg");
	SET_MODEL(ENT(pev), "models/garg.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-28, -28, 0), Vector(28, 28, 64));
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	pev->health = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargHealth", 1400);
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	pev->sequence = LookupSequence("idle3");
	ResetSequenceInfo();
	InitBoneControllers();
	pev->frame = 0;
	m_flPainTime = gpGlobals->time;
	pev->framerate = 1;
	SetEyePosition();

	m_pEyeGlow = CSprite::SpriteCreate("sprites/gargeye1.spr", pev->origin, false);
	m_pEyeGlow->SetTransparency(kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation);
	m_pEyeGlow->SetAttachment(ENT(pev), 1);
	m_pEyeGlow->pev->iuser3 = 2;
	m_eyeBrightness = 100;

	m_flNextIdle = RANDOM_FLOAT(3, 8) + gpGlobals->time;
	m_bCanIdle = true;
	m_bMoving = false;
	m_bCanTurn = true;
	m_bCanMove = true;
	m_bCanFlame = true;
	m_bCanSlash = true;
	m_bCanBite = true;
	m_bCanKick = true;
	m_bCanYaw = true;
	m_flBiteDamageTime = -1;
	m_flKickDamageTime = -1;
	m_bSlashReady = true;
	m_bBiteReady = true;
	m_bKickReady = true;
	m_bInMelee = false;
	m_flMoveTime = gpGlobals->time;
	m_vecMoveVel = g_vecZero;
	m_flNextSlash = gpGlobals->time;
	m_flNextFlameTime = gpGlobals->time;
	m_flNextBite = gpGlobals->time;
	m_flNextKick = gpGlobals->time;
	m_flameY = 0;
	m_flameX = 0;
	m_bFlameOn = false;
	m_bFlaming = false;
	m_flFlameMinTime = gpGlobals->time;
	m_streakTime = gpGlobals->time;
	m_flNextEHop = gpGlobals->time;
	SetThink(&CDriveGarg::DGThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x1568c0
void CDriveGarg::Precache()
{
	PRECACHE_MODEL("models/garg.mdl");
	PRECACHE_MODEL("sprites/gargeye1.spr");
	m_iGibModel = PRECACHE_MODEL("models/metalplategibs.mdl");
	PRECACHE_MODEL("sprites/xbeam3.spr");
	PRECACHE_SOUND("garg/gar_flameoff1.wav");
	PRECACHE_SOUND("garg/gar_flameon1.wav");
	PRECACHE_SOUND("garg/gar_flamerun1.wav");
	PRECACHE_SOUND("debris/metal4.wav");
	for (unsigned int i = 0; i < 3; i++)
		PRECACHE_SOUND(g_sDGMeleeHitSounds[i]);
	for (unsigned int i = 0; i < 2; i++)
		PRECACHE_SOUND(g_sDGMeleeMissSounds[i]);
	for (unsigned int i = 0; i < 2; i++)
		PRECACHE_SOUND(g_sDGBiteHitSounds[i]);
	for (unsigned int i = 0; i < 2; i++)
		PRECACHE_SOUND(g_sDGWalkSounds[i]);
	for (unsigned int i = 0; i < 5; i++)
		PRECACHE_SOUND(g_sDGIdleSounds[i]);
	for (unsigned int i = 0; i < 3; i++)
		PRECACHE_SOUND(g_sDGPainSounds[i]);
	for (unsigned int i = 0; i < 3; i++)
		PRECACHE_SOUND(g_sDGBreathSounds[i]);
}

// orig 0x156a60
const char* CDriveGarg::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;
	return STRING(m_pOwner->pev->netname);
}

// orig 0x156ad0
void CDriveGarg::DGThink()
{
	m_flInterval = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_pOwner)
	{
		const bool bNull = FNullEnt(m_pOwner->pev);
		edict_t* pent;
		if (!bNull && m_pOwner->pev && (pent = m_pOwner->pev->pContainingEntity) != NULL && pent->pvPrivateData && !pent->free && UTIL_PlayerByIndex(ENTINDEX(pent)))
		{
			if (pev->waterlevel >= 2)
			{
				// drowned: the original dies without letting go of the driver here
				pev->classname = MAKE_STRING("garg");
				Killed(pev, GIB_NORMAL);
				return;
			}

			if (pev->waterlevel != 0)
				pev->velocity.z = pev->velocity.z - m_flInterval * 80;

			DispatchAnimEvents(m_flInterval);

			m_pOwner->pev->rendermode = kRenderTransColor;
			m_pOwner->pev->renderamt = 0;
			m_pOwner->pev->effects |= EF_NODRAW;

			m_bCanIdle = true;
			m_bCanMove = true;
			m_bMoving = false;
			m_bCanTurn = true;
			m_bCanFlame = true;
			m_bCanSlash = true;
			m_bCanBite = true;
			m_bCanKick = true;
			m_bCanYaw = true;

			if (m_bInMelee && m_fSequenceFinished)
			{
				m_bBiteReady = true;
				m_bSlashReady = true;
				m_bKickReady = true;
				m_bInMelee = false;
			}

			if (m_flBiteDamageTime != -1 && m_flBiteDamageTime < gpGlobals->time)
			{
				DoBiteDamage();
				m_flBiteDamageTime = -1;
			}
			if (m_flKickDamageTime != -1 && m_flKickDamageTime < gpGlobals->time)
			{
				DoKickDamage();
				m_flKickDamageTime = -1;
			}

			UpdateOwnerHUD();

			if (m_bInMelee && !m_fSequenceFinished)
			{
				// a melee animation runs to its end before anything else
				EyeUpdate();
				m_bCanIdle = false;
				m_bCanMove = false;
				m_bMoving = false;
				m_bCanTurn = true;
				m_bCanFlame = false;
				m_bCanSlash = false;
				m_bCanBite = false;
				m_bCanKick = false;
				m_bCanYaw = false;
				pev->fuser4 = 0;
				return;
			}

			DoFlameWeapon();
			DoSlashWeapon();
			DoBiteWeapon();
			DoKickWeapon();
			DoMovement();
			DoYaw();
			pev->fuser4 = m_vecMoveVel.Length();
			EyeUpdate();

			if (!m_bCanIdle)
			{
				m_flNextIdle = RANDOM_FLOAT(3, 8) + gpGlobals->time;
				return;
			}

			if (m_flNextIdle < gpGlobals->time)
			{
				m_flNextIdle = SetRandomAnim(g_sDGIdleAnims, 3) + gpGlobals->time;
				m_eyeBrightness = 140;
				return;
			}

			if (pev->sequence == LookupSequence("idle3"))
				return;
			if (!m_fSequenceFinished)
				return;
			pev->sequence = LookupSequence("idle3");
			ResetSequenceInfo();
			pev->frame = 0;
			m_eyeBrightness = 100;
			return;
		}
	}

	m_pevKiller = pev;
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("garg");
	Killed(pev, GIB_NORMAL);
}

// orig 0x156ef0: +attack holds the flamethrower on
void CDriveGarg::DoFlameWeapon()
{
	if (!m_bCanFlame)
	{
		if (m_bFlameOn)
		{
			DestoryFlames();
			m_bFlameOn = false;
		}
		return;
	}

	if (gpGlobals->time < m_flNextFlameTime)
		return;

	if ((m_pOwner->pev->button & IN_ATTACK) == 0)
	{
		// let go: keep flaming until the minimum time is up
		if (m_flFlameMinTime <= gpGlobals->time)
			m_bFlaming = false;
		else
			m_bFlaming = true;
	}
	else
	{
		if (!m_bFlaming)
		{
			m_flFlameMinTime = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargMinFlameTime", 3.0) + gpGlobals->time;
			pev->sequence = LookupSequence("shootflames1");
			ResetSequenceInfo();
			pev->frame = 0;
		}
		m_bFlaming = true;
	}

	if (!m_bFlaming)
	{
		if (m_bFlameOn)
		{
			DestoryFlames();
			m_bFlameOn = false;
			m_flNextFlameTime = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargFlameOnOffTime", 0.4) + gpGlobals->time;
			m_fSequenceFinished = true;
		}
		return;
	}

	m_eyeBrightness = 240;
	m_bCanIdle = false;
	m_bCanTurn = false;
	m_bCanMove = false;
	m_bCanSlash = false;
	m_bCanBite = false;
	m_bCanKick = false;

	if (pev->sequence != LookupSequence("shootflames2") && m_fSequenceFinished)
	{
		pev->sequence = LookupSequence("shootflames2");
		ResetSequenceInfo();
		m_bFlameOn = true;
		CreateFlames();
	}

	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 3;
	if (m_bFlameOn)
	{
		UpdateFlames();
		m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 2;
		if (m_flFlameMinTime < gpGlobals->time)
			m_flFlameMinTime = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargFlameOffDelay", 0.4) + gpGlobals->time;
	}
}

// orig 0x157170: +attack2 swings
void CDriveGarg::DoSlashWeapon()
{
	if (!m_bCanSlash || m_flNextSlash > gpGlobals->time)
		return;

	if (!m_bSlashReady)
	{
		m_bCanIdle = false;
		m_bCanMove = false;
		return;
	}

	if ((m_pOwner->pev->button & IN_ATTACK2) == 0)
		return;
	if (pev->sequence == LookupSequence("Attack"))
		return;

	pev->sequence = LookupSequence("Attack");
	ResetSequenceInfo();
	pev->frame = 0;
	m_flNextSlash = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargSlashRefire", 0.4) + gpGlobals->time;
	m_bCanIdle = false;
	m_bCanMove = false;
	m_bCanTurn = false;
	m_bCanFlame = false;
	m_bCanBite = false;
	m_bCanKick = false;
	m_bInMelee = true;
	m_bBiteReady = false;
	m_bSlashReady = false;
	m_bKickReady = false;
	m_eyeBrightness = 220;
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 4;
}

// orig 0x1572e0: +jump bites; the damage lands DoBiteDamage's 0.52 seconds later
void CDriveGarg::DoBiteWeapon()
{
	if (!m_bCanBite || m_flNextBite > gpGlobals->time)
		return;

	if (!m_bBiteReady)
	{
		m_bCanIdle = false;
		m_bCanMove = false;
		return;
	}

	if ((m_pOwner->pev->button & IN_JUMP) == 0)
		return;
	if (pev->sequence == LookupSequence("bitehead"))
		return;

	pev->sequence = LookupSequence("bitehead");
	ResetSequenceInfo();
	pev->frame = 0;
	m_flNextBite = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargBiteRefire", 6.0) + gpGlobals->time;
	m_bCanIdle = false;
	m_bCanMove = false;
	m_bCanTurn = false;
	m_bCanFlame = false;
	m_bCanSlash = false;
	m_bCanKick = false;
	m_bInMelee = true;
	m_bKickReady = false;
	m_bBiteReady = false;
	m_bSlashReady = false;
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 6;
	m_flBiteDamageTime = gpGlobals->time + 0.52;
	m_eyeBrightness = 230;
}

// orig 0x157460: +duck kicks
void CDriveGarg::DoKickWeapon()
{
	if (!m_bCanKick || m_flNextKick > gpGlobals->time)
		return;

	if (!m_bKickReady)
	{
		m_bCanIdle = false;
		m_bCanMove = false;
		return;
	}

	if ((m_pOwner->pev->button & IN_DUCK) == 0)
		return;
	if (pev->sequence == LookupSequence("kickcar"))
		return;

	pev->sequence = LookupSequence("kickcar");
	ResetSequenceInfo();
	pev->frame = 0;
	m_flNextKick = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargKickRefire", 3.0) + gpGlobals->time;
	m_bCanIdle = false;
	m_bCanMove = false;
	m_bCanTurn = false;
	m_bCanFlame = false;
	m_bCanSlash = false;
	m_bCanBite = false;
	m_bInMelee = true;
	m_bBiteReady = false;
	m_bKickReady = false;
	m_bSlashReady = false;
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 4;
	m_flKickDamageTime = gpGlobals->time + 0.1;
	m_eyeBrightness = 220;
}

// orig 0x1575e0: hop off a ledge in front when there is no ground under the next step
void CDriveGarg::CheckEHop()
{
	if (m_flNextEHop > gpGlobals->time || (pev->flags & FL_ONGROUND) == 0)
		return;
	if (pev->velocity.z > 16 || !m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecFront = pev->origin + Vector(0, 0, 4) + gpGlobals->v_forward * 48;
	Vector vecDown = vecFront - Vector(0, 0, 28);

	TraceResult tr;
	UTIL_TraceLine(pev->origin + Vector(0, 0, 4), vecFront, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
	{
		UTIL_TraceLine(vecFront, vecDown, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
		{
			pev->velocity.z = pev->velocity.z + 50;
			pev->velocity = pev->velocity + gpGlobals->v_forward * 100;
			pev->flags &= ~FL_ONGROUND;
			UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
		}
	}
	m_flNextEHop = gpGlobals->time + 0.2;
}

// orig 0x157900: walk forward while the driver holds +forward
void CDriveGarg::DoMovement()
{
	if ((pev->flags & FL_ONGROUND) == 0)
	{
		m_vecMoveVel = g_vecZero;
		m_flMoveTime = gpGlobals->time;
		if (!m_bCanIdle)
			return;

		// in the air: VERIFY run below 120 and walk above, as the original has it
		float flSpeed = pev->velocity.Make2D().Length();
		if (flSpeed > 30 && flSpeed < 120)
		{
			if (pev->sequence == LookupSequence("run"))
				return;
			pev->sequence = LookupSequence("run");
			ResetSequenceInfo();
			m_eyeBrightness = 180;
			return;
		}
		if (!(flSpeed >= 120))
			return;
		if (pev->sequence == LookupSequence("walk"))
			return;
		pev->sequence = LookupSequence("walk");
		ResetSequenceInfo();
		m_eyeBrightness = 170;
		return;
	}

	if (m_bCanMove)
	{
		if ((m_pOwner->pev->button & IN_FORWARD) == 0 || (pev->flags & FL_ONGROUND) == 0)
		{
			if (pev->sequence == LookupSequence("run") || pev->sequence == LookupSequence("walk"))
				m_fSequenceFinished = true;
		}
		else
		{
			m_vecMoveVel = g_vecZero;
			const bool bWalk = (m_pOwner->pev->button & (IN_RUN | IN_USE)) != 0;
			const float flOldFrame = pev->frame;
			const int iOldSequence = pev->sequence;
			bool bWasOther = false;
			if (pev->sequence != LookupSequence("run"))
				bWasOther = pev->sequence != LookupSequence("walk");

			if (bWalk)
			{
				if (pev->sequence != LookupSequence("walk"))
				{
					pev->sequence = LookupSequence("walk");
					ResetSequenceInfo();
					m_eyeBrightness = 170;
				}
			}
			else
			{
				if (pev->sequence != LookupSequence("run"))
				{
					pev->sequence = LookupSequence("run");
					ResetSequenceInfo();
					m_eyeBrightness = 180;
				}
			}

			m_bCanIdle = false;
			m_bMoving = true;
			UTIL_MakeVectors(pev->angles);
			Vector vecGoal = Center() + gpGlobals->v_forward * 512;
			float flMoved = 0;
			Vector vecStart = pev->origin;

			// how far the walk can go this think, checked in 20 unit steps
			float flDist = m_flGroundSpeed * pev->framerate * m_flInterval;
			while (flDist > 0.001)
			{
				float flStep = flDist;
				if (flStep > 20)
					flStep = 20;
				Vector vecOld = pev->origin;
				for (; !WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY) && flStep > 2; flStep -= 2)
					;
				UTIL_SetOrigin(pev, vecOld);
				if (!WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY))
				{
					if (flMoved < 0.01)
					{
						if (bWasOther)
						{
							pev->sequence = iOldSequence;
							ResetSequenceInfo();
							m_eyeBrightness = 100;
							pev->frame = flOldFrame;
						}
						else
							m_fSequenceFinished = true;
						m_bCanIdle = true;
						m_bMoving = false;
						UTIL_SetOrigin(pev, vecStart);
						m_vecMoveVel = g_vecZero;
						m_flMoveTime = gpGlobals->time;
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

			// room for the body ahead?
			Vector vecSrc = gpGlobals->v_forward * 32 + pev->origin + Vector(0, 0, 160);
			Vector vecEnd = vecSrc + flMoved * gpGlobals->v_forward;
			TraceResult tr;
			UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, human_hull, ENT(pev), &tr);
			if (tr.fAllSolid)
				tr.flFraction = 0;
			flMoved = flMoved * tr.flFraction;
			if (flMoved < 0.1 || tr.flFraction < 0.01 || m_flGroundSpeed * pev->framerate * tr.flFraction < 64)
			{
				if (bWasOther)
				{
					pev->sequence = iOldSequence;
					ResetSequenceInfo();
					m_eyeBrightness = 100;
					pev->frame = flOldFrame;
				}
				else
					m_fSequenceFinished = true;
				m_bCanIdle = true;
				m_bMoving = false;
				return;
			}

			Vector vecPrev = pev->origin;
			const float flOldIdealYaw = pev->ideal_yaw;
			pev->ideal_yaw = pev->angles.y;
			while (flMoved > 0.001)
			{
				float flStep = flMoved;
				if (flStep > 16)
					flStep = 16;
				UTIL_MoveToOrigin(ENT(pev), vecGoal, flStep, MOVE_NORMAL);
				flMoved -= flStep;
			}
			pev->ideal_yaw = flOldIdealYaw;

			Vector vecMoved = pev->origin - vecPrev;
			if (vecMoved.Length() > 0.01 && m_flInterval > 0.001)
			{
				m_vecMoveVel = vecMoved / m_flInterval;
				m_flMoveTime = gpGlobals->time;
				return;
			}
		}
	}

	m_vecMoveVel = g_vecZero;
	m_flMoveTime = gpGlobals->time;
}

// orig 0x158220: turn the body toward the driver's view; while flaming, aim the head instead
void CDriveGarg::DoYaw()
{
	if (!m_bCanYaw)
		return;

	if (!m_bCanTurn)
	{
		ChangeYaw((int)(m_flInterval * 200));
		const float flPitch = pev->idealpitch;
		float flYaw = UTIL_AngleMod(pev->ideal_yaw - pev->angles.y);
		if (flYaw < -180)
			flYaw += 360;
		else if (flYaw > 180)
			flYaw -= 360;
		if (flYaw < -45)
			flYaw = -45;
		else if (flYaw > 45)
			flYaw = 45;

		m_flameX = UTIL_ApproachAngle(flPitch, m_flameX, m_flInterval * 60);
		m_flameY = UTIL_ApproachAngle(flYaw, m_flameY, m_flInterval * 100);
		m_flameY = SetBoneController(0, m_flameY);
		m_flameX = SetBoneController(1, m_flameX);
		return;
	}

	m_flameX = UTIL_ApproachAngle(0, m_flameX, m_flInterval * 60);
	m_flameY = UTIL_ApproachAngle(0, m_flameY, m_flInterval * 100);
	m_flameY = SetBoneController(0, m_flameY);
	m_flameX = SetBoneController(1, m_flameX);

	float flSpeed = m_flInterval * 1600;
	bool bTurnAround = false;
	const float flBodyYaw = pev->angles.y;
	const float flDelta = fabs(ChangeYaw(0x2000)); // measures the turn left, then undoes it
	pev->angles.y = flBodyYaw;
	if (flDelta < 0.001f)
		return;
	if (flDelta > 70 && !m_bMoving)
	{
		flSpeed = m_flInterval * 2800;
		bTurnAround = true;
	}
	const float flTurned = ChangeYaw((int)flSpeed);
	if (bTurnAround)
	{
		// VERIFY: the original adds gpGlobals->time twice
		m_flNextIdle = RANDOM_FLOAT(3, 8) + (gpGlobals->time + gpGlobals->time);
		m_eyeBrightness = 160;
		pev->sequence = LookupSequence(flTurned > 0 ? "180left" : "180right");
		ResetSequenceInfo();
		pev->frame = 0;
	}
}

// orig 0x158580: the driver's HUD health is the monster's, in percent
void CDriveGarg::UpdateOwnerHUD()
{
	if (!m_pOwner)
		return;
	if (pev->health == (float)m_iLastHealth)
		return;

	float flPercent = pev->health / pev->max_health * 100;
	int iPercent = flPercent > 0 ? (int)flPercent : 0;
	if (iPercent < 1)
		iPercent = 1;
	else if (iPercent > 100)
		iPercent = 100;

	MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_SHORT(iPercent);
	MESSAGE_END();
	m_iLastHealth = (int)pev->health;
}

// orig 0x1586a0 (HL's EyeUpdate): remakes the eye glow if it went away
void CDriveGarg::EyeUpdate()
{
	edict_t* pent;
	if (!m_pEyeGlow || !m_pEyeGlow->pev || FNullEnt(m_pEyeGlow->pev) || (pent = m_pEyeGlow->pev->pContainingEntity) == NULL || !pent->pvPrivateData || pent->free)
	{
		m_pEyeGlow = CSprite::SpriteCreate("sprites/gargeye1.spr", pev->origin, false);
		if (!m_pEyeGlow)
			return;
		m_pEyeGlow->SetTransparency(kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation);
	}
	m_pEyeGlow->SetAttachment(ENT(pev), 1);

	m_pEyeGlow->pev->renderamt = UTIL_Approach(m_eyeBrightness, m_pEyeGlow->pev->renderamt, m_flInterval * 200);
	if (m_pEyeGlow->pev->renderamt == 0)
		m_pEyeGlow->pev->effects |= EF_NODRAW;
	else
		m_pEyeGlow->pev->effects &= ~EF_NODRAW;
	UTIL_SetOrigin(m_pEyeGlow->pev, pev->origin);
}

// orig 0x1588a0: bite the head off what is in front; the garg heals from it
void CDriveGarg::DoBiteDamage()
{
	Vector vecMouth, vecMouthAngles;
	GetAttachment(0, vecMouth, vecMouthAngles); // VERIFY: unused, as in the original
	UTIL_MakeVectors(pev->angles);

	Vector vecStart = pev->origin + Vector(0, 0, 32) + gpGlobals->v_forward * 32;
	Vector vecEnd = vecStart + gpGlobals->v_forward * 90;
	TraceResult tr;
	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);
	if (tr.flFraction >= 1.0)
		return;
	if (!tr.pHit)
		return;

	CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
	if (!pHit || !pHit->IsAlive() || pHit->pev->takedamage != DAMAGE_AIM)
		return;
	int iCap = 0x200;
	if (!pHit->HasRCCap(iCap))
		return;

	Vector vecCenter = pHit->Center();
	Vector vecTop = Vector(vecCenter.x, vecCenter.y, pHit->pev->absmax.z + 32);
	UTIL_TraceLine(vecTop, vecTop - Vector(0, 0, 64), dont_ignore_monsters, ENT(pev), &tr);

	const float flDamage = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargBiteDamage", 180);
	pev->classname = MAKE_STRING("garg_bite");
	ClearMultiDamage();
	pHit->TraceAttack(pev, flDamage, Vector(0, 0, -1), &tr, DMG_NEVERGIB | DMG_CRUSH);
	ApplyMultiDamage(pev, m_pOwner ? m_pOwner->pev : pev);
	pev->classname = MAKE_STRING("drive_garg");

	pev->health = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargBiteGainHealth", 140) + pev->health;
	if (pev->health > pev->max_health)
		pev->health = pev->max_health;

	const int iPitch = RANDOM_LONG(0, 30) + 50;
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_sDGBiteHitSounds[RANDOM_LONG(0, 1)], 1.0, 0.8, 0, iPitch);
}

// orig 0x158cf0: kick what is in front up and away
void CDriveGarg::DoKickDamage()
{
	const float flDamage = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargKickDamage", 80);
	UTIL_MakeVectors(pev->angles);
	Vector vecStart = pev->origin;
	vecStart.z += 16;
	Vector vecEnd = vecStart + gpGlobals->v_forward * 160 + gpGlobals->v_up * 64;
	TraceResult tr;
	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 3;
	pev->classname = MAKE_STRING("garg_kick");

	CBaseEntity* pHit = NULL;
	if (tr.pHit)
		pHit = CBaseEntity::Instance(tr.pHit);

	int iPitch;
	const char* pszSound;
	if (pHit && !pHit->IsBSPModel() && pHit->pev->takedamage != DAMAGE_NO)
	{
		int iCap = 0x400;
		if (pHit->HasRCCap(iCap))
		{
			pHit->pev->punchangle.x = -40;
			pHit->pev->punchangle.z = 20;
			pHit->pev->velocity = pHit->pev->velocity + gpGlobals->v_forward * 800 + Vector(0, 0, 400);
			pHit->pev->flags &= ~FL_ONGROUND;
		}
		pHit->TakeDamage(pev, m_pOwner ? m_pOwner->pev : pev, flDamage, DMG_CLUB);
		iPitch = RANDOM_LONG(0, 20) + 70;
		pszSound = g_sDGMeleeHitSounds[RANDOM_LONG(0, 2)];
	}
	else
	{
		iPitch = RANDOM_LONG(0, 20) + 70;
		pszSound = g_sDGMeleeMissSounds[RANDOM_LONG(0, 1)];
	}
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pszSound, 1.0, 0.8, 0, iPitch);
	pev->classname = MAKE_STRING("drive_garg");
}

// orig 0x159080 (HL's FlameDamage): burn what the flame passes near
void CDriveGarg::DoFlameDamage(Vector vecStart, Vector vecEnd, float flDamage)
{
	CBaseEntity* pEntity = NULL;
	TraceResult tr;
	Vector vecMid = (vecStart + vecEnd) * 0.5;
	const float searchRadius = (vecStart - vecMid).Length();
	Vector vecAim = (vecEnd - vecStart).Normalize();

	pev->classname = MAKE_STRING("garg_flamethrower");
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecMid, searchRadius)) != NULL)
	{
		if (pEntity->pev == pev)
			continue;
		if (strcmp(STRING(pEntity->pev->classname), "drive_garg") == 0)
			continue;
		if (strcmp(STRING(pEntity->pev->classname), "monster_gargantua") == 0 || pEntity->pev->takedamage == DAMAGE_NO)
			continue;

		Vector vecSpot = pEntity->BodyTarget(vecMid);
		float dist = DotProduct(vecAim, vecSpot - vecMid);
		if (dist > searchRadius || dist < -searchRadius)
			dist = searchRadius; // as HL has it
		Vector vecSrc = vecMid + dist * vecAim;

		UTIL_TraceLine(vecSrc, vecSpot, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction != 1.0 && tr.pHit != pEntity->pev->pContainingEntity)
			continue;

		// less damage farther from the flame
		float flAdjustedDamage;
		dist = (vecSrc - tr.vecEndPos).Length();
		if (dist > 64)
		{
			flAdjustedDamage = flDamage - (dist - 64) * 0.4;
			if (!(flAdjustedDamage > 0))
				continue;
		}
		else
			flAdjustedDamage = flDamage;

		// VERIFY: the attacker is the driver, with no null check
		if (tr.flFraction != 1.0)
		{
			ClearMultiDamage();
			pEntity->TraceAttack(pev, flAdjustedDamage, (tr.vecEndPos - vecSrc).Normalize(), &tr, DMG_BURN);
			ApplyMultiDamage(pev, m_pOwner->pev);
		}
		else
			pEntity->TakeDamage(pev, m_pOwner->pev, flAdjustedDamage, DMG_BURN);
	}
	pev->classname = MAKE_STRING("drive_garg");
}

// orig 0x1594f0 (HL's FlameUpdate)
void CDriveGarg::UpdateFlames()
{
	const float flLength = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargFlameLength", 460);
	const float flDmgPerSec = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargFlameDmgPerSec", 120);
	const float flInterval = m_flInterval;
	bool streaks = false;

	for (int i = 0; i < 2; i++)
	{
		Vector vecStart, angleGun;
		GetAttachment(i + 1, vecStart, angleGun);
		Vector vecAim = pev->angles + Vector(m_flameX, m_flameY, 0);
		UTIL_MakeVectors(vecAim);
		Vector vecEnd = vecStart + flLength * gpGlobals->v_forward;
		TraceResult trace;
		UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, ENT(pev), &trace);

		if (m_pFlame[i])
			m_pFlame[i]->SetStartPos(trace.vecEndPos);
		if (m_pFlameInner[i])
			m_pFlameInner[i]->SetStartPos((vecStart * 0.6) + (trace.vecEndPos * 0.4));

		if (trace.flFraction != 1.0 && m_streakTime < gpGlobals->time)
		{
			if (CBaseEntity::Instance(trace.pHit)->IsBSPModel())
			{
				StreakSplash(trace.vecEndPos, trace.vecPlaneNormal, 6, 20, 50, 400);
				streaks = true;
				UTIL_DecalTrace(&trace, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));
			}
		}

		DoFlameDamage(vecStart, trace.vecEndPos, flDmgPerSec * flInterval);

		MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(TE_ELIGHT);
		WRITE_SHORT(entindex() + 0x2000 + i * 0x1000); // entity, attachment
		WRITE_COORD(vecStart.x);					   // origin
		WRITE_COORD(vecStart.y);
		WRITE_COORD(vecStart.z);
		WRITE_COORD(RANDOM_FLOAT(32, 48)); // radius
		WRITE_BYTE(255);				   // R
		WRITE_BYTE(255);				   // G
		WRITE_BYTE(255);				   // B
		WRITE_BYTE(2);					   // life * 10
		WRITE_COORD(0);					   // decay
		MESSAGE_END();
	}
	if (streaks)
		m_streakTime = gpGlobals->time;
}

// orig 0x159990 (HL's FlameCreate)
void CDriveGarg::CreateFlames()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "garg/gar_flameon1.wav", 1.0, 0.8, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "garg/gar_flamerun1.wav", 1.0, 0.8, 0, 100);

	for (int i = 0; i < 2; i++)
	{
		m_pFlame[i] = CBeam::BeamCreate("sprites/xbeam3.spr", 240);
		if (m_pFlame[i])
		{
			m_pFlame[i]->PointEntInit(pev->origin, entindex());
			m_pFlame[i]->SetColor(255, 130, 90);
			m_pFlame[i]->SetBrightness(190);
			m_pFlame[i]->SetFlags(BEAM_FSHADEIN);
			m_pFlame[i]->SetScrollRate(20);
			m_pFlame[i]->SetEndAttachment(i + 2); // attachment is 1 based here
		}

		m_pFlameInner[i] = CBeam::BeamCreate("sprites/xbeam3.spr", 140);
		if (m_pFlameInner[i])
		{
			m_pFlameInner[i]->PointEntInit(pev->origin, entindex());
			m_pFlameInner[i]->SetColor(0, 120, 255);
			m_pFlameInner[i]->SetBrightness(190);
			m_pFlameInner[i]->SetFlags(BEAM_FSHADEIN);
			m_pFlameInner[i]->SetScrollRate(20);
			m_pFlameInner[i]->SetEndAttachment(i + 2);
		}
	}
}

// orig 0x159c00 (HL's FlameDestroy)
void CDriveGarg::DestoryFlames()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "garg/gar_flameoff1.wav", 1.0, 0.8, 0, 100);
	for (int i = 0; i < 2; i++)
	{
		// VERIFY: removed at once and the pointers kept (CreateFlames remakes both before reuse)
		if (m_pFlame[i])
			m_pFlame[i]->SUB_Remove();
		if (m_pFlameInner[i])
			m_pFlameInner[i]->SUB_Remove();
	}
}

// orig 0x159ca0: play one of the animations by weight; returns how long to hold it
float CDriveGarg::SetRandomAnim(sDGAnimT* pAnims, int iCount)
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

	pev->sequence = LookupSequence(pAnims[i].pszName);
	ResetSequenceInfo();
	pev->frame = 0;
	return RANDOM_FLOAT(pAnims[i].flMinTime, pAnims[i].flMaxTime);
}

// orig 0x159d80 (HL's TraceAttack): only big damage gets through
void CDriveGarg::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (IsAlive())
	{
		if ((bitsDamageType & DGARG_DAMAGE) != 0 && m_flPainTime < gpGlobals->time)
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_sDGPainSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, 100);
			m_flPainTime = RANDOM_FLOAT(2.5, 4) + gpGlobals->time;
		}

		bitsDamageType &= DGARG_DAMAGE;
		if (bitsDamageType == 0)
		{
			if (pev->dmgtime != gpGlobals->time || RANDOM_LONG(1, 5) == 1)
			{
				UTIL_Ricochet(ptr->vecEndPos, RANDOM_FLOAT(0.5, 1.5));
				pev->dmgtime = gpGlobals->time;
			}
			flDamage = 0;
		}
	}

	CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

// orig 0x159f30
bool CDriveGarg::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_pOwner)
	{
		if (CBaseEntity::Instance(pevAttacker) && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, CBaseEntity::Instance(pevAttacker)))
			return false;
		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	if (IsAlive())
	{
		if ((bitsDamageType & DGARG_DAMAGE) == 0)
			flDamage = flDamage * 0.01;
		if ((bitsDamageType & DMG_MORTAR) != 0)
			flDamage = flDamage * 0.8;
	}

	// VERIFY: no null check on the driver here in the original
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 2;
	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x15a180: start dying; DyingThink finishes the job (and the driver)
void CDriveGarg::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->deadflag != DEAD_NO)
		return;

	if (m_pEyeGlow)
	{
		m_pEyeGlow->pev->renderamt = 0;
		m_pEyeGlow->pev->effects |= EF_NODRAW;
		UTIL_Remove(m_pEyeGlow);
		m_pEyeGlow = NULL;
	}
	if (m_bFlameOn)
	{
		DestoryFlames();
		m_bFlameOn = false;
	}

	m_flDieTime = gpGlobals->time + 1.6;
	pev->sequence = LookupSequence("die");
	ResetSequenceInfo();
	pev->frame = 0;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_sDGPainSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, 100);

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_BYTE(4);
		MESSAGE_END();
		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_SHORT(0);
		MESSAGE_END();
	}

	m_pevKiller = pevAttacker;
	DeathEffect();
	SetThink(&CDriveGarg::DyingThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x15a3d0 (HL's DeathEffect timing and gibs, then the driver dies)
void CDriveGarg::DyingThink()
{
	m_flInterval = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;
	if (gpGlobals->time < m_flDieTime)
		return;

	pev->renderfx = kRenderFxExplode;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 0;
	pev->rendercolor.z = 0;
	pev->framerate = 0;

	// Note: this may cause problems later (HL's comment)
	pev->nextthink = gpGlobals->time + 0.15;
	SetThink(&CBaseEntity::SUB_Remove);

	if (CVAR_GET_FLOAT("sv_lowlag") != 1)
	{
		const int parts = MODEL_FRAMES(m_iGibModel);
		for (int i = 0; i < 10; i++)
		{
			if (CountEntsInWorld("gib") > 44)
				break;
			CGib* pGib = GetClassPtr((CGib*)NULL);
			pGib->Spawn("models/metalplategibs.mdl");

			int bodyPart = 0;
			if (parts > 1)
				bodyPart = RANDOM_LONG(0, pev->body - 1);
			pGib->pev->body = bodyPart;
			pGib->m_bloodColor = BLOOD_COLOR_YELLOW;
			pGib->m_material = matNone;
			pGib->pev->origin = pev->origin;
			pGib->pev->velocity = UTIL_RandomBloodVector() * RANDOM_FLOAT(300, 500);
			pGib->pev->nextthink = gpGlobals->time + 1.25;
			pGib->SetThink(&CBaseEntity::SUB_FadeOut);
		}

		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_BREAKMODEL);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_COORD(200); // size
		WRITE_COORD(200);
		WRITE_COORD(128);
		WRITE_COORD(0); // velocity
		WRITE_COORD(0);
		WRITE_COORD(0);
		WRITE_BYTE(200); // randomization
		WRITE_SHORT(m_iGibModel);
		WRITE_BYTE(50); // count
		WRITE_BYTE(20); // 2 seconds
		WRITE_BYTE(BREAK_FLESH);
		MESSAGE_END();
	}

	// tell the pickup that made this one (no null check on the instance, as in the original)
	if (pev->owner)
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	pev->classname = MAKE_STRING("garg");
	DEClientLeave();

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		UTIL_SetOrigin(m_pOwner->pev, m_pOwner->m_pCurDriveEnt->pev->origin + Vector(0, 0, 64));
		m_pOwner->pev->effects |= EF_NODRAW;
		m_pOwner->pev->health = -1;
		g_pevLastInflictor = pev;
		m_pOwner->Killed(m_pevKiller ? m_pevKiller : pev, GIB_NORMAL);
	}
}

// orig 0x15a920: the slash trace (HL's CheckTraceHullAttack, aimed a little down)
CBaseEntity* CDriveGarg::GargCheckTraceHullAttack(float flDist, int iDamage, int iDmgType)
{
	TraceResult tr;
	UTIL_MakeVectors(pev->angles);
	Vector vecStart = pev->origin;
	vecStart.z += 64;
	Vector vecEnd = vecStart + gpGlobals->v_forward * flDist - gpGlobals->v_up * flDist * 0.3;
	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);

	if (!tr.pHit)
		return NULL;

	CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
	if (pEntity->IsBSPModel() || pEntity->pev->takedamage == DAMAGE_NO)
		return NULL;

	if (iDamage > 0)
		pEntity->TakeDamage(pev, m_pOwner ? m_pOwner->pev : pev, iDamage, iDmgType);
	return pEntity;
}

// orig 0x15ab00
void CDriveGarg::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1: // the slash
	{
		pev->classname = MAKE_STRING("garg_smack");
		CBaseEntity* pHurt = GargCheckTraceHullAttack(150, (int)PREFS_KeyLoopUpFloat("DRIVEGARG", "GargDmgSlash", 60), DMG_CLUB);
		int iPitch;
		const char* pszSound;
		if (pHurt)
		{
			int iCap = 0x400;
			if (pHurt->HasRCCap(iCap))
			{
				pHurt->pev->punchangle.x = -40;
				pHurt->pev->punchangle.y = -40;
				pHurt->pev->punchangle.z = 40;
				pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 180;
			}
			iPitch = RANDOM_LONG(0, 15) + 50;
			pszSound = g_sDGMeleeHitSounds[RANDOM_LONG(0, 2)];
		}
		else
		{
			iPitch = RANDOM_LONG(0, 15) + 50;
			pszSound = g_sDGMeleeMissSounds[RANDOM_LONG(0, 1)];
		}
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pszSound, 1.0, 0.8, 0, iPitch);
		pev->classname = MAKE_STRING("drive_garg");
		break;
	}
	case 3: // left foot
	case 4: // right foot
	{
		UTIL_ScreenShake(pev->origin, 4.0, 3.0, 1.0, 750);
		const int iPitch = RANDOM_LONG(-10, 10) + 100;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_sDGWalkSounds[RANDOM_LONG(0, 1)], 1.0, 0.8, 0, iPitch);
		break;
	}
	case 6: // breathe
	{
		const int iPitch = RANDOM_LONG(-10, 10) + 100;
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_sDGBreathSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, iPitch);
		break;
	}
	default:
		break;
	}
}

// orig 0x15adc0 (HL's DeathEffect)
void CDriveGarg::DeathEffect()
{
	int i;
	UTIL_MakeVectors(pev->angles);
	Vector deathPos = pev->origin + gpGlobals->v_forward * 100;

	// Create a spiral of streaks
	CGDSpiral::Create(deathPos, (pev->absmax.z - pev->absmin.z) * 0.6, 125, 1.5);

	Vector position = pev->origin;
	position.z += 32;
	for (i = 0; i < 7; i += 2)
	{
		DGargSpawnExplosion(position, 70, (i * 0.3), 60 + (i * 20));
		position.z += 15;
	}

	CBaseEntity* pSmoker = CBaseEntity::Create("env_smoker", pev->origin, g_vecZero, NULL);
	pSmoker->pev->health = 1;						 // 1 smoke balls
	pSmoker->pev->scale = 46;						 // 4.6X normal size
	pSmoker->pev->dmg = 0;							 // 0 radial distribution
	pSmoker->pev->nextthink = gpGlobals->time + 2.5; // Start in 2.5 seconds
}

// orig 0x15af60: the driver's view orbits behind the monster; the player entity is moved to the
// camera spot (it is invisible and non-solid while driving)
void CDriveGarg::DEClientPreThink()
{
	if ((m_pOwner->pev->button & IN_RELOAD) == 0)
	{
		pev->ideal_yaw = UTIL_AngleMod(m_pOwner->pev->v_angle.y);
		pev->idealpitch = m_pOwner->pev->v_angle.x;
	}

	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;

	UTIL_MakeVectors(m_pOwner->pev->v_angle);
	Vector vecOrg = GetCamRotOrg();
	if (m_vecMoveVel.Length() > 0.01 || pev->velocity.Length() > 0.01)
	{
		// lead the camera by the time since the last walk step
		if (m_flMoveTime < gpGlobals->time)
			vecOrg = GetCamRotOrg() + (gpGlobals->time - m_flMoveTime) * m_vecMoveVel;
	}

	Vector vecEnd = vecOrg - gpGlobals->v_forward * 300;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	TraceResult tr;
	UTIL_TraceLine(vecOrg, vecEnd, ignore_monsters, ENT(pev), &tr);

	Vector vecCam = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;

	int i = 0;
	if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
	{
		// step the camera back toward the monster until it is out of the wall
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

	m_pOwner->m_afPhysicsFlags = 0;
	UTIL_SetOrigin(m_pOwner->pev, vecCam);

	// the player's velocity follows the monster's, for client prediction
	Vector vecDelta = pev->origin - m_vecLastOrigin;
	float flDelta = gpGlobals->time - m_flLastOriginTime;
	m_flLastOriginTime = gpGlobals->time;
	m_vecLastOrigin = pev->origin;
	if (flDelta > 0.001 && vecDelta.Length() > 0.001)
		m_pOwner->pev->velocity = (1.0f / flDelta) * vecDelta;
	else
		m_pOwner->pev->velocity = g_vecZero;

	if (m_pOwner->m_fDrvEntAlpha < 1.0 && m_pOwner->m_fDrvEntAlpha > 0)
	{
		m_pOwner->pev->euser2 = ENT(pev);
		m_pOwner->pev->iuser3 = 4;
		m_pOwner->pev->fuser1 = 2;
		float flAlpha = 0;
		if (m_pOwner->m_fDrvEntAlpha * 255 >= 0)
		{
			flAlpha = 255;
			if (m_pOwner->m_fDrvEntAlpha * 255 <= 255)
				flAlpha = m_pOwner->m_fDrvEntAlpha * 255;
		}
		m_pOwner->pev->fuser2 = flAlpha;
	}
	else
	{
		m_pOwner->pev->euser3 = NULL; // VERIFY: the original clears euser3 here but sets euser2 above
		m_pOwner->pev->iuser3 = 0;
		m_pOwner->pev->fuser1 = 0;
		m_pOwner->pev->fuser2 = 0;
	}
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
}

// orig 0x15b5f0
void CDriveGarg::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x15b610: the player becomes an invisible camera and takes the controls
void CDriveGarg::DEClientEnter(CBasePlayer* pPlayer)
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

	m_iLastHealth = -1;
	m_vecLastOrigin = pev->origin;
	m_flLastOriginTime = gpGlobals->time;

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

// orig 0x15b9f0: give the player back its body
void CDriveGarg::DEClientLeave()
{
	edict_t* pent;
	if (!m_pOwner || !m_pOwner->pev || (pent = m_pOwner->pev->pContainingEntity) == NULL || pent->free || !m_pOwner->m_bInDriveEnt)
		return;

	CLIENT_COMMAND(pent, "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");
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

// orig 0x15bd00: bail out backward; the monster dies
void CDriveGarg::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVEGARG", "AllowEject", false))
	{
		if (CVAR_GET_FLOAT("sv_allowglobaleject") != 1 && g_pGameRules->IsMultiplayer() && !g_pGameRules->IsCoOp())
			return;
	}

	if (!m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev || !(pev->health > 0))
		return;
	if (!(m_pOwner->m_fDrvEntEjectTime < gpGlobals->time))
		return;

	UTIL_MakeAimVectors(pev->angles);
	Vector vecEnd = pev->origin + Vector(0, 0, 100) - gpGlobals->v_forward * 128;
	TraceResult tr;
	UTIL_TraceLine(pev->origin + Vector(0, 0, 100), vecEnd, ignore_monsters, ENT(pev), &tr);

	DEClientLeave();
	m_pOwner->UpdateClientData();
	m_pOwner->m_bInDriveEnt = false;
	Killed(pev, GIB_NORMAL);

	Vector vecPos = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecPos = tr.vecEndPos + tr.vecPlaneNormal * 64;
	UTIL_SetOrigin(m_pOwner->pev, vecPos);
	m_pOwner->pev->velocity = gpGlobals->v_forward * -900 + Vector(0, 0, 300);
	m_pOwner->pev->v_angle = pev->angles;
	m_pOwner->pev->angles = m_pOwner->pev->v_angle;
	const float flPitch = m_pOwner->pev->v_angle.x;
	m_pOwner->pev->v_angle.x = -flPitch;
	m_pOwner->pev->angles.x = -flPitch;
	m_pOwner->pev->v_angle.z = 0;
	m_pOwner->pev->angles.z = 0;
	m_pOwner->pev->fixangle = 1;
	m_pOwner->m_pCurDriveEnt = NULL;
	EMIT_SOUND_DYN(m_pOwner->pev->pContainingEntity, CHAN_BODY, "ambience/biggun3.wav", 1.0, 0.8, 0, 100);
}

// orig 0x15c110 (HL's CSpiral::Spawn)
void CGDSpiral::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->nextthink = gpGlobals->time;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->effects |= EF_NODRAW;
	pev->angles = g_vecZero;
}

// orig 0x15c190 (HL's CSpiral::Create)
CGDSpiral* CGDSpiral::Create(const Vector& origin, float height, float radius, float duration)
{
	if (duration <= 0)
		return NULL;

	CGDSpiral* pSpiral = GetClassPtr((CGDSpiral*)NULL);
	pSpiral->Spawn();
	pSpiral->pev->dmgtime = pSpiral->pev->nextthink;
	pSpiral->pev->origin = origin;
	pSpiral->pev->scale = radius;
	pSpiral->pev->dmg = height;
	pSpiral->pev->speed = duration;
	pSpiral->pev->health = 0;
	pSpiral->pev->angles = g_vecZero;
	return pSpiral;
}

#define DG_SPIRAL_INTERVAL 0.1 // HL's SPIRAL_INTERVAL

// orig 0x15c240 (HL's CSpiral::Think; RC skips the streaks under sv_lowlag and thinks at 0.1)
void CGDSpiral::Think()
{
	float time = gpGlobals->time - pev->dmgtime;

	while (time > DG_SPIRAL_INTERVAL)
	{
		Vector position = pev->origin;
		Vector direction = Vector(0, 0, 1);

		float fraction = 1.0 / pev->speed;
		float radius = (pev->scale * pev->health) * fraction;

		position.z += (pev->health * pev->dmg) * fraction;
		pev->angles.y = (pev->health * 360 * 8) * fraction;
		UTIL_MakeVectors(pev->angles);
		position = position + gpGlobals->v_forward * radius;
		direction = (direction + gpGlobals->v_forward).Normalize();

		if (CVAR_GET_FLOAT("sv_lowlag") != 1)
			StreakSplash(position, Vector(0, 0, 1), RANDOM_LONG(8, 11), 20, RANDOM_LONG(50, 150), 400);

		// Jeez, how many counters should this take ? :)
		pev->dmgtime += DG_SPIRAL_INTERVAL;
		pev->health += DG_SPIRAL_INTERVAL;
		time -= DG_SPIRAL_INTERVAL;
	}

	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->health >= pev->speed)
		UTIL_Remove(this);
}

// orig 0x15d470
int CGDSpiral::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}

//=========================================================
// CDriveGargPickup: the Gargantua statue a player uses to get in
//=========================================================
class CDriveGargPickup : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	int ObjectCaps() override;
	void SetObjectCollisionBox() override;
	int Classify() override;
	void DeathNotice(entvars_t* pevChild) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	int BloodColor() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int GetRCCaps() override;
	virtual bool IsTeleFraggabe(); // VERIFY return type: returns 0, nothing calls it

	CBaseEntity* GetSpawnOrg();
	void EXPORT PDGAnimThink();
	void EXPORT DoRespawn();

	CBaseEntity* m_pGivenGarg; // orig +0x27c, the drive_garg made for the current driver
	bool m_bTakeable;		   // orig +0x280
};

LINK_ENTITY_TO_CLASS(pickup_drive_garg, CDriveGargPickup);

TYPEDESCRIPTION CDriveGargPickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveGargPickup, m_pGivenGarg, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveGargPickup, m_bTakeable, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDriveGargPickup, CBaseMonster); // orig 0x15c550 (Save), 0x15c5a0 (Restore)

// orig 0x15c5f0
void CDriveGargPickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), "models/garg.mdl");
	UTIL_SetSize(pev, Vector(-64, -64, 0), Vector(64, 64, 200));
	UTIL_SetOrigin(pev, pev->origin);
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->sequence = LookupSequence("idle3");
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);
	InitBoneControllers();
	SetUse(&CDriveGargPickup::Use);
	SetThink(&CDriveGargPickup::PDGAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_bTakeable = true;
	m_pGivenGarg = NULL;

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
	UTIL_Remove(pSpot); // each spawn spot holds one pickup
}

// orig 0x15c830
void CDriveGargPickup::Precache()
{
	PRECACHE_MODEL("models/garg.mdl");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_garg");
}

// orig 0x15c880: the first map-placed spawn spot still standing
CBaseEntity* CDriveGargPickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_garg_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (FNullEnt(pSpot->pev))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x15c900
bool CDriveGargPickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return false;
}

// orig 0x15c910: fade in while waiting; kill the given drive_garg once its time is up
void CDriveGargPickup::PDGAnimThink()
{
	const float flInterval = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_bTakeable)
	{
		if (m_pGivenGarg && m_pGivenGarg->pev->deadflag == DEAD_NO && pev->dmgtime < gpGlobals->time)
		{
			g_pevLastInflictor = pev;
			m_pGivenGarg->Killed(pev, GIB_NORMAL);
			pev->dmgtime = 1000000 + gpGlobals->time;
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

// orig 0x15cab0: a player (the caller) gets in
void CDriveGargPickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!m_bTakeable)
		return;
	// VERIFY: the original calls IsPlayer() before the null test
	if (!pCaller->IsPlayer() || !pCaller)
		return;

	CBasePlayer* pPlayer = (CBasePlayer*)pCaller;
	if (!(pPlayer->m_fDrcEntTransTime <= gpGlobals->time) || pPlayer->m_bInDriveEnt)
		return;
	if (pPlayer->Classify() != CLASS_PLAYER)
		return;
	if (strcmp(STRING(pPlayer->pev->classname), "player") != 0)
		return;

	// the pickup owns the monster, so its death comes back here (DeathNotice)
	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_garg", pev->origin, pev->angles, ENT(pev));
	pPlayer->pev->angles = Vector(0, pev->angles.y, 0);
	pPlayer->pev->v_angle = pPlayer->pev->angles;
	pPlayer->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGivenGarg = pPlayer->m_pCurDriveEnt;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDriveGargPickup::PDGAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargMaxDriveTime", 600) + gpGlobals->time;
}

// orig 0x15ccb0
void CDriveGargPickup::DoRespawn()
{
	m_bTakeable = true;
	m_pGivenGarg = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);
	SetThink(&CDriveGargPickup::PDGAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x15cd70: the drive_garg it gave out died
void CDriveGargPickup::DeathNotice(entvars_t* pevChild)
{
	if (m_bTakeable)
		return;
	pev->nextthink = PREFS_KeyLoopUpFloat("DRIVEGARG", "GargRespawnDelayAfterDeath", 8) + gpGlobals->time;
	SetThink(&CDriveGargPickup::DoRespawn);
}

// orig 0x15d0a0
int CDriveGargPickup::ObjectCaps()
{
	if (m_bTakeable)
		return FCAP_IMPULSE_USE | FCAP_ACROSS_TRANSITION;
	return FCAP_ACROSS_TRANSITION;
}

// orig 0x15d0c0
void CDriveGargPickup::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector(-80, -80, 0);
	pev->absmax = pev->origin + Vector(80, 80, 214);
}

// orig 0x15d190
int CDriveGargPickup::GetRCCaps()
{
	return 0;
}

// orig 0x15d1a0
bool CDriveGargPickup::IsTeleFraggabe()
{
	return false;
}

// orig 0x15d1b0
int CDriveGargPickup::BloodColor()
{
	return DONT_BLEED;
}

// orig 0x15d1c0
int CDriveGargPickup::Classify()
{
	return CLASS_NONE;
}

//=========================================================
// CDrvGSpawner: places mp_drivegarg pickups a while after the map starts
//=========================================================
class CDrvGSpawner : public CBaseEntity
{
public:
	void Spawn() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	int ObjectCaps() override;

	void EXPORT DrvAThink();

	bool m_bDummy; // orig +0x34
};

// orig 0x15cde0: called from CWorld (world.cpp) on map start
void CreateDrvGargSpawner()
{
	if (g_pGameRules->IsMultiplayer() && !UTIL_FindEntityByClassname(NULL, "drive_garg_spawner"))
		CBaseEntity::Create("drive_garg_spawner", g_vecZero, g_vecZero, NULL);
}

LINK_ENTITY_TO_CLASS(drive_garg_spawner, CDrvGSpawner);

TYPEDESCRIPTION CDrvGSpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvGSpawner, m_bDummy, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDrvGSpawner, CBaseEntity); // orig 0x15ce70 (Save), 0x15cec0 (Restore)

// orig 0x15cf10
void CDrvGSpawner::DrvAThink()
{
	const int iCount = (int)CVAR_GET_FLOAT("mp_drivegarg");
	for (int i = 0; i < iCount; i++)
		CBaseEntity::Create("pickup_drive_garg", g_vecZero, g_vecZero, NULL);
	UTIL_Remove(this);
}

// orig 0x15cfa0
void CDrvGSpawner::Spawn()
{
	pev->classname = MAKE_STRING("drive_garg_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvGSpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15;
}

// orig 0x15d040
int CDrvGSpawner::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}

// orig 0x15d220: an entity is going away; if it is the driver, the monster dies
void CDriveGarg::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;
	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

// orig 0x15d2a0
int CDriveGarg::GetRCCaps()
{
	return 0x2c088;
}

// orig 0x15d2b0
float CDriveGarg::VeryHeavyDamageFract()
{
	return 0.4;
}

// orig 0x15d2d0
float CDriveGarg::ExtreemeDamageFract()
{
	return 0.05;
}

// orig 0x15d2f0
Vector CDriveGarg::GetCamRotOrg()
{
	return Center();
}

// orig 0x15d330
void CDriveGarg::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector(-80, -80, 0);
	pev->absmax = pev->origin + Vector(80, 80, 214);
}

// orig 0x15d400
int CDriveGarg::BloodColor()
{
	return BLOOD_COLOR_YELLOW;
}

// orig 0x15d410
int CDriveGarg::Classify()
{
	return CLASS_ALIEN_MONSTER;
}

