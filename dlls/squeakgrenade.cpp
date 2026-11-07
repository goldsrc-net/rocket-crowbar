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
*	Rocket Crowbar 1.9: snarks shoot lasers at their enemy, and secondary fire sticks a snark
*	cam (CSqueakCam, snark_cam) to a wall. Reload looks through a cam (and back), secondary
*	cycles through the player's cams, and primary fires the cam's laser while looking through
*	it. Numbers come from rc.ini's [SNARKS] heading.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"
#include "effects.h"
#include "client.h"
#include "decals.h"
#include "Preff.h"

enum w_squeak_e
{
	WSQUEAK_IDLE1 = 0,
	WSQUEAK_FIDGET,
	WSQUEAK_JUMP,
	WSQUEAK_RUN,
};

// The original passes NULL for the PVS and PAS, which RC's SetupVisibility (client.cpp) accepts.
static void SnarkSetupVisibility(edict_t* pViewEntity, edict_t* pClient)
{
	SetupVisibility(pViewEntity, pClient, NULL, NULL);
}

//=========================================================
// CSqueakCam: a snark stuck to a wall that a player can look and shoot through
//=========================================================
class CSqueakCam : public CBaseMonster
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	static CSqueakCam* CreateCam(TraceResult* ptr, CBaseEntity* pOwner);
	void Spawn() override;
	void Precache() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	void EntIsBeingRemoved(edict_t* pEdict) override;
	int BloodColor() override;
	int Classify() override;

	void SCamPop();
	void EXPORT SCamThink();

	float m_flDie;			   // orig +0x27c
	float m_flNextFidget;	   // orig +0x280
	EHANDLE m_hOwner;		   // orig +0x284, the player who placed it
	Vector m_vecPlaneNormal;   // orig +0x28c, normal of the wall it's on
	Vector m_vecLastOwnAng;	   // orig +0x298, the owner's v_angle last think
	Vector m_vecBaseDir;	   // orig +0x2a4, its laser's straight-out angles
	Vector m_vecCurDir;		   // orig +0x2b0, the laser's turn from m_vecBaseDir
	bool m_bInFire;			   // orig +0x2bc, laser on
	bool m_bOwnerInCam;		   // orig +0x2bd, its owner is looking through it
	float m_fLastFrame;		   // orig +0x2c0
	Vector m_angOwner;		   // orig +0x2c4, angles of the wall entity it's on
	Vector m_posOwner;		   // orig +0x2d0, origin of the wall entity it's on
	CBaseEntity* m_pViewEnt;   // orig +0x2dc, the invisible entity the view is set to
	CBaseEntity* m_pBeam;	   // orig +0x2e0, the laser
	float m_fBeamDestroyTime;  // orig +0x2e4, when m_pBeam removes itself
};

LINK_ENTITY_TO_CLASS(snark_cam, CSqueakCam);

