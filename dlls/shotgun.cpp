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

// Rocket Crowbar 1.9: the shotgun. Primary throws a screaming scientist (pipebomb, a CGrenade
// that explodes on touch); secondary fires a beam that opens a CSpaceRift where it hits a wall.
// Hitting a rift grows it; once open it pulls everything in, gibs what reaches it and ends in a
// blast. Numbers come from rc.ini [SHOTGUN]. Reconstructed from rc_i386.so (addresses are file
// addresses, reference/funcfile.tsv); see reference/dossiers/w2-shotgun-egon-crowbar.md.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "effects.h"
#include "decals.h"
#include "UserMessages.h"
#include "Preff.h"

extern unsigned short FixedUnsigned16(float value, float scale); // util.cpp

// The RC cap a space rift pulls in (HasRCCap). VERIFY name: the original has only the bit.
#define RC_CAP_SHOTGUN_RIFT_MOVEABLE 0x8

// Sprites the rift and the shotgun's beam use, precached by CShotgun::Precache.
int g_iSuckResdue;	   // sprites/flare6.spr
int g_iShotMainBeam;   // sprites/xbeam1.spr
int g_iShotMainFlare;  // sprites/xspark2.spr (precached, unused)
int g_iShotSpit;	   // sprites/cnt1.spr
int g_iSuckStartFlare; // sprites/xflare1.spr
int g_iSpit2Model;	   // sprites/xenobeam.spr
int g_iSuckFlare[2];   // sprites/c-tele1.spr, sprites/b-tele1.spr

// RC's bot object, as the original reads it (see crowbar.cpp and reference/DEVIATIONS.md).
static CBaseMonster* RCBotEnt(CBot* pBot) { return reinterpret_cast<CBaseMonster*>(pBot); }
static float RCBotFireGate(CBot* pBot) { return *reinterpret_cast<float*>(reinterpret_cast<byte*>(pBot) + 0x698); } // VERIFY name

//=========================================================
// CFlyinScientist (pipebomb): the shotgun's primary, a scientist thrown like a grenade that
// explodes on touch.
//=========================================================
class CFlyinScientist : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	void ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce) override;
	int GetRCCaps() override;
	virtual bool ProMagEffected(CBaseEntity* pMagnet);

	void EXPORT AimThink();
	static CFlyinScientist* PipeCreate();

	float m_flScreamTime; // orig +0x280
	bool m_blScreamed;	  // orig +0x284
};

LINK_ENTITY_TO_CLASS(pipebomb, CFlyinScientist); // orig 0x20bcf0

// orig 0x20bd20
CFlyinScientist* CFlyinScientist::PipeCreate()
{
	CFlyinScientist* pSci = GetClassPtr((CFlyinScientist*)NULL);
	pSci->pev->classname = MAKE_STRING("shotgun");
	pSci->Spawn();
	return pSci;
}

// orig 0x20bd70
void CFlyinScientist::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->gravity = PREFS_KeyLoopUpFloat("SHOTGUN", "SciGravity", 0.75);

	SET_MODEL(ENT(pev), "models/scientist.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-16, -16, -16), Vector(16, 16, 16));

	SetTouch(&CFlyinScientist::ExplodeTouch);
	SetThink(&CFlyinScientist::AimThink);
	pev->nextthink = gpGlobals->time + 0.1;

	pev->dmg = PREFS_KeyLoopUpFloat("SHOTGUN", "SciDamage", 100.0);
	pev->body = RANDOM_LONG(0, 3);
	pev->sequence = LookupSequence("fallingloop");
	ResetSequenceInfo();
	m_fSequenceLoops = true;

	m_flScreamTime = gpGlobals->time;
	m_blScreamed = false;
}

// orig 0x20bef0: face along the flight, scream once
void CFlyinScientist::AimThink()
{
	StudioFrameAdvance(0.0);
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->velocity.Length() > 32)
		pev->angles = UTIL_VecToAngles(pev->velocity);
	else
		pev->angles = UTIL_VecToAngles(Vector(0, 0, -1));
	pev->angles.y -= 180;
	pev->angles.z -= 180;

	if (m_flScreamTime < gpGlobals->time && !m_blScreamed)
	{
		char szSound[64];
		sprintf(szSound, "scientist/scream0%i.wav", RANDOM_LONG(1, 9));
		EMIT_SOUND(ENT(pev), CHAN_VOICE, szSound, 1, ATTN_NORM);
		m_blScreamed = true;
	}
}

// orig 0x20c040
void CFlyinScientist::Precache()
{
	PRECACHE_MODEL("models/scientist.mdl");
}

// orig 0x212140: steer toward the magnet at the current speed (only flat when above it)
void CFlyinScientist::ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce)
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

// orig 0x212240
bool CFlyinScientist::ProMagEffected(CBaseEntity* pMagnet)
{
	if (!pev->owner)
		return false;
	return pMagnet->edict() != pev->owner;
}

// orig 0x212270
int CFlyinScientist::GetRCCaps()
{
	return 0x6080001c;
}

//=========================================================
// CSpaceRift ("shotgun_rift", no entity factory): opened by the shotgun's secondary on a wall.
// Each hit grows it (Grow); RiftOpenHits hits start a 3 second pre-open (StartPreopen), then it
// sucks for 10 seconds (StartSuck, SuckThink, DoEffects) and ends in FinalBoom. Left alone it
// decays away (Decay). A second rift opening while one sucks blows both.
//=========================================================
class CSpaceRift : public CGrenade
{
public:
	void Spawn() override;
	void EntIsBeingRemoved(edict_t* pEdict) override;

	void EXPORT DamageTouch(CBaseEntity* pOther);
	void EXPORT PreOpenDamageTouch(CBaseEntity* pOther);
	void EXPORT FinalBoom();
	void DoRings(Vector vecAngles);
	void DoEffects();
	void EXPORT SuckThink();
	void StartSuck();
	static CSpaceRift* RiftCreate();
	void EXPORT Decay();
	void EXPORT PrepareOpenThink();
	void StartPreopen();
	bool Grow();

	entvars_t* OwnerVars(); // the owner's entvars while its edict is live, else NULL

	float m_flHits;					// orig +0x280, grows by 1 a hit, decays at RiftOpenDecayRate
	CBaseEntity* m_pFlare;			// orig +0x284, the "rift_flare" sprite that follows the rift
	edict_t* m_pOwner;				// orig +0x288, the player whose shot opened it
	Vector m_vecNormal;				// orig +0x28c, the wall's normal
	float m_flSuckEndTime;			// orig +0x298
	float m_flNextExpel;			// orig +0x29c
	float m_flNextBigExpel;			// orig +0x2a0
	float m_flNextShake;			// orig +0x2a4
	Vector m_vecRingAngles;			// orig +0x2a8
	float m_flRingRadius;			// orig +0x2b4
	float m_flNextRings;			// orig +0x2b8
	float m_flNextSuckPulse;		// orig +0x2bc
	float m_flSuckRange;			// orig +0x2c0
	float m_flSuckVelo;				// orig +0x2c4, read from rc.ini and never used
	bool m_blSucking;				// orig +0x2c8
	bool m_blPreOpen;				// orig +0x2c9
	CBeam* m_pPreOpenBeams[8];		// orig +0x2cc
	CBeam* m_pOtherBeams[8];		// orig +0x2ec, cleaned up but never created. VERIFY name
	CBeam* m_pSuckBeam;				// orig +0x30c
};

