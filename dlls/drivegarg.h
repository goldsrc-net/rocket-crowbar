/***
*
*	Rocket Crowbar 1.9: drivegarg.cpp's functions other files call.
*
****/

#pragma once

// Map start (CWorld): in multiplayer, create the drive_garg_spawner that places the pickups.
void CreateDrvGargSpawner();

// The Gargantua death explosions (HL's SpawnExplosion): an env_explosion that does no damage,
// within randomRange of center, going off after flTime.
void DGargSpawnExplosion(Vector center, float randomRange, float flTime, int magnitude);