TYPEDESCRIPTION CSqueakCam::m_SaveData[] =
	{
		DEFINE_FIELD(CSqueakCam, m_flDie, FIELD_TIME),
		DEFINE_FIELD(CSqueakCam, m_flNextFidget, FIELD_TIME),
		DEFINE_FIELD(CSqueakCam, m_hOwner, FIELD_EHANDLE),
		DEFINE_FIELD(CSqueakCam, m_vecPlaneNormal, FIELD_VECTOR),
		DEFINE_FIELD(CSqueakCam, m_vecLastOwnAng, FIELD_VECTOR),
		DEFINE_FIELD(CSqueakCam, m_vecBaseDir, FIELD_VECTOR),
		DEFINE_FIELD(CSqueakCam, m_vecCurDir, FIELD_VECTOR),
		DEFINE_FIELD(CSqueakCam, m_bInFire, FIELD_BOOLEAN),
		DEFINE_FIELD(CSqueakCam, m_bOwnerInCam, FIELD_BOOLEAN),
		DEFINE_FIELD(CSqueakCam, m_fLastFrame, FIELD_TIME),
		DEFINE_FIELD(CSqueakCam, m_angOwner, FIELD_VECTOR),
		DEFINE_FIELD(CSqueakCam, m_posOwner, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CSqueakCam, m_pViewEnt, FIELD_CLASSPTR),
		DEFINE_FIELD(CSqueakCam, m_pBeam, FIELD_CLASSPTR),
		DEFINE_FIELD(CSqueakCam, m_fBeamDestroyTime, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CSqueakCam, CBaseMonster); // orig 0x218460, 0x2184b0

// orig 0x218500: a cam where ptr hit, owned by pOwner. Removes itself when ptr hit nothing.
CSqueakCam* CSqueakCam::CreateCam(TraceResult* ptr, CBaseEntity* pOwner)
{
	CSqueakCam* pCam = GetClassPtr((CSqueakCam*)NULL);

	if (ptr->flFraction >= 1.0 || !ptr->pHit)
	{
		pCam->SetThink(&CBaseEntity::SUB_Remove);
		pCam->pev->nextthink = gpGlobals->time + 0.1;
		return pCam;
	}

	pCam->pev->owner = ptr->pHit;
	pCam->m_angOwner = VARS(pCam->pev->owner)->angles;
	pCam->m_posOwner = VARS(pCam->pev->owner)->origin;
	pCam->m_hOwner = pOwner;
	pCam->m_vecPlaneNormal = ptr->vecPlaneNormal;
	pCam->m_vecBaseDir = UTIL_VecToAngles(ptr->vecPlaneNormal);
	pCam->pev->angles = pCam->m_vecBaseDir;
	pCam->m_vecBaseDir.x = -pCam->m_vecBaseDir.x;

	UTIL_SetOrigin(pCam->pev, ptr->vecEndPos + ptr->vecPlaneNormal + ptr->vecPlaneNormal);
	pCam->Spawn();

	return pCam;
}

// orig 0x2186e0
void CSqueakCam::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/w_squeak.mdl");
	UTIL_SetSize(pev, Vector(-2, -2, -2), Vector(2, 2, 2));
	UTIL_SetOrigin(pev, pev->origin);

	m_flNextFidget = gpGlobals->time + RANDOM_FLOAT(0.5, 1.5);
	pev->classname = MAKE_STRING("snark_cam");
	SetThink(&CSqueakCam::SCamThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_fLastFrame = gpGlobals->time;
	m_pBeam = NULL;

	pev->flags |= FL_MONSTER;
	pev->takedamage = DAMAGE_AIM;
	pev->health = PREFS_KeyLoopUpFloat("SNARKS", "CamHealth", gSkillData.snarkHealth);
	pev->dmg = PREFS_KeyLoopUpFloat("SNARKS", "CamPopDmg", gSkillData.snarkDmgPop);
	m_flDie = gpGlobals->time + PREFS_KeyLoopUpFloat("SNARKS", "CamLife", 240.0);

	// the player's view sits a little out from the wall
	m_pViewEnt = GetClassPtr((CBaseEntity*)NULL);
	m_pViewEnt->Spawn();
	UTIL_SetOrigin(m_pViewEnt->pev, pev->origin + m_vecPlaneNormal * 4);
	UTIL_SetSize(m_pViewEnt->pev, g_vecZero, g_vecZero);
	m_pViewEnt->pev->angles = m_vecBaseDir;
	m_pViewEnt->pev->v_angle = m_pViewEnt->pev->angles;
	m_pViewEnt->pev->angles.x = m_pViewEnt->pev->v_angle.x = m_vecBaseDir.x;
	m_pViewEnt->pev->view_ofs = g_vecZero;
	m_pViewEnt->pev->movetype = MOVETYPE_NONE;
	m_pViewEnt->pev->solid = SOLID_NOT;
	SET_MODEL(m_pViewEnt->edict(), "models/w_isotopebox.mdl");
	m_pViewEnt->pev->classname = MAKE_STRING("sncam_viewent");
	m_pViewEnt->pev->owner = edict();
	m_pViewEnt->pev->rendermode = kRenderTransColor;
	m_pViewEnt->pev->renderamt = 0;

	pev->sequence = WSQUEAK_IDLE1;
	ResetSequenceInfo();
}

// orig 0x218a60
void CSqueakCam::Precache()
{
	PRECACHE_MODEL("models/w_squeak.mdl");
	PRECACHE_SOUND("squeek/sqk_blast1.wav");
	PRECACHE_SOUND("common/bodysplat.wav");
	PRECACHE_SOUND("squeek/sqk_die1.wav");
	PRECACHE_SOUND("squeek/sqk_hunt1.wav");
	PRECACHE_SOUND("squeek/sqk_hunt2.wav");
	PRECACHE_SOUND("squeek/sqk_hunt3.wav");
	PRECACHE_SOUND("squeek/sqk_deploy1.wav");
	PRECACHE_SOUND("debris/beamstart11.wav");
}

// orig 0x218b20
void CSqueakCam::Killed(entvars_t* pevAttacker, int iGib)
{
	SCamPop();
}

// orig 0x218b40: pops like a snark, giving the owner his view back.
void CSqueakCam::SCamPop()
{
	pev->model = iStringNull; // make invisible
	SetThink(&CBaseEntity::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->takedamage = DAMAGE_NO;

	if (m_bOwnerInCam && m_hOwner != NULL && m_hOwner->edict() && 0 == m_hOwner->edict()->free && m_hOwner->edict()->pvPrivateData)
	{
		SET_VIEW(m_hOwner->edict(), m_hOwner->edict());
		SnarkSetupVisibility(m_hOwner->edict(), m_hOwner->edict());
		m_bOwnerInCam = false;

		CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(m_hOwner->edict());
		if (pPlayer->m_pActiveItem)
		{
			// VERIFY: the original clears the byte at CSqueak::m_bInCam's offset in whatever
			// weapon is out; ours only when it is the snark (dossier, "Original bugs")
			CSqueak* pSqueak = dynamic_cast<CSqueak*>(pPlayer->m_pActiveItem);
			if (pSqueak)
				pSqueak->m_bInCam = false;
		}
	}

	if (m_pViewEnt && m_pViewEnt->pev && m_pViewEnt->pev->pContainingEntity && 0 == m_pViewEnt->pev->pContainingEntity->free)
		m_pViewEnt->SUB_Remove();

	if (m_bInFire && m_fBeamDestroyTime > gpGlobals->time && m_pBeam && m_pBeam->pev && !FNullEnt(m_pBeam->pev))
	{
		edict_t* pBeamEdict = m_pBeam->pev->pContainingEntity;
		if (pBeamEdict && 0 == pBeamEdict->free && pBeamEdict->pvPrivateData)
			m_pBeam->SUB_Remove();
	}

	// play squeek blast
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "squeek/sqk_blast1.wav", 1, 0.5, 0, PITCH_NORM);
	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, SMALL_EXPLOSION_VOLUME, 3.0);
	UTIL_BloodDrips(pev->origin, g_vecZero, BloodColor(), 80);

	if (m_hOwner != NULL)
		pev->owner = m_hOwner->edict();
	else
		pev->owner = edict();

	RadiusDamage(pev, pev->owner ? VARS(pev->owner) : NULL, pev->dmg, CLASS_NONE, DMG_BLAST);
}

