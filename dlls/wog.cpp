/***
*
*	Rocket Crowbar 1.9: wog.cpp. "Wrath of god": a password-guarded command (WOGCommand, the
*	password checked against an RMD hash) that drops a lightning strike on a spot, a player or
*	every entity of a class. The strike (FireWOG) burns everything within 256 units and blinds
*	every client within 1200.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "effects.h"
#include "monsters.h"
#include "player.h"
#include "driveent.h"
#include "decals.h"
#include "wog.h"

// mdhash.cpp (prefs-votes package)
extern unsigned char* RMD(unsigned char* message);

int g_iWOGImpactDisk;  // orig .bss 0x31b6dc
int g_iWOGImpactBalls; // orig .bss 0x31b6e0
int g_iWOGImpactGlow;  // orig .bss 0x31b6e4
int g_iWOGFireBall;	   // orig .bss 0x31b6e8

// orig .data 0x2aa7d8: the RMD digest WOGCommand's password must hash to
unsigned char g_pCorrect[20] = {
	0xf1, 0x83, 0xc3, 0x9c, 0x8e, 0x48, 0xeb, 0x1e, 0x0b, 0xa8,
	0x8b, 0xea, 0x89, 0x7a, 0x8c, 0x0d, 0xc4, 0x9d, 0xc0, 0x42};

//=========================================================
// CWOGBeam: one fading segment of the strike's bolt
//=========================================================
class CWOGBeam : public CBaseEntity
{
public:
	static CWOGBeam* CreateFader(Vector vecStart, Vector vecEnd);
	void EXPORT FadeThink();

	CBeam* m_pBeam; // orig +0x34. VERIFY name
};

// orig 0x23c440. Never Spawn()ed; pev->dmg is the fade length, pev->dmgtime its end.
CWOGBeam* CWOGBeam::CreateFader(Vector vecStart, Vector vecEnd)
{
	CWOGBeam* pFader = GetClassPtr((CWOGBeam*)NULL);
	pFader->SetThink(&CWOGBeam::FadeThink);
	pFader->pev->nextthink = gpGlobals->time + 0.1;
	pFader->pev->animtime = gpGlobals->time;
	pFader->pev->dmg = 0.6;
	pFader->pev->dmgtime = gpGlobals->time + pFader->pev->dmg;

	pFader->m_pBeam = CBeam::BeamCreate(g_pModelNameLaser, 100);
	pFader->m_pBeam->PointsInit(vecStart, vecEnd);
	pFader->m_pBeam->SetBrightness(255);
	pFader->m_pBeam->SetColor(255, 64, 16);
	pFader->m_pBeam->SetNoise(0);
	pFader->m_pBeam->SetScrollRate(32);
	return pFader;
}

// orig 0x23c550
void CWOGBeam::FadeThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime < gpGlobals->time)
	{
		m_pBeam->pev->effects |= EF_NODRAW;
		m_pBeam->SetThink(&CBeam::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 0.1;
		SetThink(&CWOGBeam::SUB_Remove);
	}
	else
	{
		m_pBeam->pev->renderamt = (int)(((pev->dmgtime - gpGlobals->time) / pev->dmg) * 255.0);
	}
}

// orig 0x23c650: the strike at vecOrg. The bolt climbs from the ground in up to 16 segments,
// each continuing above the ceiling the last one hit, until one reaches the sky.
void FireWOG(Vector vecOrg, edict_t* pOwner)
{
	Vector vecGround = vecOrg + Vector(0, 0, 1);
	Vector vecSrc = vecGround;
	TraceResult tr;

	for (int iSegments = 16; iSegments > 0; iSegments--)
	{
		UTIL_TraceLine(vecSrc, vecSrc + Vector(0, 0, 8192), ignore_monsters, pOwner, &tr);
		if (tr.fAllSolid)
			break;

		CWOGBeam::CreateFader(vecSrc, tr.vecEndPos - Vector(0, 0, 1));
		if (tr.flFraction >= 1.0)
			break;

		UTIL_DecalTrace(&tr, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));

		// find the top of the ceiling it hit
		TraceResult trUp;
		UTIL_TraceLine(tr.vecEndPos + Vector(0, 0, 256), tr.vecEndPos + Vector(0, 0, 1), ignore_monsters, pOwner, &trUp);
		if (trUp.fAllSolid)
			break;

		UTIL_DecalTrace(&trUp, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));
		vecSrc = trUp.vecEndPos;
	}

	UTIL_TraceLine(vecGround, vecGround - Vector(0, 0, 64), ignore_monsters, pOwner, &tr);
	UTIL_DecalTrace(&tr, DECAL_SCORCH1 + RANDOM_LONG(0, 1));

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecGround);
	WRITE_BYTE(TE_BEAMDISK);
	WRITE_COORD(vecGround.x);
	WRITE_COORD(vecGround.y);
	WRITE_COORD(vecGround.z + 16);
	WRITE_COORD(vecGround.x);
	WRITE_COORD(vecGround.y);
	WRITE_COORD(vecGround.z + 16 + 2560);
	WRITE_SHORT(g_iWOGImpactDisk);
	WRITE_BYTE(0);	// startframe
	WRITE_BYTE(0);	// framerate
	WRITE_BYTE(3);	// life
	WRITE_BYTE(20); // width
	WRITE_BYTE(0);	// noise
	WRITE_BYTE(255);
	WRITE_BYTE(64);
	WRITE_BYTE(16);
	WRITE_BYTE(220); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecGround);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(vecGround.x);
	WRITE_COORD(vecGround.y);
	WRITE_COORD(vecGround.z);
	WRITE_COORD(vecGround.x);
	WRITE_COORD(vecGround.y);
	WRITE_COORD(vecGround.z + 1);
	WRITE_SHORT(g_iWOGImpactBalls);
	WRITE_BYTE(40); // count
	WRITE_BYTE(8);	// life
	WRITE_BYTE(3);	// scale
	WRITE_BYTE(10); // velocity
	WRITE_BYTE(43); // randomness
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecGround);
	WRITE_BYTE(TE_GLOWSPRITE);
	WRITE_COORD(vecGround.x);
	WRITE_COORD(vecGround.y);
	WRITE_COORD(vecGround.z + 8);
	WRITE_SHORT(g_iWOGImpactGlow);
	WRITE_BYTE(6);	 // life
	WRITE_BYTE(30);	 // scale
	WRITE_BYTE(240); // brightness
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecGround);
	WRITE_BYTE(TE_SMOKE);
	WRITE_COORD(vecGround.x);
	WRITE_COORD(vecGround.y);
	WRITE_COORD(vecGround.z + 20);
	WRITE_SHORT(g_sModelIndexSmoke);
	WRITE_BYTE(28); // scale
	WRITE_BYTE(10); // framerate
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecGround);
	WRITE_BYTE(TE_DLIGHT);
	WRITE_COORD(vecGround.x);
	WRITE_COORD(vecGround.y);
	WRITE_COORD(vecGround.z + 8);
	WRITE_BYTE(12); // radius
	WRITE_BYTE(255);
	WRITE_BYTE(64);
	WRITE_BYTE(16);
	WRITE_BYTE(16); // life
	WRITE_BYTE(3);	// decay
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecGround);
	WRITE_BYTE(TE_DLIGHT);
	WRITE_COORD(vecGround.x);
	WRITE_COORD(vecGround.y);
	WRITE_COORD(vecGround.z + 8);
	WRITE_BYTE(20); // radius
	WRITE_BYTE(255);
	WRITE_BYTE(16);
	WRITE_BYTE(16);
	WRITE_BYTE(2); // life
	WRITE_BYTE(2); // decay
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecGround);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(vecGround.x);
	WRITE_COORD(vecGround.y);
	WRITE_COORD(vecGround.z + 32);
	WRITE_SHORT(g_iWOGFireBall);
	WRITE_BYTE(17); // scale
	WRITE_BYTE(25); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	// the sounds play from a short-lived entity at the strike
	CBaseEntity* pSound = GetClassPtr((CBaseEntity*)NULL);
	pSound->Spawn();
	pSound->pev->classname = MAKE_STRING("sound_emiiter");
	pSound->SetThink(&CBaseEntity::SUB_Remove);
	pSound->pev->nextthink = gpGlobals->time + 0.1;
	UTIL_SetOrigin(pSound->pev, vecGround);
	EMIT_SOUND_DYN(pSound->pev->pContainingEntity, CHAN_WEAPON, "weapons/mortarhit.wav", 1.0, 0.2, 0, 80);

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(pSound->pev->pContainingEntity, CHAN_VOICE, "weapons/debris1.wav", 0.55, 0.8, 0, 100);
		break;
	case 1:
		EMIT_SOUND_DYN(pSound->pev->pContainingEntity, CHAN_VOICE, "weapons/debris2.wav", 0.55, 0.8, 0, 100);
		break;
	case 2:
		EMIT_SOUND_DYN(pSound->pev->pContainingEntity, CHAN_VOICE, "weapons/debris3.wav", 0.55, 0.8, 0, 100);
		break;
	}

	// the inflictor, named for death messages; the engine frees it (FL_KILLME)
	CBaseEntity* pInflictor = GetClassPtr((CBaseEntity*)NULL);
	pInflictor->Spawn();
	pInflictor->pev->classname = MAKE_STRING("wrath of god");
	pInflictor->pev->flags |= FL_KILLME;
	entvars_t* pevInflictor = pInflictor->pev;
	entvars_t* pevAttacker = pevInflictor;
	if (pOwner)
		pevAttacker = VARS(pOwner);

	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecGround, 1200.0)) != NULL)
	{
		if (pEntity->pev->takedamage == DAMAGE_NO && !pEntity->IsNetClient())
			continue;

		Vector vecSpot = pEntity->BodyTarget(vecGround);
		Vector vecToSpot = vecSpot - vecGround;
		UTIL_TraceLine(vecGround, vecSpot, ignore_monsters, NULL, &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEntity->pev->pContainingEntity)
			continue;

		// WON ID 112290 sees his killer as "Dayzed kill-o-matic 4000"
		if (pEntity->IsNetClient() && (*g_engfuncs.pfnGetPlayerWONId)(pEntity->pev->pContainingEntity) == 112290)
			pevInflictor->classname = MAKE_STRING("Dayzed kill-o-matic 4000");

		if (pEntity->IsNetClient())
			UTIL_ScreenFade(pEntity, Vector(255, 64, 16), 0.6, 0.1, 200, 0);

		if (vecToSpot.Length() < 256 && pEntity->pev->pContainingEntity != pOwner)
		{
			Vector vecFlat = vecSpot - vecGround;
			if (vecFlat.Length2D() > 8)
			{
				ClearMultiDamage();
				Vector vecDir = (vecSpot - vecGround).Normalize();
				pEntity->TraceAttack(pevAttacker, 9000, vecDir, &tr, DMG_BURN | DMG_ALWAYSGIB | DMG_MORTAR);
				ApplyMultiDamage(pevInflictor, pevAttacker);
			}
			else
			{
				// straight under the bolt: thrown in a random direction, mostly down
				float flX = RANDOM_FLOAT(-1, 1);
				float flY = RANDOM_FLOAT(-1, 1);
				g_vecAttackDir = Vector(flX, flY, -1).Normalize();
				pEntity->TakeDamage(pevInflictor, pevAttacker, 9000, DMG_BURN | DMG_ALWAYSGIB | DMG_MORTAR);
			}

			// whatever survived dies anyway
			if (pEntity && pEntity->pev && UTIL_IsValidEntity(pEntity->pev->pContainingEntity) && pEntity->IsAlive())
				pEntity->Killed(pevAttacker, GIB_NEVER);
		}

		pevInflictor->classname = MAKE_STRING("wrath of god");
	}
}

// orig 0x23d7a0
void WOGPrecache()
{
	g_iWOGImpactDisk = PRECACHE_MODEL("sprites/shockwave.spr");
	g_iWOGImpactBalls = PRECACHE_MODEL("sprites/redflare2.spr");
	g_iWOGImpactGlow = PRECACHE_MODEL("sprites/redflare1.spr");
	g_iWOGFireBall = PRECACHE_MODEL("sprites/xfireball3.spr");
	PRECACHE_SOUND("weapons/mortarhit.wav");
	PRECACHE_SOUND("weapons/mortar.wav");
}

//=========================================================
// CWOGSpawner: the incoming-mortar sound, then the strike 2.28 s later. pev->button picks the
// target: 1 the ground under it, 2 pev->enemy's position. It is never removed.
//=========================================================
class CWOGSpawner : public CBaseEntity
{
public:
	static CWOGSpawner* CreateWSpt(Vector vecOrg, edict_t* pOwner);
	static CWOGSpawner* CreateWSed(edict_t* pTarget, edict_t* pOwner);
	void EXPORT FireThink();
};

// orig 0x23d850: strike the ground under vecOrg
CWOGSpawner* CWOGSpawner::CreateWSpt(Vector vecOrg, edict_t* pOwner)
{
	CWOGSpawner* pSpawner = GetClassPtr((CWOGSpawner*)NULL);
	pSpawner->SetThink(&CWOGSpawner::FireThink);
	pSpawner->pev->nextthink = gpGlobals->time + 2.28182;
	pSpawner->pev->button = 1;
	pSpawner->pev->owner = pOwner;
	UTIL_SetOrigin(pSpawner->pev, vecOrg);
	EMIT_SOUND_DYN(pSpawner->pev->pContainingEntity, CHAN_WEAPON, "weapons/mortar.wav", 1.0, 0.2, 0, 100);
	return pSpawner;
}

// orig 0x23d900: strike pTarget wherever it is by then
CWOGSpawner* CWOGSpawner::CreateWSed(edict_t* pTarget, edict_t* pOwner)
{
	CWOGSpawner* pSpawner = CreateWSpt(pTarget->v.origin, pOwner);
	pSpawner->pev->button = 2;
	pSpawner->pev->enemy = pTarget;
	return pSpawner;
}

// orig 0x23d960
void CWOGSpawner::FireThink()
{
	Vector vecTarget;

	if (pev->button == 1)
	{
		TraceResult tr;
		UTIL_TraceLine(pev->origin, pev->origin - Vector(0, 0, 8192), ignore_monsters, pev->pContainingEntity, &tr);
		vecTarget = tr.vecEndPos;
	}
	else if (pev->button == 2)
	{
		if (!UTIL_IsValidEntity(pev->enemy))
			return;

		// a visible client's origin is his eyes' height: strike his feet
		if ((pev->enemy->v.flags & FL_CLIENT) == 0 || (pev->enemy->v.effects & EF_NODRAW) != 0)
			vecTarget = pev->enemy->v.origin;
		else
			vecTarget = pev->enemy->v.origin - Vector(0, 0, 36);
	}
	else
	{
		return;
	}

	FireWOG(vecTarget, pev->owner);
}

// orig 0x23dac0: at what pEdict looks at; bSeed strikes the entity hit instead of the spot
void WOGCalledFireForward(edict_t* pEdict, bool bSeed)
{
	Vector vecSrc = pEdict->v.origin + pEdict->v.view_ofs;
	UTIL_MakeVectors(pEdict->v.v_angle);
	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, pEdict, &tr);

	if (bSeed)
	{
		if (tr.pHit)
			CWOGSpawner::CreateWSed(tr.pHit, pEdict);
	}
	else
	{
		CWOGSpawner::CreateWSpt(tr.vecEndPos, pEdict);
	}
}

// orig 0x23dbc0: on player iIndex, or the monster he drives
void WOGCalledFireOnPlayer(edict_t* pEdict, int iIndex)
{
	CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex(iIndex);
	if (pPlayer == NULL || pPlayer->pev == NULL || !UTIL_IsValidEntity(pPlayer->pev->pContainingEntity))
		return;

	entvars_t* pevTarget;
	if (pPlayer->m_bInDriveEnt && pPlayer->m_pCurDriveEnt)
		pevTarget = pPlayer->m_pCurDriveEnt->pev;
	else
		pevTarget = pPlayer->pev;

	CWOGSpawner::CreateWSed(pevTarget->pContainingEntity, pEdict);
}

// orig 0x23dc40: on every entity of szClassname
void WOGCalledFireOnAll(edict_t* pEdict, char* szClassname)
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = UTIL_FindEntityByClassname(pEntity, szClassname)) != NULL)
	{
		if (pEntity->pev && UTIL_IsValidEntity(pEntity->pev->pContainingEntity))
			CWOGSpawner::CreateWSed(pEntity->pev->pContainingEntity, pEdict);
	}
}

// orig 0x23dcb0: at what pEdict looks at, no warning
void WOGInstantFire(edict_t* pEdict)
{
	Vector vecSrc = pEdict->v.origin + pEdict->v.view_ofs;
	UTIL_MakeVectors(pEdict->v.v_angle);
	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 8192, dont_ignore_monsters, pEdict, &tr);
	FireWOG(tr.vecEndPos, pEdict);
}

// orig 0x23dd90: "<cmd> <password> <n> [classname]". n a player index: strike him. n < 0: strike
// what you look at, once it settles. Otherwise, with 41 or fewer arguments, strike the spot you
// look at; with more, n 42 strikes every argv(3), anything else strikes instantly.
// True when the password matched.
bool WOGCommand(edict_t* pEdict)
{
	if (CMD_ARGC() <= 2)
		return false;

	unsigned char* pDigest = RMD((unsigned char*)CMD_ARGV(1));
	for (int i = 0; i < 20; i++)
	{
		if (g_pCorrect[i] != pDigest[i])
			return false;
	}

	if (UTIL_PlayerByIndex(atoi(CMD_ARGV(2))))
	{
		WOGCalledFireOnPlayer(pEdict, atoi(CMD_ARGV(2)));
	}
	else if (atoi(CMD_ARGV(2)) < 0)
	{
		WOGCalledFireForward(pEdict, true);
	}
	else if (CMD_ARGC() <= 41) // VERIFY: an argument count; the disassembly calls pfnCmd_Argc here
	{
		WOGCalledFireForward(pEdict, false);
	}
	else if (atoi(CMD_ARGV(2)) == 42)
	{
		WOGCalledFireOnAll(pEdict, (char*)CMD_ARGV(3));
	}
	else
	{
		WOGInstantFire(pEdict);
	}

	return true;
}
