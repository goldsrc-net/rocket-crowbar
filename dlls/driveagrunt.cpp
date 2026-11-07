/***
*
*	Rocket Crowbar 1.9: driveagrunt.cpp. The drivable alien grunt (drive_agrunt): its burst gun
*	(attack), slash (attack2), bouncing big gun (jump) and hyper mode (duck), its pickup
*	(pickup_drive_agrunt), and the spawner that places the pickups (drive_agrunt_spawner).
*
*	Reconstructed from the original rc_i386.so; "orig 0x..." is each function's address in
*	reference/funcfile.tsv. reference/dossiers/houndeye-agrunt.md has the detail.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "decals.h"
#include "shake.h"
#include "client.h"
#include "UserMessages.h"
#include "driveent.h"
#include "driveagrunt.h"

// Preff.cpp (rc.ini prefs); the temporary definitions are in drivehoundeye.cpp
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
extern int PREFS_KeyLoopUpInt(const char* szHeading, const char* szKey, int iDefault);
extern bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault);

static const char* g_sDAGMeleeHitSounds[] = {"zombie/claw_strike1.wav", "zombie/claw_strike2.wav", "zombie/claw_strike3.wav"};
static const char* g_sDAGMeleeMissSounds[] = {"zombie/claw_miss1.wav", "zombie/claw_miss2.wav"};
static const char* g_sDAGDieSounds[] = {"agrunt/ag_die1.wav", "agrunt/ag_die4.wav", "agrunt/ag_die5.wav"};
static const char* g_sDAGPainSounds[] = {"agrunt/ag_pain1.wav", "agrunt/ag_pain2.wav", "agrunt/ag_pain3.wav", "agrunt/ag_pain4.wav", "agrunt/ag_pain5.wav"};
static const char* g_sDAGIdleSounds[] = {"agrunt/ag_idle1.wav", "agrunt/ag_idle2.wav", "agrunt/ag_idle3.wav", "agrunt/ag_idle4.wav"};

//=========================================================
// The alien grunt a player drives
//=========================================================
class CDriveAGrunt : public CBaseDriveEnt
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void SetObjectCollisionBox() override;
	int Classify() override;
	int BloodColor() override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	int GetGuassHull() override;
	int GetGuassDistOut() override;
	const char* GetControllerName() override;
	void EntIsBeingRemoved(edict_t* pEdict) override;
	int GetRCCaps() override;

	void DEClientPreThink() override;
	void DEClientPostThink() override;
	void DEClientEnter(CBasePlayer* pPlayer) override;
	void DEClientLeave() override;
	void DEClientEject() override;
	Vector GetCamRotOrg() override;

	void EXPORT DAGThink();
	void EXPORT DyingThink();
	void DoBurstGun();
	void DoSlash();
	void DoBouncyGun();
	void CheckDoor();
	void CheckEHop();
	void RunWaterPhysics();
	void DoMovement();
	void DoYaw();
	void DoHyper();
	void UpdateOwnerHUD();
	void BurstGunFire();
	void SlashFire(bool bLeft);
	void BigGunBeamLine(Vector vecStart, Vector vecEnd, TraceResult* ptr);
	void FireBigGun();
	void SetDeathAnim();

	// m_pOwner (+0x27c) is CBaseDriveEnt's. Saved members, names from the original's save table:
	float m_fFrameTime;		  // orig +0x280
	bool m_bInBurstGun;		  // orig +0x288
	float m_fBurstGunRefire;  // orig +0x28c
	bool m_bInBouncyGun;	  // orig +0x290
	bool m_bBouncyGunHasFired; // orig +0x291
	float m_fBouncyGunRefire; // orig +0x294
	bool m_bInSlash;		  // orig +0x298
	float m_fSlashRefire;	  // orig +0x29c
	int m_iSlashSide;		  // orig +0x2a0: the code uses it as an int, the save table says FIELD_BOOLEAN
	float m_fLastSlash;		  // orig +0x2a4
	bool m_bInHyper;		  // orig +0x2a8
	float m_fNextHyperRefire; // orig +0x2ac
	float m_fHyperSoundPitch; // orig +0x2b0
	float m_fHyperSoundVol;	  // orig +0x2b4
	float m_fHyperRegenTime;  // orig +0x2b8
	Vector m_vecSudoVel;	  // orig +0x2bc, how fast DoMovement moved us, for the camera
	float m_fLastMoveTime;	  // orig +0x2c8
	float m_fNextEHopCheck;	  // orig +0x2cc
	float m_fNextDoorCheck;	  // orig +0x2d0
	int m_iClientHealth;	  // orig +0x2e0
	int m_iClientArmor;		  // orig +0x2e4

	// unsaved, named from what they do
	bool m_bCanIdle;		 // orig +0x284, VERIFY name: DAGThink sets these each frame, the
	bool m_bMoving;			 // orig +0x285, VERIFY name: set and cleared, never read in this file
	bool m_bCanMove;		 // orig +0x286, VERIFY name: attacks clear the ones they block
	bool m_bCanFastTurn;	 // orig +0x287, VERIFY name: the turnl/turnr animations
	float m_fYawSpeedScale;	 // orig +0x2d4
	float m_fNextIdleSound;	 // orig +0x2d8
	float m_fNextPainSound;	 // orig +0x2dc
	int m_iMuzzleFlash;		 // orig +0x2e8, sprites/muzzleflash3.spr
	int m_iWallGlow;		 // orig +0x2ec, sprites/muz7.spr
	int m_iRedFlare;		 // orig +0x2f0, sprites/redflare2.spr
	int m_iBurstBeam;		 // orig +0x2f4, sprites/xbeam1.spr
	int m_iBigGunBeam;		 // orig +0x2f8, sprites/plasma.spr
	int m_iMuzzleTrail;		 // orig +0x2fc, sprites/muz4.spr
};

LINK_ENTITY_TO_CLASS(drive_agrunt, CDriveAGrunt);

// In the original's order, duplicates included.
TYPEDESCRIPTION CDriveAGrunt::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveAGrunt, m_pOwner, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveAGrunt, m_fFrameTime, FIELD_FLOAT),
		DEFINE_FIELD(CDriveAGrunt, m_bInBurstGun, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveAGrunt, m_fBurstGunRefire, FIELD_TIME),
		DEFINE_FIELD(CDriveAGrunt, m_bInBouncyGun, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveAGrunt, m_bBouncyGunHasFired, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveAGrunt, m_fBouncyGunRefire, FIELD_TIME),
		DEFINE_FIELD(CDriveAGrunt, m_bInSlash, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveAGrunt, m_fSlashRefire, FIELD_TIME),
		DEFINE_FIELD(CDriveAGrunt, m_fSlashRefire, FIELD_TIME),
		DEFINE_FIELD(CDriveAGrunt, m_iSlashSide, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveAGrunt, m_iSlashSide, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveAGrunt, m_fLastSlash, FIELD_TIME),
		DEFINE_FIELD(CDriveAGrunt, m_fLastSlash, FIELD_TIME),
		DEFINE_FIELD(CDriveAGrunt, m_bInHyper, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveAGrunt, m_fNextHyperRefire, FIELD_TIME),
		DEFINE_FIELD(CDriveAGrunt, m_fHyperSoundPitch, FIELD_FLOAT),
		DEFINE_FIELD(CDriveAGrunt, m_fHyperSoundVol, FIELD_FLOAT),
		DEFINE_FIELD(CDriveAGrunt, m_fHyperRegenTime, FIELD_TIME),
		DEFINE_FIELD(CDriveAGrunt, m_vecSudoVel, FIELD_VECTOR),
		DEFINE_FIELD(CDriveAGrunt, m_fLastMoveTime, FIELD_TIME),
		DEFINE_FIELD(CDriveAGrunt, m_fNextEHopCheck, FIELD_TIME),
		DEFINE_FIELD(CDriveAGrunt, m_fNextDoorCheck, FIELD_TIME),
		DEFINE_FIELD(CDriveAGrunt, m_iClientHealth, FIELD_INTEGER),
		DEFINE_FIELD(CDriveAGrunt, m_iClientArmor, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CDriveAGrunt, CBaseMonster);

//=========================================================
// The alien grunt standing at an info_drive_agrunt_spawn, waiting for a player to use it
//=========================================================
class CDriveAGruntPickup : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	int ObjectCaps() override;
	int Classify() override;
	int BloodColor() override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void DeathNotice(entvars_t* pevChild) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int GetRCCaps() override;
	virtual bool IsTeleFraggabe();

	CBaseEntity* GetSpawnOrg();
	void EXPORT PAnimThink();
	void EXPORT DoRespawn();

	CBaseEntity* m_pGiven; // the drive_agrunt made for the player using us
	bool m_bTakeable;
};

LINK_ENTITY_TO_CLASS(pickup_drive_agrunt, CDriveAGruntPickup);

TYPEDESCRIPTION CDriveAGruntPickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveAGruntPickup, m_pGiven, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveAGruntPickup, m_bTakeable, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDriveAGruntPickup, CBaseMonster);

//=========================================================
// Places mp_driveagrunt pickups 15 seconds into the map, then removes itself
//=========================================================
class CDrvAGruntSpawner : public CBaseEntity
{
public:
	void Spawn() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	int ObjectCaps() override;
	void EXPORT DrvAThink();

	bool m_bDummy;
};

LINK_ENTITY_TO_CLASS(drive_agrunt_spawner, CDrvAGruntSpawner);

TYPEDESCRIPTION CDrvAGruntSpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvAGruntSpawner, m_bDummy, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDrvAGruntSpawner, CBaseEntity);

//=========================================================
// CDriveAGrunt
//=========================================================

// orig 0x11c910
void CDriveAGrunt::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_agrunt");
	SET_MODEL(ENT(pev), "models/agrunt.mdl");
	UTIL_SetSize(pev, Vector(-22, -22, 0), Vector(22, 22, 64));
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	m_bloodColor = BLOOD_COLOR_RED; // VERIFY: BloodColor() says yellow, as in the original
	pev->effects = 0;
	pev->friction = 1;
	m_HackedGunPos = Vector(0, 24, 48);
	pev->health = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHealth", 300);
	pev->max_health = pev->health;
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	pev->sequence = LookupSequence("idle1");
	ResetSequenceInfo();
	InitBoneControllers();
	pev->frame = 0;

	m_bInBurstGun = false;
	m_bInBouncyGun = false;
	m_fBurstGunRefire = m_fBouncyGunRefire = gpGlobals->time;
	m_iSlashSide = RANDOM_LONG(0, 1);
	m_bInSlash = false;
	m_fSlashRefire = gpGlobals->time;
	m_fLastSlash = gpGlobals->time;
	m_bInHyper = false;
	pev->armorvalue = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntInitialHyper", 50);
	m_fNextHyperRefire = gpGlobals->time;
	m_fHyperRegenTime = gpGlobals->time + 1.0;
	pev->framerate = 1;
	SetEyePosition();

	m_fNextIdleSound = RANDOM_LONG(6, 8) + gpGlobals->time;
	m_bCanIdle = true;
	m_bMoving = false;
	m_bCanMove = true;
	m_vecSudoVel = g_vecZero;
	m_fNextEHopCheck = m_fNextDoorCheck = gpGlobals->time;
	m_fNextPainSound = gpGlobals->time;

	SetThink(&CDriveAGrunt::DAGThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x11cc40
void CDriveAGrunt::Precache()
{
	PRECACHE_MODEL("models/agrunt.mdl");

	for (const char* psz : g_sDAGMeleeHitSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_sDAGMeleeMissSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_sDAGDieSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_sDAGPainSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_sDAGIdleSounds)
		PRECACHE_SOUND(psz);

	PRECACHE_SOUND("debris/beamstart2.wav");
	PRECACHE_SOUND("debris/beamstart9.wav");
	PRECACHE_SOUND("weapons/gauss2.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("weapons/electro5.wav");
	PRECACHE_SOUND("weapons/xbow_hitbod1.wav");
	PRECACHE_SOUND("ambience/alien_minddrill.wav");

	m_iMuzzleFlash = PRECACHE_MODEL("sprites/muzzleflash3.spr");
	m_iWallGlow = PRECACHE_MODEL("sprites/muz7.spr");
	m_iRedFlare = PRECACHE_MODEL("sprites/redflare2.spr");
	m_iBurstBeam = PRECACHE_MODEL("sprites/xbeam1.spr");
	m_iBigGunBeam = PRECACHE_MODEL("sprites/plasma.spr");
	m_iMuzzleTrail = PRECACHE_MODEL("sprites/muz4.spr");
}

// orig 0x11ce30
const char* CDriveAGrunt::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;
	return STRING(m_pOwner->pev->netname);
}

// orig 0x11cea0
void CDriveAGrunt::DAGThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;
	if (m_fFrameTime < 0.001)
		m_fFrameTime = 0.001;

	if (m_pOwner)
	{
		bool bBad = false;
		if (!m_pOwner->pev || !OFFSET(m_pOwner->pev->pContainingEntity))
			bBad = true;

		if (!bBad && m_pOwner->pev && m_pOwner->pev->pContainingEntity)
		{
			edict_t* pent = m_pOwner->pev->pContainingEntity;
			if (pent->pvPrivateData && !pent->free && UTIL_PlayerByIndex(ENTINDEX(pent)))
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
					m_vecSudoVel = g_vecZero;
					m_fLastMoveTime = gpGlobals->time;
					pev->rendermode = kRenderNormal;
					return;
				}

				m_bCanIdle = true;
				m_bMoving = false;
				m_bCanMove = true;
				m_bCanFastTurn = true;
				m_fYawSpeedScale = 1.0;

				DoBurstGun();
				DoSlash();
				DoBouncyGun();
				DoMovement();
				DoYaw();
				DoHyper();

				pev->fuser4 = m_vecSudoVel.Length();

				if (m_bCanIdle)
				{
					if (pev->sequence != LookupSequence("idle1") && (m_fSequenceFinished || m_fSequenceLoops))
					{
						pev->sequence = LookupSequence("idle1");
						ResetSequenceInfo();
						pev->frame = 0;
					}

					if (!(m_fNextIdleSound < gpGlobals->time))
						return;

					int iPitch = RANDOM_LONG(95, 105);
					EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_sDAGIdleSounds[RANDOM_LONG(0, 3)], 1.0, 0.8, 0, iPitch);
				}

				m_fNextIdleSound = RANDOM_LONG(6, 8) + gpGlobals->time;
				return;
			}
		}
	}

	// the driver is gone
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("agrunt");
	g_pevLastInflictor = pev;
	Killed(pev, GIB_NORMAL);
}

// orig 0x11d1f0: attack fires the burst gun (anim events 1-5 shoot)
void CDriveAGrunt::DoBurstGun()
{
	if (m_bInSlash || m_bInBouncyGun)
		return;

	if (!m_bInBurstGun)
	{
		if (gpGlobals->time < m_fBurstGunRefire)
			return;
		if ((m_pOwner->pev->button & IN_ATTACK) == 0)
			return;

		pev->sequence = LookupSequence("longshoot");
		ResetSequenceInfo();
		pev->frame = 0;
		m_bInBurstGun = true;
	}
	else
	{
		if (m_fSequenceFinished)
		{
			m_bInBurstGun = false;
			if (!m_bInHyper)
				m_fBurstGunRefire = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntBurstGunRefire", 0.7) + gpGlobals->time;
			else
				m_fBurstGunRefire = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntBurstGunRefireHyper", 0.25) + gpGlobals->time;
			return;
		}

		m_bCanIdle = false;
		m_bCanMove = false;
		m_bCanFastTurn = false;
	}

	if (m_bInHyper)
		pev->framerate = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntBurstGunHyperSpeedScale", 1.8);
}

// orig 0x11d330: attack2 slashes, left and right in turn
void CDriveAGrunt::DoSlash()
{
	if (m_bInBurstGun || m_bInBouncyGun)
		return;

	if (m_bInSlash)
	{
		if (!m_fSequenceFinished)
		{
			m_bCanIdle = false;
			m_bCanMove = false;
			m_bCanFastTurn = false;
			if (m_bInHyper)
				pev->framerate = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntSlashHyperSpeedScale", 2.5);
			return;
		}

		m_bInSlash = false;
		m_fLastSlash = gpGlobals->time;
		if (!m_bInHyper)
			m_fSlashRefire = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntSlashRefire", 0.2) + gpGlobals->time;
		else
			m_fSlashRefire = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntSlashRefireHyper", 0) + gpGlobals->time;
	}

	if (gpGlobals->time < m_fSlashRefire)
		return;
	if ((m_pOwner->pev->button & IN_ATTACK2) == 0)
		return;

	// after a pause, start from a random side
	if (m_fLastSlash + 4.0 < gpGlobals->time)
		m_iSlashSide = RANDOM_LONG(0, 1);
	m_iSlashSide = m_iSlashSide == 0;

	pev->sequence = LookupSequence("mattack2") + m_iSlashSide;
	ResetSequenceInfo();
	pev->frame = 0;
	m_bInSlash = true;

	if (m_bInHyper)
		pev->framerate = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntSlashHyperSpeedScale", 2.5);
}

// orig 0x11d4c0: jump fires the bouncing big gun once per animation
void CDriveAGrunt::DoBouncyGun()
{
	if (m_bInSlash || m_bInBurstGun)
		return;

	if (!m_bInBouncyGun)
	{
		if (gpGlobals->time < m_fBouncyGunRefire)
			return;
		if ((m_pOwner->pev->button & IN_JUMP) == 0)
			return;

		pev->sequence = LookupSequence("quickshoot");
		ResetSequenceInfo();
		pev->frame = 0;
		m_bInBouncyGun = true;
		m_bBouncyGunHasFired = false;
	}
	else
	{
		if (m_fSequenceFinished)
		{
			m_bInBouncyGun = false;
			if (!m_bInHyper)
				m_fBouncyGunRefire = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntBigGunRefire", 1.5) + gpGlobals->time;
			else
				m_fBouncyGunRefire = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntBigGunRefireHyper", 0.7) + gpGlobals->time;
			return;
		}

		m_bCanIdle = false;
		m_bCanMove = false;
		m_bCanFastTurn = false;
	}

	if (m_bInHyper)
		pev->framerate = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntBigGunHyperSpeedScale", 1.8);
}

// orig 0x11d610: touch a door we're walking into, so it opens
void CDriveAGrunt::CheckDoor()
{
	if (gpGlobals->time < m_fNextDoorCheck)
		return;
	if ((pev->flags & FL_ONGROUND) == 0)
		return;
	if (!m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecStart = pev->origin + Vector(0, 0, 32);
	Vector vecEnd = vecStart + gpGlobals->v_forward * (m_fFrameTime * pev->framerate * m_flGroundSpeed);

	TraceResult tr;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

	if (tr.pHit && (!strcmp(STRING(tr.pHit->v.classname), "func_door") || !strcmp(STRING(tr.pHit->v.classname), "func_door_rotating")))
	{
		CBaseEntity* pDoor = CBaseEntity::Instance(tr.pHit);
		if (pDoor)
			pDoor->Touch(this);
	}

	m_fNextDoorCheck = gpGlobals->time + 0.3;
}

// orig 0x11d870: hop down when the ground ahead drops away
void CDriveAGrunt::CheckEHop()
{
	if (gpGlobals->time < m_fNextEHopCheck)
		return;
	if (16 < pev->velocity.z || (pev->flags & FL_ONGROUND) == 0 || !m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecAhead = pev->origin + Vector(0, 0, 4) + gpGlobals->v_forward * 48;
	Vector vecDown = vecAhead - Vector(0, 0, 28);

	TraceResult tr;
	UTIL_TraceLine(pev->origin + Vector(0, 0, 4), vecAhead, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
	{
		UTIL_TraceLine(vecAhead, vecDown, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
		{
			pev->velocity.z += 50;
			pev->velocity = pev->velocity + gpGlobals->v_forward * 100;
			pev->flags &= ~FL_ONGROUND;
			UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
		}
	}
	// VERIFY: unlike drivehoundeye.cpp, the original never sets m_fNextEHopCheck here
}

// orig 0x11db70
void CDriveAGrunt::RunWaterPhysics()
{
	float flFriction = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntWaterFriction", 8);
	m_fYawSpeedScale = 0.7;

	float flSpeed = pev->velocity.Length();
	if (0.1 < flSpeed)
	{
		float flControl = flSpeed < 6 ? 6 : flSpeed;
		float flNewSpeed = flSpeed - flControl * flFriction * m_fFrameTime;
		if (flNewSpeed < 0)
			flNewSpeed = 0;
		pev->velocity = pev->velocity * (flNewSpeed / flSpeed);
	}

	if (pev->waterlevel > 1)
	{
		// float up
		float flGravity = CVAR_GET_FLOAT("sv_gravity") * 1.1;
		if (0 < pev->gravity)
			flGravity *= pev->gravity;
		pev->velocity.z = flGravity * m_fFrameTime + pev->velocity.z;
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
	if (0.1 < pev->velocity.Length2D())
		flRate = pev->velocity.Length2D() / 30;
	else
		flRate = 0.6;

	if (flRate < 0.6)
		pev->framerate = 0.6;
	else if (1.0 < flRate)
		pev->framerate = 1.0;
	else
		pev->framerate = flRate;

	if ((m_pOwner->pev->button & IN_FORWARD) != 0)
	{
		UTIL_MakeAimVectors(pev->angles);
		float flMaxSpeed = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntWaterSpeed", 180);
		float flAccel = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntWaterAccel", 170);
		Vector vecForward = gpGlobals->v_forward;

		float flAdd = flMaxSpeed - DotProduct(pev->velocity, vecForward);
		if (0 < flAdd)
		{
			float flAccelSpeed = flAccel * m_fFrameTime * flMaxSpeed;
			if (flAdd < flAccelSpeed)
				flAccelSpeed = flAdd;
			pev->velocity = pev->velocity + vecForward * flAccelSpeed;
		}
	}
}

// orig 0x11dee0: forward runs (walk/use for the walk)
void CDriveAGrunt::DoMovement()
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

		bool bWalk = (m_pOwner->pev->button & (IN_USE | IN_RUN)) != 0;
		float flOldFrame = pev->frame;
		int iOldSequence = pev->sequence;
		float flOldFramerate = pev->framerate;
		bool bWasStill = false;
		if (pev->sequence != LookupSequence("run"))
			bWasStill = pev->sequence != LookupSequence("walk");

		const char* pszSequence = bWalk ? "walk" : "run";
		if (pev->sequence != LookupSequence(pszSequence))
		{
			pev->sequence = LookupSequence(pszSequence);
			ResetSequenceInfo();
			pev->frame = 0;
		}

		if (!m_bInHyper)
			pev->framerate = 1.0;
		else
			pev->framerate = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperSpeedScale", 3.0);

		CheckDoor();
		m_bCanIdle = false;
		m_bMoving = true;

		UTIL_MakeVectors(pev->angles);
		Vector vecGoal = Center() + gpGlobals->v_forward * 512;

		// find how far we can walk this frame, in steps of up to 16
		float flMoved = 0;
		Vector vecStart = pev->origin;
		float flDist = m_flGroundSpeed * pev->framerate * m_fFrameTime;
		while (0.001 < flDist)
		{
			float flStep = 16 < flDist ? 16 : flDist;
			Vector vecSave = pev->origin;

			while (!WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY) && 2 < flStep)
				flStep -= 2;

			UTIL_SetOrigin(pev, vecSave);
			if (!WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY))
			{
				if (flMoved < 0.01)
				{
					// blocked
					if (bWasStill)
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
					m_bMoving = false;
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

		// then really move that far
		UTIL_SetOrigin(pev, vecStart);
		Vector vecBefore = pev->origin;
		while (0.001 < flMoved)
		{
			float flStep = 16 < flMoved ? 16 : flMoved;
			UTIL_MoveToOrigin(ENT(pev), vecGoal, flStep, MOVE_NORMAL);
			flMoved -= flStep;
		}

		Vector vecMoved = pev->origin - vecBefore;
		if (0.01 < vecMoved.Length() && 0.001 < m_fFrameTime)
			m_vecSudoVel = vecMoved / m_fFrameTime;
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
		if (10 < flSpeed && flSpeed < 70)
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

// orig 0x11e590: turn toward the driver's view; a big turn while standing plays the turn
void CDriveAGrunt::DoYaw()
{
	float flSpeed = m_fFrameTime * 1900 * m_fYawSpeedScale;
	bool bFastTurn = false;
	if (m_bInHyper)
		flSpeed *= 1.5;

	float flDiff = UTIL_AngleDistance(pev->ideal_yaw, pev->angles.y);
	float flAbsDiff = fabs(flDiff);
	if (flAbsDiff < 0.001f)
		return;

	if (70 < flAbsDiff && m_bCanFastTurn && (pev->flags & FL_ONGROUND) != 0)
	{
		flSpeed = m_fFrameTime * 2800 * m_fYawSpeedScale;
		bFastTurn = true;
	}

	if (bFastTurn && pev->sequence == LookupSequence("idle1"))
	{
		pev->sequence = LookupSequence(0 < flDiff ? "turnl" : "turnr");
		ResetSequenceInfo();
		pev->frame = 0;
		m_bCanIdle = false;
	}

	ChangeYaw((int)flSpeed);
}

// orig 0x11e6f0: duck starts hyper mode: faster, harder hitting and regenerating, burning the
// hyper charge (armor); when it runs out, the hangover hurts
void CDriveAGrunt::DoHyper()
{
	if (!m_bInHyper)
	{
		if (pev->renderfx == kRenderFxGlowShell)
		{
			// the red glow fades off
			pev->renderamt = pev->renderamt - m_fFrameTime * 60;
			pev->rendercolor.x = pev->rendercolor.x - m_fFrameTime * 212.5;
			if (pev->renderamt <= 0 || pev->rendercolor.x <= 0)
			{
				pev->renderamt = 0;
				pev->renderfx = kRenderFxNone;
			}
		}

		if (m_fHyperRegenTime < gpGlobals->time)
		{
			m_fHyperRegenTime = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperRechargeDelay", 1.0) + gpGlobals->time;
			pev->armorvalue = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperRechargePerUnit", 1.0) + pev->armorvalue;
			if (PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperMax", 100) < pev->armorvalue)
				pev->armorvalue = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperMax", 100);
		}

		if (m_fNextHyperRefire <= gpGlobals->time && (m_pOwner->pev->button & IN_DUCK) != 0 && PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperMinToStart", 10) <= pev->armorvalue)
		{
			m_bInHyper = true;
			m_fHyperRegenTime = gpGlobals->time;
			EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "debris/beamstart2.wav", 1.0, 0.8, 0, 120);
			m_fHyperSoundPitch = 60;
			m_fHyperSoundVol = 0.4;
			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alien_minddrill.wav", 0.4, 0.8, 0, 60);
			UTIL_ScreenFade(m_pOwner, Vector(255, 0, 0), 0.5, 0, 64, FFADE_OUT | FFADE_STAYOUT);
			pev->renderamt = 0;
			pev->renderfx = kRenderFxGlowShell;
			pev->rendercolor = Vector(0, 0, 0);
		}
		return;
	}

	pev->renderamt = m_fFrameTime * 24 + pev->renderamt;
	if (72 < pev->renderamt)
		pev->renderamt = 72;
	pev->rendercolor.x = m_fFrameTime * 85 + pev->rendercolor.x;
	if (255.0 < (double)pev->rendercolor.x)
		pev->rendercolor.x = 255;

	m_fHyperSoundPitch = m_fFrameTime * 40 + m_fHyperSoundPitch;
	if (120 < m_fHyperSoundPitch)
		m_fHyperSoundPitch = 120;
	m_fHyperSoundVol = m_fFrameTime * 0.4 + m_fHyperSoundVol;
	if (1.0 < m_fHyperSoundVol)
		m_fHyperSoundVol = 1.0;

	if (m_fHyperRegenTime < gpGlobals->time)
	{
		pev->health = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperHealthRegenUnit", 15) + pev->health;
		if (pev->max_health < pev->health)
			pev->health = pev->max_health;
		m_fHyperRegenTime = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperHealthRegenDelay", 0.2) + gpGlobals->time;
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alien_minddrill.wav", m_fHyperSoundVol, 0.8, SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)m_fHyperSoundPitch);

	pev->armorvalue = pev->armorvalue - PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperDrainPerSec", 5) * m_fFrameTime;
	if (pev->armorvalue <= 0)
	{
		m_bInHyper = false;
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "debris/beamstart2.wav", 0, 0, SND_STOP, 100);
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alien_minddrill.wav", 0, 0, SND_STOP, 100);
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "weapons/electro5.wav", 1.0, 0.8, 0, 60);

		pev->classname = MAKE_STRING("agrunt_hyper_hangover");
		TakeDamage(pev, m_pOwner->pev, PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperHangoverDamage", 30), 0);
		pev->classname = MAKE_STRING("drive_agrunt");

		m_fHyperRegenTime = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperRechargeRestart", 1.0) + gpGlobals->time;
		m_fNextHyperRefire = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperRefireTiem", 10) + gpGlobals->time;
		UTIL_ScreenFade(m_pOwner, Vector(255, 0, 0), 0.5, 0, 64, FFADE_IN);
	}
}

// orig 0x11ed30: our health (as a percentage) and hyper charge are the driver's HUD health and armor
void CDriveAGrunt::UpdateOwnerHUD()
{
	if (!m_pOwner)
		return;

	if (pev->health != (float)m_iClientHealth)
	{
		float flPercent = pev->health / pev->max_health * 100;
		int iHealth = 0 < flPercent ? (int)flPercent : 0;
		if (iHealth < 1)
			iHealth = 1;
		else if (100 < iHealth)
			iHealth = 100;

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_SHORT(iHealth);
		MESSAGE_END();

		m_iClientHealth = (int)pev->health;
	}

	if (pev->armorvalue != (float)m_iClientArmor)
	{
		short sArmor;
		if (0 <= pev->armorvalue)
			sArmor = 999 < pev->armorvalue ? 999 : (short)(int)pev->armorvalue;
		else
			sArmor = 0;

		MESSAGE_BEGIN(MSG_ONE, gmsgBattery, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_SHORT(sArmor);
		MESSAGE_END();

		m_iClientArmor = (int)pev->armorvalue;
	}
}

// orig 0x11ef40: one shot of the burst gun, from the gun attachment
void CDriveAGrunt::BurstGunFire()
{
	if (!m_pOwner)
		return;

	float flDamage = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntBurstGunDamagePerBeam", 24);
	if (m_bInHyper)
		flDamage = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntBurstGunHyperDamagePerBeam", 32);

	Vector vecSrc, vecAngles;
	GetAttachment(0, vecSrc, vecAngles);

	Vector vecAim = Vector(-60, pev->angles.y, pev->angles.z);
	if (-60 <= pev->idealpitch - 15)
		vecAim.x = 60 < pev->idealpitch - 15 ? 60 : pev->idealpitch - 15;
	UTIL_MakeVectors(vecAim);
	SetBlending(0, -vecAim.x);

	float x, y;
	do
	{
		x = RANDOM_FLOAT(-0.5, 0.5);
		x = x + RANDOM_FLOAT(-0.5, 0.5);
		y = RANDOM_FLOAT(-0.5, 0.5);
		y = y + RANDOM_FLOAT(-0.5, 0.5);
	} while (1 < x * x + y * y);

	Vector vecDir = gpGlobals->v_forward + x * 0.01745 * gpGlobals->v_right + y * 0.01745 * gpGlobals->v_up;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, ENT(pev), &tr);
	Vector vecEnd = tr.vecEndPos;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(vecEnd.x);
	WRITE_COORD(vecEnd.y);
	WRITE_COORD(vecEnd.z);
	WRITE_SHORT(m_iBurstBeam);
	WRITE_BYTE(0);	// startframe
	WRITE_BYTE(50); // framerate
	WRITE_BYTE(1);	// life
	WRITE_BYTE(50); // width
	WRITE_BYTE(0);	// noise
	WRITE_BYTE(RANDOM_LONG(240, 255));
	WRITE_BYTE(RANDOM_LONG(16, 32));
	WRITE_BYTE(RANDOM_LONG(12, 24));
	WRITE_BYTE(RANDOM_LONG(240, 250)); // brightness
	WRITE_BYTE(200);				   // speed
	MESSAGE_END();

	Vector vecMuzzle = vecSrc + gpGlobals->v_forward * 12;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecMuzzle);
	WRITE_BYTE(TE_SPRITE);
	WRITE_COORD(vecMuzzle.x);
	WRITE_COORD(vecMuzzle.y);
	WRITE_COORD(vecMuzzle.z);
	WRITE_SHORT(m_iMuzzleFlash);
	WRITE_BYTE(4);	 // scale
	WRITE_BYTE(180); // brightness
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecMuzzle);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(vecMuzzle.x);
	WRITE_COORD(vecMuzzle.y);
	WRITE_COORD(vecMuzzle.z);
	WRITE_COORD(vecMuzzle.x + gpGlobals->v_forward.x);
	WRITE_COORD(vecMuzzle.y + gpGlobals->v_forward.y);
	WRITE_COORD(vecMuzzle.z + gpGlobals->v_forward.z);
	WRITE_SHORT(m_iRedFlare);
	WRITE_BYTE(4);	// count
	WRITE_BYTE(3);	// life
	WRITE_BYTE(2);	// scale
	WRITE_BYTE(18); // velocity
	WRITE_BYTE(17); // randomness
	MESSAGE_END();

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/beamstart9.wav", 1.0, 0.8, 0, m_bInHyper ? 200 : 170);
	UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));

	if (!tr.pHit)
		return;

	CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
	if (!pEntity)
		return;

	pev->classname = MAKE_STRING("agrunt_burst_gun");
	ClearMultiDamage();
	pEntity->TraceAttack(m_pOwner->pev, flDamage, gpGlobals->v_forward, &tr, DMG_ENERGYBEAM);
	ApplyMultiDamage(pev, m_pOwner->pev);
	pev->classname = MAKE_STRING("drive_agrunt");

	if (!strcmp(STRING(pEntity->pev->classname), "worldspawn"))
	{
		Vector vecGlow = vecEnd + tr.vecPlaneNormal * 2;
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecGlow);
		WRITE_BYTE(TE_GLOWSPRITE);
		WRITE_COORD(vecGlow.x);
		WRITE_COORD(vecGlow.y);
		WRITE_COORD(vecGlow.z);
		WRITE_SHORT(m_iWallGlow);
		WRITE_BYTE(4);	 // life
		WRITE_BYTE(3);	 // scale
		WRITE_BYTE(128); // brightness
		MESSAGE_END();
	}

	if (pEntity->IsBSPModel())
	{
		Vector vecSparks = vecEnd + tr.vecPlaneNormal * 4;
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSparks);
		WRITE_BYTE(TE_SPRITETRAIL);
		WRITE_COORD(vecSparks.x);
		WRITE_COORD(vecSparks.y);
		WRITE_COORD(vecSparks.z);
		WRITE_COORD(vecSparks.x + tr.vecPlaneNormal.x);
		WRITE_COORD(vecSparks.y + tr.vecPlaneNormal.y);
		WRITE_COORD(vecSparks.z + tr.vecPlaneNormal.z);
		WRITE_SHORT(m_iRedFlare);
		WRITE_BYTE(5);	// count
		WRITE_BYTE(3);	// life
		WRITE_BYTE(3);	// scale
		WRITE_BYTE(18); // velocity
		WRITE_BYTE(15); // randomness
		MESSAGE_END();
	}
}

// orig 0x11fae0: the slash of anim events 12 (right) and 13 (left)
void CDriveAGrunt::SlashFire(bool bLeft)
{
	if (!m_pOwner)
		return;

	float flDamage = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntSlashDamage", 35);
	if (m_bInHyper)
		flDamage = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntSlashHyperDamage", 45);

	Vector vecStart = pev->origin + Vector(0, 0, 38);
	UTIL_MakeVectors(pev->angles);
	Vector vecEnd = vecStart + gpGlobals->v_forward * 72;

	TraceResult tr;
	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, human_hull, ENT(pev), &tr);

	if (!tr.pHit || tr.pHit->v.takedamage == DAMAGE_NO)
	{
		int iPitch = m_bInHyper ? RANDOM_LONG(120, 130) : RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_sDAGMeleeMissSounds[RANDOM_LONG(0, 1)], 1.0, 0.9, 0, iPitch);
		return;
	}

	int iPitch = m_bInHyper ? RANDOM_LONG(120, 130) : RANDOM_LONG(95, 105);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_sDAGMeleeHitSounds[RANDOM_LONG(0, 2)], 1.0, 0.9, 0, iPitch);

	CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
	if (!pEntity)
		return;

	pev->classname = MAKE_STRING("agrunt_slash");
	ClearMultiDamage();
	pEntity->TraceAttack(m_pOwner->pev, flDamage, gpGlobals->v_forward, &tr, DMG_SLASH);
	ApplyMultiDamage(pev, m_pOwner->pev);
	pev->classname = MAKE_STRING("drive_agrunt");

	const int iCap = 0x2000000;
	if (pEntity->HasRCCap(iCap))
	{
		// VERIFY: the original knocks it to the same side for both hands
		if (bLeft)
			pEntity->pev->velocity = pEntity->pev->velocity + gpGlobals->v_forward * 150 + gpGlobals->v_right * 160;
		else
			pEntity->pev->velocity = pEntity->pev->velocity + gpGlobals->v_forward * 150 + gpGlobals->v_right * 160;
		pEntity->pev->flags &= ~FL_ONGROUND;
		UTIL_SetOrigin(pEntity->pev, pEntity->pev->origin + Vector(0, 0, 1));

		if (pEntity->IsNetClient())
		{
			if (bLeft)
				pEntity->pev->punchangle.y = pEntity->pev->punchangle.y - 25;
			else
				pEntity->pev->punchangle.y = pEntity->pev->punchangle.y + 25;
			pEntity->pev->punchangle.x = pEntity->pev->punchangle.x + 10;
			pEntity->pev->punchangle.z = pEntity->pev->punchangle.z + 5;
		}
	}
}

// orig 0x11fff0: draw one leg of the big gun's beam, with sparks and a zap where it hit
void CDriveAGrunt::BigGunBeamLine(Vector vecStart, Vector vecEnd, TraceResult* ptr)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecStart);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(vecStart.x);
	WRITE_COORD(vecStart.y);
	WRITE_COORD(vecStart.z);
	WRITE_COORD(vecEnd.x);
	WRITE_COORD(vecEnd.y);
	WRITE_COORD(vecEnd.z);
	WRITE_SHORT(m_iBigGunBeam);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(200); // framerate
	WRITE_BYTE(1);	 // life
	WRITE_BYTE(70);	 // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(RANDOM_LONG(240, 255));
	WRITE_BYTE(RANDOM_LONG(32, 64));
	WRITE_BYTE(RANDOM_LONG(200, 230));
	WRITE_BYTE(RANDOM_LONG(240, 250)); // brightness
	WRITE_BYTE(200);				   // speed
	MESSAGE_END();

	UTIL_DecalTrace(ptr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));

	if (!ptr->pHit)
		return;

	Vector vecHit = vecEnd + ptr->vecPlaneNormal * 4;

	if (!strcmp(STRING(ptr->pHit->v.classname), "worldspawn"))
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecHit);
		WRITE_BYTE(TE_GLOWSPRITE);
		WRITE_COORD(vecHit.x);
		WRITE_COORD(vecHit.y);
		WRITE_COORD(vecHit.z);
		WRITE_SHORT(m_iWallGlow);
		WRITE_BYTE(6);	 // life
		WRITE_BYTE(5);	 // scale
		WRITE_BYTE(200); // brightness
		MESSAGE_END();
	}

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecHit);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(vecHit.x);
	WRITE_COORD(vecHit.y);
	WRITE_COORD(vecHit.z);
	WRITE_COORD(vecHit.x + ptr->vecPlaneNormal.x);
	WRITE_COORD(vecHit.y + ptr->vecPlaneNormal.y);
	WRITE_COORD(vecHit.z + ptr->vecPlaneNormal.z);
	WRITE_SHORT(m_iMuzzleTrail);
	WRITE_BYTE(6);	// count
	WRITE_BYTE(4);	// life
	WRITE_BYTE(2);	// scale
	WRITE_BYTE(23); // velocity
	WRITE_BYTE(20); // randomness
	MESSAGE_END();

	// a short-lived entity at the hit to play the zap from
	CBaseEntity* pEmitter = GetClassPtr((CBaseEntity*)NULL);
	pEmitter->Spawn();
	pEmitter->pev->classname = MAKE_STRING("sound_emiiter");
	pEmitter->SetThink(&CBaseEntity::SUB_Remove);
	pEmitter->pev->nextthink = gpGlobals->time + 0.1;
	UTIL_SetOrigin(pEmitter->pev, vecEnd);

	int iPitch = RANDOM_LONG(120, 130);
	float flVolume = RANDOM_FLOAT(0.95, 1.0);
	EMIT_SOUND_DYN(pEmitter->pev->pContainingEntity, CHAN_WEAPON, "weapons/electro4.wav", flVolume, 1.2, 0, iPitch);
}

// orig 0x120560: the big gun: a beam that goes through anything it can hurt and bounces off
// walls, AGruntBigGunMaxBounces times
void CDriveAGrunt::FireBigGun()
{
	if (!m_pOwner)
		return;

	float flDamage = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntBigGunDamagePerBeam", 40);
	int iBounces = PREFS_KeyLoopUpInt("DRIVEAGRUNT", "AGruntBigGunMaxBounces", 5);
	if (m_bInHyper)
	{
		flDamage = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntBigGunHyperDamagePerBeam", 50);
		iBounces = (int)PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntBigGunHyperMaxBounces", 7.0);
	}

	Vector vecGun, vecAngles;
	GetAttachment(0, vecGun, vecAngles);

	Vector vecAim = Vector(-60, pev->angles.y, pev->angles.z);
	if (-60 <= pev->idealpitch - 15)
		vecAim.x = 60 < pev->idealpitch - 15 ? 60 : pev->idealpitch - 15;
	UTIL_MakeVectors(vecAim);
	SetBlending(0, -vecAim.x);

	Vector vecSrc = vecGun;
	Vector vecDir = gpGlobals->v_forward;
	edict_t* pentIgnore = ENT(pev);
	Vector vecMuzzle = vecGun + gpGlobals->v_forward * 12;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecMuzzle);
	WRITE_BYTE(TE_SPRITE);
	WRITE_COORD(vecMuzzle.x);
	WRITE_COORD(vecMuzzle.y);
	WRITE_COORD(vecMuzzle.z);
	WRITE_SHORT(m_iMuzzleFlash);
	WRITE_BYTE(4);	 // scale
	WRITE_BYTE(200); // brightness
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecMuzzle);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(vecMuzzle.x);
	WRITE_COORD(vecMuzzle.y);
	WRITE_COORD(vecMuzzle.z);
	WRITE_COORD(vecMuzzle.x + gpGlobals->v_forward.x);
	WRITE_COORD(vecMuzzle.y + gpGlobals->v_forward.y);
	WRITE_COORD(vecMuzzle.z + gpGlobals->v_forward.z);
	WRITE_SHORT(m_iMuzzleTrail);
	WRITE_BYTE(5);	// count
	WRITE_BYTE(3);	// life
	WRITE_BYTE(2);	// scale
	WRITE_BYTE(18); // velocity
	WRITE_BYTE(20); // randomness
	MESSAGE_END();

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/gauss2.wav", 1.0, 0.7, 0, m_bInHyper ? 130 : 110);

	for (; 0 < iBounces; iBounces--)
	{
		float x, y;
		do
		{
			x = RANDOM_FLOAT(-0.5, 0.5);
			x = x + RANDOM_FLOAT(-0.5, 0.5);
			y = RANDOM_FLOAT(-0.5, 0.5);
			y = y + RANDOM_FLOAT(-0.5, 0.5);
		} while (1 < x * x + y * y);

		UTIL_MakeAimVectors(UTIL_VecToAngles(vecDir));
		Vector vecShoot = gpGlobals->v_forward + x * 0.01745 * gpGlobals->v_right + y * 0.01745 * gpGlobals->v_up;
		Vector vecEnd = vecSrc + vecShoot * 8192;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, pentIgnore, &tr);
		pentIgnore = NULL;

		// go on through up to 8 things it can hurt
		int i;
		bool bMissed = false;
		for (i = 8; 0 < i; i--)
		{
			if (!tr.pHit)
			{
				bMissed = true;
				break;
			}
			if (tr.pHit->v.takedamage == DAMAGE_NO)
				break;
			if (!CBaseEntity::Instance(tr.pHit))
				break;
			if (CBaseEntity::Instance(tr.pHit)->IsBSPModel())
				break;
			CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
			if (!pEntity)
				break;

			pev->classname = MAKE_STRING("agrunt_bouncy_gun");
			ClearMultiDamage();
			pEntity->TraceAttack(m_pOwner->pev, flDamage, vecShoot, &tr, DMG_ENERGYBEAM);
			ApplyMultiDamage(pev, m_pOwner->pev);
			pev->classname = MAKE_STRING("drive_agrunt");
			EMIT_SOUND_DYN(pEntity->pev->pContainingEntity, CHAN_BODY, "weapons/xbow_hitbod1.wav", 0.8, 1.1, 0, 110);

			const int iCap = 0x800000;
			if (pEntity->HasRCCap(iCap))
			{
				pEntity->pev->velocity = pEntity->pev->velocity + vecShoot * 120 + Vector(0, 0, 10);
				// VERIFY: the original keeps only FL_ONGROUND here (&= where ~ was likely meant)
				pEntity->pev->flags &= FL_ONGROUND;
			}

			UTIL_TraceLine(tr.vecEndPos, vecEnd, dont_ignore_monsters, pEntity->pev->pContainingEntity, &tr);
		}

		// it stopped on something hurtable that is a brush: hurt that too
		if (!bMissed && tr.pHit && tr.pHit->v.takedamage != DAMAGE_NO && 0 < i)
		{
			CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
			if (pEntity && pEntity->IsBSPModel())
			{
				pev->classname = MAKE_STRING("agrunt_bouncy_gun");
				ClearMultiDamage();
				pEntity->TraceAttack(m_pOwner->pev, flDamage, vecShoot, &tr, DMG_ENERGYBEAM);
				ApplyMultiDamage(pev, m_pOwner->pev);
				pev->classname = MAKE_STRING("drive_agrunt");
			}
		}

		BigGunBeamLine(vecSrc, tr.vecEndPos, &tr);

		// bounce
		vecSrc = tr.vecEndPos + tr.vecPlaneNormal * 2;
		float flDot = -DotProduct(tr.vecPlaneNormal, vecShoot);
		vecDir = tr.vecPlaneNormal * 2 * flDot + vecShoot;
	}
}

// orig 0x121200
void CDriveAGrunt::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
	case 2:
	case 3:
	case 4:
	case 5: // shoot
		if (m_bInBurstGun)
		{
			BurstGunFire();
		}
		else if (m_bInBouncyGun && !m_bBouncyGunHasFired)
		{
			m_bBouncyGunHasFired = true;
			FireBigGun();
		}
		break;

	case 10: // left foot
	{
		int iPitch = m_bInHyper ? 110 : 70;
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_ladder2.wav", 1.0, 0.8, 0, iPitch);
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_ladder4.wav", 1.0, 0.8, 0, iPitch);
			break;
		}
		break;
	}

	case 11: // right foot
	{
		int iPitch = m_bInHyper ? 110 : 70;
		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_ladder1.wav", 1.0, 0.8, 0, iPitch);
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "player/pl_ladder3.wav", 1.0, 0.8, 0, iPitch);
			break;
		}
		break;
	}

	case 12:
		SlashFire(false);
		break;

	case 13:
		SlashFire(true);
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

// orig 0x121540
void CDriveAGrunt::DyingThink()
{
	pev->nextthink = 0.1 + gpGlobals->time;

	if (!m_fSequenceFinished && gpGlobals->time < pev->dmgtime)
	{
		DispatchAnimEvents(StudioFrameAdvance());
		return;
	}

	pev->deadflag = DEAD_DEAD;
	if (pev->dmgtime <= gpGlobals->time || (pev->flags & FL_ONGROUND) != 0)
		FadeMonster();
}

// orig 0x121600
void CDriveAGrunt::SetDeathAnim()
{
	UTIL_MakeVectors(pev->angles);
	float flDot = DotProduct(gpGlobals->v_forward, g_vecAttackDir * -1);

	const char* pszSequence;
	if (m_LastHitGroup == HITGROUP_HEAD)
		pszSequence = "diehead";
	else if (0.3 < flDot)
		pszSequence = "dieforward";
	else if (-0.3 < flDot)
		pszSequence = m_LastHitGroup == HITGROUP_STOMACH ? "diegut" : "diesimple";
	else
		pszSequence = "diebackward";

	pev->sequence = LookupSequence(pszSequence);
	ResetSequenceInfo();
	pev->frame = 0;
}

// orig 0x121700
bool CDriveAGrunt::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_pOwner)
	{
		// whoever can't hurt the driver can't hurt us, nor can anyone hurt a god-mode driver
		if (CBaseEntity::Instance(pevAttacker->pContainingEntity) && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, CBaseEntity::Instance(pevAttacker->pContainingEntity)))
			return false;
		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	if (!m_bInHyper)
	{
		if (m_fNextPainSound < gpGlobals->time)
		{
			m_fNextPainSound = RANDOM_FLOAT(3, 4) + gpGlobals->time;
			int iPitch = RANDOM_LONG(85, 120);
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_sDAGPainSounds[RANDOM_LONG(0, 4)], 1.0, 0.8, 0, iPitch);
		}
	}
	else
	{
		flDamage = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntHyperDamageScale", 0.6) * flDamage;
	}

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x1218c0: the armor (hitgroup 10) turns bullets and blades
void CDriveAGrunt::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (ptr->iHitgroup == 10 && (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_CLUB)) != 0)
	{
		if (pev->dmgtime != gpGlobals->time || RANDOM_LONG(0, 10) < 1)
		{
			UTIL_Ricochet(ptr->vecEndPos, RANDOM_FLOAT(1, 2));
			pev->dmgtime = gpGlobals->time;
		}

		flDamage -= 40;
		if (flDamage <= 0)
			return;
	}
	else
	{
		SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage);
		TraceBleed(flDamage, vecDir, ptr, bitsDamageType);
	}

	AddMultiDamage(pevAttacker, this, flDamage, bitsDamageType);
}

// orig 0x121a10: the driver dies with us
void CDriveAGrunt::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->deadflag != DEAD_NO)
		return;

	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;

	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 0, 0, 0, 100);

	if (m_bInHyper)
	{
		if (m_pOwner)
			UTIL_ScreenFade(m_pOwner, Vector(255, 0, 0), 0.5, 0, 64, FFADE_IN);
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alien_minddrill.wav", 0, 0, SND_STOP, 100);
	}

	pev->renderamt = 0;
	pev->renderfx = kRenderFxNone;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_BYTE(HIDEHUD_ALL);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_SHORT(0);
		MESSAGE_END();
	}

	// tell the pickup that made us
	if (pev->owner)
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	pev->classname = MAKE_STRING("AGrunt");
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
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_sDAGDieSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, 100);
		SetThink(&CDriveAGrunt::DyingThink);
		pev->nextthink = gpGlobals->time + 0.1;
		pev->dmgtime = gpGlobals->time + 5.0;
	}
}

// orig 0x121e20: steer from the driver's view, and put the driver's camera behind us
void CDriveAGrunt::DEClientPreThink()
{
	if ((m_pOwner->pev->button & IN_RELOAD) == 0)
	{
		// holding reload looks around without steering
		pev->ideal_yaw = UTIL_AngleMod(m_pOwner->pev->v_angle.y);
		pev->idealpitch = m_pOwner->pev->v_angle.x;
	}

	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;

	SetEyePosition();
	UTIL_MakeVectors(m_pOwner->pev->v_angle);

	Vector vecOrg = GetCamRotOrg();
	if (0.01 < m_vecSudoVel.Length() && m_fLastMoveTime < gpGlobals->time)
		vecOrg = GetCamRotOrg() + m_vecSudoVel * (gpGlobals->time - m_fLastMoveTime);

	TraceResult tr;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine(vecOrg, vecOrg - gpGlobals->v_forward * 180, ignore_monsters, ENT(pev), &tr);

	Vector vecCam = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;

	// out of water, walk the camera back toward us until it's in the open
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

	float flAlpha = m_pOwner->m_fDrvEntAlpha;
	if (1.0 <= flAlpha || !(0 < flAlpha) || m_bInHyper)
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
		m_pOwner->pev->fuser1 = 2.0;

		float flAmt = m_pOwner->m_fDrvEntAlpha * 255;
		if (flAmt < 0)
			flAmt = 0;
		else if (255 < flAmt)
			flAmt = 255;
		m_pOwner->pev->fuser2 = flAmt;
	}
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
}

// orig 0x1223a0
void CDriveAGrunt::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x1223c0: the player climbs in and becomes a camera
void CDriveAGrunt::DEClientEnter(CBasePlayer* pPlayer)
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
	m_iClientArmor = -1;

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

	// the driver's movement keys would move the camera
	CLIENT_COMMAND(m_pOwner->pev->pContainingEntity, "cl_forwardspeed 0\ncl_backspeed 0\ncl_sidespeed 0\n");
}

// orig 0x122780: give the player back their body
void CDriveAGrunt::DEClientLeave()
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

// orig 0x122a90: the eject command: jump out, and the grunt gibs
void CDriveAGrunt::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVEAGRUNT", "AllowEject", false))
	{
		if (CVAR_GET_FLOAT("sv_allowglobaleject") != 1 && g_pGameRules->IsMultiplayer() && !g_pGameRules->IsCoOp())
			return;
	}

	CBasePlayer* pOwner = m_pOwner;
	if (!pOwner->m_bInDriveEnt || !pOwner->m_pCurDriveEnt || pOwner->m_pCurDriveEnt->pev != pev || !(0 < pOwner->m_pCurDriveEnt->pev->health))
		return;
	if (!(pOwner->m_fDrvEntEjectTime < gpGlobals->time))
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

// orig 0x1238d0
void CDriveAGrunt::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;

	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

// orig 0x123950
int CDriveAGrunt::GetRCCaps()
{
	return 0x1aa2dfd;
}

// orig 0x123960
int CDriveAGrunt::GetGuassDistOut()
{
	return 56;
}

// orig 0x123970
int CDriveAGrunt::GetGuassHull()
{
	return human_hull;
}

// orig 0x123980
Vector CDriveAGrunt::GetCamRotOrg()
{
	return pev->origin + Vector(0, 0, 72);
}

// orig 0x1239e0
void CDriveAGrunt::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector(-32, -32, 0);
	pev->absmax = pev->origin + Vector(32, 32, 85);
}

// orig 0x123ab0
int CDriveAGrunt::BloodColor()
{
	return BLOOD_COLOR_YELLOW;
}

// orig 0x123ac0
int CDriveAGrunt::Classify()
{
	return CLASS_ALIEN_MILITARY;
}

//=========================================================
// CDriveAGruntPickup
//=========================================================

// orig 0x122d70
void CDriveAGruntPickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), "models/agrunt.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));
	UTIL_SetOrigin(pev, pev->origin);
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->sequence = LookupSequence("idle1");
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);
	InitBoneControllers();

	SetUse(&CDriveAGruntPickup::Use);
	SetThink(&CDriveAGruntPickup::PAnimThink);
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

// orig 0x122fb0
void CDriveAGruntPickup::Precache()
{
	PRECACHE_MODEL("models/agrunt.mdl");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_agrunt");
}

// orig 0x123000: the first spawn point not being removed (each pickup takes one and removes it)
CBaseEntity* CDriveAGruntPickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_agrunt_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (!pSpot->pev || !OFFSET(pSpot->pev->pContainingEntity))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x123080
bool CDriveAGruntPickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return false;
}

// orig 0x123090: fade in; and kill the grunt we gave out once its drive time is up
void CDriveAGruntPickup::PAnimThink()
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

	if (pev->rendermode != kRenderTransTexture || 150 <= pev->renderamt)
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

// orig 0x123230: a player uses us and climbs into a new drive_agrunt
void CDriveAGruntPickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!m_bTakeable)
		return;
	// VERIFY: the original calls IsPlayer before testing pCaller for NULL
	if (!pCaller || !pCaller->IsPlayer())
		return;

	CBasePlayer* pPlayer = (CBasePlayer*)pCaller;
	if (gpGlobals->time < pPlayer->m_fDrcEntTransTime || pPlayer->m_bInDriveEnt)
		return;
	if (pPlayer->Classify() != CLASS_PLAYER)
		return;
	if (strcmp(STRING(pPlayer->pev->classname), "player") != 0)
		return;

	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_agrunt", pev->origin, pev->angles, ENT(pev));
	pPlayer->pev->angles = Vector(0, pev->angles.y, 0);
	pPlayer->pev->v_angle = pPlayer->pev->angles;
	pPlayer->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGiven = pPlayer->m_pCurDriveEnt;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDriveAGruntPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntMaxDriveTime", 600) + gpGlobals->time;
}

// orig 0x123430
void CDriveAGruntPickup::DoRespawn()
{
	m_bTakeable = true;
	m_pGiven = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);
	SetThink(&CDriveAGruntPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x1234f0: the grunt we gave out died; come back after a while
void CDriveAGruntPickup::DeathNotice(entvars_t* pevChild)
{
	if (m_bTakeable)
		return;

	pev->nextthink = PREFS_KeyLoopUpFloat("DRIVEAGRUNT", "AGruntRespawnDelayAfterDeath", 5) + gpGlobals->time;
	SetThink(&CDriveAGruntPickup::DoRespawn);
}

// orig 0x123820
int CDriveAGruntPickup::ObjectCaps()
{
	if (m_bTakeable)
		return FCAP_IMPULSE_USE | FCAP_ACROSS_TRANSITION;
	return FCAP_ACROSS_TRANSITION;
}

// orig 0x123840
int CDriveAGruntPickup::GetRCCaps()
{
	return 0;
}

// orig 0x123850
bool CDriveAGruntPickup::IsTeleFraggabe()
{
	return false;
}

// orig 0x123860
int CDriveAGruntPickup::BloodColor()
{
	return DONT_BLEED;
}

// orig 0x123870
int CDriveAGruntPickup::Classify()
{
	return CLASS_NONE;
}

//=========================================================
// CDrvAGruntSpawner
//=========================================================

// orig 0x123560
void CreateDrvAGruntSpawner()
{
	if (!g_pGameRules->IsMultiplayer())
		return;
	if (!UTIL_FindEntityByClassname(NULL, "drive_agrunt_spawner"))
		CBaseEntity::Create("drive_agrunt_spawner", g_vecZero, g_vecZero, NULL);
}

// orig 0x123690
void CDrvAGruntSpawner::DrvAThink()
{
	int iCount = (int)CVAR_GET_FLOAT("mp_driveagrunt");
	for (int i = 0; i < iCount; i++)
		CBaseEntity::Create("pickup_drive_agrunt", g_vecZero, g_vecZero, NULL);

	UTIL_Remove(this);
}

// orig 0x123720
void CDrvAGruntSpawner::Spawn()
{
	pev->classname = MAKE_STRING("drive_agrunt_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvAGruntSpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15.0;
}

// orig 0x1237c0
int CDrvAGruntSpawner::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}