// orig 0x218f00
void CSqueakCam::SCamThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	// gone when its life is up, its owner or wall is gone, or the wall moves or vanishes
	if (m_flDie < gpGlobals->time || m_hOwner == NULL || !pev->owner || !pev->owner->pvPrivateData || 0 != pev->owner->free)
	{
		SCamPop();
		return;
	}
	entvars_t* pevWall = VARS(pev->owner);
	if (!(pevWall->angles == m_angOwner) || !(pevWall->origin == m_posOwner) || (pevWall->effects & EF_NODRAW) != 0 || pevWall->solid == SOLID_NOT)
	{
		SCamPop();
		return;
	}

	if (!m_hOwner->IsAlive() || m_hOwner->pev->deadflag != DEAD_NO)
	{
		m_bOwnerInCam = false;
		m_bInFire = false;
		return;
	}

	// the owner's mouse turns the laser, up to CamMaxDeltaAng from straight out
	if (!(m_hOwner->pev->v_angle == m_vecLastOwnAng))
	{
		Vector vecDelta = m_hOwner->pev->v_angle - m_vecLastOwnAng;
		if (vecDelta.y > 180)
			vecDelta.y -= 360;
		if (vecDelta.y < -180)
			vecDelta.y += 360;

		m_vecCurDir.x += vecDelta.x;
		m_vecCurDir.y += vecDelta.y;
		m_vecCurDir.z = 0;

		float flMax = PREFS_KeyLoopUpFloat("SNARKS", "CamMaxDeltaAng", 35.0);
		if (m_vecCurDir.x < -flMax)
			m_vecCurDir.x = -flMax;
		else if (m_vecCurDir.x > flMax)
			m_vecCurDir.x = flMax;
		if (m_vecCurDir.y < -flMax)
			m_vecCurDir.y = -flMax;
		else if (m_vecCurDir.y > flMax)
			m_vecCurDir.y = flMax;

		m_vecLastOwnAng = m_hOwner->pev->v_angle;
	}

	if (m_bOwnerInCam && m_bInFire)
	{
		bool bBeamOk = false;
		if (m_pBeam && m_pBeam->pev && !FNullEnt(m_pBeam->pev))
		{
			edict_t* pBeamEdict = m_pBeam->pev->pContainingEntity;
			bBeamOk = pBeamEdict && !FNullEnt(pBeamEdict) && 0 == pBeamEdict->free;
		}
		if (!bBeamOk)
			m_pBeam = CBeam::BeamCreate(g_pModelNameLaser, 10);

		CBeam* pBeam = (CBeam*)m_pBeam;
		pBeam->PointEntInit(pev->origin, entindex());
		pBeam->pev->rendercolor = Vector(255, 0, 0);
		pBeam->pev->animtime = 50; // scroll rate
		pBeam->pev->renderamt = 255;

		Vector vecSrc = pev->origin + m_vecPlaneNormal + m_vecPlaneNormal;
		UTIL_MakeVectors(m_vecBaseDir + m_vecCurDir);
		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 4096, dont_ignore_monsters, ENT(pev), &tr);

		if (tr.pHit && CBaseEntity::Instance(tr.pHit))
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			float flDamage = PREFS_KeyLoopUpFloat("SNARKS", "CamLaserDamagePerSec", 75.0);

			// damage is done by a "monster_snark", for the death message
			pev->classname = MAKE_STRING("monster_snark");
			ClearMultiDamage();
			pHit->TraceAttack(pev, (gpGlobals->time - m_fLastFrame) * flDamage, gpGlobals->v_forward, &tr, DMG_ENERGYBEAM);
			ApplyMultiDamage(pev, m_hOwner->pev);
			pev->classname = MAKE_STRING("snark_cam");
		}

		if (CVAR_GET_FLOAT("sv_lowlag") != 1)
		{
			UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));
			UTIL_Sparks(tr.vecEndPos);
		}

		m_pBeam->pev->origin = tr.vecEndPos;
		m_pBeam->SetThink(&CBaseEntity::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 5.0;
		m_fBeamDestroyTime = gpGlobals->time + 5.0;

		if ((m_hOwner->pev->button & IN_ATTACK) == 0)
		{
			m_bInFire = false;
			m_pBeam->SetThink(&CBaseEntity::SUB_Remove);
			m_pBeam->pev->nextthink = gpGlobals->time + 0.01;
			m_fBeamDestroyTime = gpGlobals->time + 0.01;
			m_pBeam = NULL;
		}
	}

	if (!(m_bOwnerInCam && m_bInFire) && m_pBeam)
	{
		m_bInFire = false;
		m_pBeam->SetThink(&CBaseEntity::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 0.01;
		m_fBeamDestroyTime = gpGlobals->time + 0.01;
		m_pBeam = NULL;
	}

	if (m_flNextFidget < gpGlobals->time)
		m_flNextFidget = gpGlobals->time + RANDOM_FLOAT(0.5, 1.5);

	if (m_bOwnerInCam && m_hOwner != NULL && m_hOwner->edict())
		SnarkSetupVisibility(edict(), m_hOwner->edict());

	m_fLastFrame = gpGlobals->time;
}

