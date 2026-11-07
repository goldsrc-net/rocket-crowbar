/***
*
*	Rocket Crowbar 1.9: the base of every monster a player can drive (drive*.cpp).
*
****/

#pragma once

#include "basemonster.h"

class CBasePlayer;

// The player's PreThink/PostThink hand over to the monster it drives (CBasePlayer::m_pCurDriveEnt)
// through these. Its original vtable adds exactly these six to CBaseMonster's, and overrides
// IsDriveent; the defaults below are the original's (inline in its header).
class CBaseDriveEnt : public CBaseMonster
{
public:
	// Every drive class has its driver first (orig +0x27c, saved by each class's own table), and
	// other drive files read it through any drive monster, so it lives here.
	CBasePlayer* m_pOwner = nullptr;

	bool IsDriveent() override { return true; }

	virtual void DEClientPreThink() {}
	virtual void DEClientPostThink() {}
	virtual void DEClientEnter(CBasePlayer* pPlayer) {}
	virtual void DEClientLeave() {}
	virtual void DEClientEject() {}
	virtual Vector GetCamRotOrg() { return Center(); } // where the driver's camera orbits
};
