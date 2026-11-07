/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

/*
*	Rocket Crowbar 1.9: the RPG fires no rockets. Primary fires a scientist (or a barney) that
*	lands, waits for something to come near, says hello, spins up into the air and explodes;
*	bombs that land near each other chat (CConversation). Secondary fires a suck mine: a sentry
*	that ropes the best target in front of its barrel and reels it in. Every tunable is in
*	rc.ini's [RPG] heading. The laser spot and guided rocket stay for func_tank (rpg_rocket).
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "effects.h"
#include "customentity.h"
#include "Preff.h"

LINK_ENTITY_TO_CLASS(weapon_rpg, CRpg); // orig 0x1ff6a0

#ifndef CLIENT_DLL

// The bombs' sentences. The original formats "%!SC_HELLO%d" and the like: Windows' CRT drops the
// unknown "%!" conversion's '%' and gives the sentence "!SC_HELLO3"; glibc keeps "%!", so the
// original Linux server asked for a sound named "%!SC_HELLO3" and played nothing. Ours plays
// the sentences, as Windows servers did. VERIFY
static void RCBombSentence(edict_t* pEdict, const char* szGroup, int iNum)
{
	char szSentence[64];
	snprintf(szSentence, sizeof(szSentence), "!%s%d", szGroup, iNum);
	EMIT_SOUND(pEdict, CHAN_VOICE, szSentence, 1.0, ATTN_NORM);
}

//=========================================================
// CSuckMine: secondary fire. A sentry gun that drops, deploys, and pulls the best target in
// front of its barrel toward itself on a rope.
//=========================================================
class CSuckMine : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override { return CLASS_MACHINE; } // orig 0x201c20
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int BloodColor() override { return DONT_BLEED; }	  // orig 0x201c30
	int GetGuassHull() override { return human_hull; }	  // orig 0x201c10
	int GetGuassDistOut() override { return 56; }		  // orig 0x201c00
	edict_t* GetRealOwner() override { return m_pRealOwner; } // orig 0x2019a0
	bool GetPullOrg(edict_t* pPulled, Vector& vecOrigin, float& flForce) override;
	void EntIsBeingRemoved(edict_t* pEdict) override;
	int GetRCCaps() override { return 0x802cdf; } // orig 0x201bf0

	void EXPORT RunThink();
	void LookForTarget();
	void MoveGun();
	void DoSucking();
	void EXPORT BecomeSolid();
	void EXPORT ActiveTouch(CBaseEntity* pOther);
	void EXPORT DeathThink();

	edict_t* m_pRealOwner; // orig +0x280: the player who fired it (CRpg::SecondaryAttack). VERIFY name
	EHANDLE m_hEnemy;	   // orig +0x284: what it is pulling. VERIFY name
	CBeam* m_pBeam;		   // orig +0x28c: the rope, while pulling. VERIFY name
	Vector m_vecGunAng;	   // orig +0x290: bone controller 1 (pitch, .x) and 0 (yaw, .y). VERIFY name
	float m_flInterval;	   // orig +0x29c: the last StudioFrameAdvance interval. VERIFY name
	float m_flMaxRange;	   // orig +0x2a0. VERIFY name
	float m_flMinRange;	   // orig +0x2a4. VERIFY name
	float m_flNextLook;	   // orig +0x2a8. VERIFY name
};

LINK_ENTITY_TO_CLASS(suck_mine, CSuckMine); // orig 0x1fc0b0

// orig 0x1fc0e0
void CSuckMine::Precache()
{
	PRECACHE_MODEL("models/sentry.mdl");
	PRECACHE_MODEL("sprites/rope.spr");
	PRECACHE_SOUND("turret/tu_die.wav");
	PRECACHE_SOUND("turret/tu_die2.wav");
	PRECACHE_SOUND("turret/tu_die3.wav");
	PRECACHE_SOUND("turret/tu_deploy.wav");
	PRECACHE_SOUND("turret/tu_active2.wav");
}

// orig 0x1fc180. pev->dmg_save holds the time it dies of old age.
void CSuckMine::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/sentry.mdl");
	pev->health = PREFS_KeyLoopUpFloat("RPG", "SuckMineHealth", 75.0);
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_AIM;
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 64));
	SetTouch(&CSuckMine::ActiveTouch);
	SetThink(&CSuckMine::BecomeSolid);
	pev->nextthink = gpGlobals->time + 0.1;
	m_hEnemy = NULL;
	m_pBeam = NULL;
	pev->sequence = 0;
	ResetSequenceInfo();
	SetBoneController(0, 0);
	SetBoneController(1, 0);
	m_flMaxRange = PREFS_KeyLoopUpFloat("RPG", "SuckMineMaxRange", 320.0);
	m_flMinRange = PREFS_KeyLoopUpFloat("RPG", "SuckMineMinRange", 64.0);
	pev->dmg_save = PREFS_KeyLoopUpFloat("RPG", "SuckMineLife", 120.0) + gpGlobals->time;
	m_flNextLook = gpGlobals->time;
}

// orig 0x1fc340
void CSuckMine::RunThink()
{
	pev->nextthink = gpGlobals->time + 0.01;
	m_flInterval = StudioFrameAdvance();

	if (pev->owner && (VARS(pev->owner)->origin - pev->origin).Length() > 64)
		pev->owner = NULL;

	if (pev->dmg_save < gpGlobals->time)
	{
		Killed(pev, GIB_NORMAL);
		return;
	}

	if (pev->sequence == 3)
	{
		// still deploying
		if (!m_fSequenceFinished)
			return;
		pev->sequence = 2;
		ResetSequenceInfo();
		EMIT_SOUND(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav", 0.3, ATTN_NORM);
	}

	LookForTarget();
	MoveGun();
	DoSucking();
}

// orig 0x1fc490. Ten times a second: drop the target when it is out of range, dead or hidden
// from the barrel, then pick the best visible one: nearer and closer to where the barrel points.
void CSuckMine::LookForTarget()
{
	if (gpGlobals->time < m_flNextLook)
		return;
	m_flNextLook = gpGlobals->time + 0.1;

	Vector vecSrc, vecAng;
	GetAttachment(1, vecSrc, vecAng);

	if (m_hEnemy != NULL)
	{
		float flDist = (m_hEnemy->Center() - Center()).Length();
		if (flDist > m_flMaxRange || flDist < m_flMinRange || !m_hEnemy->IsAlive())
			m_hEnemy = NULL;

		if (m_hEnemy != NULL)
		{
			TraceResult tr;
			UTIL_TraceLine(vecSrc, m_hEnemy->Center(), ignore_monsters, ENT(pev), &tr);
			if (tr.flFraction < 1.0 && tr.pHit != m_hEnemy->edict())
				m_hEnemy = NULL;
		}
	}

	if (m_hEnemy != NULL)
		return;

	float flBest = -9999;
	CBaseEntity* pBest = NULL;

	vecAng = pev->angles + m_vecGunAng;
	while (vecAng.x < 0)
		vecAng.x += 360;
	while (vecAng.x > 360)
		vecAng.x -= 360;
	while (vecAng.y < 0)
		vecAng.y += 360;
	while (vecAng.y > 360)
		vecAng.y -= 360;
	while (vecAng.z < 0)
		vecAng.z += 360;
	while (vecAng.z > 360)
		vecAng.z -= 360;
	UTIL_MakeVectors(vecAng);

	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, m_flMaxRange)) != NULL)
	{
		int iCap = 0x200000; // VERIFY: an RC cap bit, name unknown
		if (!pEntity->HasRCCap(iCap))
			continue;
		if (!pEntity->IsAlive())
			continue;
		if (pEntity->edict() == m_pRealOwner && PREFS_KeyLoopUpBool("RPG", "SuckMineNoSuckOwner", true))
			continue;

		Vector vecDir = pEntity->Center() - vecSrc;
		float flDot = DotProduct(vecDir.Normalize(), gpGlobals->v_forward);
		float flLen = vecDir.Length();
		if (flLen < m_flMinRange)
			continue;

		float flScore = (1 - flLen / m_flMaxRange) * 2 + flDot * 1.5;
		if (flScore < flBest)
			continue;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, pEntity->Center(), ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
			continue;

		pBest = pEntity;
		flBest = flScore;
	}

	m_hEnemy = pBest;
}

// orig 0x1fca40. Idle: level the barrel and spin. Tracking: turn toward the target, faster
// once the rope is on.
void CSuckMine::MoveGun()
{
	if (m_hEnemy == NULL)
	{
		m_vecGunAng.x = UTIL_Approach(0, m_vecGunAng.x, 100 * m_flInterval);
		m_vecGunAng.y = UTIL_AngleMod(m_vecGunAng.y + 180 * m_flInterval);
		m_vecGunAng.y = SetBoneController(0, m_vecGunAng.y);
		m_vecGunAng.x = SetBoneController(1, m_vecGunAng.x);
		return;
	}

	Vector vecSrc, vecAng;
	GetAttachment(1, vecSrc, vecAng);

	Vector vecGoal = UTIL_VecToAngles((m_hEnemy->Center() - vecSrc).Normalize());
	vecGoal.y = UTIL_AngleMod(vecGoal.y - pev->angles.y);
	if (vecGoal.x < -180)
		vecGoal.x += 360;
	if (vecGoal.x > 180)
		vecGoal.x -= 360;

	float flPitch = -vecGoal.x;
	if (flPitch < -90)
		flPitch = -90;
	else if (flPitch > 60)
		flPitch = 60;
	vecGoal.x = flPitch;

	float flYawSpeed;
	if (!m_pBeam)
	{
		vecGoal.x = UTIL_Approach(vecGoal.x, m_vecGunAng.x, 200 * m_flInterval);
		flYawSpeed = 480;
	}
	else
	{
		vecGoal.x = UTIL_Approach(vecGoal.x, m_vecGunAng.x, 300 * m_flInterval);
		flYawSpeed = 600;
	}
	vecGoal.y = UTIL_ApproachAngle(vecGoal.y, m_vecGunAng.y, flYawSpeed * m_flInterval);

	m_vecGunAng.y = SetBoneController(0, vecGoal.y);
	m_vecGunAng.x = SetBoneController(1, vecGoal.x);
}

// orig 0x1fcd00. Once the barrel is on the target (dot 0.97) the rope goes on; it comes off when
// the barrel drifts past dot 0.92. While roped, the target's velocity takes it to its pull centre
// (all suck mines pulling it, GetPullCenter) in one frame interval, capped at 1000, with a lift.
void CSuckMine::DoSucking()
{
	Vector vecAng = pev->angles + m_vecGunAng;
	while (vecAng.x < 0)
		vecAng.x += 360;
	while (vecAng.x > 360)
		vecAng.x -= 360;
	while (vecAng.y < 0)
		vecAng.y += 360;
	while (vecAng.y > 360)
		vecAng.y -= 360;
	while (vecAng.z < 0)
		vecAng.z += 360;
	while (vecAng.z > 360)
		vecAng.z -= 360;
	UTIL_MakeVectors(vecAng);

	Vector vecSrc;
	GetAttachment(1, vecSrc, vecAng);

	if (m_hEnemy == NULL)
	{
		if (m_pBeam)
		{
			m_pBeam->pev->effects |= EF_NODRAW;
			m_pBeam->SUB_Remove();
			m_pBeam = NULL;
		}
		return;
	}

	Vector vecDir = m_hEnemy->Center() - vecSrc;
	if (!m_pBeam && DotProduct(vecDir.Normalize(), gpGlobals->v_forward) < 0.97)
		return;

	if (DotProduct(vecDir.Normalize(), gpGlobals->v_forward) < 0.92)
	{
		// no null check in the original; m_pBeam is set here, since the 0.97 test passed
		m_pBeam->pev->effects |= EF_NODRAW;
		m_pBeam->SUB_Remove();
		m_pBeam = NULL;
	}

	if (!m_pBeam)
	{
		m_pBeam = CBeam::BeamCreate("sprites/rope.spr", 10);
		m_pBeam->EntsInit(m_hEnemy->entindex(), entindex());
		m_pBeam->SetFlags(BEAM_FSOLID);
		m_pBeam->SetColor(255, 255, 255);
		m_pBeam->SetEndAttachment(1);
	}
	m_pBeam->SetStartEntity(m_hEnemy->entindex());

	Vector vecPull = GetPullCenter(m_hEnemy->edict());
	if (vecPull == g_vecZero || m_flInterval == 0)
		return;

	m_hEnemy->pev->velocity = (vecPull - m_hEnemy->Center()) / m_flInterval;
	if (m_hEnemy->pev->velocity.Length() > 1000)
		m_hEnemy->pev->velocity = m_hEnemy->pev->velocity.Normalize() * 1000;
	m_hEnemy->pev->flags &= ~FL_ONGROUND;
	if (m_hEnemy->pev->velocity.z >= 0)
		m_hEnemy->pev->velocity.z += 100;
}

// orig 0x1fd410. Think while flying: let the owner be hit once he is 64 units away.
void CSuckMine::BecomeSolid()
{
	pev->nextthink = gpGlobals->time + 0.1;
	if (pev->owner && (VARS(pev->owner)->origin - pev->origin).Length() < 64)
		return;
	pev->owner = NULL;
	SetThink(NULL);
}

// orig 0x1fd4e0. First touch: deploy.
void CSuckMine::ActiveTouch(CBaseEntity* pOther)
{
	SetTouch(NULL);
	BecomeSolid();
	SetThink(&CSuckMine::RunThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->sequence = 3;
	ResetSequenceInfo();
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "turret/tu_deploy.wav", 0.8, ATTN_NORM);
}

// orig 0x1fd590
void CSuckMine::Killed(entvars_t* pevAttacker, int iGib)
{
	pev->deadflag = DEAD_DEAD;

	float flRndSound = RANDOM_FLOAT(0, 1);
	if (flRndSound <= 0.33)
		EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_die.wav", 1.0, ATTN_NORM);
	else if (flRndSound <= 0.66)
		EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_die2.wav", 1.0, ATTN_NORM);
	else
		EMIT_SOUND(ENT(pev), CHAN_BODY, "turret/tu_die3.wav", 1.0, ATTN_NORM);

	STOP_SOUND(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav");
	STOP_SOUND(ENT(pev), CHAN_VOICE, "turret/tu_deploy.wav");

	SetBoneController(0, 0);
	SetBoneController(1, 0);

	if (m_pBeam)
	{
		m_pBeam->pev->effects |= EF_NODRAW;
		m_pBeam->SUB_Remove();
		m_pBeam = NULL;
	}

	pev->sequence = 5;
	ResetSequenceInfo();
	m_hEnemy = NULL;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->angles.y = UTIL_AngleMod(RANDOM_LONG(0, 2) * 120 + pev->angles.y);

	SetThink(&CSuckMine::DeathThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x1fd780. Smoke and sparks while the death animation plays and for 2.5 s after, then
// fade out. pev->air_finished holds the end of the smoking.
void CSuckMine::DeathThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	Vector vecSrc, vecAng;
	GetAttachment(1, vecSrc, vecAng);

	if (!m_fSequenceFinished)
	{
		StudioFrameAdvance();
		if (m_fSequenceFinished)
			pev->air_finished = gpGlobals->time + 2.5;

		if (RANDOM_FLOAT(0, 1) <= 0.4)
		{
			MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_SMOKE);
			WRITE_COORD(RANDOM_FLOAT(-16, 16) + vecSrc.x);
			WRITE_COORD(RANDOM_FLOAT(-16, 16) + vecSrc.y);
			WRITE_COORD(vecSrc.z - 32);
			WRITE_SHORT(g_sModelIndexSmoke);
			WRITE_BYTE(15); // scale * 10
			WRITE_BYTE(8);	// framerate
			MESSAGE_END();
		}

		if (RANDOM_FLOAT(0, 1) <= 0.2)
			UTIL_Sparks(vecSrc);
	}
	else
	{
		if (gpGlobals->time < RANDOM_FLOAT(0, 2) + pev->air_finished)
		{
			MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_SMOKE);
			WRITE_COORD(RANDOM_FLOAT(-16, 16) + vecSrc.x);
			WRITE_COORD(RANDOM_FLOAT(-16, 16) + vecSrc.y);
			WRITE_COORD(vecSrc.z - 32);
			WRITE_SHORT(g_sModelIndexSmoke);
			WRITE_BYTE(15); // scale * 10
			WRITE_BYTE(8);	// framerate
			MESSAGE_END();
		}

		if (gpGlobals->time < RANDOM_FLOAT(0, 8) + pev->air_finished)
			UTIL_Sparks(vecSrc);

		if (pev->air_finished < gpGlobals->time)
			SUB_StartFadeOut();
	}
}

// orig 0x2019b0. While roping pPulled: pull it to the minimum range in its direction, force 3.
bool CSuckMine::GetPullOrg(edict_t* pPulled, Vector& vecOrigin, float& flForce)
{
	if (m_hEnemy != NULL && m_hEnemy->edict() == pPulled && m_pBeam)
	{
		Vector vecDir = (m_hEnemy->Center() - pev->origin).Normalize();
		vecOrigin = pev->origin + vecDir * m_flMinRange;
		flForce = 3.0;
		return true;
	}

	vecOrigin = g_vecZero;
	flForce = 0;
	return false;
}

// orig 0x201b40. Does not call the base version. The firing player leaving kills the mine.
void CSuckMine::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pBeam && m_pBeam->pev && m_pBeam->edict() == pEdict)
		m_pBeam = NULL;

	if (m_hEnemy != NULL && m_hEnemy->edict() == pEdict)
		m_hEnemy = NULL;

	if (m_pRealOwner && m_pRealOwner == pEdict)
	{
		m_pRealOwner = NULL;
		Killed(pev, GIB_NORMAL);
	}
}

//=========================================================
// CConversation: bombs that land within 512 units of each other join one, and it has a random
// member ask a question, then another answer, every few seconds. Not linked to a classname:
// it is made with GetClassPtr and found by its classname "conversation".
//=========================================================
class CConversation : public CBaseMonster
{
public:
	static CConversation* ConvCreate(Vector vecOrigin);
	void Spawn() override;
	void EXPORT ChatThink();
	int AddMemeber(edict_t* pMember) override;
	void RemoveMemeber(edict_t* pMember);

	float m_flNextChat;		 // orig +0x27c. VERIFY name
	int m_iAsk;				 // orig +0x280: nonzero when the next line is a question. VERIFY name
	edict_t* m_pMembers[64]; // orig +0x284. VERIFY name
	int m_iMembers;			 // orig +0x384. VERIFY name
};

// orig 0x1fdae0
CConversation* CConversation::ConvCreate(Vector vecOrigin)
{
	CConversation* pConv = GetClassPtr((CConversation*)NULL);
	pConv->pev->classname = MAKE_STRING("conversation");
	UTIL_SetOrigin(pConv->pev, vecOrigin);
	pConv->Spawn();
	return pConv;
}

// orig 0x1fdb40
void CConversation::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	m_iMembers = 0;
	m_iAsk = 1;
	m_flNextChat = gpGlobals->time + 1.0;
	SetThink(&CConversation::ChatThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x1fdc30. Recounts the living members, moving each to the first free slot, then lets one
// of the first m_iMembers slots talk. A dead or invalid member keeps its slot uncounted, so the
// pick can land on it (GetClassPtr then makes a CBaseMonster on that edict). VERIFY
void CConversation::ChatThink()
{
	pev->nextthink = gpGlobals->time + 0.5;

	if (m_iMembers < 1)
	{
		SetThink(&CConversation::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	m_iMembers = 0;
	for (int i = 0; i < 64; i++)
	{
		if (!m_pMembers[i] || !UTIL_IsValidEntity(m_pMembers[i]))
			continue;

		edict_t* pMember = m_pMembers[i];
		if (pMember->v.health > 0)
		{
			m_pMembers[i] = NULL;
			for (int j = 0; j < 64; j++)
			{
				if (!m_pMembers[j])
				{
					m_pMembers[j] = pMember;
					break;
				}
			}
			m_iMembers++;
		}
	}

	if (m_flNextChat < gpGlobals->time && m_iMembers > 1)
	{
		if (0 == m_iAsk)
		{
			CBaseMonster* pTalker = GetClassPtr((CBaseMonster*)VARS(m_pMembers[RANDOM_LONG(0, m_iMembers - 1)]));
			pTalker->AnswerQuest();
			m_flNextChat = RANDOM_FLOAT(4, 7) + gpGlobals->time;
			m_iAsk = 1;
		}
		else
		{
			CBaseMonster* pTalker = GetClassPtr((CBaseMonster*)VARS(m_pMembers[RANDOM_LONG(0, m_iMembers - 1)]));
			pTalker->AskQuest();
			m_flNextChat = RANDOM_FLOAT(6, 10) + gpGlobals->time;
			m_iAsk = 0;
		}
	}
}

// orig 0x1fdea0
int CConversation::AddMemeber(edict_t* pMember)
{
	if (m_iMembers < 64)
	{
		for (int i = 0; i < 64; i++)
		{
			if (!m_pMembers[i])
			{
				m_pMembers[i] = pMember;
				m_iMembers++;
				return 1;
			}
		}
	}
	return 0;
}

// orig 0x1fdef0
void CConversation::RemoveMemeber(edict_t* pMember)
{
	for (int i = 0; i < 64; i++)
	{
		if (m_pMembers[i] == pMember)
		{
			m_pMembers[i] = NULL;
			m_iMembers--;
		}
	}

	if (m_iMembers < 1)
	{
		SetThink(&CConversation::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

//=========================================================
// CScientistBomb / CBarneyBomb: primary fire. The two are the same code with different models,
// sentences and rc.ini keys, and their owner and launcher members swapped.
//=========================================================
class CScientistBomb : public CGrenade
{
public:
	static CScientistBomb* SciCreate();
	void Spawn() override;
	void Precache() override;
	void EXPORT SciPreTouchThink();
	void EXPORT SciMineExplode();
	void EXPORT SciMineActiveThink();
	void EXPORT SciBonceTouch(CBaseEntity* pOther);
	void AskQuest() override;
	void AnswerQuest() override;
	int BloodColor() override { return BLOOD_COLOR_RED; }		 // orig 0x1fead0
	int Classify() override { return CLASS_PLAYER_BIOWEAPON; }	 // orig 0x1feae0
	void Killed(entvars_t* pevAttacker, int iGib) override;
	edict_t* GetRealOwner() override { return m_pRealOwner; } // orig 0x2013e0
	void EntIsBeingRemoved(edict_t* pEdict) override;
	int GetRCCaps() override { return 0x6dc3fff; }	   // orig 0x201490
	int GetGuassDistOut() override { return 56; }	   // orig 0x2014a0
	int GetGuassHull() override { return human_hull; } // orig 0x2014b0

	CBaseEntity* m_pConv;  // orig +0x280: the CConversation it joined. VERIFY name
	CRpg* m_pLauncher;	   // orig +0x284. VERIFY name
	edict_t* m_pRealOwner; // orig +0x288: the player who fired it. VERIFY name
};

LINK_ENTITY_TO_CLASS(scientistbomb, CScientistBomb); // orig 0x1fdf80

// orig 0x1fdfb0. The classname is rpg_rocket, which CRpg::PrimaryAttack counts.
CScientistBomb* CScientistBomb::SciCreate()
{
	CScientistBomb* pBomb = GetClassPtr((CScientistBomb*)NULL);
	pBomb->pev->classname = MAKE_STRING("rpg_rocket");
	pBomb->Spawn();
	return pBomb;
}

// orig 0x1fe000
void CScientistBomb::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_AIM;
	pev->health = PREFS_KeyLoopUpFloat("RPG", "SciMineHealth", 35.0);
	pev->gravity = PREFS_KeyLoopUpFloat("RPG", "SciMinGravity", 0.75);
	SET_MODEL(ENT(pev), "models/scientist.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));
	SetTouch(&CScientistBomb::SciBonceTouch);
	SetThink(&CScientistBomb::SciPreTouchThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->body = RANDOM_LONG(0, 3);
	pev->dmg = PREFS_KeyLoopUpFloat("RPG", "SciMineDmg", 250.0);
	if (pev->body == 2)
		pev->skin = 1;
	pev->sequence = 13;
	ResetSequenceInfo();
	InitBoneControllers();
}

// orig 0x1fe1a0
void CScientistBomb::Precache()
{
	PRECACHE_MODEL("models/scientist.mdl");
	PRECACHE_MODEL("models/barney.mdl");
}

// orig 0x1fe1e0. In flight: stay upright; arm on landing once clear of the shooter.
void CScientistBomb::SciPreTouchThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;
	pev->angles.x = 0;
	pev->angles.z = 0;

	if (!pev->owner && (pev->flags & FL_ONGROUND) != 0)
		SciBonceTouch(NULL);

	if (pev->owner && (pev->origin - VARS(pev->owner)->origin).Length() >= 64)
		pev->owner = NULL;
}

// orig 0x1fe2e0
void CScientistBomb::SciMineExplode()
{
	pev->takedamage = DAMAGE_NO;
	pev->health = -100;
	pev->deadflag = DEAD_DEAD;
	pev->owner = m_pRealOwner ? m_pRealOwner : edict();

	if (m_pConv && m_pConv->pev && UTIL_IsValidEntity(m_pConv->edict()))
		static_cast<CConversation*>(m_pConv)->RemoveMemeber(edict());

	Detonate();
}

// orig 0x1fe390. Armed: the first entity in reach and in sight with RC cap 0x20000 sets it off.
// It says hello, rises spinning, and explodes after SciMineBoomDelay.
void CScientistBomb::SciMineActiveThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if ((pev->flags & FL_FROZEN) != 0 || CVAR_GET_FLOAT("sv_ceasefire") == 1)
		return;

	Vector vecSrc = pev->origin + Vector(0, 0, 10);
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, PREFS_KeyLoopUpFloat("RPG", "SciMineDist", 192.0))) != NULL)
	{
		int iCap = 0x20000; // VERIFY: an RC cap bit, name unknown
		if (!pEntity->HasRCCap(iCap))
			continue;

		Vector vecTarget = pEntity->BodyTarget(vecSrc);
		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecTarget, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction != 1.0 && tr.pHit != pEntity->edict())
			continue;

		RCBombSentence(ENT(pev), "SC_HELLO", RANDOM_LONG(0, 8));

		// this facing has no effect: angles.x and .y are zeroed right after (original)
		vecTarget.z = pev->origin.z;
		UTIL_MakeVectors((pev->origin - vecTarget).Normalize());
		pev->angles = UTIL_VecToAngles(gpGlobals->v_forward);
		pev->angles.x = 0;
		pev->angles.y = 0;
		pev->fixangle = 1;

		pev->velocity = g_vecZero;
		pev->movetype = MOVETYPE_FLY;
		UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
		pev->flags &= ~FL_ONGROUND;
		pev->avelocity = Vector(0, RANDOM_FLOAT(720, 360), 0);

		SetThink(&CScientistBomb::SciMineExplode);
		pev->nextthink = gpGlobals->time + PREFS_KeyLoopUpFloat("RPG", "SciMineBoomDelay", 0.75);

		// VERIFY: guard, the original crashes here (m_pLauncher is NULL once the RPG is removed,
		// EntIsBeingRemoved)
		if (m_pLauncher)
			m_pLauncher->m_cActiveRockets--;
		return;
	}
}

// orig 0x1fe820. Landing (or touching anything but the shooter): arm, and join a conversation
// within 512 units or start one.
void CScientistBomb::SciBonceTouch(CBaseEntity* pOther)
{
	if (pev->owner && pOther && pOther->edict() == pev->owner)
		return;

	SetTouch(NULL);
	SetThink(&CScientistBomb::SciMineActiveThink);
	pev->nextthink = gpGlobals->time + 0.1;

	CBaseEntity* pConv = NULL;
	while ((pConv = UTIL_FindEntityByClassname(pConv, "conversation")) != NULL)
	{
		if ((pConv->pev->origin - pev->origin).Length() > 512)
			continue;
		CBaseEntity* pEntity = CBaseEntity::Instance(pConv->edict());
		if (pEntity && pEntity->AddMemeber(edict()))
		{
			m_pConv = pEntity;
			return;
		}
	}

	CConversation* pNew = CConversation::ConvCreate(pev->origin);
	pNew->AddMemeber(edict());
	m_pConv = pNew;
}

// orig 0x1fe9f0
void CScientistBomb::AskQuest()
{
	RCBombSentence(ENT(pev), "SC_QUESTION", RANDOM_LONG(0, 26));
}

// orig 0x1fea60
void CScientistBomb::AnswerQuest()
{
	RCBombSentence(ENT(pev), "SC_ANSWER", RANDOM_LONG(0, 29));
}

// orig 0x1feaf0
void CScientistBomb::Killed(entvars_t* pevAttacker, int iGib)
{
	SciMineExplode();
}

// orig 0x2013f0. Does not call the base version. The firing player leaving sets it off.
void CScientistBomb::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pConv && m_pConv->pev && m_pConv->edict() == pEdict)
		m_pConv = NULL;
	if (m_pLauncher && m_pLauncher->pev && m_pLauncher->edict() == pEdict)
		m_pLauncher = NULL;
	if (m_pRealOwner && m_pRealOwner == pEdict)
	{
		m_pRealOwner = NULL;
		Killed(pev, GIB_NORMAL);
	}
}

class CBarneyBomb : public CGrenade
{
public:
	static CBarneyBomb* SciCreate();
	void Spawn() override;
	void Precache() override;
	void EXPORT SciPreTouchThink();
	void EXPORT SciMineExplode();
	void EXPORT SciMineActiveThink();
	void EXPORT SciBonceTouch(CBaseEntity* pOther);
	void AskQuest() override;
	void AnswerQuest() override;
	int BloodColor() override { return BLOOD_COLOR_RED; }		 // orig 0x1ff630
	int Classify() override { return CLASS_PLAYER_BIOWEAPON; }	 // orig 0x1ff640
	void Killed(entvars_t* pevAttacker, int iGib) override;
	edict_t* GetRealOwner() override { return m_pRealOwner; } // orig 0x2012b0
	void EntIsBeingRemoved(edict_t* pEdict) override;
	int GetRCCaps() override { return 0x6dc3fff; }	   // orig 0x201360
	int GetGuassDistOut() override { return 56; }	   // orig 0x201370
	int GetGuassHull() override { return human_hull; } // orig 0x201380

	CBaseEntity* m_pConv;  // orig +0x280. VERIFY name
	edict_t* m_pRealOwner; // orig +0x284 (+0x288 in CScientistBomb). VERIFY name
	CRpg* m_pLauncher;	   // orig +0x288. VERIFY name
};

LINK_ENTITY_TO_CLASS(barneymine, CBarneyBomb); // orig 0x1feb10

// orig 0x1feb40
CBarneyBomb* CBarneyBomb::SciCreate()
{
	CBarneyBomb* pBomb = GetClassPtr((CBarneyBomb*)NULL);
	pBomb->pev->classname = MAKE_STRING("rpg_rocket");
	pBomb->Spawn();
	return pBomb;
}

// orig 0x1feb90
void CBarneyBomb::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_AIM;
	pev->health = PREFS_KeyLoopUpFloat("RPG", "BarnMineHealth", 35.0);
	pev->gravity = PREFS_KeyLoopUpFloat("RPG", "BarnMinGravity", 0.75);
	SET_MODEL(ENT(pev), "models/barney.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));
	SetTouch(&CBarneyBomb::SciBonceTouch);
	SetThink(&CBarneyBomb::SciPreTouchThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->body = 0;
	pev->dmg = PREFS_KeyLoopUpFloat("RPG", "BarnMineDmg", 250.0);
	pev->sequence = LookupSequence("standing_idle");
	ResetSequenceInfo();
	InitBoneControllers();
}

// orig 0x1fed20
void CBarneyBomb::Precache()
{
	PRECACHE_MODEL("models/barney.mdl");
}

// orig 0x1fed50
void CBarneyBomb::SciPreTouchThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;
	pev->angles.x = 0;
	pev->angles.z = 0;

	if (!pev->owner && (pev->flags & FL_ONGROUND) != 0)
		SciBonceTouch(NULL);

	if (pev->owner && (pev->origin - VARS(pev->owner)->origin).Length() >= 64)
		pev->owner = NULL;
}

// orig 0x1fee50
void CBarneyBomb::SciMineExplode()
{
	pev->takedamage = DAMAGE_NO;
	pev->health = -100;
	pev->deadflag = DEAD_DEAD;
	pev->owner = m_pRealOwner ? m_pRealOwner : edict();

	if (m_pConv && m_pConv->pev && UTIL_IsValidEntity(m_pConv->edict()))
		static_cast<CConversation*>(m_pConv)->RemoveMemeber(edict());

	Detonate();
}

// orig 0x1fef00. As CScientistBomb's, with the BarnMine keys and hello sentences 0-6.
void CBarneyBomb::SciMineActiveThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if ((pev->flags & FL_FROZEN) != 0 || CVAR_GET_FLOAT("sv_ceasefire") == 1)
		return;

	Vector vecSrc = pev->origin + Vector(0, 0, 10);
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, PREFS_KeyLoopUpFloat("RPG", "BarnMineDist", 192.0))) != NULL)
	{
		int iCap = 0x20000; // VERIFY: an RC cap bit, name unknown
		if (!pEntity->HasRCCap(iCap))
			continue;

		Vector vecTarget = pEntity->BodyTarget(vecSrc);
		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecTarget, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction != 1.0 && tr.pHit != pEntity->edict())
			continue;

		RCBombSentence(ENT(pev), "BA_HELLO", RANDOM_LONG(0, 6));

		// this facing has no effect: angles.x and .y are zeroed right after (original)
		vecTarget.z = pev->origin.z;
		UTIL_MakeVectors((pev->origin - vecTarget).Normalize());
		pev->angles = UTIL_VecToAngles(gpGlobals->v_forward);
		pev->angles.x = 0;
		pev->angles.y = 0;
		pev->fixangle = 1;

		SetThink(&CBarneyBomb::SciMineExplode);
		pev->nextthink = gpGlobals->time + PREFS_KeyLoopUpFloat("RPG", "BarnMineBoomDelay", 0.75);

		pev->velocity = g_vecZero;
		pev->movetype = MOVETYPE_FLY;
		UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
		pev->flags &= ~FL_ONGROUND;
		pev->avelocity = Vector(0, RANDOM_FLOAT(720, 360), 0);

		// VERIFY: guard, the original crashes here (see CScientistBomb::SciMineActiveThink)
		if (m_pLauncher)
			m_pLauncher->m_cActiveRockets--;
		return;
	}
}

// orig 0x1ff380
void CBarneyBomb::SciBonceTouch(CBaseEntity* pOther)
{
	if (pev->owner && pOther && pOther->edict() == pev->owner)
		return;

	SetTouch(NULL);
	SetThink(&CBarneyBomb::SciMineActiveThink);
	pev->nextthink = gpGlobals->time + 0.1;

	CBaseEntity* pConv = NULL;
	while ((pConv = UTIL_FindEntityByClassname(pConv, "conversation")) != NULL)
	{
		if ((pConv->pev->origin - pev->origin).Length() > 512)
			continue;
		CBaseEntity* pEntity = CBaseEntity::Instance(pConv->edict());
		if (pEntity && pEntity->AddMemeber(edict()))
		{
			m_pConv = pEntity;
			return;
		}
	}

	CConversation* pNew = CConversation::ConvCreate(pev->origin);
	pNew->AddMemeber(edict());
	m_pConv = pNew;
}

// orig 0x1ff550
void CBarneyBomb::AskQuest()
{
	RCBombSentence(ENT(pev), "BA_QUESTION", RANDOM_LONG(0, 14));
}

// orig 0x1ff5c0
void CBarneyBomb::AnswerQuest()
{
	RCBombSentence(ENT(pev), "BA_ANSWER", RANDOM_LONG(0, 21));
}

// orig 0x1ff650
void CBarneyBomb::Killed(entvars_t* pevAttacker, int iGib)
{
	SciMineExplode();
}

// orig 0x2012c0
void CBarneyBomb::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pConv && m_pConv->pev && m_pConv->edict() == pEdict)
		m_pConv = NULL;
	if (m_pLauncher && m_pLauncher->pev && m_pLauncher->edict() == pEdict)
		m_pLauncher = NULL;
	if (m_pRealOwner && m_pRealOwner == pEdict)
	{
		m_pRealOwner = NULL;
		Killed(pev, GIB_NORMAL);
	}
}

//=========================================================
// The laser spot and guided rocket, as in the SDK RC was built on: CreateRpgRocket does not
// count the rocket on its launcher, and FollowThink neither tracks the launcher nor blows up in
// the sky. Only func_tankrocket makes rpg_rockets in RC; nothing makes a laser spot.
//=========================================================
LINK_ENTITY_TO_CLASS(laser_spot, CLaserSpot); // orig 0x1ff670

// orig 0x1ff6d0
CLaserSpot* CLaserSpot::CreateSpot()
{
	CLaserSpot* pSpot = GetClassPtr((CLaserSpot*)NULL);
	pSpot->Spawn();

	pSpot->pev->classname = MAKE_STRING("laser_spot");

	return pSpot;
}

// orig 0x1ff720
void CLaserSpot::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	pev->rendermode = kRenderGlow;
	pev->renderfx = kRenderFxNoDissipation;
	pev->renderamt = 255;

	SET_MODEL(ENT(pev), "sprites/laserdot.spr");
	UTIL_SetOrigin(pev, pev->origin);
};

//=========================================================
// Suspend- make the laser sight invisible.
//=========================================================
// orig 0x1ff7b0
void CLaserSpot::Suspend(float flSuspendTime)
{
	pev->effects |= EF_NODRAW;

	SetThink(&CLaserSpot::Revive);
	pev->nextthink = gpGlobals->time + flSuspendTime;
}

//=========================================================
// Revive - bring a suspended laser sight back.
//=========================================================
// orig 0x1ff800
void CLaserSpot::Revive()
{
	pev->effects &= ~EF_NODRAW;

	SetThink(NULL);
}

// orig 0x1ff830
void CLaserSpot::Precache()
{
	PRECACHE_MODEL("sprites/laserdot.spr");
};

LINK_ENTITY_TO_CLASS(rpg_rocket, CRpgRocket); // orig 0x1ff860

//=========================================================
//=========================================================
// orig 0x1ff930. Nothing calls it in RC.
CRpgRocket* CRpgRocket::CreateRpgRocket(Vector vecOrigin, Vector vecAngles, CBaseEntity* pOwner, CRpg* pLauncher)
{
	CRpgRocket* pRocket = GetClassPtr((CRpgRocket*)NULL);

	UTIL_SetOrigin(pRocket->pev, vecOrigin);
	pRocket->pev->angles = vecAngles;
	pRocket->Spawn();
	pRocket->SetTouch(&CRpgRocket::RocketTouch);
	pRocket->m_hLauncher = pLauncher; // remember what RPG fired me.
	pRocket->pev->owner = pOwner->edict();

	return pRocket;
}

//=========================================================
//=========================================================
// orig 0x1ff9c0
void CRpgRocket::Spawn()
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);

	pev->classname = MAKE_STRING("rpg_rocket");

	SetThink(&CRpgRocket::IgniteThink);
	SetTouch(&CRpgRocket::ExplodeTouch);

	pev->angles.x -= 30;
	UTIL_MakeVectors(pev->angles);
	pev->angles.x = -(pev->angles.x + 30);

	pev->velocity = gpGlobals->v_forward * 250;
	pev->gravity = 0.5;

	pev->nextthink = gpGlobals->time + 0.4;

	pev->dmg = gSkillData.plrDmgRPG;

	m_flSpawnTime = gpGlobals->time; // Rocket Crowbar 1.9
}

//=========================================================
//=========================================================
// orig 0x1ffb60
void CRpgRocket::RocketTouch(CBaseEntity* pOther)
{
	STOP_SOUND(edict(), CHAN_VOICE, "weapons/rocket1.wav");
	ExplodeTouch(pOther);
}

//=========================================================
//=========================================================
// orig 0x1ffbb0
void CRpgRocket::Precache()
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND("weapons/rocket1.wav");
}


// orig 0x1ffc10
void CRpgRocket::IgniteThink()
{
	// pev->movetype = MOVETYPE_TOSS;

	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5);

	// rocket trail
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);

	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex()); // entity
	WRITE_SHORT(m_iTrail);	 // model
	WRITE_BYTE(40);			 // life
	WRITE_BYTE(5);			 // width
	WRITE_BYTE(224);		 // r, g, b
	WRITE_BYTE(224);		 // r, g, b
	WRITE_BYTE(255);		 // r, g, b
	WRITE_BYTE(255);		 // brightness

	MESSAGE_END(); // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	m_flIgniteTime = gpGlobals->time;

	// set to follow laser spot
	SetThink(&CRpgRocket::FollowThink);
	pev->nextthink = gpGlobals->time + 0.1;
}


// orig 0x1ffda0
void CRpgRocket::FollowThink()
{
	CBaseEntity* pOther = NULL;
	Vector vecTarget;
	Vector vecDir;
	float flDist, flMax, flDot;
	TraceResult tr;

	UTIL_MakeAimVectors(pev->angles);

	vecTarget = gpGlobals->v_forward;
	flMax = 4096;

	// Examine all entities within a reasonable radius
	while ((pOther = UTIL_FindEntityByClassname(pOther, "laser_spot")) != NULL)
	{
		UTIL_TraceLine(pev->origin, pOther->pev->origin, dont_ignore_monsters, ENT(pev), &tr);
		// ALERT( at_console, "%f\n", tr.flFraction );
		if (tr.flFraction >= 0.90)
		{
			vecDir = pOther->pev->origin - pev->origin;
			flDist = vecDir.Length();
			vecDir = vecDir.Normalize();
			flDot = DotProduct(gpGlobals->v_forward, vecDir);
			if ((flDot > 0) && (flDist * (1 - flDot) < flMax))
			{
				flMax = flDist * (1 - flDot);
				vecTarget = vecDir;
			}
		}
	}

	pev->angles = UTIL_VecToAngles(vecTarget);

	// this acceleration and turning math is totally wrong, but it seems to respond well so don't change it.
	float flSpeed = pev->velocity.Length();
	if (gpGlobals->time - m_flIgniteTime < 1.0)
	{
		pev->velocity = pev->velocity * 0.2 + vecTarget * (flSpeed * 0.8 + 400);
		if (pev->waterlevel == 3)
		{
			// go slow underwater
			if (pev->velocity.Length() > 300)
			{
				pev->velocity = pev->velocity.Normalize() * 300;
			}
			UTIL_BubbleTrail(pev->origin - pev->velocity * 0.1, pev->origin, 4);
		}
		else
		{
			if (pev->velocity.Length() > 2000)
			{
				pev->velocity = pev->velocity.Normalize() * 2000;
			}
		}
	}
	else
	{
		if ((pev->effects & EF_LIGHT) != 0)
		{
			pev->effects = 0;
			STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav");
		}
		pev->velocity = pev->velocity * 0.2 + vecTarget * flSpeed * 0.798;
		if (pev->waterlevel == 0 && pev->velocity.Length() < 1500)
		{
			Detonate();
		}
	}
	// ALERT( at_console, "%.0f\n", flSpeed );

	pev->nextthink = gpGlobals->time + 0.1;
}
#endif

// RC's bot object, read as CBasePlayerWeapon::BotWeaponFrame (weapons.cpp) reads it: the
// original i386 bot's offsets.
static entvars_t* RCBotPev(CBot* pBot) { return *reinterpret_cast<entvars_t**>(pBot); }
static float RCBotFireGate(CBot* pBot) { return *reinterpret_cast<float*>(reinterpret_cast<byte*>(pBot) + 0x698); } // VERIFY name
static int RCBotInt6a0(CBot* pBot) { return *reinterpret_cast<int*>(reinterpret_cast<byte*>(pBot) + 0x6a0); }		 // VERIFY: meaning unknown

// orig 0x200410. No ammo or spot checks; the clip size is an rc.ini key, but only an empty clip
// reloads.
void CRpg::Reload()
{
	if (m_iClip == 1)
	{
		// don't bother with any of this if don't need to reload.
		return;
	}

	m_flNextPrimaryAttack = gpGlobals->time + 0.5;

	// VERIFY: the original leaves iResult uninitialized (as Valve's SDK did) when the clip is not
	// empty, so with ClipSize above 1 a part-full clip could reset the idle time. Ours is false.
	bool iResult = false;
	if (m_iClip == 0)
		iResult = DefaultReload(PREFS_KeyLoopUpIntClamp("RPG", "ClipSize", 1, 0, 250), RPG_RELOAD, 2);

	if (iResult)
		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);
}

// orig 0x2004b0
void CRpg::Spawn()
{
	Precache();
	m_iId = WEAPON_RPG;

	SET_MODEL(ENT(pev), "models/w_rpg.mdl");
	m_fSpotActive = false;
	m_cActiveRockets = 0;

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("RPG", "PickupAmmo", 2, 0, 250);

	FallInit(); // get ready to fall down.
}

// orig 0x200530
void CRpg::Precache()
{
	PRECACHE_MODEL("models/w_rpg.mdl");
	PRECACHE_MODEL("models/v_rpg.mdl");
	PRECACHE_MODEL("models/p_rpg.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	UTIL_PrecacheOther("barneymine");
	UTIL_PrecacheOther("scientistbomb");
	UTIL_PrecacheOther("suck_mine");

	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("weapons/glauncher.wav"); // alternative fire sound
}

// orig 0x2005e0
bool CRpg::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "rockets";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("RPG", "MaxAmmo", 10, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = PREFS_KeyLoopUpIntClamp("RPG", "ClipSize", 1, 0, 250);
	p->iSlot = 3;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_RPG;
	p->iFlags = 0;
	p->iWeight = 10;

	return true;
}

// orig 0x2006a0. The original sends WeapPickup when the base AddToPlayer succeeds, which it
// always does there.
void CRpg::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	m_flBotNextFire = -1;
	// As the original: this weapon tells the player it was picked up (AddPlayerItem doesn't)
	MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
	WRITE_BYTE(m_iId);
	MESSAGE_END();
}

// orig 0x200730
bool CRpg::Deploy()
{
	if (m_iClip == 0)
	{
		return DefaultDeploy("models/v_rpg.mdl", "models/p_rpg.mdl", RPG_DRAW_UL, "rpg");
	}

	return DefaultDeploy("models/v_rpg.mdl", "models/p_rpg.mdl", RPG_DRAW1, "rpg");
}

// orig 0x200780
bool CRpg::CanHolster()
{
	return true;
}

// orig 0x200790
void CRpg::Holster()
{
	m_fInReload = false; // cancel any reload in progress.

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	SendWeaponAnim(RPG_HOLSTER1);
}

// orig 0x2007f0. Fires a scientist (with probability SciMineProbability) or a barney, while the
// player has fewer than 10 rpg_rocket-class entities.
void CRpg::PrimaryAttack()
{
#ifndef CLIENT_DLL
	if (0 != m_iClip && CountEntsClaimedByEnt(m_pPlayer->edict(), "rpg_rocket") < 10)
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

		SendWeaponAnim(RPG_FIRE2);

		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
		Vector vecSrc = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -8;

		if (RANDOM_FLOAT(0, 1) >= PREFS_KeyLoopUpFloat("RPG", "SciMineProbability", 0.5))
		{
			CBarneyBomb* pBomb = CBarneyBomb::SciCreate();
			UTIL_SetOrigin(pBomb->pev, vecSrc);
			Vector vecVel = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("RPG", "BarnMineLaunchVel", 500.0);
			pBomb->pev->velocity = vecVel;
			if (m_pPlayer->IsNetClient())
				pBomb->pev->velocity.z += 100;
			pBomb->pev->owner = m_pPlayer->edict();
			pBomb->m_pRealOwner = m_pPlayer->edict();
			pBomb->pev->angles = UTIL_VecToAngles(vecVel);
			pBomb->pev->angles.x = 0;
			pBomb->pev->angles.z = 0;
			pBomb->m_pLauncher = this;
		}
		else
		{
			CScientistBomb* pBomb = CScientistBomb::SciCreate();
			UTIL_SetOrigin(pBomb->pev, vecSrc);
			Vector vecVel = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("RPG", "SciMineLaunchVel", 500.0);
			pBomb->pev->velocity = vecVel;
			if (m_pPlayer->IsNetClient())
				pBomb->pev->velocity.z += 100;
			pBomb->pev->owner = m_pPlayer->edict();
			pBomb->m_pRealOwner = m_pPlayer->edict();
			pBomb->pev->angles = UTIL_VecToAngles(vecVel);
			pBomb->pev->angles.x = 0;
			pBomb->pev->angles.z = 0;
			pBomb->m_pLauncher = this;
		}

		EMIT_SOUND(m_pPlayer->edict(), CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM);
		EMIT_SOUND(m_pPlayer->edict(), CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM);

		m_iClip--;

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("RPG", "SciMineRefire", 1.5) * m_pPlayer->m_fCurseRefireScale;
		m_flTimeWeaponIdle = gpGlobals->time + 1.5;

		m_pPlayer->pev->punchangle.x -= PREFS_KeyLoopUpInt("RPG", "SciAngleKick", 5);

		m_cActiveRockets++;
		return;
	}

	m_flNextPrimaryAttack = gpGlobals->time + 0.2;
	m_flNextSecondaryAttack = gpGlobals->time + 0.2;
	PlayEmptySound();
#endif
}

// orig 0x200d30. Fires a suck mine while the player has fewer than 5. The mine counts in
// m_cActiveRockets, and nothing ever uncounts it.
void CRpg::SecondaryAttack()
{
#ifndef CLIENT_DLL
	if (0 != m_iClip && CountEntsClaimedByEnt(m_pPlayer->edict(), "suck_mine") < 5)
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

		SendWeaponAnim(RPG_FIRE2);

		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
		Vector vecSrc = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -8;

		CSuckMine* pMine = static_cast<CSuckMine*>(CBaseEntity::Create("suck_mine", vecSrc, Vector(0, m_pPlayer->pev->v_angle.y, 0), m_pPlayer->edict()));
		pMine->pev->velocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("RPG", "SuckMineLaunchVel", 500.0);
		if (m_pPlayer->IsNetClient())
			pMine->pev->velocity.z += 100;
		pMine->pev->owner = m_pPlayer->edict();
		pMine->m_pRealOwner = m_pPlayer->edict();

		EMIT_SOUND(m_pPlayer->edict(), CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM);
		EMIT_SOUND(m_pPlayer->edict(), CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM);

		m_iClip--;

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("RPG", "SuckMineRefire", 1.5) * m_pPlayer->m_fCurseRefireScale;
		m_flTimeWeaponIdle = gpGlobals->time + 1.5;

		m_pPlayer->pev->punchangle.x -= PREFS_KeyLoopUpInt("RPG", "SuckMineAngleKick", 5);

		m_cActiveRockets++;
		return;
	}

	m_flNextPrimaryAttack = gpGlobals->time + 0.2;
	m_flNextSecondaryAttack = gpGlobals->time + 0.2;
	PlayEmptySound();
#endif
}

// orig 0x2010c0
void CRpg::WeaponIdle()
{
	ResetEmptySound();

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = RANDOM_FLOAT(0, 1);
		if (flRand > 0.75)
		{
			if (m_iClip == 0)
				iAnim = RPG_FIDGET_UL;
			else
				iAnim = RPG_FIDGET;

			m_flTimeWeaponIdle = gpGlobals->time + 3.0;
		}
		else
		{
			if (m_iClip == 0)
				iAnim = RPG_IDLE_UL;
			else
				iAnim = RPG_IDLE;

			m_flTimeWeaponIdle = gpGlobals->time + 6.0;
		}

		SendWeaponAnim(iAnim);
	}
	else
	{
		m_flTimeWeaponIdle = gpGlobals->time + 1;
	}
}

// orig 0x2011c0. Empty: RC has no laser spot.
void CRpg::UpdateSpot()
{
}

// orig 0x201560. Weak while the bot is holding fire.
int CRpg::GetBotWeight(CBot* pBot, CBaseEntity* pEnemy)
{
	if (!pEnemy)
		return ItemInfoArray[m_iId].iWeight;
	if (gpGlobals->time < m_flBotNextFire)
		return 1;
	return 30;
}

// orig 0x2015c0. Fire when a 500-speed lob from the bot reaches the enemy; when it can't, hold
// fire half a second if the bot's +0x6a0 value is 3. Either button fires a bomb or mine. The
// original also calls pEnemy->GetBotShootOrg() twice and discards the results (no side effects),
// and stores VecToAngles of the throw where nothing reads it.
void CRpg::BotWeaponFrame(CBot* pBot, CBaseEntity* pEnemy)
{
	// VERIFY: guard, the original crashes here (it calls pEnemy->GetBotShootOrg() first, with no check)
	if (!pEnemy)
		return;

	entvars_t* pevBot = RCBotPev(pBot);
	if (m_iClip <= 0)
	{
		pevBot->button |= IN_RELOAD;
		return;
	}

	if (RCBotFireGate(pBot) / 3.0 < 999.0 || 0 == m_iClip)
		return;
	if (!BotFirePrimary(pBot, pEnemy) || m_cActiveRockets > 9)
		return;

	Vector vecTarget = pEnemy->GetBotShootOrg();
	// VERIFY: the original calls the bot's own Center() (vtable slot 50 through its +0x30 vptr);
	// ours computes the default Center() from the bot's entvars
	Vector vecSrc = (pevBot->absmin + pevBot->absmax) * 0.5;
	Vector vecThrow = VecCheckThrow(pev, vecSrc, vecTarget, 500, 0.75);

	if (vecThrow == g_vecZero && RCBotInt6a0(pBot) == 3)
	{
		m_flBotNextFire = gpGlobals->time + 0.5;
		return;
	}

	if (RANDOM_LONG(0, 1) == 1)
		pevBot->button |= IN_ATTACK;
	else
		pevBot->button |= IN_ATTACK2;
}

// orig 0x201850. VERIFY: measures from the enemy's Center() to its own GetBotShootOrg() (16
// units by default) rather than from the bot, so with an enemy it never passes the 800-1512
// window (original).
bool CRpg::BotFirePrimary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (m_flBotNextFire > gpGlobals->time)
		return false;

	if (pEnemy)
	{
		float flDist = (pEnemy->Center() - pEnemy->GetBotShootOrg()).Length();
		if (flDist <= 800 || flDist >= 1512 || m_cActiveRockets > 9)
			return false;
	}
	return true;
}

// orig 0x201810
bool CRpg::BotFireSecondary(CBot* pBot, CBaseEntity* pEnemy)
{
	return BotFirePrimary(pBot, pEnemy);
}

class CRpgAmmo : public CBasePlayerAmmo
{
	// orig 0x2520c0
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_rpgammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x252080
	void Precache() override
	{
		PRECACHE_MODEL("models/w_rpgammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x251fd0
	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(PREFS_KeyLoopUpIntClamp("RPG", "BoxGive", 2, 0, 250), "rockets", PREFS_KeyLoopUpIntClamp("RPG", "MaxAmmo", 10, 0, 250)) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_rpgclip, CRpgAmmo); // orig 0x2011d0
