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
*	Rocket Crowbar 1.9: the tripmine's mine (CTripmineLaser, monster_tripmine, in place of the
*	SDK's CTripmineGrenade) is a laser that burns whatever crosses it and bounces off surfaces
*	with RC cap 0x10000. Secondary fire throws a headcrab on a rope (CHeadcrabTeather) that
*	sticks to what it hits and pulls the thrower toward it. Numbers come from rc.ini's
*	[TRIPMINES] heading.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "effects.h"
#include "customentity.h"
#include "gamerules.h"
#include "Preff.h"

#define TRIPMINE_PRIMARY_VOLUME 450

// HasRCCap bits this file tests. VERIFY names: the original's are not in the binary.
#define RC_CAP_TEATHERABLE 0x80		  // a headcrab teather sticks to it (and to the world)
#define RC_CAP_LASER_REFLECTS 0x10000 // a tripmine laser bounces off it

// A CBeam the original holds by raw pointer and tests as "pointer, pev, edict in use". The
// original's DispatchRemoveEnt is empty, so those pointers outlive the beams they point to and
// the test reads freed memory. VERIFY: guard, the original reads freed memory here: ours are
// EHANDLEs, which go NULL instead (reference/dossiers/w2-gauss-trip-squeak-satchel.md).
static CBeam* LiveBeam(EHANDLE& hBeam)
{
	CBaseEntity* pBeam = hBeam;
	if (!pBeam || !pBeam->pev || FNullEnt(pBeam->pev))
		return NULL;
	edict_t* pEdict = pBeam->pev->pContainingEntity;
	if (!pEdict || !pEdict->pvPrivateData || 0 != pEdict->free)
		return NULL;
	return (CBeam*)pBeam;
}

// The headcrab teather's test: UTIL_IsValidEntity on the beam's edict.
static CBeam* ValidBeam(EHANDLE& hBeam)
{
	CBaseEntity* pBeam = hBeam;
	if (!pBeam || !pBeam->pev || FNullEnt(pBeam->pev))
		return NULL;
	if (!UTIL_IsValidEntity(pBeam->edict()))
		return NULL;
	return (CBeam*)pBeam;
}

//=========================================================
// CHeadcrabTeather: the tripmine's secondary fire
//=========================================================
class CHeadcrabTeather : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	Vector GetBotShootOrg() override;
	int GetBotTargetWeight(CBaseEntity* pBot) override;
	bool BotTargetRealseable(CBaseEntity* pBot) override;
	void ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce) override;
	bool GetPullOrg(edict_t* pPulled, Vector& vecOrigin, float& flForce) override;
	void EntIsBeingRemoved(edict_t* pEdict) override;

	virtual bool ProMagEffected(CBaseEntity* pMagnet);

	void EXPORT ActThink();
	void EXPORT DetThink();
	void EXPORT HTTouch(CBaseEntity* pOther);
	void EXPORT AttThink();
	Vector CalcToOrigin();
	bool ShouldBotKillMe(CBaseEntity* pBot);

	EHANDLE m_hBeam;	   // orig +0x280, a CBeam* there: the rope
	EHANDLE m_hOwner;	   // orig +0x284, who threw it. VERIFY name
	EHANDLE m_hAttached;   // orig +0x28c, what it stuck to (NULL for the world). VERIFY name
	Vector m_vecVelocity;  // orig +0x294, its velocity, restored when it touches its owner. VERIFY name
	float m_flDieTime;	   // orig +0x2a0, when a stuck teather lets go. VERIFY name
	float m_flLastThink;   // orig +0x2a4, time of the last pull. VERIFY name
};

LINK_ENTITY_TO_CLASS(headcrab_teather, CHeadcrabTeather);

// orig 0x22a610
void CHeadcrabTeather::Precache()
{
	PRECACHE_MODEL("models/headcrab.mdl");
}

