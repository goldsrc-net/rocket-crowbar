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

// Rocket Crowbar 1.9: the egon. Primary is a tractor beam that holds whatever it locks onto in
// front of the player (it does no damage); secondary fires an IsoBomb, an isotope box that arms
// on touch and blows up 6 seconds later, damaging through walls; reload throws an egon_mirv, an
// invisible rocket that splits on every bounce. Numbers come from rc.ini [EGON] and [ITEMS].
// Reconstructed from rc_i386.so (addresses are file addresses, reference/funcfile.tsv); see
// reference/dossiers/w2-shotgun-egon-crowbar.md.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "effects.h"
#include "customentity.h"
#include "gamerules.h"
#include "soundent.h"
#include "decals.h"
#include "UserMessages.h"
#include "Preff.h"

#ifdef CLIENT_DLL
#include "hud.h"
#include "com_weapons.h"
#endif

extern unsigned short FixedUnsigned16(float value, float scale); // util.cpp

// The RC cap the tractor beam can hold (HasRCCap). VERIFY name: the original has only the bit.
#define RC_CAP_TRACTOR_BEAM 0x10

int g_iTractorBeamSpr; // orig .bss 0x30d2c0: sprites/xbeam2.spr, precached and never used

// RC 1.9's SDK fire animations (orig .data 0x28dff0, 0x28e000)
static int g_fireAnims1[] = {EGON_FIRE1, EGON_FIRE2, EGON_FIRE3, EGON_FIRE4};
[[maybe_unused]] static int g_fireAnims2[] = {EGON_ALTFIRECYCLE}; // unused, as in the original

// Bring each component into [0, 360], the way the original's inlined loops do.
static void RCClampAngles360(Vector& vecAngles)
{
	for (int i = 0; i < 3; i++)
	{
		while (vecAngles[i] < 0)
			vecAngles[i] += 360;
		while (vecAngles[i] > 360)
			vecAngles[i] -= 360;
	}
}

//=========================================================
// CIsoBomb (IsoBomb): the egon's secondary. A box that arms when it touches something other than
// its thrower, glows from blue to red for 6 seconds, then blows up, damaging everything with
// takedamage in range that a short punch-through trace can reach.
//=========================================================
class CIsoBomb : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	edict_t* GetRealOwner() override;
	void EntIsBeingRemoved(edict_t* pEdict) override;
	virtual bool BotFirePrimary(CBot* pBot, CBaseEntity* pEnemy);
	virtual bool BotFireSecondary(CBot* pBot, CBaseEntity* pEnemy);
	virtual int GetBotWeight(CBot* pBot, CBaseEntity* pEnemy);

	static CIsoBomb* IsoCreate(edict_t* pOwner);
	bool PunchTrace(Vector vecStart, Vector vecEnd, float flDist, int iCount);
	void IsoRadus(Vector vecSrc);
	void IsoGoBoom();
	void UpdateBeams();
	void DoRingPulse();
	void DoArc();
	void InitBeams();
	void EXPORT IsoPostArmThink();
	void EXPORT ArmTouch(CBaseEntity* pOther);
	void EXPORT IsoBecomeSolid();

	CBeam* m_pMainBeam;		 // orig +0x280, the beam straight up. VERIFY names: unsaved, from use
	CBeam* m_pBeams[5];		 // orig +0x284, the random rays
	Vector m_vecBeamDir[5];	 // orig +0x298, each ray's offset from the origin
	edict_t* m_pRealOwner;	 // orig +0x2d4, the thrower (pev->owner is cleared once clear of them)
	float m_flNextRingTime;	 // orig +0x2d8
	float m_flRed;			 // orig +0x2dc, glow colour, rises to 255
	float m_flBlue;			 // orig +0x2e0, falls to 0
	float m_flLastThinkTime; // orig +0x2e4
	float m_flNextArcTime;	 // orig +0x2e8
};

LINK_ENTITY_TO_CLASS(IsoBomb, CIsoBomb); // orig 0x183920

// orig 0x183950
CIsoBomb* CIsoBomb::IsoCreate(edict_t* pOwner)
{
	CIsoBomb* pIso = GetClassPtr((CIsoBomb*)NULL);
	pIso->pev->classname = MAKE_STRING("egon");
	pIso->pev->owner = pOwner;
	pIso->m_pRealOwner = pOwner;
	pIso->Spawn();
	return pIso;
}

// orig 0x1839b0
void CIsoBomb::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_NO;
	pev->gravity = 0.75;
	SET_MODEL(ENT(pev), "models/w_isotopebox.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 32));
	m_flRed = 0;
	m_flBlue = 255;
	SetTouch(&CIsoBomb::ArmTouch);
	SetThink(&CIsoBomb::IsoBecomeSolid);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x183ad0. Empty: CEgon::Precache precaches the box.
void CIsoBomb::Precache()
{
}

// orig 0x183ae0. Trace from vecStart toward vecEnd, punching through up to iCount solid layers
// with flDist units of wall in all: true when a trace reaches vecEnd.
bool CIsoBomb::PunchTrace(Vector vecStart, Vector vecEnd, float flDist, int iCount)
{
	Vector vecPos = vecStart;
	Vector vecDir = (vecEnd - vecStart).Normalize();
	TraceResult tr, trBack;

	if (flDist < 0)
		return false;

	while (iCount > 0)
	{
		UTIL_TraceLine(vecPos, vecEnd, ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction == 1.0)
			return true;
		if (0 != tr.fAllSolid)
			return false;
		if (0 != tr.fStartSolid)
			return false;

		// trace back from flDist inside the wall to find its far side
		Vector vecIn = tr.vecEndPos + vecDir;
		Vector vecFar = tr.vecEndPos + vecDir * flDist;
		UTIL_TraceLine(vecFar, vecIn, ignore_monsters, ENT(pev), &trBack);
		if (0 != trBack.fAllSolid)
			return false;

		flDist -= (trBack.vecEndPos - tr.vecEndPos).Length();
		vecPos = trBack.vecEndPos + vecDir;
		iCount--;
		if (flDist < 0)
			return false;
	}
	return false;
}

// orig 0x183dd0. Damage and white-flash everything with takedamage within dmg * 2.5 of vecSrc
// that a punch trace from just above the box reaches.
void CIsoBomb::IsoRadus(Vector vecSrc)
{
	CBaseEntity* pEntity = NULL;
	float flRadius = pev->dmg * 2.5;

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, flRadius)) != NULL)
	{
		if (pEntity->pev->takedamage == DAMAGE_NO)
			continue;

		Vector vecStart = pev->origin + Vector(0, 0, 16);
		Vector vecEnd = pEntity->Center();
		if (!PunchTrace(vecStart, vecEnd, PREFS_KeyLoopUpFloat("EGON", "IsoBombMaxPunchDist", 52.0), 10))
			continue;

		if (m_pRealOwner)
			pEntity->TakeDamage(pev, VARS(m_pRealOwner), pev->dmg, DMG_BLAST);
		else
			pEntity->TakeDamage(pev, pev, pev->dmg, DMG_BLAST);

		UTIL_ScreenFade(pEntity, Vector(255, 255, 255), 2, 1, 255, 0);
	}
}

