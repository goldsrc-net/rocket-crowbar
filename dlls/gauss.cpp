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
*	Rocket Crowbar 1.9 replaced the gauss gun with a teleporter. Secondary fire throws a beacon
*	(CTeleBecon, "gauss_tele") that sticks to what it hits. With a beacon out, secondary
*	teleports the player to it, primary fires a beam that teleports whatever it hits to it, and
*	reload brings the beacon back. Without one, reload teleports the player to a random spawn
*	point. Every cost and refire time comes from rc.ini's [GAUSS] heading.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "shake.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "Preff.h"

extern edict_t* EntSelectSpawnPoint(CBaseEntity* pPlayer); // player.cpp

// sprites CGauss::Precache loads; globals in the original
int m_iEndTeleBall;	   // sprites/redflare1.spr, the trail where a teleport lands
int m_iStartTeleFlash; // sprites/xflare1.spr, the flash where it starts
int m_iImpactSpr;	   // sprites/xspark2.spr, where a beam that teleports nothing hits

// HasRCCap bits this file tests. VERIFY names: the original's are not in the binary.
#define RC_CAP_GAUSS_TELEPORTABLE 0x1 // the gauss beam may teleport it to a beacon
#define RC_CAP_TELEFRAGGABLE 0x2	  // a teleport landing on it kills it instead of failing

//=========================================================
// CTeleBecon: the gauss beacon ("gauss_tele" once thrown)
//=========================================================
class CTeleBecon : public CBaseMonster
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	static CTeleBecon* BecCreate();
	void Spawn() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	bool IsAlive() override;
	Vector GetBotShootOrg() override;
	int GetBotTargetWeight(CBaseEntity* pBot) override;
	bool BotTargetRealseable(CBaseEntity* pBot) override;
	void EntIsBeingRemoved(edict_t* pEdict) override;
	int GetRCCaps() override;

	void EXPORT ActiveThink();
	void EXPORT BeconAttach(CBaseEntity* pOther);
	bool BeconTeleport(CGauss* pGauss, entvars_t* pevTarget);

	bool m_blAttached;		 // orig +0x27c, stuck to something
	Vector m_vecPlaneNormal; // orig +0x280, normal of the surface it stuck to
	EHANDLE m_hOwner;		 // orig +0x28c, the entity it stuck to (pev->owner is the thrower)
	Vector m_posOwner;		 // orig +0x294, m_hOwner's origin when it stuck
	Vector m_angleOwner;	 // orig +0x2a0, m_hOwner's angles when it stuck
};

LINK_ENTITY_TO_CLASS(telebecon, CTeleBecon);

TYPEDESCRIPTION CTeleBecon::m_SaveData[] =
	{
		DEFINE_FIELD(CTeleBecon, m_blAttached, FIELD_BOOLEAN),
		DEFINE_FIELD(CTeleBecon, m_vecPlaneNormal, FIELD_VECTOR),
		DEFINE_FIELD(CTeleBecon, m_hOwner, FIELD_EHANDLE),
		DEFINE_FIELD(CTeleBecon, m_posOwner, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CTeleBecon, m_angleOwner, FIELD_VECTOR),
};

IMPLEMENT_SAVERESTORE(CTeleBecon, CBaseMonster); // orig 0x197ea0, 0x197ef0

// Removes every tele_flare whose owner is pOwner. The beacon code repeats this loop inline.
static void RemoveTeleFlares(edict_t* pOwner)
{
	edict_t* pFind = FIND_ENTITY_BY_CLASSNAME(NULL, "tele_flare");
	while (!FNullEnt(pFind))
	{
		CBaseEntity* pFlare = CBaseEntity::Instance(pFind);
		if (pFlare && pFlare->pev->owner == pOwner)
			pFlare->SUB_Remove();
		pFind = FIND_ENTITY_BY_CLASSNAME(pFind, "tele_flare");
	}
}

// The player's beacon, or NULL.
static CTeleBecon* FindPlayersBecon(CBasePlayer* pPlayer)
{
	edict_t* pFind = NULL;
	while (true)
	{
		pFind = FIND_ENTITY_BY_CLASSNAME(pFind, "gauss_tele");
		if (FNullEnt(pFind))
			return NULL;
		CTeleBecon* pBecon = (CTeleBecon*)CBaseEntity::Instance(pFind);
		if (pBecon && pBecon->pev->owner == pPlayer->edict())
			return pBecon;
	}
}