// orig 0x21c280
void CSqueakCam::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pViewEnt && m_pViewEnt->pev && m_pViewEnt->pev->pContainingEntity == pEdict)
		m_pViewEnt = NULL;

	if (m_hOwner != NULL && m_hOwner->edict() == pEdict)
	{
		m_hOwner = NULL;
		Killed(pev, 0);
	}
}

// orig 0x21c310
int CSqueakCam::BloodColor()
{
	return BLOOD_COLOR_YELLOW;
}

// orig 0x21c320
int CSqueakCam::Classify()
{
	return CLASS_INSECT;
}

//=========================================================
// CSqueakGrenade: the snark
//=========================================================
class CSqueakGrenade : public CGrenade
{
	void Spawn() override;
	void Precache() override;
	int Classify() override;
	void EXPORT SuperBounceTouch(CBaseEntity* pOther);
	void EXPORT HuntThink();
	int BloodColor() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	void GibMonster() override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	static TYPEDESCRIPTION m_SaveData[];

	static float m_flNextBounceSoundTime;

	// Rocket Crowbar 1.9
	void ShootLaser();
	void EntIsBeingRemoved(edict_t* pEdict) override;
	bool BotTargetRealseable(CBaseEntity* pBot) override;
	int GetBotTargetWeight(CBaseEntity* pBot) override;
	bool IsEntEnemy(CBaseEntity* pEntity) override;
	Vector GetBotShootOrg() override;
	int GetRCCaps() override;
	int GetGuassDistOut() override;
	int GetGuassHull() override;

	// CBaseEntity *m_pTarget;
	float m_flDie;		// orig +0x280
	Vector m_vecTarget; // orig +0x284
	float m_flNextHunt; // orig +0x290
	float m_flNextLaser; // orig +0x294
	float m_flNextHit;	// orig +0x298
	Vector m_posPrev;	// orig +0x29c
	EHANDLE m_hOwner;	// orig +0x2a8
	int m_iMyClass;		// orig +0x2b0
};

float CSqueakGrenade::m_flNextBounceSoundTime = 0;

LINK_ENTITY_TO_CLASS(monster_snark, CSqueakGrenade);
TYPEDESCRIPTION CSqueakGrenade::m_SaveData[] =
	{
		DEFINE_FIELD(CSqueakGrenade, m_flDie, FIELD_TIME),
		DEFINE_FIELD(CSqueakGrenade, m_vecTarget, FIELD_VECTOR),
		DEFINE_FIELD(CSqueakGrenade, m_flNextHunt, FIELD_TIME),
		DEFINE_FIELD(CSqueakGrenade, m_flNextLaser, FIELD_TIME),
		DEFINE_FIELD(CSqueakGrenade, m_flNextHit, FIELD_TIME),
		DEFINE_FIELD(CSqueakGrenade, m_posPrev, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CSqueakGrenade, m_hOwner, FIELD_EHANDLE),
};

IMPLEMENT_SAVERESTORE(CSqueakGrenade, CGrenade); // orig 0x219890, 0x2198e0

// orig 0x219930
int CSqueakGrenade::Classify()
{
	if (m_iMyClass != 0)
		return m_iMyClass; // protect against recursion

	if (m_hEnemy != NULL)
	{
		m_iMyClass = CLASS_INSECT; // no one cares about it
		switch (m_hEnemy->Classify())
		{
		case CLASS_PLAYER:
		case CLASS_HUMAN_PASSIVE:
		case CLASS_HUMAN_MILITARY:
			m_iMyClass = 0;
			return CLASS_ALIEN_MILITARY; // barney's get mad, grunts get mad at it
		}
		m_iMyClass = 0;
	}

	return CLASS_ALIEN_BIOWEAPON;
}

// orig 0x2199c0
void CSqueakGrenade::Spawn()
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_squeak.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, 0), Vector(4, 4, 8));
	UTIL_SetOrigin(pev, pev->origin);

	SetTouch(&CSqueakGrenade::SuperBounceTouch);
	SetThink(&CSqueakGrenade::HuntThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time + 1E6;
	m_flNextLaser = gpGlobals->time;

	pev->flags |= FL_MONSTER;
	pev->takedamage = DAMAGE_AIM;
	pev->health = PREFS_KeyLoopUpFloat("SNARKS", "PrimaryHealth", gSkillData.snarkHealth);
	pev->gravity = 0.5;
	pev->friction = 0.5;

	pev->dmg = PREFS_KeyLoopUpFloat("SNARKS", "PrimaryPopDmg", gSkillData.snarkDmgPop);

	m_flDie = gpGlobals->time + PREFS_KeyLoopUpFloat("SNARKS", "PrimaryLife", 15.0);

	m_flFieldOfView = 0; // 180 degrees

	// without an owner the snark owns itself
	m_hOwner = Instance(pev->owner ? pev->owner : edict());

	m_flNextBounceSoundTime = gpGlobals->time; // reset each time a snark is spawned.

	pev->sequence = WSQUEAK_RUN;
	ResetSequenceInfo();
}

