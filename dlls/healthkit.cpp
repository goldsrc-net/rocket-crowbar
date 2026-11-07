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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "items.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "weapons.h"
#include "Preff.h"

class CHealthKit : public CItem
{
	void Spawn() override;
	void Precache() override;
	bool MyTouch(CBasePlayer* pPlayer) override;

public:
	// Rocket Crowbar 1.9's bot interface (not virtual in the original)
	bool BotCanHaveItem(CBaseEntity* pBot);
	int GetBotWeight(CBot* pBot, CBaseEntity* pEnemy);

	/*
	int		Save( CSave &save ) override; 
	int		Restore( CRestore &restore ) override;
	
	static	TYPEDESCRIPTION m_SaveData[];
*/
};


LINK_ENTITY_TO_CLASS(item_healthkit, CHealthKit);

/*
TYPEDESCRIPTION	CHealthKit::m_SaveData[] = 
{

};


IMPLEMENT_SAVERESTORE( CHealthKit, CItem);
*/

void CHealthKit::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_medkit.mdl");

	CItem::Spawn();
}

void CHealthKit::Precache()
{
	PRECACHE_MODEL("models/w_medkit.mdl");
	PRECACHE_SOUND("items/smallmedkit1.wav");
}

// orig 0x1ab180. RC: the amount comes from rc.ini.
bool CHealthKit::MyTouch(CBasePlayer* pPlayer)
{
	// halflife-updated's fix, kept: a dead player doesn't take a kit; the original, as SDK 2.1, has
	// no check
	if (pPlayer->pev->deadflag != DEAD_NO)
	{
		return false;
	}

	if (pPlayer->TakeHealth(PREFS_KeyLoopUpFloat("ITEMS", "HealthkitGive", gSkillData.healthkitCapacity), DMG_GENERIC))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev);
		WRITE_STRING(STRING(pev->classname));
		MESSAGE_END();

		EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "items/smallmedkit1.wav", 1, ATTN_NORM);

		//TODO: incorrect check here, but won't respawn due to respawn delay being -1 in singleplayer
		if (0 != g_pGameRules->ItemShouldRespawn(this))
		{
			Respawn();
		}
		else
		{
			UTIL_Remove(this);
		}

		return true;
	}

	return false;
}



//-------------------------------------------------------------
// Wall mounted health kit
//-------------------------------------------------------------
class CWallHealth : public CBaseToggle
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

TYPEDESCRIPTION CWallHealth::m_SaveData[] =
	{
		DEFINE_FIELD(CWallHealth, m_flNextCharge, FIELD_TIME),
		DEFINE_FIELD(CWallHealth, m_iReactivate, FIELD_INTEGER),
		DEFINE_FIELD(CWallHealth, m_iJuice, FIELD_INTEGER),
		DEFINE_FIELD(CWallHealth, m_iOn, FIELD_INTEGER),
		DEFINE_FIELD(CWallHealth, m_flSoundTime, FIELD_TIME),
};

IMPLEMENT_SAVERESTORE(CWallHealth, CBaseEntity);

LINK_ENTITY_TO_CLASS(func_healthcharger, CWallHealth);


bool CWallHealth::KeyValue(KeyValueData* pkvd)
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

// orig 0x1ab630
void CWallHealth::Spawn()
{
	Precache();

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin); // set size and link into world
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	SET_MODEL(ENT(pev), STRING(pev->model));
	m_iJuice = (int)PREFS_KeyLoopUpFloat("ITEMS", "WallHealthMaxUnits", gSkillData.healthchargerCapacity); // RC: rc.ini
	pev->frame = 0;
}

void CWallHealth::Precache()
{
	PRECACHE_SOUND("items/medshot4.wav");
	PRECACHE_SOUND("items/medshotno1.wav");
	PRECACHE_SOUND("items/medcharge4.wav");
}