// orig 0x197f40
CTeleBecon* CTeleBecon::BecCreate()
{
	CTeleBecon* pBecon = GetClassPtr((CTeleBecon*)NULL);
	pBecon->pev->classname = MAKE_STRING("gauss_tele");
	pBecon->Spawn();
	return pBecon;
}

// orig 0x197f90
void CTeleBecon::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	if (PREFS_KeyLoopUpBool("GAUSS", "BeconKillable", true))
	{
		pev->takedamage = DAMAGE_YES;
		pev->health = 1;
	}
	else
	{
		pev->takedamage = DAMAGE_NO;
	}

	pev->gravity = PREFS_KeyLoopUpFloat("GAUSS", "BeconGravity", 0.6);
	m_bloodColor = DONT_BLEED;
	SET_MODEL(ENT(pev), "models/w_adrenaline.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-2, -2, 0), Vector(2, 2, 16));

	m_hOwner = NULL;
	SetTouch(&CTeleBecon::BeconAttach);
	m_blAttached = false;
}

// orig 0x1980e0
void CTeleBecon::Killed(entvars_t* pevAttacker, int iGib)
{
	pev->deadflag = DEAD_DEAD;
	pev->takedamage = DAMAGE_NO;

	RemoveTeleFlares(edict());

	Vector vecExplode;
	if (m_blAttached)
		vecExplode = pev->origin + m_vecPlaneNormal * 16;
	else
		vecExplode = pev->origin;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(vecExplode.x);
	WRITE_COORD(vecExplode.y);
	WRITE_COORD(vecExplode.z);
	if (UTIL_PointContents(pev->origin) == CONTENTS_WATER)
		WRITE_SHORT(g_sModelIndexWExplosion);
	else
		WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(12); // scale * 10
	WRITE_BYTE(18); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	SetThink(&CBaseEntity::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x198390: while stuck. Dies when what it stuck to moves or turns, pings now and then,
// and goes away (without exploding) when its thrower is gone.
void CTeleBecon::ActiveThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_hOwner != NULL && m_posOwner == m_hOwner->pev->origin && m_angleOwner == m_hOwner->pev->angles)
	{
		if (pev->dmgtime < gpGlobals->time)
		{
			EMIT_SOUND(ENT(pev), CHAN_WEAPON, "turret/tu_ping.wav", 0.4, ATTN_NORM);
			pev->dmgtime = gpGlobals->time + RANDOM_FLOAT(1.0, 1.5);
		}

		if (pev->owner && pev->owner->pvPrivateData && 0 == pev->owner->free)
			return;

		RemoveTeleFlares(edict());
		SetThink(&CBaseEntity::SUB_Remove);
		return;
	}

	Killed(pev, 0);
}

// orig 0x198650: touch until stuck. Sticks to the surface it hit, or dies in the sky.
void CTeleBecon::BeconAttach(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;

	TraceResult trHit = UTIL_GetGlobalTrace();
	Vector vecStart = trHit.vecEndPos + trHit.vecPlaneNormal * 4;
	Vector vecEnd = trHit.vecEndPos - trHit.vecPlaneNormal * 16;

	TraceResult tr;
	UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, edict(), &tr);
	if (!tr.pHit)
		return;

	if (UTIL_PointContents(tr.vecEndPos) == CONTENTS_SKY)
	{
		Killed(pev, 0);
		return;
	}

	m_hOwner = CBaseEntity::Instance(tr.pHit);
	if (m_hOwner == NULL)
		return;

	m_posOwner = m_hOwner->pev->origin;
	m_angleOwner = m_hOwner->pev->angles;

	pev->angles = UTIL_VecToAngles(tr.vecPlaneNormal) - Vector(90, 0, 0);
	UTIL_SetOrigin(pev, tr.vecEndPos);
	pev->movetype = MOVETYPE_NONE;
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	m_blAttached = true;
	m_vecPlaneNormal = tr.vecPlaneNormal;
	SetTouch(NULL);
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/mine_deploy.wav", 0.75, ATTN_NORM);

	// the glow that marks it
	CBaseEntity* pFlare = GetClassPtr((CBaseEntity*)NULL);
	pFlare->Spawn();
	pFlare->pev->classname = MAKE_STRING("tele_flare");
	pFlare->pev->movetype = MOVETYPE_NONE;
	pFlare->pev->solid = SOLID_NOT;
	pFlare->pev->renderamt = 164;
	pFlare->pev->rendermode = kRenderGlow;
	pFlare->pev->renderfx = kRenderFxPulseFast;
	pFlare->pev->scale = 0.2;
	pFlare->pev->owner = edict();
	SET_MODEL(pFlare->edict(), "sprites/redflare1.spr");
	UTIL_SetOrigin(pFlare->pev, tr.vecEndPos + tr.vecPlaneNormal * 16);

	SetThink(&CTeleBecon::ActiveThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = gpGlobals->time + 1.0;
}