// orig 0x183f60
void CIsoBomb::IsoGoBoom()
{
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, -32), ignore_monsters, ENT(pev), &tr);

	pev->model = iStringNull;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	pev->takedamage = DAMAGE_NO;

	unsigned short usDuration = FixedUnsigned16(3, 1 << 12);
	unsigned short usFrequency = FixedUnsigned16(20, 1 << 8);
	unsigned short usAmplitude = FixedUnsigned16(400, 1 << 12);

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(255); // scale * 10
	WRITE_BYTE(15);	 // framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 32);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 32 + 4096.0);
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(0);	 // framerate
	WRITE_BYTE(10);	 // life
	WRITE_BYTE(30);	 // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(255); // r
	WRITE_BYTE(160); // g
	WRITE_BYTE(100); // b
	WRITE_BYTE(255); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	// a screen shake for everyone (UTIL_ScreenShake's message, unbounded)
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgShake);
	WRITE_SHORT(usAmplitude);
	WRITE_SHORT(usDuration);
	WRITE_SHORT(usFrequency);
	MESSAGE_END();

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0);

	if (RANDOM_FLOAT(0, 1) < 0.5)
		UTIL_DecalTrace(&tr, DECAL_SCORCH1);
	else
		UTIL_DecalTrace(&tr, DECAL_SCORCH2);

	(void)RANDOM_FLOAT(0, 1); // the SDK explosion's unused flRndSound
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

	IsoRadus(pev->origin + Vector(0, 0, 32));

	UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, 1024), ignore_monsters, ENT(pev), &tr);
	// Original bug, kept: adds the box's origin to the trace's end point (already a world
	// position), so this second blast is centred far from the box (dossier, "Original bugs").
	IsoRadus(pev->origin + tr.vecEndPos - Vector(0, 0, 32));

	pev->effects |= EF_NODRAW;
	SetThink(&CBaseEntity::SUB_Remove);
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x184550
void CIsoBomb::UpdateBeams()
{
	m_pMainBeam->SetColor((int)m_flRed, 0, (int)m_flBlue);
	m_pMainBeam->SetStartPos(pev->origin);
	m_pMainBeam->SetEndPos(pev->origin + Vector(0, 0, 1024));

	for (int i = 0; i < 5; i++)
	{
		m_pBeams[i]->SetColor((int)m_flRed, 0, (int)m_flBlue);
		m_pBeams[i]->SetStartPos(pev->origin);
		m_pBeams[i]->SetEndPos(pev->origin + m_vecBeamDir[i]);
	}
}

// orig 0x184740
void CIsoBomb::DoRingPulse()
{
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 16);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 16 + 2048.0);
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(0);	// startframe
	WRITE_BYTE(0);	// framerate
	WRITE_BYTE(10); // life
	WRITE_BYTE(20); // width
	WRITE_BYTE(10); // noise
	WRITE_BYTE((int)m_flRed);
	WRITE_BYTE(0);
	WRITE_BYTE((int)m_flBlue);
	WRITE_BYTE(255); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();
}

// orig 0x184940. A zap to wherever a random ray hits.
void CIsoBomb::DoArc()
{
	if (CVAR_GET_FLOAT("sv_lowlag") == 1)
		return;

	Vector vecAngles;
	vecAngles.y = RANDOM_FLOAT(0, 360);
	vecAngles.x = RANDOM_FLOAT(-180, 0);
	vecAngles.z = 0;
	RCClampAngles360(vecAngles);
	UTIL_MakeVectors(vecAngles);

	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * 4096, dont_ignore_monsters, ENT(pev), &tr);

	// Original bug, kept: MSG_PVS with no origin; the engine uses whatever origin the last
	// message set (dossier, "Original bugs").
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, NULL);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 16);
	WRITE_COORD(tr.vecEndPos.x);
	WRITE_COORD(tr.vecEndPos.y);
	WRITE_COORD(tr.vecEndPos.z);
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(128); // framerate
	WRITE_BYTE(2);	 // life
	WRITE_BYTE(20);	 // width
	WRITE_BYTE(128); // noise
	WRITE_BYTE(200); // r
	WRITE_BYTE(200); // g
	WRITE_BYTE(255); // b
	WRITE_BYTE(255); // brightness
	WRITE_BYTE(50);	 // speed
	MESSAGE_END();

	EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/zap3.wav", 1, ATTN_NORM);
}

// orig 0x184d00
void CIsoBomb::IsoPostArmThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime < gpGlobals->time)
	{
		STOP_SOUND(ENT(pev), CHAN_BODY, "weapons/egon_run3.wav");
		STOP_SOUND(ENT(pev), CHAN_VOICE, "debris/zap3.wav");
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/null.wav", 0, 0, 0, PITCH_NORM);
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/null.wav", 0, 0, 0, PITCH_NORM);
		IsoGoBoom();
		return;
	}

	// the windup ends, the hum starts (air_finished is ArmTouch's +3.8 s, -1 once done)
	if (pev->air_finished < gpGlobals->time && pev->air_finished != -1)
	{
		EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/egon_run3.wav", 1, ATTN_NORM);
		pev->air_finished = -1;
	}

	// shift blue to red over the 6 seconds
	float flStep = 255.0 / (6.0 / (gpGlobals->time - m_flLastThinkTime));
	m_flRed += flStep;
	m_flBlue -= flStep;
	if (m_flRed > 255)
		m_flRed = 255;
	if (m_flBlue < 0)
		m_flBlue = 0;

	UpdateBeams();
	pev->rendercolor = Vector((int)m_flRed, 0, (int)m_flBlue);
	m_flLastThinkTime = gpGlobals->time;

	if (m_flNextRingTime < gpGlobals->time)
	{
		DoRingPulse();
		m_flNextRingTime = RANDOM_FLOAT(0.3, 0.6) + gpGlobals->time;
	}

	if (m_flNextArcTime < gpGlobals->time)
	{
		DoArc();
		m_flNextArcTime = RANDOM_FLOAT(0.4, 0.8) + gpGlobals->time;
	}
}

