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
//=========================================================
// Rocket Crowbar 1.9: the crossbow. Primary fires the impaler (impail_arrow, CImpArrow): a
// rocket-propelled arrow that skewers a live target and drags it along, pins it to the wall it
// hits, and explodes. Zoomed primary is an instant sniper shot drawn with a smoke tracer
// (CXBowSmokeTracer) that bounces off RC-cap reflectors and sprays the shooter's decal where it
// lands. The SDK's crossbow_bolt is gone. Tunables in rc.ini's [CROSSBOW] heading.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "effects.h"
#include "customentity.h"
#include "decals.h"
#include "UserMessages.h"
#include "Preff.h"

// RC caps (CBaseEntity::HasRCCap) this file tests. VERIFY names: only the values are known.
#define RC_CAP_IMPALE 0x20			 // an impaler arrow can skewer and carry this
#define RC_CAP_SNIPER_REFLECT 0x8000 // the sniper shot bounces off this

#ifndef CLIENT_DLL

//=========================================================
// CImpArrow: the impaler arrow
//=========================================================
class CImpArrow : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void Killed(entvars_t* pevAttacker, int iGib) override;
	bool IsAlive() override;
	int GetBotTargetWeight(CBaseEntity* pBot) override;
	bool BotTargetRealseable(CBaseEntity* pBot) override;
	void ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce) override;
	edict_t* GetRealOwner() override;
	bool GetPullOrg(edict_t* pPulled, Vector& vecOrigin, float& flForce) override;
	void EntIsBeingRemoved(edict_t* pEdict) override;
	virtual bool ProMagEffected(CBaseEntity* pMagnet); // new in CImpArrow (orig slot 151)

	void PlayHitSound();
	void EXPORT ForceVeloThink();
	void EXPORT IABecomeSolid();
	void IABoom();
	CBaseEntity* GetNextAttachedToEnt(CBaseEntity* pStart);
	Vector CalcToOrigin();
	void EXPORT IAAttchedThink();
	void EXPORT IATouch(CBaseEntity* pOther);
	bool IsAttachedTo(CBaseEntity* pEntity);

	EHANDLE m_pAttched;		 // orig +0x280, the victim the arrow carries
	Vector m_vecVelo;		 // orig +0x288, the arrow's own flight velocity, reapplied every think
	CBeam* m_pBeam;			 // orig +0x294, the rope from the victim to the arrow. VERIFY name
	float m_flBDTime;		 // orig +0x298, next bleed damage on the victim
	Vector m_vecAttOrgVAng;	 // orig +0x29c, saved, never read or written
	Vector m_vecAttOrgAng;	 // orig +0x2a8, saved, never read or written
	edict_t* m_pOwn;		 // orig +0x2b4, the player who fired it
	float m_flLastFrame;	 // orig +0x2bc, time of the last carry think
	bool m_blHaveLinkedBeam; // orig +0x2c0, the rope is attached at both ends
	float m_flDieTime;		 // orig +0x2c4, a carrying arrow explodes then
};

LINK_ENTITY_TO_CLASS(impail_arrow, CImpArrow); // orig 0x10e050

// The original's table lists m_vecVelo twice, the second time as a FIELD_CLASSPTR: saving reads
// the velocity as an entity pointer and crashes (reference/dossiers/w2-crossbow-hornet.md,
// "Original bugs"). That entry is left out.
TYPEDESCRIPTION CImpArrow::m_SaveData[] =
	{
		DEFINE_FIELD(CImpArrow, m_pAttched, FIELD_EHANDLE),
		DEFINE_FIELD(CImpArrow, m_vecVelo, FIELD_VECTOR),
		// VERIFY: guard, the original crashes here: DEFINE_FIELD(CImpArrow, m_vecVelo, FIELD_CLASSPTR)
		DEFINE_FIELD(CImpArrow, m_flBDTime, FIELD_TIME),
		DEFINE_FIELD(CImpArrow, m_vecAttOrgVAng, FIELD_VECTOR),
		DEFINE_FIELD(CImpArrow, m_vecAttOrgAng, FIELD_VECTOR),
		DEFINE_FIELD(CImpArrow, m_pOwn, FIELD_EDICT),
		DEFINE_FIELD(CImpArrow, m_flLastFrame, FIELD_TIME),
		DEFINE_FIELD(CImpArrow, m_blHaveLinkedBeam, FIELD_BOOLEAN),
		DEFINE_FIELD(CImpArrow, m_flDieTime, FIELD_TIME),
};

// orig 0x10e080
bool CImpArrow::Save(CSave& save)
{
	if (!CGrenade::Save(save))
		return false;
	return save.WriteFields("CImpArrow", this, m_SaveData, ARRAYSIZE(m_SaveData));
}

// orig 0x10e0d0
bool CImpArrow::Restore(CRestore& restore)
{
	if (!CGrenade::Restore(restore))
		return false;
	return restore.ReadFields("CImpArrow", this, m_SaveData, ARRAYSIZE(m_SaveData));
}

// orig 0x10e120
void CImpArrow::Precache()
{
	PRECACHE_MODEL("models/hvr.mdl");
	PRECACHE_MODEL("sprites/rope.spr");
	PRECACHE_SOUND("weapons/xbow_hitbod1.wav");
	PRECACHE_SOUND("weapons/xbow_hitbod2.wav");
}

// orig 0x10e180
void CImpArrow::PlayHitSound()
{
	switch (RANDOM_LONG(0, 1))
	{
	case 0:
		EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM);
		break;
	}
}