// orig 0x219c20
void CSqueakGrenade::Precache()
{
	PRECACHE_MODEL("models/w_squeak.mdl");
	PRECACHE_SOUND("squeek/sqk_blast1.wav");
	PRECACHE_SOUND("common/bodysplat.wav");
	PRECACHE_SOUND("squeek/sqk_die1.wav");
	PRECACHE_SOUND("squeek/sqk_hunt1.wav");
	PRECACHE_SOUND("squeek/sqk_hunt2.wav");
	PRECACHE_SOUND("squeek/sqk_hunt3.wav");
	PRECACHE_SOUND("squeek/sqk_deploy1.wav");
	PRECACHE_SOUND("debris/beamstart11.wav");
}


// orig 0x219ce0
void CSqueakGrenade::Killed(entvars_t* pevAttacker, int iGib)
{
	pev->model = iStringNull; // make invisible
	SetThink(&CSqueakGrenade::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.1;

	// since squeak grenades never leave a body behind, clear out their takedamage now.
	// Squeaks do a bit of radius damage when they pop, and that radius damage will
	// continue to call this function unless we acknowledge the Squeak's death now. (sjb)
	pev->takedamage = DAMAGE_NO;

	// play squeek blast
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "squeek/sqk_blast1.wav", 1, 0.5, 0, PITCH_NORM);

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, SMALL_EXPLOSION_VOLUME, 3.0);

	UTIL_BloodDrips(pev->origin, g_vecZero, BloodColor(), 80);

	if (m_hOwner != NULL)
		RadiusDamage(pev, m_hOwner->pev, pev->dmg, CLASS_NONE, DMG_BLAST);
	else
		RadiusDamage(pev, pev, pev->dmg, CLASS_NONE, DMG_BLAST);

	// reset owner so death message happens
	if (m_hOwner != NULL)
		pev->owner = m_hOwner->edict();

	CBaseMonster::Killed(pevAttacker, GIB_ALWAYS);
}

// orig 0x219e50
void CSqueakGrenade::GibMonster()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/bodysplat.wav", 0.75, ATTN_NORM, 0, 200);
}

// orig 0x219ea0: a laser shot straight ahead, 512 units, while it has an enemy.
void CSqueakGrenade::ShootLaser()
{
	if (m_hEnemy == NULL)
		return;
	if (CVAR_GET_FLOAT("sv_ceasefire") == 1)
		return;

	UTIL_MakeVectors(pev->angles);
	Vector vecDir = gpGlobals->v_forward;

	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + vecDir.Normalize() * 512, dont_ignore_monsters, ENT(pev), &tr);

	if (tr.pHit->v.takedamage == DAMAGE_NO)
	{
		if (CVAR_GET_FLOAT("sv_lowlag") != 1)
		{
			if (CBaseEntity::Instance(tr.pHit)->IsBSPModel())
				UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));
			UTIL_Sparks(tr.vecEndPos);
		}
	}
	else
	{
		ClearMultiDamage();
		CBaseEntity::Instance(tr.pHit)->TraceAttack(pev, PREFS_KeyLoopUpFloat("SNARKS", "PrimaryLaserDamage", 12.0), vecDir.Normalize(), &tr, DMG_ENERGYBEAM);
		if (m_hOwner != NULL)
			ApplyMultiDamage(pev, m_hOwner->pev);
		else
			ApplyMultiDamage(pev, pev);
	}

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(tr.vecEndPos.x);
	WRITE_COORD(tr.vecEndPos.y);
	WRITE_COORD(tr.vecEndPos.z);
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(50);	 // framerate
	WRITE_BYTE(1);	 // life
	WRITE_BYTE(10);	 // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(255); // r, g, b
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(255); // brightness
	WRITE_BYTE(50);	 // speed
	MESSAGE_END();

	EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/beamstart11.wav", 0.5, ATTN_NORM);
}