entvars_t* CSpaceRift::OwnerVars()
{
	if (m_pOwner && !m_pOwner->free && m_pOwner->pvPrivateData)
		return &m_pOwner->v;
	return NULL;
}

// orig 0x20c070: anything that reaches an open rift is gibbed
void CSpaceRift::DamageTouch(CBaseEntity* pOther)
{
	pev->classname = MAKE_STRING("shotgun"); // kills read as the shotgun
	entvars_t* pevOwner = OwnerVars();
	pOther->TakeDamage(pev, pevOwner ? pevOwner : pev, 9999, DMG_ALWAYSGIB);
	pev->classname = MAKE_STRING("shotgun_rift");
}

// orig 0x20c110
void CSpaceRift::PreOpenDamageTouch(CBaseEntity* pOther)
{
	pev->classname = MAKE_STRING("shotgun");
	entvars_t* pevOwner = OwnerVars();
	pOther->TakeDamage(pev, pevOwner ? pevOwner : pev, 1, DMG_ALWAYSGIB);
	pev->classname = MAKE_STRING("shotgun_rift");
}

// orig 0x20c1b0
void CSpaceRift::FinalBoom()
{
	STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/egon_run3.wav");
	STOP_SOUND(ENT(pev), CHAN_BODY, "RcbAddtions/grav_greande.wav");
	STOP_SOUND(ENT(pev), CHAN_ITEM, "ambience/particle_suck1.wav");
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "common/null.wav", 1, ATTN_NORM);
	EMIT_SOUND(ENT(pev), CHAN_BODY, "common/null.wav", 1, ATTN_NORM);
	EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/null.wav", 0, ATTN_NORM);

	for (int i = 0; i < 8; i++)
	{
		if (!m_blSucking && m_blPreOpen)
		{
			if (m_pPreOpenBeams[i] && m_pPreOpenBeams[i]->pev && !FNullEnt(m_pPreOpenBeams[i]->pev))
			{
				m_pPreOpenBeams[i]->SetThink(&CBaseEntity::SUB_Remove);
				m_pPreOpenBeams[i]->pev->nextthink = gpGlobals->time + 0.01;
			}
		}
		if (i < 8)
		{
			if (m_pOtherBeams[i] && m_pOtherBeams[i]->pev && !FNullEnt(m_pOtherBeams[i]->pev))
			{
				m_pOtherBeams[i]->SetThink(&CBaseEntity::SUB_Remove);
				m_pOtherBeams[i]->pev->nextthink = gpGlobals->time + 0.01;
			}
		}
	}

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_iSuckStartFlare);
	WRITE_BYTE(80); // scale * 10
	WRITE_BYTE(15); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_GLOWSPRITE);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_iSuckResdue);
	WRITE_BYTE(8);	 // life
	WRITE_BYTE(40);	 // scale
	WRITE_BYTE(200); // brightness
	MESSAGE_END();

	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin - m_vecNormal * 32, ignore_monsters, ENT(pev), &tr);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(tr.vecEndPos.x);
	WRITE_COORD(tr.vecEndPos.y);
	WRITE_COORD(tr.vecEndPos.z);
	WRITE_COORD(tr.vecEndPos.x + m_vecNormal.x);
	WRITE_COORD(tr.vecEndPos.y + m_vecNormal.y);
	WRITE_COORD(tr.vecEndPos.z + m_vecNormal.z);
	WRITE_SHORT(g_iShotSpit);
	WRITE_BYTE(10); // count
	WRITE_BYTE(2);	// life
	WRITE_BYTE(3);	// scale
	WRITE_BYTE(20); // velocity
	WRITE_BYTE(20); // randomness
	MESSAGE_END();

	entvars_t* pevOwner = OwnerVars();
	pev->owner = NULL;
	pev->classname = MAKE_STRING("shotgun");
	RadiusDamage(pev, pevOwner, pev->dmg, CLASS_NONE, DMG_BLAST);
	pev->classname = MAKE_STRING("shotgun_rift");
	// Original crash, guarded: the original writes pevOwner->pContainingEntity even when the
	// owner is gone (EntIsBeingRemoved cleared it), a NULL read. VERIFY (reference/DEVIATIONS.md).
	pev->owner = pevOwner ? pevOwner->pContainingEntity : NULL;

	UTIL_DecalTrace(&tr, RANDOM_FLOAT(0, 1) < 0.5 ? DECAL_SCORCH1 : DECAL_SCORCH2);

	SetThink(&CSpaceRift::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_pFlare && m_pFlare->pev && !FNullEnt(m_pFlare->pev))
	{
		m_pFlare->SUB_Remove();
		m_pFlare = NULL;
	}

	if (m_pSuckBeam && m_pSuckBeam->pev && !FNullEnt(m_pSuckBeam->pev))
	{
		edict_t* pent = m_pSuckBeam->edict();
		if (pent && pent->pvPrivateData && !pent->free)
		{
			m_pSuckBeam->SetThink(&CBaseEntity::SUB_Remove);
			m_pSuckBeam->pev->nextthink = gpGlobals->time + 0.1;
		}
	}
}

// orig 0x20c9b0. Builds the eight ring points around the rift and draws nothing: the original's
// drawing loop is empty. Its only effect is UTIL_MakeVectors on gpGlobals.
void CSpaceRift::DoRings(Vector vecAngles)
{
	Vector vecPoints[9];

	UTIL_MakeVectors(vecAngles);
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
		vecPoints[i] = vecPoints[i].Normalize() * m_flRingRadius + pev->origin;

	for (int i = 0; i < 8; i++)
	{
		// the original draws nothing here
	}
}