// orig 0x1ab770. RC: health per unit and the rate come from rc.ini.
void CWallHealth::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	// Make sure that we have a caller
	if (!pActivator)
		return;
	// if it's not a player, ignore
	if (!pActivator->IsPlayer())
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
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/medshotno1.wav", 1.0, ATTN_NORM);
		}
		return;
	}

	pev->nextthink = pev->ltime + 0.25;
	SetThink(&CWallHealth::Off);

	// Time to recharge yet?

	if (m_flNextCharge >= gpGlobals->time)
		return;

	// Play the on sound or the looping charging sound
	if (0 == m_iOn)
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/medshot4.wav", 1.0, ATTN_NORM);
		m_flSoundTime = 0.56 + gpGlobals->time;
	}
	if ((m_iOn == 1) && (m_flSoundTime <= gpGlobals->time))
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_STATIC, "items/medcharge4.wav", 1.0, ATTN_NORM);
	}


	// charge the player
	if (player->TakeHealth(PREFS_KeyLoopUpFloat("ITEMS", "WallHealthPerUnit", 1.0), DMG_GENERIC))
	{
		m_iJuice--;
	}

	// govern the rate of charge
	m_flNextCharge = gpGlobals->time + PREFS_KeyLoopUpFloat("ITEMS", "WallHealthGiveRate", 0.1);
}

void CWallHealth::Recharge()
{
	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/medshot4.wav", 1.0, ATTN_NORM);
	m_iJuice = (int)PREFS_KeyLoopUpFloat("ITEMS", "WallHealthMaxUnits", gSkillData.healthchargerCapacity); // orig 0x1ab9c0
	pev->frame = 0;
	SetThink(&CWallHealth::SUB_DoNothing);
}

void CWallHealth::Off()
{
	// Stop looping sound.
	if (m_iOn > 1)
		STOP_SOUND(ENT(pev), CHAN_STATIC, "items/medcharge4.wav");

	m_iOn = 0;

	if ((0 == m_iJuice) && ((m_iReactivate = g_pGameRules->FlHealthChargerRechargeTime()) > 0))
	{
		pev->nextthink = pev->ltime + m_iReactivate;
		SetThink(&CWallHealth::Recharge);
	}
	else
		SetThink(&CWallHealth::SUB_DoNothing);
}

// The original bot's object (CBot, defined only in the bot library): its entvars come first,
// as in dlls/weapons.cpp. The 16-bit field at +0x2c is compared with the same offset of this
// entity, which the game library never writes (engine-zeroed), so ours compares with 0.
// VERIFY: what the bot keeps at +0x2c.
static entvars_t* RCBotPev(CBot* pBot) { return *reinterpret_cast<entvars_t**>(pBot); }
static short RCBotField2c(CBot* pBot) { return *reinterpret_cast<short*>(reinterpret_cast<byte*>(pBot) + 0x2c); }

// orig 0x1ab2b0
bool CHealthKit::BotCanHaveItem(CBaseEntity* pBot)
{
	return pBot->pev->health < 100;
}

// orig 0x1ab2f0. Doesn't ask BotCanHaveItem.
int CHealthKit::GetBotWeight(CBot* pBot, CBaseEntity* pEnemy)
{
	int iScale = RCBotField2c(pBot) == 0 ? 4 : 1;
	float flHealth = RCBotPev(pBot)->health;

	if (flHealth < 10)
		return iScale * 100;
	if (flHealth < 25)
		return iScale * 50;
	if (flHealth < 100)
		return iScale * 5;
	return 0;
}

// orig 0x1abb40
bool CWallHealth::BotCanHaveItem(CBaseEntity* pBot)
{
	return m_iJuice > 0 && pBot->pev->health < pBot->pev->max_health;
}

// orig 0x1abb80
int CWallHealth::GetBotWeight(CBot* pBot, CBaseEntity* pEnemy)
{
	if (!BotCanHaveItem(reinterpret_cast<CBaseEntity*>(pBot)))
		return 0;

	int iScale = RCBotField2c(pBot) == 0 ? 4 : 1;
	float flHealth = RCBotPev(pBot)->health;

	if (flHealth < 10)
		return iScale * 55 * 2;
	if (flHealth < 25)
		return iScale * 55;
	if (flHealth < 100)
		return iScale * 3 * 2;
	return 0;
}

// orig 0x1abc40: asks BotCanHaveItem and ignores the answer.
void CWallHealth::BotUseMe(CBot* pBot)
{
	BotCanHaveItem(reinterpret_cast<CBaseEntity*>(pBot));
}
