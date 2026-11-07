/***
*
*	Rocket Crowbar 1.9: nm.cpp. The nuke: a slow rocket (nuke_rocket, CNukeM) the firing player
*	steers and watches through a camera, whose blast punches through up to MaxPunchWalls walls.
*	Tunables in rc.ini's [NUKE] heading.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "client.h"
#include "decals.h"
#include "UserMessages.h"
#include "nm.h"

// Preff.cpp
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
extern int PREFS_KeyLoopUpInt(const char* szHeading, const char* szKey, int iDefault);
// util.cpp
extern unsigned short FixedUnsigned16(float value, float scale);

class CNukeM : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	bool PunchTrace(Vector vecStart, Vector vecEnd, float flMaxPunch, int iMaxWalls);
	void NukeRadus(Vector vecOrigin, float flDamage, float flRadius);
	void NukeFade();
	void EXPORT ShockwaveShake();
	void EXPORT RocketTouch(CBaseEntity* pOther);
	void EXPORT NukeThink();

	CBaseEntity* m_pGlow; // orig +0x280, the blue flare that follows the rocket. VERIFY name
	CBaseEntity* m_pCam;  // orig +0x284, the owner's view while he steers. VERIFY name
	int m_iTrail;		  // orig +0x288, sprites/smoke.spr. VERIFY name
	int m_iExplode;		  // orig +0x28c, sprites/fexplo.spr. VERIFY name
	int m_iBeam;		  // orig +0x290, sprites/white.spr. VERIFY name
};

LINK_ENTITY_TO_CLASS(nuke_rocket, CNukeM); // orig 0x1d64a0

// orig 0x1d64d0. Flies along whatever gpGlobals->v_forward holds (FireNuke does not set it).
void CNukeM::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	SET_MODEL(edict(), "models/hvr.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	UTIL_SetOrigin(pev, pev->origin);
	pev->classname = MAKE_STRING("nuke");
	SetThink(&CNukeM::NukeThink);
	SetTouch(&CNukeM::RocketTouch);
	pev->velocity = gpGlobals->v_forward * 200;
	pev->air_finished = gpGlobals->time; // last think time
	pev->nextthink = gpGlobals->time + 0.01;
	pev->effects |= EF_LIGHT;
	EMIT_SOUND_DYN(edict(), CHAN_VOICE, "weapons/rocket1.wav", 1.0, 0.5, 0, 80);

	m_pGlow = GetClassPtr((CBaseEntity*)NULL);
	m_pGlow->Spawn();
	m_pGlow->pev->movetype = MOVETYPE_FOLLOW;
	m_pGlow->pev->solid = SOLID_NOT;
	m_pGlow->pev->aiment = edict();
	m_pGlow->pev->iuser3 = 32; // VERIFY: nothing in the package reads it
	SET_MODEL(m_pGlow->pev->pContainingEntity, "sprites/blueflare2.spr");
	m_pGlow->pev->renderamt = 240;
	m_pGlow->pev->fuser1 = 240; // VERIFY: fuser1-4 unread in the package
	m_pGlow->pev->fuser2 = 90;
	m_pGlow->pev->fuser3 = 0.9;
	m_pGlow->pev->fuser4 = 0.05;
	m_pGlow->pev->rendermode = kRenderGlow;
	m_pGlow->pev->renderfx = kRenderFxNoDissipation;
	m_pGlow->pev->scale = 6.0;
	UTIL_SetOrigin(m_pGlow->pev, pev->origin);

	m_pCam = GetClassPtr((CBaseEntity*)NULL);
	m_pCam->Spawn();
	m_pCam->pev->movetype = MOVETYPE_FLY;
	m_pCam->pev->solid = SOLID_NOT;
	SET_MODEL(m_pCam->pev->pContainingEntity, "models/hvr.mdl");
	UTIL_SetOrigin(m_pCam->pev, pev->origin);
	m_pCam->pev->renderamt = 0;
	m_pCam->pev->rendermode = kRenderTransAdd;
	m_pCam->pev->renderfx = kRenderFxNone;

	if (pev->owner && pev->button)
	{
		SetupVisibility(m_pCam->pev->pContainingEntity, pev->owner, NULL, NULL);
		SET_VIEW(pev->owner, m_pCam->pev->pContainingEntity);
	}

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(m_iTrail);
	WRITE_BYTE(60); // life
	WRITE_BYTE(8);	// width
	WRITE_BYTE(224);
	WRITE_BYTE(224);
	WRITE_BYTE(255);
	WRITE_BYTE(255); // brightness
	MESSAGE_END();
}

// orig 0x1d6970: walks a line through up to iMaxWalls walls, flMaxPunch units of wall in all;
// true only when a segment reaches vecEnd unblocked
bool CNukeM::PunchTrace(Vector vecStart, Vector vecEnd, float flMaxPunch, int iMaxWalls)
{
	Vector vecSrc = vecStart;
	Vector vecDir = (vecEnd - vecStart).Normalize();
	float flPunchLeft = flMaxPunch;

	if (flPunchLeft >= 0)
	{
		while (iMaxWalls > 0)
		{
			TraceResult tr;
			UTIL_TraceLine(vecSrc, vecEnd, ignore_monsters, edict(), &tr);
			if (tr.flFraction == 1.0)
				return true;
			if (tr.fAllSolid)
				return false;
			if (tr.fStartSolid)
				return false;

			// trace back from flPunchLeft deep into the wall to find its far side
			Vector vecBack = tr.vecEndPos + vecDir;
			Vector vecDeep = tr.vecEndPos + vecDir * flPunchLeft;
			TraceResult trBack;
			UTIL_TraceLine(vecDeep, vecBack, ignore_monsters, edict(), &trBack);
			if (trBack.fAllSolid)
				return false;

			flPunchLeft -= (trBack.vecEndPos - tr.vecEndPos).Length();
			vecSrc = trBack.vecEndPos + vecDir;
			iMaxWalls--;

			if (flPunchLeft < 0)
				return false;
		}
	}

	return false;
}

// orig 0x1d6c60. The falloff only decides who is hit: everyone hit takes the full flDamage, as in
// the original.
void CNukeM::NukeRadus(Vector vecOrigin, float flDamage, float flRadius)
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecOrigin, flRadius)) != NULL)
	{
		if (pEntity->pev->takedamage == DAMAGE_NO)
			continue;

		float flDist = (pEntity->Center() - pev->origin).Length();
		float flAdjusted = flDamage;
		if (!(1.0 - flDist / flRadius > 1.0))
			flAdjusted = flDamage * (1.0 - flDist / flRadius);

		if (flAdjusted > 0)
		{
			Vector vecSrc = pev->origin;
			Vector vecTarget = pEntity->Center();
			if (PunchTrace(vecSrc, vecTarget, PREFS_KeyLoopUpFloat("NUKE", "MaxPunch", 256.0), PREFS_KeyLoopUpInt("NUKE", "MaxPunchWalls", 32)))
			{
				if (pev->owner)
					pEntity->TakeDamage(pev, VARS(pev->owner), flDamage, DMG_BLAST);
				else
					pEntity->TakeDamage(pev, pev, flDamage, DMG_BLAST);
			}
		}
	}
}

// orig 0x1d6e20: whites out every player with a line of sight to the blast
void CNukeM::NukeFade()
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, 8192.0)) != NULL)
	{
		if (!pEntity->IsPlayer())
			continue;

		Vector vecEye = pEntity->pev->origin + pEntity->pev->view_ofs;
		TraceResult tr;
		UTIL_TraceLine(pev->origin, vecEye, ignore_monsters, edict(), &tr);
		if (tr.flFraction >= 1.0 || tr.pHit == pEntity->pev->pContainingEntity)
			UTIL_ScreenFade(pEntity, Vector(255, 255, 255), 0.6, 0.2, 255, 0);
	}
}

// orig 0x1d6f20: the delayed shake for everyone, a smoke puff, then gone
void CNukeM::ShockwaveShake()
{
	SetThink(&CNukeM::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;

	unsigned short usDuration = FixedUnsigned16(1.5, 1 << 12);
	unsigned short usFrequency = FixedUnsigned16(40, 1 << 8);
	unsigned short usAmplitude = FixedUnsigned16(300, 1 << 12);
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgShake);
	WRITE_SHORT(usAmplitude);
	WRITE_SHORT(usDuration);
	WRITE_SHORT(usFrequency);
	MESSAGE_END();

	Vector vecSmoke = pev->origin + pev->angles * 128; // angles holds the hit plane's normal
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecSmoke);
	WRITE_BYTE(TE_SMOKE);
	WRITE_COORD(vecSmoke.x);
	WRITE_COORD(vecSmoke.y);
	WRITE_COORD(vecSmoke.z + 64);
	WRITE_SHORT(g_sModelIndexSmoke);
	WRITE_BYTE(230); // scale
	WRITE_BYTE(6);	 // framerate
	MESSAGE_END();
}

// orig 0x1d7120
void CNukeM::RocketTouch(CBaseEntity* pOther)
{
	if (pOther->pev->pContainingEntity == pev->owner)
		return;

	if (m_pGlow)
	{
		m_pGlow->pev->effects |= EF_NODRAW;
		m_pGlow->pev->iuser3 = 0;
		m_pGlow->SUB_Remove();
		m_pGlow = NULL;
	}

	NukeFade();

	if (pev->button && pev->owner)
	{
		SetupVisibility(pev->owner, pev->owner, NULL, NULL);
		SET_VIEW(pev->owner, pev->owner);
		UTIL_ScreenFade(CBaseEntity::Instance(pev->owner), Vector(255, 255, 255), 0.8, 0.3, 255, 0);
	}

	if (m_pCam)
	{
		m_pCam->SUB_Remove();
		m_pCam = NULL;
	}

	EMIT_SOUND_DYN(edict(), CHAN_VOICE, "weapons/rocket1.wav", 0, 0, SND_STOP, 100);

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_KILLBEAM);
	WRITE_SHORT(entindex());
	MESSAGE_END();

	Vector vecDir = pev->velocity.Normalize();
	TraceResult tr;
	UTIL_TraceLine(pev->origin - vecDir * 8, pev->origin + vecDir * 128, ignore_monsters, edict(), &tr);

	UTIL_SetOrigin(pev, pev->origin + tr.vecPlaneNormal * 64);
	pev->angles = tr.vecPlaneNormal;
	UTIL_DecalTrace(&tr, DECAL_SCORCH1 + RANDOM_LONG(0, 1));

	SetTouch(NULL);
	SetThink(&CNukeM::ShockwaveShake);
	pev->nextthink = gpGlobals->time + 0.4;
	pev->effects |= EF_NODRAW;

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_SPRITE);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 256);
	WRITE_SHORT(m_iExplode);
	WRITE_BYTE(140); // scale
	WRITE_BYTE(255); // brightness
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 4096);
	WRITE_SHORT(m_iBeam);
	WRITE_BYTE(0);	// startframe
	WRITE_BYTE(0);	// framerate
	WRITE_BYTE(4);	// life
	WRITE_BYTE(64); // width
	WRITE_BYTE(0);	// noise
	WRITE_BYTE(255);
	WRITE_BYTE(255);
	WRITE_BYTE(192);
	WRITE_BYTE(128); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMTORUS);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 4096);
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(0);	 // framerate
	WRITE_BYTE(10);	 // life
	WRITE_BYTE(255); // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(255);
	WRITE_BYTE(160);
	WRITE_BYTE(100);
	WRITE_BYTE(110); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(230); // scale
	WRITE_BYTE(12);	 // framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	EMIT_SOUND_DYN(edict(), CHAN_BODY, "weapons/mortarhit.wav", 1.0, 0.3, 0, 100);

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(edict(), CHAN_VOICE, "weapons/debris1.wav", 0.55, 0.7, 0, 100);
		break;
	case 1:
		EMIT_SOUND_DYN(edict(), CHAN_VOICE, "weapons/debris2.wav", 0.55, 0.7, 0, 100);
		break;
	case 2:
		EMIT_SOUND_DYN(edict(), CHAN_VOICE, "weapons/debris3.wav", 0.55, 0.7, 0, 100);
		break;
	}

	int iSparks = RANDOM_LONG(1, 5);
	for (int i = 0; i < iSparks; i++)
		CBaseEntity::Create("spark_shower", pev->origin, tr.vecPlaneNormal, NULL);

	Vector vecBlast = pev->origin;
	float flRadius = PREFS_KeyLoopUpFloat("NUKE", "Radius", 1400.0);
	NukeRadus(vecBlast, PREFS_KeyLoopUpFloat("NUKE", "Damage", 2000.0), flRadius);
}

// orig 0x1d7bc0: steering. Accelerates along the owner's view; stops thinking (and steering, and
// moving the camera) on any think where it is already at full forward speed, as the original does.
void CNukeM::NukeThink()
{
	float flDelta = gpGlobals->time - pev->air_finished;
	pev->air_finished = gpGlobals->time;

	if (pev->owner == NULL || !UTIL_IsValidEntity(pev->owner))
		UTIL_MakeAimVectors(pev->angles);
	else
		UTIL_MakeVectors(pev->owner->v.v_angle);

	Vector vecForward = gpGlobals->v_forward;
	pev->angles = UTIL_VecToAngles(vecForward);

	float flMaxSpeed, flMaxAccel;
	if (pev->waterlevel == 3 && UTIL_PointContents(pev->origin) != CONTENTS_SKY)
	{
		flMaxSpeed = PREFS_KeyLoopUpFloat("NUKE", "WaterMaxSpeed", 800.0);
		flMaxAccel = PREFS_KeyLoopUpFloat("NUKE", "WaterMaxAccel", 500.0);
		UTIL_BubbleTrail(pev->origin - pev->velocity * flDelta, pev->origin, 4);
		// "WatterMaxSpeed" (sic) caps it, not WaterMaxSpeed
		if (pev->velocity.Length() > PREFS_KeyLoopUpFloat("NUKE", "WatterMaxSpeed", 800.0))
			pev->velocity = pev->velocity.Normalize() * PREFS_KeyLoopUpFloat("NUKE", "WatterMaxSpeed", 800.0);
	}
	else
	{
		flMaxSpeed = PREFS_KeyLoopUpFloat("NUKE", "AirMaxSpeed", 2000.0);
		flMaxAccel = PREFS_KeyLoopUpFloat("NUKE", "AirMaxAccel", 1500.0);
	}

	float flAddSpeed = flMaxSpeed - DotProduct(pev->velocity, vecForward);
	if (flAddSpeed <= 0)
		return;

	float flAccel = flMaxAccel * flDelta * flMaxSpeed;
	if (flAccel > flAddSpeed)
		flAccel = flAddSpeed;
	pev->velocity = pev->velocity + vecForward * flAccel;

	if (pev->velocity.Length() > 2000)
		pev->velocity = pev->velocity.Normalize() * 2000;

	if (pev->button && pev->owner)
	{
		Vector vecCamAngles = pev->angles;
		vecCamAngles.y = UTIL_AngleMod(vecCamAngles.y);
		vecCamAngles.x = -vecCamAngles.x;
		if (vecCamAngles.x > 180)
			vecCamAngles.x -= 360;
		if (vecCamAngles.x < 180) // sic: 180, so pitch always ends in [180, 540)
			vecCamAngles.x += 360;

		UTIL_MakeAimVectors(pev->angles);
		m_pCam->pev->v_angle = vecCamAngles;
		m_pCam->pev->angles = m_pCam->pev->v_angle;
		UTIL_SetOrigin(m_pCam->pev, pev->origin + gpGlobals->v_forward * 48);
		m_pCam->pev->velocity = pev->velocity;
		SetupVisibility(m_pCam->pev->pContainingEntity, pev->owner, NULL, NULL);
		SET_VIEW(pev->owner, m_pCam->pev->pContainingEntity);
	}

	pev->nextthink = gpGlobals->time + 0.01;
}

// orig 0x1d8180
void CNukeM::Precache()
{
	PRECACHE_MODEL("models/hvr.mdl");
	PRECACHE_MODEL("sprites/blueflare2.spr");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	m_iExplode = PRECACHE_MODEL("sprites/fexplo.spr");
	m_iBeam = PRECACHE_MODEL("sprites/white.spr");
	PRECACHE_SOUND("weapons/rocket1.wav");
}

// orig 0x1d8220: costs CellsForNuke uranium, RocketsForNuke rockets and PythonRoundsForNuke 357
bool FireNuke(CBasePlayer* pPlayer)
{
	int iUranium = CBasePlayer::GetAmmoIndex("uranium");
	int iRockets = CBasePlayer::GetAmmoIndex("rockets");
	int i357 = CBasePlayer::GetAmmoIndex("357");

	if ((unsigned int)iUranium >= MAX_AMMO_SLOTS || iRockets < 0 || iRockets >= MAX_AMMO_SLOTS || i357 < 0 || i357 >= MAX_AMMO_SLOTS)
		return false;

	int iCells = PREFS_KeyLoopUpInt("NUKE", "CellsForNuke", 160);
	int iRocketCost = PREFS_KeyLoopUpInt("NUKE", "RocketsForNuke", 10);
	int i357Cost = PREFS_KeyLoopUpInt("NUKE", "PythonRoundsForNuke", 6);

	if (pPlayer->m_rgAmmo[iUranium] < iCells || pPlayer->m_rgAmmo[iRockets] < iRocketCost || pPlayer->m_rgAmmo[i357] < i357Cost)
		return false;

	pPlayer->m_rgAmmo[iUranium] -= iCells;
	pPlayer->m_rgAmmo[iRockets] -= iRocketCost;
	pPlayer->m_rgAmmo[i357] -= i357Cost;
	pPlayer->SendAmmoUpdate();

	edict_t* pent = CREATE_NAMED_ENTITY(MAKE_STRING("nuke_rocket"));
	if (FNullEnt(pent))
	{
		ALERT(at_console, "FOOO! Nuke ent is NULL (not supposed to happen!)\n");
		return false;
	}

	pent->v.button = 1; // steered: the owner views through the camera
	pent->v.angles = pPlayer->pev->v_angle;
	pent->v.origin = pPlayer->pev->origin + pPlayer->pev->view_ofs;
	pent->v.owner = pPlayer->pev->pContainingEntity;
	DispatchSpawn(pent);
	return true;
}