// orig 0x22a640
void CHeadcrabTeather::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/headcrab.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	pev->takedamage = DAMAGE_YES;
	pev->health = 1;

	int r = 0, g = 0, b = 0; // VERIFY: unset in the original if RANDOM_LONG(0, 2) were out of range
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		r = 255;
		g = 100;
		b = 130;
		break;
	case 1:
		r = 130;
		g = 255;
		b = 100;
		break;
	case 2:
		r = 100;
		g = 130;
		b = 255;
		break;
	}

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(8); // life
	WRITE_BYTE(5); // width
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(RANDOM_LONG(230, 255)); // brightness
	MESSAGE_END();

	pev->classname = MAKE_STRING("headcrab_teather");
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_SLIDEBOX;
	pev->frame = 0;
	pev->body = 0;
	pev->sequence = 0;

	SetThink(&CHeadcrabTeather::ActThink);
	pev->nextthink = gpGlobals->time + 0.1;
	SetTouch(&CHeadcrabTeather::HTTouch);
}

// orig 0x22a8e0: first think in flight. Lets it touch its thrower and runs out the rope.
void CHeadcrabTeather::ActThink()
{
	if (m_hOwner == NULL)
	{
		DetThink();
		return;
	}

	pev->owner = NULL;
	pev->effects |= EF_LIGHT;
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0.8, 0.6, 0, PITCH_NORM);

	CBeam* pBeam = CBeam::BeamCreate("sprites/rope.spr", 10);
	m_hBeam = pBeam;
	pBeam->EntsInit(m_hOwner->entindex(), entindex());
	pBeam->SetFlags(BEAM_FSOLID);
	pBeam->SetColor(255, 255, 255);
	float flLife = PREFS_KeyLoopUpFloat("TRIPMINES", "TeatherLife", 0.3);
	pBeam->SetThink(&CBaseEntity::SUB_Remove);
	pBeam->pev->nextthink = (1 + flLife) + gpGlobals->time;

	SetThink(&CHeadcrabTeather::DetThink);
	pev->nextthink = gpGlobals->time + PREFS_KeyLoopUpFloat("TRIPMINES", "TeatherFlightLife", 0.3);
}

// orig 0x22aaa0
void CHeadcrabTeather::DetThink()
{
	Killed(pev, 0);
}

// orig 0x22aad0: a small explosion, rope cut.
void CHeadcrabTeather::Killed(entvars_t* pevAttacker, int iGib)
{
	pev->effects = EF_NODRAW;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->velocity = g_vecZero;
	STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav");
	STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav");

	if (CBeam* pBeam = ValidBeam(m_hBeam))
	{
		pBeam->SetThink(&CBaseEntity::SUB_Remove);
		pBeam->pev->nextthink = gpGlobals->time + 0.01;
	}

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	// VERIFY: the original tests for contents 0, which no point has (CONTENTS_EMPTY is -1), so
	// it always draws the fireball, even underwater (original bug, kept)
	if (UTIL_PointContents(pev->origin) == 0)
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

// orig 0x22ad40: sticks to the world or to something alive with the teather cap; anything else
// detonates it.
void CHeadcrabTeather::HTTouch(CBaseEntity* pOther)
{
	if (!pOther || (m_hOwner != NULL && pOther->pev && pOther->pev->pContainingEntity == m_hOwner->edict()))
	{
		pev->velocity = m_vecVelocity;
		return;
	}

	if (m_hOwner == NULL || !m_hOwner->IsAlive())
	{
		DetThink();
		return;
	}

	int iCap = RC_CAP_TEATHERABLE;
	if (0 != strcmp(STRING(pOther->pev->classname), "worldspawn") && !pOther->HasRCCap(iCap))
	{
		DetThink();
		return;
	}

	iCap = RC_CAP_TEATHERABLE;
	if (!pOther->IsBSPModel() && pOther->IsAlive() && pOther->HasRCCap(iCap))
		m_hAttached = pOther;
	else
		m_hAttached = NULL;

	STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav");
	STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav");
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "common/null.wav", 1.0, ATTN_NORM);

	SetThink(&CHeadcrabTeather::AttThink);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->movetype = MOVETYPE_NONE;
	pev->velocity = g_vecZero;
	pev->effects = 0;

	if (m_hAttached != NULL)
	{
		// ride along with what it stuck to, rope from the thrower to it
		pev->effects |= EF_NODRAW;
		pev->movetype = MOVETYPE_FOLLOW;
		pev->solid = SOLID_NOT;
		UTIL_SetOrigin(pev, pOther->pev->origin);
		pev->aiment = pOther->edict();

		if (CBeam* pOld = ValidBeam(m_hBeam))
		{
			pOld->SetThink(&CBaseEntity::SUB_Remove);
			pOld->pev->nextthink = gpGlobals->time + 0.01;
		}

		CBeam* pBeam = CBeam::BeamCreate("sprites/rope.spr", 10);
		m_hBeam = pBeam;
		pBeam->EntsInit(m_hOwner->entindex(), m_hAttached->entindex());
		pBeam->SetFlags(BEAM_FSOLID);
		pBeam->SetColor(255, 255, 255);
		pBeam->SetThink(&CBaseEntity::SUB_Remove);
		pBeam->pev->nextthink = gpGlobals->time + 1.0;

		pOther->TakeDamage(pev, m_hOwner->pev, PREFS_KeyLoopUpFloat("TRIPMINES", "TeatherImpactDamage", 5.0), DMG_GENERIC);
	}

	if (CBeam* pBeam = ValidBeam(m_hBeam))
	{
		pBeam->SetThink(&CBaseEntity::SUB_Remove);
		pBeam->pev->nextthink = gpGlobals->time + 1.0;
	}

	m_flDieTime = gpGlobals->time + PREFS_KeyLoopUpFloat("TRIPMINES", "TeatherAttachedLife", 45.0);
	m_flLastThink = gpGlobals->time;
}

