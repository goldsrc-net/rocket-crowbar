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
*	Rocket Crowbar 1.9: the satchel charge is a roach bomb. The thrown "satchel" is a roach that
*	runs at the nearest living thing; the radio arms it, and it blows up three seconds later
*	with a rising squeak. Also the killer rat (killerrat), a rat its owner looks through and
*	can blow up. Numbers come from rc.ini's [SATCHEL] heading. The controls are the 2001 SDK's
*	(primary throws the first and then detonates, secondary throws more).
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "Preff.h"

int Roach_Ball_Model; // sprites/redflare1.spr, the sparks of a roach bomb going off
int g_iRatSPR;		  // sprites/shockwave.spr, the killer rat's beam

//=========================================================
// CKillerRat
//=========================================================
class CKillerRat : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override;
	void Killed(entvars_t* pevAttacker, int iGib) override;

	virtual bool IsGravGreandeMoveable();

	void EXPORT RatThink();

	edict_t* m_pRatOwner;  // orig +0x27c, the player looking through it. VERIFY name
	float m_flBeamStart;   // orig +0x280, when attack was pressed, -1 before. VERIFY name
	float m_flNextBeam;	   // orig +0x284. VERIFY name
};

LINK_ENTITY_TO_CLASS(killerrat, CKillerRat);

// orig 0x201cc0: takes its owner with it.
void CKillerRat::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->owner || (pev->owner = m_pRatOwner) != NULL)
	{
		CBasePlayer* pPlayer = GetClassPtr((CBasePlayer*)VARS(pev->owner));
		pPlayer->TakeDamage(pev, pevAttacker, 9999, DMG_NEVERGIB);
	}

	// VERIFY: the original calls CGrenade::Explode on itself (GetClassPtr returns this rat), a
	// CBaseMonster; Explode and Smoke only use CBaseEntity's members
	GetClassPtr((CGrenade*)pev)->Explode(pev->origin, g_vecZero);
}

// orig 0x201d90
void CKillerRat::RatThink()
{
	pev->nextthink = gpGlobals->time + 0.01;

	if (pev->owner)
	{
		m_pRatOwner = pev->owner;
		pev->owner = NULL;
	}

	if (!m_pRatOwner)
	{
		SUB_Remove();
		return;
	}

	if (m_pRatOwner->v.health <= 0)
	{
		GetClassPtr((CGrenade*)pev)->Explode(pev->origin, g_vecZero);
		return;
	}

	if ((pev->flags & FL_ONGROUND) == 0)
	{
		pev->angles = UTIL_VecToAngles(pev->velocity);
	}
	else
	{
		pev->angles = m_pRatOwner->v.v_angle;
		pev->angles.x = 0;
		pev->angles.z = 0;
	}

	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(m_pRatOwner);
	pPlayer->m_flNextAttack = gpGlobals->time + 0.5; // the player can't fire while it lives

	if ((pPlayer->m_afButtonPressed & IN_ATTACK) != 0)
	{
		m_flBeamStart = gpGlobals->time;
		m_flNextBeam = gpGlobals->time + RANDOM_FLOAT(0.2, 0.5);
	}
	else if ((pPlayer->m_afButtonPressed & IN_ATTACK2) != 0 && m_flBeamStart == -1)
	{
		SET_VIEW(m_pRatOwner, m_pRatOwner);
		GetClassPtr((CGrenade*)pev)->Explode(pev->origin, g_vecZero);
	}

	if (m_flBeamStart != -1 && m_flNextBeam < gpGlobals->time)
	{
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_BEAMCYLINDER);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + 8);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + 8 + 1280);
		WRITE_SHORT(g_iRatSPR);
		WRITE_BYTE(0);	 // startframe
		WRITE_BYTE(0);	 // framerate
		WRITE_BYTE(1);	 // life
		WRITE_BYTE(4);	 // width
		WRITE_BYTE(0);	 // noise
		WRITE_BYTE(128); // r, g, b
		WRITE_BYTE(128);
		WRITE_BYTE(255);
		WRITE_BYTE(255); // brightness
		WRITE_BYTE(0);	 // speed
		MESSAGE_END();

		m_flNextBeam = gpGlobals->time + RANDOM_FLOAT(0.2, 0.4);
	}
}

