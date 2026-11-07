/***
*
*	Rocket Crowbar 1.9: pointexplo.cpp's point explosion.
*
****/

#pragma once

#include "weapons.h"

// A grenade explosion with no grenade: explodes against whatever lies 32 units along vecDir
// (straight down when vecDir is zero). pev->classname is the death-message name it is given.
class CPointExplo : public CGrenade
{
public:
	static CPointExplo* ExploCreate(edict_t* pOwner, Vector vecOrigin, Vector vecDir, float flDamage, string_t iszClassname);
	void Spawn() override;
};
