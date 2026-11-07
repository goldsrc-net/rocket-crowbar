/***
*
*	Rocket Crowbar 1.9: driveapache.cpp, the drivable Apache (drive_apache).
*
****/

#pragma once

// Creates the drive_apache_spawner once per map in multiplayer; it spawns mp_driveapache
// pickup_drive_apache entities 15 seconds later. Called by the game rules / world setup.
void CreateDrvApachSpawner();