// orig 0x10e200
void CImpArrow::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/hvr.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));

	m_pAttched = NULL;
	m_pBeam = NULL;

	pev->classname = MAKE_STRING("impailer_arrow");
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_SLIDEBOX;
	pev->effects |= EF_LIGHT;
	pev->dmg = PREFS_KeyLoopUpFloat("CROSSBOW", "ImpailerBlastDmg", 30.0);

	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5, 0, PITCH_NORM);

	int r, g, b;
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		r = 255;
		g = 180;
		b = 180;
		break;
	case 1:
		r = 180;
		g = 255;
		b = 180;
		break;
	case 2:
	default:
		r = 180;
		g = 180;
		b = 255;
		break;
	}

	// rocket trail
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(5); // life
	WRITE_BYTE(3); // width
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(RANDOM_LONG(230, 255)); // brightness
	MESSAGE_END();

	SetTouch(&CImpArrow::IATouch);
	SetThink(&CImpArrow::IABecomeSolid);
	pev->nextthink = gpGlobals->time + 0.2;
}

// orig 0x10e4d0: keep flying at m_vecVelo; explode once the flight time runs out
void CImpArrow::ForceVeloThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	pev->velocity = m_vecVelo;
	if (pev->dmgtime < gpGlobals->time)
		IABoom();
}

// orig 0x10e550: clear of the shooter, the arrow can hit him now
void CImpArrow::IABecomeSolid()
{
	SetThink(&CImpArrow::ForceVeloThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = gpGlobals->time + 30.0;
	pev->owner = NULL;
}

// orig 0x10e5d0
void CImpArrow::IABoom()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0, 0, SND_STOP, PITCH_NORM);

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->owner = m_pOwn;

	int iContents = UTIL_PointContents(pev->origin);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	if (iContents == CONTENTS_WATER)
		WRITE_SHORT(g_sModelIndexWExplosion);
	else
		WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(20); // scale * 10
	WRITE_BYTE(15); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	entvars_t* pevOwner = VARS(pev->owner);

	pev->owner = NULL; // can't traceline attack owner if this is set

	// the kill is credited to "crossbow"
	pev->classname = MAKE_STRING("crossbow");
	pev->takedamage = DAMAGE_NO;
	RadiusDamage(pev, pevOwner, pev->dmg, CLASS_NONE, DMG_BLAST);
	pev->classname = MAKE_STRING("impailer_arrow");

	if (pevOwner)
		pev->owner = pevOwner->pContainingEntity;

	RANDOM_FLOAT(0, 1); // the original draws a random number here and discards it
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris1.wav", 0.55, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris2.wav", 0.55, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris3.wav", 0.55, ATTN_NORM);
		break;
	}

	pev->effects |= EF_NODRAW;
	SetThink(&CImpArrow::SUB_Remove);
	SetTouch(NULL);
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_pBeam && !FNullEnt(m_pBeam->edict()))
	{
		m_pBeam->SetThink(&CBeam::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 0.1;
	}
}

// orig 0x10e930
void CImpArrow::Killed(entvars_t* pevAttacker, int iGib)
{
	IABoom();
}

// orig 0x10e950: the next impaler arrow after pStart that carries the same victim as this one
CBaseEntity* CImpArrow::GetNextAttachedToEnt(CBaseEntity* pStart)
{
	if (m_pAttched == NULL)
		return NULL;

	while ((pStart = UTIL_FindEntityByClassname(pStart, "impailer_arrow")) != NULL)
	{
		CImpArrow* pArrow = static_cast<CImpArrow*>(pStart);
		if (pArrow->m_pAttched == NULL)
			continue;
		if (ENTINDEX(pArrow->m_pAttched->edict()) == ENTINDEX(m_pAttched->edict()))
			return pStart;
	}
	return NULL;
}

// orig 0x10ea10: where the victim is pulled to: its RC pull center when it has one, else the arrow
Vector CImpArrow::CalcToOrigin()
{
	if (m_pAttched != NULL)
	{
		Vector vecPull = GetPullCenter(m_pAttched->edict());
		if (vecPull != g_vecZero)
			return vecPull;
	}
	return pev->origin;
}