// orig 0x20cec0: the open rift's effects, every SuckThink
void CSpaceRift::DoEffects()
{
	// the flare pulses (steady with sv_lowlag 1)
	if (CVAR_GET_FLOAT("sv_lowlag") == 1)
		m_pFlare->pev->renderamt = 240;
	else
	{
		double flPhase = 1.3 * gpGlobals->time;
		m_pFlare->pev->renderamt = cos(flPhase) * cos(flPhase) * 80.0 + 180.0;
	}

	// small debris shot out of the rift
	if (m_flNextExpel < gpGlobals->time && CountEntsInWorld("flying_expel") <= 25)
	{
		if (CVAR_GET_FLOAT("sv_lowlag") != 1)
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
			pExpel->pev->owner = ENT(pev);
			pExpel->pev->nextthink = gpGlobals->time + 15;
			SET_MODEL(pExpel->edict(), "models/rpgrocket.mdl");
			UTIL_SetOrigin(pExpel->pev, pev->origin);
			UTIL_SetSize(pExpel->pev, g_vecZero, g_vecZero);
			pExpel->pev->owner = ENT(pev);

			UTIL_MakeVectors(UTIL_VecToAngles(m_vecNormal));
			float x, y, z;
			do
			{
				x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
				y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
				z = x * x + y * y;
			} while (z > 1);

			Vector vecDir = m_vecNormal + gpGlobals->v_up * x * 0.17365 + gpGlobals->v_right * y * 0.17365;
			pExpel->pev->velocity = vecDir.Normalize() * 800;
			pExpel->pev->velocity.z += 200;

			MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_BEAMFOLLOW);
			WRITE_SHORT(pExpel->entindex()); // entity
			WRITE_SHORT(g_sModelIndexLaser); // model
			WRITE_BYTE(10);					 // life
			WRITE_BYTE(2);					 // width
			WRITE_BYTE(200);				 // r, g, b
			WRITE_BYTE(200);
			WRITE_BYTE(255);
			WRITE_BYTE(128); // brightness
			MESSAGE_END();

			m_flNextExpel = gpGlobals->time + RANDOM_FLOAT(0.1, 0.4);
		}
	}

	// bigger debris, with no count limit
	if (m_flNextBigExpel < gpGlobals->time)
	{
		if (CVAR_GET_FLOAT("sv_lowlag") != 1)
		{
			CBaseEntity* pExpel = GetClassPtr((CBaseEntity*)NULL);
			pExpel->Spawn();
			pExpel->pev->classname = MAKE_STRING("big_flying_expel");
			pExpel->pev->movetype = MOVETYPE_TOSS;
			pExpel->pev->solid = SOLID_TRIGGER;
			pExpel->pev->renderamt = 0;
			pExpel->pev->rendermode = kRenderTransColor;
			pExpel->SetTouch(&CBaseEntity::RemoveTouch);
			pExpel->SetThink(&CBaseEntity::SUB_Remove);
			pExpel->pev->nextthink = gpGlobals->time + 15;
			pExpel->pev->owner = ENT(pev);
			SET_MODEL(pExpel->edict(), "models/rpgrocket.mdl");
			UTIL_SetOrigin(pExpel->pev, pev->origin);
			pExpel->pev->owner = ENT(pev);

			UTIL_MakeVectors(UTIL_VecToAngles(m_vecNormal));
			float x, y, z;
			do
			{
				x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
				y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
				z = x * x + y * y;
			} while (z > 1);

			Vector vecDir = m_vecNormal + gpGlobals->v_up * x * 0.17365 + gpGlobals->v_right * y * 0.17365;
			pExpel->pev->velocity = vecDir.Normalize() * 1000;
			pExpel->pev->velocity.z += 300;

			MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_BEAMFOLLOW);
			WRITE_SHORT(pExpel->entindex()); // entity
			WRITE_SHORT(g_iSpit2Model);		 // model
			WRITE_BYTE(10);					 // life
			WRITE_BYTE(8);					 // width
			WRITE_BYTE(255);				 // r, g, b
			WRITE_BYTE(255);
			WRITE_BYTE(255);
			WRITE_BYTE(180); // brightness
			MESSAGE_END();

			m_flNextBigExpel = gpGlobals->time + RANDOM_FLOAT(0.8, 0.9);
		}
	}

	// a pulse that hurts everything near
	if (m_flNextSuckPulse < gpGlobals->time)
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_EXPLOSION);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_SHORT(g_iSuckFlare[RANDOM_LONG(0, 1)]);
		WRITE_BYTE(30); // scale * 10
		WRITE_BYTE(30); // framerate
		WRITE_BYTE(TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOPARTICLES);
		MESSAGE_END();

		pev->classname = MAKE_STRING("shotgun");
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "ambience/particle_suck1.wav", 1, ATTN_NORM);
		entvars_t* pevOwner = OwnerVars();
		RadiusDamage(pev, pevOwner ? pevOwner : pev, 110, CLASS_NONE, DMG_BLAST);
		pev->classname = MAKE_STRING("shotgun_rift");

		m_flNextSuckPulse = gpGlobals->time + RANDOM_FLOAT(1.2, 1.8);
	}

	// shake every player within 1512 units
	if (m_flNextShake < gpGlobals->time)
	{
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
			if (!pPlayer)
				continue;
			if ((pPlayer->pev->origin - pev->origin).Length() > 1512)
				continue;

			// Original oddity, kept: the amplitude grows with distance, so the farthest
			// players shake hardest (dossier, "Original bugs").
			float flFrac = (pPlayer->pev->origin - pev->origin).Length() / 1512;
			unsigned short usDuration = FixedUnsigned16(0.5, 1 << 12);
			unsigned short usFrequency = FixedUnsigned16(25, 1 << 8);
			unsigned short usAmplitude = FixedUnsigned16(flFrac * 200, 1 << 12);

			MESSAGE_BEGIN(MSG_ONE, gmsgShake, NULL, pPlayer->edict());
			WRITE_SHORT(usAmplitude);
			WRITE_SHORT(usDuration);
			WRITE_SHORT(usFrequency);
			MESSAGE_END();
		}
		m_flNextShake = gpGlobals->time + 0.5;
	}

	// turn the (invisible) rings
	if (m_flNextRings < gpGlobals->time)
	{
		m_flRingRadius += RANDOM_FLOAT(-7, 7);
		if (m_flRingRadius < 78)
			m_flRingRadius = 78;
		if (m_flRingRadius > 208)
			m_flRingRadius = 208;

		float flPitch = RANDOM_FLOAT(10, 20);
		float flYaw = RANDOM_FLOAT(10, 20);
		m_vecRingAngles = m_vecRingAngles + Vector(flPitch, flYaw, 0);
		if (m_vecRingAngles.x > 360)
			m_vecRingAngles.x -= 360;
		if (m_vecRingAngles.y > 360)
			m_vecRingAngles.y -= 360;

		DoRings(m_vecRingAngles);
		m_flNextRings = gpGlobals->time + 0.1;
	}
}

