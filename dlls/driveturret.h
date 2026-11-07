/***
*
*	Rocket Crowbar 1.9: driveturret.cpp's classes. drive_turret is a ceiling or floor turret
*	(models/turret.mdl) a player sits in and aims: attack2 deploys and retracts it, attack fires;
*	its armorvalue is the ammo. pickup_drive_turret is the ghost on an info_drive_turret_spawn
*	the player uses to get in; drive_turret_spawner makes mp_driveturret pickups at map start.
*
****/

#pragma once

#include "driveent.h"

class CBasePlayer;
class CSprite;

// The turret model's sequences, in its order (the original's DTURRET_ANIM; member names VERIFY,
// modelled on turret.cpp's TURRET_ANIM).
typedef enum
{
	DTURRET_ANIM_NONE = 0,
	DTURRET_ANIM_FIRE,
	DTURRET_ANIM_SPIN,
	DTURRET_ANIM_DEPLOY,
	DTURRET_ANIM_RETIRE,
	DTURRET_ANIM_DIE,
} DTURRET_ANIM;

class CDriveTurret : public CBaseDriveEnt
{
public:
	void Spawn() override;
	void Precache() override;
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

	void DEClientPreThink() override;
	void DEClientPostThink() override;
	void DEClientEnter(CBasePlayer* pPlayer) override;
	void DEClientLeave() override;
	void DEClientEject() override;
	Vector GetCamRotOrg() override;

	void EXPORT KillThink();
	void EXPORT DTThink();
	void DoGun();
	void DoPing();
	void EyeUpdate();
	void UpdateOwnerHUD();
	void UpdateBarrel();
	void UpdateSpin();
	void CheckRetract();
	bool MoveTurret();
	bool RunBarrel();
	void TuFireBullets(Vector vecSrc, Vector vecDir, Vector vecSpread);
	void AttachToPlane(TraceResult* pTrace);
	void EXPORT DyingThink();
	void SetTurretAnim(DTURRET_ANIM anim);

	// None of these are saved (the class has no save table). Offsets are the original's; the
	// driver (orig +0x27c) is the inherited CBaseDriveEnt::m_pOwner. Names VERIFY.
	float m_fFrameTime;		  // orig +0x280, StudioFrameAdvance's interval this think
	float m_fLastAimTime;	  // orig +0x284, DEClientPreThink's last run
	float m_fLastBarrelTime;  // orig +0x288, RunBarrel's last run
	bool m_bDeployed;		  // orig +0x290
	bool m_bWantDeploy;		  // orig +0x291, attack2 toggles it
	bool m_bSpunUp;			  // orig +0x292
	bool m_bWantSpin;		  // orig +0x293
	int m_iSpinSound;		  // orig +0x294, 1 while the spin sound plays
	float m_fSpinDownTime;	  // orig +0x298, the barrel winds down after this
	float m_fNextShot;		  // orig +0x29c
	float m_fLastShotTime;	  // orig +0x2a0
	float m_flSpread;		  // orig +0x2a4, grows while firing
	int m_iShotCount;		  // orig +0x2a8, every second shot draws a tracer
	Vector m_vecCurAngles;	  // orig +0x2ac, gun pitch (x) and yaw (y)
	Vector m_vecGoalAngles;	  // orig +0x2b8
	Vector m_vecTurnRate;	  // orig +0x2c4, pitch and yaw degrees per second
	Vector m_vecNormal;		  // orig +0x2d0, the plane it is fixed to
	CSprite* m_pEyeGlow;	  // orig +0x2dc
	float m_flEyeBrightness;  // orig +0x2e0, what the glow fades to
	float m_flPingAmount;	  // orig +0x2e4, sent to the driver as fuser1
	float m_fNextPing;		  // orig +0x2e8
	float m_fPingEnd;		  // orig +0x2ec
	int m_iClientHealth;	  // orig +0x2f0, the health last sent to the driver's HUD
	int m_iClientArmor;		  // orig +0x2f4, the ammo last sent
	CBaseEntity* m_pViewEnt;  // orig +0x308, the driver's camera
};

class CDriveTurretPickup : public CBaseMonster
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

	Vector m_vecNormal;	   // orig +0x27c, unsaved: the floor or ceiling it sits on. VERIFY name
	CBaseEntity* m_pGiven; // the drive_turret made from this pickup
	bool m_bTakeable;
};

class CDrvTurretSpawner : public CBaseEntity
{
public:
	void Spawn() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	int ObjectCaps() override;

	void EXPORT DrvAThink();

	static TYPEDESCRIPTION m_SaveData[];

	bool m_bDummy;
};

void CreateDrvTurretSpawner();
