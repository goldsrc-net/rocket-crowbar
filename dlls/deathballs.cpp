/***
*
*	Rocket Crowbar 1.9: deathballs.cpp. The deathball: a huge rolling ball players push around
*	(by touch or by shooting it) that crushes whatever it rolls into fast enough. db_spawner keeps
*	mp_deathballs of them in play. Tunables in rc.ini's [DEATHBALL] heading.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "gamerules.h"
#include "saverestore.h"
#include "deathballs.h"

// Preff.cpp
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
extern bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault);

//=========================================================
// CDBSpawner
//=========================================================
class CDBSpawner : public CBaseEntity
{
public:
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void Spawn() override;
	int ObjectCaps() override;
	void EXPORT DBSThink();

	bool m_bDummy; // saved, never read or written
};

LINK_ENTITY_TO_CLASS(db_spawner, CDBSpawner); // orig 0x117900

// orig 0x1178a0
void CreateDBSpawner()
{
	if (g_pGameRules->IsMultiplayer())
	{
		if (UTIL_FindEntityByClassname(NULL, "db_spawner") == NULL)
			CBaseEntity::Create("db_spawner", g_vecZero, g_vecZero, NULL);
	}
}

TYPEDESCRIPTION CDBSpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDBSpawner, m_bDummy, FIELD_BOOLEAN),
};

// orig 0x117930
bool CDBSpawner::Save(CSave& save)
{
	if (!CBaseEntity::Save(save))
		return false;
	return save.WriteFields("CDBSpawner", this, m_SaveData, ARRAYSIZE(m_SaveData));
}

// orig 0x117980
bool CDBSpawner::Restore(CRestore& restore)
{
	if (!CBaseEntity::Restore(restore))
		return false;
	return restore.ReadFields("CDBSpawner", this, m_SaveData, ARRAYSIZE(m_SaveData));
}

// orig 0x1179d0: one ball more or one less per second until there are mp_deathballs
void CDBSpawner::DBSThink()
{
	int iWanted = (int)CVAR_GET_FLOAT("mp_deathballs");
	int iCount = 0;
	CBaseEntity* pBall = NULL;

	pev->nextthink = gpGlobals->time + 1.0;

	while ((pBall = UTIL_FindEntityByClassname(pBall, "deathball")) != NULL)
		iCount++;

	if (iCount == iWanted)
		return;

	if (iWanted - iCount <= 0)
	{
		pBall = UTIL_FindEntityByClassname(NULL, "deathball");
		if (pBall)
		{
			pBall->SetThink(&CBaseEntity::SUB_Remove);
			pBall->pev->nextthink = gpGlobals->time + 0.1;
		}
	}
	else
	{
		CBaseEntity::Create("deathball", g_vecZero, g_vecZero, NULL);
	}
}

// orig 0x117ae0
void CDBSpawner::Spawn()
{
	pev->classname = MAKE_STRING("db_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDBSpawner::DBSThink);
	pev->nextthink = gpGlobals->time + 15.0;
}

// orig 0x119680
int CDBSpawner::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}

//=========================================================
// CDeathBall
//=========================================================
#define RC_CAP_DEATHBALL_CRUSH 0x2000 // the RC cap a deathball crushes. VERIFY name

class CDeathBall : public CBaseMonster
{
public:
	void Precache() override;
	void Spawn() override;
	bool CheckCanSpawnHere(Vector vecOrg);
	void EXPORT DBInitThink();
	void DBDoSpawnEffects();
	void EXPORT DBThink();
	void EXPORT DBTouch(CBaseEntity* pOther);
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	void EntIsBeingRemoved(edict_t* pEdict) override;
	void SetObjectCollisionBox() override;
	bool IsDriveent() override;
	const char* GetControllerName() override;
	int GetRCCaps() override;
	int BloodColor() override;

	float m_flSpawnTime;		 // orig +0x27c, set by Spawn, never read. VERIFY name
	float m_flHitterTime;		 // orig +0x280, m_pHitter gets the kills until this. VERIFY name
	edict_t* m_pHitter;			 // orig +0x284, who last pushed or shot it. VERIFY name
	float m_flNextSpawnFXTime;	 // orig +0x288. VERIFY name
	float m_flSpawnFXDeadline;	 // orig +0x28c, start the rings by now even in the air. VERIFY name
	int m_iSpawnFXLeft;			 // orig +0x290, rings still to draw. VERIFY name
	bool m_bSpawnFXStarted;		 // orig +0x294. VERIFY name
	bool m_bOnGround;			 // orig +0x295. VERIFY name
	bool m_bRolling;			 // orig +0x296, the rolling sound is playing. VERIFY name
	int m_iShockwave;			 // orig +0x298, sprites/shockwave.spr. VERIFY name
};

LINK_ENTITY_TO_CLASS(deathball, CDeathBall); // orig 0x117b80

// orig 0x117bb0
void CDeathBall::Precache()
{
	PRECACHE_MODEL("models/db.mdl");
	PRECACHE_SOUND("gonarch/gon_step1.wav");
	PRECACHE_SOUND("gonarch/gon_step2.wav");
	PRECACHE_SOUND("gonarch/gon_step3.wav");
	PRECACHE_SOUND("doors/doormove6.wav");
	m_iShockwave = PRECACHE_MODEL("sprites/shockwave.spr");
}

// orig 0x117c40: hidden until DBInitThink finds it a spot
void CDeathBall::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_YES;
	pev->classname = MAKE_STRING("deathball");
	SET_MODEL(edict(), "models/db.mdl");
	UTIL_SetSize(pev, Vector(-71, -71, -71), Vector(71, 71, 71));
	SetTouch(&CDeathBall::DBTouch);
	SetThink(&CDeathBall::DBInitThink);
	pev->friction = PREFS_KeyLoopUpFloat("DEATHBALL", "Friction", 0.9);
	pev->gravity = PREFS_KeyLoopUpFloat("DEATHBALL", "Gravity", 1.2);
	m_bOnGround = false;
	m_bRolling = false;
	m_flSpawnTime = gpGlobals->time;
	pev->teleport_time = pev->dmgtime = gpGlobals->time; // next bounce, next crush
	m_flHitterTime = -1;
	m_pHitter = NULL;
	pev->velocity = g_vecZero;
	pev->effects |= EF_NODRAW;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	m_bSpawnFXStarted = false;
	pev->sequence = 0;
	pev->nextthink = gpGlobals->time + 0.01;
	ResetSequenceInfo();
}

// orig 0x117e20: room for the ball (8 units of slack) at vecOrg
bool CDeathBall::CheckCanSpawnHere(Vector vecOrg)
{
	Vector vecMins = pev->mins + Vector(-8, -8, -8);
	Vector vecMaxs = pev->maxs + Vector(8, 8, 8);
	TraceResult tr;

	UTIL_TraceHull(vecOrg, vecOrg, dont_ignore_monsters, large_hull, edict(), &tr);
	if (tr.flFraction < 1.0 || tr.fAllSolid || tr.fStartSolid)
		return false;

	UTIL_TraceLine(vecOrg + vecMins, vecOrg + vecMaxs, dont_ignore_monsters, edict(), &tr);
	if (tr.flFraction < 1.0 || tr.fAllSolid || tr.fStartSolid)
		return false;

	UTIL_TraceLine(vecOrg + Vector(vecMaxs.x, vecMaxs.y, vecMins.z), vecOrg + Vector(vecMins.x, vecMins.y, vecMaxs.z), dont_ignore_monsters, edict(), &tr);
	if (tr.flFraction < 1.0 || tr.fAllSolid || tr.fStartSolid)
		return false;

	return true;
}

// orig 0x118010: the first info_deathball_spawn if it has room, else a deathmatch spawn (64 up)
// with room, walked as in EntSelectSpawnPoint; retries every 0.5 s while there is none
void CDeathBall::DBInitThink()
{
	pev->nextthink = gpGlobals->time + 0.5;

	CBaseEntity* pSpot = UTIL_FindEntityByClassname(NULL, "info_deathball_spawn");
	if (pSpot && CheckCanSpawnHere(pSpot->pev->origin))
	{
		SetTouch(&CDeathBall::DBTouch);
		SetThink(&CDeathBall::DBThink);
		pev->nextthink = gpGlobals->time + 0.01;
		UTIL_SetOrigin(pev, pSpot->pev->origin);
		pev->flags &= ~FL_ONGROUND;
		pev->effects &= ~EF_NODRAW;
		m_flNextSpawnFXTime = gpGlobals->time;
		m_iSpawnFXLeft = 12;
		m_flSpawnFXDeadline = gpGlobals->time + 5.0;
		return;
	}

	CBaseEntity* pFirstSpot;
	int iCount = 0;

	pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch")) != NULL)
		iCount++;

	pSpot = NULL;
	for (int iRand = RANDOM_LONG(1, iCount); iRand > 0; iRand--)
		pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch");

	if (pSpot == NULL || FNullEnt(pSpot->pev))
		pSpot = UTIL_FindEntityByClassname(NULL, "info_player_deathmatch");
	pFirstSpot = pSpot;

	do
	{
		if (pSpot != NULL)
		{
			if (!(pSpot->pev->origin == Vector(0, 0, 0)))
			{
				if (CheckCanSpawnHere(pSpot->pev->origin + Vector(0, 0, 64)))
					break;
			}
		}
		pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch");
	} while (pSpot != pFirstSpot);

	if (pSpot == NULL && (pSpot = UTIL_FindEntityByClassname(NULL, "info_player_deathmatch")) == NULL)
	{
		SetThink(&CDeathBall::DBInitThink);
		pev->nextthink = gpGlobals->time + 0.5;
	}
	else
	{
		SetTouch(&CDeathBall::DBTouch);
		SetThink(&CDeathBall::DBThink);
		pev->nextthink = gpGlobals->time + 0.01;
		UTIL_SetOrigin(pev, pSpot->pev->origin + Vector(0, 0, 64));
		m_flNextSpawnFXTime = gpGlobals->time;
		m_iSpawnFXLeft = 12;
		m_flSpawnFXDeadline = gpGlobals->time + 5.0;
		pev->flags &= ~FL_ONGROUND;
		pev->effects &= ~EF_NODRAW;
	}
}

// orig 0x118460: twelve rising rings, red to blue, as the ball fades in; they start when it lands
// (or after 5 s)
void CDeathBall::DBDoSpawnEffects()
{
	if (m_iSpawnFXLeft <= 0 || m_flNextSpawnFXTime > gpGlobals->time)
		return;

	if ((pev->flags & FL_ONGROUND) == 0 && !m_bSpawnFXStarted && gpGlobals->time < m_flSpawnFXDeadline)
		return;

	m_flNextSpawnFXTime = gpGlobals->time + 0.1;
	m_bSpawnFXStarted = true;

	float flStep = 12 - m_iSpawnFXLeft;
	Vector vecRing(pev->origin.x, pev->origin.y, (flStep * 12 - 72) + pev->origin.z);
	float flRed = flStep * 21.25;
	Vector vecTop = vecRing + Vector(0, 0, 1280);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecRing);
	WRITE_BYTE(TE_BEAMDISK);
	WRITE_COORD(vecRing.x);
	WRITE_COORD(vecRing.y);
	WRITE_COORD(vecRing.z);
	WRITE_COORD(vecTop.x);
	WRITE_COORD(vecTop.y);
	WRITE_COORD(vecTop.z);
	WRITE_SHORT(m_iShockwave);
	WRITE_BYTE(0);	// startframe
	WRITE_BYTE(0);	// framerate
	WRITE_BYTE(2);	// life
	WRITE_BYTE(10); // width
	WRITE_BYTE(0);	// noise
	WRITE_BYTE((int)flRed);
	WRITE_BYTE(0);
	WRITE_BYTE((int)(255.0 - flRed));
	WRITE_BYTE(180); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	pev->renderamt += 21.25;
	m_iSpawnFXLeft--;
	if (pev->renderamt >= 255 || m_iSpawnFXLeft < 1)
	{
		pev->rendermode = kRenderNormal;
		pev->renderamt = 255;
	}
}

// orig 0x1187a0: the rolling sound follows the ground speed; BouncyDeathball hops on landing
void CDeathBall::DBThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.01;
	DBDoSpawnEffects();

	const bool bOnGround = (pev->flags & FL_ONGROUND) != 0;

	if (m_bOnGround)
	{
		if (!bOnGround)
		{
			EMIT_SOUND_DYN(edict(), CHAN_STATIC, "doors/doormove6.wav", 0, 0, SND_STOP, 100);
			m_bRolling = false;
			m_bOnGround = false;
		}
		else if (m_bRolling && pev->velocity.Length2D() <= 1.0)
		{
			EMIT_SOUND_DYN(edict(), CHAN_STATIC, "doors/doormove6.wav", 0, 0, SND_STOP, 100);
			m_bRolling = false;
		}
	}
	else if (bOnGround)
	{
		m_bOnGround = true;
		EMIT_SOUND_DYN(edict(), CHAN_STATIC, "doors/doormove6.wav", 0, 0, SND_STOP, 100);
		EMIT_SOUND_DYN(edict(), CHAN_STATIC, "doors/doormove6.wav", 1.0, 0.8, 0, 80);
		m_bRolling = true;
	}
	else if (m_bRolling && pev->velocity.Length2D() <= 1.0)
	{
		EMIT_SOUND_DYN(edict(), CHAN_STATIC, "doors/doormove6.wav", 0, 0, SND_STOP, 100);
		m_bRolling = false;
	}

	if (!m_bRolling)
	{
		if (pev->velocity.Length2D() > 1.0 && (pev->flags & FL_ONGROUND) != 0)
		{
			EMIT_SOUND_DYN(edict(), CHAN_STATIC, "doors/doormove6.wav", 0, 0, SND_STOP, 100);
			EMIT_SOUND_DYN(edict(), CHAN_STATIC, "doors/doormove6.wav", 1.0, 0.8, 0, 80);
			m_bRolling = true;
		}
	}

	bool bBounce = true;
	if (m_bRolling)
	{
		if ((pev->flags & FL_ONGROUND) == 0)
		{
			bBounce = false;
		}
		else
		{
			float flVolume = pev->velocity.Length2D() / 400.0 + 0.2;
			if (flVolume > 1.0)
				flVolume = 1.0;
			else
				flVolume = pev->velocity.Length2D() / 400.0 + 0.2;
			EMIT_SOUND_DYN(edict(), CHAN_STATIC, "doors/doormove6.wav", flVolume, 0.8, SND_CHANGE_VOL, 80);
		}
	}

	if (bBounce && (pev->flags & FL_ONGROUND) != 0)
	{
		if (PREFS_KeyLoopUpBool("DEATHBALL", "BouncyDeathball", false))
		{
			if (pev->teleport_time < gpGlobals->time)
			{
				pev->velocity.z = 0.5 * pev->velocity.z + PREFS_KeyLoopUpFloat("DEATHBALL", "BounceVelocity", 600.0);
				pev->teleport_time = PREFS_KeyLoopUpFloat("DEATHBALL", "BounceMinTime", 0.2) + gpGlobals->time;
				pev->flags &= ~FL_ONGROUND;
			}
		}
	}

	pev->avelocity.x = pev->velocity.x * 0.8;
	pev->avelocity.y = pev->velocity.y * 0.8;
}

// orig 0x118ae0
void CDeathBall::DBTouch(CBaseEntity* pOther)
{
	TraceResult tr = UTIL_GetGlobalTrace();

	if (!pOther->IsBSPModel())
	{
		// rolled into something fast enough: crush it
		if (tr.pHit == pOther->pev->pContainingEntity && pOther->IsAlive())
		{
			if (pev->dmgtime < gpGlobals->time)
			{
				float flSpeed = pev->velocity.Length();
				if (flSpeed > PREFS_KeyLoopUpFloat("DEATHBALL", "DamageVeloThreshold", 100.0))
				{
					int iCap = RC_CAP_DEATHBALL_CRUSH;
					if (pOther->HasRCCap(iCap))
					{
						float flDamage = PREFS_KeyLoopUpFloat("DEATHBALL", "DamageForVelo", 1.1) * pev->velocity.Length();
						if (pOther->pev->origin.z < pev->absmin.z) // under it ("Cursh" sic)
							flDamage = PREFS_KeyLoopUpFloat("DEATHBALL", "DamageExtraForCursh", 1.8) * flDamage;

						entvars_t* pevAttacker;
						if (m_flHitterTime <= gpGlobals->time || m_pHitter == NULL)
							pevAttacker = pev;
						else
							pevAttacker = VARS(m_pHitter);

						pOther->TakeDamage(pev, pevAttacker, flDamage, DMG_CRUSH | DMG_ALWAYSGIB);
						pev->dmgtime = gpGlobals->time + 0.2;
						return;
					}
				}
			}
		}

		// a player walking into it pushes it
		if (!pOther->IsPlayer())
			return;
		if (tr.pHit != pev->pContainingEntity)
			return;
		if (!pOther->IsAlive())
			return;

		pev->velocity = pev->velocity + pOther->pev->velocity * PREFS_KeyLoopUpFloat("DEATHBALL", "PushVeloScale", 0.15);
		m_flHitterTime = PREFS_KeyLoopUpFloat("DEATHBALL", "RemberHitterTime", 5.0) + gpGlobals->time;
		m_pHitter = pOther->pev->pContainingEntity;
		return;
	}

	if ((pev->flags & FL_ONGROUND) != 0 && m_bOnGround)
	{
		pev->velocity = pev->velocity * PREFS_KeyLoopUpFloat("DEATHBALL", "GroundFriction", 0.985);
		return;
	}

	// hit the world in the air
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND_DYN(edict(), CHAN_BODY, "gonarch/gon_step1.wav", 1.0, 0.8, 0, 100);
		break;
	case 1:
		EMIT_SOUND_DYN(edict(), CHAN_BODY, "gonarch/gon_step2.wav", 1.0, 0.8, 0, 100);
		break;
	case 2:
		EMIT_SOUND_DYN(edict(), CHAN_BODY, "gonarch/gon_step3.wav", 1.0, 0.8, 0, 100);
		break;
	}

	if (pev->velocity.Length() > 400.0)
	{
		float flRadius;
		if (pev->velocity.Length() / 800.0 > 1.0)
			flRadius = 628.0;
		else
			flRadius = (pev->velocity.Length() / 800.0) * 128.0 + 500.0;
		UTIL_ScreenShake(pev->origin, 4.0, 3.0, 1.0, flRadius);
	}

	if (PREFS_KeyLoopUpBool("DEATHBALL", "BouncyDeathball", false))
	{
		if (pev->teleport_time < gpGlobals->time)
		{
			pev->velocity = pev->velocity + tr.vecPlaneNormal * PREFS_KeyLoopUpFloat("DEATHBALL", "BounceVelocity", 600.0);
			pev->teleport_time = PREFS_KeyLoopUpFloat("DEATHBALL", "BounceMinTime", 0.2) + gpGlobals->time;
			pev->flags &= ~FL_ONGROUND;
		}
	}
}

// orig 0x1190e0: damage only pushes it, away from the inflictor
bool CDeathBall::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	Vector vecDir = g_vecZero;

	if (!FNullEnt(pevInflictor))
	{
		CBaseEntity* pInflictor = CBaseEntity::Instance(pevInflictor);
		if (pInflictor)
		{
			vecDir = (pInflictor->Center() - Vector(0, 0, 10) - Center()).Normalize();
			vecDir = g_vecAttackDir = vecDir.Normalize();
		}
	}

	pev->velocity = pev->velocity + (-vecDir * flDamage) * PREFS_KeyLoopUpFloat("DEATHBALL", "VeloForDamage", 4.0);

	if (pevAttacker)
	{
		m_flHitterTime = PREFS_KeyLoopUpFloat("DEATHBALL", "RemberHitterTime", 5.0) + gpGlobals->time;
		m_pHitter = pevAttacker->pContainingEntity;
	}

	return true;
}

// orig 0x1193e0
void CDeathBall::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	pev->velocity = pev->velocity + (vecDir * flDamage) * PREFS_KeyLoopUpFloat("DEATHBALL", "VeloForDamage", 4.0);
	UTIL_Sparks(ptr->vecEndPos);
	UTIL_Ricochet(ptr->vecEndPos, 1.0);

	if (pevAttacker)
	{
		m_flHitterTime = PREFS_KeyLoopUpFloat("DEATHBALL", "RemberHitterTime", 5.0) + gpGlobals->time;
		m_pHitter = pevAttacker->pContainingEntity;
	}
}

// orig 0x1194f0
void CDeathBall::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pHitter && m_pHitter == pEdict)
		m_pHitter = NULL;
}

// orig 0x119520: the bounding box ignores the angles
void CDeathBall::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector(-71, -71, -71);
	pev->absmax = pev->origin + Vector(71, 71, 71);
}

// orig 0x1195e0
bool CDeathBall::IsDriveent()
{
	return false;
}

// orig 0x1195f0: the killer's name in death messages
const char* CDeathBall::GetControllerName()
{
	return "a Deathball";
}

// orig 0x119610. VERIFY: which caps these bits are (rcbot or the drive code may name them)
int CDeathBall::GetRCCaps()
{
	return 0x1cc48;
}

// orig 0x119620
int CDeathBall::BloodColor()
{
	return DONT_BLEED;
}