// orig 0x20e000: pull everything with the rift-move cap toward the rift
void CSpaceRift::SuckThink()
{
	CBaseEntity* pEntity = NULL;

	pev->nextthink = gpGlobals->time + 0.01;

	if (pev->dmgtime <= gpGlobals->time)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/egon_run3.wav", 1, ATTN_NORM);
		pev->dmgtime = gpGlobals->time + 2.2;
	}

	if (m_flSuckEndTime <= gpGlobals->time)
	{
		if (m_pFlare)
		{
			m_pFlare->SUB_Remove();
			m_pFlare = NULL;
		}
		SetThink(&CSpaceRift::FinalBoom);
		return;
	}

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, m_flSuckRange)) != NULL)
	{
		if (pEntity->pev->movetype == MOVETYPE_NONE || pEntity->pev == pev)
			continue;

		int iCap = RC_CAP_SHOTGUN_RIFT_MOVEABLE;
		if (!pEntity->HasRCCap(iCap) || (pEntity->pev->flags & FL_FROZEN) != 0)
			continue;

		// the original builds the flat direction to the target twice
		Vector vecFlat = pEntity->pev->origin - pev->origin;
		vecFlat.z = 0;
		UTIL_MakeVectors(UTIL_VecToAngles(vecFlat.Normalize()));
		vecFlat = pEntity->pev->origin - pev->origin;
		vecFlat.z = 0;
		UTIL_MakeVectors(UTIL_VecToAngles(vecFlat.Normalize()));

		float flDist = (pEntity->pev->origin - pev->origin).Length();
		if (flDist < 64)
		{
			DamageTouch(pEntity);
			continue;
		}

		float flPull = (m_flSuckRange / flDist) * 25;
		if (!(flPull < 2000))
			flPull = 2000;
		pEntity->pev->velocity = pEntity->pev->velocity - gpGlobals->v_forward * flPull;

		entvars_t* pevTarget = pEntity->pev;
		if (fabs(pevTarget->origin.z - pev->origin.z) > 32)
		{
			float flLift = (m_flSuckRange / (pev->origin.z - pevTarget->origin.z)) * 25;
			if (!(flLift < 2000))
				flLift = 2000;
			pevTarget->velocity.z = pevTarget->velocity.z + flLift;
		}

		pevTarget = pEntity->pev;
		if ((pevTarget->flags & FL_ONGROUND) != 0 || pevTarget->velocity.z == 0 || fabs(pev->origin.z - pevTarget->origin.z) <= 32)
		{
			pEntity->pev->flags &= ~FL_ONGROUND;
			pEntity->pev->velocity.z += 200;
		}

		pevTarget = pEntity->pev;
		if (pevTarget->velocity.Length() > 2000)
			pevTarget->velocity = pevTarget->velocity.Normalize() * 2000;
	}

	DoEffects();
}

// orig 0x20e5f0: open. If another rift is already sucking, both blow instead.
void CSpaceRift::StartSuck()
{
	bool bOtherOpen = false;
	edict_t* pent = NULL;

	while (!FNullEnt(pent = FIND_ENTITY_BY_STRING(pent, "classname", "shotgun_rift")))
	{
		CSpaceRift* pRift = (CSpaceRift*)CBaseEntity::Instance(pent);
		if (pRift && pRift->m_blSucking)
		{
			pRift->SetThink(&CSpaceRift::FinalBoom);
			pRift->pev->nextthink = gpGlobals->time + 0.01;
			bOtherOpen = true;
		}
	}

	if (bOtherOpen)
	{
		if (m_pFlare)
		{
			m_pFlare->SUB_Remove();
			m_pFlare = NULL;
		}
		SetThink(&CSpaceRift::FinalBoom);
		pev->nextthink = gpGlobals->time + 0.01;
		return;
	}

	pev->effects |= EF_LIGHT;
	pev->effects |= EF_BRIGHTLIGHT;

	double flPhase = 1.3 * gpGlobals->time;
	m_pFlare->pev->renderamt = cos(flPhase) * cos(flPhase) * 80.0 + 180.0;
	m_pFlare->pev->rendermode = kRenderGlow;
	m_pFlare->pev->renderfx = kRenderFxNoDissipation;
	m_pFlare->pev->rendercolor = Vector(200, 200, 255);
	m_pFlare->pev->scale = 7.0;
	m_pFlare->pev->frame = 0;
	SET_MODEL(m_pFlare->edict(), "sprites/blueflare1.spr");

	// a beam straight out of the wall
	m_pSuckBeam = CBeam::BeamCreate(g_pModelNameLaser, 80);
	m_pSuckBeam->SetBrightness(255);
	m_pSuckBeam->SetColor(128, 128, 255);
	m_pSuckBeam->SetNoise(10);
	m_pSuckBeam->SetScrollRate(128);
	m_pSuckBeam->SetStartPos(pev->origin);
	m_pSuckBeam->SetEndPos(pev->origin + m_vecNormal * 768);
	m_pSuckBeam->SetFlags(0x80); // BEAM_FSHADEOUT (not defined in this SDK)

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_iSuckStartFlare);
	WRITE_BYTE(100); // scale * 10
	WRITE_BYTE(20);	 // framerate
	WRITE_BYTE(TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();

	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/egon_windup2.wav", 1, ATTN_NORM);
	EMIT_SOUND(ENT(pev), CHAN_BODY, "RcbAddtions/shotgun_rift.wav", 1, ATTN_NORM);

	float flPitch = RANDOM_FLOAT(0, 360);
	float flYaw = RANDOM_FLOAT(0, 360);
	float flRoll = RANDOM_FLOAT(0, 360);
	m_vecRingAngles = Vector(flPitch, flYaw, flRoll);
	m_flRingRadius = 128;

	SetTouch(&CSpaceRift::DamageTouch);
	SetThink(&CSpaceRift::SuckThink);
	pev->nextthink = gpGlobals->time + 0.01;

	UTIL_SetSize(pev, Vector(-12, -12, -12), Vector(12, 12, 12));
	pev->dmg = PREFS_KeyLoopUpFloat("SHOTGUN", "RiftFinalBlastDamage", 150.0);
	m_blSucking = true;
	m_flSuckRange = PREFS_KeyLoopUpFloat("SHOTGUN", "RiftSuckRange", 900.0);
	m_flSuckVelo = PREFS_KeyLoopUpFloat("SHOTGUN", "RiftSuckVelo", 900.0);

	// the owner can't open another rift for NewRiftDelay seconds
	if (m_pOwner && CBaseEntity::Instance(m_pOwner))
	{
		CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(m_pOwner);
		if (pPlayer)
			pPlayer->m_fShotgunRiftTime = gpGlobals->time + PREFS_KeyLoopUpFloat("SHOTGUN", "NewRiftDelay", 30.0);
	}

	m_flSuckEndTime = gpGlobals->time + 10;
	m_flNextExpel = gpGlobals->time + 0.2;
	m_flNextShake = gpGlobals->time + 0.2;
	m_flNextBigExpel = gpGlobals->time + 0.2;
	m_flNextRings = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 0.01;
	pev->dmgtime = gpGlobals->time + 3.8;
	m_flNextSuckPulse = gpGlobals->time + 0.9;
}

// orig 0x20ed70
CSpaceRift* CSpaceRift::RiftCreate()
{
	CSpaceRift* pRift = GetClassPtr((CSpaceRift*)NULL);
	pRift->pev->classname = MAKE_STRING("shotgun_rift");
	pRift->Spawn();
	return pRift;
}