// orig 0x22b2a0: where everything pulling on the thrower pulls him.
Vector CHeadcrabTeather::CalcToOrigin()
{
	if (m_hOwner == NULL)
		return g_vecZero;
	return GetPullCenter(m_hOwner->edict());
}

// orig 0x22b310: while stuck. Keeps the rope up and drags the thrower toward the pull center.
void CHeadcrabTeather::AttThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_hOwner == NULL || !m_hOwner->IsAlive())
	{
		DetThink();
		return;
	}

	if (m_hAttached != NULL && !m_hAttached->IsAlive())
	{
		DetThink();
		return;
	}

	if (m_hAttached != NULL)
	{
		// gone invisible, or lost the cap
		if ((m_hAttached->pev->effects & EF_NODRAW) != 0)
		{
			DetThink();
			return;
		}
		if ((m_hAttached->pev->rendermode == kRenderTransColor || m_hAttached->pev->rendermode == kRenderTransTexture) && m_hAttached->pev->renderamt == 0)
		{
			DetThink();
			return;
		}
		int iCap = RC_CAP_TEATHERABLE;
		if (!m_hAttached->HasRCCap(iCap))
		{
			DetThink();
			return;
		}
	}

	if (m_flDieTime < gpGlobals->time)
	{
		DetThink();
		return;
	}

	if (pev->owner && (m_hOwner->pev->origin - pev->origin).Length() > 32)
		pev->owner = NULL;

	CBeam* pBeam = ValidBeam(m_hBeam);
	float flBeamLife;
	if (pBeam)
	{
		flBeamLife = 1.0;
	}
	else
	{
		pBeam = CBeam::BeamCreate("sprites/rope.spr", 10);
		m_hBeam = pBeam;
		if (m_hAttached == NULL)
			pBeam->EntsInit(m_hOwner->entindex(), entindex());
		else
			pBeam->EntsInit(m_hOwner->entindex(), m_hAttached->entindex());
		pBeam->SetFlags(BEAM_FSOLID);
		pBeam->SetColor(255, 255, 255);
		flBeamLife = 0.3;
	}
	pBeam->SetThink(&CBaseEntity::SUB_Remove);
	pBeam->pev->nextthink = flBeamLife + gpGlobals->time;

	if (m_hAttached != NULL)
		UTIL_SetOrigin(pev, m_hAttached->pev->origin);

	Vector vecTo = CalcToOrigin();
	if (vecTo != g_vecZero)
	{
		Vector vecDelta = vecTo - m_hOwner->Center();
		float flSpeed = vecDelta.Length() / (gpGlobals->time - m_flLastThink);
		m_hOwner->pev->velocity = vecDelta.Normalize() * flSpeed;
	}

	m_flLastThink = gpGlobals->time;
}

// orig 0x22dbc0: pulls its thrower to TeatherMinDist from it.
bool CHeadcrabTeather::GetPullOrg(edict_t* pPulled, Vector& vecOrigin, float& flForce)
{
	if (m_hOwner != NULL && m_hOwner->edict() == pPulled)
	{
		float flMinDist = PREFS_KeyLoopUpFloat("TRIPMINES", "TeatherMinDist", 64.0);
		Vector vecDelta = m_hOwner->Center() - pev->origin;
		if (!(vecDelta.Length() < flMinDist))
		{
			vecOrigin = pev->origin + vecDelta.Normalize() * flMinDist;
			flForce = 4.0;
			return true;
		}
	}

	vecOrigin = g_vecZero;
	flForce = 0;
	return false;
}