// orig 0x198ae0: room for a hull and a box of vecMins/vecMaxs at vecOrigin, monsters ignored.
bool TeleIsValidSpot(Vector vecOrigin, Vector vecMins, Vector vecMaxs, int iHull, entvars_t* pevIgnore)
{
	TraceResult tr;

	UTIL_TraceLine(vecOrigin + vecMaxs, vecOrigin + vecMins, ignore_monsters, pevIgnore->pContainingEntity, &tr);
	if (tr.flFraction != 1.0 || 0 != tr.fStartSolid || 0 != tr.fAllSolid)
		return false;

	UTIL_TraceHull(vecOrigin, vecOrigin, ignore_monsters, iHull, pevIgnore->pContainingEntity, &tr);
	if (tr.flFraction != 1.0 || 0 != tr.fStartSolid || 0 != tr.fAllSolid)
		return false;

	UTIL_TraceLine(Vector(vecMins.x + vecOrigin.x, vecMins.y + vecOrigin.y, vecMaxs.z + vecOrigin.z),
		Vector(vecMaxs.x + vecOrigin.x, vecMaxs.y + vecOrigin.y, vecMins.z + vecOrigin.z),
		ignore_monsters, pevIgnore->pContainingEntity, &tr);
	if (tr.flFraction != 1.0 || 0 != tr.fStartSolid || 0 != tr.fAllSolid)
		return false;

	return true;
}

// The check TeleIsValidSpotCheckEnts makes after each trace: false when the trace hit something
// alive that can't be telefragged.
static bool TeleTraceHitBlocks(TraceResult& tr)
{
	if (!tr.pHit)
		return false;
	int iCap = RC_CAP_TELEFRAGGABLE;
	CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
	return !pHit->HasRCCap(iCap) && pHit->IsAlive();
}

// orig 0x198cd0: the same three traces with monsters, failing on anything alive that can't be
// telefragged. *ppHit is the first entity hit (the world counts).
bool TeleIsValidSpotCheckEnts(Vector vecOrigin, Vector vecMins, Vector vecMaxs, int iHull, entvars_t* pevIgnore, CBaseEntity** ppHit)
{
	TraceResult tr;
	*ppHit = NULL;

	UTIL_TraceLine(vecOrigin + vecMaxs, vecOrigin + vecMins, dont_ignore_monsters, pevIgnore->pContainingEntity, &tr);
	if (TeleTraceHitBlocks(tr))
		return false;
	if (tr.pHit && !*ppHit)
		*ppHit = CBaseEntity::Instance(tr.pHit);

	UTIL_TraceHull(vecOrigin, vecOrigin, dont_ignore_monsters, iHull, pevIgnore->pContainingEntity, &tr);
	if (TeleTraceHitBlocks(tr))
		return false;
	if (tr.pHit && !*ppHit)
		*ppHit = CBaseEntity::Instance(tr.pHit);

	UTIL_TraceLine(Vector(vecMins.x + vecOrigin.x, vecMins.y + vecOrigin.y, vecMaxs.z + vecOrigin.z),
		Vector(vecMaxs.x + vecOrigin.x, vecMaxs.y + vecOrigin.y, vecMins.z + vecOrigin.z),
		dont_ignore_monsters, pevIgnore->pContainingEntity, &tr);
	if (TeleTraceHitBlocks(tr))
		return false;
	if (tr.pHit && !*ppHit)
		*ppHit = CBaseEntity::Instance(tr.pHit);

	return true;
}

