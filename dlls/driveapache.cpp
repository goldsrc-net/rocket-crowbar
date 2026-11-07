/***
*
*	Rocket Crowbar 1.9: driveapache.cpp. The drivable Apache: a player who uses a
*	pickup_drive_apache gets a drive_apache to fly (CBaseDriveEnt), with a chin gun and
*	volleys of steered drv_hvr_rocket rockets. drive_apache_spawner places the pickups.
*	Tunables come from the DRIVEAPACHE section of rc.ini (PREFS_KeyLoopUp*).
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
#include "UserMessages.h"
#include "driveent.h"
#include "driveapache.h"

// Preff.cpp (rc.ini), declared as main's dlls/Preff.h has them.
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
extern int PREFS_KeyLoopUpInt(const char* szHeading, const char* szKey, int iDefault);
extern bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault);

#define APACHE_PREFS "DRIVEAPACHE"

//=========================================================
// CDriveApache: the Apache a player flies
//=========================================================
class CDriveApache : public CBaseDriveEnt
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void SetObjectCollisionBox() override;
	int Classify() override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int BloodColor() override;
	Vector Center() override;
	float ExtreemeDamageFract() override;
	float VeryHeavyDamageFract() override;
	const char* GetControllerName() override;
	void EntIsBeingRemoved(edict_t* pEdict) override;
	int GetRCCaps() override;

	void DEClientPreThink() override;
	void DEClientPostThink() override;
	void DEClientEnter(CBasePlayer* pPlayer) override;
	void DEClientLeave() override;
	void DEClientEject() override;
	Vector GetCamRotOrg() override;

	void EXPORT APRunThink();
	void EXPORT DyingThink();
	void EXPORT FlyTouch(CBaseEntity* pOther);
	void EXPORT CrashTouch(CBaseEntity* pOther);

	void DoControl();
	void DoFlight();
	void UpdateGun();
	void FireGun();
	void DoWeapons();
	void UpdateOwnerHUD();
	void FireRocket();
	void ShowDamage();
	void ApFireBullets(Vector vecSrc, Vector vecDirShooting, Vector vecSpread);

	Vector m_vecGun;			   // 0x280, gun pitch/yaw as set on bone controllers 1/0
	Vector m_vecWantGun;		   // 0x28c, where autoaim wants the gun
	float m_fGunRefire;			   // 0x298
	float m_fRocketRefire;		   // 0x29c
	int m_iRocketsInVolly;		   // 0x2a0
	float m_fRocketVollyRefire;	   // 0x2a4
	float m_fRocketSide;		   // 0x2a8, +1/-1, alternates pods
	float m_fGunUpdateTime;		   // 0x2ac
	EHANDLE m_pLastTarget;		   // 0x2b0
	float m_fLastTargetTime;	   // 0x2b8
	int m_iClientHealth;		   // 0x2bc
	float m_flGoalSpeed;		   // 0x2c0
	float m_flGoalSide;			   // 0x2c4
	float m_flGoalZ;			   // 0x2c8
	Vector m_vecLastClientViewPos; // 0x2cc
	bool m_bForButDown;			   // 0x2d8
	bool m_bBackButDown;		   // 0x2d9
	bool m_bJumpButDown;		   // 0x2da
	bool m_bDuckButDown;		   // 0x2db
	bool m_bSLeftButDown;		   // 0x2dc
	bool m_bSRightButDown;		   // 0x2dd
	float m_fFrameTime;			   // 0x2e0
	float m_fNextCrashTouch;	   // 0x2e4
	float m_fDieTime;			   // 0x2e8
	entvars_t* m_pKiller;		   // 0x2ec
	int m_iDoSmokePuff;			   // 0x2f0
	float m_fSmokeTime;			   // 0x2f4
	int m_iExplode;				   // orig +0x2f8, sprites/fexplo.spr (CApache's name)
	int m_iBodyGibs;			   // orig +0x2fc, models/metalplategibs_green.mdl (CApache's name)
	int m_iSpriteTexture;		   // orig +0x300, sprites/white.spr (CApache's name)
	int m_iFlare;				   // orig +0x304, sprites/flare4.spr. VERIFY name
};

// orig 0x123b20
LINK_ENTITY_TO_CLASS(drive_apache, CDriveApache);

TYPEDESCRIPTION CDriveApache::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveApache, m_pOwner, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveApache, m_vecGun, FIELD_VECTOR),
		DEFINE_FIELD(CDriveApache, m_vecWantGun, FIELD_VECTOR),
		DEFINE_FIELD(CDriveApache, m_fGunRefire, FIELD_TIME),
		DEFINE_FIELD(CDriveApache, m_fRocketRefire, FIELD_TIME),
		DEFINE_FIELD(CDriveApache, m_iRocketsInVolly, FIELD_INTEGER),
		DEFINE_FIELD(CDriveApache, m_fRocketVollyRefire, FIELD_TIME),
		DEFINE_FIELD(CDriveApache, m_fRocketSide, FIELD_FLOAT),
		DEFINE_FIELD(CDriveApache, m_fGunUpdateTime, FIELD_TIME),
		DEFINE_FIELD(CDriveApache, m_pLastTarget, FIELD_EHANDLE),
		DEFINE_FIELD(CDriveApache, m_fLastTargetTime, FIELD_TIME),
		DEFINE_FIELD(CDriveApache, m_iClientHealth, FIELD_INTEGER),
		DEFINE_FIELD(CDriveApache, m_flGoalSpeed, FIELD_FLOAT),
		DEFINE_FIELD(CDriveApache, m_flGoalSide, FIELD_FLOAT),
		DEFINE_FIELD(CDriveApache, m_flGoalZ, FIELD_FLOAT),
		DEFINE_FIELD(CDriveApache, m_vecLastClientViewPos, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CDriveApache, m_bForButDown, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveApache, m_bBackButDown, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveApache, m_bJumpButDown, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveApache, m_bDuckButDown, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveApache, m_bSLeftButDown, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveApache, m_bSRightButDown, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveApache, m_fFrameTime, FIELD_FLOAT),
		DEFINE_FIELD(CDriveApache, m_fNextCrashTouch, FIELD_TIME),
		DEFINE_FIELD(CDriveApache, m_fDieTime, FIELD_TIME),
		DEFINE_FIELD(CDriveApache, m_pKiller, FIELD_EVARS),
		DEFINE_FIELD(CDriveApache, m_iDoSmokePuff, FIELD_INTEGER),
		DEFINE_FIELD(CDriveApache, m_fSmokeTime, FIELD_TIME),
};

// orig 0x123b50 (Save), 0x123ba0 (Restore)
IMPLEMENT_SAVERESTORE(CDriveApache, CBaseDriveEnt);

// orig 0x123bf0
void CDriveApache::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/apache.mdl");
	UTIL_SetSize(pev, Vector(-80, -80, -140), Vector(80, 80, 0));
	UTIL_SetOrigin(pev, pev->origin);

	pev->flags |= FL_MONSTER;
	pev->takedamage = DAMAGE_AIM;
	pev->deadflag = DEAD_NO;
	pev->health = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheHealth", 900);
	pev->max_health = pev->health;

	pev->sequence = 0;
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 0xFF);

	InitBoneControllers();

	SetThink(&CDriveApache::APRunThink);
	SetTouch(&CDriveApache::FlyTouch);
	pev->nextthink = gpGlobals->time + 0.01;

	m_fNextCrashTouch = gpGlobals->time;
	m_fGunRefire = m_fRocketRefire = gpGlobals->time;
	m_iRocketsInVolly = 0;
	m_fRocketVollyRefire = gpGlobals->time;
	m_bForButDown = m_bBackButDown = m_bJumpButDown = m_bDuckButDown = m_bSLeftButDown = m_bSRightButDown = false;
	m_flGoalSpeed = 0;
	m_flGoalSide = 0;
	m_flGoalZ = 0;
	m_fGunUpdateTime = gpGlobals->time;
	m_iDoSmokePuff = 0;
	m_fSmokeTime = gpGlobals->time;
	m_vecLastClientViewPos = g_vecZero;
	m_fRocketSide = 1;
	m_pKiller = NULL;
	m_pLastTarget = NULL;
	m_fLastTargetTime = gpGlobals->time;
	m_vecGun = m_vecWantGun = g_vecZero;

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav", 1.0, 0.3, 0, 110);
}

// orig 0x123ed0
void CDriveApache::Precache()
{
	PRECACHE_MODEL("models/apache.mdl");
	PRECACHE_SOUND("apache/ap_rotor2.wav");
	PRECACHE_SOUND("weapons/mortarhit.wav");
	PRECACHE_SOUND("turret/tu_fire1.wav");

	m_iExplode = PRECACHE_MODEL("sprites/fexplo.spr");
	m_iBodyGibs = PRECACHE_MODEL("models/metalplategibs_green.mdl");
	m_iSpriteTexture = PRECACHE_MODEL("sprites/white.spr");
	m_iFlare = PRECACHE_MODEL("sprites/flare4.spr");

	UTIL_PrecacheOther("drv_hvr_rocket");
}

// orig 0x123fa0
const char* CDriveApache::GetControllerName()
{
	if (!m_pOwner)
		return NULL;

	CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;
	if (!pOwner->m_bInDriveEnt || !pOwner->m_pCurDriveEnt || pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;

	return STRING(m_pOwner->pev->netname);
}

// orig 0x124010
void CDriveApache::APRunThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.01;

	if (m_pOwner && !FNullEnt(m_pOwner->pev) && m_pOwner->pev && m_pOwner->pev->pContainingEntity)
	{
		edict_t* pOwnerEdict = m_pOwner->pev->pContainingEntity;
		if (pOwnerEdict->pvPrivateData && !pOwnerEdict->free && UTIL_PlayerByIndex(ENTINDEX(pOwnerEdict)))
		{
			// ditched in water: the Apache dies
			if (pev->waterlevel != 0 && UTIL_PointContents(pev->origin) != CONTENTS_SKY)
			{
				pev->classname = MAKE_STRING("apache");
				m_pKiller = m_pOwner ? m_pOwner->pev : pev;
				Killed(pev, GIB_NORMAL);
				return;
			}

			m_pOwner->pev->rendermode = kRenderTransColor;
			m_pOwner->pev->renderamt = 0;
			m_pOwner->pev->effects |= EF_NODRAW;

			ShowDamage();
			DoControl();
			DoFlight();
			UpdateGun();
			DoWeapons();
			UpdateOwnerHUD();
			return;
		}
	}

	// the driver is gone
	m_pKiller = pev;
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("apache");
	Killed(pev, GIB_NORMAL);
}

// orig 0x1241e0: turn the held movement keys into goal speeds, then blend the velocity toward them.
// The vertical decel reuses the ApacheForDecel/ApacheBackDecel keys with their own defaults.
void CDriveApache::DoControl()
{
	// forward / back
	if (m_bForButDown)
	{
		m_flGoalSpeed = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheForAccel", 300) * m_fFrameTime + m_flGoalSpeed;
		if (PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxForSpeed", 500) < m_flGoalSpeed)
			m_flGoalSpeed = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxForSpeed", 500);
	}
	else if (m_bBackButDown)
	{
		m_flGoalSpeed = m_flGoalSpeed - PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheBackAccel", 240) * m_fFrameTime;
		if (m_flGoalSpeed < -PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxBackSpeed", 320))
			m_flGoalSpeed = -PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxBackSpeed", 320);
	}
	else if (m_flGoalSpeed > 0)
	{
		if (m_flGoalSpeed - PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheForDecel", 240) * m_fFrameTime < 0)
			m_flGoalSpeed = 0;
		else
			m_flGoalSpeed = m_flGoalSpeed - PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheForDecel", 240) * m_fFrameTime;
	}
	else if (m_flGoalSpeed < 0)
	{
		if (PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheBackDecel", 190) * m_fFrameTime + m_flGoalSpeed > 0)
			m_flGoalSpeed = 0;
		else
			m_flGoalSpeed = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheBackDecel", 190) * m_fFrameTime + m_flGoalSpeed;
	}

	// strafe
	if (m_bSRightButDown)
	{
		m_flGoalSide = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheSideAccel", 260) * m_fFrameTime + m_flGoalSide;
		if (PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxSideSpeed", 300) < m_flGoalSide)
			m_flGoalSide = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxSideSpeed", 300);
	}
	else if (m_bSLeftButDown)
	{
		m_flGoalSide = m_flGoalSide - PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheSideAccel", 260) * m_fFrameTime;
		if (m_flGoalSide < -PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxSideSpeed", 300))
			m_flGoalSide = -PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxSideSpeed", 300);
	}
	else if (m_flGoalSide > 0)
	{
		if (m_flGoalSide - PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheSideDecel", 200) * m_fFrameTime < 0)
			m_flGoalSide = 0;
		else
			m_flGoalSide = m_flGoalSide - PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheSideDecel", 200) * m_fFrameTime;
	}
	else if (m_flGoalSide < 0)
	{
		if (PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheSideDecel", 200) * m_fFrameTime + m_flGoalSide > 0)
			m_flGoalSide = 0;
		else
			m_flGoalSide = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheSideDecel", 200) * m_fFrameTime + m_flGoalSide;
	}

	// up / down
	if (m_bJumpButDown)
	{
		m_flGoalZ = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheUpAccel", 150) * m_fFrameTime + m_flGoalZ;
		if (PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxUpSpeed", 230) < m_flGoalZ)
			m_flGoalZ = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxUpSpeed", 230);
	}
	else if (m_bDuckButDown)
	{
		m_flGoalZ = m_flGoalZ - PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheDownAccel", 200) * m_fFrameTime;
		if (m_flGoalZ < -PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxDownSpeed", 250))
			m_flGoalZ = -PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxDownSpeed", 250);
	}
	else if (m_flGoalZ > 0)
	{
		if (m_flGoalZ - PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheForDecel", 200) * m_fFrameTime < 0)
			m_flGoalZ = 0;
		else
			m_flGoalZ = m_flGoalZ - PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheForDecel", 200) * m_fFrameTime;
	}
	else if (m_flGoalZ < 0)
	{
		if (PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheBackDecel", 150) * m_fFrameTime + m_flGoalZ > 0)
			m_flGoalZ = 0;
		else
			m_flGoalZ = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheBackDecel", 150) * m_fFrameTime + m_flGoalZ;
	}

	if (fabs(m_flGoalZ) > 0)
		pev->velocity.z = 0.5 * pev->velocity.z + 0.5 * m_flGoalZ;
	else
		pev->velocity.z = pev->velocity.z * 0.98;

	if (fabs(m_flGoalSpeed) > 0 || fabs(m_flGoalSide) > 0)
	{
		UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
		pev->velocity = pev->velocity * 0.5 + gpGlobals->v_forward * m_flGoalSpeed * 0.5 + gpGlobals->v_right * m_flGoalSide * 0.5;
	}
	else
	{
		pev->velocity.x = pev->velocity.x * 0.98;
		pev->velocity.y = pev->velocity.y * 0.98;
	}
}

// orig 0x124950: turn toward the driver's view (yaw), and pitch and roll with the goal speeds.
void CDriveApache::DoFlight()
{
	float flDiff;

	// yaw
	float flStep = m_fFrameTime * 180;
	float flYaw = UTIL_AngleMod(UTIL_AngleMod(pev->angles.y) + 0.7 * pev->avelocity.y);
	float flIdeal = pev->ideal_yaw;
	// VERIFY: always true in the original (|| where && was meant), so the dead-zone branch never runs
	if (flIdeal < flYaw + 0.5 || flYaw - 0.5 < flIdeal)
	{
		flDiff = flIdeal - flYaw;
		if (flIdeal > flYaw)
		{
			if (flDiff >= 180)
				flDiff -= 360;
		}
		else if (flDiff <= -180)
			flDiff += 360;

		if (flDiff > 0)
		{
			if (pev->avelocity.y < 190)
				pev->avelocity.y += flStep;
		}
		else if (pev->avelocity.y > -190)
			pev->avelocity.y -= flStep;
	}
	else
	{
		pev->avelocity.y = 0;
		flDiff = 0;
	}
	pev->avelocity.y = pev->avelocity.y * 0.98;
	if (fabs(flDiff) < 3)
		pev->avelocity.y = pev->avelocity.y * 0.5;
	pev->angles.y = UTIL_AngleMod(pev->angles.y);

	// pitch: nose down with forward speed, up with backward
	flStep = m_fFrameTime * 100;
	float flPitch = pev->avelocity.x * 0.6 + pev->angles.x;
	float flBase = pev->idealpitch + -5;
	UTIL_MakeVectors(pev->angles);
	if (m_flGoalSpeed > 0)
	{
		float flTilt;
		if (1 < m_flGoalSpeed / PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxForSpeed", 500))
			flTilt = 60;
		else
			flTilt = 60 * (m_flGoalSpeed / PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxForSpeed", 500));
		flIdeal = flBase - flTilt;
	}
	else
	{
		float flTilt;
		if (1 < -m_flGoalSpeed / PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxBackSpeed", 320))
			flTilt = 60;
		else
			flTilt = 60 * (-m_flGoalSpeed / PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxBackSpeed", 320));
		flIdeal = flTilt + flBase;
	}
	flPitch = (flPitch < -40) ? -40 : ((flPitch > 30) ? 30 : flPitch);
	flIdeal = (flIdeal < -40) ? -40 : ((flIdeal > 30) ? 30 : flIdeal);
	// VERIFY: always true, as for yaw
	if (flIdeal < flPitch + 0.5 || flPitch - 0.5 < flIdeal)
	{
		flDiff = flIdeal - flPitch;
		if (flDiff > 0)
		{
			if (pev->avelocity.x < 140)
				pev->avelocity.x += flStep;
		}
		else if (pev->avelocity.x > -140)
			pev->avelocity.x -= flStep;
	}
	else
	{
		pev->avelocity.x = 0;
		flDiff = 0;
	}
	pev->avelocity.x = pev->avelocity.x * 0.98;
	if (fabs(flDiff) < 3)
		pev->avelocity.x = pev->avelocity.x * 0.5;
	pev->angles.x = (pev->angles.x < -40) ? -40 : ((pev->angles.x > 30) ? 30 : pev->angles.x);

	// roll with strafe speed
	flStep = m_fFrameTime * 100;
	float flRoll = pev->avelocity.z * 0.6 + pev->angles.z;
	UTIL_MakeVectors(pev->angles);
	if (m_flGoalSide > 0)
	{
		if (1 < m_flGoalSide / PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxSideSpeed", 300))
			flIdeal = 60;
		else
			flIdeal = 60 * (m_flGoalSide / PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxSideSpeed", 300)) + 0;
	}
	else
	{
		float flTilt;
		if (1 < -m_flGoalSide / PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxSideSpeed", 300))
			flTilt = 60;
		else
			flTilt = 60 * (-m_flGoalSide / PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxSideSpeed", 300));
		flIdeal = 0 - flTilt;
	}
	flRoll = (flRoll < -30) ? -30 : ((flRoll > 30) ? 30 : flRoll);
	flIdeal = (flIdeal < -30) ? -30 : ((flIdeal > 30) ? 30 : flIdeal);
	// VERIFY: always true, as for yaw
	if (flIdeal < flRoll + 0.5 || flRoll - 0.5 < flIdeal)
	{
		flDiff = flIdeal - flRoll;
		if (flDiff > 0)
		{
			if (pev->avelocity.z < 120)
				pev->avelocity.z += flStep;
		}
		else if (pev->avelocity.z > -120)
			pev->avelocity.z -= flStep;
	}
	else
	{
		pev->avelocity.z = 0;
		flDiff = 0;
	}
	pev->avelocity.z = pev->avelocity.z * 0.98;
	if (fabs(flDiff) < 3)
		pev->avelocity.z = pev->avelocity.z * 0.5;
	pev->angles.z = (pev->angles.z < -30) ? -30 : ((pev->angles.z > 30) ? 30 : pev->angles.z);
}

// orig 0x124ff0: swing the chin gun. With ApacheGunAutoaim off it follows the driver's view; on, it
// follows m_vecWantGun, which every 0.1s is aimed at the best visible target in front.
void CDriveApache::UpdateGun()
{
	if (!m_pOwner)
		return;

	float flPitch, flYaw;

	if (!PREFS_KeyLoopUpBool(APACHE_PREFS, "ApacheGunAutoaim", true))
	{
		flPitch = m_pOwner->pev->v_angle.x;
		flYaw = UTIL_AngleMod(m_pOwner->pev->v_angle.y - pev->angles.y);
		if (flYaw > 180)
			flYaw -= 360;
		if (flYaw < -180)
			flYaw += 360;

		float flPitchStep = m_fFrameTime * 190;
		float flYawStep = m_fFrameTime * 130;
		if (m_vecGun.x < flPitch && m_vecGun.x + flPitchStep <= flPitch)
			flPitch = m_vecGun.x + flPitchStep;
		if (flPitch < m_vecGun.x && flPitch <= m_vecGun.x - flPitchStep)
			flPitch = m_vecGun.x - flPitchStep;
		if (m_vecGun.y < flYaw && m_vecGun.y + flYawStep <= flYaw)
			flYaw = m_vecGun.y + flYawStep;
		if (flYaw < m_vecGun.y && flYaw <= m_vecGun.y - flYawStep)
			flYaw = m_vecGun.y - flYawStep;

		m_vecGun.y = SetBoneController(0, flYaw);
		m_vecGun.x = SetBoneController(1, flPitch);
		return;
	}

	float flPitchStep = m_fFrameTime * 190;
	float flYawStep = m_fFrameTime * 130;
	// VERIFY: the original leaves these unset when want == gun; the natural value (want) is used
	flPitch = m_vecWantGun.x;
	flYaw = m_vecWantGun.y;
	if (m_vecGun.x < m_vecWantGun.x)
		flPitch = (m_vecGun.x + flPitchStep <= m_vecWantGun.x) ? m_vecGun.x + flPitchStep : m_vecWantGun.x;
	if (m_vecWantGun.x < m_vecGun.x)
		flPitch = (m_vecGun.x - flPitchStep < m_vecWantGun.x) ? m_vecWantGun.x : m_vecGun.x - flPitchStep;
	if (m_vecGun.y < m_vecWantGun.y)
		flYaw = (m_vecGun.y + flYawStep <= m_vecWantGun.y) ? m_vecGun.y + flYawStep : m_vecWantGun.y;
	if (m_vecWantGun.y < m_vecGun.y)
		flYaw = (m_vecGun.y - flYawStep < m_vecWantGun.y) ? m_vecWantGun.y : m_vecGun.y - flYawStep;

	m_vecGun.y = SetBoneController(0, flYaw);
	m_vecGun.x = SetBoneController(1, flPitch);

	if (gpGlobals->time < m_fGunUpdateTime)
		return;
	m_fGunUpdateTime = gpGlobals->time + 0.1;

	if (m_fLastTargetTime < gpGlobals->time)
		m_pLastTarget = NULL;

	if ((CBaseEntity*)m_pLastTarget != NULL)
	{
		bool bValid = false;
		if (m_pLastTarget->pev && !FNullEnt(m_pLastTarget->pev))
		{
			edict_t* pTargetEdict = m_pLastTarget->pev->pContainingEntity;
			if (pTargetEdict && !pTargetEdict->free && pTargetEdict->pvPrivateData)
				bValid = true;
		}
		if (!bValid)
			m_pLastTarget = NULL;
	}

	CBaseEntity* pEnt = NULL;
	CBaseEntity* pBest = NULL;
	Vector vecSrc = pev->origin;
	Vector vecGunAng;
	float flMaxAngle = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheGunAutoaimMaxAngle", 30);
	float flBestWeight = -99;

	GetAttachment(1, vecSrc, vecGunAng);

	while ((pEnt = UTIL_FindEntityInSphere(pEnt, vecSrc, 8192)) != NULL)
	{
		if (!g_pGameRules->FPlayerCanTakeDamage((CBasePlayer*)m_pOwner, pEnt))
			continue;
		if (!pEnt->IsAlive() || pEnt->edict() == edict() || pEnt->edict() == m_pOwner->edict())
			continue;
		if (pEnt->IsBSPModel() || pEnt->pev->takedamage != DAMAGE_AIM || FStringNull(pEnt->pev->model) ||
			(pEnt->pev->effects & EF_NODRAW) != 0 || (unsigned int)(pEnt->pev->solid - SOLID_BBOX) > 1 ||
			(pEnt->pev->flags & (FL_NOTARGET | FL_KILLME)) != 0)
			continue;

		Vector vecDelta = pEnt->Center() - vecSrc;
		Vector vecAng = UTIL_VecToAngles(vecDelta.Normalize());

		float flYawDiff = vecAng.y - UTIL_AngleMod(pev->angles.y);
		if (UTIL_AngleMod(pev->angles.y) < vecAng.y)
		{
			if (flYawDiff >= 180)
				flYawDiff -= 360;
		}
		else if (flYawDiff <= -180)
			flYawDiff += 360;

		float flAbs = fabs(flYawDiff);
		if (flAbs > flMaxAngle)
			continue;

		float flAngle = (flAbs < flMaxAngle) ? flAbs : flMaxAngle;
		float flWeight;
		if (vecDelta.Length() >= 1024)
			flWeight = 0;
		else
			flWeight = 1 - vecDelta.Length() * (1.0 / 1024);
		flWeight = flWeight + (1 - flAngle / flMaxAngle) * 3;

		// stick to the last target
		if ((CBaseEntity*)m_pLastTarget != NULL && pEnt->edict() == m_pLastTarget->edict())
			flWeight = flWeight + flWeight;

		if (flWeight < flBestWeight)
			continue;

		// can the gun pitch reach it?
		float flReach = UTIL_AngleMod(SetBoneController(1, UTIL_AngleMod(vecAng.x - pev->angles.x)) + pev->angles.x);
		SetBoneController(1, m_vecGun.x);
		if (vecAng.x < flReach - 1 || flReach + 1 < vecAng.x)
			continue;

		TraceResult tr;
		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine(vecSrc, pEnt->Center(), dont_ignore_monsters, edict(), &tr);
		if (tr.flFraction >= 1.0 || tr.pHit == pEnt->edict())
		{
			flBestWeight = flWeight;
			pBest = pEnt;
		}
	}

	Vector vecWant; // VERIFY: z is left uninitialized in the original when there is no target; nothing reads it
	if (!pBest)
	{
		vecWant.x = m_pOwner->pev->v_angle.x;
		vecWant.y = UTIL_AngleMod(m_pOwner->pev->v_angle.y - pev->angles.y);
		if (vecWant.y > 180)
			vecWant.y -= 360;
		if (vecWant.y < -180)
			vecWant.y += 360;
	}
	else
	{
		Vector vecDir = (pBest->Center() - vecSrc).Normalize();
		UTIL_MakeAimVectors(pev->angles);
		Vector vecLocal(DotProduct(gpGlobals->v_forward, vecDir), -DotProduct(gpGlobals->v_right, vecDir), DotProduct(gpGlobals->v_up, vecDir));
		Vector vecAng = UTIL_VecToAngles(vecLocal);

		vecWant.y = vecAng.y;
		vecWant.z = vecAng.z;
		vecWant.x = -vecAng.x;
		if (vecWant.y > 180)
			vecWant.y -= 360;
		if (vecWant.y < -180)
			vecWant.y += 360;
		if (vecWant.x > 180)
			vecWant.x -= 360;
		if (vecWant.x < -180)
			vecWant.x += 360;

		m_pLastTarget = pBest;
		m_fLastTargetTime = gpGlobals->time + 0.5;
	}
	m_vecWantGun = vecWant;
}

// orig 0x125ce0
void CDriveApache::FireGun()
{
	if (!m_pOwner)
		return;

	Vector vecGun, vecAng, vecMuzzle, vecAng2;
	GetAttachment(1, vecGun, vecAng);
	GetAttachment(0, vecMuzzle, vecAng2);
	Vector vecDir = (vecMuzzle - vecGun).Normalize();

	Vector vecFlash = vecMuzzle - Vector(0, 0, 10);
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecFlash);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(vecFlash.x);
	WRITE_COORD(vecFlash.y);
	WRITE_COORD(vecFlash.z);
	WRITE_SHORT(m_iFlare);
	WRITE_BYTE(5);	// scale * 10
	WRITE_BYTE(50); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "turret/tu_fire1.wav", 1.0, 0.3, 0, 100);

	// the kill message names the weapon by the inflictor's classname
	pev->classname = MAKE_STRING("apache_gun");
	ApFireBullets(vecGun, vecDir, Vector(0.0349, 0.0349, 0.0349));
	pev->classname = MAKE_STRING("drive_apache");
}

// orig 0x125f40
void CDriveApache::DoWeapons()
{
	if (!m_pOwner)
		return;

	CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;

	if ((m_pOwner->pev->button & IN_ATTACK) != 0 && m_fGunRefire < gpGlobals->time)
	{
		m_fGunRefire = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheGunRefire", 0.1) + gpGlobals->time;
		FireGun();
		pOwner->m_fDrvEntEjectTime = gpGlobals->time + 2.0;
	}

	if (m_iRocketsInVolly < 1)
	{
		if ((m_pOwner->pev->button & IN_ATTACK2) != 0 && m_fRocketRefire < gpGlobals->time)
		{
			m_fRocketRefire = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheRocketNextRefire", 8) + gpGlobals->time;
			m_iRocketsInVolly = PREFS_KeyLoopUpInt(APACHE_PREFS, "ApacheRocketNumInVolly", 4);
			m_fRocketVollyRefire = gpGlobals->time;
			pOwner->m_fDrvEntEjectTime = gpGlobals->time + 3.0;
		}
	}
	else if (m_fRocketVollyRefire < gpGlobals->time)
	{
		m_fRocketVollyRefire = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheRocketVollyRefire", 0.3) + gpGlobals->time;
		FireRocket();
		m_iRocketsInVolly--;
	}
}

// orig 0x1260f0: the driver's health display shows the Apache's health in percent
void CDriveApache::UpdateOwnerHUD()
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
		WRITE_BYTE(iHealth);
		MESSAGE_END();

		m_iClientHealth = (int)pev->health;
	}
}

// orig 0x126210
void CDriveApache::FireRocket()
{
	UTIL_MakeAimVectors(pev->angles);
	Vector vecSrc = pev->origin + (gpGlobals->v_forward * 21 + gpGlobals->v_right * m_fRocketSide * 70 + gpGlobals->v_up * -79) * 1.5;

	switch (RANDOM_LONG(0, 5) % 5)
	{
	case 0:
		vecSrc = vecSrc + gpGlobals->v_right * 10;
		break;
	case 1:
		vecSrc = vecSrc - gpGlobals->v_right * 10;
		break;
	case 2:
		vecSrc = vecSrc + gpGlobals->v_up * 10;
		break;
	case 3:
		vecSrc = vecSrc - gpGlobals->v_up * 10;
		break;
	case 4:
		break;
	}

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_SMOKE);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_SHORT(g_sModelIndexSmoke);
	WRITE_BYTE(20); // scale * 10
	WRITE_BYTE(12); // framerate
	MESSAGE_END();

	CBaseEntity* pRocket = CBaseEntity::Create("drv_hvr_rocket", vecSrc, pev->angles, edict());
	if (pRocket)
	{
		pRocket->pev->velocity = pev->velocity + gpGlobals->v_forward * 100;
		pRocket->pev->owner = edict();
		// the driver steers it (CDrvApacheHVR::AccelerateThink) and is credited for it
		pRocket->pev->dmg_inflictor = (m_pOwner ? m_pOwner->pev : pev)->pContainingEntity;
	}

	m_fRocketSide = -m_fRocketSide;
}

// orig 0x126620: smoke puffs after heavy hits, and at random when badly damaged
void CDriveApache::ShowDamage()
{
	if ((m_iDoSmokePuff > 0 && m_fSmokeTime < gpGlobals->time) || (float)RANDOM_LONG(0, 200) > pev->health)
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_SMOKE);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z - 32);
		WRITE_SHORT(g_sModelIndexSmoke);
		WRITE_BYTE(RANDOM_LONG(0, 9) + 20); // scale * 10
		WRITE_BYTE(12);						// framerate
		MESSAGE_END();
	}

	if (m_iDoSmokePuff > 0 && m_fSmokeTime < gpGlobals->time)
	{
		m_iDoSmokePuff--;
		m_fSmokeTime = gpGlobals->time + 0.1;
	}
}

// orig 0x1267c0: one bullet, FireBullets style, with a tracer, credited to the driver
void CDriveApache::ApFireBullets(Vector vecSrc, Vector vecDirShooting, Vector vecSpread)
{
	TraceResult tr;
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;

	ClearMultiDamage();
	gMultiDamage.type = DMG_BULLET | DMG_NEVERGIB;

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
			pEntity->TraceAttack(m_pOwner ? m_pOwner->pev : pev, PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheDamagePerBullet", 12), vecDir, &tr, DMG_BULLET);

			if (pEntity->IsBSPModel() && CVAR_GET_FLOAT("sv_lowlag") != 1)
			{
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, BULLET_MONSTER_12MM);
				UTIL_GunshotDecalTrace(&tr, pEntity->DamageDecal(DMG_BULLET));
			}
		}
	}

	UTIL_BubbleTrail(vecSrc, tr.vecEndPos, (int)((8192 * tr.flFraction) / 64.0));

	ApplyMultiDamage(pev, m_pOwner ? m_pOwner->pev : pev);
}

// orig 0x126cc0
bool CDriveApache::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// our own rockets
	if (pevInflictor->owner == pev->pContainingEntity)
		return false;

	if (m_pOwner)
	{
		// an Apache rocket fired by our own driver
		if (pevInflictor != pev && !strcmp(STRING(pevInflictor->classname), "apache_rocket"))
		{
			if (pevInflictor->owner == m_pOwner->pev->pContainingEntity)
				return false;
			if (pevAttacker == m_pOwner->pev)
				return false;
		}

		if (m_pOwner)
		{
			CBaseEntity* pAttacker = CBaseEntity::Instance(pevAttacker);
			if (pAttacker && !g_pGameRules->FPlayerCanTakeDamage((CBasePlayer*)m_pOwner, pAttacker))
				return false;

			if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
				return false;
		}
	}

	if (flDamage > 75)
	{
		float flPuffs = flDamage / 5.0 + (m_iDoSmokePuff + 3);
		m_iDoSmokePuff = (flPuffs > 0) ? (int)flPuffs : 0;
	}

	if ((bitsDamageType & DMG_BLAST) != 0)
		flDamage = flDamage * 1.8;

	return CBaseEntity::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x126eb0: bullets and clubs off the armored hitgroup 6 do nothing; the body takes 80%
void CDriveApache::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (ptr->iHitgroup == 6 && (bitsDamageType & (DMG_ENERGYBEAM | DMG_BULLET | DMG_CLUB)) != 0)
		return;

	if ((unsigned int)(ptr->iHitgroup - 1) < 2)
	{
		AddMultiDamage(pevAttacker, this, flDamage, bitsDamageType);
		float flPuffs = flDamage / 5.0 + (m_iDoSmokePuff + 3);
		m_iDoSmokePuff = (flPuffs > 0) ? (int)flPuffs : 0;
	}
	else
	{
		AddMultiDamage(pevAttacker, this, flDamage * 0.8, bitsDamageType);
	}

	UTIL_Ricochet(ptr->vecEndPos, 2.0);
	UTIL_Sparks(ptr->vecEndPos);
}

// orig 0x126f90: fall burning until it hits something or m_fDieTime passes, then blow up
void CDriveApache::DyingThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	pev->avelocity = pev->avelocity * 1.02;

	if (gpGlobals->time < m_fDieTime)
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_EXPLOSION);
		WRITE_COORD(pev->origin.x + RANDOM_FLOAT(-150, 150));
		WRITE_COORD(pev->origin.y + RANDOM_FLOAT(-150, 150));
		WRITE_COORD(pev->origin.z + RANDOM_FLOAT(-150, -50));
		WRITE_SHORT(g_sModelIndexFireball);
		WRITE_BYTE(RANDOM_LONG(0, 29) + 30); // scale * 10
		WRITE_BYTE(12);						 // framerate
		WRITE_BYTE(TE_EXPLFLAG_NONE);
		MESSAGE_END();

		if (CVAR_GET_FLOAT("sv_lowlag") != 1)
		{
			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
			WRITE_BYTE(TE_SMOKE);
			WRITE_COORD(pev->origin.x + RANDOM_FLOAT(-150, 150));
			WRITE_COORD(pev->origin.y + RANDOM_FLOAT(-150, 150));
			WRITE_COORD(pev->origin.z + RANDOM_FLOAT(-150, -50));
			WRITE_SHORT(g_sModelIndexSmoke);
			WRITE_BYTE(100); // scale * 10
			WRITE_BYTE(10);	 // framerate
			MESSAGE_END();

			Vector vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSpot);
			WRITE_BYTE(TE_BREAKMODEL);
			WRITE_COORD(vecSpot.x);
			WRITE_COORD(vecSpot.y);
			WRITE_COORD(vecSpot.z);
			WRITE_COORD(400); // size
			WRITE_COORD(400);
			WRITE_COORD(132);
			WRITE_COORD(pev->velocity.x); // velocity
			WRITE_COORD(pev->velocity.y);
			WRITE_COORD(pev->velocity.z);
			WRITE_BYTE(50); // randomization
			WRITE_SHORT(m_iBodyGibs);
			WRITE_BYTE(4);	// count
			WRITE_BYTE(30); // life, 0.1s
			WRITE_BYTE(BREAK_METAL);
			MESSAGE_END();
		}

		pev->flags &= ~FL_ONGROUND;
		pev->nextthink = gpGlobals->time + 0.2;
		return;
	}

	Vector vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSpot);
	WRITE_BYTE(TE_SPRITE);
	WRITE_COORD(vecSpot.x);
	WRITE_COORD(vecSpot.y);
	WRITE_COORD(vecSpot.z + 256);
	WRITE_SHORT(m_iExplode);
	WRITE_BYTE(120); // scale * 10
	WRITE_BYTE(255); // brightness
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSpot);
	WRITE_BYTE(TE_SMOKE);
	WRITE_COORD(vecSpot.x);
	WRITE_COORD(vecSpot.y);
	WRITE_COORD(vecSpot.z + 512);
	WRITE_SHORT(g_sModelIndexSmoke);
	WRITE_BYTE(250); // scale * 10
	WRITE_BYTE(5);	 // framerate
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 2000); // reach damage radius over .2 seconds
	WRITE_SHORT(m_iSpriteTexture);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(0);	 // framerate
	WRITE_BYTE(4);	 // life
	WRITE_BYTE(32);	 // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(255); // r, g, b
	WRITE_BYTE(255);
	WRITE_BYTE(192);
	WRITE_BYTE(128); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/mortarhit.wav", 1.0, 0.3, 0, 100);

	pev->classname = MAKE_STRING("apache");
	if (m_pOwner && ((CBasePlayer*)m_pOwner)->m_bInDriveEnt)
		RadiusDamage(pev->origin, pev, m_pOwner->pev, 300, CLASS_NONE, DMG_BLAST);
	else
		RadiusDamage(pev->origin, pev, pev, 300, CLASS_NONE, DMG_BLAST);

	if (CVAR_GET_FLOAT("sv_lowlag") != 1)
	{
		vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSpot);
		WRITE_BYTE(TE_BREAKMODEL);
		WRITE_COORD(vecSpot.x);
		WRITE_COORD(vecSpot.y);
		WRITE_COORD(vecSpot.z + 64);
		WRITE_COORD(400); // size
		WRITE_COORD(400);
		WRITE_COORD(128);
		WRITE_COORD(0); // velocity
		WRITE_COORD(0);
		WRITE_COORD(200);
		WRITE_BYTE(30); // randomization
		WRITE_SHORT(m_iBodyGibs);
		WRITE_BYTE(100); // count
		WRITE_BYTE(50);	 // life, 0.1s
		WRITE_BYTE(BREAK_METAL);
		MESSAGE_END();
	}

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;

	// the pickup that made us schedules its respawn
	if (pev->owner)
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	DEClientLeave();

	if (m_pOwner)
	{
		CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;
		if (pOwner->m_bInDriveEnt && pOwner->m_pCurDriveEnt && pOwner->m_pCurDriveEnt->pev == pev)
		{
			// the driver goes down with it
			UTIL_SetOrigin(m_pOwner->pev, pOwner->m_pCurDriveEnt->pev->origin + Vector(0, 0, 64));
			m_pOwner->pev->effects |= EF_NODRAW;
			m_pOwner->pev->health = -1;
			g_pevLastInflictor = pev;
			m_pOwner->Killed(m_pKiller ? m_pKiller : pev, GIB_NORMAL);
		}
	}
}

// orig 0x127c10: hitting the world (or another Apache) hurts and bounces
void CDriveApache::FlyTouch(CBaseEntity* pOther)
{
	TraceResult tr = UTIL_GetGlobalTrace();

	if (pOther->pev->solid != SOLID_BSP && strcmp(STRING(pOther->pev->classname), "drive_apache") != 0 &&
		strcmp(STRING(pOther->pev->classname), "pickup_drive_apache") != 0)
		return;

	if (m_fNextCrashTouch < gpGlobals->time && UTIL_PointContents(tr.vecEndPos) != CONTENTS_SKY)
	{
		float flSpeed = pev->velocity.Length();
		pev->velocity = pev->velocity + tr.vecPlaneNormal * (flSpeed + 300);

		TakeDamage(pev, m_pOwner->pev, 400, DMG_CRUSH);

		Vector vecSpot = pev->origin;
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSpot);
		WRITE_BYTE(TE_EXPLOSION);
		WRITE_COORD(vecSpot.x + RANDOM_FLOAT(-72, 72));
		WRITE_COORD(vecSpot.y + RANDOM_FLOAT(-72, 72));
		WRITE_COORD(vecSpot.z + RANDOM_FLOAT(-72, 50));
		WRITE_SHORT(g_sModelIndexFireball);
		WRITE_BYTE(RANDOM_LONG(0, 8) + 20); // scale * 10
		WRITE_BYTE(12);						// framerate
		WRITE_BYTE(TE_EXPLFLAG_NONE);
		MESSAGE_END();

		m_fNextCrashTouch = gpGlobals->time + 0.1;
	}
}

// orig 0x127eb0
void CDriveApache::CrashTouch(CBaseEntity* pOther)
{
	SetTouch(NULL);
	m_fDieTime = gpGlobals->time - 1.0;
	pev->nextthink = gpGlobals->time;
}

// orig 0x127f00
void CDriveApache::Killed(entvars_t* pevAttacker, int iGib)
{
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 0.3;

	STOP_SOUND(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav");

	UTIL_SetSize(pev, Vector(-32, -32, -64), Vector(32, 32, 0));
	SetThink(&CDriveApache::DyingThink);
	SetTouch(&CDriveApache::CrashTouch);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;

	if (m_pOwner)
	{
		CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;
		if (pOwner->m_bInDriveEnt && pOwner->m_pCurDriveEnt && pOwner->m_pCurDriveEnt->pev == pev)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->edict());
			WRITE_BYTE(HIDEHUD_ALL);
			MESSAGE_END();

			MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->edict());
			WRITE_BYTE(0);
			MESSAGE_END();
		}
	}

	m_pKiller = pevAttacker;
	m_fDieTime = gpGlobals->time + 15.0;
}

// orig 0x128100: the driver's camera orbits the Apache along his view; his keys set the controls
void CDriveApache::DEClientPreThink()
{
	CBasePlayer* pOwner = (CBasePlayer*)m_pOwner;

	// holding reload frees the view without turning the Apache
	if ((m_pOwner->pev->button & IN_RELOAD) == 0)
	{
		pev->ideal_yaw = m_pOwner->pev->v_angle.y;
		pev->idealpitch = -m_pOwner->pev->v_angle.x;
	}

	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;

	UTIL_MakeVectors(m_pOwner->pev->v_angle);
	Vector vecOrg = GetCamRotOrg();
	Vector vecEnd = vecOrg - gpGlobals->v_forward * 512;

	TraceResult tr;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine(vecOrg, vecEnd, ignore_monsters, edict(), &tr);

	Vector vecCam = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;

	// step back toward the Apache until the camera is out of the wall
	int i;
	for (i = 1; i < 48; i++)
	{
		if (UTIL_PointContents(vecCam) == CONTENTS_EMPTY)
			break;
		vecCam = tr.vecEndPos + (float)i * gpGlobals->v_forward * 8;
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

	UTIL_SetOrigin(m_pOwner->pev, vecCam);
	m_pOwner->pev->velocity = pev->velocity;
	m_vecLastClientViewPos = vecCam;

	if ((pOwner->m_afButtonPressed & IN_FORWARD) != 0)
	{
		m_bForButDown = true;
		m_bBackButDown = false;
	}
	else if ((pOwner->m_afButtonPressed & IN_BACK) != 0)
	{
		m_bForButDown = false;
		m_bBackButDown = true;
	}

	if ((pOwner->m_afButtonPressed & IN_JUMP) != 0)
	{
		m_bJumpButDown = true;
		m_bDuckButDown = false;
	}
	else if ((pOwner->m_afButtonPressed & IN_DUCK) != 0)
	{
		m_bJumpButDown = false;
		m_bDuckButDown = true;
	}

	if ((pOwner->m_afButtonPressed & IN_MOVELEFT) != 0)
	{
		m_bSLeftButDown = true;
		m_bSRightButDown = false;
	}
	else if ((pOwner->m_afButtonPressed & IN_MOVERIGHT) != 0)
	{
		m_bSLeftButDown = false;
		m_bSRightButDown = true;
	}

	if ((pOwner->m_afButtonReleased & IN_FORWARD) != 0)
		m_bForButDown = false;
	if ((pOwner->m_afButtonReleased & IN_BACK) != 0)
		m_bBackButDown = false;
	if ((pOwner->m_afButtonReleased & IN_JUMP) != 0)
		m_bJumpButDown = false;
	if ((pOwner->m_afButtonReleased & IN_DUCK) != 0)
		m_bDuckButDown = false;
	if ((pOwner->m_afButtonReleased & IN_MOVELEFT) != 0)
		m_bSLeftButDown = false;
	if ((pOwner->m_afButtonReleased & IN_MOVERIGHT) != 0)
		m_bSRightButDown = false;

	pOwner->m_afPhysicsFlags = 0;

	// cl_de_alpha: tell the client to draw the vehicle see-through
	float flAlpha = pOwner->m_fDrvEntAlpha;
	if (flAlpha >= 1 || !(flAlpha > 0))
	{
		m_pOwner->pev->euser2 = NULL;
		m_pOwner->pev->iuser3 = 0;
		m_pOwner->pev->fuser1 = 0;
		m_pOwner->pev->fuser2 = 0;
	}
	else
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
	}
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
}

// orig 0x1286d0
void CDriveApache::DEClientPostThink()
{
	((CBasePlayer*)m_pOwner)->m_afPhysicsFlags = 0;
}

// orig 0x1286f0: the player becomes an invisible camera riding the Apache
void CDriveApache::DEClientEnter(CBasePlayer* pPlayer)
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

// orig 0x128aa0: give the driver his body back. m_bInDriveEnt and m_pCurDriveEnt stay set;
// the callers clear them.
void CDriveApache::DEClientLeave()
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

	if (pOwner->m_pActiveItem)
		pOwner->m_pActiveItem->Deploy();

	m_pOwner->pev->euser2 = NULL;
	m_pOwner->pev->iuser3 = 0;
	m_pOwner->pev->fuser1 = 0;
	m_pOwner->pev->fuser2 = 0;
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;

	pOwner->m_iHideHUD &= ~(HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT);
}

// orig 0x128d60: bail out backward; the Apache is destroyed
void CDriveApache::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool(APACHE_PREFS, "AllowEject", true))
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

	UTIL_MakeAimVectors(pev->angles);
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin - gpGlobals->v_forward * 320, ignore_monsters, ENT(pev), &tr);

	DEClientLeave();
	pOwner->UpdateClientData();
	pOwner->m_bInDriveEnt = false;
	Killed(pev, GIB_NORMAL);

	Vector vecDest = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecDest = tr.vecEndPos + tr.vecPlaneNormal * 64;
	UTIL_SetOrigin(m_pOwner->pev, vecDest);

	m_pOwner->pev->velocity = gpGlobals->v_forward * -900;
	m_pOwner->pev->v_angle = pev->angles;
	m_pOwner->pev->angles = m_pOwner->pev->v_angle;
	m_pOwner->pev->angles.x = m_pOwner->pev->v_angle.x = -m_pOwner->pev->v_angle.x;
	m_pOwner->pev->angles.z = m_pOwner->pev->v_angle.z = 0;
	m_pOwner->pev->fixangle = 1;
	pOwner->m_pCurDriveEnt = NULL;

	EMIT_SOUND_DYN(m_pOwner->edict(), CHAN_BODY, "ambience/biggun3.wav", 1.0, 0.8, 0, 100);
}

//=========================================================
// CDrvApacheHVR: the Apache's rocket, steered by the driver's view
//=========================================================
class CDrvApacheHVR : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	int GetRCCaps() override;

	void EXPORT IgniteThink();
	void EXPORT AccelerateThink();
	void EXPORT DHVRTouch(CBaseEntity* pOther);

	int m_iTrail;			// orig +0x280, sprites/smoke.spr (CApacheHVR's name)
	Vector m_vecForward;	// 0x284, current heading
	Vector m_vecOrgForward; // orig +0x290, heading at launch. VERIFY name
};

// orig 0x129090
LINK_ENTITY_TO_CLASS(drv_hvr_rocket, CDrvApacheHVR);

TYPEDESCRIPTION CDrvApacheHVR::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvApacheHVR, m_vecForward, FIELD_VECTOR),
};

// orig 0x1290c0 (Save), 0x129110 (Restore). The original chains to CBaseMonster's, as CGrenade has none.
IMPLEMENT_SAVERESTORE(CDrvApacheHVR, CGrenade);

// orig 0x129160
void CDrvApacheHVR::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/HVR.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);

	SetThink(&CDrvApacheHVR::IgniteThink);
	SetTouch(&CDrvApacheHVR::DHVRTouch);

	UTIL_MakeAimVectors(pev->angles);
	m_vecForward = gpGlobals->v_forward;
	m_vecOrgForward = gpGlobals->v_forward;
	pev->gravity = 0.5;

	pev->nextthink = gpGlobals->time + 0.1;

	pev->dmg = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheRocketDamage", 150);
}

// orig 0x1292c0
void CDrvApacheHVR::Precache()
{
	PRECACHE_MODEL("models/HVR.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND("weapons/rocket1.wav");
}

// orig 0x129320
void CDrvApacheHVR::IgniteThink()
{
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5, 0, 100);

	// rocket trail
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex()); // entity
	WRITE_SHORT(m_iTrail);	 // model
	WRITE_BYTE(15);			 // life
	WRITE_BYTE(5);			 // width
	WRITE_BYTE(224);		 // r, g, b
	WRITE_BYTE(224);
	WRITE_BYTE(255);
	WRITE_BYTE(255); // brightness
	MESSAGE_END();

	// set to accelerate
	SetThink(&CDrvApacheHVR::AccelerateThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x129490: pev->dmg_inflictor is the driver; while he looks within 90 degrees of the
// heading, the heading bends toward his view
void CDrvApacheHVR::AccelerateThink()
{
	// check world boundaries
	if (pev->origin.x < -4096 || pev->origin.x > 4096 || pev->origin.y < -4096 || pev->origin.y > 4096 || pev->origin.z < -4096 || pev->origin.z > 4096)
	{
		UTIL_Remove(this);
		return;
	}

	if (pev->dmg_inflictor)
	{
		UTIL_MakeVectors(pev->dmg_inflictor->v.v_angle);
		float flDot = DotProduct(gpGlobals->v_forward, m_vecForward);
		if (flDot > 0)
		{
			float flKeep = (1 - flDot > 0.9) ? 0.9 : 1 - flDot;
			if (!(flDot > 0.1))
				flDot = 0.1;
			m_vecForward = m_vecOrgForward * flKeep + gpGlobals->v_forward * flDot;
		}
	}

	// accelerate
	float flSpeed = pev->velocity.Length();
	if (flSpeed < 1800)
		pev->velocity = pev->velocity + m_vecForward * 200;

	// re-aim
	pev->angles = UTIL_VecToAngles(pev->velocity);

	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x129720
void CDrvApacheHVR::DHVRTouch(CBaseEntity* pOther)
{
	// credit the driver, and name the weapon in the kill message
	pev->owner = pev->dmg_inflictor;
	pev->classname = MAKE_STRING("apache_rocket");
	STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav");
	ExplodeTouch(pOther);
}

// orig 0x12a740
int CDrvApacheHVR::GetRCCaps()
{
	return 0x60800000;
}

//=========================================================
// CDriveApachePickup: the parked Apache a player uses to get in
//=========================================================
class CDriveApachePickup : public CBaseMonster
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
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	int BloodColor() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	Vector Center() override;
	int GetRCCaps() override;

	CBaseEntity* GetSpawnOrg();
	void EXPORT PDAAnimThink();
	void EXPORT DoRespawn();

	bool m_bHoverSpawn;			 // 0x27c, spawned in the air (no ground within 128 below the spot)
	Vector m_vecBaseAng;		 // 0x280
	CBaseEntity* m_pGivenApache; // 0x28c, the drive_apache made from this pickup
	bool m_bTakeable;			 // 0x290
};

// orig 0x1297a0
LINK_ENTITY_TO_CLASS(pickup_drive_apache, CDriveApachePickup);

TYPEDESCRIPTION CDriveApachePickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveApachePickup, m_bHoverSpawn, FIELD_BOOLEAN),
		DEFINE_FIELD(CDriveApachePickup, m_vecBaseAng, FIELD_VECTOR),
		DEFINE_FIELD(CDriveApachePickup, m_pGivenApache, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveApachePickup, m_bTakeable, FIELD_BOOLEAN),
};

// orig 0x1297d0 (Save), 0x129820 (Restore)
IMPLEMENT_SAVERESTORE(CDriveApachePickup, CBaseMonster);

// orig 0x129870: move to a free info_drive_apache_spawn (and use it up), else go away
void CDriveApachePickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/apache.mdl");
	UTIL_SetSize(pev, Vector(-80, -80, -140), Vector(80, 80, 0));
	UTIL_SetOrigin(pev, pev->origin);

	pev->takedamage = DAMAGE_YES;
	pev->health = 1;

	pev->sequence = 0;
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 0xFF);
	InitBoneControllers();

	SetUse(&CDriveApachePickup::Use);
	SetThink(&CDriveApachePickup::PDAAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;

	m_bTakeable = true;
	m_pGivenApache = NULL;

	CBaseEntity* pSpot = GetSpawnOrg();
	if (!pSpot)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	Vector vecSpot = pSpot->Center();
	pev->angles = Vector(0, pSpot->pev->angles.y, 0);

	TraceResult tr;
	UTIL_TraceLine(vecSpot, vecSpot - Vector(0, 0, 128), ignore_monsters, edict(), &tr);
	if (tr.flFraction >= 1.0)
	{
		m_bHoverSpawn = true;
		UTIL_SetOrigin(pev, vecSpot);
	}
	else
	{
		// parked on the ground
		m_bHoverSpawn = false;
		UTIL_SetOrigin(pev, tr.vecEndPos + Vector(0, 0, 172));
	}

	if (!m_bHoverSpawn)
		pev->framerate = 0.3;
	else
		pev->framerate = 1.0;

	m_vecBaseAng = pev->angles;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;

	DoRespawn();
	UTIL_Remove(pSpot);
}

// orig 0x129b80
void CDriveApachePickup::Precache()
{
	PRECACHE_MODEL("models/apache.mdl");
	PRECACHE_SOUND("apache/ap_rotor2.wav");
	PRECACHE_SOUND("apache/ap_rotor4.wav");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_apache");
}

// orig 0x129bf0
CBaseEntity* CDriveApachePickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_apache_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (FNullEnt(pSpot->pev))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x129c70
bool CDriveApachePickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return true;
}

// orig 0x129c80
void CDriveApachePickup::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (ptr->iHitgroup != 6 || (bitsDamageType & (DMG_ENERGYBEAM | DMG_BULLET | DMG_CLUB)) == 0)
		UTIL_Ricochet(ptr->vecEndPos, 1.4);
}

// orig 0x129cc0: fade in after a respawn; while taken, end the drive after ApacheMaxDriveTime
void CDriveApachePickup::PDAAnimThink()
{
	float flInterval = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_bTakeable)
	{
		if (m_pGivenApache && m_pGivenApache->pev->deadflag == DEAD_NO && pev->dmgtime < gpGlobals->time)
		{
			g_pevLastInflictor = pev;
			m_pGivenApache->Killed(pev, GIB_NORMAL);
			pev->dmgtime = gpGlobals->time + 1e+06;
		}
		if (!m_bTakeable)
			return;
	}

	if (pev->rendermode != kRenderTransTexture || pev->renderamt >= 255)
	{
		pev->rendermode = kRenderNormal;
		pev->renderamt = 255;
	}
	else
	{
		pev->renderamt = flInterval * 96.37188 + pev->renderamt;
		if (pev->renderamt >= 255)
		{
			pev->rendermode = kRenderNormal;
			pev->renderamt = 255;
			STOP_SOUND(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav");
		}
	}
}

// orig 0x129e40: a walking player gets in. It reads pCaller, not pActivator (the same for +use).
void CDriveApachePickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
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

	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_apache", pev->origin, pev->angles, edict());
	pCaller->pev->angles = Vector(0, pev->angles.y, 0);
	pCaller->pev->v_angle = pCaller->pev->angles;
	pCaller->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGivenApache = pPlayer->m_pCurDriveEnt;
	if (!m_bHoverSpawn)
	{
		// parked: lift off
		((CDriveApache*)m_pGivenApache)->m_flGoalZ = 200;
		STOP_SOUND(ENT(pev), CHAN_STATIC, "apache/ap_rotor4.wav");
	}
	else
	{
		STOP_SOUND(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav");
	}

	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDriveApachePickup::PDAAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheMaxDriveTime", 360) + gpGlobals->time;
}

// orig 0x12a090: wait until nothing alive stands in the way, then reappear
void CDriveApachePickup::DoRespawn()
{
	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityInSphere(pEnt, pev->origin, 180)) != NULL)
	{
		if (pEnt->IsAlive() && !pEnt->IsBSPModel() && (pEnt->IsPlayer() || (pEnt->pev->flags & FL_MONSTER) != 0))
		{
			StudioFrameAdvance();
			pev->nextthink = gpGlobals->time + 0.1;
			SetThink(&CDriveApachePickup::DoRespawn);
			return;
		}
	}

	if (!m_bHoverSpawn)
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_rotor4.wav", 0.35, 0.6, 0, 110);
	else
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav", 0.8, 0.4, 0, 110);

	m_bTakeable = true;
	m_pGivenApache = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;

	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);

	SetThink(&CDriveApachePickup::PDAAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x12a240: our drive_apache died (CDriveApache::DyingThink)
void CDriveApachePickup::DeathNotice(entvars_t* pevChild)
{
	if (!m_bTakeable)
	{
		pev->nextthink = PREFS_KeyLoopUpFloat(APACHE_PREFS, "ApacheRespawnDelayAfterDeath", 10) + gpGlobals->time;
		SetThink(&CDriveApachePickup::DoRespawn);
	}
}

// orig 0x12a570
int CDriveApachePickup::ObjectCaps()
{
	if (m_bTakeable)
		return FCAP_IMPULSE_USE | FCAP_ACROSS_TRANSITION;
	return FCAP_ACROSS_TRANSITION;
}

// orig 0x12a590
void CDriveApachePickup::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector(-300, -300, -172);
	pev->absmax = pev->origin + Vector(300, 300, 8);
}

// orig 0x12a660
int CDriveApachePickup::GetRCCaps()
{
	return 0x1c000;
}

// orig 0x12a670
Vector CDriveApachePickup::Center()
{
	return pev->origin - Vector(0, 0, 100);
}

// orig 0x12a6d0
int CDriveApachePickup::BloodColor()
{
	return DONT_BLEED;
}

// orig 0x12a6e0
int CDriveApachePickup::Classify()
{
	return CLASS_HUMAN_MILITARY;
}

//=========================================================
// CDrvASpawner: places mp_driveapache pickups 15 seconds into the map
//=========================================================
class CDrvASpawner : public CBaseEntity
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

// orig 0x12a2b0
void CreateDrvApachSpawner()
{
	if (g_pGameRules->IsMultiplayer() && !UTIL_FindEntityByClassname(NULL, "drive_apache_spawner"))
		CBaseEntity::Create("drive_apache_spawner", g_vecZero, g_vecZero, NULL);
}

// orig 0x12a310
LINK_ENTITY_TO_CLASS(drive_apache_spawner, CDrvASpawner);

TYPEDESCRIPTION CDrvASpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvASpawner, m_bDummy, FIELD_BOOLEAN),
};

// orig 0x12a340 (Save), 0x12a390 (Restore)
IMPLEMENT_SAVERESTORE(CDrvASpawner, CBaseEntity);

// orig 0x12a3e0
void CDrvASpawner::DrvAThink()
{
	int iCount = (int)CVAR_GET_FLOAT("mp_driveapache");
	for (int i = 0; i < iCount; i++)
		CBaseEntity::Create("pickup_drive_apache", g_vecZero, g_vecZero, NULL);

	UTIL_Remove(this);
}

// orig 0x12a470
void CDrvASpawner::Spawn()
{
	pev->classname = MAKE_STRING("drive_apache_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvASpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15.0;
}

// orig 0x12a510
int CDrvASpawner::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}

//=========================================================
// CDriveApache's small virtuals (the original emits them after the pickup's)
//=========================================================

// orig 0x12a7a0
void CDriveApache::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;

	if ((CBaseEntity*)m_pLastTarget != NULL && m_pLastTarget->pev->pContainingEntity == pEdict)
		m_pLastTarget = NULL;

	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

// orig 0x12a850
float CDriveApache::VeryHeavyDamageFract()
{
	return 0.4;
}

// orig 0x12a870
float CDriveApache::ExtreemeDamageFract()
{
	return 0.045;
}

// orig 0x12a890
Vector CDriveApache::GetCamRotOrg()
{
	return Center();
}

// orig 0x12a8d0
void CDriveApache::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector(-300, -300, -172);
	pev->absmax = pev->origin + Vector(300, 300, 8);
}

// orig 0x12a9a0
int CDriveApache::GetRCCaps()
{
	return 0x26088;
}

// orig 0x12a9b0
Vector CDriveApache::Center()
{
	return pev->origin - Vector(0, 0, 100);
}

// orig 0x12aa10
int CDriveApache::BloodColor()
{
	return DONT_BLEED;
}

// orig 0x12aa20
int CDriveApache::Classify()
{
	return CLASS_HUMAN_MILITARY;
}


