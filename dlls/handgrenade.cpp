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
*	Rocket Crowbar 1.9: the hand grenade throws no ordinary grenades. Primary throws a gravity
*	grenade (CImplosionGrenade, "impgren"): five seconds after the pin is pulled it rises, hangs
*	in the air sucking everything near into itself (killing what touches it), then explodes.
*	Secondary throws a bubble grenade (CBubleGrenade, "bubgren", declared in weapons.h) that
*	pushes everything away and makes walkers slide on negative friction for a while
*	(CFictionTimer, "friction_timer"). The tunables are in rc.ini's [GRENADES] heading.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "shake.h"
#include "effects.h"
#include "customentity.h"
#include "decals.h"
#include "UserMessages.h"
#include "Preff.h"

// util.cpp, static there; RC's util.cpp exports it
extern unsigned short FixedUnsigned16(float value, float scale);

#define HANDGRENADE_PRIMARY_VOLUME 450

// Rocket Crowbar 1.9: sprites, precached by CHandGrenade::Precache
int ImpFlare;		  // orig .bss 0x30db18: sprites/blueflare1.spr
int ImpInitExlpo;	  // orig .bss 0x30db14: sprites/xflare1.spr
int g_iFricGrenFlare; // orig .bss 0x30db1c: sprites/c-tele1.spr
int g_iFricGrenRing;  // orig .bss 0x30db20: sprites/shockwave.spr

#ifndef CLIENT_DLL

//=========================================================
// CFictionTimer: one per sliding entity (its pev->owner). While it runs the owner glows pink and
// has friction SlippyFriction; when it ends (or the owner dies) the owner's render settings and
// friction, saved in the timer's own pev, are put back.
//=========================================================
class CFictionTimer : public CBaseEntity
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	static void CreateOrUpdateTimer(entvars_t* pevTarget, float flTime);
	void Spawn() override;
	void EXPORT DoUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT DoThink();
	void PlayerWasKilled(edict_t* pPlayer) override;

	float m_fSetFrict; // orig +0x34
};

LINK_ENTITY_TO_CLASS(friction_timer, CFictionTimer); // orig 0x1a3530

TYPEDESCRIPTION CFictionTimer::m_SaveData[] =
	{
		DEFINE_FIELD(CFictionTimer, m_fSetFrict, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CFictionTimer, CBaseEntity); // orig 0x1a3560, 0x1a35b0

// orig 0x1a3600. Extends the target's timer by flTime, or starts one (saving the target's render
// settings and friction in the timer's pev, and fading the screen pink for a client).
void CFictionTimer::CreateOrUpdateTimer(entvars_t* pevTarget, float flTime)
{
	bool bFound = false;
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, g_vecZero, 4096)) != NULL)
	{
		if (0 == strcmp(STRING(pEntity->pev->classname), "friction_timer") && pEntity->pev->owner == pevTarget->pContainingEntity)
		{
			pEntity->Use(CBaseEntity::Instance(pevTarget->pContainingEntity), CBaseEntity::Instance(pevTarget->pContainingEntity), USE_ON, flTime);
			bFound = true;
		}
	}
	if (bFound)
		return;

	CBaseEntity* pTimer = CBaseEntity::Create("friction_timer", g_vecZero, g_vecZero, pevTarget->pContainingEntity);
	pTimer->pev->owner = pevTarget->pContainingEntity;
	pTimer->Spawn(); // a second time: Create spawned it already (original)
	pTimer->pev->dmg = flTime + gpGlobals->time;
	pTimer->pev->renderamt = pevTarget->renderamt;
	pTimer->pev->renderfx = pevTarget->renderfx;
	pTimer->pev->rendermode = pevTarget->rendermode;
	pTimer->pev->friction = pevTarget->friction;
	static_cast<CFictionTimer*>(pTimer)->m_fSetFrict = PREFS_KeyLoopUpFloat("GRENADES", "SlippyFriction", -0.1);

	if (CBaseEntity::Instance(pevTarget->pContainingEntity)->IsNetClient())
		UTIL_ScreenFade(CBaseEntity::Instance(pevTarget->pContainingEntity), Vector(255, 128, 255), 2.0, 0.0, 48, FFADE_STAYOUT);
}

// orig 0x1a3860. Ends pev's slide (player.cpp, when the player respawns).
void DestroyFrictionTimer(entvars_t* pev)
{
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, g_vecZero, 4096)) != NULL)
	{
		if (0 == strcmp(STRING(pEntity->pev->classname), "friction_timer") && pEntity->pev->owner == pev->pContainingEntity)
			pEntity->Use(CBaseEntity::Instance(pev->pContainingEntity), CBaseEntity::Instance(pev->pContainingEntity), USE_OFF, 0);
	}
}

// orig 0x1a3950. pev->dmg holds the time the slide ends.
void CFictionTimer::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->classname = MAKE_STRING("friction_timer");
	SET_MODEL(ENT(pev), "");
	UTIL_SetOrigin(pev, g_vecZero);
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	SetThink(&CFictionTimer::DoThink);
	SetUse(&CFictionTimer::DoUse);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x1a3a60. On: add value seconds (counting from now if the time had run out). Off: end the