// orig 0x202200
int CKillerRat::Classify()
{
	return CLASS_INSECT;
}

// orig 0x202210
void CKillerRat::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/bigrat.mdl");
	UTIL_SetSize(pev, Vector(-6, -8, -6), Vector(6, 8, 6));
	UTIL_SetOrigin(pev, pev->origin);

	pev->classname = MAKE_STRING("monster_satchel");
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->health = 10;
	pev->flags |= FL_MONSTER;
	pev->takedamage = DAMAGE_AIM;
	pev->dmg = 100;

	SetTouch(NULL);
	SetThink(&CKillerRat::RatThink);
	pev->nextthink = gpGlobals->time + 0.2;

	m_pRatOwner = pev->owner;
	m_flBeamStart = -1;
	if (pev->owner)
		SET_VIEW(pev->owner, edict());

	m_flGroundSpeed = 50;
	pev->sequence = 4;
	ResetSequenceInfo();
}

// orig 0x2023a0
void CKillerRat::Precache()
{
	PRECACHE_MODEL("models/bigrat.mdl");
}

// orig 0x204040
bool CKillerRat::IsGravGreandeMoveable()
{
	return true;
}

//=========================================================
// CSatchelCharge: the thrown roach
//=========================================================
class CSatchelCharge : public CGrenade
{
	void Spawn() override;
	void Precache() override;
	void BounceSound() override;
	int Classify() override;
	bool IsAlive() override;
	int GetRCCaps() override;

	void EXPORT SatchelSlide(CBaseEntity* pOther);
	void EXPORT SatchelThink();
	void EXPORT AtivateUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

public:
	void Deactivate();

	float m_flDetonateTime;	  // orig +0x280, armed: when it goes off. VERIFY name
	float m_flNextWalkSound;  // orig +0x284. VERIFY name
	int m_iArmed;			  // orig +0x288, 1 once the radio set it off. VERIFY name
};
LINK_ENTITY_TO_CLASS(monster_satchel, CSatchelCharge);

//=========================================================
// Deactivate - do whatever it is we do to an orphaned
// satchel when we don't want it in the world anymore.
//=========================================================
// orig 0x202400
void CSatchelCharge::Deactivate()
{
	pev->solid = SOLID_NOT;
	UTIL_Remove(this);
}

// orig 0x202430
int CSatchelCharge::Classify()
{
	return CLASS_PLAYER_BIOWEAPON;
}

// orig 0x202440
void CSatchelCharge::Spawn()
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/roach.mdl");
	UTIL_SetSize(pev, Vector(-1, -1, -1), Vector(1, 1, 1));
	UTIL_SetOrigin(pev, pev->origin);

	SetTouch(&CSatchelCharge::SatchelSlide);
	SetUse(&CSatchelCharge::AtivateUse);
	SetThink(&CSatchelCharge::SatchelThink);
	pev->nextthink = gpGlobals->time + 0.5;

	pev->gravity = PREFS_KeyLoopUpFloat("SATCHEL", "RoachGravity", 0.5);
	pev->friction = PREFS_KeyLoopUpFloat("SATCHEL", "RoachFriction", 0.8);

	pev->dmg = PREFS_KeyLoopUpFloat("SATCHEL", "RoachDamage", 300.0);
	ResetSequenceInfo();
	pev->sequence = 0;
}

// orig 0x2025b0
void CSatchelCharge::SatchelSlide(CBaseEntity* pOther)
{
	// don't hit the guy that launched this grenade
	if (pOther->edict() == pev->owner)
		return;

	// pev->avelocity = Vector (300, 300, 300);
	pev->gravity = 1; // normal gravity now

	// HACKHACK - On ground isn't always set, so look for ground underneath
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin - Vector(0, 0, 10), ignore_monsters, edict(), &tr);

	if (tr.flFraction < 1.0)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.95;
		pev->avelocity = pev->avelocity * 0.9;
		// play sliding sound, volume based on velocity
	}
	if ((pev->flags & FL_ONGROUND) == 0 && pev->velocity.Length2D() > 10)
	{
		BounceSound();
	}
	StudioFrameAdvance();
}