// orig 0x20edc0
void CSpaceRift::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	pev->rendermode = kRenderTransColor;
	pev->renderamt = 0;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-1, -1, -1), Vector(1, 1, 1));

	SetTouch(&CSpaceRift::PreOpenDamageTouch);
	SetThink(&CSpaceRift::Decay);
	pev->nextthink = gpGlobals->time + 0.05;

	pev->dmg = PREFS_KeyLoopUpFloat("SHOTGUN", "RiftFinalBlastDamage", 150.0);
	m_flHits = 1;
	m_blSucking = false;
	m_blPreOpen = false;

	m_pFlare = GetClassPtr((CBaseEntity*)NULL);
	m_pFlare->Spawn();
	m_pFlare->pev->classname = MAKE_STRING("rift_flare");
	m_pFlare->pev->movetype = MOVETYPE_FOLLOW;
	m_pFlare->pev->solid = SOLID_NOT;
	m_pFlare->pev->renderamt = 128;
	m_pFlare->pev->rendermode = kRenderTransAdd;
	m_pFlare->pev->renderfx = kRenderFxNone;
	m_pFlare->pev->scale = 1.5;
	SET_MODEL(m_pFlare->edict(), "sprites/flare6.spr");
	UTIL_SetOrigin(m_pFlare->pev, pev->origin);
	m_pFlare->pev->aiment = ENT(pev);
	m_pFlare->pev->frame = 0;
}

// orig 0x20f000: shrink and fade until gone, unless it has opened
void CSpaceRift::Decay()
{
	if (m_blSucking)
		return;

	pev->nextthink = gpGlobals->time + 0.05;
	m_flHits = m_flHits - PREFS_KeyLoopUpFloat("SHOTGUN", "RiftOpenDecayRate", 0.4) * 0.05;
	m_pFlare->pev->scale = m_pFlare->pev->scale - PREFS_KeyLoopUpFloat("SHOTGUN", "RiftOpenDecayRate", 0.4) * 0.05;
	m_pFlare->pev->renderamt = m_pFlare->pev->renderamt - 32 * PREFS_KeyLoopUpFloat("SHOTGUN", "RiftOpenDecayRate", 0.4) * 0.05;
	m_pFlare->pev->frame = m_pFlare->pev->frame + 1;
	if (m_pFlare->pev->frame >= 20)
		m_pFlare->pev->frame = 0;
	if (m_pFlare->pev->scale <= 0)
		m_pFlare->pev->scale = 0;
	if (m_pFlare->pev->renderamt <= 0)
		m_pFlare->pev->renderamt = 0;

	if (m_flHits <= 0)
	{
		SetThink(&CSpaceRift::SUB_Remove);
		if (m_pFlare)
			m_pFlare->SUB_Remove();
		SUB_Remove();
	}
}

// orig 0x20f1c0
void CSpaceRift::PrepareOpenThink()
{
	pev->nextthink = gpGlobals->time + 0.01;
	if (pev->dmgtime <= gpGlobals->time)
		StartSuck();
	else
	{
		double flPhase = 1.8 * gpGlobals->time;
		m_pFlare->pev->renderamt = cos(flPhase) * cos(flPhase) * 70.0 + 170.0;
	}
}

// orig 0x20f270: three seconds of crackling beams before it opens
void CSpaceRift::StartPreopen()
{
	SetThink(&CSpaceRift::PrepareOpenThink);
	pev->nextthink = gpGlobals->time + 0.01;
	pev->dmg_save = 3.0;
	pev->dmgtime = gpGlobals->time + pev->dmg_save;

	double flPhase = 1.8 * gpGlobals->time;
	m_pFlare->pev->renderamt = cos(flPhase) * cos(flPhase) * 70.0 + 170.0;
	m_pFlare->pev->rendermode = kRenderGlow;
	m_pFlare->pev->renderfx = kRenderFxNoDissipation;
	m_pFlare->pev->rendercolor = Vector(160, 160, 255);
	m_pFlare->pev->scale = 4.0;
	m_pFlare->pev->frame = 0;
	SET_MODEL(m_pFlare->edict(), "sprites/blueflare2.spr");

	pev->effects |= EF_LIGHT | EF_DIMLIGHT;
	m_blPreOpen = true;

	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/egon_windup2.wav", 1, ATTN_NORM, 0, 90);
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "ambience/particle_suck1.wav", 1, ATTN_NORM);

	(void)UTIL_VecToAngles(m_vecNormal); // computed and unused in the original

	Vector vecSrc = pev->origin;
	// RANDOM_LONG(6, 8) is drawn again each pass, as in the original
	for (int i = 0; i < RANDOM_LONG(6, 8); i++)
	{
		float flX = RANDOM_FLOAT(-0.2, 0.2);
		float flY = RANDOM_FLOAT(-0.2, 0.2);
		float flZ = RANDOM_FLOAT(-0.2, 0.2);
		Vector vecDir = (m_vecNormal + Vector(flX, flY, flZ)).Normalize();
		Vector vecEnd = vecSrc + vecDir * RANDOM_FLOAT(256, 384);

		TraceResult tr;
		UTIL_TraceLine(vecSrc, vecEnd, ignore_monsters, ENT(pev), &tr);
		Vector vecOfs = tr.flFraction >= 1.0 ? g_vecZero : tr.vecPlaneNormal * 2;
		vecEnd = tr.vecEndPos + vecOfs;

		m_pPreOpenBeams[i] = CBeam::BeamCreate(g_pModelNameLaser, 8);
		m_pPreOpenBeams[i]->PointsInit(vecSrc, vecEnd);
		m_pPreOpenBeams[i]->SetBrightness(255);
		m_pPreOpenBeams[i]->SetColor(200, 200, 255);
		m_pPreOpenBeams[i]->SetNoise(128);
		m_pPreOpenBeams[i]->SetScrollRate(128);
		m_pPreOpenBeams[i]->SetThink(&CBaseEntity::SUB_Remove);
		m_pPreOpenBeams[i]->pev->nextthink = gpGlobals->time + (pev->dmg_save + 0.1);
	}

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_iSuckStartFlare);
	WRITE_BYTE(40); // scale * 10
	WRITE_BYTE(30); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();
}