// slide now.
void CFictionTimer::DoUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (useType != USE_OFF)
	{
		if (pev->dmg <= gpGlobals->time)
			pev->dmg = value + gpGlobals->time;
		else
			pev->dmg = value + pev->dmg;
		return;
	}

	CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
	if (pOwner && pOwner->IsNetClient())
		UTIL_ScreenFade(pOwner, Vector(255, 128, 255), 2.0, 0.0, 48, FFADE_IN);

	SetThink(&CFictionTimer::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;

	// VERIFY: no null check on pev->owner in the original; every caller passes the owner's timer
	VARS(pev->owner)->renderamt = pev->renderamt;
	VARS(pev->owner)->renderfx = pev->renderfx;
	VARS(pev->owner)->rendermode = pev->rendermode;
	VARS(pev->owner)->friction = pev->friction;
}

// orig 0x1a3c20. Keeps the owner glowing and slippery (and off ladders) while the time runs and
// it lives; a deathball always counts as alive. A frozen owner gets its friction back but keeps
// the glow (original).
void CFictionTimer::DoThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (!pev->owner || FNullEnt(pev->owner) || pev->owner->free || !pev->owner->pvPrivateData)
	{
		SetThink(&CFictionTimer::SUB_Remove);
		return;
	}

	if ((pev->owner->v.flags & FL_FROZEN) != 0)
	{
		SetThink(&CFictionTimer::SUB_Remove);
		pev->owner->v.friction = pev->friction;
		return;
	}

	CBasePlayer* pPlayer = static_cast<CBasePlayer*>(CBaseEntity::Instance(pev->owner));

	if (0 == strcmp(STRING(VARS(pev->owner)->classname), "deathball") || (pev->owner->v.health > 0 && pev->owner->v.deadflag == DEAD_NO && pPlayer->IsAlive()))
	{
		if (pev->dmg >= gpGlobals->time)
		{
			if (pPlayer->IsPlayer() && pPlayer && pPlayer->IsOnLadder())
				pPlayer->Jump();

			pev->owner->v.renderfx = kRenderFxGlowShell;
			pev->owner->v.renderamt = 64;
			pev->owner->v.rendercolor = Vector(255, 128, 255);
			pev->owner->v.friction = m_fSetFrict;
			return;
		}
	}

	if (pPlayer->IsNetClient())
		UTIL_ScreenFade(pPlayer, Vector(255, 128, 255), 2.0, 0.0, 48, FFADE_IN);

	pev->owner->v.renderamt = pev->renderamt;
	pev->owner->v.renderfx = pev->renderfx;
	pev->owner->v.rendermode = pev->rendermode;
	pev->owner->v.friction = pev->friction;

	SetThink(&CFictionTimer::SUB_Remove);
}

// orig 0x1a81e0
void CFictionTimer::PlayerWasKilled(edict_t* pPlayer)
{
	if (pev->owner == pPlayer)
	{
		Use(this, this, USE_OFF, 0);
		pev->owner = NULL;
	}
}

//=========================================================
// CBubleGrenade (weapons.h)
//=========================================================
LINK_ENTITY_TO_CLASS(bubgren, CBubleGrenade); // orig 0x1a3f10

// orig 0x1a3f40. CGrenade::ShootTimed, with its own touch and think, friction 0.5, and the
// damage CGrenade::Spawn gives (100), which SlippyGoBoom does not use.
void CBubleGrenade::ShootBubTimed(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float time)
{
	CBubleGrenade* pGrenade = GetClassPtr((CBubleGrenade*)NULL);
	pGrenade->Spawn();
	UTIL_SetOrigin(pGrenade->pev, vecStart);
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles(pGrenade->pev->velocity);
	pGrenade->pev->owner = ENT(pevOwner);

	pGrenade->SetTouch(&CBubleGrenade::SlippyTouch); // Bounce if touched

	// Take one second off of the desired detonation time and set the think to PreDetonate. PreDetonate
	// will insert a DANGER sound into the world sound list and delay detonation for one second so that
	// the grenade explodes after the exact amount of time specified in the call to ShootTimed().

	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink(&CBubleGrenade::PreDetThink);
	pGrenade->pev->nextthink = gpGlobals->time + 0.1;
	if (time < 0.1)
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = Vector(0, 0, 0);
	}

	pGrenade->pev->sequence = RANDOM_LONG(3, 6);
	pGrenade->pev->framerate = 1.0;

	pGrenade->pev->gravity = 0.5;
	pGrenade->pev->friction = 0.5;

	SET_MODEL(ENT(pGrenade->pev), "models/w_grenade.mdl");
}