// orig 0x202710: on the ground the roach runs at the nearest living thing (or circles), and once
// armed squeaks faster and faster until it goes off.
void CSatchelCharge::SatchelThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	if (pev->waterlevel == 3)
	{
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = pev->velocity * 0.8;
		pev->avelocity = pev->avelocity * 0.9;
		pev->velocity.z += 8;
	}
	else if (pev->waterlevel == 0)
	{
		pev->movetype = MOVETYPE_BOUNCE;
	}
	else
	{
		pev->velocity.z -= 8;
	}

	pev->avelocity = g_vecZero;

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		CBaseEntity* pBest = NULL;
		float flBest = 9999;
		float flVelZ = pev->velocity.z;

		if (PREFS_KeyLoopUpBool("SATCHEL", "RoachFollow", true))
		{
			CBaseEntity* pEnt = NULL;
			while ((pEnt = UTIL_FindEntityInSphere(pEnt, pev->origin, PREFS_KeyLoopUpFloat("SATCHEL", "RoachLookRange", 160.0))) != NULL)
			{
				if (!pEnt->IsAlive() || pEnt->pev->takedamage != DAMAGE_AIM || pEnt->IsBSPModel())
					continue;

				// to its feet
				Vector vecFeet = pEnt->pev->origin;
				vecFeet.z += pEnt->pev->mins.z;
				float flDist = (vecFeet - pev->origin).Length();
				if (flDist <= flBest)
				{
					flBest = flDist;
					pBest = pEnt;
				}
			}
		}

		if (pBest)
		{
			Vector vecDir = (pBest->pev->origin - pev->origin).Normalize();
			if (m_iArmed != 1)
				pev->velocity = vecDir * PREFS_KeyLoopUpFloat("SATCHEL", "RoachFollowVel", 200.0);
			else
				pev->velocity = vecDir * PREFS_KeyLoopUpFloat("SATCHEL", "RoachFollowVel", 200.0) * PREFS_KeyLoopUpFloat("SATCHEL", "RoachOnSpeedFact", 2.0);
		}
		else if (m_iArmed != 1)
		{
			pev->angles.y += PREFS_KeyLoopUpFloat("SATCHEL", "RoachTurnRate", 5.0);
			if (pev->angles.y > 360)
				pev->angles.y -= 360;
			UTIL_MakeVectors(pev->angles);
			pev->velocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("SATCHEL", "RoachTurnVel", 128.0);
		}
		else
		{
			float flRate = PREFS_KeyLoopUpFloat("SATCHEL", "RoachTurnRate", 5.0);
			pev->angles.y += PREFS_KeyLoopUpFloat("SATCHEL", "RoachOnSpeedFact", 2.0) * flRate;
			if (pev->angles.y > 360)
				pev->angles.y -= 360;
			UTIL_MakeVectors(pev->angles);
			Vector vecVel = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("SATCHEL", "RoachTurnVel", 128.0);
			pev->velocity = vecVel * PREFS_KeyLoopUpFloat("SATCHEL", "RoachOnSpeedFact", 2.0);
		}

		pev->velocity.z = flVelZ;
	}

	pev->angles = UTIL_VecToAngles(pev->velocity);
	pev->angles.x = 0;
	pev->angles.z = 0;

	if (m_iArmed != 1)
		return;

	if (m_flNextWalkSound < gpGlobals->time)
	{
		// pitch rises from 100 to 250 over the three seconds
		float flPitch = 100 + 150 * ((gpGlobals->time + 3 - m_flDetonateTime) / 3);
		if (flPitch > 250)
			flPitch = 250;
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "roach/rch_walk.wav", 1.0, ATTN_NORM, 0, (int)flPitch);
		m_flNextWalkSound = gpGlobals->time + 0.3;
	}

	if (m_iArmed == 1 && m_flDetonateTime < gpGlobals->time)
	{
		TraceResult tr;
		UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, -32), ignore_monsters, edict(), &tr);

		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos);
		WRITE_BYTE(TE_SPRITETRAIL);
		WRITE_COORD(tr.vecEndPos.x);
		WRITE_COORD(tr.vecEndPos.y);
		WRITE_COORD(tr.vecEndPos.z);
		WRITE_COORD(tr.vecEndPos.x + tr.vecPlaneNormal.x);
		WRITE_COORD(tr.vecEndPos.y + tr.vecPlaneNormal.y);
		WRITE_COORD(tr.vecEndPos.z + tr.vecPlaneNormal.z);
		WRITE_SHORT(Roach_Ball_Model);
		WRITE_BYTE(20);	 // count
		WRITE_BYTE(25);	 // life * 10
		WRITE_BYTE(3);	 // scale * 10
		WRITE_BYTE(20);	 // velocity along the vector * 10
		WRITE_BYTE(100); // randomness * 10
		MESSAGE_END();

		Explode(&tr, DMG_BLAST);
	}
}