// orig 0x20f8f0. A hit on the rift: true when this hit starts the pre-open. A hit once it is
// opening or open blows it.
bool CSpaceRift::Grow()
{
	m_flHits = m_flHits + 1;

	if (m_blPreOpen || m_blSucking)
	{
		SetThink(&CSpaceRift::FinalBoom);
		pev->nextthink = gpGlobals->time + 0.01;
		return false;
	}

	if (m_flHits >= PREFS_KeyLoopUpFloat("SHOTGUN", "RiftOpenHits", 3.0))
	{
		StartPreopen();
		return true;
	}

	m_pFlare->pev->scale = m_pFlare->pev->scale + 0.5;
	m_pFlare->pev->renderamt = m_pFlare->pev->renderamt + 32;
	if (m_pFlare->pev->renderamt >= 255)
		m_pFlare->pev->renderamt = 255;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(pev->origin.x + m_vecNormal.x);
	WRITE_COORD(pev->origin.y + m_vecNormal.y);
	WRITE_COORD(pev->origin.z + m_vecNormal.z);
	WRITE_SHORT(g_iShotSpit);
	WRITE_BYTE(5);	// count
	WRITE_BYTE(2);	// life
	WRITE_BYTE(2);	// scale
	WRITE_BYTE(50); // velocity
	WRITE_BYTE(5);	// randomness
	MESSAGE_END();

	return false;
}

// orig 0x211fe0
void CSpaceRift::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner == pEdict)
		m_pOwner = NULL;
	if (m_pSuckBeam && m_pSuckBeam->pev && m_pSuckBeam->edict() == pEdict)
		m_pSuckBeam = NULL;
	if (m_pFlare && m_pFlare->pev && m_pFlare->edict() == pEdict)
		m_pFlare = NULL;
	for (int i = 0; i < 8; i++)
	{
		if (m_pPreOpenBeams[i] && m_pPreOpenBeams[i]->pev && m_pPreOpenBeams[i]->edict() == pEdict)
			m_pPreOpenBeams[i] = NULL;
		if (m_pOtherBeams[i] && m_pOtherBeams[i]->pev && m_pOtherBeams[i]->edict() == pEdict)
			m_pOtherBeams[i] = NULL;
	}
}

//=========================================================
// CShotgun
//=========================================================
LINK_ENTITY_TO_CLASS(weapon_shotgun, CShotgun); // orig 0x20fb60

// orig 0x20fc30
void CShotgun::Spawn()
{
	Precache();
	m_iId = WEAPON_SHOTGUN;
	SET_MODEL(ENT(pev), "models/w_shotgun.mdl");

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("SHOTGUN", "PickupAmmo", 5, 0, 250);
	m_flBotNextThrow = gpGlobals->time;

	FallInit(); // get ready to fall
}


// orig 0x20fcc0. RC 1.9 fires no shotgun events: no barrel sounds or events to precache.
void CShotgun::Precache()
{
	PRECACHE_MODEL("models/v_shotgun.mdl");
	PRECACHE_MODEL("models/w_shotgun.mdl");
	PRECACHE_MODEL("models/p_shotgun.mdl");
	PRECACHE_MODEL("models/grenade.mdl");

	g_iSuckResdue = PRECACHE_MODEL("sprites/flare6.spr");
	g_iShotMainBeam = PRECACHE_MODEL("sprites/xbeam1.spr");
	g_iShotMainFlare = PRECACHE_MODEL("sprites/xspark2.spr");
	g_iShotSpit = PRECACHE_MODEL("sprites/cnt1.spr");
	g_iSuckStartFlare = PRECACHE_MODEL("sprites/xflare1.spr");
	g_iSpit2Model = PRECACHE_MODEL("sprites/xenobeam.spr");
	g_iSuckFlare[0] = PRECACHE_MODEL("sprites/c-tele1.spr");
	g_iSuckFlare[1] = PRECACHE_MODEL("sprites/b-tele1.spr");
	PRECACHE_MODEL("sprites/blueflare1.spr");
	PRECACHE_MODEL("sprites/blueflare2.spr");

	PRECACHE_SOUND("weapons/egon_windup2.wav");
	PRECACHE_SOUND("weapons/egon_run3.wav");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	PRECACHE_SOUND("RcbAddtions/shotgun_rift.wav");

	m_iShell = PRECACHE_MODEL("models/shotgunshell.mdl"); // shotgun shell

	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("weapons/reload1.wav"); // shotgun reload
	PRECACHE_SOUND("weapons/reload3.wav"); // shotgun reload

	PRECACHE_SOUND("weapons/357_cock1.wav"); // gun empty sound
	PRECACHE_SOUND("weapons/scock1.wav");	 // cock gun

	PRECACHE_SOUND("scientist/scream01.wav");
	PRECACHE_SOUND("scientist/scream02.wav");
	PRECACHE_SOUND("scientist/scream03.wav");
	PRECACHE_SOUND("scientist/scream04.wav");
	PRECACHE_SOUND("scientist/scream05.wav");
	PRECACHE_SOUND("scientist/scream06.wav");
	PRECACHE_SOUND("scientist/scream07.wav");
	PRECACHE_SOUND("scientist/scream08.wav");
	PRECACHE_SOUND("scientist/scream09.wav");
	PRECACHE_SOUND("ambience/biggun2.wav");
	PRECACHE_SOUND("debris/beamstart10.wav");

	UTIL_PrecacheOther("pipebomb");
}

// orig 0x20ffc0. The original sends the WeapPickup message here after the base call;
// halflife-updated's CBasePlayer::AddPlayerItem sends it for every weapon, so only the base
// call remains.
void CShotgun::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	// As the original: this weapon tells the player it was picked up (AddPlayerItem doesn't)
	MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
	WRITE_BYTE(m_iId);
	MESSAGE_END();
}

// orig 0x210050
bool CShotgun::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("SHOTGUN", "MaxAmmo", 15, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = PREFS_KeyLoopUpIntClamp("SHOTGUN", "ClipSize", 5, 0, 250);
	p->iSlot = 2;
	p->iPosition = 1;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SHOTGUN;
	p->iWeight = SHOTGUN_WEIGHT;

	return true;
}



// orig 0x210110
bool CShotgun::Deploy()
{
	return DefaultDeploy("models/v_shotgun.mdl", "models/p_shotgun.mdl", SHOTGUN_DRAW, "shotgun");
}

