/***
*
*	Rocket Crowbar 1.9: drivehoundeye.cpp. The drivable houndeye (drive_houndeye): its pickup
*	(pickup_drive_houndeye), the spawner that places the pickups (drive_houndeye_spawner), the
*	eye flare and the charge sprites of its eye attack, and the eye beam: a line that punches
*	through walls, a cylinder of damage along it, and a fade-out beam drawn over it.
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
#include "effects.h"
#include "customentity.h"
#include "gamerules.h"
#include "decals.h"
#include "client.h"
#include "UserMessages.h"
#include "driveent.h"
#include "drivehoundeye.h"

// Preff.cpp (rc.ini prefs): a key's value, or the default when rc.ini doesn't have it
extern float PREFS_KeyLoopUpFloat(const char* pszHeading, const char* pszKey, float flDefault);
extern int PREFS_KeyLoopUpInt(const char* pszHeading, const char* pszKey, int iDefault);
extern bool PREFS_KeyLoopUpBool(const char* pszHeading, const char* pszKey, bool bDefault);

static const char* g_szDHEAlertSounds[] = {"houndeye/he_alert1.wav", "houndeye/he_alert2.wav", "houndeye/he_alert3.wav"};
static const char* g_szDHEDieSounds[] = {"houndeye/he_die1.wav", "houndeye/he_die2.wav", "houndeye/he_die3.wav"};
static const char* g_szDHEIdleSounds[] = {"houndeye/he_idle1.wav", "houndeye/he_idle2.wav", "houndeye/he_idle3.wav"};
static const char* g_szDHEHuntSounds[] = {"houndeye/he_hunt1.wav", "houndeye/he_hunt2.wav", "houndeye/he_hunt3.wav"};
static const char* g_szDHEPainSounds[] = {"houndeye/he_pain1.wav", "houndeye/he_pain3.wav", "houndeye/he_pain4.wav", "houndeye/he_pain5.wav"};
static const char* g_szDHEAttackSounds[] = {"houndeye/he_attack1.wav", "houndeye/he_attack3.wav"};
static const char* g_szDHEBlastSounds[] = {"houndeye/he_blast1.wav", "houndeye/he_blast2.wav", "houndeye/he_blast3.wav"};

// An idle animation picked by weight; it plays for a random time between the two bounds.
struct sDHEAnimT
{
	const char* pszSequence;
	float flWeight;
	float flMinTime;
	float flMaxTime;
};

static sDHEAnimT g_sDHEIdleAnims[] = {{"idle1", 0.4, 8, 16}, {"idle3", 0.25, 10, 20}};
static sDHEAnimT g_sDHEAttackAnims[] = {{"madidle", 0.4, 0.7, 1.1}, {"madidle2", 0.2, 0.8, 1.2}, {"madidle3", 0.3, 0.6, 0.9}};

static int g_iDHEGreenBallSpray; // sprites/heeyeflare3.spr
static int g_iDHEWallFlare;		 // sprites/heeyeflare4.spr

// The entities one eye beam has hit, with the damage and screen fade each one gets. Filled by the
// beam's traces, then applied all at once by DoItterOnAll, so nothing is hurt twice by one beam.
struct sDHEItteredEntT
{
	int iEntIndex; // < 1 is a free slot
	float flDamage;
	float flFadeTime;
	float flFadeHold;
};

#define DHE_MAX_ITTERED 512
static sDHEItteredEntT g_sDHEItteredEnts[DHE_MAX_ITTERED];

void FireDHEEyeBeam(entvars_t* pevInflictor, entvars_t* pevAttacker, Vector vecSrc, Vector vecDir);

//=========================================================
// The flare on the houndeye's eye while it charges
//=========================================================
class CDHEEyeSprite : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	static CDHEEyeSprite* CreateES(entvars_t* pevOwner);
	void EXPORT ESThink();
};

LINK_ENTITY_TO_CLASS(he_eye_flare, CDHEEyeSprite);

//=========================================================
// A spark the charging eye sucks in
//=========================================================
class CDHESuckingSprite : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	static CDHESuckingSprite* CreateSS(entvars_t* pevOwner, Vector vecOrigin, float flMaxBrightness, float flLife);
	void EXPORT SSThink();
};

LINK_ENTITY_TO_CLASS(he_charge_flare, CDHESuckingSprite);

//=========================================================
// A beam that fades out over its life, then removes itself (no entity factory)
//=========================================================
class DHEBeamDrawFader : public CBaseEntity
{
public:
	static DHEBeamDrawFader* CreateFader(float flLife, bool bShadeIn, Vector vecStart, Vector vecEnd);
	void EXPORT FadeThink();

	CBeam* m_pBeam; // orig +0x34
};

//=========================================================
// The houndeye a player drives
//=========================================================
class CDriveHE : public CBaseDriveEnt
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override;
	int BloodColor() override;
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

	void EXPORT DHEThink();
	void EXPORT DyingThink();
	void DoSonicAttack();
	void DoStartEyeAttack();
	void DoSleep();
	void CheckDoor();
	void CheckEHop();
	int GetRunSeq();
	int GetWalkSeq();
	bool IsWalk(int iSequence);
	void RunWaterPhysics();
	void DoMovement();
	void DoYaw();
	void DoEye();
	void UpdateOwnerHUD();
	float SetRandomAnim(sDHEAnimT* pAnims, int iCount);
	void DoSonicAttackBlast();
	void EyeAttackStop();
	void DoEyeAttackFrame();
	void SetDeathAnim();

	// None of these are saved (the original has no save table for CDriveHE); m_pOwner (+0x27c) is
	// CBaseDriveEnt's. Names are chosen from what each does (CDriveAGrunt's matching members are saved,
	// so those names are real).
	float m_fFrameTime;			 // orig +0x280
	Vector m_vecSudoVel;		 // orig +0x284, how fast DoMovement moved us, for the camera
	float m_fLastMoveTime;		 // orig +0x290
	float m_fNextEHopCheck;		 // orig +0x294
	float m_fNextDoorCheck;		 // orig +0x298
	bool m_bCanIdle;			 // orig +0x29c, VERIFY name: DHEThink sets these each frame and
	bool m_bCanMove;			 // orig +0x29d, VERIFY name: each attack or sleep step clears the
	bool m_bCanTurn;			 // orig +0x29e, VERIFY name: ones it blocks
	bool m_bCanFastTurn;		 // orig +0x29f, VERIFY name: the 180 turn animations
	bool m_bCanJumpBack;		 // orig +0x2a0, VERIFY name
	bool m_bCanSleep;			 // orig +0x2a1, VERIFY name
	bool m_bCanEyeAttack;		 // orig +0x2a2, VERIFY name: set and cleared, never read in this file
	float m_fYawSpeedScale;		 // orig +0x2a4
	bool m_bCanBlink;			 // orig +0x2a8, VERIFY name
	bool m_bInSonicAttack;		 // orig +0x2a9
	float m_fSonicRefire;		 // orig +0x2ac
	float m_fLastAttack;		 // orig +0x2b0, the sonic blast or the eye beam
	bool m_bInEyeAttack;		 // orig +0x2b4, charging the eye beam
	float m_fEyeAttackRefire;	 // orig +0x2b8
	float m_fEyeAttackFireTime;	 // orig +0x2bc, when the charge fires
	bool m_bSleeping;			 // orig +0x2c0
	bool m_bWantSleep;			 // orig +0x2c1
	float m_fSleepRegenTime;	 // orig +0x2c4
	float m_fNextIdleAnim;		 // orig +0x2c8
	float m_fNextBlink;			 // orig +0x2cc
	float m_fNextJumpBack;		 // orig +0x2d0
	float m_fNextPainSound;		 // orig +0x2d4
	float m_fNextIdleSound;		 // orig +0x2d8
	int m_iSpriteTexture;		 // orig +0x2dc, sprites/shockwave.spr
	CDHEEyeSprite* m_pEyeSprite; // orig +0x2e0
	int m_iClientHealth;		 // orig +0x2e4, the health last sent to the driver's HUD
};

LINK_ENTITY_TO_CLASS(drive_houndeye, CDriveHE);

//=========================================================
// The houndeye standing at an info_drive_houndeye_spawn, waiting for a player to use it
//=========================================================
class CDriveHEPickup : public CBaseMonster
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

	CBaseEntity* m_pGiven; // the drive_houndeye made for the player using us
	bool m_bTakeable;
};

LINK_ENTITY_TO_CLASS(pickup_drive_houndeye, CDriveHEPickup);

TYPEDESCRIPTION CDriveHEPickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveHEPickup, m_pGiven, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveHEPickup, m_bTakeable, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDriveHEPickup, CBaseMonster);

//=========================================================
// Places mp_drivehoundeye pickups 15 seconds into the map, then removes itself
//=========================================================
class CDrvHESpawner : public CBaseEntity
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

LINK_ENTITY_TO_CLASS(drive_houndeye_spawner, CDrvHESpawner);

TYPEDESCRIPTION CDrvHESpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvHESpawner, m_bDummy, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDrvHESpawner, CBaseEntity);

//=========================================================
// CDriveHE
//=========================================================

// orig 0x161840
void CDriveHE::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_houndeye");
	SET_MODEL(ENT(pev), "models/houndeye.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 36));
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->effects = 0;
	pev->friction = 1;
	pev->health = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundHealth", 120);
	pev->max_health = pev->health;
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	m_pEyeSprite = NULL;
	pev->sequence = LookupSequence("idle2");
	ResetSequenceInfo();
	pev->frame = 0;
	pev->framerate = 1;
	SetEyePosition();

	m_bInSonicAttack = false;
	m_bInEyeAttack = false;
	m_bWantSleep = false;
	m_bSleeping = false;
	m_fLastMoveTime = gpGlobals->time;
	m_vecSudoVel = g_vecZero;
	m_fNextIdleAnim = RANDOM_FLOAT(3, 8) + gpGlobals->time;
	m_fNextBlink = RANDOM_FLOAT(2, 6) + gpGlobals->time;
	m_fNextIdleSound = RANDOM_FLOAT(4, 7) + gpGlobals->time;
	m_fNextJumpBack = m_fNextPainSound = gpGlobals->time;
	m_fSonicRefire = m_fEyeAttackRefire = gpGlobals->time;
	m_fNextEHopCheck = m_fNextDoorCheck = gpGlobals->time;

	SetThink(&CDriveHE::DHEThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x161b10
void CDriveHE::Precache()
{
	PRECACHE_MODEL("models/houndeye.mdl");

	for (const char* psz : g_szDHEAlertSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_szDHEDieSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_szDHEIdleSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_szDHEHuntSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_szDHEPainSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_szDHEAttackSounds)
		PRECACHE_SOUND(psz);
	for (const char* psz : g_szDHEBlastSounds)
		PRECACHE_SOUND(psz);

	PRECACHE_SOUND("weapons/egon_run3.wav");
	PRECACHE_SOUND("debris/beamstart10.wav");
	PRECACHE_SOUND("debris/beamstart9.wav");
	PRECACHE_SOUND("ambience/particle_suck1.wav");

	m_iSpriteTexture = PRECACHE_MODEL("sprites/shockwave.spr");
	g_iDHEGreenBallSpray = PRECACHE_MODEL("sprites/heeyeflare3.spr");
	g_iDHEWallFlare = PRECACHE_MODEL("sprites/heeyeflare4.spr");

	UTIL_PrecacheOther("he_charge_flare");
	UTIL_PrecacheOther("he_eye_flare");
}

// orig 0x161ce0
const char* CDriveHE::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;
	return STRING(m_pOwner->pev->netname);
}

// orig 0x161d50
void CDriveHE::DHEThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

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

				if ((pev->flags & FL_FROZEN) != 0)
				{
					EyeAttackStop();
					m_bInSonicAttack = false;
					pev->sequence = LookupSequence("idle2");
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
				m_bCanTurn = true;
				m_bCanFastTurn = true;
				m_bCanBlink = true;
				m_bCanJumpBack = true;
				m_bCanSleep = true;
				m_bCanEyeAttack = true;
				m_fYawSpeedScale = 1.0;

				UpdateOwnerHUD();
				DoEyeAttackFrame();
				DoSonicAttack();
				DoStartEyeAttack();
				DoSleep();
				DoMovement();
				DoYaw();
				DoEye();

				pev->fuser4 = m_vecSudoVel.Length();

				if (!m_bCanIdle)
				{
					m_fNextIdleAnim = RANDOM_FLOAT(3, 7) + gpGlobals->time;
					m_fNextIdleSound = RANDOM_FLOAT(4, 7) + gpGlobals->time;
					return;
				}

				if (m_fNextIdleSound < gpGlobals->time)
				{
					if (m_fLastAttack + 2.4 <= gpGlobals->time)
					{
						m_fNextIdleSound = RANDOM_FLOAT(4, 7) + gpGlobals->time;
						EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_szDHEIdleSounds[RANDOM_LONG(0, 2)], 0.7, 1.1, 0, 100);
					}
					else
					{
						m_fNextIdleSound = RANDOM_FLOAT(1.6, 2.1) + gpGlobals->time;
					}
				}

				if (!m_fSequenceFinished)
					return;

				if (m_fNextIdleAnim < gpGlobals->time)
				{
					if (m_fLastAttack + 2.4 <= gpGlobals->time)
						m_fNextIdleAnim = SetRandomAnim(g_sDHEIdleAnims, 2) + gpGlobals->time;
					else
						m_fNextIdleAnim = SetRandomAnim(g_sDHEAttackAnims, 3) + gpGlobals->time;
					return;
				}

				if (pev->sequence == LookupSequence("idle2"))
					return;

				pev->sequence = LookupSequence("idle2");
				ResetSequenceInfo();
				pev->frame = 0;
				return;
			}
		}
	}

	// the driver is gone
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("houndeye");
	g_pevLastInflictor = pev;
	Killed(pev, GIB_NORMAL);
}

// orig 0x1621c0: the sonic blast (attack); the blast itself is the anim event 3
void CDriveHE::DoSonicAttack()
{
	if (!m_bInSonicAttack)
	{
		if (m_bSleeping || m_bWantSleep || m_bInEyeAttack)
			return;
		if ((m_pOwner->pev->button & IN_ATTACK) == 0)
			return;
		if (gpGlobals->time < m_fSonicRefire)
			return;

		pev->sequence = LookupSequence("attack");
		ResetSequenceInfo();
		pev->frame = 0;
		pev->framerate = 1.33;
		m_bInSonicAttack = true;
		m_bCanIdle = false;
		m_bCanMove = false;
		m_bCanTurn = true;
		m_bCanFastTurn = false;
		m_bCanBlink = false;
		m_bCanJumpBack = false;
		m_bCanSleep = false;
		m_bCanEyeAttack = false;
	}
	else
	{
		if (m_fSequenceFinished)
		{
			m_bInSonicAttack = false;
			m_fSonicRefire = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundSonicRefire", 0.34) + gpGlobals->time;
			m_fLastAttack = gpGlobals->time;
			m_fNextIdleAnim = m_fNextIdleSound = gpGlobals->time - 1.0;
			return;
		}

		m_bCanIdle = false;
		m_bCanMove = false;
		m_bCanTurn = true;
		m_bCanFastTurn = false;
		m_bCanBlink = false;
		m_bCanJumpBack = false;
		m_bCanSleep = false;
	}

	m_fYawSpeedScale = 0.6;
	pev->skin = 0;
}

// orig 0x162370: start charging the eye beam (attack2)
void CDriveHE::DoStartEyeAttack()
{
	if (m_bSleeping || m_bWantSleep || m_bInEyeAttack)
		return;
	if ((m_pOwner->pev->button & IN_ATTACK2) == 0 || (pev->flags & FL_ONGROUND) == 0)
		return;
	if (gpGlobals->time < m_fEyeAttackRefire)
		return;

	m_bInEyeAttack = true;
	m_fEyeAttackFireTime = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttackChargeTime", 3.5) + gpGlobals->time;
	m_bCanIdle = false;
	m_bCanMove = false;
	m_bCanTurn = true;
	m_bCanFastTurn = false;
	m_bCanBlink = false;
	m_bCanJumpBack = false;
	m_bCanSleep = false;
	m_bCanEyeAttack = false;
	m_fYawSpeedScale = 0.4;

	pev->sequence = LookupSequence("madidle2");
	ResetSequenceInfo();
	pev->frame = 0;
	pev->framerate = 0.8;

	m_pEyeSprite = CDHEEyeSprite::CreateES(pev);

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/egon_run3.wav", 0.6, 0.8, 0, 60);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "ambience/particle_suck1.wav", 0.6, 0.75, 0, 80);

	DoEyeAttackFrame();
}

// orig 0x162500: duck lies down to sleep (regenerating health), duck again gets up
void CDriveHE::DoSleep()
{
	if ((pev->flags & FL_ONGROUND) == 0)
		m_bWantSleep = false;

	if (m_bSleeping == m_bWantSleep)
	{
		if (!m_bSleeping)
		{
			if (!m_bCanSleep)
				return;
			if ((m_pOwner->pev->button & IN_DUCK) == 0)
				return;
			if ((pev->flags & FL_ONGROUND) == 0)
				return;
			m_bWantSleep = true;
		}
		else
		{
			m_bCanIdle = false;
			m_bCanMove = false;
			m_bCanTurn = false;
			m_bCanFastTurn = false;
			m_bCanBlink = false;
			m_bCanJumpBack = false;
			pev->skin = 2;

			if (m_fSleepRegenTime < gpGlobals->time && PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundSleepRegenInterval", 0.4) > 0)
			{
				m_fSleepRegenTime = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundSleepRegenInterval", 0.4) + gpGlobals->time;
				pev->health = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundSleepRegenHealthPerInterval", 1.2) + pev->health;
				if (pev->health > pev->max_health)
					pev->health = pev->max_health;
			}

			if ((m_pOwner->pev->button & IN_DUCK) == 0)
				return;
			m_bWantSleep = false;
		}
	}

	if (!m_bWantSleep)
	{
		// getting up
		if (pev->sequence != LookupSequence("sleeptostand") && pev->sequence != LookupSequence("wakefast"))
		{
			pev->sequence = LookupSequence("sleeptostand");
			ResetSequenceInfo();
			pev->frame = 0;
		}

		m_bCanIdle = false;
		m_bCanMove = false;
		m_bCanTurn = false;
		m_bCanFastTurn = false;
		m_bCanJumpBack = false;
		m_bCanBlink = false;
		m_fNextBlink = gpGlobals->time + 3.0;

		if (!m_fSequenceFinished)
		{
			if (pev->frame >= 70 && pev->skin > 0)
				pev->skin--;
		}
		else
		{
			m_bSleeping = false;
			m_bCanBlink = true;
			pev->sequence = LookupSequence("idle2");
			ResetSequenceInfo();
			pev->frame = 0;
			pev->skin = 0;
		}
	}
	else
	{
		// lying down
		if (pev->sequence != LookupSequence("gotosleep"))
		{
			pev->sequence = LookupSequence("gotosleep");
			ResetSequenceInfo();
			pev->frame = 0;
			pev->skin = 0;
			pev->framerate = 1.5;
		}

		m_bCanIdle = false;
		m_bCanMove = false;
		m_bCanTurn = false;
		m_bCanFastTurn = false;
		m_bCanJumpBack = false;
		m_bCanBlink = false;
		m_fNextBlink = gpGlobals->time + 3.0;

		if (!m_fSequenceFinished)
		{
			if (pev->frame >= 160 && pev->skin < 2)
				pev->skin++;
		}
		else
		{
			m_bSleeping = true;
			m_bCanBlink = false;
			pev->sequence = LookupSequence("sleep");
			ResetSequenceInfo();
			pev->frame = 0;
			pev->skin = 2;
			m_fSleepRegenTime = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundSleepRegenInterval", 0.4) + gpGlobals->time;
		}
	}
}

// orig 0x162910: touch a door we're walking into, so it opens
void CDriveHE::CheckDoor()
{
	if (gpGlobals->time < m_fNextDoorCheck)
		return;
	if (!m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecStart = pev->origin + Vector(0, 0, 16);
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

// orig 0x162b60: hop down when the ground ahead drops away
void CDriveHE::CheckEHop()
{
	if (gpGlobals->time < m_fNextEHopCheck || !m_bCanMove)
		return;
	if (16 < pev->velocity.z || (pev->flags & FL_ONGROUND) == 0)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecAhead = pev->origin + Vector(0, 0, 4) + gpGlobals->v_forward * 28;
	Vector vecDown = vecAhead - Vector(0, 0, 24);

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

	m_fNextEHopCheck = gpGlobals->time + 0.2;
}

// orig 0x162e80
int CDriveHE::GetRunSeq()
{
	return 4;
}

// orig 0x162e90: the limping walk below a fifth of max health
int CDriveHE::GetWalkSeq()
{
	if (pev->max_health / 5 < pev->health)
		return 15;
	return 14;
}

// orig 0x162ee0
bool CDriveHE::IsWalk(int iSequence)
{
	return (unsigned int)(iSequence - 14) < 2;
}

// orig 0x162f00
void CDriveHE::RunWaterPhysics()
{
	float flFriction = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundWaterFriction", 8);

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

	if (!m_bCanMove)
		return;

	if (pev->sequence != 3)
	{
		pev->sequence = 3;
		ResetSequenceInfo();
	}

	float flRate;
	if (0.1 < pev->velocity.Length2D())
		flRate = pev->velocity.Length2D() / 70;
	else
		flRate = 0.3;

	if (flRate < 0.3)
		pev->framerate = 0.3;
	else if (1.1 < flRate)
		pev->framerate = 1.1;
	else
		pev->framerate = flRate;

	if ((m_pOwner->pev->button & IN_FORWARD) != 0)
	{
		UTIL_MakeAimVectors(pev->angles);
		float flMaxSpeed = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundWaterSpeed", 160);
		float flAccel = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundWaterAccel", 180);
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

// orig 0x163210: forward walks (walk/use for the slow walk), back hops backward
void CDriveHE::DoMovement()
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
			if (30 < flSpeed && flSpeed < 70)
			{
				if (pev->sequence != GetRunSeq())
				{
					pev->sequence = GetRunSeq();
					ResetSequenceInfo();
					pev->frame = 0;
				}
			}
			else if (flSpeed >= 70 && pev->sequence != GetWalkSeq())
			{
				pev->sequence = GetWalkSeq();
				ResetSequenceInfo();
				pev->frame = 0;
			}
		}
	}
	else if (m_bCanMove)
	{
		if ((m_pOwner->pev->button & IN_BACK) != 0 && m_bCanJumpBack && (pev->flags & FL_ONGROUND) != 0 && m_fNextJumpBack < gpGlobals->time && pev->sequence == LookupSequence("idle2"))
		{
			m_fNextJumpBack = gpGlobals->time + 1.2;
			UTIL_MakeVectors(pev->angles);

			TraceResult tr;
			UTIL_TraceHull(pev->origin, pev->origin + gpGlobals->v_forward * -100, dont_ignore_monsters, head_hull, ENT(pev), &tr);
			if (tr.flFraction == 1.0)
			{
				// room to hop back: the jumpback anim event 6 does the hop
				m_bCanTurn = false;
				m_bCanFastTurn = false;
				m_bCanIdle = false;
				pev->sequence = LookupSequence("jumpback");
				ResetSequenceInfo();
				pev->frame = 0;
				m_vecSudoVel = g_vecZero;
				m_fLastMoveTime = gpGlobals->time;
				return;
			}
		}

		if ((m_pOwner->pev->button & IN_FORWARD) != 0)
		{
			bool bWalk = (m_pOwner->pev->button & (IN_USE | IN_RUN)) != 0;
			float flOldFrame = pev->frame;
			int iOldSequence = pev->sequence;
			bool bWasStill = false;
			if (pev->sequence != GetRunSeq())
				bWasStill = !IsWalk(pev->sequence);

			if (bWalk)
			{
				if (pev->sequence != GetWalkSeq())
				{
					pev->sequence = GetWalkSeq();
					ResetSequenceInfo();
					pev->frame = 0;
				}
			}
			else if (pev->sequence != GetRunSeq())
			{
				pev->sequence = GetRunSeq();
				ResetSequenceInfo();
				pev->frame = 0;
			}

			pev->framerate = 1.0;
			CheckDoor();
			m_bCanIdle = false;

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

			// then really move that far
			UTIL_SetOrigin(pev, vecStart);
			Vector vecBefore = pev->origin;
			float flIdealYaw = pev->ideal_yaw;
			pev->ideal_yaw = pev->angles.y;
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

			pev->ideal_yaw = flIdealYaw;
			m_bCanFastTurn = false;

			if (m_fNextIdleSound < gpGlobals->time)
			{
				m_fNextIdleSound = RANDOM_FLOAT(1.6, 2.1) + gpGlobals->time;
				const char** pszSounds = m_fLastAttack + 15 <= gpGlobals->time ? g_szDHEAlertSounds : g_szDHEHuntSounds;
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pszSounds[RANDOM_LONG(0, 2)], 0.7, 1.1, 0, 100);
			}

			m_fLastMoveTime = gpGlobals->time;
			return;
		}

		if (pev->sequence == GetRunSeq() || IsWalk(pev->sequence))
			m_fSequenceFinished = true;
	}

	m_vecSudoVel = g_vecZero;
	m_fLastMoveTime = gpGlobals->time;
}

// orig 0x163a20: turn toward the driver's view; a big turn while standing plays the 180 turn
void CDriveHE::DoYaw()
{
	if (!m_bCanTurn)
		return;

	float flSpeed = m_fFrameTime * 1900 * m_fYawSpeedScale;
	bool bFastTurn = false;

	float flDiff = UTIL_AngleDistance(pev->ideal_yaw, pev->angles.y);
	float flAbsDiff = fabs(flDiff);
	if (flAbsDiff < 0.001f)
		return;

	if (70 < flAbsDiff && m_bCanFastTurn && (pev->flags & FL_ONGROUND) != 0)
	{
		flSpeed = m_fFrameTime * 2800 * m_fYawSpeedScale;
		bFastTurn = true;
	}

	if (bFastTurn && pev->sequence == LookupSequence("idle2"))
	{
		// the original adds the time twice
		m_fNextIdleAnim = gpGlobals->time + gpGlobals->time + RANDOM_FLOAT(3, 8);
		pev->sequence = LookupSequence(0 < flDiff ? "180l" : "180r");
		ResetSequenceInfo();
		pev->frame = 0;
		m_bCanIdle = false;
	}

	ChangeYaw((int)flSpeed);
}

// orig 0x163bb0: blink now and then
void CDriveHE::DoEye()
{
	if (m_bCanBlink)
	{
		if (pev->skin < 1)
		{
			if (!(m_fNextBlink < gpGlobals->time))
				return;
			pev->skin = 2;
		}
		else
		{
			pev->skin--;
		}
	}

	m_fNextBlink = RANDOM_FLOAT(2, 6) + gpGlobals->time;
}

// orig 0x163c40: our health, as a percentage, is the driver's HUD health
void CDriveHE::UpdateOwnerHUD()
{
	if (!m_pOwner || pev->health == (float)m_iClientHealth)
		return;

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

// orig 0x163d60: play one of the animations, picked by weight; returns how long to keep it
float CDriveHE::SetRandomAnim(sDHEAnimT* pAnims, int iCount)
{
	float flTotal = 0;
	for (int i = 0; i < iCount; i++)
		flTotal += pAnims[i].flWeight;

	float flPick = RANDOM_FLOAT(0, flTotal);
	int i;
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

// orig 0x163e40
void CDriveHE::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	if (m_bInEyeAttack)
		return;

	switch (pEvent->event)
	{
	case 1:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_szDHEHuntSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, 100);
		break;

	case 2:
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_szDHEAttackSounds[RANDOM_LONG(0, 1)], 1.0, 0.8, 0, 100);
		break;

	case 3: // the sonic blast
		if (m_bInSonicAttack)
			DoSonicAttackBlast();
		break;

	case 4:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_pain3.wav", 1.0, 0.8, 0, 100);
		break;

	case 5:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "houndeye/he_pain1.wav", 1.0, 0.8, 0, 100);
		break;

	case 6: // the hop of jumpback
		pev->flags &= ~FL_ONGROUND;
		UTIL_MakeVectors(pev->angles);
		pev->velocity = gpGlobals->v_forward * -PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundHopBackVel", 260);
		pev->velocity.z = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundHopZVel", 260) + pev->velocity.z;
		m_fNextJumpBack = gpGlobals->time + 1.4;
		break;

	case 7:
		if (m_bCanBlink)
			pev->skin = 2;
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

// orig 0x164170
void WriteDHEBeamColor()
{
	WRITE_BYTE(160);
	WRITE_BYTE(200);
	WRITE_BYTE(255);
}

// orig 0x1641c0: two shock rings, and damage falling off with distance (less through walls)
void CDriveHE::DoSonicAttackBlast()
{
	float flRange = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundSonicRange", 460);
	float flDamage = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundSonicDamage", 65);
	float flWallFraction = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundSonicWallFraction", 0.6);

	Vector vecSrc = pev->origin + Vector(0, 0, 16);

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_szDHEBlastSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, 100);

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z + flRange / 0.2);
	WRITE_SHORT(m_iSpriteTexture);
	WRITE_BYTE(0);	// startframe
	WRITE_BYTE(0);	// framerate
	WRITE_BYTE(2);	// life
	WRITE_BYTE(16); // width
	WRITE_BYTE(0);	// noise
	WriteDHEBeamColor();
	WRITE_BYTE(255); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z + flRange * 0.5 / 0.2);
	WRITE_SHORT(m_iSpriteTexture);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(2);
	WRITE_BYTE(16);
	WRITE_BYTE(0);
	WriteDHEBeamColor();
	WRITE_BYTE(255);
	WRITE_BYTE(0);
	MESSAGE_END();

	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, flRange)) != NULL)
	{
		if (pEntity->pev->takedamage == DAMAGE_NO || pEntity->pev == pev)
			continue;
		// houndeyes don't hurt houndeyes
		if (!strcmp(STRING(pEntity->pev->classname), "monster_houndeye") || !strcmp(STRING(pEntity->pev->classname), "drive_houndeye"))
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, pEntity->Center(), ignore_monsters, ENT(pev), &tr);

		float flDist = (pEntity->Center() - vecSrc).Length();
		if (flDist < flRange)
			flDist = (pEntity->Center() - vecSrc).Length();
		else
			flDist = flRange;

		float flAdjusted = (1.0 - flDist / flRange) * 0.7 + 0.3;
		if (tr.flFraction >= 1.0 || tr.pHit == pEntity->edict())
			flAdjusted = flDamage * flAdjusted;
		else
			flAdjusted = flDamage * flAdjusted * flWallFraction;

		if (0 < flAdjusted)
		{
			pev->classname = MAKE_STRING("houndeye_sonic_blast");
			pEntity->TakeDamage(pev, m_pOwner ? m_pOwner->pev : pev, flAdjusted, DMG_SONIC | DMG_ALWAYSGIB);
			pev->classname = MAKE_STRING("drive_houndeye");
		}
	}
}

// orig 0x164830: drop the charge: the eye flare, the sucked-in sparks, the glow and the sound
void CDriveHE::EyeAttackStop()
{
	if (!m_bInEyeAttack)
		return;

	m_bInEyeAttack = false;

	if (m_pEyeSprite && m_pEyeSprite->pev && m_pEyeSprite->pev->pContainingEntity && UTIL_IsValidEntity(m_pEyeSprite->pev->pContainingEntity))
	{
		m_pEyeSprite->pev->effects |= EF_NODRAW;
		m_pEyeSprite->SetThink(&CBaseEntity::SUB_Remove);
		m_pEyeSprite->pev->nextthink = gpGlobals->time + 0.1;
		m_pEyeSprite = NULL;
	}
	m_pEyeSprite = NULL;

	CBaseEntity* pFlare = NULL;
	while ((pFlare = UTIL_FindEntityByClassname(pFlare, "he_charge_flare")) != NULL)
	{
		edict_t* pent = pFlare->pev->pContainingEntity;
		if (pent && UTIL_IsValidEntity(pent) && pFlare->pev->owner == ENT(pev))
		{
			pFlare->pev->nextthink = gpGlobals->time + 0.1;
			pFlare->SetThink(&CBaseEntity::SUB_Remove);
		}
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/egon_run3.wav", 0, 0, SND_STOP, 100);
	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxNone;
	pev->renderamt = 0;
	pev->rendercolor = g_vecZero;
}

// orig 0x1649d0: each frame of the eye charge, and the beam when it's done charging
void CDriveHE::DoEyeAttackFrame()
{
	if (!m_bInEyeAttack)
		return;

	if (m_fEyeAttackFireTime < gpGlobals->time)
	{
		// fire
		m_fEyeAttackRefire = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttackRefire", 5) + gpGlobals->time;
		EyeAttackStop();
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/beamstart10.wav", 1.0, 0.8, 0, 110);
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart9.wav", 1.0, 0.8, 0, 110);

		UTIL_MakeVectors(pev->angles);
		pev->classname = MAKE_STRING("houndeye_eye_beam");
		Vector vecEye = pev->origin + gpGlobals->v_forward * 30 + gpGlobals->v_up * 28;
		FireDHEEyeBeam(pev, m_pOwner ? m_pOwner->pev : pev, vecEye, gpGlobals->v_forward);
		pev->classname = MAKE_STRING("drive_houndeye");

		// the kick
		UTIL_MakeVectors(pev->angles);
		pev->flags &= ~FL_ONGROUND;
		pev->velocity = gpGlobals->v_forward * -PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttackKick", 640);
		pev->velocity.z = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttackVertKick", 200) + pev->velocity.z;
		m_fNextJumpBack = gpGlobals->time + 1.4;
		m_fLastAttack = gpGlobals->time;
		pev->frame = 0;
		m_fSequenceFinished = false;
		return;
	}

	m_bCanIdle = false;
	m_bCanMove = false;
	m_bCanTurn = true;
	m_bCanFastTurn = false;
	m_bCanBlink = false;
	m_bCanJumpBack = false;
	m_bCanSleep = false;
	m_bCanEyeAttack = false;
	m_fYawSpeedScale = 0.4;

	if (pev->frame >= 255 || m_fSequenceFinished)
	{
		pev->frame = 0;
		m_fSequenceFinished = false;
	}

	float flTimeLeft = m_fEyeAttackFireTime - gpGlobals->time;
	float flLeft = flTimeLeft / PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttackChargeTime", 3.5); // 1 down to 0
	float flCharge = 1.0 - flLeft;																		  // 0 up to 1

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/egon_run3.wav", flCharge * 0.4 + 0.6, 0.8, SND_CHANGE_VOL | SND_CHANGE_PITCH, (int)((flCharge * 0.6 + 0.6) * 100));

	UTIL_MakeVectors(pev->angles);
	Vector vecEye = pev->origin + gpGlobals->v_forward * 30 + gpGlobals->v_up * 28;

	float flFlare = flCharge * 1.33;
	if (flFlare < 0.01)
		flFlare = 0.01;
	else if (1.0 < flFlare)
		flFlare = 1.0;

	if (m_pEyeSprite && m_pEyeSprite->pev && m_pEyeSprite->pev->pContainingEntity && UTIL_IsValidEntity(m_pEyeSprite->pev->pContainingEntity))
		m_pEyeSprite->pev->renderamt = (flFlare * 0.8 + 0.1) * 240;

	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxGlowShell;
	pev->renderamt = flCharge * 18 + 26;
	pev->rendercolor = Vector(0, flCharge * 230 + 20, flCharge * 64 + 16);

	if (CVAR_GET_FLOAT("sv_lowlag") == 1)
		return;

	float flLight = 0.2 + 0.8 * flCharge;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecEye);
	WRITE_BYTE(TE_ELIGHT);
	WRITE_SHORT(ENTINDEX(ENT(pev)));
	WRITE_COORD(vecEye.x);
	WRITE_COORD(vecEye.y);
	WRITE_COORD(vecEye.z);
	WRITE_COORD(flLight * RANDOM_FLOAT(80, 128)); // radius
	WRITE_BYTE(32);
	WRITE_BYTE(255);
	WRITE_BYTE(64);
	WRITE_BYTE(2);	 // life
	WRITE_COORD(0); // decay
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecEye);
	WRITE_BYTE(TE_DLIGHT);
	WRITE_COORD(vecEye.x);
	WRITE_COORD(vecEye.y);
	WRITE_COORD(vecEye.z);
	WRITE_BYTE((int)(flLight * RANDOM_FLOAT(16, 20))); // radius
	WRITE_BYTE(32);
	WRITE_BYTE(255);
	WRITE_BYTE(64);
	WRITE_BYTE(2);	// life
	WRITE_BYTE(50); // decay
	MESSAGE_END();

	// sparks drawn in from around the eye
	int iSparks = RANDOM_LONG(1, 3);
	for (int i = 0; i < iSparks; i++)
	{
		if (!(RANDOM_FLOAT(0, flLeft * 0.75) < 0.074))
			continue;

		float flLife = 1.2 * (0.4 + 0.6 * flLeft) + RANDOM_FLOAT(-0.2, 0.2);
		if (flTimeLeft < flLife)
			flLife = flTimeLeft;
		if (flLife < 0.05)
			continue;

		float flBright = 220 * (0.5 * flCharge + 0.5) + RANDOM_FLOAT(-20, 20);
		if (255 < flBright)
			flBright = 255;

		float flPitch = RANDOM_FLOAT(-60, 20);
		float flYaw = RANDOM_FLOAT(-70, 70);
		Vector vecAngles = pev->angles + Vector(flPitch, flYaw, 0);
		for (int j = 0; j < 3; j++)
		{
			while (vecAngles[j] < 0)
				vecAngles[j] += 360;
			while (360 < vecAngles[j])
				vecAngles[j] -= 360;
		}

		UTIL_MakeVectors(vecAngles);
		Vector vecSpark = vecEye + gpGlobals->v_forward * RANDOM_FLOAT(80, 180);
		CDHESuckingSprite::CreateSS(pev, vecSpark, flBright, flLife);
	}

	float flImplode = flLeft * 0.9;
	if (flImplode < 0.1)
		flImplode = 0.1;
	else if (0.9 < flImplode)
		flImplode = 0.9;

	if (RANDOM_FLOAT(0, flImplode) < 0.08)
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecEye);
		WRITE_BYTE(TE_IMPLOSION);
		WRITE_COORD(vecEye.x);
		WRITE_COORD(vecEye.y);
		WRITE_COORD(vecEye.z);
		WRITE_BYTE((int)((flCharge * 0.4 + 0.6) * 120));	  // radius
		WRITE_BYTE((int)((flCharge * 0.3 + 0.7) * 30));	  // count
		WRITE_BYTE((int)((0.2 + flLeft * 0.8) * 0.8 * 10)); // life
		MESSAGE_END();
	}
}

// orig 0x1657c0
void CDriveHE::DyingThink()
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

// orig 0x165880
void CDriveHE::SetDeathAnim()
{
	if (pev->health < -25 || RANDOM_FLOAT(0, 1) < 0.1)
	{
		pev->sequence = LookupSequence("die_crumple");
	}
	else
	{
		switch (RANDOM_LONG(0, 3))
		{
		case 0:
			pev->sequence = LookupSequence("die");
			break;
		case 1:
			pev->sequence = LookupSequence("die1");
			break;
		case 2:
			pev->sequence = LookupSequence("die2");
			break;
		case 3:
			pev->sequence = LookupSequence("die3");
			break;
		}
	}
	ResetSequenceInfo();
	pev->frame = 0;
}

// orig 0x165980
bool CDriveHE::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_pOwner)
	{
		// whoever can't hurt the driver can't hurt us, nor can anyone hurt a god-mode driver
		if (CBaseEntity::Instance(pevAttacker->pContainingEntity) && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, CBaseEntity::Instance(pevAttacker->pContainingEntity)))
			return false;
		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	// immune to sonic blasts and to the eye beam
	if ((bitsDamageType & DMG_SONIC) != 0 || (bitsDamageType & DMG_RADIATION) != 0)
		return false;

	if (m_fNextPainSound < gpGlobals->time && flDamage < pev->health)
	{
		m_fNextPainSound = RANDOM_FLOAT(0.6, 1.2) + gpGlobals->time;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_szDHEPainSounds[RANDOM_LONG(0, 3)], 1.0, 0.8, 0, 100);
	}

	if (m_bSleeping)
	{
		// woken up
		m_bWantSleep = false;
		if (pev->sequence != LookupSequence("sleeptostand") && pev->sequence != LookupSequence("wakefast"))
		{
			pev->sequence = LookupSequence("wakefast");
			ResetSequenceInfo();
			pev->frame = 0;
		}
	}

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x165bd0: the driver dies with us
void CDriveHE::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->deadflag != DEAD_NO)
		return;

	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	EyeAttackStop();

	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 0, 0, 0, 100);

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
	// VERIFY: guard, the original crashes here: it calls through Instance()'s result unchecked
	// (NULL once the pickup's edict is freed)
	if (pev->owner && CBaseEntity::Instance(pev->owner))
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	pev->classname = MAKE_STRING("houndeye");
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
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_szDHEDieSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, 100);
		SetDeathAnim();
		SetThink(&CDriveHE::DyingThink);
		pev->nextthink = gpGlobals->time + 0.1;
		pev->dmgtime = gpGlobals->time + 5.0;
	}
}

// orig 0x165f60: steer from the driver's view, and put the driver's camera behind us
void CDriveHE::DEClientPreThink()
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
	UTIL_TraceLine(vecOrg, vecOrg - gpGlobals->v_forward * 120, ignore_monsters, ENT(pev), &tr);

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
	if (1.0 <= flAlpha || !(0 < flAlpha))
	{
		m_pOwner->pev->euser2 = ENT(pev);
		m_pOwner->pev->iuser3 = 16;
		m_pOwner->pev->fuser1 = 0;
	}
	else
	{
		m_pOwner->pev->euser2 = ENT(pev);
		m_pOwner->pev->iuser3 = 20;
		m_pOwner->pev->fuser1 = 2.0;

		float flAmt = m_pOwner->m_fDrvEntAlpha * 255;
		if (flAmt < 0)
			flAmt = 0;
		else if (255 < flAmt)
			flAmt = 255;
		m_pOwner->pev->fuser2 = flAmt;
		m_pOwner->pev->fuser3 = 0;
		m_pOwner->pev->vuser1 = g_vecZero;
	}
}

// orig 0x1664c0
void CDriveHE::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x1664e0: the player climbs in and becomes a camera
void CDriveHE::DEClientEnter(CBasePlayer* pPlayer)
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

	// the driver's movement keys would move the camera
	CLIENT_COMMAND(m_pOwner->pev->pContainingEntity, "cl_forwardspeed 0\ncl_backspeed 0\ncl_sidespeed 0\n");
}

// orig 0x166890: give the player back their body
void CDriveHE::DEClientLeave()
{
	if (!m_pOwner || !m_pOwner->pev || !m_pOwner->pev->pContainingEntity || m_pOwner->pev->pContainingEntity->free || !m_pOwner->m_bInDriveEnt)
		return;

	CLIENT_COMMAND(m_pOwner->pev->pContainingEntity, "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");
	EyeAttackStop();

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

// orig 0x166ba0: the eject command: jump out, and the houndeye gibs
void CDriveHE::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVEHOUNDEYE", "AllowEject", false))
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

// orig 0x16a450
void CDriveHE::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;
	if (m_pEyeSprite && m_pEyeSprite->pev && m_pEyeSprite->pev->pContainingEntity == pEdict)
		m_pEyeSprite = NULL;

	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

// orig 0x16a4f0
int CDriveHE::GetRCCaps()
{
	return 0x7fe3dff;
}

// orig 0x16a500
int CDriveHE::GetGuassDistOut()
{
	return 56;
}

// orig 0x16a510
int CDriveHE::GetGuassHull()
{
	return human_hull;
}

// orig 0x16a520
Vector CDriveHE::GetCamRotOrg()
{
	return pev->origin + Vector(0, 0, 32);
}

// orig 0x16a580
int CDriveHE::BloodColor()
{
	return BLOOD_COLOR_YELLOW;
}

// orig 0x16a590
int CDriveHE::Classify()
{
	return CLASS_ALIEN_MONSTER;
}

//=========================================================
// CDHESuckingSprite
//=========================================================

// orig 0x166db0
void CDHESuckingSprite::Spawn()
{
	pev->classname = MAKE_STRING("he_charge_flare");
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;
	pev->renderamt = 16;
	pev->rendermode = kRenderTransAdd;
	pev->renderfx = kRenderFxNone;
	pev->scale = 0.4 + RANDOM_FLOAT(-0.03, 0.03);
	SET_MODEL(ENT(pev), "sprites/heeyeflare2.spr");
	UTIL_SetOrigin(pev, pev->origin);
	pev->frame = 0;
	pev->speed = 0;
	SetThink(&CDHESuckingSprite::SSThink);
	pev->animtime = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x166ee0
void CDHESuckingSprite::Precache()
{
	PRECACHE_MODEL("sprites/heeyeflare2.spr");
}

// orig 0x166f10: brightens up to flMaxBrightness (kept in air_finished) and dies after flLife
// (kept in dmg)
CDHESuckingSprite* CDHESuckingSprite::CreateSS(entvars_t* pevOwner, Vector vecOrigin, float flMaxBrightness, float flLife)
{
	CDHESuckingSprite* pSprite = GetClassPtr((CDHESuckingSprite*)NULL);
	pSprite->Spawn();
	pSprite->pev->owner = pevOwner->pContainingEntity;
	pSprite->pev->air_finished = flMaxBrightness;
	pSprite->pev->dmg = flLife;
	pSprite->pev->dmgtime = gpGlobals->time + pSprite->pev->dmg;
	UTIL_SetOrigin(pSprite->pev, vecOrigin);
	return pSprite;
}

// orig 0x166f90: fly into the houndeye's eye, speeding up
void CDHESuckingSprite::SSThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime < gpGlobals->time || !pev->owner || !UTIL_IsValidEntity(pev->owner))
	{
		pev->effects |= EF_NODRAW;
		pev->nextthink = gpGlobals->time + 0.2;
		SetThink(&CBaseEntity::SUB_Remove);
		return;
	}

	float flInterval = gpGlobals->time - pev->animtime;
	float flDieTime = pev->dmgtime;
	float flNow = gpGlobals->time;
	pev->animtime = gpGlobals->time;

	pev->speed = flInterval * 600 + pev->speed;
	if (800 < pev->speed)
		pev->speed = 800;

	if (pev->renderamt < pev->air_finished)
		pev->renderamt = flInterval * 300 + pev->renderamt;
	if (pev->air_finished < pev->renderamt)
		pev->renderamt = pev->air_finished;

	entvars_t* pevOwner = VARS(pev->owner);
	UTIL_MakeVectors(pevOwner->angles);
	Vector vecToEye = pevOwner->origin + gpGlobals->v_forward * 30 + gpGlobals->v_up * 28 - pev->origin;
	float flDist = vecToEye.Length();

	if (flDist < 2)
	{
		// there
		pev->effects |= EF_NODRAW;
		SetThink(&CBaseEntity::SUB_Remove);
		return;
	}

	// as fast as we are going, or fast enough to get there by the end of our life
	float flSpeed = flDist / (flDieTime - flNow);
	if (pev->speed < flSpeed)
		flSpeed = pev->speed;
	pev->velocity = vecToEye.Normalize() * flSpeed;
}

//=========================================================
// CDHEEyeSprite
//=========================================================

// orig 0x167310
void CDHEEyeSprite::Spawn()
{
	pev->classname = MAKE_STRING("he_eye_flare");
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->renderamt = 32;
	pev->rendermode = kRenderGlow;
	pev->renderfx = kRenderFxNoDissipation;
	pev->scale = 0.7;
	SET_MODEL(ENT(pev), "sprites/heeyeflare4.spr");
	UTIL_SetOrigin(pev, pev->origin);
	pev->frame = 0;
	pev->speed = 0;
	SetThink(&CDHEEyeSprite::ESThink);
	pev->nextthink = gpGlobals->time + 0.01;
}

// orig 0x167410
void CDHEEyeSprite::Precache()
{
	PRECACHE_MODEL("sprites/heeyeflare4.spr");
}

// orig 0x167440
CDHEEyeSprite* CDHEEyeSprite::CreateES(entvars_t* pevOwner)
{
	CDHEEyeSprite* pSprite = GetClassPtr((CDHEEyeSprite*)NULL);
	pSprite->Spawn();
	pSprite->pev->owner = pevOwner->pContainingEntity;
	return pSprite;
}

// orig 0x167490: sit on the houndeye's eye
void CDHEEyeSprite::ESThink()
{
	pev->nextthink = 0.01 + gpGlobals->time;

	// VERIFY: guard, the original crashes here: no check for a missing owner (the houndeye
	// removes us first)
	entvars_t* pevOwner = VARS(pev->owner);
	if (!pevOwner)
		return;
	UTIL_MakeVectors(pevOwner->angles);
	UTIL_SetOrigin(pev, pevOwner->origin + gpGlobals->v_forward * 30 + gpGlobals->v_up * 28);
}

//=========================================================
// The eye beam
//=========================================================

// orig 0x167590
void ResetItteredEnts()
{
	for (int i = 0; i < DHE_MAX_ITTERED; i++)
	{
		g_sDHEItteredEnts[i].iEntIndex = -1;
		g_sDHEItteredEnts[i].flFadeTime = 0;
		g_sDHEItteredEnts[i].flFadeHold = 0;
		g_sDHEItteredEnts[i].flDamage = 0;
	}
}

// orig 0x167610: hurt and flash everything the beam hit, emptying the list
void DoItterOnAll(entvars_t* pevInflictor, entvars_t* pevAttacker)
{
	for (int i = 0; i < DHE_MAX_ITTERED; i++)
	{
		sDHEItteredEntT& ent = g_sDHEItteredEnts[i];
		if (ent.iEntIndex < 1)
		{
			ent.iEntIndex = -1;
			continue;
		}

		edict_t* pent = INDEXENT(ent.iEntIndex);
		if (!pent || !UTIL_IsValidEntity(pent))
		{
			ent.iEntIndex = -1;
			continue;
		}

		CBaseEntity* pEntity = CBaseEntity::Instance(pent);
		ent.iEntIndex = -1;
		if (!pEntity || pEntity->pev == pevInflictor || pEntity->pev == pevAttacker)
			continue;

		if (pEntity->IsNetClient())
			UTIL_ScreenFade(pEntity, Vector(32, 255, 64), ent.flFadeTime, ent.flFadeHold, 240, 0);

		if (pEntity->pev->takedamage != DAMAGE_NO)
			pEntity->TakeDamage(pevInflictor, pevAttacker, ent.flDamage, DMG_ENERGYBEAM | DMG_RADIATION);
	}
}

// orig 0x1677c0: true when the entity is already down for at least this much damage
bool CheckIfItteredOn(edict_t* pent, float flDamage)
{
	int iIndex = ENTINDEX(pent);
	for (int i = 0; i < DHE_MAX_ITTERED; i++)
	{
		if (0 < g_sDHEItteredEnts[i].iEntIndex && g_sDHEItteredEnts[i].iEntIndex == iIndex)
			return flDamage <= g_sDHEItteredEnts[i].flDamage;
	}
	return false;
}

// orig 0x167850: put the entity down for this damage, unless it already has as much
void AddEntToItterList(edict_t* pent, float flDamage, float flFadeTime, float flFadeHold)
{
	int iIndex = ENTINDEX(pent);
	int iSlot = -1;

	for (int i = 0; i < DHE_MAX_ITTERED; i++)
	{
		if (g_sDHEItteredEnts[i].iEntIndex < 1)
		{
			if (iSlot < 0)
				iSlot = i;
		}
		else if (g_sDHEItteredEnts[i].iEntIndex == iIndex)
		{
			iSlot = i;
			if (flDamage <= g_sDHEItteredEnts[i].flDamage)
				return;
			break;
		}
	}

	if ((unsigned int)iSlot < DHE_MAX_ITTERED)
	{
		g_sDHEItteredEnts[iSlot].iEntIndex = iIndex;
		g_sDHEItteredEnts[iSlot].flDamage = flDamage;
		g_sDHEItteredEnts[iSlot].flFadeTime = flFadeTime;
		g_sDHEItteredEnts[iSlot].flFadeHold = flFadeHold;
	}
	else
	{
		ALERT(at_console, "Ent itter list is full, cant add %s for %f damage\n", STRING(pent->v.classname), (double)flDamage);
	}
}

// orig 0x169780: the squared distance of vecTest from the line vecPt1-vecPt2, or -1 when it's
// outside the cylinder of that length and radius around it
float TestPointInCycl(Vector vecPt1, Vector vecPt2, float flLengthSq, float flRadiusSq, Vector vecTest)
{
	float dx = vecPt2.x - vecPt1.x;
	float dy = vecPt2.y - vecPt1.y;
	float dz = vecPt2.z - vecPt1.z;

	float pdx = vecTest.x - vecPt1.x;
	float pdy = vecTest.y - vecPt1.y;
	float pdz = vecTest.z - vecPt1.z;

	float flDot = pdx * dx + pdy * dy + pdz * dz;
	if (flDot < 0 || flLengthSq < flDot)
		return -1;

	float flDistSq = (pdx * pdx + pdy * pdy + pdz * pdz) - flDot * flDot / flLengthSq;
	if (flRadiusSq < flDistSq)
		return -1;
	return flDistSq;
}

// orig 0x167970: damage everything inside the beam's four nested cylinders, most in the inner one
void DoBeamCylDamage(Vector vecStart, Vector vecEnd)
{
	float flRadius4 = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttack4thRadius", 240);
	float flRadius1 = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttack1stRadius", 72);
	float flRadius2 = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttack2ndRadius", 120);
	float flRadius3 = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttack3rdRadius", 160);
	float flDamage1 = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttack1stDamage", 6000);
	float flDamage2 = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttack2ndDamage", 250);
	float flDamage3 = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttack3rdDamage", 70);
	float flDamage4 = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttack4thDamage", 30);

	float flLength = (vecEnd - vecStart).Length();
	float flRadiusSq = flRadius4 * flRadius4;
	float flLengthSq = flLength * flLength;
	// the inner bounds are the 1st radius times each radius, not each radius squared, as in the
	// original
	float flBound1 = flRadius1 * flRadius1;
	float flBound2 = flRadius1 * flRadius2;
	float flBound3 = flRadius1 * flRadius3;

	if (flLength < 8 || flRadius4 < 16)
		return;

	edict_t* pEdict = INDEXENT(1);
	if (!pEdict)
		return;

	for (int i = 1; i < gpGlobals->maxEntities; i++, pEdict++)
	{
		if (pEdict->free || !UTIL_IsValidEntity(pEdict))
			continue;

		CBaseEntity* pEntity = CBaseEntity::Instance(pEdict);
		if (!pEntity)
			continue;
		if (pEntity->pev->takedamage == DAMAGE_NO && !pEntity->IsNetClient())
			continue;

		// its center; if that's out or farther from the line than its feet, its feet, then its head
		float flDist = TestPointInCycl(vecStart, vecEnd, flLengthSq, flRadiusSq, pEntity->Center());

		Vector vecPoint = pEntity->Center();
		vecPoint.z = pEntity->pev->absmin.z + 4;
		float flTest = TestPointInCycl(vecStart, vecEnd, flLengthSq, flRadiusSq, vecPoint);
		if (flDist < 0 || flDist == -1 || (flTest < flDist && 0 <= flTest))
		{
			flDist = flTest;

			vecPoint = pEntity->Center();
			vecPoint.z = pEntity->pev->absmax.z - 4;
			flTest = TestPointInCycl(vecStart, vecEnd, flLengthSq, flRadiusSq, vecPoint);
			if (flDist < 0 || flDist == -1 || (flTest < flDist && 0 <= flTest))
				flDist = flTest;
		}

		if (flDist < 0 || flDist == -1)
			continue;

		if (flDist <= flBound1)
			AddEntToItterList(pEntity->edict(), flDamage1, 2.0, 0.8);
		else if (flDist <= flBound2)
			AddEntToItterList(pEntity->edict(), flDamage2, 1.6, 0.4);
		else if (flDist <= flBound3)
			AddEntToItterList(pEntity->edict(), flDamage3, 1.2, 0.3);
		else
			AddEntToItterList(pEntity->edict(), flDamage4, 0.8, 0.2);
	}
}

// orig 0x167ea0: along the beam, every 64 units, a little damage to anything that can see it
void DoBeamPVSItter(Vector vecStart, Vector vecEnd)
{
	Vector vecPos = vecStart;
	Vector vecDir = (vecEnd - vecStart).Normalize();
	float flDamage = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttackPVSDamage", 10);
	int iSteps = 1024;
	edict_t* pFirst = INDEXENT(1);

	while (0.1 < (vecEnd - vecPos).Length() && 0 < iSteps)
	{
		iSteps--;

		if (UTIL_PointContents(vecPos) != CONTENTS_SOLID)
		{
			edict_t* pEdict = pFirst;
			for (int i = 1; i < gpGlobals->maxEntities; i++, pEdict++)
			{
				if (pEdict->free || !UTIL_IsValidEntity(pEdict))
					continue;

				CBaseEntity* pEntity = CBaseEntity::Instance(pEdict);
				if (!pEntity)
					continue;
				if (pEntity->pev->takedamage == DAMAGE_NO && !pEntity->IsNetClient())
					continue;
				if (CheckIfItteredOn(pEntity->edict(), flDamage))
					continue;

				TraceResult tr;
				UTIL_TraceLine(vecPos, pEntity->pev->origin + pEntity->pev->view_ofs, ignore_monsters, NULL, &tr);
				if (tr.flFraction >= 1.0 || tr.pHit == pEntity->edict())
					AddEntToItterList(pEntity->edict(), flDamage, 0.6, 0.1);
			}
		}

		float flStep = (vecEnd - vecPos).Length();
		if (64 < flStep)
			flStep = 64;
		vecPos = vecPos + vecDir * flStep;
	}
}

// orig 0x1681e0: a blast where the beam enters or leaves a wall
void DoItterPointExplo(Vector vecPos, bool bEntry)
{
	float flMaxDamage, flRadius;
	if (!bEntry)
	{
		flMaxDamage = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttackWallLeaveMaxDamage", 160);
		flRadius = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttackWallLeaveRadius", 180);
	}
	else
	{
		flMaxDamage = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttackWallEntryMaxDamage", 90);
		flRadius = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttackWallEntryRadius", 128);
	}

	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecPos, flRadius)) != NULL)
	{
		if (pEntity->pev->takedamage == DAMAGE_NO && !pEntity->IsNetClient())
			continue;

		Vector vecTarget = pEntity->BodyTarget(vecPos);
		float flDist = (vecTarget - vecPos).Length();

		float flFrac;
		if (flDist / flRadius < 0)
			flFrac = 1.0;
		else if (1.0 < flDist / flRadius)
			flFrac = 0;
		else
			flFrac = 1.0 - flDist / flRadius;

		float flDamage = (flFrac * 0.8 + 0.2) * flMaxDamage;
		if (!(0 < flDamage))
			continue;
		if (CheckIfItteredOn(pEntity->edict(), flDamage))
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecPos, vecTarget, ignore_monsters, NULL, &tr);
		if (tr.flFraction >= 1.0 || tr.pHit == pEntity->edict())
		{
			float flFade = flFrac * 0.6 + 0.4;
			AddEntToItterList(pEntity->edict(), flDamage, flFade * 1.4, flFade * 0.35);
		}
	}
}

// orig 0x168450: one straight piece of the beam: draw it, blast both ends, damage along it.
// The first piece shades in over its first 64 units.
void DoEyeBeamLine(Vector vecStart, Vector vecEnd, bool bFirst)
{
	if (!bFirst)
	{
		DHEBeamDrawFader::CreateFader(1.8, false, vecStart, vecEnd);
	}
	else if ((vecEnd - vecStart).Length() <= 64)
	{
		DHEBeamDrawFader::CreateFader(1.8, true, vecStart, vecEnd);
	}
	else
	{
		Vector vecDir = (vecEnd - vecStart).Normalize();
		DHEBeamDrawFader::CreateFader(1.8, true, vecStart, vecStart + vecDir * 64);
		DHEBeamDrawFader::CreateFader(1.8, false, vecStart + vecDir * 64, vecEnd);
	}

	Vector vecDir = (vecEnd - vecStart).Normalize();
	DoItterPointExplo(vecStart + vecDir * 2, bFirst);
	DoItterPointExplo(vecEnd - vecDir * 2, true);
	DoBeamCylDamage(vecStart, vecEnd);
}

// orig 0x168850
void DoDHEBeamThrowGreenBalls(Vector vecPos, Vector vecDir)
{
	if (CVAR_GET_FLOAT("sv_lowlag") == 1)
		return;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecPos);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(vecPos.x);
	WRITE_COORD(vecPos.y);
	WRITE_COORD(vecPos.z);
	WRITE_COORD(vecPos.x + vecDir.x);
	WRITE_COORD(vecPos.y + vecDir.y);
	WRITE_COORD(vecPos.z + vecDir.z);
	WRITE_SHORT(g_iDHEGreenBallSpray);
	WRITE_BYTE(6);	// count
	WRITE_BYTE(4);	// life
	WRITE_BYTE(4);	// scale
	WRITE_BYTE(30); // velocity
	WRITE_BYTE(9);	// randomness
	MESSAGE_END();
}

// orig 0x1689d0
void DoDHEBeamMakeGlow(Vector vecPos)
{
	if (CVAR_GET_FLOAT("sv_lowlag") == 1)
		return;

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecPos);
	WRITE_BYTE(TE_GLOWSPRITE);
	WRITE_COORD(vecPos.x);
	WRITE_COORD(vecPos.y);
	WRITE_COORD(vecPos.z);
	WRITE_SHORT(g_iDHEWallFlare);
	WRITE_BYTE(15);	 // life
	WRITE_BYTE(7);	 // scale
	WRITE_BYTE(135); // brightness
	MESSAGE_END();
}

// orig 0x168af0: an invisible entity flying down the beam with a beam trail behind it
void DoDHEBeamGlobalTracer(Vector vecStart, Vector vecEnd, edict_t* pentOwner, float flSpeed, Vector vecColor, float flBrightness, float flLife, float flWidth)
{
	if (!(0 < flSpeed))
		return;
	if (CVAR_GET_FLOAT("sv_lowlag") == 1)
		return;

	float flDist = (vecEnd - vecStart).Length();

	CBaseEntity* pTracer = GetClassPtr((CBaseEntity*)NULL);
	pTracer->Spawn();
	pTracer->pev->classname = MAKE_STRING("dhe_tracer");
	pTracer->pev->solid = SOLID_NOT;
	pTracer->pev->movetype = MOVETYPE_NOCLIP;
	pTracer->pev->velocity = (vecEnd - vecStart).Normalize() * flSpeed;
	pTracer->pev->owner = pentOwner;
	pTracer->pev->rendermode = kRenderTransColor;
	pTracer->pev->renderamt = 0;
	pTracer->SetThink(&CBaseEntity::SUB_Remove);
	pTracer->pev->nextthink = flDist / flSpeed + gpGlobals->time;
	UTIL_SetOrigin(pTracer->pev, vecStart);
	UTIL_SetSize(pTracer->pev, g_vecZero, g_vecZero);

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(ENTINDEX(pTracer->pev->pContainingEntity));
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE((int)flLife);
	WRITE_BYTE((int)flWidth);
	WRITE_BYTE((int)vecColor.x);
	WRITE_BYTE((int)vecColor.y);
	WRITE_BYTE((int)vecColor.z);
	WRITE_BYTE((int)flBrightness);
	MESSAGE_END();
}

// orig 0x168e70: trace the beam, punching on through up to HoundEyeAttackMaxPunch units of wall
void DoDHEEyeBeamTraces(Vector vecSrc, Vector vecDir, edict_t* pentIgnore)
{
	float flPunch = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundEyeAttackMaxPunch", 200);
	int iWalls = 16;
	Vector vecStart = vecSrc;
	Vector vecEnd = vecStart + vecDir * 8192;
	bool bFirst = true;
	TraceResult tr;

	while (0.1 < flPunch)
	{
		UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, pentIgnore, &tr);

		DoEyeBeamLine(vecStart, tr.vecEndPos, bFirst);
		if (!bFirst)
			DoDHEBeamMakeGlow(vecStart + vecDir);
		DoDHEBeamMakeGlow(tr.vecEndPos - vecDir);
		DoDHEBeamThrowGreenBalls(vecStart, vecDir);
		DoDHEBeamThrowGreenBalls(tr.vecEndPos - vecDir, -vecDir);
		bFirst = false;
		UTIL_DecalTrace(&tr, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));

		if (tr.flFraction >= 1.0)
			break;

		// find the far side of the wall, tracing back from as deep as we can punch
		TraceResult trOut;
		UTIL_TraceLine(tr.vecEndPos + vecDir * flPunch, tr.vecEndPos + vecDir, ignore_monsters, pentIgnore, &trOut);
		if (trOut.fAllSolid)
			break;

		UTIL_DecalTrace(&trOut, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));
		vecStart = trOut.vecEndPos + vecDir;
		flPunch -= (trOut.vecEndPos - tr.vecEndPos).Length();

		if (--iWalls <= 0)
			break;
	}

	if (300 < (tr.vecEndPos - vecSrc).Length())
	{
		DoDHEBeamGlobalTracer(vecSrc, tr.vecEndPos, pentIgnore, 2000, Vector(128, 255, 128), 180, 10, 60);
		DoDHEBeamGlobalTracer(vecSrc, tr.vecEndPos, pentIgnore, 1200, Vector(128, 255, 128), 120, 18, 40);
	}

	DoBeamPVSItter(vecSrc, tr.vecEndPos);
}

// orig 0x169450
void FireDHEEyeBeam(entvars_t* pevInflictor, entvars_t* pevAttacker, Vector vecSrc, Vector vecDir)
{
	ResetItteredEnts();
	DoDHEEyeBeamTraces(vecSrc, vecDir, pevInflictor->pContainingEntity);
	DoItterOnAll(pevInflictor, pevAttacker);
	UTIL_ScreenFade(CBaseEntity::Instance(pevAttacker->pContainingEntity), Vector(32, 255, 64), 0.6, 0.2, 240, 0);
}

//=========================================================
// DHEBeamDrawFader
//=========================================================

// orig 0x169530: the life is kept in dmg, the time it ends in dmgtime
DHEBeamDrawFader* DHEBeamDrawFader::CreateFader(float flLife, bool bShadeIn, Vector vecStart, Vector vecEnd)
{
	DHEBeamDrawFader* pFader = GetClassPtr((DHEBeamDrawFader*)NULL);
	pFader->SetThink(&DHEBeamDrawFader::FadeThink);
	pFader->pev->nextthink = gpGlobals->time + 0.1;
	pFader->pev->animtime = gpGlobals->time;
	pFader->pev->dmg = flLife;
	pFader->pev->dmgtime = flLife + gpGlobals->time;

	pFader->m_pBeam = CBeam::BeamCreate(g_pModelNameLaser, 80);
	pFader->m_pBeam->PointsInit(vecStart, vecEnd);
	pFader->m_pBeam->SetBrightness(255);
	pFader->m_pBeam->SetColor(16, 255, 64);
	pFader->m_pBeam->SetNoise(0);
	pFader->m_pBeam->SetScrollRate(32);
	pFader->m_pBeam->pev->origin = vecStart;
	pFader->m_pBeam->pev->angles = vecEnd;
	if (bShadeIn)
		pFader->m_pBeam->SetFlags(BEAM_FSHADEIN);

	return pFader;
}

// orig 0x169680
void DHEBeamDrawFader::FadeThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime < gpGlobals->time)
	{
		m_pBeam->pev->effects |= EF_NODRAW;
		m_pBeam->SetThink(&CBaseEntity::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 0.1;
		SetThink(&CBaseEntity::SUB_Remove);
	}
	else
	{
		m_pBeam->pev->renderamt = (int)((pev->dmgtime - gpGlobals->time) / pev->dmg * 255);
	}
}

//=========================================================
// CDriveHEPickup
//=========================================================

// orig 0x1698f0
void CDriveHEPickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), "models/houndeye.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 36));
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->sequence = LookupSequence("idle2");
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);
	m_fSequenceLoops = true;
	InitBoneControllers();

	SetUse(&CDriveHEPickup::Use);
	SetThink(&CDriveHEPickup::PAnimThink);
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

// orig 0x169b30
void CDriveHEPickup::Precache()
{
	PRECACHE_MODEL("models/houndeye.mdl");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_houndeye");
}

// orig 0x169b80: the first spawn point not being removed (each pickup takes one and removes it)
CBaseEntity* CDriveHEPickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_houndeye_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (!pSpot->pev || !OFFSET(pSpot->pev->pContainingEntity))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x169c00
bool CDriveHEPickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return false;
}

// orig 0x169c10: fade in; and kill the houndeye we gave out once its drive time is up
void CDriveHEPickup::PAnimThink()
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

// orig 0x169db0: a player uses us and climbs into a new drive_houndeye
void CDriveHEPickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
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

	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_houndeye", pev->origin, pev->angles, ENT(pev));
	pPlayer->pev->angles = Vector(0, pev->angles.y, 0);
	pPlayer->pev->v_angle = pPlayer->pev->angles;
	pPlayer->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGiven = pPlayer->m_pCurDriveEnt;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDriveHEPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundMaxDriveTime", 600) + gpGlobals->time;
}

// orig 0x169fb0
void CDriveHEPickup::DoRespawn()
{
	m_bTakeable = true;
	m_pGiven = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);
	SetThink(&CDriveHEPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x16a070: the houndeye we gave out died; come back after a while
void CDriveHEPickup::DeathNotice(entvars_t* pevChild)
{
	if (m_bTakeable)
		return;

	pev->nextthink = PREFS_KeyLoopUpFloat("DRIVEHOUNDEYE", "HoundRespawnDelayAfterDeath", 5) + gpGlobals->time;
	SetThink(&CDriveHEPickup::DoRespawn);
}

// orig 0x16a3a0
int CDriveHEPickup::ObjectCaps()
{
	if (m_bTakeable)
		return FCAP_IMPULSE_USE | FCAP_ACROSS_TRANSITION;
	return FCAP_ACROSS_TRANSITION;
}

// orig 0x16a3c0
int CDriveHEPickup::GetRCCaps()
{
	return 0;
}

// orig 0x16a3d0
bool CDriveHEPickup::IsTeleFraggabe()
{
	return false;
}

// orig 0x16a3e0
int CDriveHEPickup::BloodColor()
{
	return DONT_BLEED;
}

// orig 0x16a3f0
int CDriveHEPickup::Classify()
{
	return CLASS_NONE;
}

//=========================================================
// CDrvHESpawner
//=========================================================

// orig 0x16a0e0
void CreateDrvHESpawner()
{
	if (!g_pGameRules->IsMultiplayer())
		return;
	if (!UTIL_FindEntityByClassname(NULL, "drive_houndeye_spawner"))
		CBaseEntity::Create("drive_houndeye_spawner", g_vecZero, g_vecZero, NULL);
}

// orig 0x16a210
void CDrvHESpawner::DrvAThink()
{
	int iCount = (int)CVAR_GET_FLOAT("mp_drivehoundeye");
	for (int i = 0; i < iCount; i++)
		CBaseEntity::Create("pickup_drive_houndeye", g_vecZero, g_vecZero, NULL);

	UTIL_Remove(this);
}

// orig 0x16a2a0
void CDrvHESpawner::Spawn()
{
	// VERIFY: the original names itself drive_panth_spawner (copied from drivepanth.cpp), so
	// CreateDrvHESpawner never finds it by its own name
	pev->classname = MAKE_STRING("drive_panth_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvHESpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15.0;
}

// orig 0x16a340
int CDrvHESpawner::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}