// orig 0x1a40f0. Everything alive within SlippyGrenRadius that has RC cap 0x800000 (or is a
// deathball) and is in sight takes up to SlippyMaxDamage, is pushed away (at least 100), and,
// when it walks or bounces, slides for up to SlippyMaxTime seconds, all scaled by nearness.
void CBubleGrenade::SlippyGoBoom()
{
	Vector vecSrc = pev->origin + Vector(0, 0, 8);
	float flMaxTime = PREFS_KeyLoopUpFloat("GRENADES", "SlippyMaxTime", 9.0);
	float flMaxPush = PREFS_KeyLoopUpFloat("GRENADES", "SlippyMaxPush", 1800.0);
	float flMaxDamage = PREFS_KeyLoopUpFloat("GRENADES", "SlippyMaxDamage", 20.0);
	float flRadius = PREFS_KeyLoopUpFloat("GRENADES", "SlippyGrenRadius", 512.0);
	Vector vecColor = Vector(255, 128, 255);

	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, flRadius)) != NULL)
	{
		if (!pEntity->IsAlive())
			continue;

		bool bSkip = false;
		int iCap = 0x800000; // VERIFY: an RC cap bit, name unknown
		if (!pEntity->HasRCCap(iCap))
		{
			if (0 != strcmp(STRING(VARS(pEntity->edict())->classname), "deathball"))
				bSkip = true;
		}
		if (bSkip)
			continue;

		float flFrac = 1 - (pEntity->pev->origin - vecSrc).Length() / flRadius;
		float flTime = flFrac * flMaxTime;
		float flPush = flFrac * flMaxPush;
		float flDamage = flFrac * flMaxDamage;
		if (flTime < 0.1)
			continue;
		if (flPush < 100)
			flPush = 100;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, pEntity->pev->origin, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEntity->edict())
			continue;

		if (flDamage > 1)
		{
			if (pev->owner && !FNullEnt(pev->owner))
				pEntity->TakeDamage(pev, VARS(pev->owner), flDamage, DMG_SONIC);
			else
				pEntity->TakeDamage(pev, pev, flDamage, DMG_SONIC);
		}

		if (pEntity->pev->movetype == MOVETYPE_WALK || pEntity->pev->movetype == MOVETYPE_BOUNCE)
			CFictionTimer::CreateOrUpdateTimer(pEntity->pev, flTime);

		pEntity->pev->velocity = pEntity->pev->velocity + (pEntity->pev->origin - vecSrc).Normalize() * flPush;
		if ((pEntity->pev->flags & FL_ONGROUND) != 0)
		{
			pEntity->pev->flags &= ~FL_ONGROUND;
			pEntity->pev->velocity.z += 50;
			UTIL_SetOrigin(pEntity->pev, pEntity->pev->origin + Vector(0, 0, 1));
		}
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/beamstart10.wav", 1.0, ATTN_NORM, 0, 110);
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart4.wav", 1.0, ATTN_NORM, 0, 90);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_SHORT(g_iFricGrenFlare);
	WRITE_BYTE(25); // scale * 10
	WRITE_BYTE(35); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();

	Vector vecEnd = vecSrc + Vector(0, 0, flRadius / 0.2);
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(vecEnd.x);
	WRITE_COORD(vecEnd.y);
	WRITE_COORD(vecEnd.z);
	WRITE_SHORT(g_iFricGrenRing);
	WRITE_BYTE(0);					// startframe
	WRITE_BYTE(0);					// framerate
	WRITE_BYTE((int)(10 * 0.2f)); // life: 2
	WRITE_BYTE(10);					// width
	WRITE_BYTE(0);					// noise
	WRITE_BYTE((int)vecColor.x);
	WRITE_BYTE((int)vecColor.y);
	WRITE_BYTE((int)vecColor.z);
	WRITE_BYTE(240); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	SetThink(&CBubleGrenade::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->effects |= EF_NODRAW;
}

// orig 0x1a4a40. CGrenade::TumbleThink without the danger sound, glowing pink.
void CBubleGrenade::PreDetThink()
{
	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	pev->renderfx = kRenderFxGlowShell;
	pev->renderamt = 32;
	pev->rendercolor = Vector(255, 128, 255);

	if (pev->dmgtime <= gpGlobals->time)
		SetThink(&CBubleGrenade::SlippyGoBoom);

	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->velocity.z += 8;
		pev->framerate = 0.2;
	}
}

// orig 0x1a4b90. CGrenade::BounceTouch as RC's SDK had it, with less ground friction (0.97).
void CBubleGrenade::SlippyTouch(CBaseEntity* pOther)
{
	// don't hit the guy that launched this grenade
	if (pOther->edict() == pev->owner)
		return;

	// only do damage if we're moving fairly fast
	if (m_flNextAttack < gpGlobals->time && pev->velocity.Length() > 100)
	{
		entvars_t* pevOwner = VARS(pev->owner);
		if (pevOwner)
		{
			TraceResult tr = UTIL_GetGlobalTrace();
			ClearMultiDamage();
			pOther->TraceAttack(pevOwner, 1, gpGlobals->v_forward, &tr, DMG_CLUB);
			ApplyMultiDamage(pev, pevOwner);
		}
		m_flNextAttack = gpGlobals->time + 1.0; // debounce
	}

	Vector vecTestVelocity;
	vecTestVelocity = pev->velocity;
	vecTestVelocity.z *= 0.45;

	if (!m_fRegisteredSound && vecTestVelocity.Length() <= 60)
	{
		// register a radius louder than the explosion, so we make sure everyone gets out of the way
		CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, pev->dmg / 0.4, 0.3);
		m_fRegisteredSound = true;
	}

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.97;

		pev->sequence = RANDOM_LONG(1, 1);
	}
	else
	{
		// play bounce sound
		BounceSound();
	}
	pev->framerate = pev->velocity.Length() / 200.0;
	if (pev->framerate > 1.0)
		pev->framerate = 1;
	else if (pev->framerate < 0.5)
		pev->framerate = 0;
}