// orig 0x10eaf0: carrying a victim. It bleeds, and is dragged along at the arrow's speed.
void CImpArrow::IAAttchedThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_pAttched == NULL || !m_pOwn || m_flDieTime < gpGlobals->time)
	{
		IABoom();
		return;
	}

	if (FNullEnt(m_pAttched->edict()))
	{
		IABoom();
		return;
	}

	// let go of anything dead, hidden or fully transparent
	if (!m_pAttched->IsAlive() || (m_pAttched->pev->effects & EF_NODRAW) != 0 ||
		((m_pAttched->pev->rendermode == kRenderTransColor || m_pAttched->pev->rendermode == kRenderTransTexture) && m_pAttched->pev->renderamt == 0))
	{
		IABoom();
		return;
	}

	if (m_flBDTime < gpGlobals->time)
	{
		m_flBDTime = gpGlobals->time + RANDOM_FLOAT(0.3, 0.5);

		pev->classname = MAKE_STRING("crossbow");
		float flDamage = PREFS_KeyLoopUpFloat("CROSSBOW", "ImpailerBleedDamage", 2.0);
		m_pAttched->TakeDamage(pev, VARS(m_pOwn), flDamage, DMG_SLASH);
		UTIL_BloodDrips(m_pAttched->pev->origin, -pev->velocity.Normalize(), m_pAttched->BloodColor(), 512);
		pev->classname = MAKE_STRING("impailer_arrow");
	}

	const int iCap = RC_CAP_IMPALE;
	if (m_pAttched == NULL || FNullEnt(m_pAttched->edict()) || !m_pAttched->IsAlive() || !m_pAttched->HasRCCap(iCap))
	{
		IABoom();
		return;
	}

	// keep the rope alive while carrying
	if (m_pBeam && !FNullEnt(m_pBeam->edict()))
	{
		m_pBeam->SetThink(&CBeam::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 1.0;
	}

	Vector vecTo = CalcToOrigin();
	Vector vecDelta = vecTo - m_pAttched->Center();
	float flSpeed = vecDelta.Length() / (gpGlobals->time - m_flLastFrame);
	m_pAttched->pev->velocity = vecDelta.Normalize() * flSpeed;

	pev->velocity = m_vecVelo;
	pev->dmg = PREFS_KeyLoopUpFloat("CROSSBOW", "ImpailerBlastDmgAttached", 20.0);
	m_flLastFrame = gpGlobals->time;

	if (m_blHaveLinkedBeam)
		return;

	// VERIFY: guard, the original crashes here (EntIsBeingRemoved can clear m_pBeam)
	if (!m_pBeam)
		return;
	m_pBeam->EntsInit(ENTINDEX(m_pAttched->edict()), entindex());
	m_pBeam->SetFlags(BEAM_FSOLID);
	m_pBeam->SetColor(255, 255, 255);
	m_pBeam->SetThink(&CBeam::SUB_Remove);
	m_pBeam->pev->nextthink = gpGlobals->time + 1.0;
	m_blHaveLinkedBeam = true;
}

// orig 0x10f1b0
void CImpArrow::IATouch(CBaseEntity* pOther)
{
	if (pOther == NULL)
		return;

	if (!m_pOwn)
	{
		IABoom();
		return;
	}

	if (pOther->edict() == m_pOwn)
		return;

	if (m_pAttched != NULL && m_pAttched->pev == pOther->pev)
		return;

	// carrying someone into a wall: stick there and pin him
	if (m_pAttched != NULL && FStrEq(STRING(pOther->pev->classname), "worldspawn"))
	{
		UTIL_SetSize(pev, Vector(-16, -16, -16), Vector(16, 16, 16));
		UTIL_SetOrigin(pev, pev->origin - m_vecVelo.Normalize() * 18);
		pev->velocity = g_vecZero;
		m_vecVelo = g_vecZero;
		pev->movetype = MOVETYPE_NONE;

		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0, 0, SND_STOP, PITCH_NORM);
		int iPitch = 98 + RANDOM_LONG(0, 7);
		float flVolume = RANDOM_FLOAT(0.95, 1.0);
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/xbow_hit1.wav", flVolume, ATTN_NORM, 0, iPitch);

		SetTouch(NULL);
		pev->takedamage = DAMAGE_YES;
		pev->health = PREFS_KeyLoopUpFloat("CROSSBOW", "ImpailerHealth", 100.0);
		pev->owner = NULL;
		return;
	}

	bool bBoom = false;
	if (!pOther->IsAlive())
	{
		bBoom = true;
	}
	else
	{
		const int iCap = RC_CAP_IMPALE;
		if (!pOther->HasRCCap(iCap) || m_pAttched != NULL)
			bBoom = true;
	}

	if (!bBoom && !FStrEq(STRING(pOther->pev->classname), "deathball") &&
		g_pGameRules->CanPlayerAnnoy(VARS(m_pOwn), pOther->pev))
	{
		// skewer it
		pev->classname = MAKE_STRING("crossbow");
		float flDamage = PREFS_KeyLoopUpFloat("CROSSBOW", "ImpailerHitDamage", 15.0);
		pOther->TakeDamage(pev, VARS(m_pOwn), flDamage, DMG_SLASH);
		UTIL_BloodDrips(pOther->pev->origin, -pev->velocity.Normalize(), pOther->BloodColor(), 512);
		pev->classname = MAKE_STRING("impailer_arrow");

		if (pOther == NULL || !pOther->IsAlive())
			return;

		m_pAttched = pOther;
		SetThink(&CImpArrow::IAAttchedThink);
		pev->nextthink = gpGlobals->time + 0.01;
		m_flBDTime = gpGlobals->time + RANDOM_FLOAT(0.3, 0.5);
		pev->owner = pOther->edict();

		m_pBeam = CBeam::BeamCreate("sprites/rope.spr", 10);
		m_blHaveLinkedBeam = false;

		m_flDieTime = gpGlobals->time + PREFS_KeyLoopUpFloat("CROSSBOW", "ImpailerLife", 240.0);
		if (FStrEq(STRING(pOther->pev->classname), "curse"))
			m_flDieTime = gpGlobals->time + PREFS_KeyLoopUpFloat("CROSSBOW", "ImpailerCurseLife", 20.0);

		pev->velocity = m_vecVelo;
		pev->effects = 0;
		PlayHitSound();
		m_flLastFrame = gpGlobals->time;
		return;
	}

	IABoom();
}

// orig 0x1122e0
edict_t* CImpArrow::GetRealOwner()
{
	return m_pOwn;
}

// orig 0x1122f0: the carried victim is pulled to the arrow at full force
bool CImpArrow::GetPullOrg(edict_t* pPulled, Vector& vecOrigin, float& flForce)
{
	if (m_pAttched != NULL && m_pAttched->edict() == pPulled)
	{
		vecOrigin = pev->origin;
		flForce = 1.0;
		return true;
	}

	vecOrigin = g_vecZero;
	flForce = 0;
	return false;
}