// orig 0x22dd90
void CHeadcrabTeather::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_hOwner != NULL && m_hOwner->edict() == pEdict)
		m_hOwner = NULL;
	if (m_hAttached != NULL && m_hAttached->edict() == pEdict)
		m_hAttached = NULL;
}

// orig 0x22de20: the pro magnet curse steers it, at its current speed.
void CHeadcrabTeather::ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce)
{
	pev->velocity = vecForce * pev->velocity.Length();
	m_vecVelocity = pev->velocity;
	pev->angles = UTIL_VecToAngles(pev->velocity);
}

// orig 0x22ded0: magnets other than the thrower move it while it flies.
bool CHeadcrabTeather::ProMagEffected(CBaseEntity* pMagnet)
{
	if (m_hOwner == NULL) // VERIFY: guard, the original crashes here (reads m_hOwner unchecked)
		return false;
	if (pMagnet->edict() != m_hOwner->edict() && pev->velocity.Length() > 0)
		return true;
	return false;
}

// orig 0x22df60
bool CHeadcrabTeather::BotTargetRealseable(CBaseEntity* pBot)
{
	return true;
}

// orig 0x22df70
int CHeadcrabTeather::GetBotTargetWeight(CBaseEntity* pBot)
{
	if (m_hOwner == NULL)
		return 0;
	return (g_pGameRules->PlayerRelationship(m_hOwner, pBot) != GR_TEAMMATE) ? 4 : 0;
}

// orig 0x22dff0
bool CHeadcrabTeather::ShouldBotKillMe(CBaseEntity* pBot)
{
	if (m_hOwner == NULL)
		return false;
	return g_pGameRules->PlayerRelationship(m_hOwner, pBot) != GR_TEAMMATE;
}

// orig 0x22e060
Vector CHeadcrabTeather::GetBotShootOrg()
{
	return pev->origin - Vector(0, 0, 16);
}

//=========================================================
// CTripmineLaser: the planted mine
//=========================================================
class CTripmineLaser : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	edict_t* GetRealOwner() override;
	void EntIsBeingRemoved(edict_t* pEdict) override;

	void EXPORT WarningThink();
	void EXPORT PowerupThink();
	void EXPORT DoBeamTrace();

	void MakeBeam(Vector vecEnd);
	void KillBeam();

	Vector m_vecDir;	   // orig +0x280, the way it points
	int m_iLaserSprite;	   // orig +0x28c, sprites/zbeam2.spr. VERIFY name
	float m_flPowerUp;	   // orig +0x290
	float m_flDieTime;	   // orig +0x294, TRIPMINES Life after planting. VERIFY name
	EHANDLE m_hOwner;	   // orig +0x298, what it's planted on
	EHANDLE m_hBeam;	   // orig +0x2a0, a CBeam* there (m_pBeam): the laser
	EHANDLE m_hBounceBeam; // orig +0x2a4, a CBeam* there: the laser's bounce. VERIFY name
	Vector m_posOwner;	   // orig +0x2a8
	Vector m_angleOwner;   // orig +0x2b4
	edict_t* m_pRealOwner; // orig +0x2c0, the player who planted it
};

LINK_ENTITY_TO_CLASS(monster_tripmine, CTripmineLaser);

// orig 0x22b910
void CTripmineLaser::Spawn()
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_NOT;

	SET_MODEL(ENT(pev), "models/v_tripmine.mdl");
	pev->frame = 0;
	pev->body = 3;
	pev->sequence = TRIPMINE_WORLD;
	ResetSequenceInfo();
	pev->framerate = 0;

	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
	UTIL_SetOrigin(pev, pev->origin);

	m_hBounceBeam = NULL;
	m_flDieTime = gpGlobals->time + PREFS_KeyLoopUpFloat("TRIPMINES", "Life", 120.0);

	if ((pev->spawnflags & 1) != 0)
	{
		// power up quickly
		m_flPowerUp = gpGlobals->time + 1.0;
	}
	else
	{
		// power up in 2.5 seconds
		m_flPowerUp = gpGlobals->time + 2.5;
	}

	SetThink(&CTripmineLaser::PowerupThink);
	pev->nextthink = gpGlobals->time + 0.2;

	pev->takedamage = DAMAGE_YES;
	pev->health = 1;

	if (pev->owner != NULL)
	{
		// play deploy sound
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/mine_deploy.wav", 1.0, ATTN_NORM);
		EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/mine_charge.wav", 0.2, ATTN_NORM); // chargeup

		m_pRealOwner = pev->owner; // see CTripmineGrenade for why we do this
	}

	UTIL_MakeAimVectors(pev->angles);

	m_vecDir = gpGlobals->v_forward;
}