// orig 0x21a3c0
void CSqueakGrenade::HuntThink()
{
	// ALERT( at_console, "think\n" );

	if (!IsInWorld())
	{
		SetTouch(NULL);
		UTIL_Remove(this);
		return;
	}

	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	// explode when ready
	if (gpGlobals->time >= m_flDie)
	{
		g_vecAttackDir = pev->velocity.Normalize();
		pev->health = -1;
		Killed(pev, 0);
		return;
	}

	// float
	if (pev->waterlevel != 0)
	{
		if (pev->movetype == MOVETYPE_BOUNCE)
		{
			pev->movetype = MOVETYPE_FLY;
		}
		pev->velocity = pev->velocity * 0.9;
		pev->velocity.z += 8.0;
	}
	else
	{
		// the 2001 SDK's "else if (pev->movetype = MOVETYPE_FLY)": always back to bounce
		pev->movetype = MOVETYPE_FLY;
		pev->movetype = MOVETYPE_BOUNCE;
	}

	// shoot the laser where it's going
	if (m_flNextLaser < gpGlobals->time)
	{
		pev->angles = UTIL_VecToAngles(pev->velocity);
		pev->angles.z = 0;
		pev->angles.x = 0;
		ShootLaser();
		float flMax = PREFS_KeyLoopUpFloat("SNARKS", "PrimaryLaserMaxDelay", 0.8);
		float flMin = PREFS_KeyLoopUpFloat("SNARKS", "PrimaryLaserMinDelay", 0.75);
		m_flNextLaser = gpGlobals->time + RANDOM_FLOAT(flMin, flMax);
	}

	// return if not time to hunt
	if (m_flNextHunt > gpGlobals->time)
		return;

	m_flNextHunt = gpGlobals->time + 2.0;

	Vector vecDir;

	Vector vecFlat = pev->velocity;
	vecFlat.z = 0;
	vecFlat = vecFlat.Normalize();

	UTIL_MakeVectors(pev->angles);

	if (m_hEnemy == NULL || FNullEnt(m_hEnemy->edict()) || !m_hEnemy->IsAlive())
	{
		// find target, bounce a bit towards it.
		Look(512);
		m_hEnemy = BestVisibleEnemy();
	}

	// squeek if it's about time blow up
	if ((m_flDie - gpGlobals->time <= 0.5) && (m_flDie - gpGlobals->time >= 0.3))
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_die1.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG(0, 0x3F));
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 256, 0.25);
	}

	// higher pitch as squeeker gets closer to detonation time (unused here, as in the SDK)
	float flpitch = 155.0 - 60.0 * ((m_flDie - gpGlobals->time) / PREFS_KeyLoopUpFloat("SNARKS", "PrimaryLife", 15.0));
	(void)flpitch;

	if (m_hEnemy != NULL)
	{
		if (FVisible(m_hEnemy))
		{
			vecDir = m_hEnemy->EyePosition() - pev->origin;
			m_vecTarget = vecDir.Normalize();
		}

		float flVel = pev->velocity.Length();
		float flAdj = 50.0 / (flVel + 10.0);

		if (flAdj > 1.2)
			flAdj = 1.2;

		pev->velocity = pev->velocity * flAdj + m_vecTarget * PREFS_KeyLoopUpFloat("SNARKS", "PrimaryJumpSpeed", 300.0);
	}

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		pev->avelocity = Vector(0, 0, 0);
	}
	else
	{
		if (pev->avelocity == Vector(0, 0, 0))
		{
			pev->avelocity.x = RANDOM_FLOAT(-100, 100);
			pev->avelocity.z = RANDOM_FLOAT(-100, 100);
		}
	}

	if ((pev->origin - m_posPrev).Length() < 1.0)
	{
		pev->velocity.x = RANDOM_FLOAT(-100, 100);
		pev->velocity.y = RANDOM_FLOAT(-100, 100);
	}
	m_posPrev = pev->origin;

	pev->angles = UTIL_VecToAngles(pev->velocity);
	pev->angles.z = 0;
	pev->angles.x = 0;
}

// orig 0x21ac80
void CSqueakGrenade::SuperBounceTouch(CBaseEntity* pOther)
{
	float flpitch;

	TraceResult tr = UTIL_GetGlobalTrace();

	// don't hit the guy that launched this grenade
	if (pev->owner && pOther->edict() == pev->owner)
		return;

	// at least until we've bounced once
	pev->owner = NULL;

	pev->angles.x = 0;
	pev->angles.z = 0;

	// avoid bouncing too much
	if (m_flNextHit > gpGlobals->time)
		return;

	// higher pitch as squeeker gets closer to detonation time
	flpitch = 155.0 - 60.0 * ((m_flDie - gpGlobals->time) / PREFS_KeyLoopUpFloat("SNARKS", "PrimaryLife", 15.0));

	if (0 != pOther->pev->takedamage && m_flNextAttack < gpGlobals->time)
	{
		// attack! unless there's a ceasefire
		// make sure it's me who has touched them
		// and it's not another squeakgrenade
		if (CVAR_GET_FLOAT("sv_ceasefire") != 1 && tr.pHit == pOther->edict() && tr.pHit->v.modelindex != pev->modelindex)
		{
			// ALERT( at_console, "hit enemy\n");
			ClearMultiDamage();
			pOther->TraceAttack(pev, PREFS_KeyLoopUpFloat("SNARKS", "PrimaryDmgBite", gSkillData.snarkDmgBite), gpGlobals->v_forward, &tr, DMG_SLASH);
			if (m_hOwner != NULL)
				ApplyMultiDamage(pev, m_hOwner->pev);
			else
				ApplyMultiDamage(pev, pev);

			pev->dmg += PREFS_KeyLoopUpFloat("SNARKS", "PrimaryBlastAddPerBite", gSkillData.snarkDmgPop); // add more explosion damage
			// m_flDie += 2.0; // add more life

			// make bite sound
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "squeek/sqk_deploy1.wav", 1.0, ATTN_NORM, 0, (int)flpitch);
			m_flNextAttack = gpGlobals->time + 0.5;
		}
	}

	m_flNextHit = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time;

	if (g_pGameRules->IsMultiplayer())
	{
		// in multiplayer, we limit how often snarks can make their bounce sounds to prevent overflows.
		if (gpGlobals->time < m_flNextBounceSoundTime)
		{
			// too soon!
			return;
		}
	}

	if ((pev->flags & FL_ONGROUND) == 0)
	{
		// play bounce sound
		float flRndSound = RANDOM_FLOAT(0, 1);

		if (flRndSound <= 0.33)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt1.wav", 1, ATTN_NORM, 0, (int)flpitch);
		else if (flRndSound <= 0.66)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, (int)flpitch);
		else
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, (int)flpitch);
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 256, 0.25);
	}
	else
	{
		// skittering sound
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, 100, 0.1);
	}

	m_flNextBounceSoundTime = gpGlobals->time + 0.5; // half second.
}