// orig 0x202fb0: the radio's use. Glows red and goes off in three seconds.
void CSatchelCharge::AtivateUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	m_iArmed = 1;
	m_flDetonateTime = gpGlobals->time + 3.0;
	pev->renderamt = 32;
	pev->rendercolor = Vector(255, 0, 0);
	pev->renderfx = kRenderFxGlowShell;
	m_flNextWalkSound = gpGlobals->time;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(10);	 // life
	WRITE_BYTE(1);	 // width
	WRITE_BYTE(255); // r, g, b
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(100); // brightness
	MESSAGE_END();
}

// orig 0x203130
void CSatchelCharge::Precache()
{
	PRECACHE_MODEL("models/grenade.mdl");
	PRECACHE_MODEL("models/roach.mdl");
	PRECACHE_SOUND("weapons/g_bounce1.wav");
	PRECACHE_SOUND("weapons/g_bounce2.wav");
	PRECACHE_SOUND("weapons/g_bounce3.wav");
	PRECACHE_SOUND("roach/rch_walk.wav");
	Roach_Ball_Model = PRECACHE_MODEL("sprites/redflare1.spr");
}

// orig 0x2031d0
void CSatchelCharge::BounceSound()
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "roach/rch_walk.wav", 1, ATTN_NORM);
}

// orig 0x203fd0
bool CSatchelCharge::IsAlive()
{
	return false;
}

// orig 0x203fe0
int CSatchelCharge::GetRCCaps()
{
	return 0x6080001c;
}


//=========================================================
// CSatchel
//=========================================================
LINK_ENTITY_TO_CLASS(weapon_satchel, CSatchel);


//=========================================================
// CALLED THROUGH the newly-touched weapon's instance. The existing player weapon is pOriginal
//=========================================================
// orig 0x2032e0
bool CSatchel::AddDuplicate(CBasePlayerItem* pOriginal)
{
	if (g_pGameRules->IsMultiplayer())
	{
		CSatchel* pSatchel = (CSatchel*)pOriginal;

		// player has some satchels deployed. Refuse to add more, unless rc.ini allows it.
		if (pSatchel->m_chargeReady != 0 && !PREFS_KeyLoopUpBool("SATCHEL", "PickupWhileOut", false))
			return false;
	}

	return CBasePlayerWeapon::AddDuplicate(pOriginal);
}

//=========================================================
//=========================================================
// orig 0x203350. Unlike the SDK's, picks up the radio when the player still has roaches out.
void CSatchel::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	m_chargeReady = 0; // this satchel charge weapon now forgets that any satchels are deployed by it.

	edict_t* pFind = FIND_ENTITY_BY_CLASSNAME(NULL, "monster_satchel");
	while (!FNullEnt(pFind))
	{
		CBaseEntity* pEnt = CBaseEntity::Instance(pFind);
		if (pEnt && pEnt->pev->owner == m_pPlayer->edict())
			m_chargeReady = 1;
		pFind = FIND_ENTITY_BY_CLASSNAME(pFind, "monster_satchel");
	}
}

// orig 0x203470
void CSatchel::Spawn()
{
	Precache();
	m_iId = WEAPON_SATCHEL;
	SET_MODEL(ENT(pev), "models/w_satchel.mdl");

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("SATCHEL", "PickupGive", 1, 0, 250);

	FallInit(); // get ready to fall down.
}