// orig 0x184fd0. One beam straight up and five random rays; each removes itself after 6.2 s.
void CIsoBomb::InitBeams()
{
	m_pMainBeam = CBeam::BeamCreate(g_pModelNameLaser, 60);
	m_pMainBeam->SetBrightness(255);
	m_pMainBeam->SetColor((int)m_flRed, 0, (int)m_flBlue);
	m_pMainBeam->SetNoise(10);
	m_pMainBeam->SetScrollRate(128);
	m_pMainBeam->SetStartPos(pev->origin);
	m_pMainBeam->SetEndPos(pev->origin + Vector(0, 0, 1024));
	m_pMainBeam->SetThink(&CBaseEntity::SUB_Remove);
	m_pMainBeam->pev->nextthink = gpGlobals->time + 6.2;
	m_pMainBeam->SetFlags(BEAM_FSHADEOUT);

	for (int i = 0; i < 5; i++)
	{
		Vector vecAngles;
		vecAngles.y = RANDOM_FLOAT(0, 360);
		vecAngles.x = RANDOM_FLOAT(-180, 0);
		vecAngles.z = 0;
		RCClampAngles360(vecAngles);
		UTIL_MakeVectors(vecAngles);
		m_vecBeamDir[i] = gpGlobals->v_forward * RANDOM_FLOAT(768, 1024);

		m_pBeams[i] = CBeam::BeamCreate(g_pModelNameLaser, 20);
		m_pBeams[i]->SetBrightness(255);
		m_pBeams[i]->SetColor((int)m_flRed, 0, (int)m_flBlue);
		m_pBeams[i]->SetNoise(50);
		m_pBeams[i]->SetScrollRate(128);
		m_pBeams[i]->SetStartPos(pev->origin);
		m_pBeams[i]->SetEndPos(pev->origin + m_vecBeamDir[i]);
		m_pBeams[i]->SetThink(&CBaseEntity::SUB_Remove);
		m_pBeams[i]->pev->nextthink = gpGlobals->time + 6.2;
		m_pBeams[i]->SetFlags(BEAM_FSHADEOUT);
	}
}

// orig 0x185520. Arm on the first touch that isn't the thrower.
void CIsoBomb::ArmTouch(CBaseEntity* pOther)
{
	if (pev->owner && pOther && pOther->edict() == pev->owner)
		return;

	InitBeams();
	m_flNextRingTime = RANDOM_FLOAT(0.3, 0.6) + gpGlobals->time;
	m_flNextArcTime = RANDOM_FLOAT(0.4, 0.8) + gpGlobals->time;
	EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/egon_windup2.wav", 1, ATTN_NORM);
	pev->air_finished = gpGlobals->time + 3.8;
	SetThink(&CIsoBomb::IsoPostArmThink);
	pev->nextthink = gpGlobals->time + 0.1;
	SetTouch(NULL);
	m_flLastThinkTime = gpGlobals->time;
	pev->renderfx = kRenderFxGlowShell;
	pev->rendercolor = Vector((int)m_flRed, 0, (int)m_flBlue);
	pev->renderamt = 64;
	pev->dmgtime = gpGlobals->time + 6.0;
	CSoundEnt::InsertSound(bits_SOUND_DANGER, pev->origin, NORMAL_EXPLOSION_VOLUME, 6.0);
}

// orig 0x185710. Until armed: clear pev->owner (so the thrower can touch it) once the box is
// more than 64 units from them and not inside anything.
void CIsoBomb::IsoBecomeSolid()
{
	// VERIFY: guard, the original crashes here: m_pRealOwner is NULL once the thrower
	// disconnected (EntIsBeingRemoved). With no owner the box ends up ownerless either way.
	float flDist = m_pRealOwner ? (pev->origin - m_pRealOwner->v.origin).Length() : 0;
	pev->nextthink = gpGlobals->time + 0.3;

	if (pev->owner)
	{
		pev->owner = NULL;
		TraceResult tr;
		UTIL_TraceHull(pev->origin, pev->origin, dont_ignore_monsters, head_hull, ENT(pev), &tr);
		if (!tr.pHit && flDist > 64)
		{
			pev->owner = NULL;
			return;
		}
		pev->owner = m_pRealOwner;
	}
}

// orig 0x187670
edict_t* CIsoBomb::GetRealOwner()
{
	return m_pRealOwner;
}

// orig 0x187680
void CIsoBomb::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pRealOwner && m_pRealOwner == pEdict)
		m_pRealOwner = NULL;
}

// orig 0x1876b0
int CIsoBomb::GetBotWeight(CBot* pBot, CBaseEntity* pEnemy)
{
	return 1;
}

// orig 0x1876c0
bool CIsoBomb::BotFireSecondary(CBot* pBot, CBaseEntity* pEnemy)
{
	return false;
}

// orig 0x1876d0
bool CIsoBomb::BotFirePrimary(CBot* pBot, CBaseEntity* pEnemy)
{
	return true;
}

//=========================================================
// CEgonMirv (egon_mirv): the egon's reload. An invisible rocket with a trail that spits debris
// in flight and, on each touch, explodes and splits into a smaller frag (pev->team is the frag
// generation, ReloadMaxFrags for the first) until the last one.
//=========================================================
class CEgonMirv : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce) override;
	virtual bool ProMagEffected(CBaseEntity* pMagnet);

	void EXPORT MFTouch(CBaseEntity* pOther);
	void EXPORT MFThink();

	int m_iTrail;		 // orig +0x280, sprites/smoke.spr
	int m_iFlare;		 // orig +0x284, sprites/blueflare1.spr
	float m_flSpitTime;	 // orig +0x288, next debris spit
	Vector m_vecTossTo;	 // orig +0x28c, the magnet a frag is tossed to
	bool m_bInToss;		 // orig +0x298, a projectile magnet has pulled this one
};

LINK_ENTITY_TO_CLASS(egon_mirv, CEgonMirv); // orig 0x181b90