// orig 0x21c030
void CSqueakGrenade::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_hOwner != NULL && m_hOwner->edict() == pEdict)
		m_hOwner = NULL;
	if (m_hEnemy != NULL && m_hEnemy->edict() == pEdict)
		m_hEnemy = NULL;
}

// orig 0x21c0c0
bool CSqueakGrenade::BotTargetRealseable(CBaseEntity* pBot)
{
	return !IsEntEnemy(pBot);
}

// orig 0x21c0f0
int CSqueakGrenade::GetBotTargetWeight(CBaseEntity* pBot)
{
	return (IsEntEnemy(pBot) ? 1 : 0) * 6 + 3;
}

// orig 0x21c130
bool CSqueakGrenade::IsEntEnemy(CBaseEntity* pEntity)
{
	return pEntity && m_hEnemy != NULL && pEntity->pev == m_hEnemy->pev;
}

// orig 0x21c190
Vector CSqueakGrenade::GetBotShootOrg()
{
	return pev->origin - Vector(0, 0, 16);
}

// orig 0x21c1f0
int CSqueakGrenade::GetRCCaps()
{
	return 0x7fe3d5f;
}

// orig 0x21c200
int CSqueakGrenade::GetGuassDistOut()
{
	return 16;
}

// orig 0x21c210
int CSqueakGrenade::GetGuassHull()
{
	return head_hull;
}

// orig 0x21c220
int CSqueakGrenade::BloodColor()
{
	return BLOOD_COLOR_YELLOW;
}

//=========================================================
// CSqueak
//=========================================================
LINK_ENTITY_TO_CLASS(weapon_snark, CSqueak);

// orig 0x21b090
bool CSqueak::IsHornetGun()
{
	return true;
}

// orig 0x21b0a0
void CSqueak::Spawn()
{
	Precache();
	m_iId = WEAPON_SNARK;
	SET_MODEL(ENT(pev), "models/w_sqknest.mdl");

	FallInit(); //get ready to fall down.

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("SNARKS", "PickupAmmo", 6, 0, 250);

	pev->sequence = 1;
	pev->animtime = gpGlobals->time;
	pev->framerate = 1.0;
}

// orig 0x21b140
void CSqueak::Precache()
{
	PRECACHE_MODEL("models/w_sqknest.mdl");
	PRECACHE_MODEL("models/v_squeak.mdl");
	PRECACHE_MODEL("models/p_squeak.mdl");
	PRECACHE_SOUND("squeek/sqk_hunt2.wav");
	PRECACHE_SOUND("squeek/sqk_hunt3.wav");
	UTIL_PrecacheOther("monster_snark");
	UTIL_PrecacheOther("snark_cam");
}

// orig 0x21b1d0
bool CSqueak::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Snarks";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("SNARKS", "MaxAmmo", 18, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_SNARK;
	p->iWeight = 7;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return true;
}

// orig 0x21b280
bool CSqueak::Deploy()
{
	// play hunt sound
	float flRndSound = RANDOM_FLOAT(0, 1);

	if (flRndSound <= 0.5)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, 100);
	else
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 100);

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

	// halflife-updated's fix, kept: the first idle waits 1.7 s, so it doesn't cut the deploy
	// animation short (halflife #2495); the original keeps DefaultDeploy's 1 s
	const bool result = DefaultDeploy("models/v_squeak.mdl", "models/p_squeak.mdl", SQUEAK_UP, "squeak");

	if (result)
	{
		m_flTimeWeaponIdle = gpGlobals->time + 1.7;
	}

	return result;
}

// orig 0x21b330: also takes the player out of a cam.
void CSqueak::Holster()
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		m_pPlayer->ClearWeaponBit(m_iId);
		SetThink(&CSqueak::DestroyItem);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	if (!m_bInCam)
	{
		SendWeaponAnim(SQUEAK_DOWN);
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
		return;
	}

	m_bInCam = false;
	if (m_pCam)
		m_pCam->m_bOwnerInCam = false;
	if (m_pPlayer)
	{
		if ((m_pPlayer->IsAlive() && m_pPlayer->pev->deadflag == DEAD_NO) || m_pPlayer->IsActingAsCam())
			SET_VIEW(m_pPlayer->edict(), m_pPlayer->edict());
	}
}

// orig 0x21b4a0: throws a snark, at most 20 in the world; in a cam, fires its laser.
void CSqueak::PrimaryAttack()
{
	if (m_bInCam)
	{
		if (m_pCam)
			m_pCam->m_bInFire = true;
		return;
	}

	if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		return;
	if (CountEntsInWorld("monster_snark") >= 21)
		return;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	TraceResult tr;

	// find place to toss monster
	UTIL_TraceLine(m_pPlayer->pev->origin + gpGlobals->v_forward * 16, m_pPlayer->pev->origin + gpGlobals->v_forward * 64, dont_ignore_monsters, NULL, &tr);

	if (tr.fAllSolid == 0 && tr.fStartSolid == 0 && tr.flFraction > 0.25)
	{
		SendWeaponAnim(SQUEAK_THROW);

		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		CBaseEntity* pSqueak = CBaseEntity::Create("monster_snark", tr.vecEndPos, m_pPlayer->pev->v_angle, m_pPlayer->edict());
		pSqueak->pev->velocity = gpGlobals->v_forward * 200 + m_pPlayer->pev->velocity;

		// play hunt sound
		float flRndSound = RANDOM_FLOAT(0, 1);

		if (flRndSound <= 0.5)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, 105);
		else
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 105);

		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		m_fJustThrown = true;

		m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("SNARKS", "PrimaryRefire", 0.3);
		m_flNextSecondaryAttack = gpGlobals->time + 0.4;
		m_flTimeWeaponIdle = gpGlobals->time + 1.0;
	}
}

