/***
*
*	Rocket Crowbar 1.9: drivepanth.cpp's classes. drive_panth is a panther (models/panther.mdl)
*	a player drives; pickup_drive_panth is the ghost on an info_drive_panth_spawn the player
*	uses to get in; drive_panth_spawner makes mp_drivepanth pickups at map start.
*
****/

#pragma once

#include "driveent.h"

class CBasePlayer;

// A sound with its pitch range (the original's g_pDP*Sounds tables). VERIFY type name: the
// original's tables are untyped data.
struct sDPSoundT
{
	const char* pszSound;
	int iMinPitch;
	int iMaxPitch;
};

// An idle animation with its pick weight and the range of time it holds (g_sDPanthIdleAnims).
struct sDPAnimT
{
	const char* pszSequence;
	float flWeight;
	float flMinTime;
	float flMaxTime;
};

class CDrivePanth : public CBaseDriveEnt
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

	void EXPORT DPThink();
	void CheckGround();
	void EXPORT JumpTouch(CBaseEntity* pOther);
	void DoJump();
	void DoClaw();
	void CheckDoor();
	void CheckEHop();
	void RunWaterPhysics();
	void DoMovement();
	void DoYaw();
	void DoRegen();
	void UpdateOwnerHUD();
	void ClawDamage(bool bRight);
	float SetRandomAnim(sDPAnimT* pAnims, int iCount);
	void EXPORT DyingThink();
	void SetDeathAnim();

	// None of these are saved (the class has no save table). Offsets are the original's; names
	// for 0x280-0x2a8 follow CDriveASlave's save table, which has the same layout there. The
	// driver (orig +0x27c) is the inherited CBaseDriveEnt::m_pOwner.
	float m_fFrameTime;		// orig +0x280, StudioFrameAdvance's interval this think
	Vector m_vecSudoVel;	// orig +0x284, velocity the walk moved us at (camera lead)
	float m_fLastMoveTime;	// orig +0x290
	float m_fNextEHopCheck; // orig +0x294
	float m_fNextDoorCheck; // orig +0x298
	float m_fYawSpeedScale; // orig +0x29c, scales the turn rate. VERIFY name
	float m_fNextIdleTime;	// orig +0x2a0
	float m_fNextPainSound; // orig +0x2a4
	float m_fIdleSoundTime; // orig +0x2a8
	bool m_bCanIdle;		// orig +0x2ac. VERIFY name, as all the m_bCan* flags
	bool m_bCanMove;		// orig +0x2ad
	bool m_bCanTurn;		// orig +0x2ae
	bool m_bCanTurnAnim;	// orig +0x2af, a big turn may play turn_left/turn_right
	bool m_bCanClaw;		// orig +0x2b0
	bool m_bCanJump;		// orig +0x2b1
	bool m_bHeadTrack;		// orig +0x2b2, the head bone turns toward ideal_yaw. VERIFY name
	float m_flHeadYaw;		// orig +0x2b4, bone controller 0
	float m_flHeadYawSpeed; // orig +0x2b8
	bool m_bInJump;			// orig +0x2bc
	float m_fJumpLandTime;	// orig +0x2c0. VERIFY name
	float m_fNextJump;		// orig +0x2c4
	bool m_bInClaw;			// orig +0x2c8
	float m_fUnknown2cc;	// orig +0x2cc, only Spawn sets it (to the time). VERIFY name
	bool m_bMainClaw;		// orig +0x2d0, set by DoClaw, read nowhere. VERIFY name
	bool m_bClawSide;		// orig +0x2d1, which claw the next swipe uses
	float m_fLastClaw;		// orig +0x2d4
	float m_fLastActiveTime; // orig +0x2d8, regen waits for this much stillness
	float m_fNextRegen;		// orig +0x2dc
	float m_fUnknown2e0;	// orig +0x2e0, only Spawn sets it (to the time). VERIFY name
	int m_iClientHealth;	// orig +0x2e4, the health last sent to the driver's HUD
};

class CDrivePanthPickup : public CBaseMonster
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

	CBaseEntity* m_pGiven; // the drive_panth made from this pickup
	bool m_bTakeable;
};

class CDrvPanthSpawner : public CBaseEntity
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

void CreateDrvPanthSpawner();
