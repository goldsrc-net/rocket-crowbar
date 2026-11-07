/***
*
*	Rocket Crowbar 1.9: corpse.cpp. player_corpse, a dead body that plays out its animation and
*	holds the last frame.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"

class CPlayerCorpse : public CBaseMonster
{
public:
	void Spawn() override;
	int ObjectCaps() override;
	void EXPORT CThink();
};

LINK_ENTITY_TO_CLASS(player_corpse, CPlayerCorpse); // orig 0x10de90

// orig 0x10dec0
void CPlayerCorpse::Spawn()
{
	pev->classname = MAKE_STRING("corpse");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_TOSS;
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	SetThink(&CPlayerCorpse::CThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x10df80
void CPlayerCorpse::CThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_fSequenceFinished)
		DispatchAnimEvents(StudioFrameAdvance());
	else
		pev->frame = 255;
}

// orig 0x10dff0
int CPlayerCorpse::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}