// orig 0x22bb30
void CTripmineLaser::Precache()
{
	PRECACHE_MODEL("models/v_tripmine.mdl");
	PRECACHE_SOUND("weapons/mine_deploy.wav");
	PRECACHE_SOUND("weapons/mine_activate.wav");
	PRECACHE_SOUND("weapons/mine_charge.wav");
	m_iLaserSprite = PRECACHE_MODEL("sprites/zbeam2.spr");
}

// orig 0x22bbb0
void CTripmineLaser::WarningThink()
{
	// set to power up
	SetThink(&CTripmineLaser::PowerupThink);
	pev->nextthink = gpGlobals->time + 1.0;
}

// orig 0x22bc00: the armed mine's think. Burns what the laser hits, bounces it off reflective
// surfaces, and blows up when its planter or what it's planted on goes away or it runs out of
// life.
void CTripmineLaser::DoBeamTrace()
{
	if (!m_pRealOwner || !m_pRealOwner->pvPrivateData || 0 != m_pRealOwner->free)
	{
		m_pRealOwner = NULL;
		Killed(pev, 0);
		return;
	}

	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + m_vecDir * 4096, dont_ignore_monsters, ENT(pev), &tr);
	MakeBeam(tr.vecEndPos);

	int iCap = RC_CAP_LASER_REFLECTS;
	if (tr.pHit && CBaseEntity::Instance(tr.pHit)->HasRCCap(iCap))
	{
		Vector vecRefl = m_vecDir + tr.vecPlaneNormal * 2 * -DotProduct(tr.vecPlaneNormal, m_vecDir);
		Vector vecStart = tr.vecEndPos + tr.vecPlaneNormal;

		TraceResult trRefl;
		UTIL_TraceLine(vecStart, vecStart + vecRefl * 4096, dont_ignore_monsters, tr.pHit, &trRefl);

		if (trRefl.pHit && trRefl.pHit->v.takedamage != DAMAGE_NO && CBaseEntity::Instance(trRefl.pHit)->IsAlive())
		{
			CBaseEntity* pHit = CBaseEntity::Instance(trRefl.pHit);
			ClearMultiDamage();
			pHit->TraceAttack(pev, PREFS_KeyLoopUpFloat("TRIPMINES", "LaserBounceDamage", 1000.0) * pHit->ExtreemeDamageFract(), vecRefl, &trRefl, DMG_ENERGYBEAM);
			ApplyMultiDamage(pev, m_pRealOwner ? VARS(m_pRealOwner) : pev);
		}

		CBeam* pBounce = LiveBeam(m_hBounceBeam);
		if (!pBounce)
		{
			pBounce = CBeam::BeamCreate(g_pModelNameLaser, 24);
			m_hBounceBeam = pBounce;
			if (pBounce)
			{
				pBounce->PointsInit(trRefl.vecEndPos, vecStart);
				pBounce->pev->modelindex = m_iLaserSprite;
				pBounce->SetColor(255, 0, 0);
				pBounce->SetScrollRate(32);
				pBounce->SetBrightness(200);
				pBounce->pev->iuser3 |= 3;
			}
		}
		pBounce->pev->origin = trRefl.vecEndPos;
		pBounce->pev->angles = vecStart;
		pBounce->SetThink(&CBaseEntity::SUB_Remove);
		pBounce->pev->nextthink = gpGlobals->time + 1.0;
		pBounce->pev->iuser3 = 3;
		pev->iuser3 = 2;
	}
	else if ((CBaseEntity*)m_hBounceBeam != NULL)
	{
		CBaseEntity* pBounce = m_hBounceBeam;
		pBounce->pev->effects |= EF_NODRAW;
		pBounce->SUB_Remove();
		m_hBounceBeam = NULL;
	}

	if (tr.pHit && tr.pHit->v.takedamage != DAMAGE_NO && CBaseEntity::Instance(tr.pHit)->IsAlive())
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		ClearMultiDamage();
		pHit->TraceAttack(pev, PREFS_KeyLoopUpFloat("TRIPMINES", "LaserDamage", 2000.0) * pHit->ExtreemeDamageFract(), m_vecDir, &tr, DMG_ENERGYBEAM);
		ApplyMultiDamage(pev, m_pRealOwner ? VARS(m_pRealOwner) : pev);
	}

	if (m_hOwner != NULL && m_posOwner == m_hOwner->pev->origin && m_angleOwner == m_hOwner->pev->angles &&
		(m_hOwner->pev->effects & EF_NODRAW) == 0 && m_hOwner->pev->solid != SOLID_NOT && !(m_flDieTime < gpGlobals->time))
	{
		if (!(pev->health <= 0 || pev->takedamage == DAMAGE_NO || (pev->effects & EF_NODRAW) != 0 || !m_pRealOwner))
		{
			SetThink(&CTripmineLaser::DoBeamTrace);
			pev->nextthink = gpGlobals->time + 0.1;
			return;
		}
	}

	Killed(pev, 0);
}