//=========================================================
// CImplosionGrenade: the gravity grenade. Stages (m_iDetStage): 0 thrown, 1 rising, 2 hanging
// and sucking; Suck's stages 3 and 4 (a looping egon sound) are never reached (original).
//=========================================================
class CImplosionGrenade : public CGrenade
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce) override;
	int GetRCCaps() override { return 0x60800000; } // orig 0x1a8180
	virtual bool ProMagEffected(CBaseEntity* pOther);

	void EXPORT DamageTouch(CBaseEntity* pOther);
	void EXPORT FinalBoom();
	void DoRing(Vector vecAng);
	void DoEffects();
	void EXPORT Suck();
	void InitBeams();
	void EXPORT PreImplo();
	void EXPORT PreDetThink();
	void EXPORT StickyTouch(CBaseEntity* pOther);
	static void ShootImpTimed(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float time);

	int m_iDetStage;			   // orig +0x280
	float m_flDetTime;			   // orig +0x284: the explosion
	float m_flSpitTime;			   // orig +0x288: the next flying_expel
	float m_flRingRad;			   // orig +0x28c
	float m_flShakeTime;		   // orig +0x290
	float m_flBeamTime;			   // orig +0x294
	float m_flSuckRad;			   // orig +0x298
	float m_flSuckVel;			   // orig +0x29c: saved, never read
	Vector vecRingAng;			   // orig +0x2a0
	CBaseEntity* m_pSpot;		   // orig +0x2ac: saved, never set
	CBaseEntity* m_pRingBeams[8]; // orig +0x2b0: saved, never set (DoRing draws nothing)
};

LINK_ENTITY_TO_CLASS(impgren, CImplosionGrenade); // orig 0x1a4e70

TYPEDESCRIPTION CImplosionGrenade::m_SaveData[] =
	{
		DEFINE_FIELD(CImplosionGrenade, m_iDetStage, FIELD_INTEGER),
		DEFINE_FIELD(CImplosionGrenade, m_flDetTime, FIELD_TIME),
		DEFINE_FIELD(CImplosionGrenade, m_flSpitTime, FIELD_TIME),
		DEFINE_FIELD(CImplosionGrenade, m_flRingRad, FIELD_FLOAT),
		DEFINE_FIELD(CImplosionGrenade, m_flShakeTime, FIELD_TIME),
		DEFINE_FIELD(CImplosionGrenade, m_flBeamTime, FIELD_TIME),
		DEFINE_FIELD(CImplosionGrenade, m_flSuckRad, FIELD_FLOAT),
		DEFINE_FIELD(CImplosionGrenade, m_flSuckVel, FIELD_FLOAT),
		DEFINE_FIELD(CImplosionGrenade, vecRingAng, FIELD_VECTOR),
		DEFINE_FIELD(CImplosionGrenade, m_pSpot, FIELD_CLASSPTR),
		DEFINE_ARRAY(CImplosionGrenade, m_pRingBeams, FIELD_CLASSPTR, 8),
};

IMPLEMENT_SAVERESTORE(CImplosionGrenade, CGrenade); // orig 0x1a4ea0, 0x1a4ef0

// orig 0x1a4f40. Touching the hanging grenade gibs.
void CImplosionGrenade::DamageTouch(CBaseEntity* pOther)
{
	if (!pev->owner || pev->owner->free || !pev->owner->pvPrivateData)
		pOther->TakeDamage(pev, pev, 9999, DMG_ALWAYSGIB);
	else
		pOther->TakeDamage(pev, VARS(pev->owner), 9999, DMG_ALWAYSGIB);
}