// Kills whatever living thing (other than pevTarget and pevSkip) is in the box at vecSpot. When
// one survives, pevTarget is killed, and when that survives too, the gauss's player. Returns
// false when something in the box can't be telefragged. RandomTeleEnt and BeconTeleport repeat
// this loop inline; they differ in which takedamage counts and in the damage.
static bool TeleFragBox(CGauss* pGauss, entvars_t* pevTarget, entvars_t* pevSkip, const Vector& vecSpot,
	bool bAimOnly, float flDamage, entvars_t* pevLastInflictor)
{
	CBaseEntity* pList[8];
	int iCount = UTIL_EntitiesInBox(pList, 8, vecSpot + pevTarget->mins, vecSpot + pevTarget->maxs, 0);
	for (int i = 0; i < iCount; i++)
	{
		entvars_t* pevEnt = pList[i]->pev;
		if (pevEnt->pContainingEntity == pevTarget->pContainingEntity)
			continue;
		if (pevSkip && pevEnt->pContainingEntity == pevSkip->pContainingEntity)
			continue;
		if (bAimOnly ? pevEnt->takedamage != DAMAGE_AIM : pevEnt->takedamage == DAMAGE_NO)
			continue;
		if (pevEnt->deadflag != DEAD_NO)
			continue;

		int iCap = RC_CAP_TELEFRAGGABLE;
		if (!pList[i]->HasRCCap(iCap))
			return false;

		pList[i]->TakeDamage(pGauss->pev, pGauss->m_pPlayer->pev, flDamage, DMG_ALWAYSGIB);
		if (pList[i]->pev->takedamage != DAMAGE_NO && pList[i]->pev->deadflag == DEAD_NO)
		{
			CBaseEntity* pTarget = CBaseEntity::Instance(pevTarget->pContainingEntity);
			pTarget->TakeDamage(pGauss->pev, pGauss->m_pPlayer->pev, flDamage, DMG_ALWAYSGIB);
			if (pevTarget->takedamage != DAMAGE_NO && pevTarget->deadflag == DEAD_NO)
			{
				pTarget->TakeDamage(pevLastInflictor, pevLastInflictor, flDamage, DMG_ALWAYSGIB);
				pGauss->m_pPlayer->TakeDamage(pGauss->pev, pGauss->m_pPlayer->pev, flDamage, DMG_ALWAYSGIB);
			}
		}
	}
	return true;
}

// orig 0x1990d0: moves pevTarget out from the beacon along the surface normal. Telefrags what's
// there, removes the beacon. False when there's no room.
bool CTeleBecon::BeconTeleport(CGauss* pGauss, entvars_t* pevTarget)
{
	float flDistOut;
	int iHull;
	Vector vecOfs;

	CBaseEntity* pTarget = CBaseEntity::Instance(pevTarget->pContainingEntity);
	if (!pTarget)
	{
		flDistOut = 56;
		iHull = head_hull;
		vecOfs = g_vecZero;
	}
	else
	{
		flDistOut = pTarget->GetGuassDistOut();
		iHull = pTarget->GetGuassHull();
		vecOfs = Vector(0, 0, (pTarget->pev->mins.z + pTarget->pev->maxs.z) * 0.5);
	}

	if (!m_blAttached)
		return false;

	if (pev->angles.x > 360)
		pev->angles.x -= 360;
	if (pev->angles.y > 360)
		pev->angles.y -= 360;
	if (pev->angles.z > 360)
		pev->angles.z -= 360;
	if (pev->angles.x < 0)
		pev->angles.x += 360;
	if (pev->angles.y < 0)
		pev->angles.y += 360;
	if (pev->angles.z < 0)
		pev->angles.z += 360;

	Vector vecDest = pev->origin + m_vecPlaneNormal * flDistOut;
	if (m_vecPlaneNormal.z == 0)
	{
		vecDest = vecDest - vecOfs;
	}
	else
	{
		float flSign = fabs(m_vecPlaneNormal.z) / m_vecPlaneNormal.z;
		vecDest = vecDest + vecOfs * flSign;
	}

	TraceResult tr;
	UTIL_TraceLine(pev->origin + m_vecPlaneNormal, vecDest, ignore_monsters, edict(), &tr);
	Vector vecSpot = tr.vecEndPos;
	if (tr.flFraction != 1.0 || 0 != tr.fStartSolid || 0 != tr.fAllSolid)
		return false;

	if (!TeleIsValidSpot(vecSpot, pevTarget->mins, pevTarget->maxs, iHull, pev))
		return false;

	CBaseEntity* pHit;
	if (!TeleIsValidSpotCheckEnts(vecSpot, pevTarget->mins, pevTarget->maxs, iHull, pev, &pHit))
		return false;

	if (pHit && pHit->edict() != pevTarget->pContainingEntity)
		pHit->TakeDamage(pGauss->pev, pGauss->m_pPlayer->pev, 50000, DMG_ALWAYSGIB);

	if (!TeleFragBox(pGauss, pevTarget, pev, vecSpot, false, 50000, pev))
		return false;

	pev->solid = SOLID_NOT;
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(pev->origin.x + m_vecPlaneNormal.x);
	WRITE_COORD(pev->origin.y + m_vecPlaneNormal.y);
	WRITE_COORD(pev->origin.z + m_vecPlaneNormal.z);
	WRITE_SHORT(m_iEndTeleBall);
	WRITE_BYTE(5);	// count
	WRITE_BYTE(3);	// life * 10
	WRITE_BYTE(3);	// scale * 10
	WRITE_BYTE(10); // velocity along the vector * 10
	WRITE_BYTE(30); // randomness * 10
	MESSAGE_END();
	pev->solid = SOLID_BBOX;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pevTarget->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pevTarget->origin.x);
	WRITE_COORD(pevTarget->origin.y);
	WRITE_COORD(pevTarget->origin.z);
	WRITE_SHORT(m_iStartTeleFlash);
	WRITE_BYTE(40); // scale * 10
	WRITE_BYTE(20); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();

	RemoveTeleFlares(edict());

	UTIL_SetOrigin(pevTarget, vecSpot);
	pevTarget->velocity = g_vecZero;
	pevTarget->fixangle = 2; // VERIFY: 2, not the 1 that sets angles; the original writes 2
	pevTarget->flags &= ~FL_ONGROUND;

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
	return true;
}