// orig 0x210150: throw a scientist
void CShotgun::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		m_flNextSecondaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		Reload();
		if (m_iClip == 0)
			PlayEmptySound();
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	SendWeaponAnim(SHOTGUN_FIRE);

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	float flRight = RANDOM_FLOAT(50, 70);
	Vector vecShellVelocity = m_pPlayer->pev->velocity + gpGlobals->v_right * flRight;
	float flUp = RANDOM_FLOAT(100, 150);
	vecShellVelocity = vecShellVelocity + gpGlobals->v_up * flUp + gpGlobals->v_forward * 25;

	EjectBrass(m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_up * -12 + gpGlobals->v_forward * 20 + gpGlobals->v_right * 4, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHOTSHELL);
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "ambience/biggun2.wav", 1, ATTN_NORM);

	Vector vecSrc = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 18;
	(void)m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES); // unused in the original

	CFlyinScientist* pSci = CFlyinScientist::PipeCreate();
	UTIL_SetOrigin(pSci->pev, vecSrc);
	pSci->pev->velocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("SHOTGUN", "PrimaryVelo", 800.0);
	if (m_pPlayer->IsNetClient())
		pSci->pev->velocity.z = PREFS_KeyLoopUpFloat("SHOTGUN", "PrimayZBoost", 200.0) + pSci->pev->velocity.z;
	pSci->pev->owner = m_pPlayer->edict();
	pSci->pev->angles = UTIL_VecToAngles(pSci->pev->velocity);
	pSci->pev->angles.y -= 180;
	pSci->pev->angles.z -= 180;

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	if (m_iClip != 0)
		m_flPumpTime = gpGlobals->time + 0.5;

	m_flNextPrimaryAttack = PREFS_KeyLoopUpFloat("SHOTGUN", "PrimaryRefire", 0.75) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;
	m_flNextSecondaryAttack = PREFS_KeyLoopUpFloat("SHOTGUN", "PrimaryRefire", 0.75) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;
	if (m_iClip != 0)
		m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	else
		m_flTimeWeaponIdle = 0.75; // Original bug, kept: an absolute time, not time + 0.75
	m_fInSpecialReload = 0;

	m_pPlayer->pev->punchangle.x -= PREFS_KeyLoopUpInt("SHOTGUN", "PrimaryAngleKick", 5);
	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * PREFS_KeyLoopUpFloat("SHOTGUN", "PrimaryKick", 400.0);
}


// orig 0x2107e0: the rift beam. One shell; opens a rift on a wall, grows one it hits.
void CShotgun::SecondaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		m_flNextSecondaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		Reload();
		if (m_iClip == 0)
			PlayEmptySound();
		return;
	}

	// a player whose rift closed recently, or a server without rifts, can't fire it
	if (m_pPlayer->m_fShotgunRiftTime > gpGlobals->time || CVAR_GET_FLOAT("mp_allowrifts") != 1)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.9;
		m_flNextSecondaryAttack = gpGlobals->time + 0.9;
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	SendWeaponAnim(SHOTGUN_FIRE);

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	m_flNextPrimaryAttack = PREFS_KeyLoopUpFloat("SHOTGUN", "SecondaryRefire", 0.9) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;
	m_flNextSecondaryAttack = PREFS_KeyLoopUpFloat("SHOTGUN", "SecondaryRefire", 0.9) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 4096;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

	// the beam starts at the muzzle
	vecSrc = vecSrc + gpGlobals->v_right * 2.5 + gpGlobals->v_forward * 15 - gpGlobals->v_up * 2;

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "debris/beamstart10.wav", 1, ATTN_NORM);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(tr.vecEndPos.x);
	WRITE_COORD(tr.vecEndPos.y);
	WRITE_COORD(tr.vecEndPos.z);
	WRITE_SHORT(g_iShotMainBeam);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(50);	 // framerate
	WRITE_BYTE(1);	 // life
	WRITE_BYTE(50);	 // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(255); // r, g, b
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	WRITE_BYTE(250); // brightness
	WRITE_BYTE(50);	 // speed
	MESSAGE_END();

	if (CVAR_GET_FLOAT("sv_lowlag") != 1)
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
		WRITE_BYTE(TE_SPRITETRAIL);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_COORD(vecSrc.x + gpGlobals->v_forward.x);
		WRITE_COORD(vecSrc.y + gpGlobals->v_forward.y);
		WRITE_COORD(vecSrc.z + gpGlobals->v_forward.z);
		WRITE_SHORT(g_iShotSpit);
		WRITE_BYTE(2);	// count
		WRITE_BYTE(2);	// life
		WRITE_BYTE(2);	// scale
		WRITE_BYTE(20); // velocity
		WRITE_BYTE(8);	// randomness
		MESSAGE_END();
	}

	if (!tr.pHit)
		return;

	if (VARS(tr.pHit)->takedamage != DAMAGE_NO)
	{
		// something alive: a shock
		ClearMultiDamage();
		CBaseEntity::Instance(tr.pHit)->TraceAttack(m_pPlayer->pev, PREFS_KeyLoopUpFloat("SHOTGUN", "SecondaryHitLivingDamage", 10.0), gpGlobals->v_forward, &tr, DMG_SHOCK);
		ApplyMultiDamage(pev, m_pPlayer->pev);
		return;
	}

	CSpaceRift* pRift = NULL;

	if (CBaseEntity::Instance(tr.pHit)->IsBSPModel())
	{
		// a wall: grow a rift already near the hit, or open a new one
		if (!m_pPlayer->IsNetClient())
		{
			m_flBotRiftTime = gpGlobals->time + 1.3;
			m_vecBotRiftSpot = tr.vecEndPos + tr.vecPlaneNormal * 8;
		}

		CBaseEntity* pEntity = NULL;
		while ((pEntity = UTIL_FindEntityInSphere(pEntity, tr.vecEndPos, PREFS_KeyLoopUpFloat("SHOTGUN", "SecondaryMaxRehitDist", 24.0))) != NULL)
		{
			if (strcmp(STRING(VARS(pEntity->edict())->classname), "shotgun_rift") == 0)
			{
				if (!pEntity)
					return;
				if (!m_pPlayer->IsNetClient())
					m_flBotRiftTime = gpGlobals->time + 1.3;
				pRift = (CSpaceRift*)pEntity;
				break;
			}
		}

		if (!pRift)
		{
			pRift = CSpaceRift::RiftCreate();
			UTIL_SetOrigin(pRift->pev, tr.vecEndPos + tr.vecPlaneNormal * 8);
			pRift->m_pOwner = m_pPlayer->edict();
			pRift->m_vecNormal = tr.vecPlaneNormal;

			Vector vecRift = tr.vecEndPos + tr.vecPlaneNormal * 8;
			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
			WRITE_BYTE(TE_SPRITETRAIL);
			WRITE_COORD(vecRift.x);
			WRITE_COORD(vecRift.y);
			WRITE_COORD(vecRift.z);
			WRITE_COORD(vecRift.x + tr.vecPlaneNormal.x);
			WRITE_COORD(vecRift.y + tr.vecPlaneNormal.y);
			WRITE_COORD(vecRift.z + tr.vecPlaneNormal.z);
			WRITE_SHORT(g_iShotSpit);
			WRITE_BYTE(5);	// count
			WRITE_BYTE(2);	// life
			WRITE_BYTE(2);	// scale
			WRITE_BYTE(50); // velocity
			WRITE_BYTE(5);	// randomness
			MESSAGE_END();
			return;
		}
	}
	else
	{
		// the beam hit a rift itself
		if (strcmp(STRING(VARS(tr.pHit)->classname), "shotgun_rift") != 0)
			return;
		pRift = (CSpaceRift*)CBaseEntity::Instance(tr.pHit);
		if (!pRift)
			return;
		if (!m_pPlayer->IsNetClient())
			m_flBotRiftTime = gpGlobals->time + 1.3;
	}

	if (pRift->Grow())
	{
		// it is opening
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
		WRITE_BYTE(255); // r, g, b
		WRITE_BYTE(30);
		WRITE_BYTE(70);
		WRITE_BYTE(120); // brightness
		WRITE_BYTE(50);	 // speed
		MESSAGE_END();
	}
}


