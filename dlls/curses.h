/***
*
*	Rocket Crowbar 1.9: curses.cpp's interface for the rest of the game.
*
****/

#pragma once

class CBaseEntity;
class CBasePlayer;

extern int g_iCurseMSGColor[4]; // r, g, b, a of the "Cursed by:" HUD text

void CreateCurseSpawner();				 // multiplayer with mp_curses 1: makes the curse_spawner
bool CheckForCurseNearOrg(Vector vecOrg); // a curse or a player within 64 units
void DrawCursedMsg(CBasePlayer* pPlayer); // HUD list of the curses on pPlayer, every 0.5 s
void DropCurses(CBaseEntity* pEntity, Vector vecOrg); // empty in the original