// orig 0x112380
void CImpArrow::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pBeam && m_pBeam->pev && m_pBeam->edict() == pEdict)
		m_pBeam = NULL;

	if (m_pOwn && m_pOwn == pEdict)
	{
		m_pOwn = NULL;
		Killed(pev, GIB_NORMAL);
	}

	if (m_pAttched != NULL && m_pAttched->edict() == pEdict)
	{
		m_pAttched = NULL;
		Killed(pev, GIB_NORMAL);
	}
}

// orig 0x112440: a bot can let go of anything but the victim it carries
bool CImpArrow::BotTargetRealseable(CBaseEntity* pBot)
{
	return !IsAttachedTo(pBot);
}

// orig 0x1124a0
int CImpArrow::GetBotTargetWeight(CBaseEntity* pBot)
{
	return IsAttachedTo(pBot) ? 10 : 0;
}

// orig 0x112510
bool CImpArrow::IsAttachedTo(CBaseEntity* pEntity)
{
	if (pEntity == NULL || m_pAttched == NULL)
		return false;
	return pEntity->pev == m_pAttched->pev;
}

// orig 0x112570: the pro-magnet curse steers the arrow without changing its speed
void CImpArrow::ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce)
{
	m_vecVelo = vecForce * m_vecVelo.Length();
	pev->angles = UTIL_VecToAngles(m_vecVelo);
}

// orig 0x112620: the magnet steers an empty arrow that isn't its own
bool CImpArrow::ProMagEffected(CBaseEntity* pMagnet)
{
	if (pMagnet->edict() == m_pOwn)
		return false;
	return m_pAttched == NULL;
}

// orig 0x112670
bool CImpArrow::IsAlive()
{
	return true;
}

//=========================================================
// CCrossbowSparkTracer: a point that flies along movedir trailing spark streaks. Nothing in the
// original creates one (CreateST has no caller).
//
// OVERLOADS SOME ENTVARS:
// movedir - direction, speed - its speed, dmgtime - sparks per second, impulse - spark color
//=========================================================
class CCrossbowSparkTracer : public CBaseEntity
{
public:
	void Spawn() override;
	static CCrossbowSparkTracer* CreateST(Vector vecOrigin, Vector vecDir, float flSpeed, float flRate, int iColor);
	void MakeSparks(Vector vecDir, float flSpeed, float flRandom, int iCount);
	void EXPORT STThink();

	float m_flLastThink; // orig +0x34, time of the last think that moved it. VERIFY name
};

// orig 0x10f800
CCrossbowSparkTracer* CCrossbowSparkTracer::CreateST(Vector vecOrigin, Vector vecDir, float flSpeed, float flRate, int iColor)
{
	CCrossbowSparkTracer* pTracer = GetClassPtr((CCrossbowSparkTracer*)NULL);
	pTracer->Spawn();
	pTracer->pev->movedir = vecDir;
	pTracer->pev->speed = flSpeed;
	pTracer->pev->dmgtime = flRate;
	pTracer->pev->impulse = iColor;
	UTIL_SetOrigin(pTracer->pev, vecOrigin);
	return pTracer;
}

// orig 0x10f880
void CCrossbowSparkTracer::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->effects |= EF_NODRAW;
	pev->angles = g_vecZero;
	SetThink(&CCrossbowSparkTracer::STThink);
	pev->nextthink = gpGlobals->time + 0.01;
	m_flLastThink = gpGlobals->time;
}

// orig 0x10f930
void CCrossbowSparkTracer::MakeSparks(Vector vecDir, float flSpeed, float flRandom, int iCount)
{
	Vector vecOrigin = pev->origin;

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecOrigin);
	WRITE_BYTE(TE_STREAK_SPLASH);
	WRITE_COORD(vecOrigin.x);
	WRITE_COORD(vecOrigin.y);
	WRITE_COORD(vecOrigin.z);
	WRITE_COORD(vecDir.x);
	WRITE_COORD(vecDir.y);
	WRITE_COORD(vecDir.z);
	WRITE_BYTE(pev->impulse); // color
	WRITE_SHORT(iCount);
	WRITE_SHORT((int)(flSpeed + 0.5));
	WRITE_SHORT((int)(flRandom + 0.5));
	MESSAGE_END();
}

// orig 0x10fac0
void CCrossbowSparkTracer::STThink()
{
	pev->nextthink = gpGlobals->time + 0.01;

	float flElapsed = gpGlobals->time - m_flLastThink;
	int iSparks = (int)(flElapsed * pev->dmgtime);
	ALERT(at_console, "El: %f WS: %i\n", (double)flElapsed, iSparks);

	if (iSparks <= 0)
		return;

	m_flLastThink = gpGlobals->time;
	if ((double)flElapsed < 0.001)
		flElapsed = 0.001;

	Vector vecEnd = pev->origin + pev->movedir * (flElapsed * pev->speed);

	TraceResult tr;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine(pev->origin, vecEnd, ignore_monsters, ENT(pev), &tr);

	if (tr.pHit != NULL || tr.flFraction != 1.0)
	{
		SetThink(&CCrossbowSparkTracer::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.01;
		MakeSparks(tr.vecPlaneNormal, 400, 500, 60);
	}

	float flStep = (flElapsed * pev->speed * tr.flFraction) / (float)iSparks;
	Vector vecStep = pev->movedir;
	int iSteps = (int)(flElapsed * pev->dmgtime * tr.flFraction);
	for (int i = 0; i < iSteps; i++)
	{
		MakeSparks(-pev->movedir, 400, 100, 10);
		pev->origin = pev->origin + vecStep * flStep;
	}

	UTIL_SetOrigin(pev, pev->origin);
}

//=========================================================
// CXBowSmokeTracer: the sniper shot's smoke line, a hidden point that flies at 2000 u/s from the
// gun to the hit (or to the reflector, then on to the second hit) trailing a beam.
//=========================================================
class CXBowSmokeTracer : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT BounceThink();
	static CXBowSmokeTracer* CreateTR(entvars_t* pevOwner, Vector vecStart, Vector vecEnd, Vector vecColor);
	static CXBowSmokeTracer* CreateTRBounce(entvars_t* pevOwner, Vector vecStart, Vector vecBounce, Vector vecEnd, Vector vecColor);

	int m_iSmoke; // orig +0x34, sprites/smoke.spr
};