// orig 0x211660
void CShotgun::Reload()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == PREFS_KeyLoopUpIntClamp("SHOTGUN", "ClipSize", 5, 0, 250))
		return;

	// don't reload until the last shell is in
	if (m_flNextReload > gpGlobals->time)
		return;

	// don't reload until recoil is done
	if (m_flNextPrimaryAttack > gpGlobals->time)
		return;

	// check to see if we're ready to reload
	if (m_fInSpecialReload == 0)
	{
		SendWeaponAnim(SHOTGUN_START_RELOAD);
		m_fInSpecialReload = 1;
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.6; // relative in halflife-updated (the player decrements it)
		m_flTimeWeaponIdle = gpGlobals->time + 0.6;
		m_flNextPrimaryAttack = gpGlobals->time + 1.0;
		m_flNextSecondaryAttack = gpGlobals->time + 1.0;
		return;
	}
	else if (m_fInSpecialReload == 1)
	{
		if (m_flTimeWeaponIdle > gpGlobals->time)
			return;
		// was waiting for gun to move to side
		m_fInSpecialReload = 2;

		if (RANDOM_LONG(0, 1))
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/reload1.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG(0, 0x1f));
		else
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/reload3.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG(0, 0x1f));

		SendWeaponAnim(SHOTGUN_RELOAD);

		m_flNextReload = PREFS_KeyLoopUpFloat("SHOTGUN", "ReloadDelay", 0.5) + gpGlobals->time;
		m_flTimeWeaponIdle = PREFS_KeyLoopUpFloat("SHOTGUN", "ReloadDelay", 0.5) + gpGlobals->time;
	}
	else
	{
		// Add them to the clip
		m_iClip += 1;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 1;
		m_fInSpecialReload = 1;
	}
}


// orig 0x2118f0. The pump sound is here, as in RC 1.9's SDK (halflife-updated moved it to
// ItemPostFrame, which plays it while the trigger is held too).
void CShotgun::WeaponIdle()
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	if (0 != m_flPumpTime && m_flPumpTime < gpGlobals->time)
	{
		// play pumping sound
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/scock1.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG(0, 0x1f));
		m_flPumpTime = 0;
	}

	if (m_flTimeWeaponIdle < gpGlobals->time)
	{
		if (m_iClip == 0 && m_fInSpecialReload == 0 && 0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			Reload();
		}
		else if (m_fInSpecialReload != 0)
		{
			if (m_iClip != PREFS_KeyLoopUpIntClamp("SHOTGUN", "ClipSize", 5, 0, 250) && 0 != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
			{
				Reload();
			}
			else
			{
				// reload debounce has timed out
				SendWeaponAnim(SHOTGUN_PUMP);

				// play cocking sound
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/scock1.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG(0, 0x1f));
				m_fInSpecialReload = 0;
				m_flTimeWeaponIdle = gpGlobals->time + 1.5;
			}
		}
		else
		{
			int iAnim;
			float flRand = RANDOM_FLOAT(0, 1);
			if (flRand <= 0.8)
			{
				iAnim = SHOTGUN_IDLE_DEEP;
				m_flTimeWeaponIdle = gpGlobals->time + (60.0 / 12.0); // * RANDOM_LONG(2, 5);
			}
			else if (flRand <= 0.95)
			{
				iAnim = SHOTGUN_IDLE;
				m_flTimeWeaponIdle = gpGlobals->time + (20.0 / 9.0);
			}
			else
			{
				iAnim = SHOTGUN_IDLE4;
				m_flTimeWeaponIdle = gpGlobals->time + (20.0 / 9.0);
			}
			SendWeaponAnim(iAnim);
		}
	}
}


class CShotgunAmmo : public CBasePlayerAmmo
{
	// orig 0x252bb0
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_shotbox.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x252b70
	void Precache() override
	{
		PRECACHE_MODEL("models/w_shotbox.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x252ac0
	bool AddAmmo(CBaseEntity* pOther) override
	{
		int iMax = PREFS_KeyLoopUpIntClamp("SHOTGUN", "MaxAmmo", 15, 0, 250);
		int iGive = PREFS_KeyLoopUpIntClamp("SHOTGUN", "BoxGive", 5, 0, 250);
		if (pOther->GiveAmmo(iGive, "buckshot", iMax) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_buckshot, CShotgunAmmo); // orig 0x211b40

// orig 0x211b70
void CShotgun::BotWeaponFrame(CBot* pBot, CBaseEntity* pEnemy)
{
	entvars_t* pevBot = RCBotEnt(pBot)->pev;

	(void)pEnemy->GetBotShootOrg(); // the original's aim point, unused

	if (m_iClip <= 0)
	{
		pevBot->button |= IN_RELOAD;
		return;
	}
	if (m_fInSpecialReload != 0)
		return;

	(void)pEnemy->GetBotShootOrg();
	if (RCBotFireGate(pBot) / 3.0 < 999.0)
		return;

	if ((pEnemy->Center() - RCBotEnt(pBot)->Center()).Length() <= 768)
		pevBot->button |= IN_FORWARD;

	if (BotFirePrimary(pBot, pEnemy))
	{
		pevBot->button |= IN_ATTACK;
		(void)pEnemy->GetBotShootOrg();
	}
}

// orig 0x211d00
int CShotgun::BotFireButton(CBot* pBot, CBaseEntity* pEnemy)
{
	if ((pEnemy->Center() - RCBotEnt(pBot)->Center()).Length() <= 768)
		RCBotEnt(pBot)->pev->button |= IN_FORWARD;
	return BotFirePrimary(pBot, pEnemy) ? 1 : 0;
}

// orig 0x211dc0
bool CShotgun::BotFireSecondary(CBot* pBot, CBaseEntity* pEnemy)
{
	return false;
}

// orig 0x211dd0: throw only when a toss from here can reach the enemy, from 256 to 1512 out
bool CShotgun::BotFirePrimary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (m_flBotNextThrow > gpGlobals->time)
		return false;
	if (!pEnemy)
		return true;

	float flDist = (pEnemy->Center() - RCBotEnt(pBot)->Center()).Length();

	if (m_flNextPrimaryAttack <= gpGlobals->time)
	{
		Vector vecTarget = pEnemy->GetBotShootOrg();
		Vector vecFrom = Center();
		Vector vecToss = VecCheckThrow(pev, vecFrom, vecTarget, 600, 0.75);
		if (vecToss == g_vecZero)
		{
			m_flBotNextThrow = gpGlobals->time + 0.75;
			return false;
		}
	}

	if (flDist > 256 && flDist < 1512)
		return true;
	return false;
}