TYPEDESCRIPTION CEgonMirv::m_SaveData[] =
	{
		DEFINE_FIELD(CEgonMirv, m_flSpitTime, FIELD_TIME),
		DEFINE_FIELD(CEgonMirv, m_vecTossTo, FIELD_POSITION_VECTOR),
		DEFINE_FIELD(CEgonMirv, m_bInToss, FIELD_BOOLEAN),
};
IMPLEMENT_SAVERESTORE(CEgonMirv, CGrenade); // orig 0x181bc0, 0x181c10

// orig 0x181c60
void CEgonMirv::Precache()
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	m_iFlare = PRECACHE_MODEL("sprites/blueflare1.spr");
	PRECACHE_SOUND("weapons/rocket1.wav");
}

// The frag's place in its line: 1 for the first (pev->team == ReloadMaxFrags), 0 for the last.
// Inlined in the original at every use, each with its own PREFS read.
static float MirvFragFraction(entvars_t* pev, int iMaxFrags)
{
	return (pev->team - 1.0f) / (iMaxFrags - 1.0f);
}

// orig 0x181cd0
void CEgonMirv::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);
	pev->rendermode = kRenderTransColor;
	pev->renderamt = 0;
	pev->classname = MAKE_STRING("egon");
	pev->gravity = PREFS_KeyLoopUpFloat("EGON", "ReloadGravity", 0.8);
	pev->effects |= EF_LIGHT;
	SetThink(&CEgonMirv::MFThink);
	SetTouch(&CEgonMirv::MFTouch);
	m_bInToss = false;
	pev->nextthink = gpGlobals->time + 0.1;

	float flMaxDamage = PREFS_KeyLoopUpFloat("EGON", "ReloadFragMaxMaxDamage", 130.0);
	float flMinDamage = PREFS_KeyLoopUpFloat("EGON", "ReloadFragMaxMinDamage", 95.0);
	int iMaxFrags = PREFS_KeyLoopUpInt("EGON", "ReloadMaxFrags", 4);
	float flBase = PREFS_KeyLoopUpFloat("EGON", "ReloadFragMaxMinDamage", 95.0);
	pev->dmg = MirvFragFraction(pev, iMaxFrags) * (flMaxDamage - flMinDamage) + flBase;
	pev->dmgtime = gpGlobals->time + 30.0;
	m_flSpitTime = RANDOM_FLOAT(0.08, 0.1) + gpGlobals->time;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex()); // entity
	WRITE_SHORT(m_iTrail);	 // model
	iMaxFrags = PREFS_KeyLoopUpInt("EGON", "ReloadMaxFrags", 4);
	WRITE_BYTE((int)(30.0f - MirvFragFraction(pev, iMaxFrags) * 10.0f)); // life
	iMaxFrags = PREFS_KeyLoopUpInt("EGON", "ReloadMaxFrags", 4);
	WRITE_BYTE((int)(6.0f - MirvFragFraction(pev, iMaxFrags) * 2.0f)); // width
	WRITE_BYTE(100);
	WRITE_BYTE(100);
	WRITE_BYTE(255);
	iMaxFrags = PREFS_KeyLoopUpInt("EGON", "ReloadMaxFrags", 4);
	WRITE_BYTE((int)(255.0f - MirvFragFraction(pev, iMaxFrags) * 64.0f)); // brightness
	MESSAGE_END();
}

