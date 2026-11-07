/***
*
*	Rocket Crowbar 1.9: levelcustom.cpp's interface (the per-map .lec entity configs).
*
****/

#pragma once

#include <cstdio>

class CBaseEntity;

void CreateLCEnt(); // deathmatch: precaches the map's .lec and makes the lc_init_ent that runs it
CBaseEntity* FindOtherSameSpawner(const char* szClassname);
int LC_ReadLine(FILE* pFile, char* szLine, int iSize);
bool LC_CheckStartEqu(char* szLine, char* szCommand);
void LC_ConfOppAll(char* szClassname, bool bRemove);
void LC_ConfOppOnEnt(char* szClassname, Vector vecOrg, bool bRemove);
void LC_ConfMoveEnt(char* szClassname, Vector vecFind, Vector vecOrigin, Vector vecAngles);
void LC_ConfUseEnt(char* szClassname, Vector vecOrg);
bool LC_ExecConf(char* szFile);
bool LC_PrecacheConf(char* szFile);
