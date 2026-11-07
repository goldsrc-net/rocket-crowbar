/***
*
*	Rocket Crowbar 1.9: drivebm.cpp's functions other files call.
*
****/

#pragma once

// Map start (CWorld): in multiplayer, create the drive_bm_spawner that places the pickups.
void CreateDrvBMSpawner();

// Fire a gonarch_headcrabifier ring from vecOrigin along vecDir; what it catches becomes a
// headcrab owned by pEnemy (the Big Momma's driver).
void BMHCFCreate(edict_t* pOwner, edict_t* pEnemy, Vector vecOrigin, Vector vecDir);