// orig 0x19c260
void CTeleBecon::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_hOwner != NULL && m_hOwner->edict() == pEdict)
		m_hOwner = NULL;

	if (pev->owner && pev->owner == pEdict)
		pev->owner = NULL;

	if (m_blAttached && (m_hOwner == NULL || !pev->owner))
		Killed(pev, 0);
}

// orig 0x19c310
Vector CTeleBecon::GetBotShootOrg()
{
	return pev->origin - Vector(0, 0, 16);
}

// orig 0x19c370
int CTeleBecon::GetRCCaps()
{
	return 0x802; // VERIFY meaning: telefraggable (0x2) and 0x800
}

// orig 0x19c380
bool CTeleBecon::BotTargetRealseable(CBaseEntity* pBot)
{
	return true;
}

// orig 0x19c390
int CTeleBecon::GetBotTargetWeight(CBaseEntity* pBot)
{
	return 1;
}

// orig 0x19c3a0
bool CTeleBecon::IsAlive()
{
	return true;
}

//=========================================================
// CGauss
//=========================================================
LINK_ENTITY_TO_CLASS(weapon_gauss, CGauss);

// orig 0x199ca0
bool CGauss::IsHornetGun()
{
	return true;
}

// The "retrieve the beacon" effect and cost shared by Reload and SecondaryAttack.
static void RetrieveBecon(CGauss* pGauss, CTeleBecon* pBecon)
{
	EMIT_SOUND(ENT(pGauss->m_pPlayer->pev), CHAN_WEAPON, "weapons/electro4.wav", 0.75, ATTN_NORM);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pGauss->pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pBecon->pev->origin.x);
	WRITE_COORD(pBecon->pev->origin.y);
	WRITE_COORD(pBecon->pev->origin.z);
	WRITE_SHORT(m_iStartTeleFlash);
	WRITE_BYTE(10); // scale * 10
	WRITE_BYTE(25); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();

	RemoveTeleFlares(pBecon->edict());
	pBecon->SUB_Remove();

	pGauss->m_pPlayer->m_rgAmmo[pGauss->m_iPrimaryAmmoType] -= PREFS_KeyLoopUpInt("GAUSS", "RetriveBeconCaust", 5);
}

// orig 0x199cb0: with a beacon out, retrieve it; without one, teleport to a random spawn point.
void CGauss::Reload()
{
	if (m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time)
		return;

	float flNext;
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
		flNext = gpGlobals->time + 0.15;
	}
	else
	{
		CTeleBecon* pBecon = FindPlayersBecon(m_pPlayer);
		if (!pBecon)
		{
			if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= PREFS_KeyLoopUpInt("GAUSS", "SelfTeleportCaust", 15) && RandomTeleEnt(m_pPlayer->pev))
			{
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "debris/beamstart7.wav", 1.0, ATTN_NORM);
				m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= PREFS_KeyLoopUpInt("GAUSS", "SelfTeleportCaust", 15);
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("GAUSS", "SelfTeleRefire", 0.8) * m_pPlayer->m_fCurseRefireScale;
				return;
			}
			PlayEmptySound();
			m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
			return;
		}

		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < PREFS_KeyLoopUpInt("GAUSS", "RetriveBeconCaust", 5))
		{
			PlayEmptySound();
			m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
			return;
		}

		RetrieveBecon(this, pBecon);
		flNext = gpGlobals->time + PREFS_KeyLoopUpFloat("GAUSS", "RetriveRefire", 0.8) * m_pPlayer->m_fCurseRefireScale;
	}

	m_flNextPrimaryAttack = flNext;
	m_flNextSecondaryAttack = flNext;
}