// orig 0x1a4fb0
void CImplosionGrenade::FinalBoom()
{
	STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/egon_run3.wav");
	STOP_SOUND(ENT(pev), CHAN_BODY, "RcbAddtions/grav_greande.wav");
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "common/null.wav", 1.0, ATTN_NORM);
	EMIT_SOUND(ENT(pev), CHAN_BODY, "common/null.wav", 1.0, ATTN_NORM);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(ImpInitExlpo);
	WRITE_BYTE(50); // scale * 10
	WRITE_BYTE(15); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMTORUS);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 2560);
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(0);	 // framerate
	WRITE_BYTE(2);	 // life
	WRITE_BYTE(12);	 // width
	WRITE_BYTE(10);	 // noise
	WRITE_BYTE(180); // r
	WRITE_BYTE(180); // g
	WRITE_BYTE(255); // b
	WRITE_BYTE(128); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, -32), ignore_monsters, ENT(pev), &tr);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(tr.vecEndPos.x);
	WRITE_COORD(tr.vecEndPos.y);
	WRITE_COORD(tr.vecEndPos.z);
	WRITE_COORD(tr.vecEndPos.x + tr.vecPlaneNormal.x);
	WRITE_COORD(tr.vecEndPos.y + tr.vecPlaneNormal.y);
	WRITE_COORD(tr.vecEndPos.z + tr.vecPlaneNormal.z);
	WRITE_SHORT(ImpFlare);
	WRITE_BYTE(10); // count
	WRITE_BYTE(5);	// life
	WRITE_BYTE(3);	// scale
	WRITE_BYTE(20); // velocity
	WRITE_BYTE(20); // randomness
	MESSAGE_END();

	entvars_t* pevOwner;
	if (!pev->owner || pev->owner->free || !pev->owner->pvPrivateData)
		pevOwner = pev;
	else
		pevOwner = VARS(pev->owner);

	pev->owner = NULL; // can't traceline attack owner if this is set
	RadiusDamage(pev, pevOwner, pev->dmg, CLASS_NONE, DMG_BLAST);
	pev->owner = pevOwner->pContainingEntity;

	if (RANDOM_FLOAT(0, 1) < 0.5)
		UTIL_DecalTrace(&tr, DECAL_SCORCH1);
	else
		UTIL_DecalTrace(&tr, DECAL_SCORCH2);

	SetThink(&CImplosionGrenade::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x1a5580. Works out the eight corners of a ring of radius m_flRingRad around the grenade
// and draws nothing: the original's loop over them is empty (the ring beams were taken out).
void CImplosionGrenade::DoRing(Vector vecAng)
{
	Vector vecPoints[9];

	UTIL_MakeVectors(vecAng);
	vecPoints[0] = gpGlobals->v_forward;
	vecPoints[1] = gpGlobals->v_forward * 0.5 + gpGlobals->v_right * 0.5;
	vecPoints[2] = gpGlobals->v_right;
	vecPoints[3] = -gpGlobals->v_forward * 0.5 + gpGlobals->v_right * 0.5;
	vecPoints[4] = -gpGlobals->v_forward;
	vecPoints[5] = -gpGlobals->v_forward * 0.5 + -gpGlobals->v_right * 0.5;
	vecPoints[6] = -gpGlobals->v_right;
	vecPoints[7] = gpGlobals->v_forward * 0.5 + -gpGlobals->v_right * 0.5;
	vecPoints[8] = gpGlobals->v_forward;

	for (int i = 0; i < 9; i++)
	{
		vecPoints[i] = vecPoints[i].Normalize() * m_flRingRad;
		vecPoints[i] = vecPoints[i] + Center();
	}

	for (int i = 0; i < 8; i++)
	{
		// empty in the original
	}
}

// orig 0x1a5ad0. While hanging: spit a flying_expel (a rocket model with a trail, removed on
// touch) every 0.1-0.4 s, up to 26 in the world and not with sv_lowlag 1; shake clients within
// 1024 every 0.5 s; and turn the (invisible) ring every 0.1 s.
void CImplosionGrenade::DoEffects()
{
	if (m_flSpitTime < gpGlobals->time && CountEntsInWorld("flying_expel") < 26 && CVAR_GET_FLOAT("sv_lowlag") != 1.0)
	{
		CBaseEntity* pExpel = GetClassPtr((CBaseEntity*)NULL);
		pExpel->Spawn();
		pExpel->pev->classname = MAKE_STRING("flying_expel");
		pExpel->pev->movetype = MOVETYPE_TOSS;
		pExpel->pev->solid = SOLID_BBOX;
		pExpel->pev->renderamt = 0;
		pExpel->pev->rendermode = kRenderTransColor;
		pExpel->SetTouch(&CBaseEntity::RemoveTouch);
		pExpel->SetThink(&CBaseEntity::SUB_Remove);
		pExpel->pev->nextthink = gpGlobals->time + 15.0;
		SET_MODEL(ENT(pExpel->pev), "models/rpgrocket.mdl");
		UTIL_SetOrigin(pExpel->pev, pev->origin);
		UTIL_SetSize(pExpel->pev, g_vecZero, g_vecZero);
		pExpel->pev->owner = edict();

		float x = RANDOM_FLOAT(-32, 32);
		float y = RANDOM_FLOAT(-32, 32);
		float z = RANDOM_FLOAT(64, 72);
		Vector vecVel = Vector(x, y, z).Normalize() * 600;
		if (vecVel != Vector(0, 0, 0))
			pExpel->pev->velocity = vecVel;

		MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(TE_BEAMFOLLOW);
		WRITE_SHORT(pExpel->entindex()); // entity
		WRITE_SHORT(g_sModelIndexLaser); // model
		WRITE_BYTE(5);					 // life
		WRITE_BYTE(2);					 // width
		WRITE_BYTE(200);				 // r, g, b
		WRITE_BYTE(200);				 // r, g, b
		WRITE_BYTE(255);				 // r, g, b
		WRITE_BYTE(128);				 // brightness
		MESSAGE_END();

		m_flSpitTime = RANDOM_FLOAT(0.1, 0.4) + gpGlobals->time;
	}

	if (m_flShakeTime < gpGlobals->time)
	{
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
			if (!pPlayer || !pPlayer->IsNetClient())
				continue;
			if ((pPlayer->pev->origin - pev->origin).Length() > 1024)
				continue;

			// VERIFY: the amplitude grows with distance (original)
			float flFrac = (pPlayer->pev->origin - pev->origin).Length() * (1.0 / 1024);
			unsigned short duration = FixedUnsigned16(0.5, 1 << 12);
			unsigned short frequency = FixedUnsigned16(25, 1 << 8);
			unsigned short amplitude = FixedUnsigned16(flFrac * 200, 1 << 12);

			MESSAGE_BEGIN(MSG_ONE, gmsgShake, NULL, pPlayer->edict());
			WRITE_SHORT(amplitude);
			WRITE_SHORT(duration);
			WRITE_SHORT(frequency);
			MESSAGE_END();
		}
		m_flShakeTime = gpGlobals->time + 0.5;
	}

	if (m_flBeamTime < gpGlobals->time)
	{
		m_flRingRad = RANDOM_FLOAT(-7, 7) + m_flRingRad;
		if (m_flRingRad < 42)
			m_flRingRad = 42;
		if (m_flRingRad > 112)
			m_flRingRad = 112;

		float x = RANDOM_FLOAT(10, 20);
		float y = RANDOM_FLOAT(10, 20);
		vecRingAng = vecRingAng + Vector(x, y, 0);
		if (vecRingAng.x > 360)
			vecRingAng.x -= 360;
		if (vecRingAng.y > 360)
			vecRingAng.y -= 360;
		while (vecRingAng.x < 0)
			vecRingAng.x += 360;
		while (vecRingAng.x > 360)
			vecRingAng.x -= 360;
		while (vecRingAng.y < 0)
			vecRingAng.y += 360;
		while (vecRingAng.y > 360)
			vecRingAng.y -= 360;
		while (vecRingAng.z < 0)
			vecRingAng.z += 360;
		while (vecRingAng.z > 360)
			vecRingAng.z -= 360;

		DoRing(vecRingAng);
		m_flBeamTime = gpGlobals->time + 0.1;
	}
}