// The player's next valid snark cam after pStart, or NULL.
static CSqueakCam* NextPlayersCam(CBaseEntity* pStart, CBasePlayer* pPlayer)
{
	CBaseEntity* pEnt = pStart;
	while ((pEnt = UTIL_FindEntityByClassname(pEnt, "snark_cam")) != NULL)
	{
		if (!pEnt->pev || !pEnt->pev->pContainingEntity || !UTIL_IsValidEntity(pEnt->pev->pContainingEntity))
			continue;
		CSqueakCam* pCam = (CSqueakCam*)pEnt;
		if (pCam->m_hOwner != NULL && pCam->m_hOwner->pev == pPlayer->pev)
			return pCam;
	}
	return NULL;
}

// Puts the player's view in pCam.
static void EnterCam(CSqueak* pSqueak, CSqueakCam* pCam)
{
	SET_VIEW(pSqueak->m_pPlayer->edict(), pCam->m_pViewEnt->edict());
	SnarkSetupVisibility(pCam->m_pViewEnt->edict(), pSqueak->m_pPlayer->edict());
	pSqueak->m_bInCam = true;
	pSqueak->m_pCam = pCam;
	pCam->m_bOwnerInCam = true;
	pCam->m_vecLastOwnAng = pSqueak->m_pPlayer->pev->v_angle;
}

// orig 0x21b7e0: sticks a cam (at most 30 in the world) to the wall in front; in a cam,
// switches to the player's next one.
void CSqueak::SecondaryAttack()
{
	if (!m_bInCam)
	{
		if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
			return;
		if (CountEntsInWorld("snark_cam") > 30)
			return;

		UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
		Vector vecEnd = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 64;
		Vector vecStart = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 16;
		TraceResult tr;
		UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr);

		if (tr.flFraction < 1.0 && tr.pHit)
		{
			SendWeaponAnim(SQUEAK_THROW);
			m_pPlayer->SetAnimation(PLAYER_ATTACK1);
			CSqueakCam::CreateCam(&tr, m_pPlayer);

			if (RANDOM_FLOAT(0, 1) > 0.5)
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 105);
			else
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, 105);

			m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
			m_fJustThrown = true;

			m_flNextPrimaryAttack = gpGlobals->time + 0.3;
			m_flNextSecondaryAttack = gpGlobals->time + 0.4;
			m_flTimeWeaponIdle = gpGlobals->time + 1.0;
		}
		return;
	}

	CSqueakCam* pCam = NextPlayersCam(m_pCam, m_pPlayer);
	if (!pCam)
		pCam = NextPlayersCam(NULL, m_pPlayer);
	if (!pCam)
		return;

	if (m_pCam)
		m_pCam->m_bOwnerInCam = false;
	EnterCam(this, pCam);

	m_flNextPrimaryAttack = gpGlobals->time + 0.4;
	m_flNextSecondaryAttack = gpGlobals->time + 0.4;
}

// orig 0x21bd00: into the player's first cam, or back out.
void CSqueak::Reload()
{
	if (m_flNextReload > gpGlobals->time)
		return;
	m_flNextReload = gpGlobals->time + 0.5;

	if (!m_bInCam)
	{
		if (CVAR_GET_FLOAT("sv_ceasefire") == 1)
			return;

		CBaseEntity* pEnt = NULL;
		while ((pEnt = UTIL_FindEntityByClassname(pEnt, "snark_cam")) != NULL)
		{
			CSqueakCam* pCam = (CSqueakCam*)pEnt;
			if (pCam->m_hOwner != NULL && pCam->m_hOwner->pev == m_pPlayer->pev)
			{
				EnterCam(this, pCam);
				return;
			}
		}
		return;
	}

	if (m_pCam)
		m_pCam->m_bOwnerInCam = false;
	SET_VIEW(m_pPlayer->edict(), m_pPlayer->edict());
	SnarkSetupVisibility(m_pPlayer->edict(), m_pPlayer->edict());
	m_bInCam = false;
}

// orig 0x21bee0
void CSqueak::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_fJustThrown)
	{
		m_fJustThrown = false;

		if (0 == m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()])
		{
			RetireWeapon();
			return;
		}

		SendWeaponAnim(SQUEAK_UP);
		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);
		return;
	}

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.75)
	{
		iAnim = SQUEAK_IDLE1;
		m_flTimeWeaponIdle = gpGlobals->time + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = SQUEAK_FIDGETFIT;
		m_flTimeWeaponIdle = gpGlobals->time + 70.0 / 16.0;
	}
	else
	{
		iAnim = SQUEAK_FIDGETNIP;
		m_flTimeWeaponIdle = gpGlobals->time + 80.0 / 16.0;
	}
	SendWeaponAnim(iAnim);
}

// orig 0x21c380: the bot never reloads it (reload is the cam).
bool CSqueak::BotReloadWeapon()
{
	return false;
}
