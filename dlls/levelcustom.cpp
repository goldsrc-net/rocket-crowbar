/***
*
*	Rocket Crowbar 1.9: levelcustom.cpp. Per-map entity configs: <gamedir>/maps/<map>.lec, one
*	command per line, '#' for a comment:
*
*	  removeall <classname>       remove every entity of the class ("*prefix" matches a prefix)
*	  killall <classname>         kill them instead (50000 damage, then Killed)
*	  entremove <classname> x y z remove the one within 8 units of x y z
*	  entkill <classname> x y z   kill it
*	  move <classname> x y z  nx ny nz  ax ay az   move the one within 8 units to n, angles a
*	  create <classname> x y z ax ay az            spawn one (through an lc_spawner)
*	  respawncreate <classname> x y z ax ay az     spawn one that comes back 1 s after it dies
*	  useent <classname> x y z    Use() the one within 64 units
*
*	CreateLCEnt precaches what the file creates; lc_init_ent runs it 2 s into the map.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "gamerules.h"
#include "levelcustom.h"

#include <cstdio>

//=========================================================
// CLCEnt (lc_init_ent): runs the map's .lec, then removes itself
//=========================================================
class CLCEnt : public CBaseEntity
{
public:
	void Spawn() override;
	void EXPORT LCInitThink();
};

LINK_ENTITY_TO_CLASS(lc_init_ent, CLCEnt); // orig 0x1bd840

// orig 0x1bd870
void CLCEnt::LCInitThink()
{
	char szGameDir[512];
	char szPath[512];

	GET_GAME_DIR(szGameDir);
	sprintf(szPath, "%s/maps/%s.lec", szGameDir, STRING(gpGlobals->mapname));
	LC_ExecConf(szPath);
	SetThink(&CLCEnt::SUB_Remove);
	pev->nextthink = gpGlobals->time + 1.0;
}

// orig 0x1bd910
void CLCEnt::Spawn()
{
	pev->classname = MAKE_STRING("lc_init_ent");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CLCEnt::LCInitThink);
	pev->nextthink = gpGlobals->time + 2.0;
}

// orig 0x1bd9b0
void CreateLCEnt()
{
	if (!g_pGameRules->IsDeathmatch())
		return;
	if (UTIL_FindEntityByClassname(NULL, "lc_init_ent") != NULL)
		return;

	char szGameDir[512];
	char szPath[512];

	CBaseEntity::Create("lc_init_ent", g_vecZero, g_vecZero, NULL);
	GET_GAME_DIR(szGameDir);
	sprintf(szPath, "%s/maps/%s.lec", szGameDir, STRING(gpGlobals->mapname));
	LC_PrecacheConf(szPath);
}

//=========================================================
// CLCReSpawner (lc_respawner): keeps one pev->netname entity alive. The entity gets this as its
// owner, so its death calls DeathNotice; it is named pev->noise, which is shared by every
// respawner of the same class (or "lcnetname-<index>" for the first).
//=========================================================
class CLCReSpawner : public CBaseEntity
{
public:
	void Spawn() override;
	void DeathNotice(entvars_t* pevChild) override;
	void EXPORT LCRSThink();

	bool m_bSpawned;	   // orig +0x34, its entity is alive. VERIFY name
	float m_flRespawnTime; // orig +0x38. VERIFY name
};

LINK_ENTITY_TO_CLASS(lc_respawner, CLCReSpawner); // orig 0x1bda60

// orig 0x1bda90: the first lc_respawner of that class, which may be the caller itself
CBaseEntity* FindOtherSameSpawner(const char* szClassname)
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = UTIL_FindEntityByClassname(pEntity, "lc_respawner")) != NULL)
	{
		if (stricmp(szClassname, STRING(pEntity->pev->netname)) == 0)
			return pEntity;
	}

	return NULL;
}

// orig 0x1bdb00
void CLCReSpawner::LCRSThink()
{
	pev->nextthink = gpGlobals->time + 0.5;

	if (m_bSpawned)
		return;
	if (m_flRespawnTime > gpGlobals->time)
		return;

	if (FStringNull(pev->noise))
	{
		CBaseEntity* pOther = FindOtherSameSpawner(STRING(pev->netname));
		if (pOther == NULL)
		{
			char szName[64];
			sprintf(szName, "lcnetname-%i", ENTINDEX(pev->pContainingEntity));
			pev->noise = ALLOC_STRING(szName);
		}
		else
		{
			pev->noise = pOther->pev->noise;
		}
	}

	CBaseEntity* pEntity = CBaseEntity::Create(STRING(pev->netname), pev->origin, pev->angles, pev->pContainingEntity);
	if (pEntity)
	{
		pEntity->pev->flags |= FL_ONGROUND | FL_DORMANT;
		pEntity->pev->netname = pev->noise;
		m_bSpawned = true;
	}
}

// orig 0x1bdc40: its entity died
void CLCReSpawner::DeathNotice(entvars_t* pevChild)
{
	m_bSpawned = false;
	m_flRespawnTime = gpGlobals->time + 1.0;
}

// orig 0x1bdc80
void CLCReSpawner::Spawn()
{
	pev->classname = MAKE_STRING("lc_respawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->netname = MAKE_STRING("monster_headcrab"); // LC_ExecConf sets the real class after Spawn
	pev->noise = 0;
	m_bSpawned = false;
	m_flRespawnTime = gpGlobals->time + 1.0;
	SetThink(&CLCReSpawner::LCRSThink);
	pev->nextthink = gpGlobals->time + 0.5;
}

//=========================================================
// CLCSpawner (lc_spawner): makes one pev->netname entity 0.5 s in, owned by the world, then
// removes itself
//=========================================================
class CLCSpawner : public CBaseEntity
{
public:
	void Spawn() override;
	void EXPORT LCSThink();
};

LINK_ENTITY_TO_CLASS(lc_spawner, CLCSpawner); // orig 0x1bdd60

// orig 0x1bdd90
void CLCSpawner::LCSThink()
{
	pev->nextthink = gpGlobals->time + 1.5;
	SetThink(&CLCSpawner::SUB_Remove);

	CBaseEntity* pEntity = CBaseEntity::Create(STRING(pev->netname), pev->origin, pev->angles, INDEXENT(0));
	if (pEntity)
		pEntity->pev->flags |= FL_ONGROUND | FL_DORMANT;
}

// orig 0x1bde30
void CLCSpawner::Spawn()
{
	pev->classname = MAKE_STRING("lc_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->netname = MAKE_STRING("info_player_deathmatch"); // LC_ExecConf sets the real class after Spawn
	SetThink(&CLCSpawner::LCSThink);
	pev->nextthink = gpGlobals->time + 0.5;
}

//=========================================================
// the .lec reader
//=========================================================

// orig 0x1bdef0: reads a line, cuts it at ';' and squeezes whitespace runs to single spaces
// (dropping leading ones). -1 at end of file, 1 when anything is left, else 0. Nothing calls it.
int LC_ReadLine(FILE* pFile, char* szLine, int iSize)
{
	if (feof(pFile))
		return -1;

	fgets(szLine, iSize, pFile);
	szLine[iSize - 1] = 0;

	if (strlen(szLine) > 1)
	{
		bool bLastWasChar = true;
		bool bAnyChar = false;
		int iChars = 0;
		char* pOut = szLine;

		for (char* pIn = szLine; *pIn != 0 && *pIn != ';'; pIn++)
		{
			if (*pIn == ' ' || *pIn == '\t' || *pIn == '\n')
			{
				if (bAnyChar)
					bLastWasChar = false;
			}
			else
			{
				if (!bLastWasChar)
					*pOut++ = ' ';
				*pOut++ = *pIn;
				bLastWasChar = true;
				bAnyChar = true;
				iChars++;
			}
		}
		*pOut = 0;

		if (iChars > 0)
			return 1;
	}

	return 0;
}

// orig 0x1bdfc0: szLine starts with szCommand, any case
bool LC_CheckStartEqu(char* szLine, char* szCommand)
{
	return strnicmp(szLine, szCommand, strlen(szCommand)) == 0;
}

// orig 0x1be010: every entity of szClassname ("*prefix" for a prefix match)
void LC_ConfOppAll(char* szClassname, bool bRemove)
{
	const char cFirst = szClassname[0];
	if (cFirst == '*')
		szClassname++;

	edict_t* pEdict = INDEXENT(1);
	if (pEdict == NULL)
		return;

	for (int i = 1; i < gpGlobals->maxEntities; i++, pEdict++)
	{
		if (pEdict->free)
			continue;

		int iDiff;
		if (cFirst == '*')
			iDiff = strncmp(STRING(pEdict->v.classname), szClassname, strlen(szClassname));
		else
			iDiff = strcmp(STRING(pEdict->v.classname), szClassname);
		if (iDiff != 0)
			continue;

		CBaseEntity* pEntity = CBaseEntity::Instance(pEdict);
		if (pEntity == NULL)
			continue;

		if (bRemove)
		{
			pEntity->SUB_Remove();
		}
		else
		{
			pEntity->TakeDamage(pEntity->pev, pEntity->pev, 50000.0, DMG_GENERIC);
			if (pEntity && !FNullEnt(pEntity->pev) && UTIL_IsValidEntity(pEntity->pev->pContainingEntity) && pEntity->pev->deadflag == DEAD_NO)
				pEntity->Killed(pEntity->pev, GIB_NORMAL);
		}
	}
}

// orig 0x1be1e0: the szClassname entity within 8 units of vecOrg
void LC_ConfOppOnEnt(char* szClassname, Vector vecOrg, bool bRemove)
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecOrg, 8.0)) != NULL)
	{
		if (strcmp(STRING(pEntity->pev->classname), szClassname) == 0)
			break;
	}

	if (pEntity == NULL)
		return;

	if (!bRemove)
	{
		pEntity->TakeDamage(pEntity->pev, pEntity->pev, 50000.0, DMG_GENERIC);
		if (pEntity && !FNullEnt(pEntity->pev) && UTIL_IsValidEntity(pEntity->pev->pContainingEntity) && pEntity->pev->deadflag == DEAD_NO)
			pEntity->Killed(pEntity->pev, GIB_NORMAL);
		return;
	}

	pEntity->SUB_Remove();
}

// orig 0x1be2e0
void LC_ConfMoveEnt(char* szClassname, Vector vecFind, Vector vecOrigin, Vector vecAngles)
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecFind, 8.0)) != NULL)
	{
		if (strcmp(STRING(pEntity->pev->classname), szClassname) == 0)
			break;
	}

	if (pEntity == NULL)
		return;

	pEntity->pev->origin = vecOrigin;
	pEntity->pev->v_angle = g_vecZero;
	pEntity->pev->velocity = g_vecZero;
	pEntity->pev->angles = vecAngles;
	pEntity->pev->punchangle = g_vecZero;
	pEntity->pev->fixangle = 1;
	UTIL_SetOrigin(pEntity->pev, pEntity->pev->origin);
	pEntity->pev->velocity = g_vecZero;
	pEntity->pev->fixangle = 2;
	pEntity->pev->flags &= ~FL_ONGROUND;
}

// orig 0x1be400
void LC_ConfUseEnt(char* szClassname, Vector vecOrg)
{
	CBaseEntity* pEntity = NULL;

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecOrg, 64.0)) != NULL)
	{
		if (strcmp(STRING(pEntity->pev->classname), szClassname) == 0)
			break;
	}

	if (pEntity != NULL)
		pEntity->Use(pEntity, pEntity, USE_TOGGLE, 0);
}

// orig 0x1be480: false when the file can't be opened
bool LC_ExecConf(char* szFile)
{
	FILE* pFile = fopen(szFile, "r");
	if (pFile == NULL)
		return false;

	char szLine[512];
	char szCommand[256];
	char szClassname[768];
	Vector vecOrg, vecOrigin, vecAngles;

	while (!feof(pFile))
	{
		memset(szLine, 0, sizeof(szLine));
		if (fgets(szLine, sizeof(szLine), pFile) == NULL)
			continue;
		if (strlen(szLine) <= 1 || szLine[0] == '#')
			continue;

		if (LC_CheckStartEqu(szLine, (char*)"removeall"))
		{
			sscanf(szLine, "%s %s", szCommand, szClassname);
			LC_ConfOppAll(szClassname, true);
		}
		else if (LC_CheckStartEqu(szLine, (char*)"killall"))
		{
			sscanf(szLine, "%s %s", szCommand, szClassname);
			LC_ConfOppAll(szClassname, false);
		}
		else if (LC_CheckStartEqu(szLine, (char*)"entremove"))
		{
			sscanf(szLine, "%s %s %f %f %f", szCommand, szClassname, &vecOrg.x, &vecOrg.y, &vecOrg.z);
			LC_ConfOppOnEnt(szClassname, vecOrg, true);
		}
		else if (LC_CheckStartEqu(szLine, (char*)"entkill"))
		{
			sscanf(szLine, "%s %s %f %f %f", szCommand, szClassname, &vecOrg.x, &vecOrg.y, &vecOrg.z);
			LC_ConfOppOnEnt(szClassname, vecOrg, false);
		}
		else if (LC_CheckStartEqu(szLine, (char*)"move"))
		{
			sscanf(szLine, "%s %s %f %f %f %f %f %f %f %f %f", szCommand, szClassname, &vecOrg.x, &vecOrg.y, &vecOrg.z,
				&vecOrigin.x, &vecOrigin.y, &vecOrigin.z, &vecAngles.x, &vecAngles.y, &vecAngles.z);
			LC_ConfMoveEnt(szClassname, vecOrg, vecOrigin, vecAngles);
		}
		else if (LC_CheckStartEqu(szLine, (char*)"create"))
		{
			sscanf(szLine, "%s %s %f %f %f %f %f %f", szCommand, szClassname, &vecOrg.x, &vecOrg.y, &vecOrg.z,
				&vecAngles.x, &vecAngles.y, &vecAngles.z);
			CBaseEntity* pSpawner = CBaseEntity::Create("lc_spawner", vecOrg, vecAngles, NULL);
			pSpawner->pev->netname = ALLOC_STRING(szClassname); // no NULL check in the original
		}
		else if (LC_CheckStartEqu(szLine, (char*)"respawncreate"))
		{
			sscanf(szLine, "%s %s %f %f %f %f %f %f", szCommand, szClassname, &vecOrg.x, &vecOrg.y, &vecOrg.z,
				&vecAngles.x, &vecAngles.y, &vecAngles.z);
			CBaseEntity* pSpawner = CBaseEntity::Create("lc_respawner", vecOrg, vecAngles, NULL);
			pSpawner->pev->netname = ALLOC_STRING(szClassname); // no NULL check in the original
		}
		else if (LC_CheckStartEqu(szLine, (char*)"useent"))
		{
			sscanf(szLine, "%s %s %f %f %f", szCommand, szClassname, &vecOrg.x, &vecOrg.y, &vecOrg.z);
			LC_ConfUseEnt(szClassname, vecOrg);
		}
	}

	fclose(pFile);
	return true;
}

// orig 0x1be9e0: precaches every class the file creates
bool LC_PrecacheConf(char* szFile)
{
	FILE* pFile = fopen(szFile, "r");
	if (pFile == NULL)
		return false;

	char szLine[512];
	char szCommand[256];
	char szClassname[768];

	while (!feof(pFile))
	{
		memset(szLine, 0, sizeof(szLine));
		if (fgets(szLine, sizeof(szLine), pFile) == NULL)
			continue;
		if (strlen(szLine) <= 1 || szLine[0] == '#')
			continue;

		if (LC_CheckStartEqu(szLine, (char*)"create") || LC_CheckStartEqu(szLine, (char*)"respawncreate"))
		{
			sscanf(szLine, "%s %s", szCommand, szClassname);
			UTIL_PrecacheOther(szClassname);
		}
	}

	fclose(pFile);
	return true;
}
