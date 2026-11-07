/***
*
*	Rocket Crowbar 1.9: auth_stuff.cpp.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"

bool ClientConnecting(edict_t* pEntity);

// orig 0xebea0. Reads sv_reserved and ignores it: every client may connect. Nothing in the game
// library calls it. VERIFY: a stub left from a reserved-slots check.
bool ClientConnecting(edict_t* pEntity)
{
	CVAR_GET_FLOAT("sv_reserved");
	return true;
}