// orig 0x182120
void CEgonMirv::MFTouch(CBaseEntity* pOther)
{
	TraceResult tr = UTIL_GetGlobalTrace();

	if (pOther && pOther->edict() == pev->owner)
		return;

	// reflect off the surface, then a 20 degree cone. The cone uses whatever gpGlobals->v_up and
	// v_right the last UTIL_MakeVectors left (the original never sets them here).
	Vector vecDir = pev->velocity.Normalize();
	vecDir = (vecDir + (tr.vecPlaneNormal * 2) * -DotProduct(tr.vecPlaneNormal, vecDir)).Normalize();

	float x, y, z;
	do
	{
		x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
		y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
		z = x * x + y * y;
	} while (z > 1);

	Vector vecShot = vecDir + gpGlobals->v_up * x * VECTOR_CONE_20DEGREES.x + gpGlobals->v_right * y * VECTOR_CONE_20DEGREES.y;

	if (pev->team > 1)
	{
		if (!m_bInToss)
		{
			CEgonMirv* pFrag = GetClassPtr((CEgonMirv*)NULL);
			pFrag->pev->team = pev->team - 1;
			pFrag->Spawn();
			pFrag->pev->owner = pev->owner;
			UTIL_SetOrigin(pFrag->pev, pev->origin + tr.vecPlaneNormal * 4);

			float flMax = PREFS_KeyLoopUpFloat("EGON", "ReloadFragMaxVelo", 1000.0);
			float flMin = PREFS_KeyLoopUpFloat("EGON", "ReloadFragMinVelo", 600.0);
			int iMaxFrags = PREFS_KeyLoopUpInt("EGON", "ReloadMaxFrags", 4);
			float flBase = PREFS_KeyLoopUpFloat("EGON", "ReloadFragMinVelo", 600.0);
			pFrag->pev->velocity = vecShot.Normalize() * (MirvFragFraction(pev, iMaxFrags) * (flMax - flMin) + flBase);
		}
		else
		{
			float flMax = PREFS_KeyLoopUpFloat("EGON", "ReloadFragMaxVelo", 1000.0);
			float flMin = PREFS_KeyLoopUpFloat("EGON", "ReloadFragMinVelo", 600.0);
			int iMaxFrags = PREFS_KeyLoopUpInt("EGON", "ReloadMaxFrags", 4);
			float flBase = PREFS_KeyLoopUpFloat("EGON", "ReloadFragMinVelo", 600.0);
			float flSpeed = MirvFragFraction(pev, iMaxFrags) * (flMax - flMin) + flBase;

			// toss the frag at the magnet that pulled this one
			Vector vecToss = VecCheckThrow(pev, pev->origin + tr.vecPlaneNormal * 4, m_vecTossTo, flSpeed, pev->gravity);

			CEgonMirv* pFrag = GetClassPtr((CEgonMirv*)NULL);
			pFrag->pev->team = pev->team - 1;
			pFrag->Spawn();
			pFrag->pev->owner = pev->owner;
			UTIL_SetOrigin(pFrag->pev, pev->origin + tr.vecPlaneNormal * 4);
			if (vecToss != g_vecZero)
				pFrag->pev->velocity = vecToss;
			else
				pFrag->pev->velocity = vecShot.Normalize() * flSpeed;
		}
	}

	if (CVAR_GET_FLOAT("sv_lowlag") != 1)
	{
		// up to three debris bits off the surface, while fewer than 26 exist
		for (int i = 0; i <= 2; i++)
		{
			if (CountEntsInWorld("flying_expel") > 25)
				break;

			CBaseEntity* pDebris = GetClassPtr((CBaseEntity*)NULL);
			pDebris->Spawn();
			pDebris->pev->classname = MAKE_STRING("flying_expel");
			pDebris->pev->movetype = MOVETYPE_TOSS;
			pDebris->pev->solid = SOLID_BBOX;
			pDebris->pev->renderamt = 0;
			pDebris->pev->gravity = 0.2;
			pDebris->pev->rendermode = kRenderTransColor;
			pDebris->SetTouch(&CBaseEntity::RemoveTouch);
			pDebris->SetThink(&CBaseEntity::SUB_Remove);
			pDebris->pev->nextthink = gpGlobals->time + 5;
			SET_MODEL(pDebris->edict(), "models/rpgrocket.mdl");
			UTIL_SetOrigin(pDebris->pev, pev->origin);
			UTIL_SetSize(pDebris->pev, g_vecZero, g_vecZero);
			pDebris->pev->owner = edict();

			do
			{
				x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
				y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
				z = x * x + y * y;
			} while (z > 1);

			Vector vecDebris = tr.vecPlaneNormal + gpGlobals->v_up * x * VECTOR_CONE_20DEGREES.x * 3 + gpGlobals->v_right * y * VECTOR_CONE_20DEGREES.y * 3;
			float flRand = RANDOM_FLOAT(700, 800);
			int iMaxFrags = PREFS_KeyLoopUpInt("EGON", "ReloadMaxFrags", 4);
			pDebris->pev->velocity = vecDebris * (flRand - MirvFragFraction(pev, iMaxFrags) * 400);

			MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_BEAMFOLLOW);
			WRITE_SHORT(ENTINDEX(pDebris->edict())); // entity
			WRITE_SHORT(g_sModelIndexLaser);		  // model
			WRITE_BYTE(3);							  // life
			WRITE_BYTE(1);							  // width
			WRITE_BYTE(120);
			WRITE_BYTE(120);
			WRITE_BYTE(255);
			WRITE_BYTE(128); // brightness
			MESSAGE_END();
		}

		// a burst of blue sparks off the surface
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_SPRITETRAIL);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_COORD(pev->origin.x + tr.vecPlaneNormal.x);
		WRITE_COORD(pev->origin.y + tr.vecPlaneNormal.y);
		WRITE_COORD(pev->origin.z + tr.vecPlaneNormal.z);
		WRITE_SHORT(m_iFlare);
		int iMaxFrags = PREFS_KeyLoopUpInt("EGON", "ReloadMaxFrags", 4);
		WRITE_BYTE((int)(15.0f - MirvFragFraction(pev, iMaxFrags) * 10.0f)); // count
		WRITE_BYTE(10);															// life
		WRITE_BYTE(3);															// scale
		WRITE_BYTE(64);															// velocity
		iMaxFrags = PREFS_KeyLoopUpInt("EGON", "ReloadMaxFrags", 4);
		WRITE_BYTE((int)(6.0f - MirvFragFraction(pev, iMaxFrags) * 2.0f)); // randomness
		MESSAGE_END();
	}

	ExplodeTouch(pOther);
}

// orig 0x1832d0. Spit a debris bit backwards every 0.12-0.24 s; remove after 30 s.
void CEgonMirv::MFThink()
{
	if (pev->dmgtime < gpGlobals->time)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	if (m_flSpitTime < gpGlobals->time && CountEntsInWorld("flying_expel") < 26 && CVAR_GET_FLOAT("sv_lowlag") != 1)
	{
		CBaseEntity* pDebris = GetClassPtr((CBaseEntity*)NULL);
		pDebris->Spawn();
		pDebris->pev->classname = MAKE_STRING("flying_expel");
		pDebris->pev->movetype = MOVETYPE_TOSS;
		pDebris->pev->solid = SOLID_BBOX;
		pDebris->pev->renderamt = 0;
		pDebris->pev->gravity = 0.2;
		pDebris->pev->rendermode = kRenderTransColor;
		pDebris->SetTouch(&CBaseEntity::RemoveTouch);
		pDebris->SetThink(&CBaseEntity::SUB_Remove);
		pDebris->pev->nextthink = gpGlobals->time + 5.0;
		SET_MODEL(pDebris->edict(), "models/rpgrocket.mdl");
		UTIL_SetOrigin(pDebris->pev, pev->origin);
		UTIL_SetSize(pDebris->pev, g_vecZero, g_vecZero);
		pDebris->pev->owner = edict();

		float x, y, z;
		do
		{
			x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
			y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
			z = x * x + y * y;
		} while (z > 1);

		// backwards along the flight, in a cone (stale v_up/v_right, as in MFTouch)
		Vector vecDir = pev->velocity.Normalize();
		Vector vecUp = gpGlobals->v_up * x * VECTOR_CONE_20DEGREES.x;
		Vector vecRight = gpGlobals->v_right * y * VECTOR_CONE_20DEGREES.y;
		Vector vecSpit = -vecDir + vecUp * 2 + vecRight * 2;
		pDebris->pev->velocity = vecSpit * RANDOM_FLOAT(600, 800);

		MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(TE_BEAMFOLLOW);
		WRITE_SHORT(ENTINDEX(pDebris->edict())); // entity
		WRITE_SHORT(g_sModelIndexLaser);		  // model
		WRITE_BYTE(3);							  // life
		WRITE_BYTE(1);							  // width
		WRITE_BYTE(120);
		WRITE_BYTE(120);
		WRITE_BYTE(255);
		WRITE_BYTE(128); // brightness
		MESSAGE_END();

		m_flSpitTime = RANDOM_FLOAT(0.12, 0.24) + gpGlobals->time;
	}

	pev->nextthink = gpGlobals->time + 0.01;
}

// orig 0x187730. A projectile magnet doesn't pull the mirv; its next frag is tossed at it.
void CEgonMirv::ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce)
{
	m_vecTossTo = pMagnet->Center();
	m_bInToss = true;
}

