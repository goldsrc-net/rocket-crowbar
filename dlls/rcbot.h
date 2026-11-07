/***
*
*	Rocket Crowbar 1.9: what the game library reads of RC's bot object (CBot, weapons.h).
*
*	The bot lived in the bot library RC shipped (rcbot.dll), so CBot is never defined here. The
*	original weapons read it by the original i386 bot's offsets and call its virtuals by the
*	original's vtable slots; neither holds for anything built against our layout
*	(reference/DEVIATIONS.md, "Bot interface"). These helpers keep each read in one place.
*
****/

#pragma once

// Its first member is the bot's entvars.
inline entvars_t* RCBotPev(CBot* pBot) { return *reinterpret_cast<entvars_t**>(pBot); }

// The float at +0x698 gates firing (CBasePlayerWeapon::BotWeaponFrame). VERIFY name
inline float RCBotFireGate(CBot* pBot) { return *reinterpret_cast<float*>(reinterpret_cast<byte*>(pBot) + 0x698); }

// The int at +0x6a0: the weapons only test it for 3 (the python's and mp5's secondary fire
// need it). VERIFY name and meaning
inline int RCBotState(CBot* pBot) { return *reinterpret_cast<int*>(reinterpret_cast<byte*>(pBot) + 0x6a0); }

// The original calls the bot's virtual Center() (CBasePlayer's slot 50, which is
// CBaseEntity::Center). This is CBaseEntity::Center's body on the bot's entvars.
// VERIFY: the bot could override Center
inline Vector RCBotCenter(CBot* pBot)
{
	entvars_t* pev = RCBotPev(pBot);
	return (pev->absmax + pev->absmin) * 0.5;
}