// orig 0x22c480
void CTripmineLaser::PowerupThink()
{
	TraceResult tr;

	if (m_hOwner == NULL)
	{
		// find an owner
		edict_t* oldowner = pev->owner;
		pev->owner = NULL;
		UTIL_TraceLine(pev->origin + m_vecDir * 8, pev->origin - m_vecDir * 32, dont_ignore_monsters, ENT(pev), &tr);
		if (0 != tr.fStartSolid || (oldowner && tr.pHit == oldowner))
		{
			pev->owner = oldowner;
			m_flPowerUp += 0.1;
			pev->nextthink = gpGlobals->time + 0.1;
			return;
		}
		if (tr.flFraction < 1.0)
		{
			pev->owner = tr.pHit;
			m_hOwner = CBaseEntity::Instance(pev->owner);
			m_posOwner = m_hOwner->pev->origin;
			m_angleOwner = m_hOwner->pev->angles;
		}
		else
		{
			STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/mine_deploy.wav");
			STOP_SOUND(ENT(pev), CHAN_BODY, "weapons/mine_charge.wav");
			SetThink(&CBaseEntity::SUB_Remove);
			pev->nextthink = gpGlobals->time + 0.1;
			ALERT(at_console, "WARNING:Tripmine at %.0f, %.0f, %.0f removed\n", pev->origin.x, pev->origin.y, pev->origin.z);
			KillBeam();
			return;
		}
	}
	else if (m_posOwner != m_hOwner->pev->origin || m_angleOwner != m_hOwner->pev->angles)
	{
		// disable
		STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/mine_deploy.wav");
		STOP_SOUND(ENT(pev), CHAN_BODY, "weapons/mine_charge.wav");
		CBaseEntity* pMine = Create("weapon_tripmine", pev->origin + m_vecDir * 24, pev->angles);
		pMine->pev->spawnflags |= SF_NORESPAWN;

		SetThink(&CBaseEntity::SUB_Remove);
		KillBeam();
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	if (gpGlobals->time > m_flPowerUp)
	{
		// make solid
		pev->solid = SOLID_BBOX;
		UTIL_SetOrigin(pev, pev->origin);

		m_hBeam = NULL;
		DoBeamTrace();

		// play enabled sound
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/mine_activate.wav", 0.5, ATTN_NORM, 1, 75);
	}
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x22c950
void CTripmineLaser::KillBeam()
{
	if (CBeam* pBeam = LiveBeam(m_hBeam))
	{
		UTIL_Remove(pBeam);
		m_hBeam = NULL;
	}
	if (CBeam* pBounce = LiveBeam(m_hBounceBeam))
	{
		UTIL_Remove(pBounce);
		m_hBounceBeam = NULL;
	}
}

// orig 0x22ca50: the laser from the mine to vecEnd, kept up for a second at a time.
void CTripmineLaser::MakeBeam(Vector vecEnd)
{
	CBeam* pBeam = LiveBeam(m_hBeam);
	if (!pBeam)
	{
		pBeam = CBeam::BeamCreate(g_pModelNameLaser, 24);
		m_hBeam = pBeam;
		if (!pBeam)
			return;
		pBeam->PointEntInit(vecEnd, entindex());
		pBeam->pev->modelindex = m_iLaserSprite;
		pBeam->SetColor(255, 0, 0);
		pBeam->SetScrollRate(32);
		pBeam->SetBrightness(200);
		pBeam->pev->iuser3 |= 3; // VERIFY meaning: RC marks its laser beams in iuser3
	}

	pBeam->pev->origin = vecEnd;
	pBeam->SetThink(&CBaseEntity::SUB_Remove);
	pBeam->pev->nextthink = gpGlobals->time + 1.0;
	pBeam->pev->iuser3 = 3;
	pev->iuser3 = 2;
}

// orig 0x22cc10: any hit of 1 or more sets it off.
bool CTripmineLaser::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (pev->health <= 0 || flDamage < 1)
		return false;

	Killed(pevAttacker, 0);
	return true;
}

// orig 0x22cc60
void CTripmineLaser::Killed(entvars_t* pevAttacker, int iGib)
{
	pev->owner = m_pRealOwner;
	pev->takedamage = DAMAGE_NO;
	pev->health = 0;
	KillBeam();

	int iContents = UTIL_PointContents(pev->origin);
	pev->dmg = PREFS_KeyLoopUpFloat("TRIPMINES", "BlastDamage", 40.0);
	pev->dmgtime = -1;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	if (iContents == CONTENTS_WATER)
		WRITE_SHORT(g_sModelIndexWExplosion);
	else
		WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(16); // scale * 10
	WRITE_BYTE(18); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	entvars_t* pevOwner = pev->owner ? VARS(pev->owner) : pev;
	pev->owner = NULL;
	::RadiusDamage(pev->origin, pev, pevOwner, pev->dmg, pev->dmg * 2.1, CLASS_NONE, DMG_BLAST);

	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x22da80
edict_t* CTripmineLaser::GetRealOwner()
{
	return m_pRealOwner;
}

// orig 0x22da90: blows up when its planter leaves.
void CTripmineLaser::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_hOwner != NULL && m_hOwner->edict() == pEdict)
		m_hOwner = NULL;

	CBaseEntity* pBeam = m_hBeam;
	if (pBeam && pBeam->pev && pBeam->pev->pContainingEntity == pEdict)
		m_hBeam = NULL;

	CBaseEntity* pBounce = m_hBounceBeam;
	if (pBounce && pBounce->pev && pBounce->pev->pContainingEntity == pEdict)
		m_hBounceBeam = NULL;

	if (m_pRealOwner && m_pRealOwner == pEdict)
	{
		m_pRealOwner = NULL;
		Killed(pev, 0);
	}
}