// orig 0x187790
bool CEgonMirv::ProMagEffected(CBaseEntity* pMagnet)
{
	return pMagnet->edict() != pev->owner;
}

//=========================================================
// CEgon
//=========================================================
LINK_ENTITY_TO_CLASS(weapon_egon, CEgon); // orig 0x185800

// orig 0x1858d0
void CEgon::Spawn()
{
	Precache();
	m_iId = WEAPON_EGON;
	SET_MODEL(ENT(pev), "models/w_egon.mdl");

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("EGON", "PickupAmmo", 40, 0, 250);

	FallInit(); // get ready to fall down.
}

// orig 0x185950
void CEgon::ItemPreFrame()
{
	// Original oddity, kept: a NULL m_pPlayer is still read on the next line
	if (!m_pPlayer)
		DestroyEffect();
	if (!m_pPlayer->IsAlive())
		EndAttack();
}

// orig 0x1859a0. No events: RC 1.9's server plays the egon's sounds itself.
void CEgon::Precache()
{
	PRECACHE_MODEL("models/w_egon.mdl");
	PRECACHE_MODEL("models/v_egon.mdl");
	PRECACHE_MODEL("models/p_egon.mdl");

	PRECACHE_MODEL("models/w_9mmclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND(EGON_SOUND_OFF);
	PRECACHE_SOUND(EGON_SOUND_RUN);
	PRECACHE_SOUND(EGON_SOUND_STARTUP);

	PRECACHE_MODEL(EGON_BEAM_SPRITE);
	PRECACHE_MODEL(EGON_FLARE_SPRITE);

	PRECACHE_SOUND("weapons/357_cock1.wav");

	PRECACHE_MODEL("models/hvr.mdl");
	PRECACHE_SOUND("debris/beamstart8.wav");
	PRECACHE_SOUND("debris/zap3.wav");
	g_iTractorBeamSpr = PRECACHE_MODEL("sprites/xbeam2.spr");
	PRECACHE_MODEL("models/w_isotopebox.mdl");
	UTIL_PrecacheOther("egon_mirv");
	PRECACHE_SOUND("weapons/glauncher.wav");
	PRECACHE_SOUND("weapons/glauncher2.wav");
}

// orig 0x185b10. Lock the tractor beam on an entity with the tractor cap.
void CEgon::StartTractor(CBaseEntity* pEntity)
{
	if (!pEntity || !pEntity->IsAlive())
		return;

	int iCap = RC_CAP_TRACTOR_BEAM;
	if (!pEntity->HasRCCap(iCap))
		return;

	m_pTrackEnt = pEntity;
	m_flEntDist = (pEntity->Center() - m_pPlayer->Center()).Length();
	pEntity->pev->flags &= ~FL_ONGROUND;
	m_flTractTime = gpGlobals->time;
}

// orig 0x185c00
void CEgon::StopTractor()
{
	if (m_pTrackEnt)
	{
		m_pTrackEnt = NULL;
		m_flEntDist = 0;
	}
}

// orig 0x185c50. Every 0.05 s, set the held entity's velocity to bring it to m_flEntDist in front
// of the player; let go when it dies or would have to move faster than TractorMaxVel.
void CEgon::UpdateTractor()
{
	if (!m_pTrackEnt)
		return;

	if (FNullEnt(m_pTrackEnt->edict()))
	{
		m_pTrackEnt = NULL;
		m_flEntDist = 0;
		return;
	}

	if (m_pTrackEnt->IsAlive())
	{
		if (m_flTractTime > gpGlobals->time)
			return;
		m_flTractTime = gpGlobals->time + 0.05;

		UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

		// the original works out the angle from the player to the entity and never uses it
		Vector vecAngles = UTIL_VecToAngles(m_pPlayer->Center() - m_pTrackEnt->Center());
		(void)(vecAngles - (m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle));

		Vector vecTarget = m_pPlayer->Center() + gpGlobals->v_forward * m_flEntDist;
		Vector vecVelocity = (vecTarget - m_pTrackEnt->Center()) / 0.05f;
		if (vecVelocity.Length() <= PREFS_KeyLoopUpFloat("EGON", "TractorMaxVel", 1600.0))
		{
			m_pTrackEnt->pev->velocity = vecVelocity;
			m_pTrackEnt->pev->flags &= ~FL_ONGROUND;
			return;
		}
	}

	StopTractor();
}

// orig 0x185f60. Throw an IsoBomb carrying all the player's cells.
void CEgon::SecondaryAttack()
{
	if (m_pPlayer->pev->waterlevel != 3 && m_pPlayer->m_fIsoBombTime <= gpGlobals->time && CVAR_GET_FLOAT("mp_allowiso") == 1)
	{
		CBasePlayer* pPlayer = m_pPlayer;
		int iAmmoType = m_iPrimaryAmmoType;
		if (pPlayer->m_rgAmmo[iAmmoType] >= PREFS_KeyLoopUpInt("EGON", "IsoMinAmmo", 100) && CountEntsInWorld("egon") < 5)
		{
			EndAttack();
			m_flNextPrimaryAttack = PREFS_KeyLoopUpFloat("EGON", "IsoRefire", 1.5) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;
			m_flNextSecondaryAttack = PREFS_KeyLoopUpFloat("EGON", "IsoRefire", 1.5) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;
			m_flTimeWeaponIdle = gpGlobals->time + 3.0;

			UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
			Vector vecSrc = m_pPlayer->GetGunPosition();
			m_pPlayer->pev->punchangle.x -= PREFS_KeyLoopUpFloat("EGON", "IsoAngleKick", 8.0);

			CIsoBomb* pIso = CIsoBomb::IsoCreate(m_pPlayer->edict());
			UTIL_SetOrigin(pIso->pev, vecSrc);
			pIso->pev->velocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("EGON", "IsoFireSpeed", 500.0);
			pIso->pev->velocity.z += 100;
			pIso->pev->angles = UTIL_VecToAngles(pIso->pev->velocity);
			pIso->pev->angles.x = 0;
			pIso->pev->angles.z = 0;
			pIso->pev->dmg = PREFS_KeyLoopUpFloat("EGON", "IsoDamagePerCell", 10.0) * m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;

			if (RANDOM_FLOAT(0, 1) > 0.5)
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/glauncher2.wav", 1, ATTN_NORM);
			else
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/glauncher.wav", 1, ATTN_NORM);

			m_pPlayer->m_fIsoBombTime = PREFS_KeyLoopUpInt("EGON", "IsoNextCreateDelay", 45) + gpGlobals->time;
			return;
		}
	}

	PlayEmptySound();
	m_flNextPrimaryAttack = gpGlobals->time + 0.2;
	m_flNextSecondaryAttack = gpGlobals->time + 0.2;
}

// orig 0x186370. Unlike halflife-updated, does not reset m_fireState.
bool CEgon::Deploy()
{
	m_deployed = 0;
	return DefaultDeploy("models/v_egon.mdl", "models/p_egon.mdl", EGON_DRAW, "egon");
}

// orig 0x1863c0. The original sends WeapPickup when the base AddToPlayer succeeds, which it
// always does there.
void CEgon::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	// As the original: this weapon tells the player it was picked up (AddPlayerItem doesn't)
	MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
	WRITE_BYTE(m_iId);
	MESSAGE_END();
}