// orig 0x1a63a0. Every 0.01 s: pull everything with RC cap 4 within m_flSuckRad (and not frozen)
// toward the grenade, harder when nearer, and lift it; within 48 units it touches the grenade
// (DamageTouch). Explodes at m_flDetTime.
void CImplosionGrenade::Suck()
{
	pev->nextthink = gpGlobals->time + 0.01;

	if (pev->dmgtime <= gpGlobals->time && m_iDetStage == 3)
	{
		m_iDetStage = 4;
	}
	else if (m_iDetStage == 4 && pev->dmgtime <= gpGlobals->time)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/egon_run3.wav", 1.0, ATTN_NORM);
		pev->dmgtime = gpGlobals->time + 2.2;
	}

	if (m_flDetTime <= gpGlobals->time)
		SetThink(&CImplosionGrenade::FinalBoom);

	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, m_flSuckRad)) != NULL)
	{
		int iCap = 4; // VERIFY: an RC cap bit, name unknown
		if (!pEntity->HasRCCap(iCap))
			continue;
		if ((pEntity->pev->flags & FL_FROZEN) != 0)
			continue;

		// the original makes vectors from the flat direction first, then at once from the full one
		Vector vecFlat = pEntity->pev->origin - pev->origin;
		vecFlat.z = 0;
		UTIL_MakeVectors(UTIL_VecToAngles(vecFlat.Normalize()));
		UTIL_MakeVectors(UTIL_VecToAngles((pEntity->pev->origin - pev->origin).Normalize()));

		float flDist = (pEntity->pev->origin - pev->origin).Length();
		if (flDist < 48)
		{
			DamageTouch(pEntity);
			continue;
		}

		float flPull = m_flSuckRad / flDist * 22;
		if (!(flPull < 2000))
			flPull = 2000;
		pEntity->pev->velocity = pEntity->pev->velocity - gpGlobals->v_forward * flPull;

		if (fabs(pEntity->pev->origin.z - pev->origin.z) > 32)
		{
			float flLift = m_flSuckRad / (pev->origin.z - pEntity->pev->origin.z) * 22;
			if (flLift < 2000)
				pEntity->pev->velocity.z = flLift + pEntity->pev->velocity.z;
			else
				pEntity->pev->velocity.z = pEntity->pev->velocity.z + 2000;
		}

		if ((pEntity->pev->flags & FL_ONGROUND) != 0 || pEntity->pev->velocity.z == 0 || fabs(pev->origin.z - pEntity->pev->origin.z) <= 32)
		{
			pEntity->pev->flags &= ~FL_ONGROUND;
			pEntity->pev->velocity.z += 200;
		}

		if (pEntity->pev->velocity.Length() > 2000)
			pEntity->pev->velocity = pEntity->pev->velocity.Normalize() * 2000;
	}

	DoEffects();
}

// orig 0x1a6990. A short-lived beam straight up and an explosion sprite when it starts hanging.
void CImplosionGrenade::InitBeams()
{
	CBeam* pBeam = CBeam::BeamCreate(g_pModelNameLaser, 60);
	pBeam->SetBrightness(255);
	pBeam->SetColor(128, 128, 255);
	pBeam->SetNoise(7);
	pBeam->SetScrollRate(128);
	pBeam->SetStartPos(pev->origin);
	pBeam->SetEndPos(pev->origin + Vector(0, 0, 300));
	pBeam->SetThink(&CBeam::SUB_Remove);
	pBeam->pev->nextthink = gpGlobals->time + 5.0;
	pBeam->SetFlags(BEAM_FSHADEOUT);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(Center().x);
	WRITE_COORD(Center().y);
	WRITE_COORD(Center().z);
	WRITE_SHORT(ImpInitExlpo);
	WRITE_BYTE(100); // scale * 10
	WRITE_BYTE(20);	 // framerate
	WRITE_BYTE(TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();
}

// orig 0x1a6bd0. Stage 1 on the ground: rise for 0.4 s, spinning. Stage 2 (or 1 in the air):
// stop, glow, and start sucking for 5 s.
void CImplosionGrenade::PreImplo()
{
	if ((pev->flags & FL_ONGROUND) == 0 && m_iDetStage == 1)
		m_iDetStage = 2;

	if (m_iDetStage == 1)
	{
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = Vector(0, 0, 50);
		pev->nextthink = gpGlobals->time + 0.4;
		float x = RANDOM_FLOAT(-1000, 1000);
		float y = RANDOM_FLOAT(-1000, 1000);
		float z = RANDOM_FLOAT(-1000, 1000);
		pev->avelocity = Vector(x, y, z);
		m_iDetStage++;
		return;
	}

	if (m_iDetStage == 2)
	{
		pev->movetype = MOVETYPE_NONE;
		pev->solid = SOLID_TRIGGER;
		pev->velocity = Vector(0, 0, 0);
		UTIL_SetSize(pev, Vector(-10, -10, -10), Vector(10, 10, 10));
		SetTouch(&CImplosionGrenade::DamageTouch);
		pev->effects |= EF_LIGHT;
		pev->effects |= EF_BRIGHTLIGHT;
		m_flBeamTime = gpGlobals->time;
		InitBeams();
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/egon_windup2.wav", 1.0, ATTN_NORM);
		EMIT_SOUND(ENT(pev), CHAN_BODY, "RcbAddtions/grav_greande.wav", 1.0, ATTN_NORM);
		pev->nextthink = gpGlobals->time + 0.1;
		pev->dmgtime = gpGlobals->time + 3.8;
		m_flDetTime = gpGlobals->time + 5.0;
		m_flSpitTime = gpGlobals->time + 0.2;
		m_flShakeTime = gpGlobals->time + 0.2;
		SetThink(&CImplosionGrenade::Suck);
		pev->nextthink = gpGlobals->time + 0.1;

		float x = RANDOM_FLOAT(0, 360);
		float y = RANDOM_FLOAT(0, 360);
		vecRingAng = Vector(x, y, 0);
		while (vecRingAng.x < 0)
			vecRingAng.x += 360;
		while (vecRingAng.x > 360)
			vecRingAng.x -= 360;
		while (vecRingAng.y < 0)
			vecRingAng.y += 360;
		while (vecRingAng.y > 360)
			vecRingAng.y -= 360;
		while (vecRingAng.z < 0)
			vecRingAng.z += 360;
		while (vecRingAng.z > 360)
			vecRingAng.z -= 360;

		m_flRingRad = 72;
		m_flSuckRad = PREFS_KeyLoopUpFloat("GRENADES", "GravGrenSuckRad", 512.0);
		m_flSuckVel = PREFS_KeyLoopUpFloat("GRENADES", "GravGrenSuckVel", 800.0);
	}
	else
	{
		m_iDetStage = 1;
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

// orig 0x1a7090. CGrenade::TumbleThink without the danger sound.
void CImplosionGrenade::PreDetThink()
{
	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime <= gpGlobals->time)
	{
		m_iDetStage = 1;
		SetThink(&CImplosionGrenade::PreImplo);
	}

	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}

// orig 0x1a7180. Unused: nothing sets it as a touch. Hits the living, else sticks where it lands.
void CImplosionGrenade::StickyTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;

	if (pOther->IsAlive() && m_flNextAttack < gpGlobals->time && pev->velocity.Length() > 100)
	{
		entvars_t* pevOwner = VARS(pev->owner);
		if (pev->owner && pevOwner)
		{
			TraceResult tr = UTIL_GetGlobalTrace();
			ClearMultiDamage();
			pOther->TraceAttack(pevOwner, 1, gpGlobals->v_forward, &tr, DMG_CLUB);
			ApplyMultiDamage(pev, pevOwner);
		}
		m_flNextAttack = gpGlobals->time + 1.0; // debounce
		return;
	}

	CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, pev->dmg / 0.4, 0.3);
	BounceSound();
	pev->movetype = MOVETYPE_FLY;
	pev->velocity = g_vecZero;
}