//=========================================================
// CTripmine
//=========================================================
LINK_ENTITY_TO_CLASS(weapon_tripmine, CTripmine);

// orig 0x22ced0
void CTripmine::Spawn()
{
	Precache();
	m_iId = WEAPON_TRIPMINE;
	SET_MODEL(ENT(pev), "models/v_tripmine.mdl");
	pev->frame = 0;
	pev->body = 3;
	pev->sequence = TRIPMINE_GROUND;
	// ResetSequenceInfo( );
	pev->framerate = 0;

	FallInit(); // get ready to fall down

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("TRIPMINES", "PickupAmmo", 1, 0, 250);

	if (!g_pGameRules->IsDeathmatch())
	{
		UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 28));
	}
}

// orig 0x22cfd0
void CTripmine::Precache()
{
	PRECACHE_MODEL("models/v_tripmine.mdl");
	PRECACHE_MODEL("models/p_tripmine.mdl");
	UTIL_PrecacheOther("monster_tripmine");
}

// orig 0x22d020
bool CTripmine::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Trip Mine";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("TRIPMINES", "MaxAmmo", 10, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_TRIPMINE;
	p->iWeight = -1;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return true;
}

// orig 0x22d0d0
bool CTripmine::Deploy()
{
	pev->body = 0;
	return DefaultDeploy("models/v_tripmine.mdl", "models/p_tripmine.mdl", TRIPMINE_DRAW, "trip");
}