// orig 0x186450
void CEgon::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(EGON_HOLSTER);

	if (m_fireState != FIRE_OFF)
		EndAttack();
	DestroyEffect();
}

// orig 0x1864c0
bool CEgon::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("ITEMS", "CellsMax", 200, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_EGON;
	p->iFlags = 0;
	p->iWeight = 18; // RC 1.9's EGON_WEIGHT (halflife-updated's is 20)

	return true;
}

#define EGON_PULSE_INTERVAL 0.1
#define EGON_DISCHARGE_INTERVAL 0.1

// orig 0x186570. The original asks IsMultiplayer() and returns 0.1 either way.
float CEgon::GetPulseInterval()
{
	(void)g_pGameRules->IsMultiplayer();
	return EGON_PULSE_INTERVAL;
}

// orig 0x1865a0
float CEgon::GetDischargeInterval()
{
	(void)g_pGameRules->IsMultiplayer();
	return EGON_DISCHARGE_INTERVAL;
}

// orig 0x1875e0
bool CEgon::HasAmmo()
{
	return m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0;
}

// orig 0x187590. No busters exception (halflife-updated's IsBustingGame postdates RC).
void CEgon::UseAmmo(int count)
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= count)
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= count;
	else
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;
}

// orig 0x1865d0. RC 1.9's SDK version: the windup and run sounds play here (no events).
void CEgon::Attack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		if (m_pBeam)
			EndAttack();
		else
			PlayEmptySound();
		return;
	}

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition();

	switch (m_fireState)
	{
	case FIRE_OFF:
	{
		if (!HasAmmo())
		{
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 0.25;
			PlayEmptySound();
			return;
		}

		m_flAmmoUseTime = gpGlobals->time; // start using ammo ASAP.

		SendWeaponAnim(g_fireAnims1[RANDOM_LONG(0, 3)]);
		m_shakeTime = 0;

		m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;
		m_flTimeWeaponIdle = gpGlobals->time + 0.1;
		m_shootTime = gpGlobals->time + 2;

		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, EGON_SOUND_STARTUP, 0.98, ATTN_NORM, 0, 125);

		pev->dmgtime = gpGlobals->time + 0.1;
		m_fireState = FIRE_CHARGE;
	}
	break;

	case FIRE_CHARGE:
	{
		Fire(vecSrc, vecAiming);
		m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;

		if (m_shootTime != 0 && m_shootTime < gpGlobals->time)
		{
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_STATIC, EGON_SOUND_RUN, 0.98, ATTN_NORM, 0, 125);
			m_shootTime = 0;
		}

		if (!HasAmmo())
		{
			EndAttack();
			m_fireState = FIRE_OFF;
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 1.0;
		}
	}
	break;
	}
}

// orig 0x1868e0
void CEgon::PrimaryAttack()
{
	m_fireMode = FIRE_WIDE;
	Attack();
}

// orig 0x186910. RC 1.9's egon does no damage: it eats a cell every NoTargetAmmoEatTime
// (TargetAmmoEatTime while holding something) and locks the tractor beam on what it hits.
void CEgon::Fire(const Vector& vecOrigSrc, const Vector& vecDir)
{
	Vector vecDest = vecOrigSrc + vecDir * 4096;
	edict_t* pentIgnore = m_pPlayer->edict();
	Vector tmpSrc = vecOrigSrc + gpGlobals->v_up * -8 + gpGlobals->v_right * 3;

	// VERIFY: the original's screen shake below reads tr.vecEndPos before the trace fills it
	// (stack garbage, an original bug); ours is zeroed, so the shake is centred on the world origin
	TraceResult tr = {};

	if (pev->dmgtime < gpGlobals->time)
	{
		if (!m_pPlayer->IsAlive())
			return;

		if (gpGlobals->time >= m_flAmmoUseTime)
		{
			if (!m_pTrackEnt)
			{
				UseAmmo(1);
				m_flAmmoUseTime = PREFS_KeyLoopUpFloat("EGON", "NoTargetAmmoEatTime", 0.4) + gpGlobals->time;
			}
			else
			{
				UseAmmo(1);
				m_flAmmoUseTime = PREFS_KeyLoopUpFloat("EGON", "TargetAmmoEatTime", 0.2) + gpGlobals->time;
			}
		}

		pev->dmgtime = GetDischargeInterval() + gpGlobals->time;
		if (m_shakeTime < gpGlobals->time)
		{
			UTIL_ScreenShake(tr.vecEndPos, 5.0, 150.0, 0.75, 250.0);
			m_shakeTime = gpGlobals->time + 1.5;
		}
	}

	float timedist = (pev->dmgtime - gpGlobals->time) / GetDischargeInterval();
	if (timedist < 0)
		timedist = 0;
	else if (timedist > 1)
		timedist = 1;

	if (m_pTrackEnt)
	{
		UpdateEffect(tmpSrc, vecOrigSrc + vecDir * m_flEntDist, 1 - timedist);
	}
	else
	{
		UTIL_TraceLine(vecOrigSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr);

		if (0 != tr.fAllSolid)
			return;

		UpdateEffect(tmpSrc, tr.vecEndPos, 1 - timedist);

		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
		if (pEntity == NULL)
			return;

		int iCap = RC_CAP_TRACTOR_BEAM;
		if (!pEntity->HasRCCap(iCap))
			return;
		if (!g_pGameRules->CanPlayerAnnoy(m_pPlayer->pev, pEntity->pev))
			return;

		StartTractor(pEntity);
	}

	UpdateTractor();
}