// orig 0x10fdb0
void CXBowSmokeTracer::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	pev->classname = MAKE_STRING("crossbow_tracer");
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	pev->rendermode = kRenderTransColor;
	pev->renderamt = 0;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin(pev, pev->origin);
}

// orig 0x10fe60
void CXBowSmokeTracer::Precache()
{
	m_iSmoke = PRECACHE_MODEL("sprites/smoke.spr");
}

// orig 0x10fea0: at the reflector, turn toward the end at the same speed.
//
// OVERLOADS SOME ENTVARS:
// movedir - the bounce point, angles - the end point
void CXBowSmokeTracer::BounceThink()
{
	UTIL_SetOrigin(pev, pev->movedir);

	Vector vecDelta = pev->angles - pev->movedir;
	float flSpeed = pev->velocity.Length();
	pev->velocity = vecDelta.Normalize() * flSpeed;

	SetThink(&CXBowSmokeTracer::SUB_Remove);
	pev->nextthink = vecDelta.Length() / flSpeed + gpGlobals->time;
}

// orig 0x10ffe0
CXBowSmokeTracer* CXBowSmokeTracer::CreateTR(entvars_t* pevOwner, Vector vecStart, Vector vecEnd, Vector vecColor)
{
	CXBowSmokeTracer* pTracer = GetClassPtr((CXBowSmokeTracer*)NULL);
	pTracer->Spawn();
	pTracer->pev->owner = ENT(pevOwner);
	UTIL_SetOrigin(pTracer->pev, vecStart);

	Vector vecDelta = vecEnd - vecStart;
	pTracer->pev->velocity = vecDelta.Normalize() * 2000;
	pTracer->SetThink(&CXBowSmokeTracer::SUB_Remove);
	pTracer->pev->nextthink = vecDelta.Length() / 2000 + gpGlobals->time;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(pTracer->entindex());
	WRITE_SHORT(pTracer->m_iSmoke);
	WRITE_BYTE(12); // life
	WRITE_BYTE(3);	// width
	WRITE_BYTE((int)vecColor.x);
	WRITE_BYTE((int)vecColor.y);
	WRITE_BYTE((int)vecColor.z);
	WRITE_BYTE(250); // brightness
	MESSAGE_END();

	return pTracer;
}

// orig 0x110280
CXBowSmokeTracer* CXBowSmokeTracer::CreateTRBounce(entvars_t* pevOwner, Vector vecStart, Vector vecBounce, Vector vecEnd, Vector vecColor)
{
	CXBowSmokeTracer* pTracer = GetClassPtr((CXBowSmokeTracer*)NULL);
	pTracer->Spawn();
	pTracer->pev->owner = ENT(pevOwner);
	UTIL_SetOrigin(pTracer->pev, vecStart);

	Vector vecDelta = vecBounce - vecStart;
	pTracer->pev->velocity = vecDelta.Normalize() * 2000;
	pTracer->SetThink(&CXBowSmokeTracer::BounceThink);
	pTracer->pev->nextthink = vecDelta.Length() / 2000 + gpGlobals->time;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(pTracer->entindex());
	WRITE_SHORT(pTracer->m_iSmoke);
	WRITE_BYTE(12); // life
	WRITE_BYTE(3);	// width
	WRITE_BYTE((int)vecColor.x);
	WRITE_BYTE((int)vecColor.y);
	WRITE_BYTE((int)vecColor.z);
	WRITE_BYTE(250); // brightness
	MESSAGE_END();

	pTracer->pev->movedir = vecBounce;
	pTracer->pev->angles = vecEnd;

	return pTracer;
}

// The sniper shot's noise where it lands on something that takes no damage, from a throwaway
// entity there. The original repeats this inline at each such spot.
static void XBowHitSound(const Vector& vecOrigin)
{
	CBaseEntity* pEmitter = GetClassPtr((CBaseEntity*)NULL);
	pEmitter->Spawn();
	pEmitter->pev->classname = MAKE_STRING("sound_emiiter");
	pEmitter->SetThink(&CBaseEntity::SUB_Remove);
	pEmitter->pev->nextthink = gpGlobals->time + 0.1;
	UTIL_SetOrigin(pEmitter->pev, vecOrigin);
	EMIT_SOUND_DYN(pEmitter->edict(), CHAN_WEAPON, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, PITCH_NORM);

	if (UTIL_PointContents(vecOrigin) != CONTENTS_WATER)
		UTIL_Sparks(vecOrigin);
}