// orig 0x22d120
void CTripmine::Holster()
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		// out of mines
		m_pPlayer->ClearWeaponBit(m_iId);
		SetThink(&CTripmine::DestroyItem);
		pev->nextthink = gpGlobals->time + 0.1;
	}

	SendWeaponAnim(TRIPMINE_HOLSTER);
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

// orig 0x22d1f0: plants a mine, at most 30 per player.
void CTripmine::PrimaryAttack()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		return;
	if (CountEntsClaimedByEnt(m_pPlayer->edict(), "monster_tripmine") >= 31)
		return;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = gpGlobals->v_forward;

	TraceResult tr;

	UTIL_TraceLine(vecSrc, vecSrc + vecAiming * 128, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

	if (tr.flFraction < 1.0)
	{
		CBaseEntity::Instance(tr.pHit); // the original looks it up and drops it: no FL_CONVEYOR test

		Vector angles = UTIL_VecToAngles(tr.vecPlaneNormal);

		CBaseEntity::Create("monster_tripmine", tr.vecEndPos + tr.vecPlaneNormal * 8, angles, m_pPlayer->edict());

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		{
			// no more mines!
			RetireWeapon();
			return;
		}

		SendWeaponAnim(TRIPMINE_DRAW);
	}

	m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("TRIPMINES", "PrimaryRefire", 0.3) * m_pPlayer->m_fCurseRefireScale;
	m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("TRIPMINES", "PrimaryRefire", 0.3) * m_pPlayer->m_fCurseRefireScale;
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);
}

// orig 0x22d4c0: throws a headcrab teather, or sets off the one already out.
void CTripmine::SecondaryAttack()
{
	edict_t* pFind = NULL;
	while (true)
	{
		pFind = FIND_ENTITY_BY_CLASSNAME(pFind, "headcrab_teather");
		if (FNullEnt(pFind))
			break;

		CHeadcrabTeather* pTeather = (CHeadcrabTeather*)CBaseEntity::Instance(pFind);
		// VERIFY: guard, the original crashes here (reads m_hOwner unchecked, a teather whose
		// thrower is gone)
		if (pTeather && pTeather->m_hOwner != NULL && pTeather->m_hOwner->edict() == m_pPlayer->edict())
		{
			pTeather->DetThink();
			m_flNextPrimaryAttack = gpGlobals->time + 0.3;
			m_flNextSecondaryAttack = gpGlobals->time + 0.3;
			return;
		}
	}

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		return;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = gpGlobals->v_forward;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		RetireWeapon();
	else
		SendWeaponAnim(TRIPMINE_DRAW);

	CHeadcrabTeather* pTeather = (CHeadcrabTeather*)CBaseEntity::Create("headcrab_teather", vecSrc, m_pPlayer->pev->v_angle, m_pPlayer->edict());
	float flRange = PREFS_KeyLoopUpFloat("TRIPMINES", "TeatherRange", 256.0);
	float flFlight = PREFS_KeyLoopUpFloat("TRIPMINES", "TeatherFlightLife", 0.3);
	pTeather->pev->velocity = vecAiming * (flRange / flFlight);
	pTeather->pev->angles = UTIL_VecToAngles(pTeather->pev->velocity);
	pTeather->m_hOwner = m_pPlayer;

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/rocketfire1.wav", 0.6, ATTN_NORM);

	m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("TRIPMINES", "SecondaryRefire", 0.6) * m_pPlayer->m_fCurseRefireScale;
	m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("TRIPMINES", "SecondaryRefire", 0.6) * m_pPlayer->m_fCurseRefireScale;
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);
}

// orig 0x22d850
void CTripmine::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
	{
		SendWeaponAnim(TRIPMINE_DRAW);
	}
	else
	{
		RetireWeapon();
		return;
	}

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.25)
	{
		iAnim = TRIPMINE_IDLE1;
		m_flTimeWeaponIdle = gpGlobals->time + 90.0 / 30.0;
	}
	else if (flRand <= 0.75)
	{
		iAnim = TRIPMINE_IDLE2;
		m_flTimeWeaponIdle = gpGlobals->time + 60.0 / 30.0;
	}
	else
	{
		iAnim = TRIPMINE_FIDGET;
		m_flTimeWeaponIdle = gpGlobals->time + 100.0 / 30.0;
	}

	SendWeaponAnim(iAnim);
}