// orig 0x19a190
void CGauss::Spawn()
{
	Precache();
	m_iId = WEAPON_GAUSS;
	SET_MODEL(ENT(pev), "models/w_gauss.mdl");

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("GAUSS", "PickupAmmo", 40, 0, 250);

	FallInit(); // get ready to fall down.
}

// orig 0x19a210
void CGauss::Precache()
{
	PRECACHE_MODEL("models/w_gauss.mdl");
	PRECACHE_MODEL("models/v_gauss.mdl");
	PRECACHE_MODEL("models/p_gauss.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("weapons/gauss2.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("debris/beamstart7.wav");
	PRECACHE_SOUND("debris/beamstart14.wav");
	PRECACHE_SOUND("weapons/mine_deploy.wav");
	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("debris/beamstart4.wav");
	PRECACHE_SOUND("turret/tu_ping.wav");
	PRECACHE_MODEL("models/w_adrenaline.mdl");

	m_iEndTeleBall = PRECACHE_MODEL("sprites/redflare1.spr");
	m_iStartTeleFlash = PRECACHE_MODEL("sprites/xflare1.spr");
	m_iImpactSpr = PRECACHE_MODEL("sprites/xspark2.spr");
}

// orig 0x19a370. The 2001 SDK's weapons send WeapPickup from AddToPlayer; halflife-updated sends
// it from CBasePlayer::AddPlayerItem, so this only calls the base (reference/dossiers).
void CGauss::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	// As the original: this weapon tells the player it was picked up (AddPlayerItem doesn't)
	MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
	WRITE_BYTE(m_iId);
	MESSAGE_END();
}

// orig 0x19a400
bool CGauss::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("ITEMS", "CellsMax", 200, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_GAUSS;
	p->iFlags = 0;
	p->iWeight = -1;

	return true;
}

// orig 0x19a4b0
bool CGauss::Deploy()
{
	return DefaultDeploy("models/v_gauss.mdl", "models/p_gauss.mdl", GAUSS_DRAW, "gauss");
}

// orig 0x19a4f0
void CGauss::Holster()
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	SendWeaponAnim(GAUSS_HOLSTER);
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

// orig 0x19a570: teleports pevTarget to a spawn point (EntSelectSpawnPoint), telefragging
// what's there. False when there's no room.
bool CGauss::RandomTeleEnt(entvars_t* pevTarget)
{
	int iHull;
	CBaseEntity* pTarget = CBaseEntity::Instance(pevTarget->pContainingEntity);
	if (!pTarget)
	{
		iHull = head_hull;
	}
	else
	{
		pTarget->GetGuassDistOut(); // the original calls it and drops the result
		iHull = pTarget->GetGuassHull();
	}

	EMIT_SOUND(pevTarget->pContainingEntity, CHAN_WEAPON, "debris/beamstart7.wav", 1.0, ATTN_NORM);

	edict_t* pSpot = EntSelectSpawnPoint(pTarget);
	Vector vecSpot = VARS(pSpot)->origin;

	if (!TeleIsValidSpot(vecSpot, pevTarget->mins, pevTarget->maxs, iHull, m_pPlayer->pev))
		return false;

	if (!TeleFragBox(this, pevTarget, NULL, vecSpot, true, 500, pev))
		return false;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pevTarget->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pevTarget->origin.x);
	WRITE_COORD(pevTarget->origin.y);
	WRITE_COORD(pevTarget->origin.z);
	WRITE_SHORT(m_iStartTeleFlash);
	WRITE_BYTE(40); // scale * 10
	WRITE_BYTE(20); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();

	pevTarget->origin = vecSpot + Vector(0, 0, 1);
	pevTarget->v_angle = g_vecZero;
	pevTarget->velocity = g_vecZero;
	pevTarget->angles = VARS(pSpot)->angles;
	pevTarget->punchangle = g_vecZero;
	pevTarget->fixangle = 1;
	UTIL_SetOrigin(pevTarget, pevTarget->origin);
	pevTarget->velocity = g_vecZero;
	pevTarget->fixangle = 2; // VERIFY: overwrites the 1 above; the original writes both
	pevTarget->flags &= ~FL_ONGROUND;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, VARS(pSpot)->origin);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(vecSpot.x);
	WRITE_COORD(vecSpot.y);
	WRITE_COORD(vecSpot.z);
	WRITE_COORD(vecSpot.x);
	WRITE_COORD(vecSpot.y);
	WRITE_COORD(vecSpot.z + 1);
	WRITE_SHORT(m_iEndTeleBall);
	WRITE_BYTE(5);	// count
	WRITE_BYTE(3);	// life * 10
	WRITE_BYTE(3);	// scale * 10
	WRITE_BYTE(10); // velocity along the vector * 10
	WRITE_BYTE(30); // randomness * 10
	MESSAGE_END();

	return true;
}

