/***
*
*	Rocket Crowbar 1.9: curses.cpp. Curses are headcrabs (CBaseCurse derives from CHeadCrab) that
*	hop around the map; one that lands on a player attaches to him, hides, and applies its effect
*	until he dies, then respawns somewhere else. curse_spawner makes one of each when mp_curses is
*	1. Every tunable comes from rc.ini's [CURSES] heading, keyed by the curse's name plus a suffix
*	(GetPrefsFloat).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "player.h"
#include "gamerules.h"
#include "weapons.h"
#include "animation.h"
#include "headcrab.h"
#include "curses.h"

#include <cmath>

// Preff.cpp (prefs-votes package)
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
// util.cpp, static there; RC's util.cpp exports them
extern unsigned short FixedUnsigned16(float value, float scale);
extern short FixedSigned16(float value, float scale);

int g_iCurseMSGColor[4] = {255, 64, 64, 160}; // orig .data 0x2826c0

//=========================================================
// CBaseCurseSpawner: waits 15 s, then makes one of each curse and removes itself
//=========================================================
class CBaseCurseSpawner : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT CSThink();
};

LINK_ENTITY_TO_CLASS(curse_spawner, CBaseCurseSpawner); // orig 0x114f80

// orig 0x114fb0
void CreateCurseSpawner()
{
	if (g_pGameRules->IsMultiplayer())
	{
		if (CVAR_GET_FLOAT("mp_curses") == 1)
		{
			if (UTIL_FindEntityByClassname(NULL, "curse_spawner") == NULL)
			{
				// Precache is never called: Spawn only sets the think
				GetClassPtr((CBaseCurseSpawner*)NULL)->Spawn();
			}
		}
	}
}

// orig 0x115030
void CBaseCurseSpawner::Spawn()
{
	SetThink(&CBaseCurseSpawner::CSThink);
	pev->nextthink = gpGlobals->time + 15.0;
	pev->classname = MAKE_STRING("curse_spawner");
}

// orig 0x115090
void CBaseCurseSpawner::Precache()
{
	UTIL_PrecacheOther("monster_headcrab");
}

// orig 0x1150c0. drunkcurse is not among them (seven Create calls in the original).
// With mp_curses off it does nothing and never thinks again.
void CBaseCurseSpawner::CSThink()
{
	if (CVAR_GET_FLOAT("mp_curses") == 1)
	{
		CBaseEntity::Create("reversecurse", g_vecZero, g_vecZero, NULL);
		CBaseEntity::Create("slowcurse", g_vecZero, g_vecZero, NULL);
		CBaseEntity::Create("slowrefirecurse", g_vecZero, g_vecZero, NULL);
		CBaseEntity::Create("dmgmirrorcurse", g_vecZero, g_vecZero, NULL);
		CBaseEntity::Create("dmgscalecurse", g_vecZero, g_vecZero, NULL);
		CBaseEntity::Create("promagcurse", g_vecZero, g_vecZero, NULL);
		CBaseEntity::Create("sdcurse", g_vecZero, g_vecZero, NULL);
		SUB_Remove();
	}
}

//=========================================================
// CBaseCurse
//=========================================================
class CBaseCurse : public CHeadCrab
{
public:
	float GetPrefsFloat(const char* szKey, float flDefault);
	void Spawn() override;
	void MonsterThink() override;
	void SetSpawnOrg();
	void EXPORT LeapyTouch(CBaseEntity* pOther);
	void StartTask(Task_t* pTask) override;
	void Precache() override;
	void SetYawSpeed() override;
	int IRelationship(CBaseEntity* pTarget) override;
	bool CheckRangeAttack1(float flDot, float flDist) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	void AttachToHost(CBaseEntity* pHost);
	void EXPORT AttachedThink();
	void EntIsBeingRemoved(edict_t* pEdict) override;

	// RC's new virtuals, in the original's slot order (153-160)
	virtual void InherAttach();
	virtual void InherDetach();
	virtual void InherATThink(); // think while attached
	virtual void InherSpawn();
	virtual void InherThink(); // think while free
	virtual Vector InherGetShellColor();
	virtual const char* InherGetCurseName();	 // rc.ini key prefix
	virtual const char* InherGetCurseNiceName(); // shown in the "Cursed by:" HUD text

	CBaseEntity* m_pHost;	 // orig +0x27c, the player it is attached to. VERIFY name
	float m_flRespawnTime;	 // orig +0x280, respawn while free once past this (RespawnTime). VERIFY name
	float m_flNextMoveTime;	 // orig +0x284, next random hop. VERIFY name
	float m_flSpawnOrgTime;	 // orig +0x288, when to pick a spawn spot, -1 once done. VERIFY name
};

LINK_ENTITY_TO_CLASS(basecurse, CBaseCurse); // orig 0x1151b0

// orig 0x1151e0: PREFS [CURSES] <curse name><szKey>
float CBaseCurse::GetPrefsFloat(const char* szKey, float flDefault)
{
	char szName[512];
	strncpy(szName, InherGetCurseName(), 511);
	szName[510] = 0;
	strncat(szName, szKey, 511);
	szName[510] = 0;
	return PREFS_KeyLoopUpFloat("CURSES", szName, flDefault);
}

// orig 0x115250
void CBaseCurse::Spawn()
{
	pev->spawnflags |= SF_MONSTER_FALL_TO_GROUND;
	CHeadCrab::Spawn();
	m_flFieldOfView = -1.0;
	pev->classname = MAKE_STRING("curse");
	m_hEnemy = NULL;
	pev->effects = 0;
	pev->health = 50.0;
	m_flRespawnTime = GetPrefsFloat("RespawnTime", 120.0) + gpGlobals->time;
	m_flNextMoveTime = gpGlobals->time + 0.8;
	m_flSpawnOrgTime = gpGlobals->time + 0.2;
	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = InherGetShellColor();
	pev->renderamt = 32.0;
	m_pHost = NULL;
	InherSpawn();
}

// orig 0x115380: CBaseMonster::MonsterThink with random hops, the respawn timer and the shell
// colour added
void CBaseCurse::MonsterThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ((pev->flags & FL_FROZEN) != 0)
	{
		pev->animtime = gpGlobals->time;
		return;
	}

	InherThink();

	if (m_flNextMoveTime < gpGlobals->time && m_hEnemy == NULL)
	{
		pev->flags &= ~FL_ONGROUND;
		UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
		pev->ideal_yaw = RANDOM_FLOAT(0, 360);
		UTIL_MakeVectors(Vector(0, pev->ideal_yaw, 0));
		pev->velocity = gpGlobals->v_forward * 600 + Vector(0, 0, 200);
		float flMax = GetPrefsFloat("MaxMoveTime", 5.0);
		m_flNextMoveTime = RANDOM_FLOAT(GetPrefsFloat("MinMoveTime", 5.0), flMax) + gpGlobals->time;
	}
	else if (m_flRespawnTime < gpGlobals->time && m_hEnemy == NULL)
	{
		Spawn();
	}
	else if (m_flSpawnOrgTime < gpGlobals->time && m_flSpawnOrgTime != -1)
	{
		SetSpawnOrg();
		m_flSpawnOrgTime = -1;
	}
	else
	{
		if (pev->waterlevel != 0)
		{
			pev->velocity = pev->velocity * 0.8;
			if (pev->waterlevel < 2 || pev->velocity.z >= 64)
				pev->velocity.z -= 8;
			else
				pev->velocity.z += 8;
			pev->flags &= ~FL_ONGROUND;
		}

		RunAI();

		float flInterval = StudioFrameAdvance();

		if (m_MonsterState != MONSTERSTATE_SCRIPT && m_MonsterState != MONSTERSTATE_DEAD && m_Activity == ACT_IDLE && m_fSequenceFinished)
		{
			int iSequence;

			if (m_fSequenceLoops)
				iSequence = LookupActivity(ACT_IDLE);
			else
				iSequence = LookupActivityHeaviest(m_Activity);

			if (iSequence != ACTIVITY_NOT_AVAILABLE)
			{
				pev->sequence = iSequence;
				ResetSequenceInfo();
			}
		}

		pev->renderfx = kRenderFxGlowShell;
		pev->rendercolor = InherGetShellColor();
		pev->renderamt = 32.0;

		DispatchAnimEvents(flInterval);

		if (!MovementIsComplete())
			Move(flInterval);
	}
}

// orig 0x115760: true when a curse or a player is within 64 units
bool CheckForCurseNearOrg(Vector vecOrg)
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecOrg, 64.0)) != NULL)
	{
		if (strcmp(STRING(VARS(pEntity->pev->pContainingEntity)->classname), "curse") == 0)
			return true;
		if (strcmp(STRING(VARS(pEntity->pev->pContainingEntity)->classname), "player") == 0)
			return true;
	}

	return false;
}

// orig 0x115840: EntSelectSpawnPoint's deathmatch walk (player.cpp), skipping spots with a curse
// or a player near them
void CBaseCurse::SetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	CBaseEntity* pFirstSpot;
	int iCount = 0;

	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch")) != NULL)
		iCount++;

	pSpot = NULL;
	for (int iRand = RANDOM_LONG(1, iCount); iRand > 0; iRand--)
		pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch");

	if (pSpot == NULL || FNullEnt(pSpot->pev))
		pSpot = UTIL_FindEntityByClassname(NULL, "info_player_deathmatch");
	pFirstSpot = pSpot;

	do
	{
		if (pSpot != NULL)
		{
			if (!(pSpot->pev->origin == Vector(0, 0, 0)))
			{
				if (!CheckForCurseNearOrg(pSpot->pev->origin))
					break;
			}
		}
		pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch");
	} while (pSpot != pFirstSpot);

	if (pSpot == NULL && (pSpot = UTIL_FindEntityByClassname(NULL, "info_player_deathmatch")) == NULL)
	{
		SetThink(&CBaseCurse::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		UTIL_SetOrigin(pev, pSpot->pev->origin);
		pev->flags &= ~FL_ONGROUND;
	}
}

// orig 0x115a40: a player gets the curse, anything else gets bitten
void CBaseCurse::LeapyTouch(CBaseEntity* pOther)
{
	if (pOther == NULL || pOther->pev->takedamage == DAMAGE_NO)
		return;

	if (pOther->Classify() == CLASS_PLAYER)
	{
		if ((pev->flags & FL_ONGROUND) == 0)
		{
			EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "headcrab/hc_headbite.wav", 1.0, ATTN_IDLE, 0, 100);
			AttachToHost(pOther);
		}
	}
	else if ((pev->flags & FL_ONGROUND) == 0)
	{
		pOther->TakeDamage(pev, pev, 15, DMG_SLASH);
		EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "headcrab/hc_headbite.wav", 1.0, ATTN_IDLE, 0, 100);
	}

	SetTouch(NULL);
}

// orig 0x115b20
void CBaseCurse::StartTask(Task_t* pTask)
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	if (pTask->iTask == TASK_RANGE_ATTACK1)
	{
		EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "headcrab/hc_attack1.wav", 1.0, ATTN_IDLE, 0, 100);
		m_IdealActivity = ACT_RANGE_ATTACK1;
		SetTouch(&CBaseCurse::LeapyTouch);
	}
	else
	{
		CBaseMonster::StartTask(pTask);
	}
}

// orig 0x115ba0
void CBaseCurse::Precache()
{
	PRECACHE_MODEL("models/headcrab.mdl");
	CHeadCrab::Precache();
}

// orig 0x115be0
void CBaseCurse::SetYawSpeed()
{
	pev->yaw_speed = GetPrefsFloat("YawSpeed", 120.0);
}

// orig 0x115c20: players only, everything else is ignored
int CBaseCurse::IRelationship(CBaseEntity* pTarget)
{
	if (pTarget->Classify() == CLASS_PLAYER)
		return CBaseMonster::IRelationship(pTarget);
	return R_NO;
}

// orig 0x115c70
bool CBaseCurse::CheckRangeAttack1(float flDot, float flDist)
{
	if ((pev->flags & FL_ONGROUND) != 0)
	{
		if (pev->groundentity && (pev->groundentity->v.flags & (FL_CLIENT | FL_MONSTER)) != 0)
			return true;

		if (flDist <= GetPrefsFloat("JumpRange", 384.0) && flDot >= GetPrefsFloat("JumpMinDot", 0.65))
			return true;
	}
	return false;
}

// orig 0x115d00: never takes damage (health stays 50); keeps CBaseMonster's enemy tracking and
// damage conditions. The conditions are set twice, as in the original.
bool CBaseCurse::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	pev->health = 50.0;

	if ((pev->flags & FL_MONSTER) != 0 && !FNullEnt(pevAttacker) && (pevAttacker->flags & (FL_MONSTER | FL_CLIENT)) != 0)
	{
		if (pevInflictor)
		{
			if (m_hEnemy == NULL || pevInflictor == m_hEnemy->pev || !HasConditions(bits_COND_SEE_ENEMY))
				m_vecEnemyLKP = pevInflictor->origin;
		}
		else
		{
			m_vecEnemyLKP = pev->origin + (g_vecAttackDir * 64);
		}

		MakeIdealYaw(m_vecEnemyLKP);

		if (flDamage > 0)
			SetConditions(bits_COND_LIGHT_DAMAGE);
		if (flDamage >= 20)
			SetConditions(bits_COND_HEAVY_DAMAGE);
	}

	if (flDamage > 0)
		SetConditions(bits_COND_LIGHT_DAMAGE);
	if (flDamage >= 20)
		SetConditions(bits_COND_HEAVY_DAMAGE);

	return true;
}

// orig 0x115ed0: ricochets, no blood
void CBaseCurse::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	UTIL_Ricochet(ptr->vecEndPos, 1.0);
	AddMultiDamage(pevAttacker, this, flDamage, bitsDamageType);
}

// orig 0x115f20
void CBaseCurse::AttachToHost(CBaseEntity* pHost)
{
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	SetThink(&CBaseCurse::AttachedThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_pHost = pHost;
	InherAttach();
}

// orig 0x115fc0: respawns once the host is gone or dead (InherDetach only for a dead host)
void CBaseCurse::AttachedThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_pHost && m_pHost->pev && m_pHost->pev->pContainingEntity && !m_pHost->pev->pContainingEntity->free)
	{
		if (m_pHost->IsAlive())
		{
			InherATThink();
			return;
		}
		InherDetach();
	}

	Spawn();
}

// orig 0x117700
void CBaseCurse::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pHost && m_pHost->pev && m_pHost->pev->pContainingEntity == pEdict)
		m_pHost = NULL;
}

// orig 0x1177f0
void CBaseCurse::InherAttach() {}

// orig 0x1177e0
void CBaseCurse::InherDetach() {}

// orig 0x1177d0
void CBaseCurse::InherATThink() {}

// orig 0x1177c0
void CBaseCurse::InherSpawn() {}

// orig 0x1177b0
void CBaseCurse::InherThink() {}

// orig 0x117780
Vector CBaseCurse::InherGetShellColor()
{
	return Vector(255, 255, 255);
}

// orig 0x117760
const char* CBaseCurse::InherGetCurseName()
{
	return "Base";
}

// orig 0x117740
const char* CBaseCurse::InherGetCurseNiceName()
{
	return "Base (BUG: You shouldnt be seeing this)";
}

//=========================================================
// CCurseBackwards (reversecurse): the host's movement keys run backwards
//=========================================================
class CCurseBackwards : public CBaseCurse
{
public:
	void InherAttach() override;
	void InherDetach() override;
	void InherATThink() override;
	Vector InherGetShellColor() override;
	const char* InherGetCurseName() override;
	const char* InherGetCurseNiceName() override;

	float m_flForwardSpeed;	 // orig +0x28c, cl_forwardspeed sent on detach. VERIFY name
	float m_flBackSpeed;	 // orig +0x290. VERIFY name
	float m_flSideSpeed;	 // orig +0x294. VERIFY name
	float m_flNextSpeedTime; // orig +0x298. VERIFY name
};

LINK_ENTITY_TO_CLASS(reversecurse, CCurseBackwards); // orig 0x116070

// orig 0x1160a0
void CCurseBackwards::InherAttach()
{
	m_flForwardSpeed = 1000.0;
	m_flBackSpeed = 1000.0;
	m_flSideSpeed = 1000.0;
	m_flNextSpeedTime = gpGlobals->time;
}

// orig 0x1160f0
void CCurseBackwards::InherDetach()
{
	CLIENT_COMMAND(m_pHost->pev->pContainingEntity, UTIL_VarArgs("cl_forwardspeed %f\ncl_backspeed %f\ncl_sidespeed %f\n", m_flForwardSpeed, m_flBackSpeed, m_flSideSpeed));
}

// orig 0x116160: every 0.5 s, negative speeds; with Slowness on the same host too, the slow speed
// negated
void CCurseBackwards::InherATThink()
{
	if (m_flNextSpeedTime > gpGlobals->time)
		return;

	m_flNextSpeedTime = gpGlobals->time + 0.5;

	bool bSlow = false;
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityByClassname(pEntity, "curse")) != NULL)
	{
		CBaseCurse* pCurse = static_cast<CBaseCurse*>(pEntity);
		if (pCurse->m_pHost == NULL || pCurse->m_pHost->pev->pContainingEntity != m_pHost->pev->pContainingEntity)
			continue;
		if (strcmp(pCurse->InherGetCurseName(), "Slowness") == 0)
		{
			bSlow = true;
			break;
		}
	}

	float flSlow = PREFS_KeyLoopUpFloat("CURSES", "SlownessSetSpeed", 120.0);
	double flFwd, flBack, flSide;
	if (bSlow)
	{
		flFwd = flBack = flSide = -flSlow;
	}
	else
	{
		flFwd = -m_flForwardSpeed;
		flBack = -m_flBackSpeed;
		flSide = -m_flSideSpeed;
	}

	CLIENT_COMMAND(m_pHost->pev->pContainingEntity, UTIL_VarArgs("cl_forwardspeed %f\ncl_backspeed %f\ncl_sidespeed %f\n", flFwd, flBack, flSide));
}

// orig 0x117670
Vector CCurseBackwards::InherGetShellColor()
{
	return Vector(64, 64, 255);
}

// orig 0x117650
const char* CCurseBackwards::InherGetCurseName()
{
	return "Reversal";
}

// orig 0x117630
const char* CCurseBackwards::InherGetCurseNiceName()
{
	return "Reversal";
}

//=========================================================
// CCurseSlow (slowcurse): the host moves at SlownessSetSpeed
//=========================================================
class CCurseSlow : public CBaseCurse
{
public:
	void InherAttach() override;
	void InherDetach() override;
	void InherATThink() override;
	Vector InherGetShellColor() override;
	const char* InherGetCurseName() override;
	const char* InherGetCurseNiceName() override;

	float m_flForwardSpeed;	 // orig +0x28c. VERIFY name
	float m_flBackSpeed;	 // orig +0x290. VERIFY name
	float m_flSideSpeed;	 // orig +0x294. VERIFY name
	float m_flNextSpeedTime; // orig +0x298. VERIFY name
};

LINK_ENTITY_TO_CLASS(slowcurse, CCurseSlow); // orig 0x1162b0

// orig 0x1162e0
void CCurseSlow::InherAttach()
{
	m_flForwardSpeed = 1000.0;
	m_flBackSpeed = 1000.0;
	m_flSideSpeed = 1000.0;
	m_flNextSpeedTime = gpGlobals->time;
}

// orig 0x116330
void CCurseSlow::InherDetach()
{
	CLIENT_COMMAND(m_pHost->pev->pContainingEntity, UTIL_VarArgs("cl_forwardspeed %f\ncl_backspeed %f\ncl_sidespeed %f\n", m_flForwardSpeed, m_flBackSpeed, m_flSideSpeed));
}

// orig 0x1163a0: every 0.5 s; negated when Reversal is on the same host
void CCurseSlow::InherATThink()
{
	if (m_flNextSpeedTime > gpGlobals->time)
		return;

	m_flNextSpeedTime = gpGlobals->time + 0.5;

	bool bReverse = false;
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityByClassname(pEntity, "curse")) != NULL)
	{
		CBaseCurse* pCurse = static_cast<CBaseCurse*>(pEntity);
		if (pCurse->m_pHost == NULL || pCurse->m_pHost->pev->pContainingEntity != m_pHost->pev->pContainingEntity)
			continue;
		if (strcmp(pCurse->InherGetCurseName(), "Reversal") == 0)
		{
			bReverse = true;
			break;
		}
	}

	float flSpeed = PREFS_KeyLoopUpFloat("CURSES", "SlownessSetSpeed", 120.0);
	if (bReverse)
		flSpeed = -flSpeed;

	CLIENT_COMMAND(m_pHost->pev->pContainingEntity, UTIL_VarArgs("cl_forwardspeed %f\ncl_backspeed %f\ncl_sidespeed %f\n", (double)flSpeed, (double)flSpeed, (double)flSpeed));
}

// orig 0x1175a0
Vector CCurseSlow::InherGetShellColor()
{
	return Vector(16, 255, 16);
}

// orig 0x117580
const char* CCurseSlow::InherGetCurseName()
{
	return "Slowness";
}

// orig 0x117560
const char* CCurseSlow::InherGetCurseNiceName()
{
	return "Slugishness";
}

//=========================================================
// CCurseRefire (slowrefirecurse): the host's weapons refire slower (CBasePlayer::m_fCurseRefireScale)
//=========================================================
class CCurseRefire : public CBaseCurse
{
public:
	void InherAttach() override;
	void InherDetach() override;
	Vector InherGetShellColor() override;
	const char* InherGetCurseName() override;
	const char* InherGetCurseNiceName() override;
};

LINK_ENTITY_TO_CLASS(slowrefirecurse, CCurseRefire); // orig 0x1164d0

// orig 0x116500
void CCurseRefire::InherAttach()
{
	if (m_pHost)
		static_cast<CBasePlayer*>(m_pHost)->m_fCurseRefireScale = GetPrefsFloat("RefireScale", 2.0);
}

// orig 0x116540
void CCurseRefire::InherDetach()
{
	if (m_pHost)
		static_cast<CBasePlayer*>(m_pHost)->m_fCurseRefireScale = 1.0;
}

// orig 0x1174e0
Vector CCurseRefire::InherGetShellColor()
{
	return Vector(255, 16, 16);
}

// orig 0x1174c0
const char* CCurseRefire::InherGetCurseName()
{
	return "Slowrefire";
}

// orig 0x1174a0
const char* CCurseRefire::InherGetCurseNiceName()
{
	return "Slow Fire";
}

//=========================================================
// CCurseDamageMirror (dmgmirrorcurse): damage the host deals comes back (m_fCurseDmgMirror)
//=========================================================
class CCurseDamageMirror : public CBaseCurse
{
public:
	void InherAttach() override;
	void InherDetach() override;
	Vector InherGetShellColor() override;
	const char* InherGetCurseName() override;
	const char* InherGetCurseNiceName() override;
};

LINK_ENTITY_TO_CLASS(dmgmirrorcurse, CCurseDamageMirror); // orig 0x116560

// orig 0x116590
void CCurseDamageMirror::InherAttach()
{
	if (m_pHost)
		static_cast<CBasePlayer*>(m_pHost)->m_fCurseDmgMirror = GetPrefsFloat("MirrorScale", 1.0);
}

// orig 0x1165d0
void CCurseDamageMirror::InherDetach()
{
	if (m_pHost)
		static_cast<CBasePlayer*>(m_pHost)->m_fCurseDmgMirror = 0;
}

// orig 0x117420
Vector CCurseDamageMirror::InherGetShellColor()
{
	return Vector(16, 255, 255);
}

// orig 0x117400
const char* CCurseDamageMirror::InherGetCurseName()
{
	return "DmgMirror";
}

// orig 0x1173e0
const char* CCurseDamageMirror::InherGetCurseNiceName()
{
	return "Damage Mirror";
}

//=========================================================
// CCurseDamageScale (dmgscalecurse): the host deals less damage (m_fCurseDmgScale)
//=========================================================
class CCurseDamageScale : public CBaseCurse
{
public:
	void InherAttach() override;
	void InherDetach() override;
	Vector InherGetShellColor() override;
	const char* InherGetCurseName() override;
	const char* InherGetCurseNiceName() override;
};

LINK_ENTITY_TO_CLASS(dmgscalecurse, CCurseDamageScale); // orig 0x1165f0

// orig 0x116620
void CCurseDamageScale::InherAttach()
{
	if (m_pHost)
		static_cast<CBasePlayer*>(m_pHost)->m_fCurseDmgScale = GetPrefsFloat("AppliedScale", 0.5);
}

// orig 0x116660
void CCurseDamageScale::InherDetach()
{
	if (m_pHost)
		static_cast<CBasePlayer*>(m_pHost)->m_fCurseDmgScale = 1.0;
}

// orig 0x117350
Vector CCurseDamageScale::InherGetShellColor()
{
	return Vector(255, 16, 255);
}

// orig 0x117330
const char* CCurseDamageScale::InherGetCurseName()
{
	return "DmgScale";
}

// orig 0x117310
const char* CCurseDamageScale::InherGetCurseNiceName()
{
	return "Lessen Damage";
}

//=========================================================
// CCurseDrunk (drunkcurse): the host's view sways and his FOV breathes. curse_spawner never
// makes one; only a map or a .lec file can.
//=========================================================
class CCurseDrunk : public CBaseCurse
{
public:
	void InherAttach() override;
	void InherDetach() override;
	void InherATThink() override;
	Vector InherGetShellColor() override;
	const char* InherGetCurseName() override;
	const char* InherGetCurseNiceName() override;

	float m_flAttachTime; // orig +0x28c. VERIFY name
};

LINK_ENTITY_TO_CLASS(drunkcurse, CCurseDrunk); // orig 0x116680

// orig 0x1166b0
void CCurseDrunk::InherAttach()
{
	m_flAttachTime = gpGlobals->time;
}

// orig 0x1166e0
void CCurseDrunk::InherDetach()
{
	m_pHost->pev->angles.x = m_pHost->pev->v_angle.x;
	m_pHost->pev->angles.y = m_pHost->pev->v_angle.y;
	m_pHost->pev->v_angle.z = 0;
	m_pHost->pev->angles.z = 0;
	m_pHost->pev->fixangle = 1;
	if (m_pHost)
		static_cast<CBasePlayer*>(m_pHost)->m_iFOV = 0;
}

// orig 0x116750
void CCurseDrunk::InherATThink()
{
	float flPhase = (gpGlobals->time - m_flAttachTime) * 0.15915494; // 1 / (2 pi)
	while (flPhase > 6.2831855)
		flPhase -= 6.2831855;
	while (flPhase < 0)
		flPhase += 6.2831855;

	// the roll term is g_vecZero's z, so roll never changes
	Vector vecSway = g_vecZero;
	double flCos = cos(flPhase * 1.2);
	float flYScale = GetPrefsFloat("CircleYScale", 0.4);
	double flSin = sin(flPhase * 1.4);
	float flXScale = GetPrefsFloat("CircleXScale", 0.2);
	vecSway.x = flSin * flXScale;
	vecSway.y = flCos * flYScale;

	m_pHost->pev->v_angle.x = m_pHost->pev->v_angle.x + vecSway.x;
	m_pHost->pev->angles.x = m_pHost->pev->v_angle.x;
	m_pHost->pev->v_angle.y = m_pHost->pev->v_angle.y + vecSway.y;
	m_pHost->pev->angles.y = m_pHost->pev->v_angle.y;
	m_pHost->pev->v_angle.z = m_pHost->pev->v_angle.z + vecSway.z;
	m_pHost->pev->angles.z = m_pHost->pev->v_angle.z;
	m_pHost->pev->fixangle = 1;

	if (m_pHost)
	{
		// cos(3t) squared: the FOV runs from FOVMin to halfway to FOVMax and back
		double flCos3 = cos(flPhase * 3.0);
		float flMax = GetPrefsFloat("FOVMax", 120.0);
		float flMin = GetPrefsFloat("FOVMin", 60.0);
		static_cast<CBasePlayer*>(m_pHost)->m_iFOV = (int)((flMax - flMin) * 0.5 * flCos3 * flCos3 + GetPrefsFloat("FOVMin", 60.0) + 0.5);
	}

	pev->nextthink = gpGlobals->time + 0.01;
}

// orig 0x117280
Vector CCurseDrunk::InherGetShellColor()
{
	return Vector(255, 255, 16);
}

// orig 0x117260
const char* CCurseDrunk::InherGetCurseName()
{
	return "Drunkness";
}

// orig 0x117240
const char* CCurseDrunk::InherGetCurseNiceName()
{
	return "Drunkness";
}

//=========================================================
// CCurseProMagnet (promagcurse): pulls projectiles within Radius toward the host
//=========================================================
class CCurseProMagnet : public CBaseCurse
{
public:
	void InherATThink() override;
	Vector InherGetShellColor() override;
	const char* InherGetCurseName() override;
	const char* InherGetCurseNiceName() override;
};

LINK_ENTITY_TO_CLASS(promagcurse, CCurseProMagnet); // orig 0x116990

#define RC_CAP_PROMAGNET 0x20000000 // the RC cap the magnet pulls. VERIFY name

// orig 0x1169c0
void CCurseProMagnet::InherATThink()
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, m_pHost->Center(), GetPrefsFloat("Radius", 768.0))) != NULL)
	{
		int iCap = RC_CAP_PROMAGNET;
		if (!pEntity->HasRCCap(iCap))
			continue;

		Vector vecEnd = pEntity->Center();
		Vector vecStart = m_pHost->Center();
		TraceResult tr;
		UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, m_pHost->pev->pContainingEntity, &tr);

		if (tr.flFraction >= 1.0 || tr.pHit == pEntity->pev->pContainingEntity)
		{
			Vector vecDir = (m_pHost->Center() - pEntity->Center()).Normalize();
			pEntity->ProMagApplyForce(m_pHost, vecDir);
		}
	}

	pev->nextthink = gpGlobals->time + 0.2;
}

// orig 0x1171b0
Vector CCurseProMagnet::InherGetShellColor()
{
	return Vector(255, 128, 64);
}

// orig 0x117190
const char* CCurseProMagnet::InherGetCurseName()
{
	return "ProMagnet";
}

// orig 0x117170
const char* CCurseProMagnet::InherGetCurseNiceName()
{
	return "Projectile Magnet";
}

//=========================================================
// CCurseSDeath (sdcurse): bleeds the host Dmg every DmgInterval
//=========================================================
class CCurseSDeath : public CBaseCurse
{
public:
	void InherAttach() override;
	void InherATThink() override;
	Vector InherGetShellColor() override;
	const char* InherGetCurseName() override;
	const char* InherGetCurseNiceName() override;

	float m_flNextDmgTime; // orig +0x28c. VERIFY name
};

LINK_ENTITY_TO_CLASS(sdcurse, CCurseSDeath); // orig 0x116bb0

// orig 0x116be0
void CCurseSDeath::InherAttach()
{
	m_flNextDmgTime = gpGlobals->time;
}

// orig 0x116c10
void CCurseSDeath::InherATThink()
{
	if (m_flNextDmgTime > gpGlobals->time)
		return;

	m_flNextDmgTime = GetPrefsFloat("DmgInterval", 0.5) + gpGlobals->time;
	m_pHost->TakeDamage(pev, m_pHost->pev, GetPrefsFloat("Dmg", 1.0), DMG_SLASH);
	UTIL_BloodDrips(m_pHost->Center(), g_vecZero, m_pHost->BloodColor(), 255);
}

// orig 0x1170e0
Vector CCurseSDeath::InherGetShellColor()
{
	return Vector(255, 255, 16);
}

// orig 0x1170c0
const char* CCurseSDeath::InherGetCurseName()
{
	return "SlowDeath";
}

// orig 0x1170a0
const char* CCurseSDeath::InherGetCurseNiceName()
{
	return "Degeneration";
}

//=========================================================
// HUD text
//=========================================================

// orig 0x116cf0: "Cursed by: A, B, C,\nD, ..." (a line break after every third), as a
// TE_TEXTMESSAGE on channel 8 at the top centre
void DrawCursedMsg(CBasePlayer* pPlayer)
{
	if (pPlayer == NULL)
		return;
	if (pPlayer->m_fCurseMSGTime > gpGlobals->time)
		return;

	char szMsg[512];
	bool bAny = false;
	int iCount = 0;
	CBaseEntity* pEntity = NULL;

	pPlayer->m_fCurseMSGTime = gpGlobals->time + 0.5;
	strcpy(szMsg, "Cursed by: ");

	while ((pEntity = UTIL_FindEntityByClassname(pEntity, "curse")) != NULL)
	{
		CBaseCurse* pCurse = static_cast<CBaseCurse*>(pEntity);
		if (pCurse->m_pHost == NULL || pCurse->m_pHost->pev->pContainingEntity != pPlayer->pev->pContainingEntity)
			continue;

		if (bAny)
		{
			strncat(szMsg, (iCount + 1) % 3 == 0 ? ",\n" : ", ", 511);
			szMsg[511] = 0;
		}
		else
		{
			bAny = true;
		}
		strncat(szMsg, pCurse->InherGetCurseNiceName(), 511);
		szMsg[511] = 0;
		iCount++;
	}

	if (!bAny)
		return;

	szMsg[510] = 0;
	MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, pPlayer->pev->pContainingEntity);
	WRITE_BYTE(TE_TEXTMESSAGE);
	WRITE_BYTE(8); // channel
	WRITE_SHORT(FixedSigned16(-1, 1 << 13));
	WRITE_SHORT(FixedSigned16(0, 1 << 13));
	WRITE_BYTE(0); // effect
	WRITE_BYTE(g_iCurseMSGColor[0]);
	WRITE_BYTE(g_iCurseMSGColor[1]);
	WRITE_BYTE(g_iCurseMSGColor[2]);
	WRITE_BYTE(g_iCurseMSGColor[3]);
	WRITE_BYTE(g_iCurseMSGColor[0]);
	WRITE_BYTE(g_iCurseMSGColor[1]);
	WRITE_BYTE(g_iCurseMSGColor[2]);
	WRITE_BYTE(g_iCurseMSGColor[3]);
	WRITE_SHORT(0);							   // fade in
	WRITE_SHORT(0);							   // fade out
	WRITE_SHORT(FixedUnsigned16(0.5, 1 << 8)); // hold
	WRITE_STRING(szMsg);
	MESSAGE_END();
}

// orig 0x117090
void DropCurses(CBaseEntity* pEntity, Vector vecOrg)
{
}