static void XBowHitBodySound(edict_t* pEdict)
{
	switch (RANDOM_LONG(0, 1))
	{
	case 0:
		EMIT_SOUND(pEdict, CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(pEdict, CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM);
		break;
	}
}

#endif // CLIENT_DLL

LINK_ENTITY_TO_CLASS(weapon_crossbow, CCrossbow); // orig 0x110550

// orig 0x110580
void CCrossbow::Spawn()
{
	Precache();
	m_iId = WEAPON_CROSSBOW;
	SET_MODEL(ENT(pev), "models/w_crossbow.mdl");

	m_iDefaultAmmo = PREFS_KeyLoopUpInt("CROSSBOW", "PickupAmmo", CROSSBOW_DEFAULT_GIVE);
	m_bSniperFiring = false;
	m_flSniperFireTime = gpGlobals->time;

	FallInit(); // get ready to fall down.
}

// orig 0x110600. The original sends WeapPickup here; halflife-updated sends it from
// CBasePlayer::AddPlayerItem for every weapon, so only the base call is left.
void CCrossbow::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	// As the original: this weapon tells the player it was picked up (AddPlayerItem doesn't)
	MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
	WRITE_BYTE(m_iId);
	MESSAGE_END();
}

// orig 0x110690
void CCrossbow::Precache()
{
	PRECACHE_MODEL("models/w_crossbow.mdl");
	PRECACHE_MODEL("models/v_crossbow.mdl");
	PRECACHE_MODEL("models/p_crossbow.mdl");

	PRECACHE_SOUND("weapons/xbow_fire1.wav");
	PRECACHE_SOUND("weapons/xbow_reload1.wav");

	m_iSmoke = PRECACHE_MODEL("sprites/smoke.spr");
	UTIL_PrecacheOther("impail_arrow");

	PRECACHE_SOUND("weapons/xbow_hitbod1.wav");
	PRECACHE_SOUND("weapons/xbow_hitbod2.wav");
	PRECACHE_SOUND("weapons/xbow_hit1.wav");
}

// orig 0x110760
bool CCrossbow::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "bolts";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("CROSSBOW", "MaxAmmo", 80, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = PREFS_KeyLoopUpIntClamp("CROSSBOW", "ClipSize", CROSSBOW_MAX_CLIP, 0, 250);
	p->iSlot = 2;
	p->iPosition = 2;
	p->iId = WEAPON_CROSSBOW;
	p->iFlags = 0;
	p->iWeight = CROSSBOW_WEIGHT;
	return true;
}