// orig 0x19ac50: with a beacon out, a beam that teleports what it hits to the beacon.
void CGauss::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
		// halflife-updated's fix, kept (Steven 2026-11-07): GetNextAttackDelay evens out the fire-rate creep
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < PREFS_KeyLoopUpInt("GAUSS", "FireBlankCaust", 2))
	{
		PlayEmptySound();
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
		return;
	}

	CTeleBecon* pBecon = FindPlayersBecon(m_pPlayer);
	if (!pBecon)
	{
		PlayEmptySound();
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
		return;
	}

	m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("GAUSS", "PrimaryHasBeconRefire", 0.5) * m_pPlayer->m_fCurseRefireScale;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	SendWeaponAnim(GAUSS_FIRE2);
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->pev->punchangle.x -= PREFS_KeyLoopUpInt("GAUSS", "PrimaryAngleKick", 4);

	Vector vecSrc = m_pPlayer->GetGunPosition();
	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 4096, dont_ignore_monsters, m_pPlayer->edict(), &tr);

	// the beam starts at the gun's muzzle
	vecSrc = vecSrc + gpGlobals->v_right * 8 + gpGlobals->v_up * -12;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(tr.vecEndPos.x);
	WRITE_COORD(tr.vecEndPos.y);
	WRITE_COORD(tr.vecEndPos.z);
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(50);	 // framerate
	WRITE_BYTE(1);	 // life
	WRITE_BYTE(40);	 // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(128); // r, g, b
	WRITE_BYTE(128);
	WRITE_BYTE(255);
	WRITE_BYTE(250); // brightness
	WRITE_BYTE(50);	 // speed
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(tr.vecEndPos.x);
	WRITE_COORD(tr.vecEndPos.y);
	WRITE_COORD(tr.vecEndPos.z);
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(50);	 // framerate
	WRITE_BYTE(1);	 // life
	WRITE_BYTE(12);	 // width
	WRITE_BYTE(60);	 // noise
	WRITE_BYTE(200); // r, g, b
	WRITE_BYTE(200);
	WRITE_BYTE(255);
	WRITE_BYTE(250); // brightness
	WRITE_BYTE(50);	 // speed
	MESSAGE_END();

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "debris/beamstart4.wav", 1.0, ATTN_NORM);

	if (g_pGameRules->CanPlayerAnnoy(m_pPlayer->pev, tr.pHit ? VARS(tr.pHit) : NULL) && tr.pHit)
	{
		int iCap = RC_CAP_GAUSS_TELEPORTABLE;
		if (CBaseEntity::Instance(tr.pHit)->HasRCCap(iCap))
		{
			if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= PREFS_KeyLoopUpInt("GAUSS", "OtherTeleportCaust", 20))
			{
				if (!pBecon->BeconTeleport(this, VARS(tr.pHit)))
				{
					m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= PREFS_KeyLoopUpInt("GAUSS", "FireBlankCaust", 2);
					EMIT_SOUND(tr.pHit, CHAN_WEAPON, "debris/beamstart14.wav", 0.5, ATTN_NORM);

					MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
					WRITE_BYTE(TE_BEAMPOINTS);
					WRITE_COORD(vecSrc.x);
					WRITE_COORD(vecSrc.y);
					WRITE_COORD(vecSrc.z);
					WRITE_COORD(tr.vecEndPos.x);
					WRITE_COORD(tr.vecEndPos.y);
					WRITE_COORD(tr.vecEndPos.z);
					WRITE_SHORT(g_sModelIndexLaser);
					WRITE_BYTE(0);	 // startframe
					WRITE_BYTE(50);	 // framerate
					WRITE_BYTE(1);	 // life
					WRITE_BYTE(10);	 // width
					WRITE_BYTE(40);	 // noise
					WRITE_BYTE(200); // r, g, b
					WRITE_BYTE(20);
					WRITE_BYTE(10);
					WRITE_BYTE(250); // brightness
					WRITE_BYTE(20);	 // speed
					MESSAGE_END();
				}
				else
				{
					EMIT_SOUND(tr.pHit, CHAN_WEAPON, "debris/beamstart7.wav", 1.0, ATTN_NORM);
					m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= PREFS_KeyLoopUpInt("GAUSS", "OtherTeleportCaust", 20);
				}
			}

			m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("GAUSS", "OtherTeleportedRefire", 0.8) * m_pPlayer->m_fCurseRefireScale;
			return;
		}
	}

	// hit nothing teleportable: a spark where the beam ends
	Vector vecSpark = tr.vecEndPos - gpGlobals->v_forward * 5;
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos);
	WRITE_BYTE(TE_SPRITE);
	WRITE_COORD(vecSpark.x);
	WRITE_COORD(vecSpark.y);
	WRITE_COORD(vecSpark.z);
	WRITE_SHORT(m_iImpactSpr);
	WRITE_BYTE(10);	 // scale * 10
	WRITE_BYTE(255); // brightness
	MESSAGE_END();

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= PREFS_KeyLoopUpInt("GAUSS", "FireBlankCaust", 2);
}

