/***
*
*	Rocket Crowbar 1.9: driveaslave.cpp. The drivable alien slave (drive_asl), its pickup and
*	spawner, the charge flares its zap draws in, the sapper balls that drain a victim and carry
*	the health back to the slave, and an energy ball nothing in this build fires.
*
*	Reconstructed from rc_i386.so; "orig 0x..." gives each function's file address
*	(reference/funcfile.tsv), "orig +0x..." the original's member offsets.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "effects.h"
#include "gamerules.h"
#include "decals.h"
#include "client.h"
#include "UserMessages.h"
#include "driveent.h"

// Preff.cpp (rc.ini prefs)
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
extern int PREFS_KeyLoopUpInt(const char* szHeading, const char* szKey, int iDefault);
extern bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault);

extern float TEXTURETYPE_PlaySound(TraceResult* ptr, Vector vecSrc, Vector vecEnd, int iBulletType);

void CreateDrvSlaveSpawner();
// thin wrappers so CDriveASlave can reach the static creators of classes declared below it
CBaseEntity* CASLZapChargeSprite_CreateSS(entvars_t* pevOwner, Vector vecOrigin, float flTime, int iAttachment);
CBaseEntity* CASLRechargeBall_CreateRB(entvars_t* pevSlave, entvars_t* pevDriver, entvars_t* pevVictim, float flDelay);

const char* g_pDASLAttackHitSounds[] =
	{
		"zombie/claw_strike1.wav",
		"zombie/claw_strike2.wav",
		"zombie/claw_strike3.wav",
};

const char* g_pDASLAttackMissSounds[] =
	{
		"zombie/claw_miss1.wav",
		"zombie/claw_miss2.wav",
};

const char* g_pDASLPainSounds[] =
	{
		"aslave/slv_pain1.wav",
		"aslave/slv_pain2.wav",
};

const char* g_pDASLDeathSounds[] =
	{
		"aslave/slv_die1.wav",
		"aslave/slv_die2.wav",
};

const char* g_pDASLIdleSounds[] =
	{
		"aslave/slv_word1.wav",
		"aslave/slv_word2.wav",
		"aslave/slv_word5.wav",
		"aslave/slv_word6.wav",
		"aslave/slv_word8.wav",
};

// An idle animation picked by weight; it plays again after a random wait. Name is the original's.
struct sDASLAnimT
{
	const char* pszSequence;
	float flWeight;
	float flMinWait;
	float flMaxWait;
};

sDASLAnimT g_sDASLIdleAnims[] =
	{
		{"idle1", 0.25, 10, 20},
		{"idle2", 0.25, 10, 20},
};

//=========================================================
// CDriveASlave
//=========================================================
class CDriveASlave : public CBaseDriveEnt
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	const char* GetControllerName() override;
	void EXPORT DASLThink();
	void DoZap();
	void DoBallAttack();
	void DoSapAttack();
	void DoClaw();
	void CheckDoor();
	void CheckEHop();
	void RunWaterPhysics();
	void DoMovement();
	void DoYaw();
	void UpdateOwnerHUD();
	CBeam* AllocBeam();
	void ClearBeams();
	void CreateBeamToGeom(int iSide);
	void ChargeupThrust();
	Vector DoZapBeamFire(Vector vecSrc, Vector vecDir, Vector vecRight, Vector vecUp, int iSide, float flDamage);
	void FireZap();
	void EmmitSapBalls();
	void ClawDamage();
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	float SetRandomAnim(sDASLAnimT* pAnims, int iCount);
	void EXPORT DyingThink();
	void SetDeathAnim();
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;

	void DEClientPreThink() override;
	void DEClientPostThink() override;
	void DEClientEnter(CBasePlayer* pPlayer) override;
	void DEClientLeave() override;
	void DEClientEject() override;

	void EntIsBeingRemoved(edict_t* pEdict) override;
	int GetRCCaps() override { return 0x7fe3dff; }								// orig 0x134600
	int GetGuassDistOut() override { return 56; }								// orig 0x134610
	int GetGuassHull() override { return human_hull; }							// orig 0x134620
	Vector GetCamRotOrg() override { return pev->origin + Vector(0, 0, 64); } // orig 0x134630
	int BloodColor() override { return BLOOD_COLOR_YELLOW; }					// orig 0x134690
	int Classify() override { return CLASS_ALIEN_MONSTER; }					// orig 0x1346a0

	// saved (reference/savefields.tsv); m_pOwner (+0x27c) is CBaseDriveEnt's
	float m_fFrameTime;		  // orig +0x280
	Vector m_vecSudoVel;	  // orig +0x284, how fast WALK_MOVE actually moved it (it has no real velocity on the ground)
	float m_fLastMoveTime;	  // orig +0x290
	float m_fNextEHopCheck;	  // orig +0x294
	float m_fNextDoorCheck;	  // orig +0x298
	float m_fNextIdleTime;	  // orig +0x2a0
	float m_fNextPainSound;	  // orig +0x2a4
	float m_fIdleSoundTime;	  // orig +0x2a8
	bool m_bInClaw;			  // orig +0x2b4
	float m_fNextClaw;		  // orig +0x2b8
	bool m_bInZap;			  // orig +0x2bc
	bool m_bWantInZap;		  // orig +0x2bd
	bool m_bFarFutureWishZap; // orig +0x2be
	bool m_bCanStopZap;		  // orig +0x2bf
	float m_fZapEmitSoundTime; // orig +0x2c0
	float m_fZapPower;		  // orig +0x2c4
	float m_fNextZap;		  // orig +0x2c8
	bool m_bInSapAttack;	  // orig +0x2dc
	float m_fSapEmmitNext;	  // orig +0x2e0
	float m_fNextSapAttack;	  // orig +0x2e4
	CBeam* m_pBeams[16];	  // orig +0x2e8
	int m_iBeamCnt;			  // orig +0x328
	CSprite* m_pHandGlow;	  // orig +0x32c

	// unsaved, recovered from code; names are ours. Each think starts with every m_bCan* true and
	// whatever is busy (zap, sap, claw) clears the ones it blocks.
	float m_fYawSpeedScale; // orig +0x29c
	bool m_bCanIdle;		// orig +0x2ac, idle anims and sounds. VERIFY name
	bool m_bCanMove;		// orig +0x2ad. VERIFY name
	bool m_bCanTurn;		// orig +0x2ae. VERIFY name
	bool m_bCanTurnAnim;	// orig +0x2af, a big turn may play the left/right turn anim. VERIFY name
	bool m_bCanZap;			// orig +0x2b0. VERIFY name
	bool m_bCanBall;		// orig +0x2b1, for the ball attack, which is empty. VERIFY name
	bool m_bCanSap;			// orig +0x2b2. VERIFY name
	bool m_bCanClaw;		// orig +0x2b3. VERIFY name
	bool m_bInBallAttack;	// orig +0x2cc, never set (DoBallAttack is empty). VERIFY name
	float m_fNextBallAttack; // orig +0x2d8. VERIFY name
	int m_iLastHUDHealth;	// orig +0x330
};

LINK_ENTITY_TO_CLASS(drive_asl, CDriveASlave); // orig 0x12ab10

TYPEDESCRIPTION CDriveASlave::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveASlave, m_pOwner, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveASlave, m_fFrameTime, FIELD_FLOAT),
		DEFINE_FIELD(CDriveASlave, m_vecSudoVel, FIELD_VECTOR),
		DEFINE_FIELD(CDriveASlave, m_fLastMoveTime, FIELD_TIME),
		DEFINE_FIELD(CDriveASlave, m_fNextEHopCheck, FIELD_TIME),
		DEFINE_FIELD(CDriveASlave, m_fNextDoorCheck, FIELD_TIME),
		DEFINE_FIELD(CDriveASlave, m_fNextIdleTime, FIELD_TIME),
		DEFINE_FIELD(CDriveASlave, m_fNextPainSound, FIELD_TIME),
		DEFINE_FIELD(CDriveASlave, m_fIdleSoundTime, FIELD_TIME),
		DEFINE_FIELD(CDriveASlave, m_bInClaw, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveASlave, m_fNextClaw, FIELD_TIME),
		DEFINE_FIELD(CDriveASlave, m_bInZap, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveASlave, m_bWantInZap, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveASlave, m_bFarFutureWishZap, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveASlave, m_bCanStopZap, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveASlave, m_fZapEmitSoundTime, FIELD_TIME),
		DEFINE_FIELD(CDriveASlave, m_fZapPower, FIELD_FLOAT),
		DEFINE_FIELD(CDriveASlave, m_fNextZap, FIELD_TIME),
		DEFINE_FIELD(CDriveASlave, m_bInSapAttack, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveASlave, m_fSapEmmitNext, FIELD_TIME),
		DEFINE_FIELD(CDriveASlave, m_fNextSapAttack, FIELD_TIME),
		DEFINE_ARRAY(CDriveASlave, m_pBeams, FIELD_CLASSPTR, 16),
		DEFINE_FIELD(CDriveASlave, m_iBeamCnt, FIELD_INTEGER),
		DEFINE_FIELD(CDriveASlave, m_pHandGlow, FIELD_CLASSPTR),
};

IMPLEMENT_SAVERESTORE(CDriveASlave, CBaseMonster); // orig 0x12ab40, 0x12ab90

// orig 0x12abe0
void CDriveASlave::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_asl");
	SET_MODEL(ENT(pev), "models/islave.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->effects = 0;
	pev->friction = 1.0;
	pev->health = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveHealth", 260);
	pev->max_health = pev->health;
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	pev->sequence = LookupSequence("idle3");
	ResetSequenceInfo();
	pev->frame = 0;
	InitBoneControllers();
	pev->framerate = 1.0;
	SetEyePosition();

	m_bInZap = false;
	m_bCanStopZap = true;
	m_fNextZap = gpGlobals->time;
	m_bInClaw = false;
	m_fNextClaw = gpGlobals->time;
	m_bInBallAttack = false;
	m_fNextBallAttack = gpGlobals->time;
	m_iBeamCnt = 0;
	m_fLastMoveTime = gpGlobals->time;
	m_vecSudoVel = g_vecZero;
	m_fNextIdleTime = gpGlobals->time + RANDOM_FLOAT(6, 8);
	m_fIdleSoundTime = gpGlobals->time + RANDOM_FLOAT(6, 10);
	m_fNextPainSound = gpGlobals->time;
	m_fNextEHopCheck = m_fNextDoorCheck = gpGlobals->time;

	m_pHandGlow = CSprite::SpriteCreate("sprites/xspark4.spr", pev->origin, true);
	m_pHandGlow->SetTransparency(kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation);
	m_pHandGlow->SetAttachment(edict(), 1);
	m_pHandGlow->pev->iuser3 = 2; // VERIFY: literal store to the sprite's pev +0x24c (iuser3)
	m_pHandGlow->pev->effects |= EF_NODRAW;
	m_pHandGlow->TurnOff();

	SetThink(&CDriveASlave::DASLThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x12af90
void CDriveASlave::Precache()
{
	PRECACHE_MODEL("models/islave.mdl");
	PRECACHE_MODEL("sprites/lgtning.spr");
	PRECACHE_MODEL("sprites/xspark4.spr");
	PRECACHE_SOUND("debris/zap3.wav");
	PRECACHE_SOUND("debris/zap4.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("weapons/electro5.wav");
	PRECACHE_SOUND("x/x_ballattack1.wav");
	PRECACHE_SOUND("x/x_shoot1.wav");

	for (unsigned int i = 0; i < 3; i++)
		PRECACHE_SOUND(g_pDASLAttackHitSounds[i]);
	for (unsigned int i = 0; i < 2; i++)
		PRECACHE_SOUND(g_pDASLAttackMissSounds[i]);
	for (unsigned int i = 0; i < 2; i++)
		PRECACHE_SOUND(g_pDASLPainSounds[i]);
	for (unsigned int i = 0; i < 2; i++)
		PRECACHE_SOUND(g_pDASLDeathSounds[i]);
	for (unsigned int i = 0; i < 5; i++)
		PRECACHE_SOUND(g_pDASLIdleSounds[i]);

	UTIL_PrecacheOther("asl_charge_flare");
	UTIL_PrecacheOther("alien_slave_energy_ball");
	UTIL_PrecacheOther("alien_slave_sapper");
}

// orig 0x12b120
const char* CDriveASlave::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;

	return STRING(m_pOwner->pev->netname);
}

// orig 0x12b190
void CDriveASlave::DASLThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	bool bNull = !m_pOwner || FNullEnt(m_pOwner->pev);
	if (bNull || !m_pOwner->pev || !m_pOwner->edict() || !m_pOwner->edict()->pvPrivateData || m_pOwner->edict()->free || !UTIL_PlayerByIndex(ENTINDEX(m_pOwner->edict())))
	{
		// the driver is gone
		m_pOwner = NULL;
		pev->classname = MAKE_STRING("alien_slave");
		g_pevLastInflictor = pev;
		Killed(pev, GIB_NORMAL);
		return;
	}

	if (pev->waterlevel > 1)
		pev->flags &= ~FL_ONGROUND;

	DispatchAnimEvents(m_fFrameTime);

	if ((pev->flags & FL_FROZEN) != 0)
	{
		pev->sequence = LookupSequence("idle3");
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
	m_bCanTurnAnim = true;
	m_bCanZap = true;
	m_bCanClaw = true;
	m_bCanBall = true;
	m_bCanSap = true;
	m_fYawSpeedScale = 1.0;

	UpdateOwnerHUD();

	bool bBusy = false;
	if (m_bInBallAttack)
	{
		if (m_fSequenceFinished)
		{
			m_bInBallAttack = false;
			m_fNextBallAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveBallRefire", 1.2);
		}
		else
			bBusy = true;
	}
	else if (m_bInSapAttack)
	{
		if (m_fSequenceFinished)
		{
			m_bInSapAttack = false;
			m_fNextSapAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveSapRefire", 2.0);
		}
		else
			bBusy = true;
	}
	else if (m_bInClaw)
	{
		if (!m_fSequenceFinished)
		{
			m_bCanIdle = false;
			m_bCanMove = false;
			m_bCanTurn = true;
			m_bCanTurnAnim = false;
			m_bCanZap = false;
			m_bCanClaw = false;
			m_bCanBall = false;
			m_bCanSap = false;
			m_fYawSpeedScale = 0.7;
		}
		else
		{
			m_bInClaw = false;
			m_fNextClaw = gpGlobals->time + PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveClawRefire", 0.2);
		}
	}

	if (bBusy)
	{
		m_bCanIdle = false;
		m_bCanMove = false;
		m_bCanTurn = true;
		m_bCanTurnAnim = false;
		m_bCanZap = false;
		m_bCanClaw = false;
		m_bCanBall = false;
		m_bCanSap = false;
		m_fYawSpeedScale = 0.9;
	}

	DoZap();
	DoBallAttack();
	DoSapAttack();
	DoClaw();
	DoMovement();
	DoYaw();

	pev->fuser4 = m_vecSudoVel.Length();

	if (!m_bCanIdle)
	{
		m_fNextIdleTime = gpGlobals->time + RANDOM_FLOAT(3, 7);
		m_fIdleSoundTime = gpGlobals->time + RANDOM_FLOAT(4, 7);
		return;
	}

	if (m_fIdleSoundTime < gpGlobals->time)
	{
		m_fIdleSoundTime = gpGlobals->time + RANDOM_FLOAT(6, 10);
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDASLIdleSounds[RANDOM_LONG(0, 4)], 0.7, 1.1, 0, 100);
	}

	if (!m_fSequenceFinished)
		return;

	if (m_fNextIdleTime < gpGlobals->time)
	{
		m_fNextIdleTime = SetRandomAnim(g_sDASLIdleAnims, 2) + gpGlobals->time;
		return;
	}

	if (pev->sequence == LookupSequence("idle3"))
		return;

	pev->sequence = LookupSequence("idle3");
	ResetSequenceInfo();
	pev->frame = 0;
}

// Primary attack: hold to charge (each anim cycle a ChargeupThrust), release to fire. orig 0x12b6f0
void CDriveASlave::DoZap()
{
	if (m_bInZap)
	{
		m_bCanIdle = false;
		m_bCanMove = false;
		m_bCanTurn = true;
		m_bCanTurnAnim = false;
		m_bCanClaw = false;
		m_bCanBall = false;
		m_fYawSpeedScale = 0.8;

		if (m_fZapEmitSoundTime < gpGlobals->time)
		{
			float flMax = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveZapMaxDamage", 320);
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/zap4.wav", 1.0, 0.8, 0, (int)(130 + 50 * (m_fZapPower / flMax)));
			m_fZapEmitSoundTime = gpGlobals->time + RANDOM_FLOAT(0.7, 0.75);
		}

		if (m_fSequenceFinished || pev->sequence != LookupSequence("zapattack1"))
		{
			pev->sequence = LookupSequence("zapattack1");
			ResetSequenceInfo();
			pev->frame = 0;
		}
		pev->framerate = 1.33;

		m_bFarFutureWishZap = (m_pOwner->pev->button & IN_ATTACK) != 0;
		return;
	}

	if (!m_bCanZap)
		return;
	if ((m_pOwner->pev->button & IN_ATTACK) == 0)
		return;
	if ((pev->flags & FL_ONGROUND) == 0)
		return;
	if (gpGlobals->time < m_fNextZap)
		return;

	m_bCanIdle = false;
	m_bCanMove = false;
	m_bCanTurn = true;
	m_bCanTurnAnim = false;
	m_fYawSpeedScale = 0.8;
	m_bWantInZap = true;
	m_bInZap = true;
	m_bCanStopZap = true;
	m_bCanClaw = false;
	m_bCanBall = false;
	m_bCanSap = false;
	m_fZapEmitSoundTime = gpGlobals->time;

	pev->sequence = LookupSequence("zapattack1");
	ResetSequenceInfo();
	pev->frame = 0;
	m_fZapPower = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveZapStartDamage", 40);

	for (int i = 0; i < 3; i++)
	{
		CreateBeamToGeom(1);
		CreateBeamToGeom(-1);
	}

	UTIL_MakeVectors(pev->angles);
	Vector vecLight = pev->origin + gpGlobals->v_forward * 2;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecLight);
	WRITE_BYTE(TE_DLIGHT);
	WRITE_COORD(vecLight.x);
	WRITE_COORD(vecLight.y);
	WRITE_COORD(vecLight.z);
	WRITE_BYTE(12); // radius
	WRITE_BYTE(255);
	WRITE_BYTE(180);
	WRITE_BYTE(96);
	WRITE_BYTE(20); // life
	WRITE_BYTE(0);	// decay
	MESSAGE_END();
}

// Empty in the original. orig 0x12bae0
void CDriveASlave::DoBallAttack()
{
}

// Jump: drain everything in front of it with sapper balls. orig 0x12bb00
void CDriveASlave::DoSapAttack()
{
	UTIL_SetOrigin(m_pHandGlow->pev, pev->origin);
	m_pHandGlow->SetAttachment(edict(), 1);

	if (m_bInZap)
		return;

	if (!m_bInSapAttack)
	{
		m_pHandGlow->pev->renderamt = UTIL_Approach(0, m_pHandGlow->pev->renderamt, m_fFrameTime * 200);
		if (m_pHandGlow->pev->renderamt < 16)
		{
			m_pHandGlow->pev->renderamt = 0;
			m_pHandGlow->pev->effects |= EF_NODRAW;
			m_pHandGlow->TurnOff();
		}

		if ((m_pOwner->pev->button & IN_JUMP) != 0 && m_bCanSap && m_fNextSapAttack <= gpGlobals->time)
		{
			m_bCanIdle = false;
			m_bCanMove = false;
			m_bCanTurn = true;
			m_bCanTurnAnim = false;
			m_fYawSpeedScale = 0.9;
			m_bCanClaw = false;
			m_bInSapAttack = true;
			m_fSapEmmitNext = gpGlobals->time + 0.7;
			m_pHandGlow->pev->effects &= ~EF_NODRAW;
			m_pHandGlow->pev->renderamt = 16;
			m_pHandGlow->TurnOn();
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "x/x_ballattack1.wav", 1.0, 1.0, 0, RANDOM_LONG(100, 110));
			pev->sequence = LookupSequence("updown");
			ResetSequenceInfo();
			pev->frame = 0;
			pev->framerate = 0.9;
		}
		return;
	}

	m_bCanIdle = false;
	m_bCanMove = false;
	m_bCanTurn = true;
	m_bCanTurnAnim = false;
	m_bCanClaw = false;
	m_fYawSpeedScale = 0.8;

	if (m_fSapEmmitNext < gpGlobals->time && m_fSapEmmitNext != -1)
	{
		EmmitSapBalls();
		m_fSapEmmitNext = -1;
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "x/x_shoot1.wav", 0.8, 1.0, 0, RANDOM_LONG(120, 130));

		Vector vecHand, vecAngles;
		GetAttachment(0, vecHand, vecAngles);
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecHand);
		WRITE_BYTE(TE_IMPLOSION);
		WRITE_COORD(vecHand.x);
		WRITE_COORD(vecHand.y);
		WRITE_COORD(vecHand.z);
		WRITE_BYTE(64); // radius
		WRITE_BYTE(18); // count
		WRITE_BYTE(4);	// life
		MESSAGE_END();
	}

	m_pHandGlow->pev->effects &= ~EF_NODRAW;
	m_pHandGlow->pev->renderamt = UTIL_Approach(180, m_pHandGlow->pev->renderamt, m_fFrameTime * 180);
}

// Secondary attack. orig 0x12bf20
void CDriveASlave::DoClaw()
{
	if (m_bInZap || !m_bCanClaw)
		return;
	if (m_fNextClaw > gpGlobals->time || (m_pOwner->pev->button & IN_ATTACK2) == 0)
		return;

	pev->sequence = LookupSequence("attack1");
	ResetSequenceInfo();
	pev->frame = 0;
	m_bCanIdle = false;
	m_bCanMove = false;
	m_bCanTurn = true;
	m_bCanTurnAnim = false;
	m_fYawSpeedScale = 0.7;
	m_bInClaw = true;
}

// Walk into a door to open it. orig 0x12bfe0
void CDriveASlave::CheckDoor()
{
	if (gpGlobals->time < m_fNextDoorCheck)
		return;
	if (!m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecStart = pev->origin + Vector(0, 0, 16);
	Vector vecEnd = vecStart + gpGlobals->v_forward * (m_fFrameTime * pev->framerate * m_flGroundSpeed);

	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	TraceResult tr;
	UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

	if (tr.pHit && (FClassnameIs(VARS(tr.pHit), "func_door") || FClassnameIs(VARS(tr.pHit), "func_door_rotating")))
	{
		CBaseEntity* pDoor = CBaseEntity::Instance(tr.pHit);
		if (pDoor)
			pDoor->Touch(this);
	}

	m_fNextDoorCheck = gpGlobals->time + 0.3;
}

// Hop off a ledge in front instead of stopping at it. orig 0x12c230
void CDriveASlave::CheckEHop()
{
	if (m_fNextEHopCheck > gpGlobals->time || !m_bCanMove)
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
			pev->velocity.z = pev->velocity.z + 50;
			pev->velocity = pev->velocity + gpGlobals->v_forward * 100;
			pev->flags &= ~FL_ONGROUND;
			UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
		}
	}

	m_fNextEHopCheck = gpGlobals->time + 0.2;
}

// orig 0x12c520
void CDriveASlave::RunWaterPhysics()
{
	float flFriction = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveWaterFriction", 8);

	float flSpeed = pev->velocity.Length();
	if (flSpeed > 0.1)
	{
		float flControl = flSpeed < 6.0 ? 6.0 : flSpeed;
		float flNewSpeed = flSpeed - flControl * flFriction * m_fFrameTime;
		if (flNewSpeed < 0)
			flNewSpeed = 0;
		flNewSpeed = flNewSpeed / flSpeed;
		pev->velocity = pev->velocity * flNewSpeed;
	}

	if (pev->waterlevel > 1)
	{
		// VERIFY: added, not subtracted: under water it rises
		float flGravity = 1.1 * CVAR_GET_FLOAT("sv_gravity");
		if (pev->gravity > 0)
			flGravity = flGravity * pev->gravity;
		pev->velocity.z = flGravity * m_fFrameTime + pev->velocity.z;
	}

	if (!m_bCanMove)
		return;

	if (pev->sequence != LookupSequence("run1"))
	{
		pev->sequence = LookupSequence("run1");
		ResetSequenceInfo();
	}

	float flRate = pev->velocity.Length2D() > 0.1 ? pev->velocity.Length2D() / 70 : 0.3;
	if (flRate >= 0.3)
		pev->framerate = flRate > 1.1 ? 1.1 : flRate;
	else
		pev->framerate = 0.3;

	if ((m_pOwner->pev->button & IN_FORWARD) != 0)
	{
		UTIL_MakeAimVectors(pev->angles);
		float flMaxSpeed = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveWaterSpeed", 190);
		float flAccel = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveWaterAccel", 220);
		Vector vecDir = gpGlobals->v_forward;

		float flAddSpeed = flMaxSpeed - DotProduct(pev->velocity, vecDir);
		if (flAddSpeed > 0)
		{
			float flAccelSpeed = flAccel * m_fFrameTime * flMaxSpeed;
			if (flAddSpeed < flAccelSpeed)
				flAccelSpeed = flAddSpeed;
			pev->velocity = pev->velocity + vecDir * flAccelSpeed;
		}
	}
}

// orig 0x12c880
void CDriveASlave::DoMovement()
{
	if ((pev->flags & FL_ONGROUND) == 0)
	{
		if (pev->waterlevel != 0)
		{
			RunWaterPhysics();
		}
		else if (m_bCanIdle)
		{
			// airborne: VERIFY the original picks walk1 when fast and run1 when slower
			float flSpeed = pev->velocity.Length2D();
			if (flSpeed > 30 && flSpeed < 70)
			{
				if (pev->sequence != LookupSequence("run1"))
				{
					pev->sequence = LookupSequence("run1");
					ResetSequenceInfo();
					pev->frame = 0;
				}
			}
			else if (flSpeed >= 70 && pev->sequence != LookupSequence("walk1"))
			{
				pev->sequence = LookupSequence("walk1");
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
			int iOldSeq = pev->sequence;
			bool bWasWalking = false; // VERIFY: true only for walk1, not run1
			if (pev->sequence != LookupSequence("run1"))
				bWasWalking = pev->sequence == LookupSequence("walk1");

			if (bWalk)
			{
				if (pev->sequence != LookupSequence("walk1"))
				{
					pev->sequence = LookupSequence("walk1");
					ResetSequenceInfo();
					pev->frame = 0;
				}
			}
			else if (pev->sequence != LookupSequence("run1"))
			{
				pev->sequence = LookupSequence("run1");
				ResetSequenceInfo();
				pev->frame = 0;
				pev->framerate = 1.4;
			}

			if (pev->sequence == LookupSequence("run1"))
				pev->framerate = 1.4;
			else
				pev->framerate = 1.0;

			CheckDoor();
			m_bCanIdle = false;

			UTIL_MakeVectors(pev->angles);
			Vector vecGoal = Center() + gpGlobals->v_forward * 512;

			// probe how far it can walk this frame
			float flMoved = 0;
			Vector vecStartOrg = pev->origin;
			float flDist = m_flGroundSpeed * pev->framerate * m_fFrameTime;
			while (flDist > 0.001)
			{
				float flStep = flDist > 16 ? 16 : flDist;
				Vector vecOrg = pev->origin;
				for (; !WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY) && flStep > 2; flStep -= 2)
					;
				UTIL_SetOrigin(pev, vecOrg);

				if (!WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY))
				{
					if (flMoved < 0.01)
					{
						// blocked
						if (bWasWalking)
						{
							pev->sequence = iOldSeq;
							ResetSequenceInfo();
							pev->frame = flOldFrame;
						}
						else
						{
							m_fSequenceFinished = true;
						}
						m_bCanIdle = true;
						m_vecSudoVel = g_vecZero;
						UTIL_SetOrigin(pev, vecStartOrg);
						if (!bWalk)
							CheckEHop();
						return;
					}
					break;
				}

				flDist = flDist - flStep;
				flMoved = flMoved + flStep;
			}

			// then really move that far toward the goal
			UTIL_SetOrigin(pev, vecStartOrg);
			Vector vecOrg = pev->origin;
			float flOldIdealYaw = pev->ideal_yaw;
			pev->ideal_yaw = pev->angles.y;
			while (flMoved > 0.001)
			{
				float flStep = flMoved > 16 ? 16 : flMoved;
				UTIL_MoveToOrigin(ENT(pev), vecGoal, flStep, MOVE_NORMAL);
				flMoved = flMoved - flStep;
			}

			Vector vecDelta = pev->origin - vecOrg;
			if (vecDelta.Length() > 0.01 && m_fFrameTime > 0.001)
				m_vecSudoVel = vecDelta / m_fFrameTime;
			else
				m_vecSudoVel = g_vecZero;

			pev->ideal_yaw = flOldIdealYaw;
			m_bCanTurnAnim = false;
			m_fLastMoveTime = gpGlobals->time;
			return;
		}

		if (pev->sequence == LookupSequence("run1") || pev->sequence == LookupSequence("walk1"))
			m_fSequenceFinished = true;
	}

	m_vecSudoVel = g_vecZero;
	m_fLastMoveTime = gpGlobals->time;
}

// orig 0x12cf40
void CDriveASlave::DoYaw()
{
	if (!m_bCanTurn)
		return;

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

	if (bTurnAnim && pev->sequence == LookupSequence("idle3"))
	{
		m_fNextIdleTime = gpGlobals->time + gpGlobals->time + RANDOM_FLOAT(3, 8); // VERIFY: time is added twice
		pev->sequence = LookupSequence(flDelta > 0 ? "left" : "right");
		ResetSequenceInfo();
		pev->frame = 0;
		m_bCanIdle = false;
	}

	ChangeYaw((int)flSpeed);
}

// orig 0x12d0d0
void CDriveASlave::UpdateOwnerHUD()
{
	if (!m_pOwner)
		return;

	if (pev->health == (float)m_iLastHUDHealth)
		return;

	float flPercent = (pev->health / pev->max_health) * 100;
	int iHealth = flPercent > 0 ? (int)flPercent : 0;
	if (iHealth < 1)
		iHealth = 1;
	else if (iHealth > 100)
		iHealth = 100;

	MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->edict());
	WRITE_BYTE(iHealth);
	MESSAGE_END();

	m_iLastHUDHealth = (int)pev->health;
}

// A new beam while there is room, else a random old one. orig 0x12d1f0
CBeam* CDriveASlave::AllocBeam()
{
	if (m_iBeamCnt < 0)
		m_iBeamCnt = 0;

	if (m_iBeamCnt < 16)
	{
		int i = m_iBeamCnt;
		m_pBeams[i] = CBeam::BeamCreate("sprites/lgtning.spr", 30);
		m_iBeamCnt++;
		return m_pBeams[i];
	}

	return m_pBeams[RANDOM_LONG(0, 15)];
}

// orig 0x12d280
void CDriveASlave::ClearBeams()
{
	for (int i = 0; i < m_iBeamCnt; i++)
	{
		if (m_pBeams[i])
		{
			UTIL_Remove(m_pBeams[i]);
			m_pBeams[i] = NULL;
		}
	}
	m_iBeamCnt = 0;

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/zap4.wav", 0, 0, SND_STOP, 100);
}

// A charge-up arc from one hand (iSide 1 right, -1 left) to the nearest of a few random walls. orig 0x12d320
void CDriveASlave::CreateBeamToGeom(int iSide)
{
	UTIL_MakeVectors(pev->angles);
	Vector vecSrc = pev->origin + gpGlobals->v_up * 36 + gpGlobals->v_forward * 32;
	Vector vecStart = vecSrc + (float)iSide * gpGlobals->v_right * 16;

	TraceResult tr, trBest;
	float flBest = 1.0;
	int iTries = RANDOM_LONG(1, 4);
	for (int i = 0; i < iTries; i++)
	{
		Vector vecDir = g_vecZero;
		float flUp = RANDOM_FLOAT(-0.7, 1.0);
		Vector vecUp = gpGlobals->v_up * flUp;
		Vector vecSide = (float)iSide * gpGlobals->v_right;
		float flSide = RANDOM_FLOAT(0, 1.0);
		vecDir = vecUp + vecSide * flSide;
		float flFwd = RANDOM_FLOAT(0.3, -0.1); // sic, low > high
		vecDir = vecDir + gpGlobals->v_forward * flFwd;

		Vector vecEnd = vecStart + vecDir * RANDOM_FLOAT(480, 512);
		UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction < flBest)
		{
			flBest = tr.flFraction;
			trBest = tr;
		}
	}

	CBeam* pBeam = AllocBeam();
	pBeam->SetWidth(RANDOM_LONG(28, 32));
	pBeam->PointEntInit(pev->origin, ENTINDEX(edict()));
	pBeam->SetEndAttachment(iSide < 0 ? 2 : 1);
	pBeam->SetColor(96, 128, 16);
	pBeam->SetBrightness(200);
	pBeam->SetNoise(80);

	if (flBest < 1.0 && trBest.flFraction < 1.0)
	{
		pBeam->pev->origin = trBest.vecEndPos;
		// VERIFY: the hit test reads the last trace, the decal goes on the best one
		if (tr.pHit)
			UTIL_DecalTrace(&trBest, ::DamageDecal(CBaseEntity::Instance(tr.pHit), DMG_BULLET));
	}
	else
	{
		pBeam->pev->origin = tr.vecEndPos;
	}

	pBeam->SetFlags(0x40); // BEAM_FSHADEIN (the shade-in flag in the beam rendermode bits)
}

// One zap charge cycle: flares drawn into the hands, more arcs, more power. orig 0x12d8b0
void CDriveASlave::ChargeupThrust()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "weapons/electro5.wav", 0.4, 1.1, 0, RANDOM_LONG(120, 140));

	int iFlares = RANDOM_LONG(6, 8);
	UTIL_MakeVectors(pev->angles);
	Vector vecSrc = pev->origin + gpGlobals->v_up * 36 + gpGlobals->v_forward * 32;

	for (int i = 0; i < iFlares; i++)
	{
		float flTime = RANDOM_FLOAT(0.9, 1.4);
		int iHand = RANDOM_LONG(0, 1);
		float flPitch = RANDOM_FLOAT(-60, 20);
		float flYaw = RANDOM_FLOAT(-90, 90);

		Vector vecAngles = pev->angles + Vector(flPitch, flYaw, 0);
		for (int j = 0; j < 3; j++)
		{
			if (vecAngles[j] < 0)
			{
				do
					vecAngles[j] = vecAngles[j] + 360;
				while (vecAngles[j] < 0);
			}
			for (; vecAngles[j] > 360; vecAngles[j] = vecAngles[j] - 360)
				;
		}

		UTIL_MakeVectors(vecAngles);
		Vector vecPos = vecSrc + gpGlobals->v_forward * RANDOM_FLOAT(120, 180);
		CASLZapChargeSprite_CreateSS(pev, vecPos, flTime, iHand);
	}

	CreateBeamToGeom(1);
	CreateBeamToGeom(-1);
	if (RANDOM_FLOAT(0, 1) < 0.1)
		CreateBeamToGeom(1);
	if (RANDOM_FLOAT(0, 1) < 0.1)
		CreateBeamToGeom(-1);

	m_fZapPower = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveZapDamagePerThrust", 40) + m_fZapPower;
	float flMax = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveZapMaxDamage", 320);
	m_bWantInZap = true;
	m_bCanStopZap = false;
	if (m_fZapPower > flMax)
		m_fZapPower = flMax;
}

// One of the two zap bolts; returns where it ended. orig 0x12dce0
Vector CDriveASlave::DoZapBeamFire(Vector vecSrc, Vector vecDir, Vector vecRight, Vector vecUp, int iSide, float flDamage)
{
	Vector vecSide = (float)iSide * vecRight;
	float flSpread = RANDOM_FLOAT(0, 0.01);
	Vector vecAim = vecDir + vecSide * flSpread;
	vecAim = vecAim + vecUp * RANDOM_FLOAT(-0.01, 0.01);

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecAim * 2048, dont_ignore_monsters, ENT(pev), &tr);

	if (tr.pHit && VARS(tr.pHit)->takedamage != DAMAGE_NO)
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		entvars_t* pevAttacker = pev;
		if (m_pOwner)
			pevAttacker = m_pOwner->pev;

		pev->classname = MAKE_STRING("alien_slave_lightning");
		ClearMultiDamage();
		pHit->TraceAttack(pevAttacker, flDamage, gpGlobals->v_forward, &tr, DMG_SHOCK | DMG_ENERGYBEAM);
		ApplyMultiDamage(pev, pevAttacker);
		pev->classname = MAKE_STRING("drive_asl");
	}

	CBeam* pBeam = AllocBeam();
	pBeam->SetWidth(50);
	pBeam->PointEntInit(tr.vecEndPos, ENTINDEX(edict()));
	pBeam->SetEndAttachment(iSide < 0 ? 2 : 1);
	pBeam->SetColor(180, 255, 96);
	pBeam->SetBrightness(255);
	pBeam->SetNoise(20);
	UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));

	return tr.vecEndPos;
}

// Release: two bolts, auto-aimed at a visible target near the crosshair. orig 0x12e020
void CDriveASlave::FireZap()
{
	if (m_fZapPower < 0)
		return;

	float flBestDot = 0.964;
	m_bCanStopZap = false;
	m_bWantInZap = false;
	m_bFarFutureWishZap = false;

	UTIL_MakeVectors(pev->angles);
	Vector vecSrc = pev->origin + gpGlobals->v_up * 36;
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;

	Vector vecAimAngles;
	vecAimAngles.x = -30;
	if (pev->idealpitch >= -30)
	{
		vecAimAngles.x = 30;
		if (!(pev->idealpitch > 30))
			vecAimAngles.x = pev->idealpitch;
	}
	vecAimAngles.y = pev->angles.y;
	vecAimAngles.z = 0;
	UTIL_MakeVectors(vecAimAngles);
	Vector vecDir = gpGlobals->v_forward;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 2048, dont_ignore_monsters, ENT(pev), &tr);

	if (!tr.pHit || VARS(tr.pHit)->takedamage == DAMAGE_NO)
	{
		CBaseEntity* pEnt = NULL;
		while ((pEnt = UTIL_FindEntityInSphere(pEnt, vecSrc, 2048)) != NULL)
		{
			if (!pEnt->pev || FNullEnt(pEnt->pev))
				continue;
			edict_t* pent = pEnt->pev->pContainingEntity;
			if (!pent || !pent->pvPrivateData || pent->free || pEnt->pev == pev)
				continue;
			if (!pEnt->IsAlive() || (pEnt->pev->flags & FL_NOTARGET) != 0 || pEnt->pev->takedamage != DAMAGE_AIM)
				continue;

			Vector vecTarget = pEnt->BodyTarget(vecSrc);
			Vector vecTo = vecTarget - vecSrc;
			UTIL_MakeVectors(vecAimAngles);
			float flDot = DotProduct(vecTo.Normalize(), gpGlobals->v_forward);
			if (flDot < flBestDot)
				continue;

			UTIL_TraceLine(vecSrc, vecTarget, dont_ignore_monsters, ENT(pev), &tr);
			if (tr.flFraction >= 1.0 || tr.pHit == pEnt->edict())
			{
				vecDir = vecTo.Normalize();
				flBestDot = flDot;
			}
		}
	}

	Vector vecEnd1 = DoZapBeamFire(vecSrc, vecDir, vecRight, vecUp, 1, m_fZapPower * 0.5);
	Vector vecEnd2 = DoZapBeamFire(vecSrc, vecDir, vecRight, vecUp, -1, m_fZapPower * 0.5);
	Vector vecMid = (vecEnd1 + vecEnd2) * 0.5;

	UTIL_EmitAmbientSound(ENT(pev), vecMid, "weapons/electro4.wav", 1.0, 0.8, 0, RANDOM_LONG(140, 160));
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/zap3.wav", 1.0, 1.1, 0, RANDOM_LONG(130, 150));
	m_fZapPower = 0;
}

// Sapper balls onto every visible bleeding target near the crosshair. orig 0x12e7a0
void CDriveASlave::EmmitSapBalls()
{
	CBaseEntity* pEnt = NULL;
	entvars_t* pevOwner = pev;
	int iBalls = 0;
	if (m_pOwner)
		pevOwner = m_pOwner->pev;

	// VERIFY: uses whatever v_up the last MakeVectors left
	Vector vecSrc = pev->origin + gpGlobals->v_up * 36;

	Vector vecAimAngles;
	vecAimAngles.x = -30;
	if (pev->idealpitch >= -30)
	{
		vecAimAngles.x = 30;
		if (!(pev->idealpitch > 30))
			vecAimAngles.x = pev->idealpitch;
	}
	vecAimAngles.y = pev->angles.y;
	vecAimAngles.z = 0;
	UTIL_MakeVectors(vecAimAngles);

	while ((pEnt = UTIL_FindEntityInSphere(pEnt, vecSrc, 2048)) != NULL && iBalls <= 14)
	{
		if (!pEnt->pev || FNullEnt(pEnt->pev))
			continue;
		edict_t* pent = pEnt->pev->pContainingEntity;
		if (!pent || !pent->pvPrivateData || pent->free || pEnt->pev == pev)
			continue;
		if (!pEnt->IsAlive() || pEnt->pev->takedamage != DAMAGE_AIM)
			continue;
		if (pEnt->BloodColor() == DONT_BLEED)
			continue;
		if (FClassnameIs(pEnt->pev, "monster_alien_slave") || FClassnameIs(pEnt->pev, "drive_asl"))
			continue;

		Vector vecTarget = pEnt->Center();
		Vector vecTo = vecTarget - vecSrc;
		UTIL_MakeVectors(vecAimAngles);
		if (DotProduct(vecTo.Normalize(), gpGlobals->v_forward) < 0.96)
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecTarget, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction >= 1.0 || tr.pHit == pEnt->edict())
		{
			int iCount = PREFS_KeyLoopUpInt("DRIVEASLAVE", "SlaveSapNumBalls", 5);
			for (int i = 0; i < iCount; i++)
			{
				int iDiv = iCount - 1;
				if (iDiv < 1)
					iDiv = 1;
				CASLRechargeBall_CreateRB(pev, pevOwner, pEnt->pev, ((float)i / (float)iDiv) * 3.0);
				iBalls++;
			}
		}
	}
}

// orig 0x12ebf0
void CDriveASlave::ClawDamage()
{
	float flDamage = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveClawDamage", 20);
	UTIL_MakeAimVectors(pev->angles);

	Vector vecStart = pev->origin;
	vecStart.z = pev->size.z * 0.5 + vecStart.z;
	Vector vecEnd = vecStart + gpGlobals->v_forward * 70;

	TraceResult tr;
	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);

	if (tr.pHit && VARS(tr.pHit)->takedamage != DAMAGE_NO)
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		entvars_t* pevAttacker = pev;
		if (m_pOwner)
			pevAttacker = m_pOwner->pev;

		pev->classname = MAKE_STRING("alien_slave_claws");
		ClearMultiDamage();
		pHit->TraceAttack(pevAttacker, flDamage, gpGlobals->v_forward, &tr, DMG_SLASH);
		ApplyMultiDamage(pev, pevAttacker);
		pev->classname = MAKE_STRING("drive_asl");

		if (!pHit->IsBSPModel())
		{
			pHit->pev->punchangle.z = -18;
			pHit->pev->punchangle.x = 5;
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_pDASLAttackHitSounds[RANDOM_LONG(0, 2)], 1.0, ATTN_NORM, 0, 100);
			return;
		}

		TEXTURETYPE_PlaySound(&tr, vecStart, vecEnd, BULLET_NONE);
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_pDASLAttackMissSounds[RANDOM_LONG(0, 1)], 1.0, ATTN_NORM, 0, 100);
}

// orig 0x12ef00
void CDriveASlave::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1: // claw hits
		ClawDamage();
		break;

	case 4: // end of a zap cycle: fire, or charge another
		m_bWantInZap = m_bFarFutureWishZap;
		if (!m_bFarFutureWishZap)
		{
			FireZap();
		}
		else
		{
			if ((pev->flags & FL_ONGROUND) == 0)
			{
				ClearBeams();
				for (int i = 0; i < 3; i++)
				{
					CreateBeamToGeom(1);
					CreateBeamToGeom(-1);
				}
			}
			ChargeupThrust();
		}
		break;

	case 5:
		if (!m_bWantInZap)
		{
			m_bInZap = false;
			ClearBeams();
			return;
		}
		else
		{
			UTIL_MakeVectors(pev->angles);
			Vector vecLight = pev->origin + gpGlobals->v_forward * 2;

			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecLight);
			WRITE_BYTE(TE_DLIGHT);
			WRITE_COORD(vecLight.x);
			WRITE_COORD(vecLight.y);
			WRITE_COORD(vecLight.z);
			WRITE_BYTE(12); // radius
			WRITE_BYTE(255);
			WRITE_BYTE(180);
			WRITE_BYTE(96);
			WRITE_BYTE(20); // life
			WRITE_BYTE(0);	// decay
			MESSAGE_END();

			m_bCanStopZap = true;
		}
		break;

	case 2001:
		if ((pev->flags & FL_ONGROUND) == 0)
			return;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, RANDOM_LONG(0, 1) == 0 ? "common/bodydrop3.wav" : "common/bodydrop4.wav", 1.0, ATTN_NORM, 0, 100);
		break;

	case 2002:
		if ((pev->flags & FL_ONGROUND) == 0)
			return;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, RANDOM_LONG(0, 1) == 0 ? "common/bodydrop3.wav" : "common/bodydrop4.wav", 1.0, ATTN_NORM, 0, 90);
		break;

	case 2010:
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "zombie/claw_miss2.wav", 1.0, ATTN_NORM, 0, 100);
		break;
	}
}

// Play one of pAnims by weight; returns how long to wait before the next. orig 0x12f220
float CDriveASlave::SetRandomAnim(sDASLAnimT* pAnims, int iCount)
{
	float flTotal = 0;
	for (int i = 0; i < iCount; i++)
		flTotal = flTotal + pAnims[i].flWeight;

	float flPick = RANDOM_FLOAT(0, flTotal);
	int i;
	for (i = 0; i < iCount; i++)
	{
		if (flPick < pAnims[i].flWeight)
			break;
		flPick = flPick - pAnims[i].flWeight;
	}
	if (i >= iCount)
		i = iCount - 1;

	pev->sequence = LookupSequence(pAnims[i].pszSequence);
	ResetSequenceInfo();
	pev->frame = 0;
	return RANDOM_FLOAT(pAnims[i].flMinWait, pAnims[i].flMaxWait);
}

// orig 0x12f300
void CDriveASlave::DyingThink()
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

// orig 0x12f3c0
void CDriveASlave::SetDeathAnim()
{
	UTIL_MakeVectors(pev->angles);
	float flDot = DotProduct(gpGlobals->v_forward, g_vecAttackDir * -1);

	const char* pszSeq;
	if (m_LastHitGroup == HITGROUP_HEAD)
		pszSeq = "dieheadshot";
	else if (flDot > 0.3)
		pszSeq = "dieforward";
	else if (flDot > -0.3)
		pszSeq = "diesimple";
	else
		pszSeq = "diebackward";

	pev->sequence = LookupSequence(pszSeq);
	ResetSequenceInfo();
	pev->frame = 0;
}

// orig 0x12f4b0
bool CDriveASlave::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_pOwner)
	{
		CBaseEntity* pAttacker = CBaseEntity::Instance(pevAttacker);
		if (pAttacker && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, CBaseEntity::Instance(pevAttacker)))
			return false;

		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	if (m_fNextPainSound < gpGlobals->time && flDamage < pev->health)
	{
		m_fNextPainSound = gpGlobals->time + RANDOM_FLOAT(0.9, 1.8);
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_pDASLPainSounds[RANDOM_LONG(0, 1)], 1.0, ATTN_NORM, 0, 100);
	}

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x12f640
void CDriveASlave::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->deadflag != DEAD_NO)
		return;

	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	ClearBeams();

	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 0, 0, 0, 100);

	if (m_pHandGlow)
	{
		m_pHandGlow->TurnOff();
		m_pHandGlow->pev->effects |= EF_NODRAW;
		m_pHandGlow->SUB_Remove();
		m_pHandGlow = NULL;
	}

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->edict());
		WRITE_BYTE(HIDEHUD_ALL);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->edict());
		WRITE_BYTE(0);
		MESSAGE_END();
	}

	// tell the pickup it may respawn
	if (pev->owner)
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	pev->classname = MAKE_STRING("alien_slave");
	DEClientLeave();

	// the driver dies with it
	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		UTIL_SetOrigin(m_pOwner->pev, pev->origin + Vector(0, 0, 64));
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
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_pDASLDeathSounds[RANDOM_LONG(0, 1)], 1.0, ATTN_NORM, 0, 100);
		SetDeathAnim();
		SetThink(&CDriveASlave::DyingThink);
		pev->nextthink = gpGlobals->time + 0.1;
		pev->dmgtime = gpGlobals->time + 5.0;
	}
}

// Steer from the driver's view and put the driver's camera behind the slave. orig 0x12fa10
void CDriveASlave::DEClientPreThink()
{
	if ((m_pOwner->pev->button & IN_RELOAD) == 0)
	{
		// reload held frees the camera from the slave's heading
		pev->ideal_yaw = UTIL_AngleMod(m_pOwner->pev->v_angle.y);
		pev->idealpitch = m_pOwner->pev->v_angle.x;
	}

	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;

	SetEyePosition();
	UTIL_MakeVectors(m_pOwner->pev->v_angle);

	// the slave only moves on its thinks: lead the camera by how it is moving
	Vector vecRot = GetCamRotOrg();
	if (m_vecSudoVel.Length() > 0.01 && m_fLastMoveTime < gpGlobals->time)
		vecRot = GetCamRotOrg() + m_vecSudoVel * (gpGlobals->time - m_fLastMoveTime);

	Vector vecEnd = vecRot - gpGlobals->v_forward * 160;

	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	TraceResult tr;
	UTIL_TraceLine(vecRot, vecEnd, ignore_monsters, ENT(pev), &tr);

	Vector vecCam = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;

	// out of water, pull the camera forward until it is in open air
	int i = 0;
	if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
	{
		for (i = 1; i < 48; i++)
		{
			if (UTIL_PointContents(vecCam) == CONTENTS_EMPTY)
				break;
			vecCam = tr.vecEndPos + gpGlobals->v_forward * 8 * (float)i;
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
		vecCam = vecRot;

	m_pOwner->m_afPhysicsFlags = 0;
	UTIL_SetOrigin(m_pOwner->pev, vecCam);

	if (m_pOwner->m_fDrvEntAlpha < 1.0 && m_pOwner->m_fDrvEntAlpha > 0)
	{
		m_pOwner->pev->euser2 = edict();
		m_pOwner->pev->iuser3 = 4;
		m_pOwner->pev->fuser1 = 2.0;

		float flAlpha = 0;
		if (m_pOwner->m_fDrvEntAlpha * 255 >= 0)
		{
			flAlpha = 255;
			if (m_pOwner->m_fDrvEntAlpha * 255 <= 255)
				flAlpha = m_pOwner->m_fDrvEntAlpha * 255;
		}
		m_pOwner->pev->fuser2 = flAlpha;
		m_pOwner->pev->fuser3 = 0;
		m_pOwner->pev->vuser1 = g_vecZero;
	}
}

// orig 0x12ff40
void CDriveASlave::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x12ff60
void CDriveASlave::DEClientEnter(CBasePlayer* pPlayer)
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
	m_iLastHUDHealth = -1;

	MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, m_pOwner->edict());
	WRITE_BYTE(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->edict());
	WRITE_BYTE(m_pOwner->m_iHideHUD);
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

// Give the driver back a normal player body. orig 0x130310
void CDriveASlave::DEClientLeave()
{
	if (!m_pOwner || !m_pOwner->pev || !m_pOwner->edict() || m_pOwner->edict()->free || !m_pOwner->m_bInDriveEnt)
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

	SET_VIEW(m_pOwner->edict(), m_pOwner->edict());
	SetupVisibility(m_pOwner->edict(), m_pOwner->edict(), NULL, NULL);
}

// Put the driver on top of the slave, which dies. orig 0x130620
void CDriveASlave::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVEASLAVE", "AllowEject", false))
	{
		if (CVAR_GET_FLOAT("sv_allowglobaleject") != 1 && g_pGameRules->IsMultiplayer() && !g_pGameRules->IsCoOp())
			return;
	}

	if (!m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt)
		return;
	if (m_pOwner->m_pCurDriveEnt->pev != pev || !(m_pOwner->m_pCurDriveEnt->pev->health > 0))
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
	m_pOwner->pev->angles.x = m_pOwner->pev->v_angle.x = 0;
	m_pOwner->pev->angles.z = m_pOwner->pev->v_angle.z = 0;
	m_pOwner->pev->fixangle = 1;
	m_pOwner->m_pCurDriveEnt = NULL;
}

// orig 0x134510
void CDriveASlave::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->edict() == pEdict)
		m_pOwner = NULL;

	if (m_pHandGlow && m_pHandGlow->pev && m_pHandGlow->edict() == pEdict)
		m_pHandGlow = NULL;

	for (int i = 0; i < 16; i++)
	{
		if (m_pBeams[i] && m_pBeams[i]->pev && m_pBeams[i]->edict() == pEdict)
			m_pBeams[i] = NULL;
	}

	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

//=========================================================
// CASLZapChargeSprite: a flare that flies into a hand (attachment pev->button) of its owner
// before pev->dmgtime, as the zap charges.
//=========================================================
class CASLZapChargeSprite : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	static CASLZapChargeSprite* CreateSS(entvars_t* pevOwner, Vector vecOrigin, float flTime, int iAttachment);
	void EXPORT SSThink();
};

CBaseEntity* CASLZapChargeSprite_CreateSS(entvars_t* pevOwner, Vector vecOrigin, float flTime, int iAttachment)
{
	return CASLZapChargeSprite::CreateSS(pevOwner, vecOrigin, flTime, iAttachment);
}

LINK_ENTITY_TO_CLASS(asl_charge_flare, CASLZapChargeSprite); // orig 0x12aa80

// orig 0x130830
void CASLZapChargeSprite::Spawn()
{
	pev->classname = MAKE_STRING("asl_charge_flare");
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;
	pev->renderamt = 16;
	pev->rendermode = kRenderTransAdd;
	pev->renderfx = kRenderFxNone;
	pev->scale = 0.1 + RANDOM_FLOAT(-0.04, 0.04);
	SET_MODEL(ENT(pev), "sprites/heeyeflare2.spr");
	UTIL_SetOrigin(pev, pev->origin);
	pev->frame = 0;
	pev->speed = 0;
	SetThink(&CASLZapChargeSprite::SSThink);
	pev->animtime = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x130960
void CASLZapChargeSprite::Precache()
{
	PRECACHE_MODEL("sprites/heeyeflare2.spr");
}

// orig 0x130990
CASLZapChargeSprite* CASLZapChargeSprite::CreateSS(entvars_t* pevOwner, Vector vecOrigin, float flTime, int iAttachment)
{
	CASLZapChargeSprite* pSprite = GetClassPtr((CASLZapChargeSprite*)NULL);
	pSprite->Spawn();
	pSprite->pev->owner = pevOwner->pContainingEntity;
	pSprite->pev->dmg = flTime;
	pSprite->pev->dmgtime = gpGlobals->time + pSprite->pev->dmg;
	pSprite->pev->button = iAttachment;
	UTIL_SetOrigin(pSprite->pev, vecOrigin);
	return pSprite;
}

// orig 0x130a10
void CASLZapChargeSprite::SSThink()
{
	pev->nextthink = gpGlobals->time + 0.2;

	if (!(pev->dmgtime < gpGlobals->time) && pev->owner && UTIL_IsValidEntity(pev->owner))
	{
		CBaseAnimating* pOwner = (CBaseAnimating*)CBaseEntity::Instance(pev->owner);
		if (pOwner)
		{
			float flInterval = gpGlobals->time - pev->animtime;
			float flLeft = pev->dmgtime - gpGlobals->time;
			pev->animtime = gpGlobals->time;

			pev->speed = 400 * flInterval + pev->speed;
			if (pev->speed > 800)
				pev->speed = 800;

			if (pev->renderamt < 140)
				pev->renderamt = flInterval * 260 + pev->renderamt;
			if (pev->renderamt > 140)
				pev->renderamt = 140;

			if (pev->scale < 0.2)
				pev->scale = flInterval * 0.4 + pev->scale;
			if (pev->scale > 0.2)
				pev->scale = 0.2;

			UTIL_MakeVectors(VARS(pev->owner)->angles);
			Vector vecHand, vecAngles;
			pOwner->GetAttachment(pev->button, vecHand, vecAngles);

			Vector vecDelta = vecHand - pev->origin;
			if (vecDelta.Length() >= 2)
			{
				// arrive on time, never faster than speed
				float flSpeed = vecDelta.Length() / flLeft;
				if (pev->speed < flSpeed)
					flSpeed = pev->speed;
				pev->velocity = vecDelta.Normalize() * flSpeed;
				return;
			}

			pev->effects |= EF_NODRAW;
			SetThink(&CASLZapChargeSprite::SUB_Remove);
			return;
		}
	}

	pev->effects |= EF_NODRAW;
	pev->nextthink = gpGlobals->time + 0.2;
	SetThink(&CASLZapChargeSprite::SUB_Remove);
}

//=========================================================
// CASLEngBall: a homing energy ball that shocks what is near it. Nothing in this build creates
// one (CDriveASlave::DoBallAttack is empty).
// pev->dmg_inflictor: the attacker. pev->air_finished: when it becomes solid. pev->dmg_save: next shock.
//=========================================================
class CASLEngBall : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	static CASLEngBall* CreateEB(entvars_t* pevOwner, entvars_t* pevInflictor, Vector vecOrigin, Vector vecDir);
	void EXPORT EBThink();
	void EXPORT EBTouch(CBaseEntity* pOther);
};

LINK_ENTITY_TO_CLASS(alien_slave_energy_ball, CASLEngBall); // orig 0x12aab0

// orig 0x130d60
void CASLEngBall::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("alien_slave_energy_ball");
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SET_MODEL(ENT(pev), "sprites/nhth1.spr");
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;
	pev->rendercolor = Vector(255, 255, 255);
	pev->scale = 0.4;
	UTIL_SetOrigin(pev, pev->origin);
	pev->frame = 0;
	SetThink(&CASLEngBall::EBThink);
	SetTouch(&CASLEngBall::EBTouch);
	pev->nextthink = gpGlobals->time + 0.05;
	pev->animtime = gpGlobals->time;
	pev->speed = 100;
}

// orig 0x130eb0
void CASLEngBall::Precache()
{
	PRECACHE_MODEL("sprites/nhth1.spr");
	PRECACHE_SOUND("weapons/electro4.wav");
}

// orig 0x130ef0
CASLEngBall* CASLEngBall::CreateEB(entvars_t* pevOwner, entvars_t* pevInflictor, Vector vecOrigin, Vector vecDir)
{
	CASLEngBall* pBall = GetClassPtr((CASLEngBall*)NULL);
	pBall->pev->owner = pevOwner->pContainingEntity;
	pBall->Spawn();
	pBall->pev->dmg_inflictor = pevInflictor->pContainingEntity;
	pBall->pev->dmgtime = gpGlobals->time + 20.0;
	pBall->pev->movedir = vecDir;
	pBall->pev->dmg_save = gpGlobals->time + RANDOM_FLOAT(0.5, 0.8);
	pBall->pev->air_finished = gpGlobals->time + 0.2;
	UTIL_SetOrigin(pBall->pev, vecOrigin);
	return pBall;
}

// orig 0x130fd0
void CASLEngBall::EBThink()
{
	pev->nextthink = gpGlobals->time + 0.05;

	if (!pev->owner || !UTIL_IsValidEntity(pev->owner) || pev->dmgtime < gpGlobals->time || pev->waterlevel != 0)
	{
		SetTouch(NULL);
		SetThink(NULL);
		pev->effects |= EF_NODRAW;
		UTIL_Remove(this);
		pev->nextthink = gpGlobals->time + 0.2;
		return;
	}

	if (pev->air_finished < gpGlobals->time && pev->air_finished != -1)
	{
		SetTouch(&CASLEngBall::EBTouch);
		pev->solid = SOLID_BBOX;
		pev->air_finished = -1;
	}

	float flNow = gpGlobals->time;
	float flLast = pev->animtime;
	pev->animtime = gpGlobals->time;

	if (pev->dmg_save < gpGlobals->time)
	{
		CBaseEntity* pBest = NULL;
		float flBestDist = 9999;
		TraceResult tr;
		memset(&tr, 0, sizeof(tr)); // VERIFY: the original reads tr below even when nothing traced

		CBaseEntity* pEnt = NULL;
		while ((pEnt = UTIL_FindEntityInSphere(pEnt, pev->origin, PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveBallShockRadius", 200))) != NULL)
		{
			if (FStrEq(STRING(pEnt->pev->classname), STRING(pev->classname)))
			{
				// arc to other balls now and then
				if (RANDOM_FLOAT(0, 1) < 0.2)
				{
					MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
					WRITE_BYTE(TE_BEAMENTS);
					WRITE_SHORT(ENTINDEX(edict()));
					WRITE_SHORT(ENTINDEX(pEnt->edict()));
					WRITE_SHORT(g_sModelIndexLaser);
					WRITE_BYTE(0);	 // start frame
					WRITE_BYTE(10);	 // framerate
					WRITE_BYTE(3);	 // life
					WRITE_BYTE(10);	 // width
					WRITE_BYTE(20);	 // noise
					WRITE_BYTE(64);
					WRITE_BYTE(196);
					WRITE_BYTE(255);
					WRITE_BYTE(255); // brightness
					WRITE_BYTE(10);	 // speed
					MESSAGE_END();
				}
			}
			else if (pEnt->pev->takedamage == DAMAGE_AIM && pEnt->pev != VARS(pev->owner))
			{
				float flDist = (pEnt->Center() - pev->origin).Length();
				if (flDist < flBestDist)
				{
					UTIL_TraceLine(pev->origin, pEnt->Center(), ignore_monsters, ENT(pev), &tr);
					if (tr.flFraction >= 1.0 || tr.pHit == pEnt->edict())
					{
						pBest = pEnt;
						flBestDist = flDist;
					}
				}
			}
		}

		if (!pBest)
		{
			// arc at a random wall ahead
			Vector vecAngles = UTIL_VecToAngles(pev->movedir);
			float flPitch = RANDOM_FLOAT(-50, 50);
			float flYaw = RANDOM_FLOAT(-50, 50);
			UTIL_MakeAimVectors(vecAngles + Vector(flPitch, flYaw, 0));
			Vector vecEnd = pev->origin + gpGlobals->v_forward * RANDOM_FLOAT(200, 300);
			UTIL_TraceLine(pev->origin, vecEnd, ignore_monsters, ENT(pev), &tr);

			if (tr.flFraction >= 1.0)
			{
				pev->dmg_save = gpGlobals->time + RANDOM_FLOAT(0.2, 0.4);
			}
			else
			{
				MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
				WRITE_BYTE(TE_BEAMENTPOINT);
				WRITE_SHORT(ENTINDEX(edict()));
				WRITE_COORD(tr.vecEndPos.x);
				WRITE_COORD(tr.vecEndPos.y);
				WRITE_COORD(tr.vecEndPos.z);
				WRITE_SHORT(g_sModelIndexLaser);
				WRITE_BYTE(0);	 // start frame
				WRITE_BYTE(10);	 // framerate
				WRITE_BYTE(3);	 // life
				WRITE_BYTE(15);	 // width
				WRITE_BYTE(20);	 // noise
				WRITE_BYTE(64);
				WRITE_BYTE(196);
				WRITE_BYTE(255);
				WRITE_BYTE(255); // brightness
				WRITE_BYTE(10);	 // speed
				MESSAGE_END();

				UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));
				UTIL_EmitAmbientSound(ENT(pev), tr.vecEndPos, "weapons/electro4.wav", 0.4, 0.8, 0, RANDOM_LONG(140, 160));
				pev->dmg_save = gpGlobals->time + RANDOM_FLOAT(0.4, 0.6);
			}
		}
		else
		{
			pBest->TakeDamage(pev, pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL, PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveBallShockDamage", 15), DMG_SHOCK | DMG_ENERGYBEAM);

			// VERIFY: the arc goes to the last traced point, not necessarily the target's
			MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
			WRITE_BYTE(TE_BEAMENTPOINT);
			WRITE_SHORT(ENTINDEX(edict()));
			WRITE_COORD(tr.vecEndPos.x);
			WRITE_COORD(tr.vecEndPos.y);
			WRITE_COORD(tr.vecEndPos.z);
			WRITE_SHORT(g_sModelIndexLaser);
			WRITE_BYTE(0);	 // start frame
			WRITE_BYTE(10);	 // framerate
			WRITE_BYTE(3);	 // life
			WRITE_BYTE(15);	 // width
			WRITE_BYTE(20);	 // noise
			WRITE_BYTE(64);
			WRITE_BYTE(196);
			WRITE_BYTE(255);
			WRITE_BYTE(255); // brightness
			WRITE_BYTE(10);	 // speed
			MESSAGE_END();

			UTIL_EmitAmbientSound(ENT(pev), tr.vecEndPos, "weapons/electro4.wav", 0.5, 0.8, 0, RANDOM_LONG(140, 160));
			pev->dmg_save = gpGlobals->time + RANDOM_FLOAT(0.6, 0.8);
		}
	}

	pev->frame = (int)(pev->frame + 1) % 11;

	float flSpeed = (flNow - flLast) * 1000 + pev->speed;
	if (flSpeed >= 1200)
		flSpeed = 1200;
	pev->speed = flSpeed;
	pev->velocity = pev->movedir * pev->speed;
}

// orig 0x131b40
void CASLEngBall::EBTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;
	if (FStrEq(STRING(pOther->pev->classname), STRING(pev->classname)))
		return;

	pOther->TakeDamage(pev, pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL, PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveBallHitDamage", 20), DMG_SHOCK | DMG_ENERGYBEAM);
	UTIL_EmitAmbientSound(ENT(pev), pev->origin, "weapons/electro4.wav", 0.7, 0.8, 0, RANDOM_LONG(90, 95));

	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + pev->movedir * 64, ignore_monsters, ENT(pev), &tr);
	UTIL_DecalTrace(&tr, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));

	SetTouch(NULL);
	pev->effects |= EF_NODRAW;
	UTIL_Remove(this);
	pev->nextthink = gpGlobals->time + 0.2;
}

//=========================================================
// CASLRechargeBall: a sapper ball. Circles its victim, which it hurt once, then flies to the
// slave and circles it handing over health.
// pev->owner: the victim. pev->enemy: who it heads for (the slave). pev->dmg_inflictor: the driver.
// pev->ideal_yaw: end of this circling, pev->dmg_take its length, pev->dmg: angle round the
// circle, pev->dmg_save: circle radius, pev->movedir.y: start angle, pev->pitch_speed: height
// offset, pev->maxspeed: top speed, pev->air_finished: health it carries.
//=========================================================
class CASLRechargeBall : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	static CASLRechargeBall* CreateRB(entvars_t* pevSlave, entvars_t* pevDriver, entvars_t* pevVictim, float flDelay);
	void EXPORT InitThink();
	void EXPORT CircleLeaveVictem();
	void EXPORT FlyToOwner();
	void EXPORT RechargeOwner();

	CBeam* m_pBeam; // orig +0x34, the healing beam
};

CBaseEntity* CASLRechargeBall_CreateRB(entvars_t* pevSlave, entvars_t* pevDriver, entvars_t* pevVictim, float flDelay)
{
	return CASLRechargeBall::CreateRB(pevSlave, pevDriver, pevVictim, flDelay);
}

LINK_ENTITY_TO_CLASS(alien_slave_sapper, CASLRechargeBall); // orig 0x12aae0

// orig 0x131cf0
void CASLRechargeBall::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("alien_slave_sapper");
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;
	SET_MODEL(ENT(pev), "sprites/muzzleflash3.spr");
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;
	pev->renderamt = 255;
	pev->rendercolor = Vector(255, 224, 192);
	pev->scale = 0.7;
	pev->avelocity.z = 2000;
	pev->effects |= EF_NODRAW;
	pev->armorvalue = 0.4;
	UTIL_SetOrigin(pev, pev->origin);
	pev->frame = 0;
	SetThink(&CASLRechargeBall::InitThink);
	pev->speed = 100;
}

// orig 0x131e10
void CASLRechargeBall::Precache()
{
	PRECACHE_MODEL("sprites/muzzleflash3.spr");
	PRECACHE_MODEL("sprites/xbeam1.spr");
	PRECACHE_SOUND("weapons/electro4.wav");
}

// orig 0x131e60
CASLRechargeBall* CASLRechargeBall::CreateRB(entvars_t* pevSlave, entvars_t* pevDriver, entvars_t* pevVictim, float flDelay)
{
	CASLRechargeBall* pBall = GetClassPtr((CASLRechargeBall*)NULL);
	pBall->Spawn();
	pBall->pev->owner = pevVictim->pContainingEntity;
	pBall->pev->enemy = pevSlave->pContainingEntity;
	pBall->pev->dmg_inflictor = pevDriver->pContainingEntity;
	pBall->pev->nextthink = flDelay + gpGlobals->time;
	UTIL_SetOrigin(pBall->pev, pevVictim->origin);
	return pBall;
}

// Hurt the victim and start circling it. orig 0x131ef0
void CASLRechargeBall::InitThink()
{
	CBaseEntity* pVictim = NULL;
	if (pev->owner && UTIL_IsValidEntity(pev->owner) && pev->enemy && UTIL_IsValidEntity(pev->enemy))
		pVictim = CBaseEntity::Instance(pev->owner);

	if (!pVictim || !pVictim->IsAlive() || pVictim->pev->takedamage != DAMAGE_AIM || pVictim->pev->deadflag != DEAD_NO)
	{
		SetTouch(NULL);
		SetThink(NULL);
		pev->effects |= EF_NODRAW;
		UTIL_Remove(this);
		pev->nextthink = gpGlobals->time + 0.2;
		return;
	}

	pev->effects &= ~EF_NODRAW;
	pev->renderamt = 255;

	pev->movedir = Vector(0, RANDOM_FLOAT(0, 360), 0);
	pev->dmg = 0;
	pev->dmg_save = 8;
	pev->dmg_take = RANDOM_FLOAT(2.8, 3.6);
	pev->ideal_yaw = gpGlobals->time + pev->dmg_take;
	pev->pitch_speed = RANDOM_FLOAT(-16, 16);
	pev->renderamt = 0;
	pev->scale = 0.05;
	UTIL_SetOrigin(pev, pVictim->Center());
	pev->maxspeed = RANDOM_FLOAT(500, 650);

	pVictim->TakeDamage(pev, pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL, PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveSapperBallDamagePerBall", 8), DMG_SHOCK | DMG_ENERGYBEAM | DMG_PARALYZE);
	pev->air_finished = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveSapperBallHealthGivePerBall", 10);

	if (!pVictim->IsAlive() || pVictim->pev->takedamage != DAMAGE_AIM || pVictim->pev->deadflag != DEAD_NO)
		SetThink(&CASLRechargeBall::FlyToOwner);
	else
		SetThink(&CASLRechargeBall::CircleLeaveVictem);

	pev->nextthink = gpGlobals->time + 0.1;
	pev->animtime = gpGlobals->time;
}

// Spiral out from the victim. orig 0x1322c0
void CASLRechargeBall::CircleLeaveVictem()
{
	pev->nextthink = gpGlobals->time + 0.2;
	float flInterval = gpGlobals->time - pev->animtime;
	if (!(flInterval > 0.01))
		flInterval = 0.01;
	pev->animtime = gpGlobals->time;
	pev->speed = 100;

	if (!pev->owner || !UTIL_IsValidEntity(pev->owner))
	{
		SetThink(&CASLRechargeBall::FlyToOwner);
		pev->owner = NULL;
		return;
	}

	CBaseEntity* pSlave = NULL;
	if (pev->enemy && UTIL_IsValidEntity(pev->enemy))
		pSlave = CBaseEntity::Instance(pev->enemy);
	if (!pSlave || !pSlave->IsAlive() || pSlave->pev->deadflag != DEAD_NO)
	{
		SetThink(&CASLRechargeBall::FlyToOwner);
		pev->enemy = pev->owner; // VERIFY: as the original
		return;
	}

	if (pev->ideal_yaw < gpGlobals->time)
	{
		// done: head for the slave
		pev->nextthink = gpGlobals->time + 0.01;
		SetThink(&CASLRechargeBall::FlyToOwner);
		pev->movedir = Vector(0, RANDOM_FLOAT(0, 360), 0);
		pev->pitch_speed = RANDOM_FLOAT(-16, 16);
		return;
	}

	CBaseEntity* pVictim = CBaseEntity::Instance(pev->owner);
	Vector vecCenter = pVictim->Center() + Vector(0, 0, pev->pitch_speed) + pVictim->pev->velocity * flInterval;
	if (FClassnameIs(pVictim->pev, "drive_asl"))
		vecCenter = vecCenter + ((CDriveASlave*)pVictim)->m_vecSudoVel * flInterval;

	float flFrac = 1.0 - (pev->ideal_yaw - gpGlobals->time) / pev->dmg_take;

	pev->dmg = UTIL_AngleMod(flFrac * 360);
	float flRadius = 4;
	if (flFrac * 90 >= 4)
	{
		flRadius = 120;
		if (!(flFrac * 90 > 120))
			flRadius = flFrac * 90;
	}
	pev->dmg_save = flRadius;

	UTIL_MakeVectors(Vector(0, UTIL_AngleMod(pev->movedir.y + pev->dmg), 0));
	pev->velocity = ((vecCenter + gpGlobals->v_forward * pev->dmg_save) - pev->origin) / flInterval;
	if (pev->velocity.Length() > pev->maxspeed)
		pev->velocity = pev->velocity.Normalize() * pev->maxspeed;

	float flAmt = 16;
	if (flFrac * 240 >= 16)
	{
		flAmt = 240;
		if (flFrac * 240 <= 240)
			flAmt = flFrac * 240;
	}
	pev->renderamt = flAmt;

	float flScale = 0.05;
	if (flFrac * 0.5 >= 0.05)
	{
		flScale = 0.5;
		if (flFrac * 0.5 <= 0.5)
			flScale = flFrac * 0.5;
	}
	pev->scale = flScale;
}

// orig 0x132a20
void CASLRechargeBall::FlyToOwner()
{
	pev->nextthink = gpGlobals->time + 0.2;
	float flInterval = gpGlobals->time - pev->animtime;
	if (!(0.01 < flInterval))
		flInterval = 0.01;
	pev->animtime = gpGlobals->time;

	if (!pev->owner || !UTIL_IsValidEntity(pev->owner))
		pev->owner = NULL;

	if (!pev->enemy || !UTIL_IsValidEntity(pev->enemy))
	{
		if (!pev->owner)
		{
			pev->effects |= EF_NODRAW;
			SetThink(&CASLRechargeBall::SUB_Remove);
			return;
		}
		pev->enemy = pev->owner;
	}

	CBaseEntity* pTarget = CBaseEntity::Instance(pev->enemy);
	if (!pTarget || !pTarget->IsAlive())
	{
		if (!pev->owner)
		{
			pev->effects |= EF_NODRAW;
			SetThink(&CASLRechargeBall::SUB_Remove);
			return;
		}
		pev->enemy = pev->owner;

		// VERIFY: the original calls pTarget->IsAlive() again unchecked, then flies on toward the
		// old target this think if the owner is alive
		if (!pTarget || !pTarget->IsAlive())
		{
			CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
			if (!pOwner || !pOwner->IsAlive())
			{
				pev->effects |= EF_NODRAW;
				SetThink(&CASLRechargeBall::SUB_Remove);
				return;
			}
			if (!pTarget)
				return;
		}
	}

	pev->speed = flInterval * 400 + pev->speed;
	float flSpeed = 50;
	if (pev->speed >= 50)
		flSpeed = pev->speed <= pev->maxspeed ? pev->speed : pev->maxspeed;
	pev->speed = flSpeed;

	Vector vecCenter = pTarget->Center() + Vector(0, 0, pev->pitch_speed) + pTarget->pev->velocity * flInterval;
	if (FClassnameIs(pTarget->pev, "drive_asl"))
	{
		CDriveASlave* pSlave = (CDriveASlave*)pTarget;
		vecCenter = vecCenter + pSlave->m_vecSudoVel * (gpGlobals->time - pSlave->m_fLastMoveTime);
	}

	UTIL_MakeVectors(Vector(0, UTIL_AngleMod(pev->movedir.y), 0));
	Vector vecGoal = vecCenter + gpGlobals->v_forward * 100;
	Vector vecDelta = vecGoal - pev->origin;

	if (vecDelta.Length() < 100)
	{
		// arrived: start circling the slave
		UTIL_SetOrigin(pev, vecGoal);
		pev->dmg = 0;
		pev->dmg_save = 100;
		pev->dmg_take = RANDOM_FLOAT(4.9, 5.8);
		pev->ideal_yaw = gpGlobals->time + pev->dmg_take;
		m_pBeam = NULL;
		SetThink(&CASLRechargeBall::RechargeOwner);
		pev->nextthink = 0.01 + gpGlobals->time;
		return;
	}

	float flNeed = vecDelta.Length() / flInterval;
	if (pev->speed < flNeed)
		flNeed = pev->speed;
	pev->velocity = vecDelta.Normalize() * flNeed;
}

// Spiral in on the slave, healing it through a beam. orig 0x133010
void CASLRechargeBall::RechargeOwner()
{
	pev->nextthink = gpGlobals->time + 0.2;
	float flInterval = gpGlobals->time - pev->animtime;
	if (!(flInterval > 0.01))
		flInterval = 0.01;
	pev->animtime = gpGlobals->time;

	if (pev->enemy && UTIL_IsValidEntity(pev->enemy))
	{
		CBaseEntity* pTarget = CBaseEntity::Instance(pev->enemy);
		CBaseEntity* pVictim = CBaseEntity::Instance(pev->owner);

		if (pTarget && pTarget->IsAlive())
		{
			if (!(pev->ideal_yaw < gpGlobals->time))
			{
				Vector vecCenter = pTarget->Center() + Vector(0, 0, pev->pitch_speed) + pTarget->pev->velocity * flInterval;
				if (FClassnameIs(pTarget->pev, "drive_asl"))
					vecCenter = vecCenter + ((CDriveASlave*)pTarget)->m_vecSudoVel * flInterval * 0.5;

				float flFrac = (pev->ideal_yaw - gpGlobals->time) / pev->dmg_take;

				pev->dmg = UTIL_AngleMod((1.0 - flFrac) * 720);
				float flRadius = 8;
				if (flFrac * 90 + 10 >= 8)
				{
					flRadius = 120;
					if (!(flFrac * 90 + 10 > 120))
						flRadius = flFrac * 90 + 10;
				}
				pev->dmg_save = flRadius;

				UTIL_MakeVectors(Vector(0, UTIL_AngleMod(pev->movedir.y + pev->dmg), 0));
				pev->velocity = ((vecCenter + gpGlobals->v_forward * pev->dmg_save) - pev->origin) / flInterval;
				if (pev->velocity.Length() > pev->maxspeed)
					pev->velocity = pev->velocity.Normalize() * pev->maxspeed;

				float flHeal = flFrac * pev->air_finished * flInterval;
				if (pTarget->pev->takedamage != DAMAGE_NO && flHeal > 0)
				{
					pTarget->pev->health = flHeal + pTarget->pev->health;
					if (pTarget->pev->health > pTarget->pev->max_health)
						pTarget->pev->health = pTarget->pev->max_health;
				}

				float flAmt = 1;
				if (flFrac * 240 >= 1)
				{
					flAmt = 255;
					if (!(flFrac * 240 > 255))
						flAmt = flFrac * 240;
				}
				pev->renderamt = flAmt;

				float flScale = 0.01;
				if (flFrac * 0.5 >= 0.01)
				{
					flScale = 0.5;
					if (flFrac * 0.5 <= 0.5)
						flScale = flFrac * 0.5;
				}
				pev->scale = flScale;

				if (CVAR_GET_FLOAT("sv_lowlag") == 1)
					return;

				if (!m_pBeam)
				{
					m_pBeam = CBeam::BeamCreate(g_pModelNameLaser, 40);
					m_pBeam->PointEntInit(pTarget->Center(), ENTINDEX(edict()));
				}
				m_pBeam->pev->origin = vecCenter;

				int iWidth;
				if (flFrac * 35 < 1)
					iWidth = 1;
				else if (flFrac * 35 > 40)
					iWidth = 40;
				else
					iWidth = (int)(flFrac * 35);
				m_pBeam->pev->scale = iWidth;
				m_pBeam->pev->rendercolor = Vector(255, 128, 64);

				int iBright;
				if (flFrac * 240 < 1)
					iBright = 1;
				else if (flFrac * 240 > 240)
					iBright = 240;
				else
					iBright = (int)(flFrac * 240);
				m_pBeam->pev->renderamt = iBright;
				m_pBeam->pev->body = 25; // noise
				return;
			}

			pev->effects |= EF_NODRAW;
		}
		else if (pVictim && pVictim->IsAlive())
		{
			// the slave died: take it back to the victim
			pev->enemy = pev->owner;
			if (m_pBeam)
			{
				m_pBeam->SetThink(&CBeam::SUB_Remove);
				m_pBeam->pev->nextthink = gpGlobals->time + 0.01;
			}
			SetThink(&CASLRechargeBall::FlyToOwner);
			return;
		}
		else
		{
			pev->effects |= EF_NODRAW;
		}
	}
	else
	{
		pev->effects |= EF_NODRAW;
	}

	SetThink(&CASLRechargeBall::SUB_Remove);
	if (m_pBeam)
	{
		m_pBeam->SetThink(&CBeam::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 0.01;
	}
}

//=========================================================
// CDriveSlavePickup: the dormant slave a player +uses to drive one
//=========================================================
class CDriveSlavePickup : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	CBaseEntity* GetSpawnOrg();
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void EXPORT PAnimThink();
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT DoRespawn();
	void DeathNotice(entvars_t* pevChild) override;

	int ObjectCaps() override { return m_bTakeable ? FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION; } // orig 0x134460
	int GetRCCaps() override { return 0; }													  // orig 0x134480
	virtual bool IsTeleFraggabe() { return false; }											  // orig 0x134490
	int BloodColor() override { return DONT_BLEED; }										  // orig 0x1344a0
	int Classify() override { return CLASS_NONE; }											  // orig 0x1344b0

	CBaseEntity* m_pGiven; // orig +0x27c, the drive_asl it made
	bool m_bTakeable;	   // orig +0x280
};

LINK_ENTITY_TO_CLASS(pickup_drive_slave, CDriveSlavePickup); // orig 0x1338e0

TYPEDESCRIPTION CDriveSlavePickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveSlavePickup, m_pGiven, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveSlavePickup, m_bTakeable, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDriveSlavePickup, CBaseMonster); // orig 0x133910, 0x133960

// orig 0x1339b0
void CDriveSlavePickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), "models/islave.mdl");
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));
	UTIL_SetOrigin(pev, pev->origin);
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->sequence = LookupSequence("idle3");
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);
	InitBoneControllers();

	SetUse(&CDriveSlavePickup::Use);
	SetThink(&CDriveSlavePickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;

	m_bTakeable = true;
	m_pGiven = NULL;

	CBaseEntity* pSpot = GetSpawnOrg();
	if (!pSpot)
	{
		SetThink(&CDriveSlavePickup::SUB_Remove);
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

// orig 0x133bf0
void CDriveSlavePickup::Precache()
{
	PRECACHE_MODEL("models/islave.mdl");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_asl");
}

// The first usable info_drive_slave_spawn. orig 0x133c40
CBaseEntity* CDriveSlavePickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_slave_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (FNullEnt(pSpot->pev))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x133cc0
bool CDriveSlavePickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return false;
}

// Fade in when takeable; end the drive when its time is up. orig 0x133cd0
void CDriveSlavePickup::PAnimThink()
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

// orig 0x133e70
void CDriveSlavePickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!m_bTakeable)
		return;
	if (!pCaller->IsPlayer() || !pCaller)
		return;

	CBasePlayer* pPlayer = (CBasePlayer*)pCaller;
	if (pPlayer->m_fDrcEntTransTime > gpGlobals->time || pPlayer->m_bInDriveEnt)
		return;
	if (pPlayer->Classify() != CLASS_PLAYER)
		return;
	if (strcmp(STRING(pPlayer->pev->classname), "player") != 0)
		return;

	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_asl", pev->origin, pev->angles, edict());
	pPlayer->pev->angles = Vector(0, pev->angles.y, 0);
	pPlayer->pev->v_angle = pPlayer->pev->angles;
	pPlayer->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGiven = pPlayer->m_pCurDriveEnt;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDriveSlavePickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveMaxDriveTime", 600) + gpGlobals->time;
}

// orig 0x134070
void CDriveSlavePickup::DoRespawn()
{
	m_bTakeable = true;
	m_pGiven = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);
	SetThink(&CDriveSlavePickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// The slave it gave out died. orig 0x134130
void CDriveSlavePickup::DeathNotice(entvars_t* pevChild)
{
	if (m_bTakeable)
		return;

	pev->nextthink = PREFS_KeyLoopUpFloat("DRIVEASLAVE", "SlaveRespawnDelayAfterDeath", 5.0) + gpGlobals->time;
	SetThink(&CDriveSlavePickup::DoRespawn);
}

//=========================================================
// CDrvSlaveSpawner: makes mp_driveslave pickups 15 seconds into a multiplayer map
//=========================================================
class CDrvSlaveSpawner : public CBaseEntity
{
public:
	void Spawn() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT DrvAThink();
	int ObjectCaps() override { return FCAP_DONT_SAVE; } // orig 0x134400

	bool m_bDummy; // orig +0x34
};

// Called from CWorld::Precache. orig 0x1341a0
void CreateDrvSlaveSpawner()
{
	if (g_pGameRules->IsMultiplayer() && !UTIL_FindEntityByClassname(NULL, "drive_slave_spawner"))
		CBaseEntity::Create("drive_slave_spawner", g_vecZero, g_vecZero, NULL);
}

LINK_ENTITY_TO_CLASS(drive_slave_spawner, CDrvSlaveSpawner); // orig 0x134200

TYPEDESCRIPTION CDrvSlaveSpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvSlaveSpawner, m_bDummy, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDrvSlaveSpawner, CBaseEntity); // orig 0x134230, 0x134280

// orig 0x1342d0
void CDrvSlaveSpawner::DrvAThink()
{
	for (int i = 0; i < (int)CVAR_GET_FLOAT("mp_driveslave"); i++)
		CBaseEntity::Create("pickup_drive_slave", g_vecZero, g_vecZero, NULL);

	UTIL_Remove(this);
}

// orig 0x134360
void CDrvSlaveSpawner::Spawn()
{
	pev->classname = MAKE_STRING("drive_slave_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvSlaveSpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15.0;
}
