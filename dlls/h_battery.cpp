/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== h_battery.cpp ========================================================

  battery-related code

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"
#include "skill.h"
#include "gamerules.h"
#include "weapons.h"
#include "Preff.h"

class CRecharge : public CBaseToggle
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT Off();
	void EXPORT Recharge();
	bool KeyValue(KeyValueData* pkvd) override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int ObjectCaps() override { return (CBaseToggle::ObjectCaps() | FCAP_CONTINUOUS_USE) & ~FCAP_ACROSS_TRANSITION; }
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;

	// Rocket Crowbar 1.9's bot interface (not virtual in the original)
	bool BotCanHaveItem(CBaseEntity* pBot);
	int GetBotWeight(CBot* pBot, CBaseEntity* pEnemy);
	void BotUseMe(CBot* pBot);

	static TYPEDESCRIPTION m_SaveData[];

	float m_flNextCharge;
	int m_iReactivate; // DeathMatch Delay until reactvated
	int m_iJuice;
	int m_iOn; // 0 = off, 1 = startup, 2 = going
	float m_flSoundTime;
};

TYPEDESCRIPTION CRecharge::m_SaveData[] =
	{
		DEFINE_FIELD(CRecharge, m_flNextCharge, FIELD_TIME),
		DEFINE_FIELD(CRecharge, m_iReactivate, FIELD_INTEGER),
		DEFINE_FIELD(CRecharge, m_iJuice, FIELD_INTEGER),
		DEFINE_FIELD(CRecharge, m_iOn, FIELD_INTEGER),
		DEFINE_FIELD(CRecharge, m_flSoundTime, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CRecharge, CBaseEntity);

LINK_ENTITY_TO_CLASS(func_recharge, CRecharge);


bool CRecharge::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "style") ||
		FStrEq(pkvd->szKeyName, "height") ||
		FStrEq(pkvd->szKeyName, "value1") ||
		FStrEq(pkvd->szKeyName, "value2") ||
		FStrEq(pkvd->szKeyName, "value3"))
	{
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "dmdelay"))
	{
		m_iReactivate = atoi(pkvd->szValue);
		return true;
	}

	return CBaseToggle::KeyValue(pkvd);
}

// orig 0x1a1370
void CRecharge::Spawn()
{
	Precache();

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin); // set size and link into world
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	SET_MODEL(ENT(pev), STRING(pev->model));
	m_iJuice = (int)PREFS_KeyLoopUpFloat("ITEMS", "WallChargerGive", gSkillData.suitchargerCapacity); // RC: rc.ini
	pev->frame = 0;
}

void CRecharge::Precache()
{
	PRECACHE_SOUND("items/suitcharge1.wav");
	PRECACHE_SOUND("items/suitchargeno1.wav");
	PRECACHE_SOUND("items/suitchargeok1.wav");
}


// orig 0x1a14b0. RC: the armor cap and charge rate come from rc.ini.
void CRecharge::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// if it's not a player, ignore
	if (!FClassnameIs(pActivator->pev, "player"))
		return;

	auto player = static_cast<CBasePlayer*>(pActivator);

	// if there is no juice left, turn it off
	if (m_iJuice <= 0)
	{
		pev->frame = 1;
		Off();
	}

	// if the player doesn't have the suit, or there is no juice left, make the deny noise
	if ((m_iJuice <= 0) || !player->HasSuit())
	{
		if (m_flSoundTime <= gpGlobals->time)
		{
			m_flSoundTime = gpGlobals->time + 0.62;
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/suitchargeno1.wav", 0.85, ATTN_NORM);
		}
		return;
	}

	pev->nextthink = pev->ltime + 0.25;
	SetThink(&CRecharge::Off);

	// Time to recharge yet?

	if (m_flNextCharge >= gpGlobals->time)
		return;

	// Make sure that we have a caller
	//TODO: useless, it's accessed earlier on.
	if (!pActivator)
		return;

	m_hActivator = pActivator;

	//only recharge the player

	//TODO: put this check at the top.
	if (!m_hActivator->IsPlayer())
		return;

	// Play the on sound or the looping charging sound
	if (0 == m_iOn)
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/suitchargeok1.wav", 0.85, ATTN_NORM);
		m_flSoundTime = 0.56 + gpGlobals->time;
	}
	if ((m_iOn == 1) && (m_flSoundTime <= gpGlobals->time))
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_STATIC, "items/suitcharge1.wav", 0.85, ATTN_NORM);
	}


	// charge the player
	if (m_hActivator->pev->armorvalue < PREFS_KeyLoopUpIntClamp("PLAYER", "MaxArmor", 100, 0, 999))
	{
		m_iJuice--;
		m_hActivator->pev->armorvalue += 1;

		if (m_hActivator->pev->armorvalue > PREFS_KeyLoopUpIntClamp("PLAYER", "MaxArmor", 100, 0, 999))
			m_hActivator->pev->armorvalue = PREFS_KeyLoopUpIntClamp("PLAYER", "MaxArmor", 100, 0, 999);
	}

	// govern the rate of charge
	m_flNextCharge = gpGlobals->time + PREFS_KeyLoopUpFloat("ITEMS", "WallChargerRate", 0.1);
}

// orig 0x1a17f0
void CRecharge::Recharge()
{
	m_iJuice = (int)PREFS_KeyLoopUpFloat("ITEMS", "WallChargerGive", gSkillData.suitchargerCapacity);
	pev->frame = 0;
	SetThink(&CRecharge::SUB_DoNothing);
}

void CRecharge::Off()
{
	// Stop looping sound.
	if (m_iOn > 1)
		STOP_SOUND(ENT(pev), CHAN_STATIC, "items/suitcharge1.wav");

	m_iOn = 0;

	if ((0 == m_iJuice) && ((m_iReactivate = g_pGameRules->FlHEVChargerRechargeTime()) > 0))
	{
		pev->nextthink = pev->ltime + m_iReactivate;
		SetThink(&CRecharge::Recharge);
	}
	else
		SetThink(&CRecharge::SUB_DoNothing);
}

// The original bot's object (CBot, defined only in the bot library): it begins like an entity
// (its entvars first), and these read its 16-bit field at +0x2c, compared with the same offset
// of this entity. That offset of CBaseEntity is never written by the game library (memory the
// engine zeroes), so ours compares with 0. VERIFY: what the bot keeps at +0x2c.
static short RCBotField2c(CBot* pBot) { return *reinterpret_cast<short*>(reinterpret_cast<byte*>(pBot) + 0x2c); }

// orig 0x1a1950. The armor cap here clamps to 250 (Use clamps it to 999).
bool CRecharge::BotCanHaveItem(CBaseEntity* pBot)
{
	return m_iJuice > 0 && pBot->pev->armorvalue < PREFS_KeyLoopUpIntClamp("PLAYER", "MaxArmor", 100, 0, 250);
}

// orig 0x1a19c0
int CRecharge::GetBotWeight(CBot* pBot, CBaseEntity* pEnemy)
{
	if (!BotCanHaveItem(reinterpret_cast<CBaseEntity*>(pBot)))
		return 0;

	return RCBotField2c(pBot) == 0 ? 50 : 40;
}

// orig 0x1a1a10: asks BotCanHaveItem and ignores the answer.
void CRecharge::BotUseMe(CBot* pBot)
{
	BotCanHaveItem(reinterpret_cast<CBaseEntity*>(pBot));
}
