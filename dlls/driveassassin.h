/***
*
*	Rocket Crowbar 1.9: driveassassin.cpp, the drivable female assassin (drive_assassin).
*
****/

#pragma once

// Creates the drive_assassin_spawner once per map in multiplayer; it spawns mp_driveassassin
// pickup_drive_assassin entities 15 seconds later. Called by the game rules / world setup.
void CreateDrvAssassinSpawner();
