/***
*
*	Rocket Crowbar 1.9: pointexplo.cpp. CPointExplo, a grenade explosion at a point (no entity
*	factory; made only through ExploCreate).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "pointexplo.h"

// orig 0x1f7200
CPointExplo* CPointExplo::ExploCreate(edict_t* pOwner, Vector vecOrigin, Vector vecDir, float flDamage, string_t iszClassname)
{
	CPointExplo* pExplo = GetClassPtr((CPointExplo*)NULL);
	pExplo->pev->classname = iszClassname;
	pExplo->pev->owner = pOwner;

	if (vecDir == g_vecZero)
		vecDir = Vector(0, 0, -1);

	pExplo->pev->angles = vecDir;
	UTIL_SetOrigin(pExplo->pev, vecOrigin);
	pExplo->pev->dmg = flDamage;
	pExplo->Spawn();
	return pExplo;
}

// orig 0x1f7300
void CPointExplo::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SET_MODEL(edict(), "");
	pev->effects |= EF_NODRAW;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	SetTouch(NULL);
	SetThink(&CPointExplo::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;

	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + pev->angles * 32, ignore_monsters, edict(), &tr);
	Explode(&tr, DMG_BLAST);
}