// orig 0x1a7350. CGrenade::ShootTimed with stage 0 and FinalBlastDamage.
void CImplosionGrenade::ShootImpTimed(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float time)
{
	CImplosionGrenade* pGrenade = GetClassPtr((CImplosionGrenade*)NULL);
	pGrenade->Spawn();
	UTIL_SetOrigin(pGrenade->pev, vecStart);
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles(pGrenade->pev->velocity);
	pGrenade->pev->owner = ENT(pevOwner);

	pGrenade->SetTouch(&CImplosionGrenade::BounceTouch); // Bounce if touched

	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink(&CImplosionGrenade::PreDetThink);
	pGrenade->pev->nextthink = gpGlobals->time + 0.1;
	if (time < 0.1)
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = Vector(0, 0, 0);
	}

	pGrenade->pev->sequence = RANDOM_LONG(3, 6);
	pGrenade->pev->framerate = 1.0;

	pGrenade->pev->gravity = 0.5;
	pGrenade->pev->friction = 0.8;
	pGrenade->m_iDetStage = 0;

	SET_MODEL(ENT(pGrenade->pev), "models/w_grenade.mdl");
	pGrenade->pev->dmg = PREFS_KeyLoopUpFloat("GRENADES", "FinalBlastDamage", 120.0);
}

// orig 0x1a8040. As CFreezeGreande's (weapons.h).
void CImplosionGrenade::ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce)
{
	if (pMagnet->Center().z + pMagnet->pev->mins.z < pev->origin.z)
	{
		pev->velocity = vecForce * pev->velocity.Length();
	}
	else
	{
		float flSpeed = pev->velocity.Length2D();
		pev->velocity.x = vecForce.x * flSpeed;
		pev->velocity.y = vecForce.y * flSpeed;
	}
}

// orig 0x1a8140. Only while thrown, and not by its own thrower's magnet.
bool CImplosionGrenade::ProMagEffected(CBaseEntity* pOther)
{
	return pev->owner && pOther->edict() != pev->owner && m_iDetStage == 0;
}

#endif

LINK_ENTITY_TO_CLASS(weapon_handgrenade, CHandGrenade); // orig 0x1a7520

// orig 0x1a7550
void CHandGrenade::Spawn()
{
	Precache();
	m_iId = WEAPON_HANDGRENADE;
	SET_MODEL(ENT(pev), "models/w_grenade.mdl");

#ifndef CLIENT_DLL
	pev->dmg = gSkillData.plrDmgHandGrenade;
#endif

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("GRENADES", "PickupAmmo", 1, 0, 250);

	FallInit(); // get ready to fall down.
}