// orig 0x2034f0
void CSatchel::Precache()
{
	PRECACHE_MODEL("models/v_satchel.mdl");
	PRECACHE_MODEL("models/v_satchel_radio.mdl");
	PRECACHE_MODEL("models/w_satchel.mdl");
	PRECACHE_MODEL("models/p_satchel.mdl");
	PRECACHE_MODEL("models/p_satchel_radio.mdl");
	g_iRatSPR = PRECACHE_MODEL("sprites/shockwave.spr");

	UTIL_PrecacheOther("monster_satchel");
}


// orig 0x203590
bool CSatchel::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Satchel Charge";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("SATCHEL", "MaxAmmo", 10, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 1;
	p->iFlags = ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	p->iId = m_iId = WEAPON_SATCHEL;
	p->iWeight = -1;

	return true;
}

//=========================================================
//=========================================================
// orig 0x203640
bool CSatchel::IsUseable()
{
	if (m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] > 0)
	{
		// player is carrying some satchels
		return true;
	}

	if (m_chargeReady != 0)
	{
		// player isn't carrying any satchels, but has some out
		return true;
	}

	return false;
}

// orig 0x203690
bool CSatchel::CanDeploy()
{
	if (m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] > 0)
	{
		// player is carrying some satchels
		return true;
	}

	if (m_chargeReady != 0)
	{
		// player isn't carrying any satchels, but has some out
		return true;
	}

	return false;
}

// orig 0x2036e0. Sets the models itself, as the 2001 SDK's did, rather than DefaultDeploy.
bool CSatchel::Deploy()
{
	if (0 == m_chargeReady)
	{
		m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_satchel.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_satchel.mdl");
		SendWeaponAnim(SATCHEL_DRAW);
		// use tripmine animations
		strcpy(m_pPlayer->m_szAnimExtention, "trip");
	}
	else
	{
		m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_satchel_radio.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_satchel_radio.mdl");
		SendWeaponAnim(SATCHEL_RADIO_DRAW);
		// use hivehand animations
		strcpy(m_pPlayer->m_szAnimExtention, "hive");
	}

	m_pPlayer->m_flNextAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("SATCHEL", "DeployNextFire", 1.0);
	// halflife-updated's fix, kept: the first idle comes 2 s after the deploy (halflife #2495); the
	// original waits 10-15 s
	m_flTimeWeaponIdle = gpGlobals->time + 2;
	return true;
}


// orig 0x203830
void CSatchel::Holster()
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	if (0 != m_chargeReady)
	{
		SendWeaponAnim(SATCHEL_RADIO_HOLSTER);
	}
	else
	{
		SendWeaponAnim(SATCHEL_DROP);
	}
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);

	if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] && 0 == m_chargeReady)
	{
		m_pPlayer->ClearWeaponBit(m_iId);
		SetThink(&CSatchel::DestroyItem);
		pev->nextthink = gpGlobals->time + 0.1;
	}
}



// orig 0x203920
void CSatchel::PrimaryAttack()
{
	switch (m_chargeReady)
	{
	case 0:
	{
		Throw();
	}
	break;
	case 1:
	{
		SendWeaponAnim(SATCHEL_RADIO_FIRE);

		edict_t* pPlayer = m_pPlayer->edict();

		CBaseEntity* pSatchel = NULL;

		while ((pSatchel = UTIL_FindEntityInSphere(pSatchel, m_pPlayer->pev->origin, 4096)) != NULL)
		{
			if (FClassnameIs(pSatchel->pev, "monster_satchel"))
			{
				if (pSatchel->pev->owner == pPlayer)
				{
					pSatchel->Use(m_pPlayer, m_pPlayer, USE_ON, 0);
					m_chargeReady = 2;
				}
			}
		}

		m_chargeReady = 2;
		// halflife-updated's fix, kept (Steven 2026-11-07): GetNextAttackDelay evens out the fire-rate creep
		m_flNextPrimaryAttack = GetNextAttackDelay(PREFS_KeyLoopUpFloat("SATCHEL", "PrimaryDetRefire", 0.5) * m_pPlayer->m_fCurseRefireScale);
		m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("SATCHEL", "PrimaryDetRefire", 0.5) * m_pPlayer->m_fCurseRefireScale;
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;
		break;
	}

	case 2:
		// we're reloading, don't allow fire
		{
		}
		break;
	}
}