// orig 0x110810
bool CCrossbow::Deploy()
{
	if (0 != m_iClip)
		return DefaultDeploy("models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW1, "bow");
	return DefaultDeploy("models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW2, "bow");
}

// orig 0x110860
void CCrossbow::Holster()
{
	m_fInReload = false; // cancel any reload in progress.

	if (0 != m_fInZoom)
	{
		SecondaryAttack();
	}

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	if (0 != m_iClip)
		SendWeaponAnim(CROSSBOW_HOLSTER1);
	else
		SendWeaponAnim(CROSSBOW_HOLSTER2);
}

// orig 0x1108e0: zoomed, the sniper shot; else the impaler. There is no single-player bolt, so
// zoom picks the sniper shot in every game mode.
void CCrossbow::PrimaryAttack()
{
	if (m_bSniperFiring)
		return;

	if (0 != m_fInZoom)
		FireSniperBolt();
	else
		FireBolt();
}

// orig 0x110920: the shooter's spray logo (or a lambda when he has none) on whatever is up to
// 128 units past where the shot landed, along vecAngles
void CCrossbow::PaintDecal(edict_t* pEdict, Vector vecOrigin, Vector vecAngles)
{
#ifndef CLIENT_DLL
	CBasePlayer* pPlayer = pEdict ? static_cast<CBasePlayer*>(GET_PRIVATE(pEdict)) : NULL;

	// VERIFY: guard, the original crashes here (it calls IsNetClient before its own NULL check)
	if (!pPlayer || !pPlayer->IsNetClient())
		return;

	int nFrames = pPlayer ? pPlayer->GetCustomDecalFrames() : -1;
	int iEntIndex = ENTINDEX(pEdict);

	UTIL_MakeVectors(vecAngles);
	TraceResult tr;
	UTIL_TraceLine(vecOrigin, vecOrigin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, &tr);

	if (nFrames == -1)
		UTIL_DecalTrace(&tr, DECAL_LAMBDA6);
	else
		UTIL_PlayerDecalTrace(&tr, iEntIndex, (int)pev->frame, true);
#endif
}

// orig 0x110a50
void CCrossbow::FireSniperBolt()
{
	m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("CROSSBOW", "CrossbowRefire", 0.9) * m_pPlayer->m_fCurseRefireScale;

	if (m_iClip == 0)
	{
		PlayEmptySound();
		return;
	}

	DoSniperFire();
}

// orig 0x110ac0
void CCrossbow::DoSniperFire()
{
	if (m_iClip == 0)
	{
		m_bSniperFiring = false;
		PlayEmptySound();
		return;
	}

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_iClip--;

	int iPitch = 93 + RANDOM_LONG(0, 0xF);
	float flVolume = RANDOM_FLOAT(0.95, 1.0);
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/xbow_fire1.wav", flVolume, ATTN_NORM, 0, iPitch);

	if (m_iClip != 0)
	{
		iPitch = 93 + RANDOM_LONG(0, 0xF);
		flVolume = RANDOM_FLOAT(0.95, 1.0);
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/xbow_reload1.wav", flVolume, ATTN_NORM, 0, iPitch);
		SendWeaponAnim(CROSSBOW_FIRE1);
	}
	else if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0)
	{
		SendWeaponAnim(CROSSBOW_FIRE3);
	}

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

#ifndef CLIENT_DLL
	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors(anglesAim);
	Vector vecSrc = m_pPlayer->GetGunPosition() - gpGlobals->v_up * 2;
	Vector vecDir = gpGlobals->v_forward;
	m_bSniperFiring = false;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);
	// The original also works out vecDir * 2000 and GetGunPosition() - v_up * 8 here and never
	// uses them.

	const int iCap = RC_CAP_SNIPER_REFLECT;
	if (tr.pHit != NULL && CBaseEntity::Instance(tr.pHit)->HasRCCap(iCap))
	{
		// a reflector: it takes the reflector damage and the shot bounces off it
		XBowHitSound(tr.vecEndPos);

		ClearMultiDamage();
		CBaseEntity* pReflector = CBaseEntity::Instance(tr.pHit);
		float flFract = pReflector->ExtreemeDamageFract();
		float flDamage = PREFS_KeyLoopUpFloat("CROSSBOW", "SecondaryDamageHitReflector", 500.0) * flFract;
		pReflector->TraceAttack(m_pPlayer->pev, flDamage, vecDir, &tr, DMG_BULLET | DMG_NEVERGIB);
		ApplyMultiDamage(pev, m_pPlayer->pev);

		float flDot = -DotProduct(tr.vecPlaneNormal, vecDir);
		vecDir = tr.vecPlaneNormal * 2 * flDot + vecDir;
		Vector vecBounce = tr.vecEndPos;

		TraceResult tr2;
		UTIL_TraceLine(vecBounce, vecBounce + vecDir * 8192, dont_ignore_monsters, tr.pHit, &tr2);

		if (tr2.pHit != NULL && tr2.pHit->v.takedamage != 0)
		{
			// VERIFY: the original plays the body hit on the reflector, and passes the first
			// trace to TraceAttack
			XBowHitBodySound(tr.pHit);

			ClearMultiDamage();
			CBaseEntity* pHit = CBaseEntity::Instance(tr2.pHit);
			flFract = pHit->ExtreemeDamageFract();
			flDamage = PREFS_KeyLoopUpFloat("CROSSBOW", "SecondaryDamage", 4000.0) * flFract;
			pHit->TraceAttack(m_pPlayer->pev, flDamage, vecDir, &tr, DMG_BULLET | DMG_NEVERGIB);
			ApplyMultiDamage(pev, m_pPlayer->pev);

			CXBowSmokeTracer::CreateTRBounce(m_pPlayer->pev, vecSrc, vecBounce, tr2.vecEndPos, Vector(255, 16, 16));
		}
		else
		{
			XBowHitSound(tr2.vecEndPos);
			CXBowSmokeTracer::CreateTRBounce(m_pPlayer->pev, vecSrc, vecBounce, tr2.vecEndPos, Vector(16, 16, 255));
		}

		PaintDecal(m_pPlayer->edict(), tr2.vecEndPos, UTIL_VecToAngles(vecDir));
		return;
	}

	// VERIFY: guard, the original crashes here (it reads tr.pHit without a NULL check, as the
	// SDK's FireSniperBolt does); a NULL pHit takes the no-damage branch
	if (tr.pHit != NULL && tr.pHit->v.takedamage != 0)
	{
		XBowHitBodySound(tr.pHit);

		ClearMultiDamage();
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		float flFract = pHit->ExtreemeDamageFract();
		float flDamage = PREFS_KeyLoopUpFloat("CROSSBOW", "SecondaryDamage", 4000.0) * flFract;
		pHit->TraceAttack(m_pPlayer->pev, flDamage, vecDir, &tr, DMG_BULLET | DMG_NEVERGIB);
		ApplyMultiDamage(pev, m_pPlayer->pev);

		CXBowSmokeTracer::CreateTR(m_pPlayer->pev, vecSrc, tr.vecEndPos, Vector(255, 16, 16));
	}
	else
	{
		XBowHitSound(tr.vecEndPos);
		CXBowSmokeTracer::CreateTR(m_pPlayer->pev, vecSrc, tr.vecEndPos, Vector(26, 16, 255));
	}

	PaintDecal(m_pPlayer->edict(), tr.vecEndPos, anglesAim);
#endif
}

// orig 0x111930: launch an impaler arrow, which kicks the shooter back
void CCrossbow::FireBolt()
{
	if (m_iClip == 0)
	{
		PlayEmptySound();
		return;
	}

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

	m_iClip--;

	int iPitch = 93 + RANDOM_LONG(0, 0xF);
	float flVolume = RANDOM_FLOAT(0.95, 1.0);
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/xbow_fire1.wav", flVolume, ATTN_NORM, 0, iPitch);

	if (m_iClip != 0)
	{
		iPitch = 93 + RANDOM_LONG(0, 0xF);
		flVolume = RANDOM_FLOAT(0.95, 1.0);
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/xbow_reload1.wav", flVolume, ATTN_NORM, 0, iPitch);
		SendWeaponAnim(CROSSBOW_FIRE1);
	}
	else if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0)
	{
		SendWeaponAnim(CROSSBOW_FIRE3);
	}

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors(anglesAim);

	anglesAim.x = -anglesAim.x;
	Vector vecSrc = m_pPlayer->GetGunPosition() - gpGlobals->v_up * 2;
	Vector vecDir = gpGlobals->v_forward;

#ifndef CLIENT_DLL
	CImpArrow* pArrow = static_cast<CImpArrow*>(CBaseEntity::Create("impail_arrow", vecSrc, anglesAim, m_pPlayer->edict()));
	pArrow->pev->speed = 2000;
	pArrow->pev->velocity = vecDir * pArrow->pev->speed;
	pArrow->pev->angles = UTIL_VecToAngles(pArrow->pev->velocity);
	pArrow->m_pOwn = m_pPlayer->edict();
	pArrow->m_vecVelo = pArrow->pev->velocity;