// orig 0x19b8a0: throws the beacon, or with one out teleports the player to it (retrieving it
// when the teleport fails or the player can't afford one).
void CGauss::SecondaryAttack()
{
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
		// halflife-updated's fix, kept (Steven 2026-11-07): GetNextAttackDelay evens out the fire-rate creep
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		return;
	}

	CTeleBecon* pBecon = FindPlayersBecon(m_pPlayer);
	if (!pBecon)
	{
		UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
		SendWeaponAnim(GAUSS_FIRE2);
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		m_pPlayer->pev->punchangle.x = -2;

		Vector vecSrc = m_pPlayer->GetGunPosition();
		CTeleBecon* pNew = CTeleBecon::BecCreate();
		UTIL_SetOrigin(pNew->pev, vecSrc);
		pNew->pev->velocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("GAUSS", "BeconLaunchSpeed", 600);
		pNew->pev->velocity.z += PREFS_KeyLoopUpFloat("GAUSS", "BeconLaunchZBoost", 100);
		pNew->pev->owner = m_pPlayer->edict();
		pNew->pev->angles = UTIL_VecToAngles(pNew->pev->velocity);
		pNew->pev->angles.y -= 180;
		pNew->pev->angles.x -= 180;
		pNew->pev->avelocity = Vector(RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(-100, 100));

		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/rocketfire1.wav", 0.75, ATTN_NORM);

		// the original sets 0.5 and then 0.8
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + 0.8;
		return;
	}

	const char* pszRefire;
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < PREFS_KeyLoopUpInt("GAUSS", "SelfTeleportCaust", 15))
	{
		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < PREFS_KeyLoopUpInt("GAUSS", "RetriveBeconCaust", 5))
			PlayEmptySound();
		else
			RetrieveBecon(this, pBecon);
		pszRefire = "RetriveRefire";
	}
	else
	{
		if (!pBecon->BeconTeleport(this, m_pPlayer->pev))
		{
			if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < PREFS_KeyLoopUpInt("GAUSS", "RetriveBeconCaust", 5))
				PlayEmptySound();
			else
				RetrieveBecon(this, pBecon);
		}
		else
		{
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "debris/beamstart7.wav", 1.0, ATTN_NORM);
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= PREFS_KeyLoopUpInt("GAUSS", "SelfTeleportCaust", 15);
		}
		pszRefire = "SelfTeleRefire";
	}

	m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("GAUSS", pszRefire, 0.8) * m_pPlayer->m_fCurseRefireScale;
}

// orig 0x19c170. Only times the next idle: like the SDK's, it never sends the idle animation.
void CGauss::WeaponIdle()
{
	ResetEmptySound();

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.5 || flRand <= 0.75)
		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);
	else
		m_flTimeWeaponIdle = gpGlobals->time + 3;
}



class CGaussAmmo : public CBasePlayerAmmo
{
	// orig 0x24c850
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_gaussammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x24c810
	void Precache() override
	{
		PRECACHE_MODEL("models/w_gaussammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x24c760. Reads the egon's heading for the amount, as the original does.
	bool AddAmmo(CBaseEntity* pOther) override
	{
		if (pOther->GiveAmmo(PREFS_KeyLoopUpIntClamp("EGON", "AmmoBoxGive", 40, 0, 250), "uranium", PREFS_KeyLoopUpIntClamp("ITEMS", "CellsMax", 200, 0, 250)) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
	// orig 0x24c750
	int GetBotWeight(CBot* pBot, CBaseEntity* pEnemy) override
	{
		return 5;
	}
};
LINK_ENTITY_TO_CLASS(ammo_gaussclip, CGaussAmmo);