// orig 0x203aa0
void CSatchel::SecondaryAttack()
{
	if (m_chargeReady != 2)
	{
		Throw();
	}
}


// orig 0x203ad0: throws a roach, at most 9 out per player.
void CSatchel::Throw()
{
	if (0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] && CountEntsClaimedByEnt(m_pPlayer->edict(), "monster_satchel") < 10)
	{
		Vector vecSrc = m_pPlayer->pev->origin;

		Vector vecThrow = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("SATCHEL", "ThrowVel", 274.0) + m_pPlayer->pev->velocity;

		CBaseEntity* pSatchel = Create("monster_satchel", vecSrc, Vector(0, 0, 0), m_pPlayer->edict());
		pSatchel->pev->velocity = vecThrow;
		pSatchel->pev->avelocity.y = 400;

		m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_satchel_radio.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_satchel_radio.mdl");

		SendWeaponAnim(SATCHEL_RADIO_DRAW);

		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		m_chargeReady = 1;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		// halflife-updated's fix, kept (Steven 2026-11-07): GetNextAttackDelay evens out the fire-rate creep
		m_flNextPrimaryAttack = GetNextAttackDelay(PREFS_KeyLoopUpFloat("SATCHEL", "ThrowPrimaryRefire", 1.0) * m_pPlayer->m_fCurseRefireScale);
		m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("SATCHEL", "ThrowSecondaryRefire", 0.5) * m_pPlayer->m_fCurseRefireScale;
	}
}


// orig 0x203cd0
void CSatchel::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	switch (m_chargeReady)
	{
	case 0:
		SendWeaponAnim(SATCHEL_FIDGET1);
		// use tripmine animations
		strcpy(m_pPlayer->m_szAnimExtention, "trip");
		break;
	case 1:
		SendWeaponAnim(SATCHEL_RADIO_FIDGET1);
		// use hivehand animations
		strcpy(m_pPlayer->m_szAnimExtention, "hive");
		break;
	case 2:
		if (0 == m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			m_chargeReady = 0;
			RetireWeapon();
			return;
		}

		m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_satchel.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_satchel.mdl");

		SendWeaponAnim(SATCHEL_DRAW);

		// use tripmine animations
		strcpy(m_pPlayer->m_szAnimExtention, "trip");

		// halflife-updated's fix, kept (Steven 2026-11-07): GetNextAttackDelay evens out the fire-rate creep
		m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		m_flNextSecondaryAttack = gpGlobals->time + 0.5;
		m_chargeReady = 0;
		break;
	}
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15); // how long till we do this again.
}

//=========================================================
// DeactivateSatchels - removes all satchels owned by
// the provided player. Should only be used upon death.
//
// Made this global on purpose.
//=========================================================
// orig 0x203e90. Leaves armed roaches to go off.
void DeactivateSatchels(CBasePlayer* pOwner)
{
	edict_t* pFind;

	pFind = FIND_ENTITY_BY_CLASSNAME(NULL, "monster_satchel");

	while (!FNullEnt(pFind))
	{
		CBaseEntity* pEnt = CBaseEntity::Instance(pFind);

		if (pEnt && pEnt->pev->owner == pOwner->edict())
		{
			// VERIFY: a killer rat is a "monster_satchel" too; the original reads CSatchelCharge's
			// m_iArmed past the end of the rat, ours treats a rat as unarmed (dossier, "Original bugs")
			CSatchelCharge* pSatchel = dynamic_cast<CSatchelCharge*>(pEnt);
			if (!pSatchel || pSatchel->m_iArmed != 1)
			{
				pEnt->pev->solid = SOLID_NOT; // CSatchelCharge::Deactivate, which the original calls on either
				UTIL_Remove(pEnt);
			}
		}

		pFind = FIND_ENTITY_BY_CLASSNAME(pFind, "monster_satchel");
	}
}