#endif

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("CROSSBOW", "ImpailerRefire", 0.9) * m_pPlayer->m_fCurseRefireScale;
	m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("CROSSBOW", "ImpailerRefireNextZoom", 0.9) * m_pPlayer->m_fCurseRefireScale;

	if (m_iClip == 0)
		// The original stores the bare 0.75 without the current time, a moment long past, so
		// the idle comes at once.
		m_flTimeWeaponIdle = 0.75;
	else
		m_flTimeWeaponIdle = gpGlobals->time + 5.0;

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, PITCH_NORM);

	// recoil
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * 600;
	m_pPlayer->pev->punchangle.x -= PREFS_KeyLoopUpFloat("CROSSBOW", "ImpailerAngleKick", 7.0);
}

// orig 0x111db0
void CCrossbow::SecondaryAttack()
{
	if (m_bSniperFiring)
		return;

	if (0 == m_fInZoom)
	{
		m_pPlayer->m_iFOV = 20;
		m_fInZoom = 1;
	}
	else
	{
		m_pPlayer->m_iFOV = 0; // 0 means reset to default fov
		m_fInZoom = 0;
	}

	pev->nextthink = gpGlobals->time + 0.1;
	m_flNextSecondaryAttack = gpGlobals->time + 1.0;
}

// orig 0x111e50
void CCrossbow::Reload()
{
	if (m_bSniperFiring)
		return;

	if (0 != m_fInZoom)
	{
		SecondaryAttack();
	}

	if (DefaultReload(5, CROSSBOW_RELOAD, 4.5))
	{
		int iPitch = 93 + RANDOM_LONG(0, 0xF);
		float flVolume = RANDOM_FLOAT(0.95, 1.0);
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/xbow_reload1.wav", flVolume, ATTN_NORM, 0, iPitch);
	}
}

// orig 0x111f00
void CCrossbow::WeaponIdle()
{
	if (m_bSniperFiring)
	{
		if (m_flSniperFireTime < gpGlobals->time)
			DoSniperFire();
		return;
	}

	m_pPlayer->GetAutoaimVector(AUTOAIM_2DEGREES); // get the autoaim vector but ignore it;  used for autoaim crosshair in DM

	ResetEmptySound();

	if (m_flTimeWeaponIdle < gpGlobals->time)
	{
		float flRand = RANDOM_FLOAT(0, 1);
		if (flRand <= 0.75)
		{
			if (0 != m_iClip)
			{
				SendWeaponAnim(CROSSBOW_IDLE1);
			}
			else
			{
				SendWeaponAnim(CROSSBOW_IDLE2);
			}
			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);
		}
		else
		{
			if (0 != m_iClip)
			{
				SendWeaponAnim(CROSSBOW_FIDGET1);
				m_flTimeWeaponIdle = gpGlobals->time + 90.0 / 30.0;
			}
			else
			{
				SendWeaponAnim(CROSSBOW_FIDGET2);
				m_flTimeWeaponIdle = gpGlobals->time + 80.0 / 30.0;
			}
		}
	}
}

// RC's bot object, as the original reads it: a CBaseEntity (its Center() is called) with an int
// at +0x6a0. These are the original i386 bot's offsets in a class this library never defines
// (reference/DEVIATIONS.md, "Bot interface").
static CBaseEntity* RCBotEnt(CBot* pBot) { return reinterpret_cast<CBaseEntity*>(pBot); }
static int RCBotField6a0(CBot* pBot) { return *reinterpret_cast<int*>(reinterpret_cast<byte*>(pBot) + 0x6a0); } // VERIFY name: 1 keeps the bot off the crossbow

// orig 0x112080
int CCrossbow::GetBotWeight(CBot* pBot, CBaseEntity* pEnemy)
{
	if (pEnemy == NULL)
		return ItemInfoArray[m_iId].iWeight;
	return BotFirePrimary(pBot, pEnemy) ? 5000 : 1;
}

// orig 0x1120e0: zoom in, then fire
int CCrossbow::BotFireButton(CBot* pBot, CBaseEntity* pEnemy)
{
	if (0 == m_fInZoom)
		SecondaryAttack();
	return IN_ATTACK;
}

// orig 0x112120
bool CCrossbow::BotFireSecondary(CBot* pBot, CBaseEntity* pEnemy)
{
	return false;
}

// orig 0x112130: only at long range
bool CCrossbow::BotFirePrimary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (pEnemy != NULL)
	{
		float flDist = (pEnemy->Center() - RCBotEnt(pBot)->Center()).Length();
		if (RCBotField6a0(pBot) == 1 || !(flDist > 768))
			return false;
	}
	return true;
}

class CCrossbowAmmo : public CBasePlayerAmmo
{
	// orig 0x248490
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_crossbow_clip.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x248450
	void Precache() override
	{
		PRECACHE_MODEL("models/w_crossbow_clip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x2483a0
	bool AddAmmo(CBaseEntity* pOther) override
	{
		int iMax = PREFS_KeyLoopUpIntClamp("CROSSBOW", "MaxAmmo", 80, 0, 250);
		int iGive = PREFS_KeyLoopUpIntClamp("CROSSBOW", "ClipGive", AMMO_CROSSBOWCLIP_GIVE, 0, 250);
		if (pOther->GiveAmmo(iGive, "bolts", iMax) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_crossbow, CCrossbowAmmo); // orig 0x112050
