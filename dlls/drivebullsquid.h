/***
*
*	Rocket Crowbar 1.9: drivebullsquid.cpp's classes. drive_bullsquid is a bullsquid
*	(models/bullsquid.mdl) a player drives: bite, tail whip and the chumtoadifier beam;
*	pickup_drive_bullsquid is the ghost on an info_drive_bullsquid_spawn the player uses to get
*	in; drive_bullsquid_spawner makes mp_drivebullsquid pickups at map start.
*
****/

#pragma once

#include "driveent.h"

class CBasePlayer;

class CDriveBullsquid : public CBaseDriveEnt
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

	void EXPORT DBSThink();
	void DoBite();
	void DoWhip();
	void DoChumtoadifier();
	void CheckDoor();
	void CheckEHop();
	void RunWaterPhysics();
	void DoMovement();
	void DoYaw();
	void UpdateOwnerHUD();
	void DoMeleeDamage(bool bBite);
	void EXPORT DyingThink();
	void SetDeathAnim();

	// None of these are saved (the class has no save table). Offsets are the original's. The
	// driver (orig +0x27c) is the inherited CBaseDriveEnt::m_pOwner.
	float m_fFrameTime;			// orig +0x280, StudioFrameAdvance's interval this think
	bool m_bCanIdle;			// orig +0x284. VERIFY name, as all the m_bCan* flags
	bool m_bMoved;				// orig +0x285, set when the walk moved, read nowhere. VERIFY name
	bool m_bCanMove;			// orig +0x286
	bool m_bCanTurnAnim;		// orig +0x287, a big turn may play turnleft/turnright
	bool m_bInBite;				// orig +0x288
	float m_fNextBite;			// orig +0x28c
	bool m_bInWhip;				// orig +0x290
	float m_fNextWhip;			// orig +0x294
	bool m_bInChumtoadifier;	// orig +0x298
	float m_fNextChumtoadifier; // orig +0x29c
	Vector m_vecSudoVel;		// orig +0x2a0, velocity the walk moved us at (camera lead)
	float m_fLastMoveTime;		// orig +0x2ac
	float m_fNextEHopCheck;		// orig +0x2b0
	float m_fNextDoorCheck;		// orig +0x2b4
	float m_fYawSpeedScale;		// orig +0x2b8, scales the turn rate. VERIFY name
	float m_fIdleSoundTime;		// orig +0x2bc
	float m_fNextPainSound;		// orig +0x2c0
	int m_iClientHealth;		// orig +0x2c4, the health last sent to the driver's HUD
};

class CDriveBullsquidPickup : public CBaseMonster
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

	CBaseEntity* m_pGiven; // the drive_bullsquid made from this pickup
	bool m_bTakeable;
};

class CDrvBullsquidSpawner : public CBaseEntity
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

void CreateDrvBullsquidSpawner();
void DBSFireChumtoadifer(edict_t* pentShooter, edict_t* pentOwner, Vector vecSrc, Vector vecDir);
