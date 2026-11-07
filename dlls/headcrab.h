/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

// CHeadCrab's declaration, moved out of headcrab.cpp so Rocket Crowbar's CBaseCurse (curses.cpp)
// can derive from it, as the original does.

#pragma once

#include "monsters.h"
#include "schedule.h"
#include "skill.h"

class CHeadCrab : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void RunTask(Task_t* pTask) override;
	void StartTask(Task_t* pTask) override;
	void SetYawSpeed() override;
	void EXPORT LeapTouch(CBaseEntity* pOther);
	Vector Center() override;
	Vector BodyTarget(const Vector& posSrc) override;
	void PainSound() override;
	void DeathSound() override;
	void IdleSound() override;
	void AlertSound() override;
	void PrescheduleThink() override;
	int Classify() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool CheckRangeAttack2(float flDot, float flDist) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;

	virtual float GetDamageAmount() { return gSkillData.headcrabDmgBite; }
	virtual int GetVoicePitch() { return 100; }
	virtual float GetSoundVolue() { return 1.0; }
	Schedule_t* GetScheduleOfType(int Type) override;

	// Rocket Crowbar 1.9
	Vector GetBotShootOrg() override { return pev->origin - Vector(0, 0, 16); } // orig 0x1aafa0
	int GetRCCaps() override { return 0x6fe3d5f; }								// orig 0x1ab000
	int GetGuassDistOut() override { return 32; }								// orig 0x1ab010
	int GetGuassHull() override { return head_hull; }							// orig 0x1ab020

	CUSTOM_SCHEDULES;

	static const char* pIdleSounds[];
	static const char* pAlertSounds[];
	static const char* pPainSounds[];
	static const char* pAttackSounds[];
	static const char* pDeathSounds[];
	static const char* pBiteSounds[];
};
