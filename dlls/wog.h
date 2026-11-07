/***
*
*	Rocket Crowbar 1.9: wog.cpp's interface for the rest of the game.
*
****/

#pragma once

void FireWOG(Vector vecOrg, edict_t* pOwner); // the strike itself
void WOGPrecache();
void WOGCalledFireForward(edict_t* pEdict, bool bSeed);
void WOGCalledFireOnPlayer(edict_t* pEdict, int iIndex);
void WOGCalledFireOnAll(edict_t* pEdict, char* szClassname);
void WOGInstantFire(edict_t* pEdict);
bool WOGCommand(edict_t* pEdict); // the client command; true when the password matched