// orig 0x1a75e0. The bubble grenade's debris/beamstart sounds are not precached here.
void CHandGrenade::Precache()
{
	PRECACHE_MODEL("models/w_grenade.mdl");
	PRECACHE_MODEL("models/v_grenade.mdl");
	PRECACHE_MODEL("models/p_grenade.mdl");
	PRECACHE_MODEL("models/rpgrocket.mdl");
	ImpFlare = PRECACHE_MODEL("sprites/blueflare1.spr");
	ImpInitExlpo = PRECACHE_MODEL("sprites/xflare1.spr");
	g_iFricGrenFlare = PRECACHE_MODEL("sprites/c-tele1.spr");
	g_iFricGrenRing = PRECACHE_MODEL("sprites/shockwave.spr");
	PRECACHE_SOUND("weapons/egon_windup2.wav");
	PRECACHE_SOUND("weapons/egon_run3.wav");
	PRECACHE_SOUND("RcbAddtions/grav_greande.wav");
}

// orig 0x1a76e0
bool CHandGrenade::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Hand Grenade";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("GRENADES", "MaxAmmo", 5, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_HANDGRENADE;
	p->iWeight = 8;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return true;
}

// orig 0x1a7790
bool CHandGrenade::Deploy()
{
	m_flReleaseThrow = -1;
	return DefaultDeploy("models/v_grenade.mdl", "models/p_grenade.mdl", HANDGRENADE_DRAW, "crowbar");
}

// orig 0x1a77e0
bool CHandGrenade::CanHolster()
{
	// can only holster hand grenades when not primed!
	return (m_flStartThrow == 0);
}

// orig 0x1a7810. Does not clear m_flStartThrow (halflife-updated does), so a grenade primed when
// the weapon is put away is thrown on the next deploy's idle, as in the original.
void CHandGrenade::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if (0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		SendWeaponAnim(HANDGRENADE_HOLSTER);
	}
	else
	{
		// no more grenades!
		m_pPlayer->ClearWeaponBit(m_iId);
		SetThink(&CHandGrenade::DestroyItem);
		pev->nextthink = gpGlobals->time + 0.1;
	}

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

// orig 0x1a78e0. Pull the pin for a gravity grenade, while fewer than 6 grenades are in the world.
void CHandGrenade::PrimaryAttack()
{
	if (0 == m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0 && CountEntsInWorld("grenade") < 6)
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;
		m_bBubble = false;

		SendWeaponAnim(HANDGRENADE_PINPULL);
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;
	}
}

// orig 0x1a7990. Pull the pin for a bubble grenade.
void CHandGrenade::SecondaryAttack()
{
	if (0 == m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0 && CountEntsInWorld("grenade") < 6)
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;
		m_bBubble = true;

		SendWeaponAnim(HANDGRENADE_PINPULL);
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;
	}
}

// orig 0x1a7a40. The throw goes off 5 s after the pin is pulled (Valve's: 3), at ThrowScale per
// degree up to ThrowMax.
void CHandGrenade::WeaponIdle()
{
	if (m_flReleaseThrow == 0)
		m_flReleaseThrow = gpGlobals->time;

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (0 != m_flStartThrow)
	{
		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if (angThrow.x < 0)
			angThrow.x = -10 + angThrow.x * ((90 - 10) / 90.0);
		else
			angThrow.x = -10 + angThrow.x * ((90 + 10) / 90.0);

		float flVel = (90 - angThrow.x) * PREFS_KeyLoopUpFloat("GRENADES", "ThrowScale", 4.0);
		if (flVel > PREFS_KeyLoopUpFloat("GRENADES", "ThrowMax", 500.0))
			flVel = PREFS_KeyLoopUpFloat("GRENADES", "ThrowMax", 500.0);

		UTIL_MakeVectors(angThrow);

		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;

		Vector vecThrow = gpGlobals->v_forward * flVel + m_pPlayer->pev->velocity;

		// alway explode 5 seconds after the pin was pulled
		float time = m_flStartThrow - gpGlobals->time + 5.0;
		if (time < 0)
			time = 0;

#ifndef CLIENT_DLL
		if (!m_bBubble)
			CImplosionGrenade::ShootImpTimed(m_pPlayer->pev, vecSrc, vecThrow, time);
		else
			CBubleGrenade::ShootBubTimed(m_pPlayer->pev, vecSrc, vecThrow, time);
#endif

		if (flVel < 500)
		{
			SendWeaponAnim(HANDGRENADE_THROW1);
		}
		else if (flVel < 1000)
		{
			SendWeaponAnim(HANDGRENADE_THROW2);
		}
		else
		{
			SendWeaponAnim(HANDGRENADE_THROW3);
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		m_flStartThrow = 0;
		m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("GRENADES", "RefireRate", 0.5) * m_pPlayer->m_fCurseRefireScale;
		m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("GRENADES", "RefireRate", 0.5) * m_pPlayer->m_fCurseRefireScale;
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			// just threw last grenade
			// set attack times in the future, and weapon idle in the future so we can see the whole throw
			// animation, weapon idle will automatically retire the weapon for us.
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + 0.5; // ensure that the animation can finish playing
		}
		return;
	}
	else if (m_flReleaseThrow > 0)
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if (0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			SendWeaponAnim(HANDGRENADE_DRAW);
		}
		else
		{
			RetireWeapon();
			return;
		}

		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);
		m_flReleaseThrow = -1;
		return;
	}

	if (0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = RANDOM_FLOAT(0, 1);
		if (flRand > 0.75)
		{
			iAnim = HANDGRENADE_FIDGET;
			m_flTimeWeaponIdle = gpGlobals->time + 75.0 / 30.0;
		}
		else
		{
			iAnim = HANDGRENADE_IDLE;
			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15); // how long till we do this again.
		}

		SendWeaponAnim(iAnim);
	}
}