// orig 0x186d30. Green beam while searching, red while holding. timeBlend is unused.
void CEgon::UpdateEffect(const Vector& startPoint, const Vector& endPoint, float timeBlend)
{
	if (!m_pBeam)
	{
		CreateEffect();
	}

	if (!m_pTrackEnt)
		m_pBeam->pev->rendercolor = Vector(30, 255, 40);
	else
		m_pBeam->pev->rendercolor = Vector(255, 20, 30);

	// Original oddity, kept: links each beam at startPoint, then moves its origin (the point end of
	// a point-entity beam) to endPoint without relinking.
	UTIL_SetOrigin(m_pBeam->pev, startPoint);
	UTIL_SetOrigin(m_pNoise->pev, startPoint);
	m_pBeam->pev->origin = endPoint;
	m_pNoise->pev->origin = endPoint;
}

// orig 0x186e30. Two beams from the gun attachment, no flare sprite.
void CEgon::CreateEffect()
{
	DestroyEffect();

	m_pBeam = CBeam::BeamCreate(EGON_BEAM_SPRITE, 20);
	m_pBeam->PointEntInit(pev->origin, m_pPlayer->entindex());
	m_pBeam->SetFlags(BEAM_FSINE);
	m_pBeam->SetEndAttachment(1);
	m_pBeam->SetBrightness(255);
	m_pBeam->SetScrollRate(128);
	m_pBeam->pev->rendercolor = Vector(30, 255, 40);
	m_pBeam->SetNoise(20);
	m_pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY; // Flag these to be destroyed on save/restore or level transition

	m_pNoise = CBeam::BeamCreate(g_pModelNameLaser, 25);
	m_pNoise->PointEntInit(pev->origin, m_pPlayer->entindex());
	m_pNoise->SetScrollRate(25);
	m_pNoise->SetBrightness(255);
	m_pNoise->SetEndAttachment(1);
	m_pNoise->pev->rendercolor = Vector(220, 220, 255);
	m_pNoise->SetNoise(64);
	m_pNoise->pev->spawnflags |= SF_BEAM_TEMPORARY;
}


// orig 0x187020
void CEgon::DestroyEffect()
{
	if (m_pBeam)
	{
		UTIL_Remove(m_pBeam);
		m_pBeam = NULL;
	}
	if (m_pNoise)
	{
		UTIL_Remove(m_pNoise);
		m_pNoise = NULL;
	}
	StopTractor();
}


// orig 0x187090. Called only with no attack button held (RC's CEgon has no ShouldWeaponIdle).
void CEgon::WeaponIdle()
{
	ResetEmptySound();

	if (!m_pPlayer->IsAlive())
		DestroyEffect();

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_fireState != FIRE_OFF)
		EndAttack();

	int iAnim;

	float flRand = RANDOM_FLOAT(0, 1);

	if (flRand <= 0.5)
	{
		iAnim = EGON_IDLE1;
		m_flTimeWeaponIdle = RANDOM_FLOAT(10, 15) + gpGlobals->time;
	}
	else
	{
		iAnim = EGON_FIDGET1;
		m_flTimeWeaponIdle = gpGlobals->time + 3;
	}

	SendWeaponAnim(iAnim);
	m_deployed = 1;
}

// orig 0x187190
void CEgon::EndAttack()
{
	STOP_SOUND(ENT(m_pPlayer->pev), CHAN_STATIC, EGON_SOUND_RUN);
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, EGON_SOUND_OFF, 0.98, ATTN_NORM, 0, 100);

	m_fireState = FIRE_OFF;
	m_flTimeWeaponIdle = gpGlobals->time + 2.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_pPlayer->m_fCurseRefireScale * 0.5 + gpGlobals->time;

	DestroyEffect();
}

// orig 0x187250: lets ItemPostFrame call Reload although the egon has no clip
bool CEgon::IsHornetGun()
{
	return true;
}

// orig 0x187260. Throw an egon_mirv for ReloadAmmo cells.
void CEgon::Reload()
{
	if (m_flNextSecondaryAttack > gpGlobals->time || m_flNextPrimaryAttack > gpGlobals->time)
		return;

	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.2;
		m_flNextSecondaryAttack = gpGlobals->time + 0.2;
		return;
	}

	CBasePlayer* pPlayer = m_pPlayer;
	int iAmmoType = m_iPrimaryAmmoType;
	if (pPlayer->m_rgAmmo[iAmmoType] < PREFS_KeyLoopUpInt("EGON", "ReloadAmmo", 40))
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.2;
		m_flNextSecondaryAttack = gpGlobals->time + 0.2;
		return;
	}

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= PREFS_KeyLoopUpInt("EGON", "ReloadAmmo", 40);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->pev->punchangle.x = -PREFS_KeyLoopUpFloat("EGON", "ReloadAngleKick", 6.0);
	(void)m_pPlayer->GetGunPosition(); // computed and discarded in the original

	CEgonMirv* pMirv = GetClassPtr((CEgonMirv*)NULL);
	pMirv->pev->team = PREFS_KeyLoopUpInt("EGON", "ReloadMaxFrags", 4);
	pMirv->Spawn();
	pMirv->pev->owner = m_pPlayer->edict();
	UTIL_SetOrigin(pMirv->pev, m_pPlayer->GetGunPosition());
	pMirv->pev->velocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("EGON", "ReloadFragMaxVelo", 1000.0);

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "debris/beamstart8.wav", 1, ATTN_NORM, 0, 90);

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = PREFS_KeyLoopUpFloat("EGON", "ReloadRefire", 2.0) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;
}



class CEgonAmmo : public CBasePlayerAmmo
{
	// orig 0x24bbd0
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_chainammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x24bb90
	void Precache() override
	{
		PRECACHE_MODEL("models/w_chainammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x24bae0
	bool AddAmmo(CBaseEntity* pOther) override
	{
		int iMax = PREFS_KeyLoopUpIntClamp("ITEMS", "CellsMax", 200, 0, 250);
		if (pOther->GiveAmmo(PREFS_KeyLoopUpIntClamp("EGON", "AmmoBoxGive", 40, 0, 250), "uranium", iMax) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_egonclip, CEgonAmmo); // orig 0x187560
