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

===== player.cpp ========================================================

  functions dealing with the player

*/

#include <limits>
#include <algorithm>

#include "extdll.h"
#include "util.h"

#include "cbase.h"
#include "player.h"
#include "trains.h"
#include "nodes.h"
#include "weapons.h"
#include "hornetgun.h"
#include "soundent.h"
#include "monsters.h"
#include "shake.h"
#include "decals.h"
#include "gamerules.h"
#include "game.h"
#include "pm_shared.h"
#include "hltv.h"
#include "UserMessages.h"
#include "client.h"
#include "driveent.h"
#include "flag.h"
#include "curses.h"
#include "pregame.h"
#include "Preff.h"

// #define DUCKFIX

extern void CopyToBodyQue(entvars_t* pev);
extern edict_t* EntSelectSpawnPoint(CBaseEntity* pPlayer);

extern bool IsBustingGame();

// Rocket Crowbar 1.9: from other files
extern const char* GetTeamNameForIndex(int iTeam); // teamplay_gamerules.cpp (RC)
extern bool IsClientListenServer(edict_t* pEntity); // client.cpp (RC)
extern short FixedSigned16(float value, float scale); // util.cpp
extern unsigned short FixedUnsigned16(float value, float scale); // util.cpp
void DestroyFreezeTimer(entvars_t* pev);   // hornetgun.cpp (RC)
void DestroyFrictionTimer(entvars_t* pev); // handgrenade.cpp (RC)

// Rocket Crowbar 1.9: this file's RC free functions
int MapTextureTypeStepType(char chTextureType);
void DoPlayerShockwaveEffect(Vector vecOrigin);
CBaseEntity* PickRandomSpawnOfClassname(const char* szClassname, CBaseEntity* pPlayer, bool bTeam, bool& bForced);
edict_t* DoEntSpawnPointStuff(const char* szClassname, CBaseEntity* pPlayer, bool bTeam);
CBaseEntity* FindEntityForward(CBaseEntity* pMe);
bool AllowToTransform(edict_t* pPlayer);

#define TRAIN_ACTIVE 0x80
#define TRAIN_NEW 0xc0
#define TRAIN_OFF 0x00
#define TRAIN_NEUTRAL 0x01
#define TRAIN_SLOW 0x02
#define TRAIN_MEDIUM 0x03
#define TRAIN_FAST 0x04
#define TRAIN_BACK 0x05

#define FLASH_DRAIN_TIME 1.2  //100 units/3 minutes
#define FLASH_CHARGE_TIME 0.2 // 100 units/20 seconds  (seconds per unit)

// Global Savedata for player
TYPEDESCRIPTION CBasePlayer::m_playerSaveData[] =
	{
		DEFINE_FIELD(CBasePlayer, m_flFlashLightTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_iFlashBattery, FIELD_INTEGER),

		DEFINE_FIELD(CBasePlayer, m_afButtonLast, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_afButtonPressed, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_afButtonReleased, FIELD_INTEGER),

		DEFINE_ARRAY(CBasePlayer, m_rgItems, FIELD_INTEGER, MAX_ITEMS),
		DEFINE_FIELD(CBasePlayer, m_afPhysicsFlags, FIELD_INTEGER),

		DEFINE_FIELD(CBasePlayer, m_flTimeStepSound, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_flTimeWeaponIdle, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_flSwimTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_flDuckTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_flWallJumpTime, FIELD_TIME),

		DEFINE_FIELD(CBasePlayer, m_flSuitUpdate, FIELD_TIME),
		DEFINE_ARRAY(CBasePlayer, m_rgSuitPlayList, FIELD_INTEGER, CSUITPLAYLIST),
		DEFINE_FIELD(CBasePlayer, m_iSuitPlayNext, FIELD_INTEGER),
		DEFINE_ARRAY(CBasePlayer, m_rgiSuitNoRepeat, FIELD_INTEGER, CSUITNOREPEAT),
		DEFINE_ARRAY(CBasePlayer, m_rgflSuitNoRepeatTime, FIELD_TIME, CSUITNOREPEAT),
		DEFINE_FIELD(CBasePlayer, m_lastDamageAmount, FIELD_INTEGER),

		DEFINE_ARRAY(CBasePlayer, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES),
		DEFINE_FIELD(CBasePlayer, m_pActiveItem, FIELD_CLASSPTR),
		DEFINE_FIELD(CBasePlayer, m_pLastItem, FIELD_CLASSPTR),
		DEFINE_FIELD(CBasePlayer, m_WeaponBits, FIELD_INT64),

		DEFINE_ARRAY(CBasePlayer, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS),
		DEFINE_FIELD(CBasePlayer, m_idrowndmg, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_idrownrestored, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_tSneaking, FIELD_TIME),

		DEFINE_FIELD(CBasePlayer, m_iTrain, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_bitsHUDDamage, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_flFallVelocity, FIELD_FLOAT),
		DEFINE_FIELD(CBasePlayer, m_iTargetVolume, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_iWeaponVolume, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_iExtraSoundTypes, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_iWeaponFlash, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_fLongJump, FIELD_BOOLEAN),
		DEFINE_FIELD(CBasePlayer, m_fInitHUD, FIELD_BOOLEAN),
		DEFINE_FIELD(CBasePlayer, m_tbdPrev, FIELD_TIME),

		DEFINE_FIELD(CBasePlayer, m_pTank, FIELD_EHANDLE),
		DEFINE_FIELD(CBasePlayer, m_hViewEntity, FIELD_EHANDLE),
		DEFINE_FIELD(CBasePlayer, m_iHideHUD, FIELD_INTEGER),
		DEFINE_FIELD(CBasePlayer, m_iFOV, FIELD_INTEGER),

		DEFINE_FIELD(CBasePlayer, m_SndRoomtype, FIELD_INTEGER),
		// Don't save these. Let the game recalculate the closest env_sound, and continue to use the last room type like it always has.
		//DEFINE_FIELD(CBasePlayer, m_SndLast, FIELD_EHANDLE),
		//DEFINE_FIELD(CBasePlayer, m_flSndRange, FIELD_FLOAT),

		DEFINE_FIELD(CBasePlayer, m_flStartCharge, FIELD_TIME),

		// Rocket Crowbar 1.9, in the original's order
		DEFINE_FIELD(CBasePlayer, m_flIDTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_blInVote, FIELD_BOOLEAN),
		DEFINE_FIELD(CBasePlayer, m_pCam, FIELD_CLASSPTR),
		DEFINE_FIELD(CBasePlayer, m_blSetView, FIELD_BOOLEAN),
		DEFINE_FIELD(CBasePlayer, m_blCamInited, FIELD_BOOLEAN),
		DEFINE_FIELD(CBasePlayer, m_flNVNUpdateTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_blInButStomp, FIELD_BOOLEAN),
		DEFINE_FIELD(CBasePlayer, m_flMinStompTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_blOnTractor, FIELD_BOOLEAN),
		DEFINE_FIELD(CBasePlayer, m_vecDeathAng, FIELD_VECTOR),
		DEFINE_FIELD(CBasePlayer, m_fCurseMSGTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_fCurseRefireScale, FIELD_FLOAT),
		DEFINE_FIELD(CBasePlayer, m_fCurseDmgMirror, FIELD_FLOAT),
		DEFINE_FIELD(CBasePlayer, m_fCurseDmgScale, FIELD_FLOAT),
		DEFINE_FIELD(CBasePlayer, m_fHealthRegenTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_fArmorRegenTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_fShotgunRiftTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_fIsoBombTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_fNukeTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_pCurDriveEnt, FIELD_CLASSPTR),
		DEFINE_FIELD(CBasePlayer, m_bInDriveEnt, FIELD_BOOLEAN),
		DEFINE_FIELD(CBasePlayer, m_fDrvEntEjectTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_fDrcEntTransTime, FIELD_TIME),
		DEFINE_FIELD(CBasePlayer, m_flBloodScale, FIELD_FLOAT),
		DEFINE_FIELD(CBasePlayer, m_iGibExtra, FIELD_INTEGER),

		//DEFINE_FIELD( CBasePlayer, m_fDeadTime, FIELD_FLOAT ), // only used in multiplayer games
		//DEFINE_FIELD( CBasePlayer, m_fGameHUDInitialized, FIELD_INTEGER ), // only used in multiplayer games
		//DEFINE_FIELD( CBasePlayer, m_flStopExtraSoundTime, FIELD_TIME ),
		//DEFINE_FIELD( CBasePlayer, m_fKnownItem, FIELD_BOOLEAN ), // reset to zero on load
		//DEFINE_FIELD( CBasePlayer, m_iPlayerSound, FIELD_INTEGER ),	// Don't restore, set in Precache()
		//DEFINE_FIELD( CBasePlayer, m_fNewAmmo, FIELD_INTEGER ), // Don't restore, client needs reset
		//DEFINE_FIELD( CBasePlayer, m_flgeigerRange, FIELD_FLOAT ),	// Don't restore, reset in Precache()
		//DEFINE_FIELD( CBasePlayer, m_flgeigerDelay, FIELD_FLOAT ),	// Don't restore, reset in Precache()
		//DEFINE_FIELD( CBasePlayer, m_igeigerRangePrev, FIELD_FLOAT ),	// Don't restore, reset in Precache()
		//DEFINE_FIELD( CBasePlayer, m_iStepLeft, FIELD_INTEGER ), // Don't need to restore
		//DEFINE_ARRAY( CBasePlayer, m_szTextureName, FIELD_CHARACTER, CBTEXTURENAMEMAX ), // Don't need to restore
		//DEFINE_FIELD( CBasePlayer, m_chTextureType, FIELD_CHARACTER ), // Don't need to restore
		//DEFINE_FIELD( CBasePlayer, m_fNoPlayerSound, FIELD_BOOLEAN ), // Don't need to restore, debug
		//DEFINE_FIELD( CBasePlayer, m_iUpdateTime, FIELD_INTEGER ), // Don't need to restore
		//DEFINE_FIELD( CBasePlayer, m_iClientHealth, FIELD_INTEGER ), // Don't restore, client needs reset
		//DEFINE_FIELD( CBasePlayer, m_iClientBattery, FIELD_INTEGER ), // Don't restore, client needs reset
		//DEFINE_FIELD( CBasePlayer, m_iClientHideHUD, FIELD_INTEGER ), // Don't restore, client needs reset
		//DEFINE_FIELD( CBasePlayer, m_fWeapon, FIELD_BOOLEAN ),  // Don't restore, client needs reset
		//DEFINE_FIELD( CBasePlayer, m_nCustomSprayFrames, FIELD_INTEGER ), // Don't restore, depends on server message after spawning and only matters in multiplayer
		//DEFINE_FIELD( CBasePlayer, m_vecAutoAim, FIELD_VECTOR ), // Don't save/restore - this is recomputed
		//DEFINE_ARRAY( CBasePlayer, m_rgAmmoLast, FIELD_INTEGER, MAX_AMMO_SLOTS ), // Don't need to restore
		//DEFINE_FIELD( CBasePlayer, m_fOnTarget, FIELD_BOOLEAN ), // Don't need to restore
		//DEFINE_FIELD( CBasePlayer, m_nCustomSprayFrames, FIELD_INTEGER ), // Don't need to restore

};

LINK_ENTITY_TO_CLASS(player, CBasePlayer);



void CBasePlayer::Pain()
{
	float flRndSound; //sound randomizer

	flRndSound = RANDOM_FLOAT(0, 1);

	if (flRndSound <= 0.33)
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain5.wav", 1, ATTN_NORM);
	else if (flRndSound <= 0.66)
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain6.wav", 1, ATTN_NORM);
	else
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain7.wav", 1, ATTN_NORM);
}

/* 
 *
 */
Vector VecVelocityForDamage(float flDamage)
{
	Vector vec(RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(200, 300));

	if (flDamage > -50)
		vec = vec * 0.7;
	else if (flDamage > -200)
		vec = vec * 2;
	else
		vec = vec * 10;

	return vec;
}

#if 0 /*                                                                 \
static void ThrowGib(entvars_t *pev, char *szGibModel, float flDamage)   \
{                                                                        \
	edict_t *pentNew = CREATE_ENTITY();                                  \
	entvars_t *pevNew = VARS(pentNew);                                   \
																	   \ \
	pevNew->origin = pev->origin;                                        \
	SET_MODEL(ENT(pevNew), szGibModel);                                  \
	UTIL_SetSize(pevNew, g_vecZero, g_vecZero);                          \
																	   \ \
	pevNew->velocity		= VecVelocityForDamage(flDamage);                  \
	pevNew->movetype		= MOVETYPE_BOUNCE;                                 \
	pevNew->solid			= SOLID_NOT;                                         \
	pevNew->avelocity.x		= RANDOM_FLOAT(0,600);                          \
	pevNew->avelocity.y		= RANDOM_FLOAT(0,600);                          \
	pevNew->avelocity.z		= RANDOM_FLOAT(0,600);                          \
	CHANGE_METHOD(ENT(pevNew), em_think, SUB_Remove);                    \
	pevNew->ltime		= gpGlobals->time;                                    \
	pevNew->nextthink	= gpGlobals->time + RANDOM_FLOAT(10,20);           \
	pevNew->frame		= 0;                                                  \
	pevNew->flags		= 0;                                                  \
}                                                                        \
																	   \ \
																	   \ \
static void ThrowHead(entvars_t *pev, char *szGibModel, floatflDamage)   \
{                                                                        \
	SET_MODEL(ENT(pev), szGibModel);                                     \
	pev->frame			= 0;                                                    \
	pev->nextthink		= -1;                                                \
	pev->movetype		= MOVETYPE_BOUNCE;                                    \
	pev->takedamage		= DAMAGE_NO;                                        \
	pev->solid			= SOLID_NOT;                                            \
	pev->view_ofs		= Vector(0,0,8);                                      \
	UTIL_SetSize(pev, Vector(-16,-16,0), Vector(16,16,56));              \
	pev->velocity		= VecVelocityForDamage(flDamage);                     \
	pev->avelocity		= RANDOM_FLOAT(-1,1) * Vector(0,600,0);              \
	pev->origin.z -= 24;                                                 \
	ClearBits(pev->flags, FL_ONGROUND);                                  \
}                                                                        \
																	   \ \
																	   \ \
*/
#endif

int TrainSpeed(int iSpeed, int iMax)
{
	float fSpeed, fMax;
	int iRet = 0;

	fMax = (float)iMax;
	fSpeed = iSpeed;

	fSpeed = fSpeed / fMax;

	if (iSpeed < 0)
		iRet = TRAIN_BACK;
	else if (iSpeed == 0)
		iRet = TRAIN_NEUTRAL;
	else if (fSpeed < 0.33)
		iRet = TRAIN_SLOW;
	else if (fSpeed < 0.66)
		iRet = TRAIN_MEDIUM;
	else
		iRet = TRAIN_FAST;

	return iRet;
}

void CBasePlayer::DeathSound()
{
	// water death sounds
	/*
	if (pev->waterlevel == 3)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/h2odeath.wav", 1, ATTN_NONE);
		return;
	}
	*/

	// temporarily using pain sounds for death sounds
	switch (RANDOM_LONG(1, 5))
	{
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain5.wav", 1, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain6.wav", 1, ATTN_NORM);
		break;
	case 3:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_pain7.wav", 1, ATTN_NORM);
		break;
	}

	// play one of the suit death alarms
	EMIT_GROUPNAME_SUIT(ENT(pev), "HEV_DEAD");
}

// override takehealth
// bitsDamageType indicates type of damage healed.

bool CBasePlayer::TakeHealth(float flHealth, int bitsDamageType)
{
	return CBaseMonster::TakeHealth(flHealth, bitsDamageType);
}

Vector CBasePlayer::GetGunPosition()
{
	//	UTIL_MakeVectors(pev->v_angle);
	//	m_HackedGunPos = pev->view_ofs;
	Vector origin;

	origin = pev->origin + pev->view_ofs;

	return origin;
}

//=========================================================
// TraceAttack
//=========================================================
void CBasePlayer::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (0 != pev->takedamage)
	{
		m_LastHitGroup = ptr->iHitgroup;

		switch (ptr->iHitgroup)
		{
		case HITGROUP_GENERIC:
			break;
		case HITGROUP_HEAD:
			flDamage *= gSkillData.plrHead;
			break;
		case HITGROUP_CHEST:
			flDamage *= gSkillData.plrChest;
			break;
		case HITGROUP_STOMACH:
			flDamage *= gSkillData.plrStomach;
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
			flDamage *= gSkillData.plrArm;
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			flDamage *= gSkillData.plrLeg;
			break;
		default:
			break;
		}

		SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage); // a little surface blood.
		TraceBleed(flDamage, vecDir, ptr, bitsDamageType);
		AddMultiDamage(pevAttacker, this, flDamage, bitsDamageType);
	}
}

/*
	Take some damage.  
	NOTE: each call to TakeDamage with bitsDamageType set to a time-based damage
	type will cause the damage time countdown to be reset.  Thus the ongoing effects of poison, radiation
	etc are implemented with subsequent calls to TakeDamage using DMG_GENERIC.
*/

#define ARMOR_RATIO 0.2 // Armor Takes 80% of the damage
#define ARMOR_BONUS 0.5 // Each Point of Armor is work 1/x points of health

// orig 0x1ea2c0; RC: mp_teampush
bool CBasePlayer::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// have suit diagnose the problem - ie: report damage type
	int bitsDamage = bitsDamageType;
	bool ffound = true;
	bool fmajor;
	bool fcritical;
	bool fTookDamage;
	bool ftrivial;
	float flRatio;
	float flBonus;
	float flHealthPrev = pev->health;

	flBonus = ARMOR_BONUS;
	flRatio = ARMOR_RATIO;

	if ((bitsDamageType & DMG_BLAST) != 0 && g_pGameRules->IsMultiplayer())
	{
		// blasts damage armor more.
		flBonus *= 2;
	}

	// Already dead
	if (!IsAlive())
		return false;
	// go take the damage first


	CBaseEntity* pAttacker = CBaseEntity::Instance(pevAttacker);

	if (!g_pGameRules->FPlayerCanTakeDamage(this, pAttacker))
	{
		// Refuse the damage. Rocket Crowbar: with mp_teampush 1 the hit still pushes the player,
		// as CBaseMonster::TakeDamage would.
		Vector vecDir = g_vecZero;
		if (!FNullEnt(pevInflictor))
		{
			CBaseEntity* pInflictor = CBaseEntity::Instance(pevInflictor);
			if (pInflictor)
			{
				vecDir = (pInflictor->Center() - Vector(0, 0, 10) - Center()).Normalize();
				vecDir = g_vecAttackDir = vecDir.Normalize();
			}
		}

		if (CVAR_GET_FLOAT("mp_teampush") != 1)
			return false;

		if (!FNullEnt(pevInflictor) && pev->movetype == MOVETYPE_WALK && (!pevAttacker || pevAttacker->solid != SOLID_TRIGGER))
			pev->velocity = pev->velocity + vecDir * -DamageForce(flDamage);

		return false;
	}

	// keep track of amount of damage last sustained
	m_lastDamageAmount = flDamage;

	// Armor.
	if (0 != pev->armorvalue && (bitsDamageType & (DMG_FALL | DMG_DROWN)) == 0) // armor doesn't protect against fall or drown damage!
	{
		float flNew = flDamage * flRatio;

		float flArmor;

		flArmor = (flDamage - flNew) * flBonus;

		// Does this use more armor than we have?
		if (flArmor > pev->armorvalue)
		{
			flArmor = pev->armorvalue;
			flArmor *= (1 / flBonus);
			flNew = flDamage - flArmor;
			pev->armorvalue = 0;
		}
		else
			pev->armorvalue -= flArmor;

		flDamage = flNew;
	}

	// this cast to INT is critical!!! If a player ends up with 0.5 health, the engine will get that
	// as an int (zero) and think the player is dead! (this will incite a clientside screentilt, etc)
	fTookDamage = CBaseMonster::TakeDamage(pevInflictor, pevAttacker, (int)flDamage, bitsDamageType);

	// reset damage time countdown for each type of time based damage player just sustained

	{
		for (int i = 0; i < CDMG_TIMEBASED; i++)
			if ((bitsDamageType & (DMG_PARALYZE << i)) != 0)
				m_rgbTimeBasedDamage[i] = 0;
	}

	// tell director about it
	MESSAGE_BEGIN(MSG_SPEC, SVC_DIRECTOR);
	WRITE_BYTE(9);							  // command length in bytes
	WRITE_BYTE(DRC_CMD_EVENT);				  // take damage event
	WRITE_SHORT(ENTINDEX(this->edict()));	  // index number of primary entity
	WRITE_SHORT(ENTINDEX(ENT(pevInflictor))); // index number of secondary entity
	WRITE_LONG(5);							  // eventflags (priority and flags)
	MESSAGE_END();


	// how bad is it, doc?

	ftrivial = (pev->health > 75 || m_lastDamageAmount < 5);
	fmajor = (m_lastDamageAmount > 25);
	fcritical = (pev->health < 30);

	// handle all bits set in this damage message,
	// let the suit give player the diagnosis

	// UNDONE: add sounds for types of damage sustained (ie: burn, shock, slash )

	// UNDONE: still need to record damage and heal messages for the following types

	// DMG_BURN
	// DMG_FREEZE
	// DMG_BLAST
	// DMG_SHOCK

	m_bitsDamageType |= bitsDamage; // Save this so we can report it to the client
	m_bitsHUDDamage = -1;			// make sure the damage bits get resent

	while (fTookDamage && (!ftrivial || (bitsDamage & DMG_TIMEBASED) != 0) && ffound && 0 != bitsDamage)
	{
		ffound = false;

		if ((bitsDamage & DMG_CLUB) != 0)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG4", false, SUIT_NEXT_IN_30SEC); // minor fracture
			bitsDamage &= ~DMG_CLUB;
			ffound = true;
		}
		if ((bitsDamage & (DMG_FALL | DMG_CRUSH)) != 0)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG5", false, SUIT_NEXT_IN_30SEC); // major fracture
			else
				SetSuitUpdate("!HEV_DMG4", false, SUIT_NEXT_IN_30SEC); // minor fracture

			bitsDamage &= ~(DMG_FALL | DMG_CRUSH);
			ffound = true;
		}

		if ((bitsDamage & DMG_BULLET) != 0)
		{
			if (m_lastDamageAmount > 5)
				SetSuitUpdate("!HEV_DMG6", false, SUIT_NEXT_IN_30SEC); // blood loss detected
			//else
			//	SetSuitUpdate("!HEV_DMG0", false, SUIT_NEXT_IN_30SEC);	// minor laceration

			bitsDamage &= ~DMG_BULLET;
			ffound = true;
		}

		if ((bitsDamage & DMG_SLASH) != 0)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG1", false, SUIT_NEXT_IN_30SEC); // major laceration
			else
				SetSuitUpdate("!HEV_DMG0", false, SUIT_NEXT_IN_30SEC); // minor laceration

			bitsDamage &= ~DMG_SLASH;
			ffound = true;
		}

		if ((bitsDamage & DMG_SONIC) != 0)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG2", false, SUIT_NEXT_IN_1MIN); // internal bleeding
			bitsDamage &= ~DMG_SONIC;
			ffound = true;
		}

		if ((bitsDamage & (DMG_POISON | DMG_PARALYZE)) != 0)
		{
			SetSuitUpdate("!HEV_DMG3", false, SUIT_NEXT_IN_1MIN); // blood toxins detected
			bitsDamage &= ~(DMG_POISON | DMG_PARALYZE);
			ffound = true;
		}

		if ((bitsDamage & DMG_ACID) != 0)
		{
			SetSuitUpdate("!HEV_DET1", false, SUIT_NEXT_IN_1MIN); // hazardous chemicals detected
			bitsDamage &= ~DMG_ACID;
			ffound = true;
		}

		if ((bitsDamage & DMG_NERVEGAS) != 0)
		{
			SetSuitUpdate("!HEV_DET0", false, SUIT_NEXT_IN_1MIN); // biohazard detected
			bitsDamage &= ~DMG_NERVEGAS;
			ffound = true;
		}

		if ((bitsDamage & DMG_RADIATION) != 0)
		{
			SetSuitUpdate("!HEV_DET2", false, SUIT_NEXT_IN_1MIN); // radiation detected
			bitsDamage &= ~DMG_RADIATION;
			ffound = true;
		}
		if ((bitsDamage & DMG_SHOCK) != 0)
		{
			bitsDamage &= ~DMG_SHOCK;
			ffound = true;
		}
	}

	pev->punchangle.x = -2;

	if (fTookDamage && !ftrivial && fmajor && flHealthPrev >= 75)
	{
		// first time we take major damage...
		// turn automedic on if not on
		SetSuitUpdate("!HEV_MED1", false, SUIT_NEXT_IN_30MIN); // automedic on

		// give morphine shot if not given recently
		SetSuitUpdate("!HEV_HEAL7", false, SUIT_NEXT_IN_30MIN); // morphine shot
	}

	if (fTookDamage && !ftrivial && fcritical && flHealthPrev < 75)
	{

		// already took major damage, now it's critical...
		if (pev->health < 6)
			SetSuitUpdate("!HEV_HLTH3", false, SUIT_NEXT_IN_10MIN); // near death
		else if (pev->health < 20)
			SetSuitUpdate("!HEV_HLTH2", false, SUIT_NEXT_IN_10MIN); // health critical

		// give critical health warnings
		if (!RANDOM_LONG(0, 3) && flHealthPrev < 50)
			SetSuitUpdate("!HEV_DMG7", false, SUIT_NEXT_IN_5MIN); //seek medical attention
	}

	// if we're taking time based damage, warn about its continuing effects
	if (fTookDamage && (bitsDamageType & DMG_TIMEBASED) != 0 && flHealthPrev < 75)
	{
		if (flHealthPrev < 50)
		{
			if (!RANDOM_LONG(0, 3))
				SetSuitUpdate("!HEV_DMG7", false, SUIT_NEXT_IN_5MIN); //seek medical attention
		}
		else
			SetSuitUpdate("!HEV_HLTH1", false, SUIT_NEXT_IN_10MIN); // health dropping
	}

	return fTookDamage;
}

//=========================================================
// PackDeadPlayerItems - call this when a player dies to
// pack up the appropriate weapons and ammo items, and to
// destroy anything that shouldn't be packed.
//
// This is pretty brute force :(
//=========================================================
void CBasePlayer::PackDeadPlayerItems()
{
	int iWeaponRules;
	int iAmmoRules;
	int i;
	CBasePlayerWeapon* rgpPackWeapons[MAX_WEAPONS];
	int iPackAmmo[MAX_AMMO_SLOTS + 1];
	int iPW = 0; // index into packweapons array
	int iPA = 0; // index into packammo array

	memset(rgpPackWeapons, 0, sizeof(rgpPackWeapons));
	memset(iPackAmmo, -1, sizeof(iPackAmmo));

	// get the game rules
	iWeaponRules = g_pGameRules->DeadPlayerWeapons(this);
	iAmmoRules = g_pGameRules->DeadPlayerAmmo(this);

	if (iWeaponRules == GR_PLR_DROP_GUN_NO && iAmmoRules == GR_PLR_DROP_AMMO_NO)
	{
		// nothing to pack. Remove the weapons and return. Don't call create on the box!
		RemoveAllItems(true);
		return;
	}

	// go through all of the weapons and make a list of the ones to pack
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem* pPlayerItem = m_rgpPlayerItems[i];

			while (pPlayerItem)
			{
				switch (iWeaponRules)
				{
				case GR_PLR_DROP_GUN_ACTIVE:
					if (m_pActiveItem && pPlayerItem == m_pActiveItem)
					{
						// this is the active item. Pack it.
						// RC 1.9's (as SDK 2.1); halflife-updated reloaded the weapon from the
						// player's ammo before packing it
						rgpPackWeapons[iPW++] = (CBasePlayerWeapon*)pPlayerItem;
					}
					break;

				case GR_PLR_DROP_GUN_ALL:
					rgpPackWeapons[iPW++] = (CBasePlayerWeapon*)pPlayerItem;
					break;

				default:
					break;
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	// now go through ammo and make a list of which types to pack.
	if (iAmmoRules != GR_PLR_DROP_AMMO_NO)
	{
		for (i = 0; i < MAX_AMMO_SLOTS; i++)
		{
			if (m_rgAmmo[i] > 0)
			{
				// player has some ammo of this type.
				switch (iAmmoRules)
				{
				case GR_PLR_DROP_AMMO_ALL:
					iPackAmmo[iPA++] = i;
					break;

				case GR_PLR_DROP_AMMO_ACTIVE:
					if (m_pActiveItem && i == m_pActiveItem->PrimaryAmmoIndex())
					{
						// this is the primary ammo type for the active weapon
						iPackAmmo[iPA++] = i;
					}
					else if (m_pActiveItem && i == m_pActiveItem->SecondaryAmmoIndex())
					{
						// this is the secondary ammo type for the active weapon
						iPackAmmo[iPA++] = i;
					}
					break;

				default:
					break;
				}
			}
		}
	}

	// create a box to pack the stuff into.
	CWeaponBox* pWeaponBox = (CWeaponBox*)CBaseEntity::Create("weaponbox", pev->origin, pev->angles, edict());

	pWeaponBox->pev->angles.x = 0; // don't let weaponbox tilt.
	pWeaponBox->pev->angles.z = 0;

	pWeaponBox->SetThink(&CWeaponBox::Kill);
	pWeaponBox->pev->nextthink = gpGlobals->time + 120;

	// back these two lists up to their first elements
	iPA = 0;
	iPW = 0;

	if (IsBustingGame())
	{
		if (HasNamedPlayerItem("weapon_egon"))
		{
			for (i = 0; i < MAX_ITEM_TYPES; i++)
			{
				CBasePlayerItem* pItem = m_rgpPlayerItems[i];

				if (pItem)
				{
					if (!strcmp("weapon_egon", STRING(pItem->pev->classname)))
					{
						pWeaponBox->PackWeapon(pItem);

						SET_MODEL(ENT(pWeaponBox->pev), "models/w_egon.mdl");

						pWeaponBox->pev->velocity = Vector(0, 0, 0);
						pWeaponBox->pev->renderfx = kRenderFxGlowShell;
						pWeaponBox->pev->renderamt = 25;
						pWeaponBox->pev->rendercolor = Vector(0, 75, 250);

						break;
					}
				}
			}
		}
	}
	else
	{
		// halflife-updated's fix, kept: nothing is packed when the active weapon is a satchel with
		// none left to throw, so the box doesn't hand out an empty satchel
		bool bPackItems = true;

		if (iAmmoRules == GR_PLR_DROP_AMMO_ACTIVE && iWeaponRules == GR_PLR_DROP_GUN_ACTIVE)
		{
			if (rgpPackWeapons[0] && FClassnameIs(rgpPackWeapons[0]->pev, "weapon_satchel") && (iPackAmmo[0] == -1 || (m_rgAmmo[iPackAmmo[0]] == 0)))
			{
				bPackItems = false;
			}
		}

		if (bPackItems)
		{
			// pack the ammo
			while (iPackAmmo[iPA] != -1)
			{
				pWeaponBox->PackAmmo(MAKE_STRING(CBasePlayerItem::AmmoInfoArray[iPackAmmo[iPA]].pszName), m_rgAmmo[iPackAmmo[iPA]]);
				iPA++;
			}

			// now pack all of the items in the lists
			while (rgpPackWeapons[iPW])
			{
				// weapon unhooked from the player. Pack it into der box.
				pWeaponBox->PackWeapon(rgpPackWeapons[iPW]);

				iPW++;
			}
		}

		pWeaponBox->pev->velocity = pev->velocity * 1.2; // weaponbox has player's velocity, then some.
	}

	RemoveAllItems(true); // now strip off everything that wasn't handled by the code above.
}

void CBasePlayer::RemoveAllItems(bool removeSuit)
{
	if (m_pActiveItem)
	{
		ResetAutoaim();
		m_pActiveItem->Holster();
		m_pActiveItem = NULL;
	}

	m_pLastItem = NULL;

	// halflife-updated's fix, kept: a func_tank the player was using is let go, so it isn't left
	// controlled by a stripped player
	if (m_pTank != NULL)
	{
		m_pTank->Use(this, this, USE_OFF, 0);
		m_pTank = NULL;
	}

	int i;
	CBasePlayerItem* pPendingItem;
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		m_pActiveItem = m_rgpPlayerItems[i];
		while (m_pActiveItem)
		{
			pPendingItem = m_pActiveItem->m_pNext;
			m_pActiveItem->Drop();
			m_pActiveItem = pPendingItem;
		}
		m_rgpPlayerItems[i] = NULL;
	}
	m_pActiveItem = NULL;

	pev->viewmodel = 0;
	pev->weaponmodel = 0;

	m_WeaponBits = 0ULL;
	// Mirror m_WeaponBits = 0 onto the documented pev->weapons mask so
	// server-side consumers (Metamod plugins, the engine's
	// cdata->iWeaponBits delivery to vanilla clients) see the wipe too.
	// SetHasSuit() below goes through SetWeaponBit() / ClearWeaponBit(),
	// which are paired to maintain pev->weapons in lockstep with
	// m_WeaponBits.
	pev->weapons = 0;

	//Re-add suit bit if needed.
	SetHasSuit(!removeSuit);

	for (i = 0; i < MAX_AMMO_SLOTS; i++)
		m_rgAmmo[i] = 0;

	UpdateClientData();

	// RC 1.9's (as SDK 2.1); halflife-updated dropped this message
	// send Selected Weapon Message to our client
	MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	MESSAGE_END();
}

/*
 * GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
 *
 * ENTITY_METHOD(PlayerDie)
 */

// orig 0x1eb6a0. RC: drops the CTF flag, takes the driver out of a drive monster, the dying
// camera (StartDyingCam), no gibs or death sound for an invisible player.
void CBasePlayer::Killed(entvars_t* pevAttacker, int iGib)
{
	CSound* pSound;

	// halflife-updated holstered the active weapon here; the original doesn't.

	DispatchPlayerKilled(edict());
	DestroyFreezeTimer(pev);
	ClearBits(pev->flags, FL_FROZEN);
	DestroyFrictionTimer(pev);
	pev->gravity = 1;
	pev->friction = 1;

	CBaseFlag* pFlag = DoesEntHaveFlag(this);
	if (pFlag)
	{
		char szMsg[512];

		pFlag->FlagDrop(pev);
		sprintf(szMsg, "* %s of team %s (%s) lost the %s (%s) flag!\n", STRING(pev->netname), m_szTeamName, GetFlagTeamColor(g_pGameRules->GetTeamIndex(m_szTeamName) + 1), GetTeamNameForIndex(pFlag->m_iTeam), GetFlagTeamColor(pFlag->m_iTeam));
		UTIL_SayTextAll(szMsg, this);
		UTIL_ScreenFade(this, GetFlagBaseColor(pFlag->m_iTeam), 1, 0, 32, 0);
	}

	pev->renderfx = kRenderFxNone;
	pev->rendercolor = g_vecZero;
	pev->renderamt = 0;

	if (m_bInDriveEnt && m_pCurDriveEnt)
	{
		if (m_pCurDriveEnt->IsAlive())
		{
			m_pCurDriveEnt->DEClientLeave();
			m_bInDriveEnt = false;
			m_pCurDriveEnt->Killed(pev, GIB_NORMAL);
			UTIL_SetOrigin(pev, m_pCurDriveEnt->pev->origin);
			m_pCurDriveEnt = NULL;
			pev->effects |= EF_NODRAW;
		}
		else
			m_pCurDriveEnt->DEClientLeave();

		m_bInDriveEnt = false;
		m_pCurDriveEnt = NULL;
	}

	g_pGameRules->PlayerKilled(this, pevAttacker, g_pevLastInflictor);

	if (m_pTank != NULL)
	{
		m_pTank->Use(this, this, USE_OFF, 0);
		m_pTank = NULL;
	}

	// this client isn't going to be thinking for a while, so reset the sound until they respawn
	pSound = CSoundEnt::SoundPointerForIndex(CSoundEnt::ClientSoundIndex(edict()));
	{
		if (pSound)
		{
			pSound->Reset();
		}
	}

	if (pev->iuser4 == 1) // VERIFY: nothing in RC's code sets iuser4 on a player
		return;

	ClearBits(pev->flags, FL_FROZEN);
	SetAnimation(PLAYER_DIE);

	// halflife-updated's fix, kept (Steven, 2026-11-07): the death-animation count starts from 0 at
	// each death (the original's m_iRespawnFrames was reset only by respawn())
	m_flRespawnTimer = 0.0f;

	// The original doesn't reset pev->modelindex to g_ulModelIndexPlayer here.

	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	pev->takedamage = DAMAGE_NO;
	ClearBits(pev->flags, FL_ONGROUND);
	if (pev->velocity.z < 10)
		pev->velocity.z += RANDOM_FLOAT(0, 300);

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, false, 0);

	// send "health" update message to zero
	m_iClientHealth = 0;
	MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, pev);
	WRITE_SHORT(m_iClientHealth); // VERIFY: the original's Health is 1 byte (UserMessages.cpp registers 2, see the dossier)
	MESSAGE_END();

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
	WRITE_BYTE(0);
	WRITE_BYTE(0XFF);
	WRITE_BYTE(0xFF);
	MESSAGE_END();

	// reset FOV
	m_iFOV = m_iClientFOV = 0;

	MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, pev);
	WRITE_BYTE(0);
	MESSAGE_END();

	StartDyingCam();

	// a player hidden by a drive monster neither gibs nor makes a death sound
	if ((pev->effects & EF_NODRAW) != 0)
		return;

	if ((pev->health < -40 && iGib != GIB_NEVER) || iGib == GIB_ALWAYS)
	{
		pev->solid = SOLID_NOT;
		GibMonster(); // This clears pev->model
		pev->effects |= EF_NODRAW;
		return;
	}

	DeathSound();

	pev->v_angle.x = 0;
	pev->v_angle.z = 0;
	m_vecDeathAng = pev->v_angle;

	SetThink(&CBasePlayer::PlayerDeathThink);
	pev->nextthink = gpGlobals->time + 0.1;

	// non-solid while dead (halflife-updated does this for multiplayer only, before the gib test)
	pev->solid = SOLID_NOT;
}


// Set the activity based on an event or current state
void CBasePlayer::SetAnimation(PLAYER_ANIM playerAnim)
{
	int animDesired;
	float speed;
	char szAnim[64];

	speed = pev->velocity.Length2D();

	if ((pev->flags & FL_FROZEN) != 0)
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
	}

	switch (playerAnim)
	{
	case PLAYER_JUMP:
		m_IdealActivity = ACT_HOP;
		break;

	case PLAYER_SUPERJUMP:
		m_IdealActivity = ACT_LEAP;
		break;

	case PLAYER_DIE:
		m_IdealActivity = ACT_DIESIMPLE;
		m_IdealActivity = GetDeathActivity();
		break;

	case PLAYER_ATTACK1:
		switch (m_Activity)
		{
		case ACT_HOVER:
		case ACT_SWIM:
		case ACT_HOP:
		case ACT_LEAP:
		case ACT_DIESIMPLE:
			m_IdealActivity = m_Activity;
			break;
		default:
			m_IdealActivity = ACT_RANGE_ATTACK1;
			break;
		}
		break;
	case PLAYER_IDLE:
	case PLAYER_WALK:
		if (!FBitSet(pev->flags, FL_ONGROUND) && (m_Activity == ACT_HOP || m_Activity == ACT_LEAP)) // Still jumping
		{
			m_IdealActivity = m_Activity;
		}
		else if (pev->waterlevel > 1)
		{
			if (speed == 0)
				m_IdealActivity = ACT_HOVER;
			else
				m_IdealActivity = ACT_SWIM;
		}
		else
		{
			m_IdealActivity = ACT_WALK;
		}
		break;
	}

	switch (m_IdealActivity)
	{
	case ACT_HOVER:
	case ACT_LEAP:
	case ACT_SWIM:
	case ACT_HOP:
	case ACT_DIESIMPLE:
	default:
		if (m_Activity == m_IdealActivity)
			return;
		m_Activity = m_IdealActivity;

		animDesired = LookupActivity(m_Activity);
		// Already using the desired animation?
		if (pev->sequence == animDesired)
			return;

		//ALERT(at_console, "Set die animation to %d\n", animDesired);

		pev->gaitsequence = 0;
		pev->sequence = animDesired;
		pev->frame = 0;
		ResetSequenceInfo();
		return;

	case ACT_RANGE_ATTACK1:
		if (FBitSet(pev->flags, FL_DUCKING)) // crouching
			strcpy(szAnim, "crouch_shoot_");
		else
			strcpy(szAnim, "ref_shoot_");
		strcat(szAnim, m_szAnimExtention);
		animDesired = LookupSequence(szAnim);
		if (animDesired == -1)
			animDesired = 0;

		if (pev->sequence != animDesired || !m_fSequenceLoops)
		{
			pev->frame = 0;
		}

		if (!m_fSequenceLoops)
		{
			pev->effects |= EF_NOINTERP;
		}

		m_Activity = m_IdealActivity;

		pev->sequence = animDesired;
		ResetSequenceInfo();
		break;

	case ACT_WALK:
		if (m_Activity != ACT_RANGE_ATTACK1 || m_fSequenceFinished)
		{
			if (FBitSet(pev->flags, FL_DUCKING)) // crouching
				strcpy(szAnim, "crouch_aim_");
			else
				strcpy(szAnim, "ref_aim_");
			strcat(szAnim, m_szAnimExtention);
			animDesired = LookupSequence(szAnim);
			if (animDesired == -1)
				animDesired = 0;
			m_Activity = ACT_WALK;
		}
		else
		{
			animDesired = pev->sequence;
		}
	}

	if (FBitSet(pev->flags, FL_DUCKING))
	{
		if (speed == 0)
		{
			pev->gaitsequence = LookupActivity(ACT_CROUCHIDLE);
			// pev->gaitsequence	= LookupActivity( ACT_CROUCH );
		}
		else
		{
			pev->gaitsequence = LookupActivity(ACT_CROUCH);
		}
	}
	else if (speed > 220)
	{
		pev->gaitsequence = LookupActivity(ACT_RUN);
	}
	else if (speed > 0)
	{
		pev->gaitsequence = LookupActivity(ACT_WALK);
	}
	else
	{
		// pev->gaitsequence	= LookupActivity( ACT_WALK );
		pev->gaitsequence = LookupSequence("deep_idle");
	}


	// Already using the desired animation?
	if (pev->sequence == animDesired)
		return;

	//ALERT( at_console, "Set animation to %d\n", animDesired );
	// Reset to first frame of desired animation
	pev->sequence = animDesired;
	pev->frame = 0;
	ResetSequenceInfo();
}

/*
===========
TabulateAmmo
This function is used to find and store 
all the ammo we have into the ammo vars.
============
*/
void CBasePlayer::TabulateAmmo()
{
	ammo_9mm = AmmoInventory(GetAmmoIndex("9mm"));
	ammo_357 = AmmoInventory(GetAmmoIndex("357"));
	ammo_argrens = AmmoInventory(GetAmmoIndex("ARgrenades"));
	ammo_bolts = AmmoInventory(GetAmmoIndex("bolts"));
	ammo_buckshot = AmmoInventory(GetAmmoIndex("buckshot"));
	ammo_rockets = AmmoInventory(GetAmmoIndex("rockets"));
	ammo_uranium = AmmoInventory(GetAmmoIndex("uranium"));
	ammo_hornets = AmmoInventory(GetAmmoIndex("Hornets"));
}


/*
===========
WaterMove
============
*/
#define AIRTIME 12 // lung full of air lasts this many seconds

void CBasePlayer::WaterMove()
{
	int air;

	if (pev->movetype == MOVETYPE_NOCLIP)
		return;

	if (pev->health < 0)
		return;

	// waterlevel 0 - not in water
	// waterlevel 1 - feet in water
	// waterlevel 2 - waist in water
	// waterlevel 3 - head in water

	if (pev->waterlevel != 3)
	{
		// not underwater

		// play 'up for air' sound
		if (pev->air_finished < gpGlobals->time)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_wade1.wav", 1, ATTN_NORM);
		else if (pev->air_finished < gpGlobals->time + 9)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_wade2.wav", 1, ATTN_NORM);

		pev->air_finished = gpGlobals->time + AIRTIME;
		pev->dmg = 2;

		// if we took drowning damage, give it back slowly
		if (m_idrowndmg > m_idrownrestored)
		{
			// set drowning damage bit.  hack - dmg_drownrecover actually
			// makes the time based damage code 'give back' health over time.
			// make sure counter is cleared so we start count correctly.

			// NOTE: this actually causes the count to continue restarting
			// until all drowning damage is healed.

			m_bitsDamageType |= DMG_DROWNRECOVER;
			m_bitsDamageType &= ~DMG_DROWN;
			m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
		}
	}
	else
	{ // fully under water
		// stop restoring damage while underwater
		m_bitsDamageType &= ~DMG_DROWNRECOVER;
		m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;

		if (pev->air_finished < gpGlobals->time) // drown!
		{
			if (pev->pain_finished < gpGlobals->time)
			{
				// take drowning damage
				pev->dmg += 1;
				if (pev->dmg > 5)
					pev->dmg = 5;

				const float oldHealth = pev->health;

				TakeDamage(CWorld::World->pev, CWorld::World->pev, pev->dmg, DMG_DROWN);
				pev->pain_finished = gpGlobals->time + 1;

				// track drowning damage, give it back when
				// player finally takes a breath

				// halflife-updated's fix, kept: only the health actually lost is given back, so god
				// mode or a guard doesn't gain health on surfacing
				// Account for god mode, the unkillable flag, potential damage mitigation
				// and gaining health from drowning to avoid counting damage not actually taken.
				const float drownDamageTaken = std::max(0.f, std::floor(oldHealth - pev->health));

				m_idrowndmg += drownDamageTaken;
			}
		}
		else
		{
			m_bitsDamageType &= ~DMG_DROWN;
		}
	}

	if (0 == pev->waterlevel)
	{
		if (FBitSet(pev->flags, FL_INWATER))
		{
			ClearBits(pev->flags, FL_INWATER);
		}
		return;
	}

	// make bubbles
	// halflife-updated's fix, kept: only with the head under water, so shallow water doesn't play
	// breathing sounds (halflife #3110)
	if (pev->waterlevel == 3)
	{
		air = (int)(pev->air_finished - gpGlobals->time);
		if (!RANDOM_LONG(0, 0x1f) && RANDOM_LONG(0, AIRTIME - 1) >= air)
		{
			switch (RANDOM_LONG(0, 3))
			{
			case 0:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim1.wav", 0.8, ATTN_NORM);
				break;
			case 1:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim2.wav", 0.8, ATTN_NORM);
				break;
			case 2:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim3.wav", 0.8, ATTN_NORM);
				break;
			case 3:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim4.wav", 0.8, ATTN_NORM);
				break;
			}
		}
	}

	if (pev->watertype == CONTENT_LAVA) // do damage
	{
		if (pev->dmgtime < gpGlobals->time)
			TakeDamage(CWorld::World->pev, CWorld::World->pev, 10 * pev->waterlevel, DMG_BURN);
	}
	else if (pev->watertype == CONTENT_SLIME) // do damage
	{
		pev->dmgtime = gpGlobals->time + 1;
		TakeDamage(CWorld::World->pev, CWorld::World->pev, 4 * pev->waterlevel, DMG_ACID);
	}

	if (!FBitSet(pev->flags, FL_INWATER))
	{
		SetBits(pev->flags, FL_INWATER);
		pev->dmgtime = 0;
	}

	// halflife-updated's fix, kept: no water friction here; pm_shared applies it, so SDK 2.1's line
	// slowed the player twice and the client mispredicted it (SDK 2.2+)
}


// true if the player is attached to a ladder
bool CBasePlayer::IsOnLadder()
{
	return (pev->movetype == MOVETYPE_FLY);
}

// orig 0x1ec5b0. RC: points the view at the dying camera and holds the death angles.
void CBasePlayer::PlayerDeathThink()
{
	float flForward;

	if (m_blSetView)
	{
		SET_VIEW(edict(), m_pCam->edict());
		m_blSetView = false;
	}

	pev->angles = m_vecDeathAng;
	pev->fixangle = 1;

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		flForward = pev->velocity.Length() - 20;
		if (flForward <= 0)
			pev->velocity = g_vecZero;
		else
			pev->velocity = flForward * pev->velocity.Normalize();
	}

	if (HasWeapons())
	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		PackDeadPlayerItems();
	}

	ClearBits(pev->flags, FL_FROZEN);

	if (0 != pev->modelindex && (!m_fSequenceFinished) && (pev->deadflag == DEAD_DYING))
	{
		StudioFrameAdvance();

		// The original counts think frames (its m_iRespawnFrames); halflife-updated counts 4 seconds.
		m_flRespawnTimer += 1;
		if (m_flRespawnTimer < 120) // 120 frames at 30 fps -- animations should be no longer than this
			return;
	}

	// halflife-updated also copies the body to the body queue here in multiplayer and stops a
	// grounded corpse (MOVETYPE_NONE); the original does neither.
	if (pev->deadflag == DEAD_DYING)
		pev->deadflag = DEAD_DEAD;

	pev->frame = 255;
	StopAnimation();

	pev->effects |= EF_NOINTERP;
	pev->framerate = 0.0;

	bool fAnyButtonDown = (pev->button & ~IN_SCORE) != 0;

	// wait for all buttons released
	if (pev->deadflag == DEAD_DEAD)
	{
		if (fAnyButtonDown)
			return;

		if (g_pGameRules->FPlayerCanRespawn(this))
		{
			m_fDeadTime = gpGlobals->time;
			pev->deadflag = DEAD_RESPAWNABLE;
		}

		return;
	}

	// if the player has been dead for one second longer than allowed by forcerespawn,
	// forcerespawn isn't on. Send the player off to an intermission camera until they
	// choose to respawn.
	if (g_pGameRules->IsMultiplayer() && (gpGlobals->time > (m_fDeadTime + 6)) && (m_afPhysicsFlags & PFLAG_OBSERVER) == 0)
	{
		// go to dead camera.
		StartDeathCam();
	}

	// wait for any button down,  or mp_forcerespawn is set and the respawn time is up
	if (!fAnyButtonDown && !(g_pGameRules->IsMultiplayer() && forcerespawn.value > 0 && (gpGlobals->time > (m_fDeadTime + 5))))
		return;

	pev->button = 0;
	m_flRespawnTimer = 0.0f;

	//ALERT(at_console, "Respawn\n");

	respawn(pev, (m_afPhysicsFlags & PFLAG_OBSERVER) == 0); // don't copy a corpse if we're in deathcam.
	pev->nextthink = -1;
}

//=========================================================
// StartDeathCam - find an intermission spot and send the
// player off into observer mode
//=========================================================
// orig 0x1ec910. RC: takes the view back from the dying camera first.
void CBasePlayer::StartDeathCam()
{
	edict_t *pSpot, *pNewSpot;
	int iRand;

	if (pev->view_ofs == g_vecZero)
	{
		// don't accept subsequent attempts to StartDeathCam()
		return;
	}

	SET_VIEW(edict(), edict());

	pSpot = FIND_ENTITY_BY_CLASSNAME(NULL, "info_intermission");

	if (FNullEnt(pSpot))
	{
		// no intermission spot. Push them up in the air, looking down at their corpse
		TraceResult tr;
		CopyToBodyQue(pev);
		UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, 128), ignore_monsters, edict(), &tr);
		StartObserver(tr.vecEndPos, UTIL_VecToAngles(tr.vecEndPos - pev->origin));
		return;
	}

	// at least one intermission spot in the world.
	iRand = RANDOM_LONG(0, 3);

	while (iRand > 0)
	{
		pNewSpot = FIND_ENTITY_BY_CLASSNAME(pSpot, "info_intermission");

		if (pNewSpot)
		{
			pSpot = pNewSpot;
		}

		iRand--;
	}

	CopyToBodyQue(pev);
	StartObserver(pSpot->v.origin, pSpot->v.v_angle);
}

// orig 0x1ecb50. The original's is the old SDK's death camera; halflife-updated's spectator
// mode (TE_KILLPLAYERATTACHMENTS, holster, team reset, RemoveAllItems, Observer_SetMode) isn't in it.
void CBasePlayer::StartObserver(Vector vecPosition, Vector vecViewAngle)
{
	m_afPhysicsFlags |= PFLAG_OBSERVER;

	pev->view_ofs = g_vecZero;
	pev->angles = pev->v_angle = vecViewAngle;
	pev->fixangle = 1;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NONE;
	pev->modelindex = 0;
	UTIL_SetOrigin(pev, vecPosition);
}

//
// PlayerUse - handles USE keypress
//
#define PLAYER_SEARCH_RADIUS (float)64

// orig 0x1ecc10. RC: with nothing usable in reach, a pickup_drive_apache straight ahead (80
// units) is used. halflife-updated's IsObserver() early-out isn't in the original.
void CBasePlayer::PlayerUse()
{
	// Was use pressed or released?
	if (((pev->button | m_afButtonPressed | m_afButtonReleased) & IN_USE) == 0)
		return;

	// Hit Use on a train?
	if ((m_afButtonPressed & IN_USE) != 0)
	{
		if (m_pTank != NULL)
		{
			// Stop controlling the tank
			// TODO: Send HUD Update
			m_pTank->Use(this, this, USE_OFF, 0);
			m_pTank = NULL;
			return;
		}
		else
		{
			if ((m_afPhysicsFlags & PFLAG_ONTRAIN) != 0)
			{
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW | TRAIN_OFF;
				CBaseEntity* pTrain = CBaseEntity::Instance(pev->groundentity);
				if (pTrain && (pTrain->Classify() == CLASS_VEHICLE))
					((CFuncVehicle*)pTrain)->m_pDriver = NULL;
				return;
			}
			else
			{ // Start controlling the train!
				CBaseEntity* pTrain = CBaseEntity::Instance(pev->groundentity);

				if (pTrain && (pev->button & IN_JUMP) == 0 && FBitSet(pev->flags, FL_ONGROUND) && (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) != 0 && pTrain->OnControls(pev))
				{
					m_afPhysicsFlags |= PFLAG_ONTRAIN;
					m_iTrain = TrainSpeed(pTrain->pev->speed, pTrain->pev->impulse);
					m_iTrain |= TRAIN_NEW;
					if (pTrain->Classify() == CLASS_VEHICLE)
					{
						EMIT_SOUND(ENT(pev), CHAN_ITEM, "plats/vehicle_ignition.wav", 0.8, ATTN_NORM);
						((CFuncVehicle*)pTrain)->m_pDriver = this;
					}
					else
						EMIT_SOUND(ENT(pev), CHAN_ITEM, "plats/train_use1.wav", 0.8, ATTN_NORM);
					return;
				}
			}
		}
	}

	CBaseEntity* pObject = NULL;
	CBaseEntity* pClosest = NULL;
	Vector vecLOS;
	float flMaxDot = VIEW_FIELD_NARROW;
	float flDot;

	UTIL_MakeVectors(pev->v_angle); // so we know which way we are facing

	while ((pObject = UTIL_FindEntityInSphere(pObject, pev->origin, PLAYER_SEARCH_RADIUS)) != NULL)
	{

		if ((pObject->ObjectCaps() & (FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE)) != 0)
		{
			// !!!PERFORMANCE- should this check be done on a per case basis AFTER we've determined that
			// this object is actually usable? This dot is being done for every object within PLAYER_SEARCH_RADIUS
			// when player hits the use key. How many objects can be in that area, anyway? (sjb)
			vecLOS = (VecBModelOrigin(pObject->pev) - (pev->origin + pev->view_ofs));

			// This essentially moves the origin of the target to the corner nearest the player to test to see
			// if it's "hull" is in the view cone
			vecLOS = UTIL_ClampVectorToBox(vecLOS, pObject->pev->size * 0.5);

			flDot = DotProduct(vecLOS, gpGlobals->v_forward);
			if (flDot > flMaxDot)
			{ // only if the item is in front of the user
				pClosest = pObject;
				flMaxDot = flDot;
				//				ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
			}
			//			ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
		}
	}
	pObject = pClosest;

	if (!pObject)
	{
		// the Apache pickup is too big for the sphere search: look straight ahead for it
		TraceResult tr;
		Vector vecSrc = pev->origin + pev->view_ofs;

		UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 80, dont_ignore_monsters, edict(), &tr);
		if (tr.pHit)
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (pHit && 0 == strcmp(STRING(pHit->pev->classname), "pickup_drive_apache"))
				pObject = pHit;
		}
	}

	// Found an object
	if (pObject)
	{
		//!!!UNDONE: traceline here to prevent USEing buttons through walls
		int caps = pObject->ObjectCaps();

		if ((m_afButtonPressed & IN_USE) != 0)
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/wpn_select.wav", 0.4, ATTN_NORM);

		if (((pev->button & IN_USE) != 0 && (caps & FCAP_CONTINUOUS_USE) != 0) ||
			((m_afButtonPressed & IN_USE) != 0 && (caps & (FCAP_IMPULSE_USE | FCAP_ONOFF_USE)) != 0))
		{
			if ((caps & FCAP_CONTINUOUS_USE) != 0)
				m_afPhysicsFlags |= PFLAG_USING;

			pObject->Use(this, this, USE_SET, 1);
		}
		// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
		else if ((m_afButtonReleased & IN_USE) != 0 && (pObject->ObjectCaps() & FCAP_ONOFF_USE) != 0) // BUGBUG This is an "off" use
		{
			pObject->Use(this, this, USE_SET, 0);
		}
	}
	else
	{
		if ((m_afButtonPressed & IN_USE) != 0)
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/wpn_denyselect.wav", 0.4, ATTN_NORM);
	}
}



// orig 0x1ed140. RC jumps on the server: the jump speed and the long jump come from rc.ini
// ([PLAYER] JumpHeight, LongjumpForwardVelo, LongjumpZVelo), a long jump starts a butt stomp
// (ButStompTouch), the jump plays a step sound, and the old SDK's swimming-up is back.
void CBasePlayer::Jump()
{
	if (FBitSet(pev->flags, FL_WATERJUMP) || FBitSet(pev->flags, FL_FROZEN))
		return;

	if (pev->waterlevel >= 2)
	{
		if (pev->watertype == CONTENT_WATER)
			pev->velocity.z = 100;
		else if (pev->watertype == CONTENT_SLIME)
			pev->velocity.z = 80;
		else
			pev->velocity.z = 50;

		// play swiming sound
		if (m_flSwimTime < gpGlobals->time)
		{
			m_flSwimTime = gpGlobals->time + 1;
			switch (RANDOM_LONG(0, 3))
			{
			case 0:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM);
				break;
			case 1:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM);
				break;
			case 2:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM);
				break;
			case 3:
				EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM);
				break;
			}
		}
		return;
	}

	// If this isn't the first frame pressing the jump button, break out.
	if (!FBitSet(m_afButtonPressed, IN_JUMP))
		return; // don't pogo stick

	if ((pev->flags & FL_ONGROUND) == 0 || !pev->groundentity)
		return;

	// many features in this function use v_forward, so makevectors now.
	UTIL_MakeVectors(pev->angles);

	ClearBits(pev->flags, FL_ONGROUND); // don't stairwalk

	SetAnimation(PLAYER_JUMP);

	PlayStepSound(MapTextureTypeStepType(m_chTextureType), 1.0);

	if ((FBitSet(pev->flags, FL_DUCKING) || FBitSet(m_afPhysicsFlags, PFLAG_DUCKING)) && m_fLongJump &&
		(pev->button & IN_DUCK) != 0 && gpGlobals->time - m_flDuckTime < 1)
	{
		RANDOM_LONG(0, 1); // the original throws the result away
		pev->punchangle.x = -5;
		pev->velocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("PLAYER", "LongjumpForwardVelo", 200);
		pev->velocity.z = PREFS_KeyLoopUpFloat("PLAYER", "LongjumpZVelo", 500);
		SetAnimation(PLAYER_SUPERJUMP);
		m_blInButStomp = true;
		m_flMinStompTime = gpGlobals->time + 0.1;
	}
	else
	{
		// jump velocity is sqrt( height * gravity * 2)
		pev->velocity.z = sqrt(1600 * PREFS_KeyLoopUpFloat("PLAYER", "JumpHeight", 45));
	}

	// If you're standing on a conveyor, add it's velocity to yours (for momentum)
	entvars_t* pevGround = VARS(pev->groundentity);
	if (pevGround && (pevGround->flags & FL_CONVEYOR) != 0)
	{
		pev->velocity = pev->velocity + pev->basevelocity;
	}

	// halflife-updated's func_vehicle momentum isn't in the original.
}



// This is a glorious hack to find free space when you've crouched into some solid space
// Our crouching collisions do not work correctly for some reason and this is easier
// than fixing the problem :(
void FixPlayerCrouchStuck(edict_t* pPlayer)
{
	TraceResult trace;

	// Move up as many as 18 pixels if the player is stuck.
	for (int i = 0; i < 18; i++)
	{
		UTIL_TraceHull(pPlayer->v.origin, pPlayer->v.origin, dont_ignore_monsters, head_hull, pPlayer, &trace);
		if (0 != trace.fStartSolid)
			pPlayer->v.origin.z++;
		else
			break;
	}
}

// orig 0x1ed530. RC ducks on the server, the old SDK's way: holding duck lowers the view over
// 0.4 seconds and then shrinks the hull to the duck hull; letting go stands back up when the
// standing hull fits. Not while frozen, in noclip or observing.
void CBasePlayer::Duck()
{
	if (FBitSet(pev->flags, FL_FROZEN) || pev->movetype == MOVETYPE_NOCLIP || m_blIsObserver)
		return;

	if ((pev->button & IN_DUCK) != 0)
	{
		if ((m_afButtonPressed & IN_DUCK) != 0 && !FBitSet(pev->flags, FL_DUCKING))
		{
			m_flDuckTime = gpGlobals->time;
			m_afPhysicsFlags |= PFLAG_DUCKING;
		}

		float time = gpGlobals->time - m_flDuckTime;

		if ((m_afPhysicsFlags & PFLAG_DUCKING) != 0)
		{
			if (time >= 0.4 || !FBitSet(pev->flags, FL_ONGROUND))
			{
				// finished ducking (or ducked in the air)
				if (FBitSet(pev->flags, FL_ONGROUND))
				{
					pev->origin = pev->origin - Vector(0, 0, 18);
					FixPlayerCrouchStuck(edict());
				}

				UTIL_SetOrigin(pev, pev->origin);
				UTIL_SetSize(pev, Vector(-16, -16, -18), Vector(16, 16, 18));
				pev->view_ofs = Vector(0, 0, 12);
				pev->flags |= FL_DUCKING;
				m_afPhysicsFlags &= ~PFLAG_DUCKING;
			}
			else
			{
				// still lowering the view
				float duckFraction = UTIL_SplineFraction(time, 1.0 / 0.4);
				pev->view_ofs = (Vector(0, 0, 12) - (Vector(-16, -16, -18) - Vector(-16, -16, -36))) * duckFraction + Vector(0, 0, 28) * (1 - duckFraction);
			}
		}

		SetAnimation(PLAYER_WALK);
	}
	else
	{
		// try to stand up (every frame duck isn't held, ducked or not)
		TraceResult trace;
		Vector newOrigin = pev->origin;

		if (FBitSet(pev->flags, FL_ONGROUND))
			newOrigin = newOrigin + Vector(0, 0, 18);

		UTIL_TraceHull(newOrigin, newOrigin, dont_ignore_monsters, human_hull, edict(), &trace);

		if (0 == trace.fStartSolid)
		{
			ClearBits(pev->flags, FL_DUCKING);
			m_afPhysicsFlags &= ~PFLAG_DUCKING;
			pev->view_ofs = Vector(0, 0, 28);
			UTIL_SetSize(pev, Vector(-16, -16, -36), Vector(16, 16, 36));
			pev->origin = newOrigin;
		}
	}
}

//
// ID's player as such.
//
// orig 0x1ed990. RC: an observer is CLASS_NONE, a driver takes the class of what he drives.
int CBasePlayer::Classify()
{
	if (m_blIsObserver)
		return CLASS_NONE;

	if (m_bInDriveEnt && m_pCurDriveEnt && m_pCurDriveEnt->pev && m_pCurDriveEnt->pev->pContainingEntity && UTIL_IsValidEntity(m_pCurDriveEnt->pev->pContainingEntity))
		return m_pCurDriveEnt->Classify();

	return CLASS_PLAYER;
}


void CBasePlayer::AddPoints(int score, bool bAllowNegativeScore)
{
	// Positive score always adds
	if (score < 0)
	{
		if (!bAllowNegativeScore)
		{
			if (pev->frags < 0) // Can't go more negative
				return;

			if (-score > pev->frags) // Will this go negative?
			{
				score = -pev->frags; // Sum will be 0
			}
		}
	}

	pev->frags += score;

	MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
	WRITE_BYTE(ENTINDEX(edict()));
	WRITE_SHORT(pev->frags);
	WRITE_SHORT(m_iDeaths);
	WRITE_SHORT(0);
	WRITE_SHORT(g_pGameRules->GetTeamIndex(m_szTeamName) + 1);
	MESSAGE_END();
}


void CBasePlayer::AddPointsToTeam(int score, bool bAllowNegativeScore)
{
	int index = entindex();

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);

		if (pPlayer && i != index)
		{
			if (g_pGameRules->PlayerRelationship(this, pPlayer) == GR_TEAMMATE)
			{
				pPlayer->AddPoints(score, bAllowNegativeScore);
			}
		}
	}
}

// Rocket Crowbar keeps the old SDK's server-side step sounds (pm_shared plays the walking ones;
// the player code plays one when jumping).
#define STEP_CONCRETE 0 // default step sound
#define STEP_METAL 1	// metal floor
#define STEP_DIRT 2		// dirt, sand, rock
#define STEP_VENT 3		// ventillation duct
#define STEP_GRATE 4	// metal grating
#define STEP_TILE 5		// floor tiles
#define STEP_SLOSH 6	// shallow liquid puddle
#define STEP_WADE 7		// wading in liquid
#define STEP_LADDER 8	// climbing ladder

// orig 0x1edbf0. Play a step sound of the given type at fvol, alternating left and right feet.
void CBasePlayer::PlayStepSound(int step, float fvol)
{
	static int iSkipStep = 0;

	if (!g_pGameRules->PlayFootstepSounds(this, fvol))
		return;

	// irand - 0,1 for right foot, 2,3 for left foot
	// used to alternate left and right foot
	int irand = RANDOM_LONG(0, 1) + (m_iStepLeft * 2);

	m_iStepLeft = !m_iStepLeft;

	const char* pszSound = NULL;

	switch (step)
	{
	default:
	case STEP_CONCRETE:
	{
		static const char* const sounds[] = {"player/pl_step1.wav", "player/pl_step3.wav", "player/pl_step2.wav", "player/pl_step4.wav"};
		pszSound = sounds[irand];
		break;
	}
	case STEP_METAL:
	{
		static const char* const sounds[] = {"player/pl_metal1.wav", "player/pl_metal3.wav", "player/pl_metal2.wav", "player/pl_metal4.wav"};
		pszSound = sounds[irand];
		break;
	}
	case STEP_DIRT:
	{
		static const char* const sounds[] = {"player/pl_dirt1.wav", "player/pl_dirt3.wav", "player/pl_dirt2.wav", "player/pl_dirt4.wav"};
		pszSound = sounds[irand];
		break;
	}
	case STEP_VENT:
	{
		static const char* const sounds[] = {"player/pl_duct1.wav", "player/pl_duct3.wav", "player/pl_duct2.wav", "player/pl_duct4.wav"};
		pszSound = sounds[irand];
		break;
	}
	case STEP_GRATE:
	{
		static const char* const sounds[] = {"player/pl_grate1.wav", "player/pl_grate3.wav", "player/pl_grate2.wav", "player/pl_grate4.wav"};
		pszSound = sounds[irand];
		break;
	}
	case STEP_TILE:
	{
		static const char* const sounds[] = {"player/pl_tile1.wav", "player/pl_tile3.wav", "player/pl_tile2.wav", "player/pl_tile4.wav", "player/pl_tile5.wav"};
		if (0 == RANDOM_LONG(0, 4))
			irand = 4;
		pszSound = sounds[irand];
		break;
	}
	case STEP_SLOSH:
	{
		static const char* const sounds[] = {"player/pl_slosh1.wav", "player/pl_slosh3.wav", "player/pl_slosh2.wav", "player/pl_slosh4.wav"};
		pszSound = sounds[irand];
		break;
	}
	case STEP_WADE:
	{
		static const char* const sounds[] = {"player/pl_wade1.wav", "player/pl_wade2.wav", "player/pl_wade3.wav", "player/pl_wade4.wav"};
		if (iSkipStep == 0)
		{
			iSkipStep++;
			return;
		}

		if (iSkipStep++ == 3)
			iSkipStep = 0;

		pszSound = sounds[irand];
		break;
	}
	case STEP_LADDER:
	{
		static const char* const sounds[] = {"player/pl_ladder1.wav", "player/pl_ladder3.wav", "player/pl_ladder2.wav", "player/pl_ladder4.wav"};
		pszSound = sounds[irand];
		break;
	}
	}

	EMIT_SOUND(ENT(pev), CHAN_BODY, pszSound, fvol, ATTN_NORM);
}

// orig 0x1ee1a0. Simple mapping from texture type character to step type.
int MapTextureTypeStepType(char chTextureType)
{
	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE:
		return STEP_CONCRETE;
	case CHAR_TEX_METAL:
		return STEP_METAL;
	case CHAR_TEX_DIRT:
		return STEP_DIRT;
	case CHAR_TEX_VENT:
		return STEP_VENT;
	case CHAR_TEX_GRATE:
		return STEP_GRATE;
	case CHAR_TEX_TILE:
		return STEP_TILE;
	case CHAR_TEX_SLOSH:
		return STEP_SLOSH;
	}
}

// orig 0x1ee260. The old SDK's footstep timing with the sound itself taken out (the original
// computes the step type and throws it away): it keeps the walk animation, the texture under the
// player and m_flTimeStepSound up to date. Nothing while frozen or on low friction.
void CBasePlayer::UpdateStepSound()
{
	if (gpGlobals->time <= m_flTimeStepSound)
		return;

	if (FBitSet(pev->flags, FL_FROZEN))
		return;

	if (pev->friction < 1)
		return;

	float speed = pev->velocity.Length();
	bool fLadder = IsOnLadder();
	float velwalk, velrun, flduck;

	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!
	if (FBitSet(pev->flags, FL_DUCKING) || fLadder)
	{
		velwalk = 60; // These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		velrun = 80;  // UNDONE: Move walking to server
		flduck = 0.1;
	}
	else
	{
		velwalk = 120;
		velrun = 210;
		flduck = 0.0;
	}

	// if we're on a ladder or on the ground, and we're moving fast enough,
	// play step sound.  Also, if m_flTimeStepSound is zero, get the new
	// sound right away - we just started moving in new level.
	if ((fLadder || FBitSet(pev->flags, FL_ONGROUND)) && pev->velocity != g_vecZero && (speed >= velwalk || 0 == m_flTimeStepSound))
	{
		SetAnimation(PLAYER_WALK);

		bool fWalking = speed < velrun;

		Vector center, knee, feet;
		center = knee = feet = (pev->absmin + pev->absmax) * 0.5;
		float height = pev->absmax.z - pev->absmin.z;

		knee.z = pev->absmin.z + height * 0.2;
		feet.z = pev->absmin.z;

		// find out what we're stepping in or on...
		if (fLadder)
		{
			m_flTimeStepSound = gpGlobals->time + 0.35;
		}
		else if (UTIL_PointContents(knee) == CONTENTS_WATER)
		{
			m_flTimeStepSound = gpGlobals->time + 0.6;
		}
		else
		{
			if (UTIL_PointContents(feet) != CONTENTS_WATER)
			{
				// find texture under player, if different from current texture,
				// get material type
				Vector start, end;
				float rgfl1[3];
				float rgfl2[3];
				char szbuffer[64];

				start = end = center;			// center point of player BB
				start.z = end.z = pev->absmin.z; // copy zmin
				start.z += 4.0;					// extend start up
				end.z -= 24.0;					// extend end down

				start.CopyToArray(rgfl1);
				end.CopyToArray(rgfl2);

				const char* pTextureName = TRACE_TEXTURE(ENT(pev->groundentity), rgfl1, rgfl2);
				if (pTextureName)
				{
					// strip leading '-0' or '{' or '!'
					if (*pTextureName == '-')
						pTextureName += 2;
					if (*pTextureName == '{' || *pTextureName == '!')
						pTextureName++;

					if (0 != strnicmp(pTextureName, m_szTextureName, CBTEXTURENAMEMAX - 1))
					{
						// current texture is different from texture player is on...
						// set current texture
						strcpy(szbuffer, pTextureName);
						szbuffer[CBTEXTURENAMEMAX - 1] = 0;
						strcpy(m_szTextureName, szbuffer);

						// get the material type
						m_chTextureType = TEXTURETYPE_Find(m_szTextureName);
					}
				}

				MapTextureTypeStepType(m_chTextureType); // the original discards the step type
			}

			// a puddle and every texture type step at the same rate
			m_flTimeStepSound = fWalking ? gpGlobals->time + 0.4 : gpGlobals->time + 0.3;
		}

		m_flTimeStepSound += flduck; // slower step time if ducking
	}
}

// orig 0x1ee750. Rocket Crowbar's player ID: the player (or driven monster) closest to the
// crosshair within 4096 units and a 0.968 dot, if visible, is named in an orange HUD text line
// for 0.3 seconds. A driver looks from his origin along v_angle, ignoring his own monster.
void CBasePlayer::PlayerDoID()
{
	Vector vecSrc = pev->origin + pev->view_ofs;
	float flBestDot = 0.968;

	UTIL_MakeVectors(pev->v_angle + pev->punchangle);

	if (m_bInDriveEnt && m_pCurDriveEnt)
	{
		vecSrc = pev->origin;
		UTIL_MakeVectors(pev->v_angle);
	}

	Vector vecForward = gpGlobals->v_forward;
	CBaseEntity* pBest = NULL;
	CBaseEntity* pEntity = NULL;

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, 4096)) != NULL)
	{
		if (!pEntity->pev || FNullEnt(pEntity->pev))
			continue;

		edict_t* pent = pEntity->pev->pContainingEntity;
		if (!pent || !pent->pvPrivateData || pent->free || pEntity->pev == pev)
			continue;

		if (!pEntity->IsAlive() || (pEntity->pev->flags & FL_NOTARGET) != 0)
			continue;

		if (m_bInDriveEnt && m_pCurDriveEnt && m_pCurDriveEnt->pev == pEntity->pev)
			continue;

		// players, and anything somebody drives
		if (pEntity->Classify() != CLASS_PLAYER)
		{
			if (!pEntity->GetControllerName())
				continue;
			if (!pEntity->IsDriveent())
				continue;
		}

		float flZDiff = pEntity->Center().z - vecSrc.z;
		Vector vecLOS = pEntity->Center() - vecSrc;
		float flDot;

		// close up and level, only the yaw counts
		if (fabs(flZDiff) >= 32 || vecLOS.Length() >= 100 || (m_bInDriveEnt && m_pCurDriveEnt))
			flDot = DotProduct(vecLOS.Normalize(), vecForward);
		else
			flDot = DotProduct(vecLOS.Make2D().Normalize(), vecForward.Make2D().Normalize());

		if (flDot > flBestDot)
		{
			TraceResult tr;
			Vector vecEnd = pEntity->Center();

			gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
			if (!m_bInDriveEnt || !m_pCurDriveEnt)
				UTIL_TraceLine(vecSrc, vecEnd, ignore_monsters, edict(), &tr);
			else
				UTIL_TraceLine(vecSrc + vecForward * 16, vecEnd, ignore_monsters, m_pCurDriveEnt->edict(), &tr);

			if (tr.flFraction >= 1.0 || tr.pHit == pEntity->pev->pContainingEntity)
			{
				flBestDot = flDot;
				pBest = pEntity;
			}
		}
	}

	if (!pBest || FNullEnt(pBest->pev))
		return;

	char szName[256];

	if (pBest->Classify() == CLASS_PLAYER)
		sprintf(szName, "%s", STRING(pBest->pev->netname));
	else
		sprintf(szName, "%s", pBest->GetControllerName());

	MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, edict());
	WRITE_BYTE(TE_TEXTMESSAGE);
	WRITE_BYTE(14 & 0xFF); // channel
	WRITE_SHORT(FixedSigned16(-1, 1 << 13));
	WRITE_SHORT(FixedSigned16(0.7, 1 << 13));
	WRITE_BYTE(0); // effect
	WRITE_BYTE(255);
	WRITE_BYTE(128);
	WRITE_BYTE(0);
	WRITE_BYTE(100);
	WRITE_BYTE(255);
	WRITE_BYTE(128);
	WRITE_BYTE(0);
	WRITE_BYTE(100);
	WRITE_SHORT(0); // fade in
	WRITE_SHORT(0); // fade out
	WRITE_SHORT(FixedUnsigned16(0.3, 1 << 8));
	WRITE_STRING(szName);
	MESSAGE_END();
}

int g_iPlayerShockwave = 0; // sprites/shockwave.spr, precached by ClientPrecache (client.cpp)

// orig 0x1eef90. The butt stomp's ring: a blue-white beam cylinder from 12 units above vecOrigin.
void DoPlayerShockwaveEffect(Vector vecOrigin)
{
	vecOrigin = vecOrigin + Vector(0, 0, 12);

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecOrigin);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(vecOrigin.x);
	WRITE_COORD(vecOrigin.y);
	WRITE_COORD(vecOrigin.z);
	WRITE_COORD(vecOrigin.x);
	WRITE_COORD(vecOrigin.y);
	WRITE_COORD(vecOrigin.z + 1800); // reach damage radius over .2 seconds
	WRITE_SHORT(g_iPlayerShockwave);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(0);	 // framerate
	WRITE_BYTE(1);	 // life
	WRITE_BYTE(12);	 // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(220); // r
	WRITE_BYTE(220); // g
	WRITE_BYTE(255); // b
	WRITE_BYTE(180); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();
}

// orig 0x1ef190. The touch a long jump sets (Jump): landing on something that takes damage, from
// above its center, crushes it ([PLAYER] LongJumpStompDmg, inflictor classname "stomp"); landing
// on the world just makes the ring and the sound. Either way the touch ends and gravity resets.
void CBasePlayer::ButStompTouch(CBaseEntity* pOther)
{
	if (pOther->pev->takedamage != DAMAGE_NO && pev->origin.z + pev->mins.z > pOther->Center().z)
	{
		CBaseEntity* pStomp = GetClassPtr((CBaseEntity*)NULL);
		pStomp->Spawn();
		pStomp->pev->classname = MAKE_STRING("stomp");
		pOther->TakeDamage(pStomp->pev, pev, PREFS_KeyLoopUpFloat("PLAYER", "LongJumpStompDmg", 1000), DMG_CRUSH | DMG_ALWAYSGIB);
		pStomp->SUB_Remove();

		DoPlayerShockwaveEffect(Vector(pev->origin.x, pev->origin.y, pev->absmin.z));
		EMIT_SOUND_DYN(edict(), CHAN_ITEM, "garg/gar_stomp1.wav", 0.7, 1.1, 0, 130);
	}
	else
	{
		TraceResult tr;
		Vector vecFeet = Vector(pev->origin.x, pev->origin.y, pev->origin.z + pev->mins.z + 4);

		UTIL_TraceLine(vecFeet, vecFeet - Vector(0, 0, 16), ignore_monsters, edict(), &tr);

		// VERIFY: literal reading: a trace that hits nothing still counts when its pHit is a brush model
		bool bGround = tr.flFraction < 1.0;
		if (!bGround && tr.pHit && CBaseEntity::Instance(tr.pHit) && CBaseEntity::Instance(tr.pHit)->IsBSPModel())
			bGround = true;

		if (bGround)
		{
			DoPlayerShockwaveEffect(vecFeet);
			EMIT_SOUND_DYN(edict(), CHAN_ITEM, "garg/gar_stomp1.wav", 0.75, 1.1, 0, 130);
		}
	}

	SetTouch(NULL);
	pev->gravity = 1;
}

// orig 0x1eb0c0. Rocket Crowbar's death camera: an invisible "dying_cam" entity (kept in m_pCam)
// placed up to 200 units from the body at a random yaw and a 30-60 degree pitch (one time in 20
// along or against the player's velocity instead), looking at the body. PlayerDeathThink points
// the player's view at it (m_blSetView). Bots don't get one.
void CBasePlayer::StartDyingCam()
{
	if (IsBot())
		return;

	if (!m_pCam || !m_blCamInited || !m_pCam->pev->pContainingEntity->pvPrivateData)
	{
		m_pCam = GetClassPtr((CBaseEntity*)NULL);
		m_pCam->pev->movetype = MOVETYPE_NONE;
		m_pCam->pev->solid = SOLID_NOT;
		SET_MODEL(m_pCam->edict(), "models/w_isotopebox.mdl");
		m_pCam->pev->classname = MAKE_STRING("dying_cam");
		UTIL_SetOrigin(m_pCam->pev, m_pCam->pev->origin);
		UTIL_SetSize(m_pCam->pev, g_vecZero, g_vecZero);
		m_pCam->pev->owner = edict();
		m_pCam->pev->rendermode = kRenderTransColor;
		m_pCam->pev->renderamt = 0;
		m_blCamInited = true;
	}

	Vector vecAngles;
	vecAngles.y = RANDOM_FLOAT(0, 360);
	vecAngles.x = RANDOM_FLOAT(-30, -60);
	vecAngles.z = 0;

	if (RANDOM_LONG(1, 20) == 1)
	{
		if (RANDOM_FLOAT(0, 1) > 0.5)
			vecAngles = UTIL_VecToAngles(-pev->velocity);
		else
			vecAngles = UTIL_VecToAngles(pev->velocity);
		vecAngles.z = 0;
	}

	while (vecAngles.x < 0)
		vecAngles.x += 360;
	while (vecAngles.x > 360)
		vecAngles.x -= 360;
	while (vecAngles.y < 0)
		vecAngles.y += 360;
	while (vecAngles.y > 360)
		vecAngles.y -= 360;
	while (vecAngles.z < 0)
		vecAngles.z += 360;
	while (vecAngles.z > 360)
		vecAngles.z -= 360;

	UTIL_MakeVectors(vecAngles);
	Vector vecForward = gpGlobals->v_forward;

	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + vecForward * 200, ignore_monsters, edict(), &tr);

	Vector vecCam;
	if (tr.flFraction <= 0.1 || 0 != tr.fAllSolid || 0 != tr.fStartSolid)
		vecCam = pev->origin + Vector(0, 0, 16);
	else if (tr.flFraction < 1.0)
		vecCam = tr.vecEndPos + tr.vecPlaneNormal.Normalize() * 8;
	else
		vecCam = tr.vecEndPos;

	if (UTIL_PointContents(vecCam) == CONTENTS_SOLID)
		vecCam = pev->origin + Vector(0, 0, 16);

	UTIL_SetOrigin(m_pCam->pev, vecCam);

	Vector vecLook = UTIL_VecToAngles((pev->origin - vecCam).Normalize());
	m_pCam->pev->v_angle = Vector(-vecLook.x, vecLook.y, vecLook.z);
	m_pCam->pev->angles = Vector(-vecLook.x, vecLook.y, vecLook.z);

	m_blSetView = true;
}

//Player ID
void CBasePlayer::InitStatusBar()
{
	m_flStatusBarDisappearDelay = 0;
	m_SbarString1[0] = m_SbarString0[0] = 0;
}

void CBasePlayer::UpdateStatusBar()
{
	int newSBarState[SBAR_END];
	char sbuf0[SBAR_STRING_SIZE];
	char sbuf1[SBAR_STRING_SIZE];

	memset(newSBarState, 0, sizeof(newSBarState));
	strcpy(sbuf0, m_SbarString0);
	strcpy(sbuf1, m_SbarString1);

	// Find an ID Target
	TraceResult tr;
	UTIL_MakeVectors(pev->v_angle + pev->punchangle);
	Vector vecSrc = EyePosition();
	Vector vecEnd = vecSrc + (gpGlobals->v_forward * MAX_ID_RANGE);
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, edict(), &tr);

	if (tr.flFraction != 1.0)
	{
		if (!FNullEnt(tr.pHit))
		{
			CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);

			if (pEntity->Classify() == CLASS_PLAYER)
			{
				newSBarState[SBAR_ID_TARGETNAME] = ENTINDEX(pEntity->edict());
				strcpy(sbuf1, "1 %p1\n2 Health: %i2%%\n3 Armor: %i3%%");

				// allies and medics get to see the targets health
				if (g_pGameRules->PlayerRelationship(this, pEntity) == GR_TEAMMATE)
				{
					newSBarState[SBAR_ID_TARGETHEALTH] = 100 * (pEntity->pev->health / pEntity->pev->max_health);
					newSBarState[SBAR_ID_TARGETARMOR] = pEntity->pev->armorvalue; //No need to get it % based since 100 it's the max.
				}

				m_flStatusBarDisappearDelay = gpGlobals->time + 1.0;
			}
		}
		else if (m_flStatusBarDisappearDelay > gpGlobals->time)
		{
			// hold the values for a short amount of time after viewing the object
			newSBarState[SBAR_ID_TARGETNAME] = m_izSBarState[SBAR_ID_TARGETNAME];
			newSBarState[SBAR_ID_TARGETHEALTH] = m_izSBarState[SBAR_ID_TARGETHEALTH];
			newSBarState[SBAR_ID_TARGETARMOR] = m_izSBarState[SBAR_ID_TARGETARMOR];
		}
	}

	bool bForceResend = false;

	if (0 != strcmp(sbuf0, m_SbarString0))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgStatusText, NULL, pev);
		WRITE_BYTE(0);
		WRITE_STRING(sbuf0);
		MESSAGE_END();

		strcpy(m_SbarString0, sbuf0);

		// make sure everything's resent
		bForceResend = true;
	}

	if (0 != strcmp(sbuf1, m_SbarString1))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgStatusText, NULL, pev);
		WRITE_BYTE(1);
		WRITE_STRING(sbuf1);
		MESSAGE_END();

		strcpy(m_SbarString1, sbuf1);

		// make sure everything's resent
		bForceResend = true;
	}

	// Check values and send if they don't match
	for (int i = 1; i < SBAR_END; i++)
	{
		if (newSBarState[i] != m_izSBarState[i] || bForceResend)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgStatusValue, NULL, pev);
			WRITE_BYTE(i);
			WRITE_SHORT(newSBarState[i]);
			MESSAGE_END();

			m_izSBarState[i] = newSBarState[i];
		}
	}
}









#define CLIMB_SHAKE_FREQUENCY 22 // how many frames in between screen shakes when climbing
#define MAX_CLIMB_SPEED 200		 // fastest vertical climbing speed possible
#define CLIMB_SPEED_DEC 15		 // climbing deceleration rate
#define CLIMB_PUNCH_X -7		 // how far to 'punch' client X axis when climbing
#define CLIMB_PUNCH_Z 7			 // how far to 'punch' client Z axis when climbing

// orig 0x1ef420. RC: the pregame observer (ObserverFrame) and its respawn, the hand-off to a
// driven monster (DEClientPreThink, and nothing else of the player's think while driving), the
// player ID every 0.3 s, curse messages, health and armor regeneration from rc.ini, and the
// butt stomp. The old SDK's node graph pointer check isn't in it, nor halflife-updated's view
// entity reset or its spectator buttons.
void CBasePlayer::PreThink()
{
	int buttonsChanged = (m_afButtonLast ^ pev->button); // These buttons have changed this frame

	m_afButtonLast = pev->button;

	// Debounced button codes for pressed/released
	// UNDONE: Do we need auto-repeat?
	m_afButtonPressed = buttonsChanged & pev->button;	  // The changed ones still down are "pressed"
	m_afButtonReleased = buttonsChanged & (~pev->button); // The ones not down are "released"

	g_pGameRules->PlayerThink(this);

	if (g_fGameOver)
		return; // intermission or finale

	// JOHN: checks if new client data (for HUD and view control) needs to be sent to the client
	UpdateClientData();

	if (m_blIsObserver)
	{
		ObserverFrame(this);
		if (m_blIsObserver)
			return;
	}

	if (m_blObserverRespawn)
	{
		Spawn();
		m_fInitHUD = true;
		UpdateClientData();
		m_blObserverRespawn = false;
		return;
	}

	if (m_bInDriveEnt && m_pCurDriveEnt)
		m_pCurDriveEnt->DEClientPreThink();

	if (IsNetClient() && m_flIDTime < gpGlobals->time)
	{
		PlayerDoID();
		m_flIDTime = gpGlobals->time + 0.3;
	}

	// the driven monster does the rest
	if (m_bInDriveEnt && m_pCurDriveEnt)
		return;

	UTIL_MakeVectors(pev->v_angle); // is this still used?

	ItemPreFrame();
	WaterMove();

	if (g_pGameRules && g_pGameRules->FAllowFlashlight())
		m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	else
		m_iHideHUD |= HIDEHUD_FLASHLIGHT;

	CheckTimeBasedDamage();

	CheckSuitUpdate();

	if (pev->deadflag >= DEAD_DYING)
	{
		PlayerDeathThink();
		return;
	}

	DrawCursedMsg(this);

	// health and armor regeneration (on when rc.ini sets the times and the timers were started)
	if (m_fHealthRegenTime > 0 && m_fHealthRegenTime < gpGlobals->time && PREFS_KeyLoopUpFloat("PLAYER", "HealthRegenTime", -1) > 0)
	{
		m_fHealthRegenTime = PREFS_KeyLoopUpFloat("PLAYER", "HealthRegenTime", -1) + gpGlobals->time;
		if (pev->health < PREFS_KeyLoopUpFloat("PLAYER", "HealthRegenMax", pev->max_health))
			TakeHealth(PREFS_KeyLoopUpFloat("PLAYER", "HealthRegenAmount", 1), DMG_GENERIC);
	}

	if (m_fArmorRegenTime > 0 && m_fArmorRegenTime < gpGlobals->time && PREFS_KeyLoopUpFloat("PLAYER", "ArmorRegenTime", -1) > 0)
	{
		m_fArmorRegenTime = PREFS_KeyLoopUpFloat("PLAYER", "ArmorRegenTime", -1) + gpGlobals->time;
		if (pev->armorvalue < (float)PREFS_KeyLoopUpInt("PLAYER", "ArmorRegenMax", PREFS_KeyLoopUpIntClamp("PLAYER", "MaxArmor", 100, 0, 999)))
		{
			pev->armorvalue += PREFS_KeyLoopUpFloat("PLAYER", "ArmorRegenAmount", 1);
			if (pev->armorvalue > (float)PREFS_KeyLoopUpIntClamp("PLAYER", "MaxArmor", 100, 0, 999))
				pev->armorvalue = (float)PREFS_KeyLoopUpIntClamp("PLAYER", "MaxArmor", 100, 0, 999);
		}
	}

	// So the correct flags get sent to client asap.
	//
	if ((m_afPhysicsFlags & PFLAG_ONTRAIN) != 0)
		pev->flags |= FL_ONTRAIN;
	else
		pev->flags &= ~FL_ONTRAIN;

	// Train speed control
	if ((m_afPhysicsFlags & PFLAG_ONTRAIN) != 0)
	{
		CBaseEntity* pTrain = CBaseEntity::Instance(pev->groundentity);
		float vel;

		if (!pTrain)
		{
			TraceResult trainTrace;
			// Maybe this is on the other side of a level transition
			UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, -38), ignore_monsters, ENT(pev), &trainTrace);

			// HACKHACK - Just look for the func_tracktrain classname
			if (trainTrace.flFraction != 1.0 && trainTrace.pHit)
				pTrain = CBaseEntity::Instance(trainTrace.pHit);


			if (!pTrain || (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) == 0 || !pTrain->OnControls(pev))
			{
				//ALERT( at_error, "In train mode with no train!\n" );
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW | TRAIN_OFF;
				if (pTrain && pTrain->Classify() == CLASS_VEHICLE)
					((CFuncVehicle*)pTrain)->m_pDriver = NULL;
				return;
			}
		}
		else if (!FBitSet(pev->flags, FL_ONGROUND) || FBitSet(pTrain->pev->spawnflags, SF_TRACKTRAIN_NOCONTROL) || ((pev->button & (IN_MOVELEFT | IN_MOVERIGHT)) != 0 && pTrain->Classify() != CLASS_VEHICLE))
		{
			// Turn off the train if you jump, strafe, or the train controls go dead
			// and it isn't a func_vehicle
			m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
			m_iTrain = TRAIN_NEW | TRAIN_OFF;
			return;
		}

		pev->velocity = g_vecZero;
		vel = 0;
		if (pTrain->Classify() == CLASS_VEHICLE)
		{
			if (pev->button & IN_FORWARD)
			{
				vel = 1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			if (pev->button & IN_BACK)
			{
				vel = -1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			if (pev->button & IN_MOVELEFT)
			{
				vel = 20;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			if (pev->button & IN_MOVERIGHT)
			{
				vel = 30;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
		}
		else
		{
			if ((m_afButtonPressed & IN_FORWARD) != 0)
			{
				vel = 1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			else if ((m_afButtonPressed & IN_BACK) != 0)
			{
				vel = -1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
		}

		if (0 != vel)
		{
			m_iTrain = TrainSpeed(pTrain->pev->speed, pTrain->pev->impulse);
			m_iTrain |= TRAIN_ACTIVE | TRAIN_NEW;
		}
	}
	else if ((m_iTrain & TRAIN_ACTIVE) != 0)
		m_iTrain = TRAIN_NEW; // turn off train

	// a long jump turns into a butt stomp once it starts coming down
	if (m_blInButStomp)
	{
		if (!FBitSet(pev->flags, FL_ONGROUND) && !IsOnLadder())
		{
			if (pev->velocity.z < 10 && m_flMinStompTime < gpGlobals->time)
			{
				pev->gravity = 3;
				m_blInButStomp = false;
				SetTouch(&CBasePlayer::ButStompTouch);
			}
		}
		else
			m_blInButStomp = false;
	}

	if ((pev->button & IN_JUMP) != 0)
	{
		// If on a ladder, jump off the ladder
		// else Jump
		Jump();
	}


	// If trying to duck, already ducked, or in the process of ducking
	if ((pev->button & IN_DUCK) != 0 || FBitSet(pev->flags, FL_DUCKING) || (m_afPhysicsFlags & PFLAG_DUCKING) != 0)
		Duck();

	UpdateStepSound();

	if (!FBitSet(pev->flags, FL_ONGROUND))
	{
		m_flFallVelocity = -pev->velocity.z;
	}

	// StudioFrameAdvance( );//!!!HACKHACK!!! Can't be hit by traceline when not animating?

	// Clear out ladder pointer
	m_hEnemy = NULL;

	if ((m_afPhysicsFlags & PFLAG_ONBARNACLE) != 0)
	{
		pev->velocity = g_vecZero;
	}
}
/* Time based Damage works as follows: 
	1) There are several types of timebased damage:

		#define DMG_PARALYZE		(1 << 14)	// slows affected creature down
		#define DMG_NERVEGAS		(1 << 15)	// nerve toxins, very bad
		#define DMG_POISON			(1 << 16)	// blood poisioning
		#define DMG_RADIATION		(1 << 17)	// radiation exposure
		#define DMG_DROWNRECOVER	(1 << 18)	// drown recovery
		#define DMG_ACID			(1 << 19)	// toxic chemicals or acid burns
		#define DMG_SLOWBURN		(1 << 20)	// in an oven
		#define DMG_SLOWFREEZE		(1 << 21)	// in a subzero freezer

	2) A new hit inflicting tbd restarts the tbd counter - each monster has an 8bit counter,
		per damage type. The counter is decremented every second, so the maximum time 
		an effect will last is 255/60 = 4.25 minutes.  Of course, staying within the radius
		of a damaging effect like fire, nervegas, radiation will continually reset the counter to max.

	3) Every second that a tbd counter is running, the player takes damage.  The damage
		is determined by the type of tdb.  
			Paralyze		- 1/2 movement rate, 30 second duration.
			Nervegas		- 5 points per second, 16 second duration = 80 points max dose.
			Poison			- 2 points per second, 25 second duration = 50 points max dose.
			Radiation		- 1 point per second, 50 second duration = 50 points max dose.
			Drown			- 5 points per second, 2 second duration.
			Acid/Chemical	- 5 points per second, 10 second duration = 50 points max.
			Burn			- 10 points per second, 2 second duration.
			Freeze			- 3 points per second, 10 second duration = 30 points max.

	4) Certain actions or countermeasures counteract the damaging effects of tbds:

		Armor/Heater/Cooler - Chemical(acid),burn, freeze all do damage to armor power, then to body
							- recharged by suit recharger
		Air In Lungs		- drowning damage is done to air in lungs first, then to body
							- recharged by poking head out of water
							- 10 seconds if swiming fast
		Air In SCUBA		- drowning damage is done to air in tanks first, then to body
							- 2 minutes in tanks. Need new tank once empty.
		Radiation Syringe	- Each syringe full provides protection vs one radiation dosage
		Antitoxin Syringe	- Each syringe full provides protection vs one poisoning (nervegas or poison).
		Health kit			- Immediate stop to acid/chemical, fire or freeze damage.
		Radiation Shower	- Immediate stop to radiation damage, acid/chemical or fire damage.
		
	
*/

// If player is taking time based damage, continue doing damage to player -
// this simulates the effect of being poisoned, gassed, dosed with radiation etc -
// anything that continues to do damage even after the initial contact stops.
// Update all time based damage counters, and shut off any that are done.

// The m_bitsDamageType bit MUST be set if any damage is to be taken.
// This routine will detect the initial on value of the m_bitsDamageType
// and init the appropriate counter.  Only processes damage every second.

//#define PARALYZE_DURATION	30		// number of 2 second intervals to take damage
//#define PARALYZE_DAMAGE		0.0		// damage to take each 2 second interval

//#define NERVEGAS_DURATION	16
//#define NERVEGAS_DAMAGE		5.0

//#define POISON_DURATION		25
//#define POISON_DAMAGE		2.0

//#define RADIATION_DURATION	50
//#define RADIATION_DAMAGE	1.0

//#define ACID_DURATION		10
//#define ACID_DAMAGE			5.0

//#define SLOWBURN_DURATION	2
//#define SLOWBURN_DAMAGE		1.0

//#define SLOWFREEZE_DURATION	1.0
//#define SLOWFREEZE_DAMAGE	3.0

/* */


void CBasePlayer::CheckTimeBasedDamage()
{
	int i;
	byte bDuration = 0;

	static float gtbdPrev = 0.0;

	if ((m_bitsDamageType & DMG_TIMEBASED) == 0)
		return;

	// only check for time based damage approx. every 2 seconds
	if (fabs(gpGlobals->time - m_tbdPrev) < 2.0)
		return;

	m_tbdPrev = gpGlobals->time;

	for (i = 0; i < CDMG_TIMEBASED; i++)
	{
		// make sure bit is set for damage type
		if ((m_bitsDamageType & (DMG_PARALYZE << i)) != 0)
		{
			switch (i)
			{
			case itbd_Paralyze:
				// UNDONE - flag movement as half-speed
				bDuration = PARALYZE_DURATION;
				break;
			case itbd_NerveGas:
				//				TakeDamage(pev, pev, NERVEGAS_DAMAGE, DMG_GENERIC);
				bDuration = NERVEGAS_DURATION;
				break;
			case itbd_Poison:
				TakeDamage(pev, pev, POISON_DAMAGE, DMG_GENERIC);
				bDuration = POISON_DURATION;
				break;
			case itbd_Radiation:
				//				TakeDamage(pev, pev, RADIATION_DAMAGE, DMG_GENERIC);
				bDuration = RADIATION_DURATION;
				break;
			case itbd_DrownRecover:
				// NOTE: this hack is actually used to RESTORE health
				// after the player has been drowning and finally takes a breath
				if (m_idrowndmg > m_idrownrestored)
				{
					int idif = V_min(m_idrowndmg - m_idrownrestored, 10);

					TakeHealth(idif, DMG_GENERIC);
					m_idrownrestored += idif;
				}
				bDuration = 4; // get up to 5*10 = 50 points back
				break;
			case itbd_Acid:
				//				TakeDamage(pev, pev, ACID_DAMAGE, DMG_GENERIC);
				bDuration = ACID_DURATION;
				break;
			case itbd_SlowBurn:
				//				TakeDamage(pev, pev, SLOWBURN_DAMAGE, DMG_GENERIC);
				bDuration = SLOWBURN_DURATION;
				break;
			case itbd_SlowFreeze:
				//				TakeDamage(pev, pev, SLOWFREEZE_DAMAGE, DMG_GENERIC);
				bDuration = SLOWFREEZE_DURATION;
				break;
			default:
				bDuration = 0;
			}

			if (0 != m_rgbTimeBasedDamage[i])
			{
				// use up an antitoxin on poison or nervegas after a few seconds of damage
				if (((i == itbd_NerveGas) && (m_rgbTimeBasedDamage[i] < NERVEGAS_DURATION)) ||
					((i == itbd_Poison) && (m_rgbTimeBasedDamage[i] < POISON_DURATION)))
				{
					if (0 != m_rgItems[ITEM_ANTIDOTE])
					{
						m_rgbTimeBasedDamage[i] = 0;
						m_rgItems[ITEM_ANTIDOTE]--;
						SetSuitUpdate("!HEV_HEAL4", false, SUIT_REPEAT_OK);
					}
				}


				// decrement damage duration, detect when done.
				if (0 == m_rgbTimeBasedDamage[i] || --m_rgbTimeBasedDamage[i] == 0)
				{
					m_rgbTimeBasedDamage[i] = 0;
					// if we're done, clear damage bits
					m_bitsDamageType &= ~(DMG_PARALYZE << i);
				}
			}
			else
				// first time taking this damage type - init damage duration
				m_rgbTimeBasedDamage[i] = bDuration;
		}
	}
}

/*
THE POWER SUIT

The Suit provides 3 main functions: Protection, Notification and Augmentation. 
Some functions are automatic, some require power. 
The player gets the suit shortly after getting off the train in C1A0 and it stays
with him for the entire game.

Protection

	Heat/Cold
		When the player enters a hot/cold area, the heating/cooling indicator on the suit 
		will come on and the battery will drain while the player stays in the area. 
		After the battery is dead, the player starts to take damage. 
		This feature is built into the suit and is automatically engaged.
	Radiation Syringe
		This will cause the player to be immune from the effects of radiation for N seconds. Single use item.
	Anti-Toxin Syringe
		This will cure the player from being poisoned. Single use item.
	Health
		Small (1st aid kits, food, etc.)
		Large (boxes on walls)
	Armor
		The armor works using energy to create a protective field that deflects a
		percentage of damage projectile and explosive attacks. After the armor has been deployed,
		it will attempt to recharge itself to full capacity with the energy reserves from the battery.
		It takes the armor N seconds to fully charge. 

Notification (via the HUD)

x	Health
x	Ammo  
x	Automatic Health Care
		Notifies the player when automatic healing has been engaged. 
x	Geiger counter
		Classic Geiger counter sound and status bar at top of HUD 
		alerts player to dangerous levels of radiation. This is not visible when radiation levels are normal.
x	Poison
	Armor
		Displays the current level of armor. 

Augmentation 

	Reanimation (w/adrenaline)
		Causes the player to come back to life after he has been dead for 3 seconds. 
		Will not work if player was gibbed. Single use.
	Long Jump
		Used by hitting the ??? key(s). Caused the player to further than normal.
	SCUBA	
		Used automatically after picked up and after player enters the water. 
		Works for N seconds. Single use.	
	
Things powered by the battery

	Armor		
		Uses N watts for every M units of damage.
	Heat/Cool	
		Uses N watts for every second in hot/cold area.
	Long Jump	
		Uses N watts for every jump.
	Alien Cloak	
		Uses N watts for each use. Each use lasts M seconds.
	Alien Shield	
		Augments armor. Reduces Armor drain by one half
 
*/

// if in range of radiation source, ping geiger counter

#define GEIGERDELAY 0.25

void CBasePlayer::UpdateGeigerCounter()
{
	byte range;

	// delay per update ie: don't flood net with these msgs
	if (gpGlobals->time < m_flgeigerDelay)
		return;

	m_flgeigerDelay = gpGlobals->time + GEIGERDELAY;

	// send range to radition source to client

	range = (byte)(m_flgeigerRange / 4);

	if (range != m_igeigerRangePrev)
	{
		m_igeigerRangePrev = range;

		MESSAGE_BEGIN(MSG_ONE, gmsgGeigerRange, NULL, pev);
		WRITE_BYTE(range);
		MESSAGE_END();
	}

	// reset counter and semaphore
	if (!RANDOM_LONG(0, 3))
		m_flgeigerRange = 1000;
}

/*
================
CheckSuitUpdate

Play suit update if it's time
================
*/

#define SUITUPDATETIME 3.5
#define SUITFIRSTUPDATETIME 0.1

void CBasePlayer::CheckSuitUpdate()
{
	int i;
	int isentence = 0;
	int isearch = m_iSuitPlayNext;

	// Ignore suit updates if no suit
	if (!HasSuit())
		return;

	// if in range of radiation source, ping geiger counter
	UpdateGeigerCounter();

	if (g_pGameRules->IsMultiplayer())
	{
		// don't bother updating HEV voice in multiplayer.
		return;
	}

	if (gpGlobals->time >= m_flSuitUpdate && m_flSuitUpdate > 0)
	{
		// play a sentence off of the end of the queue
		for (i = 0; i < CSUITPLAYLIST; i++)
		{
			isentence = m_rgSuitPlayList[isearch];
			if (0 != isentence)
				break;

			if (++isearch == CSUITPLAYLIST)
				isearch = 0;
		}

		if (0 != isentence)
		{
			m_rgSuitPlayList[isearch] = 0;
			if (isentence > 0)
			{
				// play sentence number

				char sentence[CBSENTENCENAME_MAX + 1];
				strcpy(sentence, "!");
				strcat(sentence, gszallsentencenames[isentence]);
				EMIT_SOUND_SUIT(ENT(pev), sentence);
			}
			else
			{
				// play sentence group
				EMIT_GROUPID_SUIT(ENT(pev), -isentence);
			}
			m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
		}
		else
			// queue is empty, don't check
			m_flSuitUpdate = 0;
	}
}

// add sentence to suit playlist queue. if fgroup is true, then
// name is a sentence group (HEV_AA), otherwise name is a specific
// sentence name ie: !HEV_AA0.  If iNoRepeat is specified in
// seconds, then we won't repeat playback of this word or sentence
// for at least that number of seconds.

void CBasePlayer::SetSuitUpdate(const char* name, bool fgroup, int iNoRepeatTime)
{
	int i;
	int isentence;
	int iempty = -1;


	// Ignore suit updates if no suit
	if (!HasSuit())
		return;

	if (g_pGameRules->IsMultiplayer())
	{
		// due to static channel design, etc. We don't play HEV sounds in multiplayer right now.
		return;
	}

	// if name == NULL, then clear out the queue

	if (!name)
	{
		for (i = 0; i < CSUITPLAYLIST; i++)
			m_rgSuitPlayList[i] = 0;
		return;
	}
	// get sentence or group number
	if (!fgroup)
	{
		isentence = SENTENCEG_Lookup(name, NULL);
		if (isentence < 0)
			return;
	}
	else
		// mark group number as negative
		isentence = -SENTENCEG_GetIndex(name);

	// check norepeat list - this list lets us cancel
	// the playback of words or sentences that have already
	// been played within a certain time.

	for (i = 0; i < CSUITNOREPEAT; i++)
	{
		if (isentence == m_rgiSuitNoRepeat[i])
		{
			// this sentence or group is already in
			// the norepeat list

			if (m_rgflSuitNoRepeatTime[i] < gpGlobals->time)
			{
				// norepeat time has expired, clear it out
				m_rgiSuitNoRepeat[i] = 0;
				m_rgflSuitNoRepeatTime[i] = 0.0;
				iempty = i;
				break;
			}
			else
			{
				// don't play, still marked as norepeat
				return;
			}
		}
		// keep track of empty slot
		if (0 == m_rgiSuitNoRepeat[i])
			iempty = i;
	}

	// sentence is not in norepeat list, save if norepeat time was given

	if (0 != iNoRepeatTime)
	{
		if (iempty < 0)
			iempty = RANDOM_LONG(0, CSUITNOREPEAT - 1); // pick random slot to take over
		m_rgiSuitNoRepeat[iempty] = isentence;
		m_rgflSuitNoRepeatTime[iempty] = iNoRepeatTime + gpGlobals->time;
	}

	// find empty spot in queue, or overwrite last spot

	m_rgSuitPlayList[m_iSuitPlayNext++] = isentence;
	if (m_iSuitPlayNext == CSUITPLAYLIST)
		m_iSuitPlayNext = 0;

	if (m_flSuitUpdate <= gpGlobals->time)
	{
		if (m_flSuitUpdate == 0)
			// play queue is empty, don't delay too long before playback
			m_flSuitUpdate = gpGlobals->time + SUITFIRSTUPDATETIME;
		else
			m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
	}
}

/*
================
CheckPowerups

Check for turning off powerups

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
================
*/
// orig 0x1f0260 (a global in the original)
void CheckPowerups(entvars_t* pev)
{
	if (pev->health <= 0)
		return;

	pev->modelindex = g_ulModelIndexPlayer; // don't use eyes
}


//=========================================================
// UpdatePlayerSound - updates the position of the player's
// reserved sound slot in the sound list.
//=========================================================
void CBasePlayer::UpdatePlayerSound()
{
	int iBodyVolume;
	int iVolume;
	CSound* pSound;

	pSound = CSoundEnt::SoundPointerForIndex(CSoundEnt::ClientSoundIndex(edict()));

	if (!pSound)
	{
		ALERT(at_console, "Client lost reserved sound!\n");
		return;
	}

	pSound->m_iType = bits_SOUND_NONE;

	// now calculate the best target volume for the sound. If the player's weapon
	// is louder than his body/movement, use the weapon volume, else, use the body volume.

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		iBodyVolume = pev->velocity.Length();

		// clamp the noise that can be made by the body, in case a push trigger,
		// weapon recoil, or anything shoves the player abnormally fast.
		if (iBodyVolume > 512)
		{
			iBodyVolume = 512;
		}
	}
	else
	{
		iBodyVolume = 0;
	}

	if ((pev->button & IN_JUMP) != 0)
	{
		iBodyVolume += 100;
	}

	// convert player move speed and actions into sound audible by monsters.
	if (m_iWeaponVolume > iBodyVolume)
	{
		m_iTargetVolume = m_iWeaponVolume;

		// OR in the bits for COMBAT sound if the weapon is being louder than the player.
		pSound->m_iType |= bits_SOUND_COMBAT;
	}
	else
	{
		m_iTargetVolume = iBodyVolume;
	}

	// decay weapon volume over time so bits_SOUND_COMBAT stays set for a while
	m_iWeaponVolume -= 250 * gpGlobals->frametime;
	if (m_iWeaponVolume < 0)
	{
		iVolume = 0;
	}


	// if target volume is greater than the player sound's current volume, we paste the new volume in
	// immediately. If target is less than the current volume, current volume is not set immediately to the
	// lower volume, rather works itself towards target volume over time. This gives monsters a much better chance
	// to hear a sound, especially if they don't listen every frame.
	iVolume = pSound->m_iVolume;

	if (m_iTargetVolume > iVolume)
	{
		iVolume = m_iTargetVolume;
	}
	else if (iVolume > m_iTargetVolume)
	{
		iVolume -= 250 * gpGlobals->frametime;

		if (iVolume < m_iTargetVolume)
		{
			iVolume = 0;
		}
	}

	// RC 1.9's own: sv_ignoreplayer silences players too (orig 0x1f02a0)
	if (m_fNoPlayerSound || CVAR_GET_FLOAT("sv_ignoreplayer") != 0)
	{
		// debugging flag, lets players move around and shoot without monsters hearing.
		iVolume = 0;
	}

	if (gpGlobals->time > m_flStopExtraSoundTime)
	{
		// since the extra sound that a weapon emits only lasts for one client frame, we keep that sound around for a server frame or two
		// after actual emission to make sure it gets heard.
		m_iExtraSoundTypes = 0;
	}

	if (pSound)
	{
		pSound->m_vecOrigin = pev->origin;
		pSound->m_iType |= (bits_SOUND_PLAYER | m_iExtraSoundTypes);
		pSound->m_iVolume = iVolume;
	}

	// keep track of virtual muzzle flash
	m_iWeaponFlash -= 256 * gpGlobals->frametime;
	if (m_iWeaponFlash < 0)
		m_iWeaponFlash = 0;

	// RC 1.9's (as SDK 2.1); halflife-updated commented these out, which leaves the global
	// vectors as the previous caller set them for whatever reads them next
	UTIL_MakeVectors(pev->angles);
	gpGlobals->v_forward.z = 0;

	// Below are a couple of useful little bits that make it easier to determine just how much noise the
	// player is making.
	// UTIL_ParticleEffect ( pev->origin + gpGlobals->v_forward * iVolume, g_vecZero, 255, 25 );
	//ALERT ( at_console, "%d/%d\n", iVolume, m_iTargetVolume );
}


// orig 0x1f0520. RC: nothing for an observer, a driver hands over to his monster
// (DEClientPostThink), the safe fall speed comes from rc.ini ([PLAYER] MaxSafeFallVelo), a hard
// landing plays the fall pain sounds and a landing step sound, and falling does no damage (the
// original asks the game rules for the fall damage and only uses it for the splat sound).
// The original returns early where this goes to pt_end; pt_end is halflife-updated's decay of
// the predicted weapon timers, which stand in for the original's absolute weapon times.
void CBasePlayer::PostThink()
{
	if (m_blIsObserver)
		goto pt_end;

	if (g_fGameOver)
		goto pt_end; // intermission or finale

	if (!IsAlive())
		goto pt_end;

	if (m_bInDriveEnt && m_pCurDriveEnt)
	{
		m_pCurDriveEnt->DEClientPostThink();
		goto pt_end;
	}

	// Handle Tank controlling
	if (m_pTank != NULL)
	{ // if they've moved too far from the gun,  or selected a weapon, unuse the gun
		if (m_pTank->OnControls(pev) && 0 == pev->weaponmodel)
		{
			m_pTank->Use(this, this, USE_SET, 2); // try fire the gun
		}
		else
		{ // they've moved off the platform
			m_pTank->Use(this, this, USE_OFF, 0);
			m_pTank = NULL;
		}
	}

	// do weapon stuff
	ItemPostFrame();

	// check to see if player landed hard enough to make a sound
	// falling farther than half of the maximum safe distance, but not as far a max safe distance will
	// play a bootscrape sound, and no damage will be inflicted. Fallling a distance shorter than half
	// of maximum safe distance will make no sound. Falling farther than max safe distance will play a
	// fallpain sound, and damage will be inflicted based on how far the player fell

	if ((FBitSet(pev->flags, FL_ONGROUND)) && (pev->health > 0) && m_flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD)
	{
		float fvol = 0.5;

		if (pev->watertype == CONTENT_WATER)
		{
			// Did he hit the world or a non-moving entity?
			// BUG - this happens all the time in water, especially when
			// BUG - water has current force
			// if ( !pev->groundentity || VARS(pev->groundentity)->velocity.z == 0 )
			// EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM);
		}
		else if (m_flFallVelocity > PREFS_KeyLoopUpFloat("PLAYER", "MaxSafeFallVelo", 580))
		{
			switch (RANDOM_LONG(0, 1))
			{
			case 0:
				EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_fallpain2.wav", 1, ATTN_NORM);
				// the original has no break here: case 0 plays both sounds
				[[fallthrough]];
			case 1:
				EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_fallpain3.wav", 1, ATTN_NORM);
				break;
			}

			float flFallDamage = g_pGameRules->FlPlayerFallDamage(this);

			if (flFallDamage > pev->health)
			{ //splat
				// note: play on item channel because we play footstep landing on body channel
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/bodysplat.wav", 1, ATTN_NORM);
			}

			fvol = 1.0;
		}
		else if (m_flFallVelocity > PREFS_KeyLoopUpFloat("PLAYER", "MaxSafeFallVelo", 580) * 0.5)
		{
			fvol = 0.85;
		}
		else if (m_flFallVelocity < PLAYER_MIN_BOUNCE_SPEED)
		{
			fvol = 0;
		}

		if (fvol > 0.0)
		{
			// get current texture under player right away
			m_flTimeStepSound = 0;
			UpdateStepSound();

			// play step sound for current texture
			PlayStepSound(MapTextureTypeStepType(m_chTextureType), fvol);

			RANDOM_LONG(0, 1); // the original throws the result away
		}

		if (IsAlive())
		{
			SetAnimation(PLAYER_WALK);
		}
	}

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		if (m_flFallVelocity > 64 && !g_pGameRules->IsMultiplayer())
		{
			CSoundEnt::InsertSound(bits_SOUND_PLAYER, pev->origin, m_flFallVelocity, 0.2);
			// ALERT( at_console, "fall %f\n", m_flFallVelocity );
		}
		m_flFallVelocity = 0;
	}

	// select the proper animation for the player character
	if (IsAlive())
	{
		if (0 == pev->velocity.x && 0 == pev->velocity.y)
			SetAnimation(PLAYER_IDLE);
		else if ((0 != pev->velocity.x || 0 != pev->velocity.y) && (FBitSet(pev->flags, FL_ONGROUND)))
			SetAnimation(PLAYER_WALK);
		else if (pev->waterlevel > 1)
			SetAnimation(PLAYER_WALK);
	}

	StudioFrameAdvance();
	CheckPowerups(pev);

	UpdatePlayerSound();

pt_end:
#if defined(CLIENT_WEAPONS)
	// Decay timers on weapons
	// go through all of the weapons and make a list of the ones to pack
	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			CBasePlayerItem* pPlayerItem = m_rgpPlayerItems[i];

			while (pPlayerItem)
			{
				CBasePlayerWeapon* gun = pPlayerItem->GetWeaponPtr();

				if (gun && gun->UseDecrement())
				{
					gun->m_flNextPrimaryAttack = V_max(gun->m_flNextPrimaryAttack - gpGlobals->frametime, -1.0f);
					gun->m_flNextSecondaryAttack = V_max(gun->m_flNextSecondaryAttack - gpGlobals->frametime, -0.001f);

					if (gun->m_flTimeWeaponIdle != 1000)
					{
						gun->m_flTimeWeaponIdle = V_max(gun->m_flTimeWeaponIdle - gpGlobals->frametime, -0.001f);
					}

					if (gun->pev->fuser1 != 1000)
					{
						gun->pev->fuser1 = V_max(gun->pev->fuser1 - gpGlobals->frametime, -0.001f);
					}

					gun->DecrementTimers();

					// Only decrement if not flagged as NO_DECREMENT
					//					if ( gun->m_flPumpTime != 1000 )
					//	{
					//		gun->m_flPumpTime	= V_max( gun->m_flPumpTime - gpGlobals->frametime, -0.001 );
					//	}
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	m_flNextAttack -= gpGlobals->frametime;
	if (m_flNextAttack < -0.001)
		m_flNextAttack = -0.001;

	if (m_flNextAmmoBurn != 1000)
	{
		m_flNextAmmoBurn -= gpGlobals->frametime;

		if (m_flNextAmmoBurn < -0.001)
			m_flNextAmmoBurn = -0.001;
	}

	if (m_flAmmoStartCharge != 1000)
	{
		m_flAmmoStartCharge -= gpGlobals->frametime;

		if (m_flAmmoStartCharge < -0.001)
			m_flAmmoStartCharge = -0.001;
	}
#endif

	// Track button info so we can detect 'pressed' and 'released' buttons next frame
	// halflife-updated's fix, kept: set on every path, so after an early exit above (observer, game
	// over, dead, driving) a held button doesn't read as pressed again
	m_afButtonLast = pev->button;
}


// checks if the spot is clear of players
// orig 0x1f0970. RC: anything alive within 64 units blocks the spot (the SDK: a player within 128).
bool IsSpawnPointValid(CBaseEntity* pPlayer, CBaseEntity* pSpot)
{
	CBaseEntity* ent = NULL;

	if (!pSpot->IsTriggered(pPlayer))
	{
		return false;
	}

	while ((ent = UTIL_FindEntityInSphere(ent, pSpot->pev->origin, 64)) != NULL)
	{
		if (ent->IsAlive() && ent != pPlayer)
			return false;
	}

	return true;
}

// orig 0x1f09f0. A random free spot of szClassname (with bTeam, one whose pev->button is
// pPlayer's team number); with none free, a random occupied one and bForced set. NULL when the
// map has none.
CBaseEntity* PickRandomSpawnOfClassname(const char* szClassname, CBaseEntity* pPlayer, bool bTeam, bool& bForced)
{
	CBaseEntity* rgpSpots[64];
	int iCount = 0;
	CBaseEntity* pSpot = NULL;

	// the original caps the count at 63, so a 64th and later spot keeps overwriting the last slot
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, szClassname)) != NULL)
	{
		if (FNullEnt(pSpot->pev->pContainingEntity))
			continue;
		if (!IsSpawnPointValid(pPlayer, pSpot))
			continue;
		if (bTeam && pSpot->pev->button != g_pGameRules->GetTeamIndex(((CBasePlayer*)pPlayer)->m_szTeamName) + 1)
			continue;

		rgpSpots[iCount++] = pSpot;
		if (iCount > 63)
			iCount = 63;
	}

	if (iCount < 1)
	{
		iCount = 0;
		pSpot = NULL;
		while ((pSpot = UTIL_FindEntityByClassname(pSpot, szClassname)) != NULL)
		{
			if (FNullEnt(pSpot->pev->pContainingEntity))
				continue;
			if (bTeam && pSpot->pev->button != g_pGameRules->GetTeamIndex(((CBasePlayer*)pPlayer)->m_szTeamName) + 1)
				continue;

			rgpSpots[iCount++] = pSpot;
			if (iCount > 63)
				iCount = 63;
		}

		if (iCount < 1)
			return NULL;

		bForced = true;
	}
	else
		bForced = false;

	return rgpSpots[RANDOM_LONG(0, iCount - 1)];
}

// orig 0x1f0c10. Picks a spot (PickRandomSpawnOfClassname); on an occupied one, everything alive
// within 64 units is killed: 9999 damage from pPlayer, and again from the world if it survives.
edict_t* DoEntSpawnPointStuff(const char* szClassname, CBaseEntity* pPlayer, bool bTeam)
{
	bool bForced = false;
	CBaseEntity* pSpot = PickRandomSpawnOfClassname(szClassname, pPlayer, bTeam, bForced);

	if (!pSpot)
		return NULL;

	if (bForced)
	{
		CBaseEntity* ent = NULL;
		edict_t* pWorld = INDEXENT(0);

		// VERIFY: pPlayer itself is alive here if it stands within 64 units of the spot
		while ((ent = UTIL_FindEntityInSphere(ent, pSpot->pev->origin, 64)) != NULL)
		{
			if (!ent->IsAlive())
				continue;

			ent->TakeDamage(pPlayer->pev, pPlayer->pev, 9999, DMG_GENERIC);
			if (pWorld && !FNullEnt(ent->pev) && ent->IsAlive())
				ent->TakeDamage(VARS(pWorld), VARS(pWorld), 9999, DMG_GENERIC);
		}
	}

	return pSpot->pev->pContainingEntity;
}

/*
============
EntSelectSpawnPoint

Returns the entity to spawn at
============
*/
// orig 0x1f0d70. RC: random spots through DoEntSpawnPointStuff, info_teamspawn in teamplay, and
// as a last resort the map's scientists, grunts or barneys. No g_pLastSpawn, no startspot.
edict_t* EntSelectSpawnPoint(CBaseEntity* pPlayer)
{
	const char* szClassname;
	edict_t* pSpot;

	if (g_pGameRules->IsCoOp())
	{
		pSpot = DoEntSpawnPointStuff("info_player_coop", pPlayer, false);
		if (pSpot)
			return pSpot;
		szClassname = "info_player_start";
	}
	else if (g_pGameRules->IsTeamplay())
	{
		pSpot = DoEntSpawnPointStuff("info_teamspawn", pPlayer, true);
		if (pSpot)
			return pSpot;
		szClassname = "info_player_deathmatch";
	}
	else if (g_pGameRules->IsDeathmatch())
		szClassname = "info_player_deathmatch";
	else
		szClassname = NULL;

	if (szClassname)
	{
		pSpot = DoEntSpawnPointStuff(szClassname, pPlayer, false);
		if (pSpot)
			return pSpot;
	}

	CBaseEntity* pStart = UTIL_FindEntityByClassname(NULL, "info_player_start");
	if (pStart && !FNullEnt(pStart->pev) && IsSpawnPointValid(pPlayer, pStart))
		return pStart->pev->pContainingEntity;

	if ((pSpot = DoEntSpawnPointStuff("info_player_start", pPlayer, false)) != NULL)
		return pSpot;
	if ((pSpot = DoEntSpawnPointStuff("info_player_coop", pPlayer, false)) != NULL)
		return pSpot;
	if ((pSpot = DoEntSpawnPointStuff("info_player_deathmatch", pPlayer, false)) != NULL)
		return pSpot;
	if ((pSpot = DoEntSpawnPointStuff("info_teamspawn", pPlayer, true)) != NULL)
		return pSpot;
	if ((pSpot = DoEntSpawnPointStuff("info_teamspawn", pPlayer, false)) != NULL)
		return pSpot;
	if ((pSpot = DoEntSpawnPointStuff("monster_scientist", pPlayer, false)) != NULL)
		return pSpot;
	if ((pSpot = DoEntSpawnPointStuff("monster_human_grunt", pPlayer, false)) != NULL)
		return pSpot;
	if ((pSpot = DoEntSpawnPointStuff("monster_barney", pPlayer, false)) != NULL)
		return pSpot;

	ALERT(at_error, "No spawnpoints on level\n");
	return INDEXENT(0);
}

// orig 0x1f1000. RC: precaches first, reads the client's extragibs, bloodscale and
// driveentalpha userinfo, forces the client's move speeds and slot 1/2 binds, puts a new player
// into the pregame observer (sv_pregameobserver 1, multiplayer, once per connection), takes the
// start health, armor, max health and long jump from rc.ini, resets the drive, stomp and curse
// state, starts the regeneration timers, gives the active weapon a 0.5 s delay, and sets up the
// dying camera.
void CBasePlayer::Spawn()
{
	m_flStartCharge = gpGlobals->time;
	m_bIsSpawning = true;

	//Make sure this gets reset even if somebody adds an early return or throws an exception.
	const CallOnDestroy resetIsSpawning{[this]()
		{
			//Done spawning; reset.
			m_bIsSpawning = false;
		}};

	Precache();

	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "slj", "0");
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "hl", "1");
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "bj", UTIL_dtos1(sv_allowbunnyhopping.value != 0 ? 1 : 0));

	m_iGibExtra = atoi(g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(edict()), "extragibs"));
	m_flBloodScale = atof(g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(edict()), "bloodscale"));
	m_fDrvEntAlpha = atof(g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(edict()), "driveentalpha"));

	if (m_iGibExtra > 255)
		m_iGibExtra = 255;
	if (m_iGibExtra < 0)
		m_iGibExtra = 0;
	if (m_fDrvEntAlpha > 1)
		m_fDrvEntAlpha = 1;
	if (m_fDrvEntAlpha < 0)
		m_fDrvEntAlpha = 0;

	CLIENT_COMMAND(edict(), "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");
	CLIENT_COMMAND(edict(), "bind \"1\" \"slot1\"\nbind \"2\" \"slot2\"\n");

	g_pGameRules->SetDefaultPlayerTeam(this);
	m_fInitHUD = true;

	// a player who just joined watches first
	if (CVAR_GET_FLOAT("sv_pregameobserver") == 1 && !m_blIsObserver && !m_blWasObserver && g_pGameRules->IsMultiplayer())
	{
		m_blWasObserver = true;
		m_blIsObserver = true;
		m_blObserverRespawn = true;
		SET_VIEW(edict(), edict());
		InitObserver(this);
		return;
	}

	// The original sets pev->flags to FL_CLIENT; halflife-updated keeps the engine's FL_PROXY and FL_FAKECLIENT.
	pev->flags &= FL_PROXY | FL_FAKECLIENT;
	pev->flags |= FL_CLIENT;
	pev->classname = MAKE_STRING("player");
	pev->health = PREFS_KeyLoopUpFloat("PLAYER", "StartHealth", 100);
	pev->armorvalue = PREFS_KeyLoopUpInt("PLAYER", "StartArmor", 0);
	pev->takedamage = DAMAGE_AIM;
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_WALK;
	pev->max_health = PREFS_KeyLoopUpFloat("PLAYER", "MaxHealth", pev->health);
	pev->air_finished = gpGlobals->time + 12;
	pev->dmg = 2; // initial water damage
	pev->effects = 0;
	pev->deadflag = DEAD_NO;
	pev->dmg_take = 0;
	pev->dmg_save = 0;
	pev->friction = 1.0;
	pev->gravity = 1.0;
	pev->rendermode = kRenderNormal;
	pev->renderamt = 255;
	m_bitsHUDDamage = -1;
	m_bitsDamageType = 0;
	m_afPhysicsFlags = 0;
	pev->iuser1 = 0;
	pev->iuser4 = 0;
	m_fLongJump = PREFS_KeyLoopUpBool("PLAYER", "StartWithlongJump", false);

	m_fCurseMSGTime = gpGlobals->time;
	m_flNextChatTime = gpGlobals->time; // VERIFY: the original's write-only member at +0x688
	pev->euser3 = NULL;
	pev->iuser3 = 0;
	pev->euser2 = NULL;
	m_pLastItem = NULL;
	m_iClientHideHUD = -1; // force this to be recalculated
	m_iHideHUD = 0;
	m_fWeapon = false;
	m_pClientActiveItem = NULL;
	m_iClientBattery = -1;
	m_blInButStomp = false;
	m_blOnTractor = false;
	m_pCurDriveEnt = NULL;
	m_bInDriveEnt = false;
	m_fDrcEntTransTime = gpGlobals->time;
	m_flFallVelocity = 0; // dont let uninitialized value here hurt the player
	pev->velocity.z = 0;
	m_bitsDamageType = 0;
	m_afPhysicsFlags = 0;
	m_blIsObserver = false;
	m_blObserverRespawn = false;
	m_blWasObserver = true;
	m_fCurseRefireScale = 1;
	m_fCurseDmgMirror = 0;
	m_fCurseDmgScale = 1;

	if (PREFS_KeyLoopUpFloat("PLAYER", "HealthRegenTime", -1) > 0)
		m_fHealthRegenTime = PREFS_KeyLoopUpFloat("PLAYER", "HealthRegenTime", -1) + gpGlobals->time;
	else
		m_fHealthRegenTime = -1;

	if (PREFS_KeyLoopUpFloat("PLAYER", "ArmorRegenTime", -1) > 0)
		m_fArmorRegenTime = PREFS_KeyLoopUpFloat("PLAYER", "ArmorRegenTime", -1) + gpGlobals->time;
	else
		m_fArmorRegenTime = -1;

	pev->velocity = g_vecZero;
	m_iFOV = 0;		   // init field of view.
	m_iClientFOV = -1; // make sure fov reset is sent
	m_iClientHealth = -1;
	m_ClientSndRoomtype = -1;

	m_flNextDecalTime = 0; // let this player decal as soon as he spawns.

	m_flgeigerDelay = gpGlobals->time + 2.0; // wait a few seconds until user-defined message registrations
											 // are recieved by all clients

	m_flTimeStepSound = 0;
	m_iStepLeft = 0;
	m_flFieldOfView = 0.5; // some monsters use this to determine whether or not the player is looking at them.

	m_bloodColor = BLOOD_COLOR_RED;
	m_flNextAttack = UTIL_WeaponTimeBase();
	StartSneaking();

	g_pGameRules->GetPlayerSpawnSpot(this);

	m_fNoPlayerSound = false; // normal sound behavior.
	m_iFlashBattery = 99;
	m_flFlashLightTime = 1; // force first message

	SET_MODEL(ENT(pev), "models/player.mdl");
	g_ulModelIndexPlayer = pev->modelindex;
	pev->sequence = LookupActivity(ACT_IDLE);

	if (FBitSet(pev->flags, FL_DUCKING))
		UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	else
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);

	pev->view_ofs = VEC_VIEW;
	m_HackedGunPos = Vector(0, 32, 0);
	m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if (m_iPlayerSound == SOUNDLIST_EMPTY)
	{
		ALERT(at_console, "Couldn't alloc player sound slot!\n");
	}

	// reset all ammo values to 0; the original marks the client's copies unknown (-1, the SDK 0)
	for (int i = 0; i < MAX_AMMO_SLOTS; i++)
	{
		m_rgAmmo[i] = 0;
		m_rgAmmoLast[i] = -1;
	}

	m_lastx = m_lasty = 0;

	g_pGameRules->PlayerSpawn(this);

	for (int i = 0; i < MAX_AMMO_SLOTS; i++)
		m_rgAmmoLast[i] = -1;

	// no shooting for half a second after spawning
	m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	if (m_pActiveItem)
	{
		CBasePlayerWeapon* pWeapon = m_pActiveItem->GetWeaponPtr();
		if (pWeapon) // VERIFY: the original treats every item as a weapon here
		{
			pWeapon->m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
			pWeapon->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
			pWeapon->m_iClientClip = -1;
		}
		m_pActiveItem->UpdateClientData(this);
	}

	UpdateNVN();
	m_flNVNUpdateTime = gpGlobals->time + 0.5;

	if (IsBot())
		return;

	SET_VIEW(edict(), edict());
	UpdateClientData();

	if (!m_pCam || !m_blCamInited || !m_pCam->pev->pContainingEntity->pvPrivateData)
	{
		m_pCam = GetClassPtr((CBaseEntity*)NULL);
		m_pCam->pev->movetype = MOVETYPE_NONE;
		m_pCam->pev->solid = SOLID_NOT;
		SET_MODEL(m_pCam->edict(), "models/w_isotopebox.mdl");
		m_pCam->pev->classname = MAKE_STRING("dying_cam");
		m_pCam->pev->owner = edict();
		UTIL_SetOrigin(m_pCam->pev, m_pCam->pev->origin);
		UTIL_SetSize(m_pCam->pev, g_vecZero, g_vecZero);
		m_pCam->pev->rendermode = kRenderTransColor;
		m_pCam->pev->renderamt = 0;
		m_blCamInited = true;
	}

	m_blSetView = false;
	m_flIDTime = gpGlobals->time;
}


void CBasePlayer::Precache()
{
	// SOUNDS / MODELS ARE PRECACHED in ClientPrecache() (game specific)
	// because they need to precache before any clients have connected

	// in the event that the player JUST spawned, and the level node graph
	// was loaded, fix all of the node graph pointers before the game starts.
	// (orig 0x1f1d00: here, as in the old SDK; halflife-updated moved it to PreThink)

	// !!!BUGBUG - now that we have multiplayer, this needs to be moved!
	if (0 != WorldGraph.m_fGraphPresent && 0 == WorldGraph.m_fGraphPointersSet)
	{
		if (!WorldGraph.FSetGraphPointers())
		{
			ALERT(at_console, "**Graph pointers were not set!\n");
		}
		else
		{
			ALERT(at_console, "**Graph Pointers Set!\n");
		}
	}

	// init geiger counter vars during spawn and each time
	// we cross a level transition

	m_flgeigerRange = 1000;
	m_igeigerRangePrev = 1000;

	m_bitsDamageType = 0;
	m_bitsHUDDamage = -1;

	m_iClientBattery = -1;

	m_iTrain = TRAIN_NEW; // the original assigns; halflife-updated ORs

	// Make sure any necessary user messages have been registered
	LinkUserMessages();

	m_iUpdateTime = 5; // won't update for 1/2 a second

	if (gInitHUD)
		m_fInitHUD = true;
}


bool CBasePlayer::Save(CSave& save)
{
	if (!CBaseMonster::Save(save))
		return false;

	return save.WriteFields("PLAYER", this, m_playerSaveData, ARRAYSIZE(m_playerSaveData));
}


//
// Marks everything as new so the player will resend this to the hud.
//
void CBasePlayer::RenewItems()
{
}


bool CBasePlayer::Restore(CRestore& restore)
{
	if (!CBaseMonster::Restore(restore))
		return false;

	bool status = restore.ReadFields("PLAYER", this, m_playerSaveData, ARRAYSIZE(m_playerSaveData));

	SAVERESTOREDATA* pSaveData = (SAVERESTOREDATA*)gpGlobals->pSaveData;
	// landmark isn't present.
	if (0 == pSaveData->fUseLandmark)
	{
		ALERT(at_console, "No Landmark:%s\n", pSaveData->szLandmarkName);

		// default to normal spawn
		edict_t* pentSpawnSpot = EntSelectSpawnPoint(this);
		pev->origin = VARS(pentSpawnSpot)->origin + Vector(0, 0, 1);
		pev->angles = VARS(pentSpawnSpot)->angles;
	}
	pev->v_angle.z = 0; // Clear out roll
	pev->angles = pev->v_angle;

	pev->fixangle = 1; // turn this way immediately

	m_iClientFOV = -1; // Make sure the client gets the right FOV value.
	m_ClientSndRoomtype = -1;

	// Reset room type on level change.
	if (!FStrEq(restore.GetData().szCurrentMapName, STRING(gpGlobals->mapname)))
	{
		m_SndRoomtype = 0;
	}

	// Copied from spawn() for now
	m_bloodColor = BLOOD_COLOR_RED;

	g_ulModelIndexPlayer = pev->modelindex;

	if (FBitSet(pev->flags, FL_DUCKING))
	{
		// Use the crouch HACK
		//FixPlayerCrouchStuck( edict() );
		// Don't need to do this with new player prediction code.
		UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	}
	else
	{
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
	}

	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "hl", "1");

	if (m_fLongJump)
	{
		g_engfuncs.pfnSetPhysicsKeyValue(edict(), "slj", "1");
	}
	else
	{
		g_engfuncs.pfnSetPhysicsKeyValue(edict(), "slj", "0");
	}

	RenewItems();

	TabulateAmmo();

#if defined(CLIENT_WEAPONS)
	// HACK:	This variable is saved/restored in CBaseMonster as a time variable, but we're using it
	//			as just a counter.  Ideally, this needs its own variable that's saved as a plain float.
	//			Barring that, we clear it out here instead of using the incorrect restored time value.
	m_flNextAttack = UTIL_WeaponTimeBase();
#endif

	m_bResetViewEntity = true;

	m_bRestored = true;

	return status;
}



void CBasePlayer::SelectNextItem(int iItem)
{
	CBasePlayerItem* pItem;

	pItem = m_rgpPlayerItems[iItem];

	if (!pItem)
		return;

	if (pItem == m_pActiveItem)
	{
		// select the next one in the chain
		pItem = m_pActiveItem->m_pNext;
		if (!pItem)
		{
			return;
		}

		CBasePlayerItem* pLast;
		pLast = pItem;
		while (pLast->m_pNext)
			pLast = pLast->m_pNext;

		// relink chain
		pLast->m_pNext = m_pActiveItem;
		m_pActiveItem->m_pNext = NULL;
		m_rgpPlayerItems[iItem] = pItem;
	}

	ResetAutoaim();

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem)
	{
		m_pActiveItem->Holster();
	}

	m_pActiveItem = pItem;

	if (m_pActiveItem)
	{
		m_pActiveItem->Deploy();
		m_pActiveItem->UpdateItemInfo();
	}
}

void CBasePlayer::SelectItem(const char* pstr)
{
	if (!pstr)
		return;

	CBasePlayerItem* pItem = NULL;

	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			pItem = m_rgpPlayerItems[i];

			while (pItem)
			{
				if (FClassnameIs(pItem->pev, pstr))
					break;
				pItem = pItem->m_pNext;
			}
		}

		if (pItem)
			break;
	}

	if (!pItem)
		return;


	if (pItem == m_pActiveItem)
		return;

	ResetAutoaim();

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem)
		m_pActiveItem->Holster();

	m_pLastItem = m_pActiveItem;
	m_pActiveItem = pItem;

	if (m_pActiveItem)
	{
		m_pActiveItem->m_ForceSendAnimations = true;
		m_pActiveItem->Deploy();
		m_pActiveItem->m_ForceSendAnimations = false;
		m_pActiveItem->UpdateItemInfo();
	}
}

//==============================================
// HasWeapons - do I have any weapons at all?
//==============================================
bool CBasePlayer::HasWeapons()
{
	int i;

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			return true;
		}
	}

	return false;
}

void CBasePlayer::SelectPrevItem(int iItem)
{
}


const char* CBasePlayer::TeamID()
{
	if (pev == NULL) // Not fully connected yet
		return "";

	// return their team name
	return m_szTeamName;
}


//==============================================
// !!!UNDONE:ultra temporary SprayCan entity to apply
// decal frame at a time. For PreAlpha CD
//==============================================
class CSprayCan : public CBaseEntity
{
public:
	void Spawn(entvars_t* pevOwner);
	void Think() override;

	int ObjectCaps() override { return FCAP_DONT_SAVE; }
};

void CSprayCan::Spawn(entvars_t* pevOwner)
{
	pev->origin = pevOwner->origin + Vector(0, 0, 32);
	pev->angles = pevOwner->v_angle;
	pev->owner = ENT(pevOwner);
	pev->frame = 0;

	pev->nextthink = gpGlobals->time + 0.1;
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/sprayer.wav", 1, ATTN_NORM);
}

void CSprayCan::Think()
{
	TraceResult tr;
	int playernum;
	int nFrames;
	CBasePlayer* pPlayer;

	pPlayer = (CBasePlayer*)GET_PRIVATE(pev->owner);

	if (pPlayer)
		nFrames = pPlayer->GetCustomDecalFrames();
	else
		nFrames = -1;

	playernum = ENTINDEX(pev->owner);

	// ALERT(at_console, "Spray by player %i, %i of %i\n", playernum, (int)(pev->frame + 1), nFrames);

	UTIL_MakeVectors(pev->angles);
	UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, &tr);

	// No customization present.
	if (nFrames == -1)
	{
		UTIL_DecalTrace(&tr, DECAL_LAMBDA6);
		UTIL_Remove(this);
	}
	else
	{
		UTIL_PlayerDecalTrace(&tr, playernum, pev->frame, true);
		// Just painted last custom frame.
		if (pev->frame++ >= (nFrames - 1))
			UTIL_Remove(this);
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

class CBloodSplat : public CBaseEntity
{
public:
	void Spawn(entvars_t* pevOwner);
	void Spray();
};

void CBloodSplat::Spawn(entvars_t* pevOwner)
{
	pev->origin = pevOwner->origin + Vector(0, 0, 32);
	pev->angles = pevOwner->v_angle;
	pev->owner = ENT(pevOwner);

	SetThink(&CBloodSplat::Spray);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CBloodSplat::Spray()
{
	TraceResult tr;

	if (g_Language != LANGUAGE_GERMAN)
	{
		UTIL_MakeVectors(pev->angles);
		UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, &tr);

		UTIL_BloodDecalTrace(&tr, BLOOD_COLOR_RED);
	}
	SetThink(&CBloodSplat::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
}

//==============================================

// orig 0x1f2720 (GiveNamedItem). RC: refuses a classname nothing has spawned yet or that isn't
// an entity, and interns the classname in Create's table (cbase.cpp) instead of MAKE_STRING.
static edict_t* GiveNamedItem_Common(entvars_t* pev, const char* pszName)
{
	if (!IsClassnamePrecached(const_cast<char*>(pszName)))
	{
		ALERT(at_console, "Trying to GiveNamedItem %s, not precached!\n", pszName);
		return nullptr;
	}

	if (!IsValidEnitityClassname(const_cast<char*>(pszName)))
	{
		ALERT(at_console, "NULL Ent, %s, in GiveNamedItem!\n", pszName);
		return nullptr;
	}

	int i;
	for (i = 0; i < g_iCntFakeClassnames; i++)
	{
		if (!stricmp(g_pFakeConstClassnames[i], pszName))
			break;
	}

	int istr;
	if (i < g_iCntFakeClassnames)
		istr = MAKE_STRING(g_pFakeConstClassnames[i]);
	else if (g_iCntFakeClassnames < 256)
	{
		strcpy(g_pFakeConstClassnames[g_iCntFakeClassnames], pszName);
		istr = MAKE_STRING(g_pFakeConstClassnames[g_iCntFakeClassnames]);
		g_iCntFakeClassnames++;
	}
	else
		istr = MAKE_STRING(pszName);

	edict_t* pent = CREATE_NAMED_ENTITY(istr);
	if (FNullEnt(pent))
	{
		ALERT(at_console, "NULL Ent, %s, in GiveNamedItem!\n", pszName);
		return nullptr;
	}
	VARS(pent)->origin = pev->origin;
	pent->v.spawnflags |= SF_NORESPAWN;

	DispatchSpawn(pent);

	return pent;
}

void CBasePlayer::GiveNamedItem(const char* szName)
{
	auto pent = GiveNamedItem_Common(pev, szName);

	if (!pent)
	{
		return;
	}

	DispatchTouch(pent, ENT(pev));
}

void CBasePlayer::GiveNamedItem(const char* szName, int defaultAmmo)
{
	auto pent = GiveNamedItem_Common(pev, szName);

	if (!pent)
	{
		return;
	}

	auto entity = CBaseEntity::Instance(pent);

	if (!entity)
	{
		return;
	}

	if (auto weapon = dynamic_cast<CBasePlayerWeapon*>(entity); weapon)
	{
		weapon->m_iDefaultAmmo = defaultAmmo;
	}

	DispatchTouch(pent, ENT(pev));
}

bool CBasePlayer::FlashlightIsOn()
{
	return FBitSet(pev->effects, EF_DIMLIGHT);
}


void CBasePlayer::FlashlightTurnOn()
{
	if (!g_pGameRules->FAllowFlashlight())
	{
		return;
	}

	if (HasSuit())
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, SOUND_FLASHLIGHT_ON, 1.0, ATTN_NORM, 0, PITCH_NORM);
		SetBits(pev->effects, EF_DIMLIGHT);
		MESSAGE_BEGIN(MSG_ONE, gmsgFlashlight, NULL, pev);
		WRITE_BYTE(1);
		WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();

		m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
	}
}


void CBasePlayer::FlashlightTurnOff()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, SOUND_FLASHLIGHT_OFF, 1.0, ATTN_NORM, 0, PITCH_NORM);
	ClearBits(pev->effects, EF_DIMLIGHT);
	MESSAGE_BEGIN(MSG_ONE, gmsgFlashlight, NULL, pev);
	WRITE_BYTE(0);
	WRITE_BYTE(m_iFlashBattery);
	MESSAGE_END();

	m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;
}

/*
===============
ForceClientDllUpdate

When recording a demo, we need to have the server tell us the entire client state
so that the client side .dll can behave correctly.
Reset stuff so that the state is transmitted.
===============
*/
void CBasePlayer::ForceClientDllUpdate()
{
	m_iClientHealth = -1;
	m_iClientBattery = -1;
	m_iClientHideHUD = -1;
	m_iClientFOV = -1;
	m_ClientWeaponBits = 0;
	m_ClientSndRoomtype = -1;

	for (int i = 0; i < MAX_AMMO_SLOTS; ++i)
	{
		m_rgAmmoLast[i] = 0;
	}

	m_iTrain |= TRAIN_NEW; // Force new train message.
	m_fWeapon = false;	   // Force weapon send
	m_fKnownItem = false;  // Force weaponinit messages.
	m_fInitHUD = true;	   // Force HUD gmsgResetHUD message

	// Now force all the necessary messages
	//  to be sent.
	UpdateClientData();
}

/*
============
ImpulseCommands
============
*/
void CBasePlayer::ImpulseCommands()
{
	TraceResult tr; // UNDONE: kill me! This is temporary for PreAlpha CDs

	// Handle use events
	PlayerUse();

	int iImpulse = (int)pev->impulse;
	switch (iImpulse)
	{
	case 99:
	{

		bool iOn;

		if (0 == gmsgLogo)
		{
			iOn = true;
			gmsgLogo = REG_USER_MSG("Logo", 1);
		}
		else
		{
			iOn = false;
		}

		ASSERT(gmsgLogo > 0);
		// send "health" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgLogo, NULL, pev);
		WRITE_BYTE(static_cast<int>(iOn));
		MESSAGE_END();

		if (!iOn)
			gmsgLogo = 0;
		break;
	}
	case 100:
		// temporary flashlight for level designers
		if (FlashlightIsOn())
		{
			FlashlightTurnOff();
		}
		else
		{
			FlashlightTurnOn();
		}
		break;

	case 201: // paint decal

		if (gpGlobals->time < m_flNextDecalTime)
		{
			// too early!
			break;
		}

		UTIL_MakeVectors(pev->v_angle);
		UTIL_TraceLine(pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, ENT(pev), &tr);

		if (tr.flFraction != 1.0)
		{ // line hit something, so paint a decal
			m_flNextDecalTime = gpGlobals->time + decalfrequency.value;
			CSprayCan* pCan = GetClassPtr((CSprayCan*)NULL);
			pCan->Spawn(pev);
		}

		break;

	// RC 1.9's (as SDK 2.1); halflife-updated dropped impulse 204 (it has the fullupdate command)
	case 204: //  Demo recording, update client dll specific data again.
		ForceClientDllUpdate();
		break;

	default:
		// check all of the cheat impulse commands now
		CheatImpulseCommands(iImpulse);
		break;
	}

	pev->impulse = 0;
}

// orig 0x1f28d0. What the player is looking at, up to 8192 units (the SDK's static
// FindEntityForward, a global in the original; nothing in the original calls it any more).
CBaseEntity* FindEntityForward(CBaseEntity* pMe)
{
	TraceResult tr;

	UTIL_MakeVectors(pMe->pev->v_angle);
	UTIL_TraceLine(pMe->pev->origin + pMe->pev->view_ofs, pMe->pev->origin + pMe->pev->view_ofs + gpGlobals->v_forward * 8192, dont_ignore_monsters, pMe->edict(), &tr);
	if (tr.flFraction != 1.0 && !FNullEnt(tr.pHit))
	{
		CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
		return pHit;
	}
	return NULL;
}

// orig 0x1f2f10. Cheat transforms are allowed outside multiplayer, and in it with sv_allowtransform 1.
bool AllowToTransform(edict_t* pPlayer)
{
	if (g_pGameRules->IsMultiplayer() && CVAR_GET_FLOAT("sv_allowtransform") != 1)
		return false;

	return true;
}

// Puts pPlayer into a new drive monster it just created (the transform cheats).
static void EnterNewDriveEnt(CBasePlayer* pPlayer)
{
	pPlayer->m_pCurDriveEnt->pev->angles = Vector(0, pPlayer->pev->v_angle.y, 0);
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;
}

//=========================================================
//=========================================================
// orig 0x1f2f70. RC replaced the SDK's debug impulses: 33-46 turn the player into a drive
// monster (sv_allowtransform; the bullsquid and chumtoad only for the listen server's host),
// 101 is stock, 111 gives all ammo and 112 the throwables. The others (76, 102-107, 195-199,
// 202) are gone.
void CBasePlayer::CheatImpulseCommands(int iImpulse)
{
	if (0 == g_psv_cheats->value)
	{
		return;
	}

	const char* pszDrive = NULL;
	bool bUnsafe = false; // the creates that may spawn a never-spawned classname, and check for NULL

	switch (iImpulse)
	{
	case 33:
		if (!AllowToTransform(edict()) || m_bInDriveEnt)
			return;
		// VERIFY: the original doesn't check Create's result for 33-39 (crashes if it fails)
		m_pCurDriveEnt = (CBaseDriveEnt*)Create("drive_apache", Center() + Vector(0, 0, 256), Vector(0, pev->v_angle.y, 0));
		EnterNewDriveEnt(this);
		return;

	case 34:
		pszDrive = "drive_garg";
		break;
	case 35:
		pszDrive = "drive_assassin";
		break;

	case 36:
	{
		if (!AllowToTransform(edict()) || m_bInDriveEnt)
			return;

		// the turret goes on the floor or ceiling the player looks at
		TraceResult tr;
		UTIL_MakeVectors(pev->v_angle);
		Vector vecSrc = pev->origin + pev->view_ofs;
		UTIL_TraceLine(vecSrc, vecSrc + gpGlobals->v_forward * 1024, ignore_monsters, edict(), &tr);
		if (tr.flFraction >= 1.0)
			return;

		if (tr.vecPlaneNormal != Vector(0, 0, 1) && tr.vecPlaneNormal != Vector(0, 0, -1))
			return;

		m_pCurDriveEnt = (CBaseDriveEnt*)Create("drive_turret", tr.vecEndPos + tr.vecPlaneNormal, tr.vecPlaneNormal);
		if (tr.vecPlaneNormal == Vector(0, 0, 1))
			m_pCurDriveEnt->pev->angles.y = pev->v_angle.y;
		else
			m_pCurDriveEnt->pev->angles.y = UTIL_AngleMod(180 - pev->v_angle.y);

		m_pCurDriveEnt->DEClientEnter(this);
		m_bInDriveEnt = true;
		return;
	}

	case 37:
		pszDrive = "drive_icky";
		break;
	case 38:
		pszDrive = "drive_houndeye";
		break;
	case 39:
		pszDrive = "drive_asl";
		break;

	case 40:
		pszDrive = "drive_panth";
		bUnsafe = true;
		break;
	case 41:
		pszDrive = "drive_bm";
		bUnsafe = true;
		break;
	case 42:
		pszDrive = "drive_headcrab";
		bUnsafe = true;
		break;
	case 43:
		pszDrive = "drive_agrunt";
		bUnsafe = true;
		break;
	case 44:
		pszDrive = "drive_controller";
		bUnsafe = true;
		break;

	case 45:
	case 46:
		if (!IsClientListenServer(edict()) || m_bInDriveEnt)
			return;

		g_bUnsafeCreate = true;
		m_pCurDriveEnt = (CBaseDriveEnt*)Create(iImpulse == 45 ? "drive_bullsquid" : "drive_chumtoad", Center(), Vector(0, pev->v_angle.y, 0));
		g_bUnsafeCreate = false;
		if (!m_pCurDriveEnt)
			return;
		EnterNewDriveEnt(this);
		return;

	case 101:
		gEvilImpulse101 = true;
		GiveNamedItem("item_suit");
		GiveNamedItem("item_battery");
		GiveNamedItem("weapon_crowbar");
		GiveNamedItem("weapon_9mmhandgun");
		GiveNamedItem("ammo_9mmclip");
		GiveNamedItem("weapon_shotgun");
		GiveNamedItem("ammo_buckshot");
		GiveNamedItem("weapon_9mmAR");
		GiveNamedItem("ammo_9mmAR");
		GiveNamedItem("ammo_ARgrenades");
		GiveNamedItem("weapon_handgrenade");
		GiveNamedItem("weapon_tripmine");
		GiveNamedItem("weapon_357");
		GiveNamedItem("ammo_357");
		GiveNamedItem("weapon_crossbow");
		GiveNamedItem("ammo_crossbow");
		GiveNamedItem("weapon_egon");
		GiveNamedItem("weapon_gauss");
		GiveNamedItem("ammo_gaussclip");
		GiveNamedItem("weapon_rpg");
		GiveNamedItem("ammo_rpgclip");
		GiveNamedItem("weapon_satchel");
		GiveNamedItem("weapon_snark");
		GiveNamedItem("weapon_hornetgun");
		gEvilImpulse101 = false;
		return;

	case 111:
		gEvilImpulse101 = true;
		GiveNamedItem("ammo_9mmclip");
		GiveNamedItem("ammo_buckshot");
		GiveNamedItem("ammo_9mmAR");
		GiveNamedItem("ammo_ARgrenades");
		GiveNamedItem("ammo_357");
		GiveNamedItem("ammo_crossbow");
		GiveNamedItem("ammo_gaussclip");
		GiveNamedItem("ammo_rpgclip");
		gEvilImpulse101 = false;
		return;

	case 112:
		gEvilImpulse101 = true;
		GiveNamedItem("weapon_handgrenade");
		GiveNamedItem("weapon_tripmine");
		GiveNamedItem("weapon_satchel");
		GiveNamedItem("weapon_snark");
		gEvilImpulse101 = false;
		return;

	default:
		return;
	}

	// 34, 35, 37-44: a drive monster at the player's center, facing his way
	if (!AllowToTransform(edict()) || m_bInDriveEnt)
		return;

	if (bUnsafe)
		g_bUnsafeCreate = true;

	m_pCurDriveEnt = (CBaseDriveEnt*)Create(pszDrive, Center(), Vector(0, pev->v_angle.y, 0));

	if (bUnsafe)
	{
		g_bUnsafeCreate = false;
		if (!m_pCurDriveEnt)
			return;
	}

	EnterNewDriveEnt(this);
}

//
// Add a weapon to the player (Item == Weapon == Selectable Object)
//
bool CBasePlayer::AddPlayerItem(CBasePlayerItem* pItem)
{
	CBasePlayerItem* pInsert;

	pInsert = m_rgpPlayerItems[pItem->iItemSlot()];

	while (pInsert)
	{
		if (FClassnameIs(pInsert->pev, STRING(pItem->pev->classname)))
		{
			if (pItem->AddDuplicate(pInsert))
			{
				g_pGameRules->PlayerGotWeapon(this, pItem);
				pItem->CheckRespawn();

				// ugly hack to update clip w/o an update clip message
				pInsert->UpdateItemInfo();
				if (m_pActiveItem)
					m_pActiveItem->UpdateItemInfo();

				pItem->Kill();
			}
			else if (gEvilImpulse101)
			{
				// FIXME: remove anyway for deathmatch testing
				pItem->Kill();
			}
			return false;
		}
		pInsert = pInsert->m_pNext;
	}


	if (pItem->CanAddToPlayer(this))
	{
		pItem->AddToPlayer(this);

		g_pGameRules->PlayerGotWeapon(this, pItem);
		pItem->CheckRespawn();

		// Add to the list before extracting ammo so weapon ownership checks work properly.
		pItem->m_pNext = m_rgpPlayerItems[pItem->iItemSlot()];
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem;

		if (auto weapon = pItem->GetWeaponPtr(); weapon)
		{
			weapon->ExtractAmmo(weapon);

			//Immediately update the ammo HUD so weapon pickup isn't sometimes red because the HUD doesn't know about regenerating/free ammo yet.
			if (-1 != weapon->m_iPrimaryAmmoType)
			{
				SendSingleAmmoUpdate(CBasePlayer::GetAmmoIndex(weapon->pszAmmo1()));
			}

			if (-1 != weapon->m_iSecondaryAmmoType)
			{
				SendSingleAmmoUpdate(CBasePlayer::GetAmmoIndex(weapon->pszAmmo2()));
			}

			// Rocket Crowbar 1.9: no WeapPickup here; the original sends it from eight weapons' own
			// AddToPlayer (crossbow, egon, gauss, hornetgun, mp5, python, rpg, shotgun), at spawn too.
		}

		// should we switch to this item?
		if (g_pGameRules->FShouldSwitchWeapon(this, pItem))
		{
			SwitchWeapon(pItem);
		}

		return true;
	}
	else if (gEvilImpulse101)
	{
		// FIXME: remove anyway for deathmatch testing
		pItem->Kill();
	}
	return false;
}



bool CBasePlayer::RemovePlayerItem(CBasePlayerItem* pItem)
{
	if (m_pActiveItem == pItem)
	{
		ResetAutoaim();
		pItem->Holster();
		pItem->pev->nextthink = 0; // crowbar may be trying to swing again, etc.
		pItem->SetThink(NULL);
		m_pActiveItem = NULL;
		pev->viewmodel = 0;
		pev->weaponmodel = 0;
	}
	if (m_pLastItem == pItem)
		m_pLastItem = NULL;

	CBasePlayerItem* pPrev = m_rgpPlayerItems[pItem->iItemSlot()];

	if (pPrev == pItem)
	{
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem->m_pNext;
		return true;
	}
	else
	{
		while (pPrev && pPrev->m_pNext != pItem)
		{
			pPrev = pPrev->m_pNext;
		}
		if (pPrev)
		{
			pPrev->m_pNext = pItem->m_pNext;
			return true;
		}
	}
	return false;
}


//
// Returns the unique ID for the ammo, or -1 if error
//
int CBasePlayer::GiveAmmo(int iCount, const char* szName, int iMax)
{
	// RC 1.9's own: no ammo given for a count under 1 (orig 0x1f4140)
	if (!szName || iCount < 1)
	{
		// no ammo.
		return -1;
	}

	if (!g_pGameRules->CanHaveAmmo(this, szName, iMax))
	{
		// game rules say I can't have any more of this ammo type.
		return -1;
	}

	int i = 0;

	i = GetAmmoIndex(szName);

	if (i < 0 || i >= MAX_AMMO_SLOTS)
		return -1;

	int iAdd = V_min(iCount, iMax - m_rgAmmo[i]);
	if (iAdd < 1)
		return i;

	// halflife-updated's fix, kept: ammo for an exhaustible weapon (grenades, satchels, ...) also
	// gives the weapon when the player lacks it (halflife-updated #153, #157)
	// If this is an exhaustible weapon make sure the player has it.
	if (const auto& ammoType = CBasePlayerItem::AmmoInfoArray[i]; ammoType.WeaponName != nullptr)
	{
		if (!HasNamedPlayerItem(ammoType.WeaponName))
		{
			GiveNamedItem(ammoType.WeaponName, 0);
		}
	}

	m_rgAmmo[i] += iAdd;


	if (0 != gmsgAmmoPickup) // make sure the ammo messages have been linked first
	{
		// Send the message that ammo has been picked up
		MESSAGE_BEGIN(MSG_ONE, gmsgAmmoPickup, NULL, pev);
		WRITE_BYTE(GetAmmoIndex(szName)); // ammo ID
		WRITE_BYTE(iAdd);				  // amount
		MESSAGE_END();
	}

	TabulateAmmo();

	return i;
}


/*
============
ItemPreFrame

Called every frame by the player PreThink
============
*/
void CBasePlayer::ItemPreFrame()
{
#if defined(CLIENT_WEAPONS)
	if (m_flNextAttack > 0)
#else
	if (gpGlobals->time < m_flNextAttack)
#endif
	{
		return;
	}

	if (!m_pActiveItem)
		return;

	m_pActiveItem->ItemPreFrame();
}


/*
============
ItemPostFrame

Called every frame by the player PostThink
============
*/
void CBasePlayer::ItemPostFrame()
{
	// check if the player is using a tank
	if (m_pTank != NULL)
		return;

#if defined(CLIENT_WEAPONS)
	if (m_flNextAttack > 0)
#else
	if (gpGlobals->time < m_flNextAttack)
#endif
	{
		return;
	}

	ImpulseCommands();

	// halflife-updated's fix, kept: no weapon frame on the frame PlayerUse started using a tank
	// (halflife #3345)
	// check again if the player is using a tank if they started using it in PlayerUse
	if (m_pTank != NULL)
		return;

	if (!m_pActiveItem)
		return;

	m_pActiveItem->ItemPostFrame();
}

int CBasePlayer::AmmoInventory(int iAmmoIndex)
{
	if (iAmmoIndex == -1)
	{
		return -1;
	}

	return m_rgAmmo[iAmmoIndex];
}

int CBasePlayer::GetAmmoIndex(const char* psz)
{
	int i;

	if (!psz)
		return -1;

	for (i = 1; i < MAX_AMMO_SLOTS; i++)
	{
		if (!CBasePlayerItem::AmmoInfoArray[i].pszName)
			continue;

		if (stricmp(psz, CBasePlayerItem::AmmoInfoArray[i].pszName) == 0)
			return i;
	}

	return -1;
}

// Called from UpdateClientData
// makes sure the client has all the necessary ammo info,  if values have changed
void CBasePlayer::SendAmmoUpdate()
{
	for (int i = 0; i < MAX_AMMO_SLOTS; i++)
	{
		InternalSendSingleAmmoUpdate(i);
	}
}

void CBasePlayer::SendSingleAmmoUpdate(int ammoIndex)
{
	if (ammoIndex < 0 || ammoIndex >= MAX_AMMO_SLOTS)
	{
		return;
	}

	InternalSendSingleAmmoUpdate(ammoIndex);
}

void CBasePlayer::InternalSendSingleAmmoUpdate(int ammoIndex)
{
	if (m_rgAmmo[ammoIndex] != m_rgAmmoLast[ammoIndex])
	{
		m_rgAmmoLast[ammoIndex] = m_rgAmmo[ammoIndex];

		ASSERT(m_rgAmmo[ammoIndex] >= 0);
		ASSERT(m_rgAmmo[ammoIndex] < 255);

		// send "Ammo" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgAmmoX, NULL, pev);
		WRITE_BYTE(ammoIndex);
		WRITE_BYTE(V_max(V_min(m_rgAmmo[ammoIndex], 254), 0)); // clamp the value to one byte
		MESSAGE_END();
	}
}

/*
=========================================================
	UpdateClientData

resends any changed player HUD info to the client.
Called every frame by PlayerPreThink
Also called at start of demo recording and playback by
ForceClientDllUpdate to ensure the demo gets messages
reflecting all of the HUD state info.
=========================================================
*/
// orig 0x1f44a0. RC: a new player gets the vote timer, the player ID timer (bots none) and
// the version banner (PrintRocketCrowbarMsg); an observer gets nothing past the HUD reset.
// The train HUD always gets 0. Not in the original: halflife-updated's status bar, its
// "no weapon" CurWeapon message and the Weapons bits message (kept, see the dossier).
void CBasePlayer::UpdateClientData()
{
	if (m_fInitHUD)
	{
		m_fInitHUD = false;
		gInitHUD = false;

		MESSAGE_BEGIN(MSG_ONE, gmsgResetHUD, NULL, pev);
		WRITE_BYTE(0);
		MESSAGE_END();

		if (!m_fGameHUDInitialized)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgInitHUD, NULL, pev);
			MESSAGE_END();

			g_pGameRules->InitHUD(this);
			m_fGameHUDInitialized = true;

			m_iObserverLastMode = OBS_ROAMING;

			if (g_pGameRules->IsMultiplayer())
			{
				FireTargets("game_playerjoin", this, this, USE_TOGGLE, 0);
			}

			m_flNextVoteTime = gpGlobals->time;

			if (m_flIDTime != -1)
				m_flIDTime = gpGlobals->time;

			if (!IsBot())
				PrintRocketCrowbarMsg(this);
			else
				m_flIDTime = -1; // bots don't ID

			m_fShotgunRiftTime = m_fIsoBombTime = gpGlobals->time;
		}

		FireTargets("game_playerspawn", this, this, USE_TOGGLE, 0);
	}

	if (m_blIsObserver)
		return;

	if (m_iHideHUD != m_iClientHideHUD)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, pev);
		WRITE_BYTE(m_iHideHUD);
		MESSAGE_END();

		m_iClientHideHUD = m_iHideHUD;
	}

	if (m_iFOV != m_iClientFOV)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, pev);
		WRITE_BYTE(m_iFOV);
		MESSAGE_END();

		// cache FOV change at end of function, so weapon updates can see that FOV has changed
	}

	// HACKHACK -- send the message to display the game title
	//TODO: will not work properly in multiplayer
	if (gDisplayTitle)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgShowGameTitle, NULL, pev);
		WRITE_BYTE(0);
		MESSAGE_END();
		gDisplayTitle = false;
	}

	if (pev->health != m_iClientHealth)
	{
		int iHealth = pev->health > 0 ? (int)pev->health : 0; // the original: no upper clamp, no 0-1 to 1

		// send "health" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, pev);
		WRITE_SHORT(iHealth); // VERIFY: a byte in the original (UserMessages.cpp registers Health as 2 bytes)
		MESSAGE_END();

		m_iClientHealth = pev->health;
	}


	if (pev->armorvalue != m_iClientBattery)
	{
		m_iClientBattery = pev->armorvalue;

		ASSERT(gmsgBattery > 0);
		// send "health" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgBattery, NULL, pev);
		WRITE_SHORT((int)pev->armorvalue);
		MESSAGE_END();
	}

	if (m_WeaponBits != m_ClientWeaponBits)
	{
		m_ClientWeaponBits = m_WeaponBits;

		const int lowerBits = m_WeaponBits & 0xFFFFFFFF;
		const int upperBits = (m_WeaponBits >> 32) & 0xFFFFFFFF;

		MESSAGE_BEGIN(MSG_ONE, gmsgWeapons, nullptr, pev);
		WRITE_LONG(lowerBits);
		WRITE_LONG(upperBits);
		MESSAGE_END();
	}

	if (0 != pev->dmg_take || 0 != pev->dmg_save || m_bitsHUDDamage != m_bitsDamageType)
	{
		// Comes from inside me if not set
		Vector damageOrigin = pev->origin;
		// send "damage" message
		// causes screen to flash, and pain compass to show direction of damage
		edict_t* other = pev->dmg_inflictor;
		if (other)
		{
			CBaseEntity* pEntity = CBaseEntity::Instance(other);
			if (pEntity)
				damageOrigin = pEntity->Center();
		}

		// only send down damage type that have hud art
		int visibleDamageBits = m_bitsDamageType & DMG_SHOWNHUD;

		MESSAGE_BEGIN(MSG_ONE, gmsgDamage, NULL, pev);
		WRITE_BYTE(pev->dmg_save);
		WRITE_BYTE(pev->dmg_take);
		WRITE_LONG(visibleDamageBits);
		WRITE_COORD(damageOrigin.x);
		WRITE_COORD(damageOrigin.y);
		WRITE_COORD(damageOrigin.z);
		MESSAGE_END();

		pev->dmg_take = 0;
		pev->dmg_save = 0;
		m_bitsHUDDamage = m_bitsDamageType;

		// Clear off non-time-based damage indicators
		m_bitsDamageType &= DMG_TIMEBASED;
	}

	if (m_bRestored)
	{
		//Always tell client about battery state
		MESSAGE_BEGIN(MSG_ONE, gmsgFlashBattery, NULL, pev);
		WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();

		//Tell client the flashlight is on
		if (FlashlightIsOn())
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgFlashlight, NULL, pev);
			WRITE_BYTE(1);
			WRITE_BYTE(m_iFlashBattery);
			MESSAGE_END();
		}
	}

	// Update Flashlight
	if ((0 != m_flFlashLightTime) && (m_flFlashLightTime <= gpGlobals->time))
	{
		if (FlashlightIsOn())
		{
			if (0 != m_iFlashBattery)
			{
				m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
				m_iFlashBattery--;

				if (0 == m_iFlashBattery)
					FlashlightTurnOff();
			}
		}
		else
		{
			if (m_iFlashBattery < 100)
			{
				m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;
				m_iFlashBattery++;
			}
			else
				m_flFlashLightTime = 0;
		}

		MESSAGE_BEGIN(MSG_ONE, gmsgFlashBattery, NULL, pev);
		WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();
	}


	if ((m_iTrain & TRAIN_NEW) != 0)
	{
		ASSERT(gmsgTrain > 0);
		// send "health" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgTrain, NULL, pev);
		WRITE_BYTE(0); // the original sends 0 (no train HUD), the SDK m_iTrain & 0xF
		MESSAGE_END();

		m_iTrain &= ~TRAIN_NEW;
	}

	//
	// New Weapon?
	//
	if (!m_fKnownItem)
	{
		m_fKnownItem = true;

		// WeaponInit Message
		// byte  = # of weapons
		//
		// for each weapon:
		// byte		name str length (not including null)
		// bytes... name
		// byte		Ammo Type
		// byte		Ammo2 Type
		// byte		bucket
		// byte		bucket pos
		// byte		flags
		// ????		Icons

		// Send ALL the weapon info now
		int i;

		for (i = 0; i < MAX_WEAPONS; i++)
		{
			ItemInfo& II = CBasePlayerItem::ItemInfoArray[i];

			if (WEAPON_NONE == II.iId)
				continue;

			const char* pszName;
			if (!II.pszName)
				pszName = "Empty";
			else
				pszName = II.pszName;

			MESSAGE_BEGIN(MSG_ONE, gmsgWeaponList, NULL, pev);
			WRITE_STRING(pszName);				   // string	weapon name
			WRITE_BYTE(GetAmmoIndex(II.pszAmmo1)); // byte		Ammo Type
			WRITE_BYTE(II.iMaxAmmo1);			   // byte     Max Ammo 1
			WRITE_BYTE(GetAmmoIndex(II.pszAmmo2)); // byte		Ammo2 Type
			WRITE_BYTE(II.iMaxAmmo2);			   // byte     Max Ammo 2
			WRITE_BYTE(II.iSlot);				   // byte		bucket
			WRITE_BYTE(II.iPosition);			   // byte		bucket pos
			WRITE_BYTE(II.iId);					   // byte		id (bit index into m_WeaponBits)
			WRITE_BYTE(II.iFlags);				   // byte		Flags
			MESSAGE_END();
		}
	}


	SendAmmoUpdate();

	// Update all the items
	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		// RC 1.9's own: skips an item whose entity is gone (orig 0x1f44a0)
		if (m_rgpPlayerItems[i] && !FNullEnt(m_rgpPlayerItems[i]->pev)) // each item updates it's successors
			m_rgpPlayerItems[i]->UpdateClientData(this);
	}

	// Cache and client weapon change
	m_pClientActiveItem = m_pActiveItem;
	m_iClientFOV = m_iFOV;

	// Send new room type to client.
	if (m_ClientSndRoomtype != m_SndRoomtype)
	{
		m_ClientSndRoomtype = m_SndRoomtype;

		MESSAGE_BEGIN(MSG_ONE, SVC_ROOMTYPE, nullptr, edict());
		WRITE_SHORT((short)m_SndRoomtype); // sequence number
		MESSAGE_END();
	}

	//Handled anything that needs resetting
	m_bRestored = false;
}


//=========================================================
// FBecomeProne - Overridden for the player to set the proper
// physics flags when a barnacle grabs player.
//=========================================================
bool CBasePlayer::FBecomeProne()
{
	m_afPhysicsFlags |= PFLAG_ONBARNACLE;
	return true;
}

//=========================================================
// BarnacleVictimBitten - bad name for a function that is called
// by Barnacle victims when the barnacle pulls their head
// into its mouth. For the player, just die.
//=========================================================
void CBasePlayer::BarnacleVictimBitten(entvars_t* pevBarnacle)
{
	TakeDamage(pevBarnacle, pevBarnacle, pev->health + pev->armorvalue, DMG_SLASH | DMG_ALWAYSGIB);
}

//=========================================================
// BarnacleVictimReleased - overridden for player who has
// physics flags concerns.
//=========================================================
void CBasePlayer::BarnacleVictimReleased()
{
	m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
}


//=========================================================
// Illumination
// return player light level plus virtual muzzle flash
//=========================================================
int CBasePlayer::Illumination()
{
	int iIllum = CBaseEntity::Illumination();

	iIllum += m_iWeaponFlash;
	if (iIllum > 255)
		return 255;
	return iIllum;
}


void CBasePlayer::EnableControl(bool fControl)
{
	if (!fControl)
		pev->flags |= FL_FROZEN;
	else
		pev->flags &= ~FL_FROZEN;
}


#define DOT_1DEGREE 0.9998476951564
#define DOT_2DEGREE 0.9993908270191
#define DOT_3DEGREE 0.9986295347546
#define DOT_4DEGREE 0.9975640502598
#define DOT_5DEGREE 0.9961946980917
#define DOT_6DEGREE 0.9945218953683
#define DOT_7DEGREE 0.9925461516413
#define DOT_8DEGREE 0.9902680687416
#define DOT_9DEGREE 0.9876883405951
#define DOT_10DEGREE 0.9848077530122
#define DOT_15DEGREE 0.9659258262891
#define DOT_20DEGREE 0.9396926207859
#define DOT_25DEGREE 0.9063077870367

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector CBasePlayer::GetAutoaimVector(float flDelta)
{
	if (g_iSkillLevel == SKILL_HARD)
	{
		UTIL_MakeVectors(pev->v_angle + pev->punchangle);
		return gpGlobals->v_forward;
	}

	Vector vecSrc = GetGunPosition();
	float flDist = 8192;

	// always use non-sticky autoaim
	// UNDONE: use sever variable to chose!
	if (true || g_iSkillLevel == SKILL_MEDIUM)
	{
		m_vecAutoAim = Vector(0, 0, 0);
		// flDelta *= 0.5;
	}

	bool m_fOldTargeting = m_fOnTarget;
	Vector angles = AutoaimDeflection(vecSrc, flDist, flDelta);

	// update ontarget if changed
	if (!g_pGameRules->AllowAutoTargetCrosshair())
		m_fOnTarget = false;
	else if (m_fOldTargeting != m_fOnTarget)
	{
		m_pActiveItem->UpdateItemInfo();
	}

	if (angles.x > 180)
		angles.x -= 360;
	if (angles.x < -180)
		angles.x += 360;
	if (angles.y > 180)
		angles.y -= 360;
	if (angles.y < -180)
		angles.y += 360;

	if (angles.x > 25)
		angles.x = 25;
	if (angles.x < -25)
		angles.x = -25;
	if (angles.y > 12)
		angles.y = 12;
	if (angles.y < -12)
		angles.y = -12;


	// always use non-sticky autoaim
	// UNDONE: use sever variable to chose!
	if (false || g_iSkillLevel == SKILL_EASY)
	{
		m_vecAutoAim = m_vecAutoAim * 0.67 + angles * 0.33;
	}
	else
	{
		m_vecAutoAim = angles * 0.9;
	}

	// m_vecAutoAim = m_vecAutoAim * 0.99;

	// Don't send across network if sv_aim is 0
	// RC 1.9's (as SDK 2.1): sv_aim alone; halflife-updated also tested the engine's
	// sv_allow_autoaim (a null pointer on an engine without it)
	if (CVAR_GET_FLOAT("sv_aim") != 0)
	{
		if (m_vecAutoAim.x != m_lastx ||
			m_vecAutoAim.y != m_lasty)
		{
			SET_CROSSHAIRANGLE(edict(), -m_vecAutoAim.x, m_vecAutoAim.y);

			m_lastx = m_vecAutoAim.x;
			m_lasty = m_vecAutoAim.y;
		}
	}
	else
	{
		// halflife-updated's fix, kept: autoaim off resets the crosshair angle, so it doesn't stay
		// where a target held it (halflife #966)
		ResetAutoaim();
	}

	// ALERT( at_console, "%f %f\n", angles.x, angles.y );

	UTIL_MakeVectors(pev->v_angle + pev->punchangle + m_vecAutoAim);
	return gpGlobals->v_forward;
}


Vector CBasePlayer::AutoaimDeflection(Vector& vecSrc, float flDist, float flDelta)
{
	edict_t* pEdict = UTIL_GetEntityList() + 1;
	CBaseEntity* pEntity;
	float bestdot;
	Vector bestdir;
	edict_t* bestent;
	TraceResult tr;

	// RC 1.9's (as SDK 2.1); halflife-updated also returned when the engine's sv_allow_autoaim was 0
	if (CVAR_GET_FLOAT("sv_aim") == 0)
	{
		m_fOnTarget = false;
		return g_vecZero;
	}

	UTIL_MakeVectors(pev->v_angle + pev->punchangle + m_vecAutoAim);

	// try all possible entities
	bestdir = gpGlobals->v_forward;
	bestdot = flDelta; // +- 10 degrees
	bestent = NULL;

	m_fOnTarget = false;

	UTIL_TraceLine(vecSrc, vecSrc + bestdir * flDist, dont_ignore_monsters, edict(), &tr);


	if (tr.pHit && tr.pHit->v.takedamage != DAMAGE_NO)
	{
		// don't look through water
		if (!((pev->waterlevel != 3 && tr.pHit->v.waterlevel == 3) || (pev->waterlevel == 3 && tr.pHit->v.waterlevel == 0)))
		{
			if (tr.pHit->v.takedamage == DAMAGE_AIM)
				m_fOnTarget = true;

			return m_vecAutoAim;
		}
	}

	for (int i = 1; i < gpGlobals->maxEntities; i++, pEdict++)
	{
		Vector center;
		Vector dir;
		float dot;

		if (0 != pEdict->free) // Not in use
			continue;

		if (pEdict->v.takedamage != DAMAGE_AIM)
			continue;
		if (pEdict == edict())
			continue;
		//		if (pev->team > 0 && pEdict->v.team == pev->team)
		//			continue;	// don't aim at teammate
		if (!g_pGameRules->ShouldAutoAim(this, pEdict))
			continue;

		pEntity = Instance(pEdict);
		if (pEntity == NULL)
			continue;

		if (!pEntity->IsAlive())
			continue;

		// don't look through water
		if ((pev->waterlevel != 3 && pEntity->pev->waterlevel == 3) || (pev->waterlevel == 3 && pEntity->pev->waterlevel == 0))
			continue;

		center = pEntity->BodyTarget(vecSrc);

		dir = (center - vecSrc).Normalize();

		// make sure it's in front of the player
		if (DotProduct(dir, gpGlobals->v_forward) < 0)
			continue;

		dot = fabs(DotProduct(dir, gpGlobals->v_right)) + fabs(DotProduct(dir, gpGlobals->v_up)) * 0.5;

		// tweek for distance
		dot *= 1.0 + 0.2 * ((center - vecSrc).Length() / flDist);

		if (dot > bestdot)
			continue; // to far to turn

		UTIL_TraceLine(vecSrc, center, dont_ignore_monsters, edict(), &tr);
		if (tr.flFraction != 1.0 && tr.pHit != pEdict)
		{
			// ALERT( at_console, "hit %s, can't see %s\n", STRING( tr.pHit->v.classname ), STRING( pEdict->v.classname ) );
			continue;
		}

		// don't shoot at friends
		if (IRelationship(pEntity) < 0)
		{
			if (!pEntity->IsPlayer() && !g_pGameRules->IsDeathmatch())
				// ALERT( at_console, "friend\n");
				continue;
		}

		// can shoot at this one
		bestdot = dot;
		bestent = pEdict;
		bestdir = dir;
	}

	if (bestent)
	{
		bestdir = UTIL_VecToAngles(bestdir);
		bestdir.x = -bestdir.x;
		bestdir = bestdir - pev->v_angle - pev->punchangle;

		if (bestent->v.takedamage == DAMAGE_AIM)
			m_fOnTarget = true;

		return bestdir;
	}

	return Vector(0, 0, 0);
}


void CBasePlayer::ResetAutoaim()
{
	if (m_vecAutoAim.x != 0 || m_vecAutoAim.y != 0)
	{
		m_vecAutoAim = Vector(0, 0, 0);
		SET_CROSSHAIRANGLE(edict(), 0, 0);
	}
	m_fOnTarget = false;
}

/*
=============
SetCustomDecalFrames

  UNDONE:  Determine real frame limit, 8 is a placeholder.
  Note:  -1 means no custom frames present.
=============
*/
void CBasePlayer::SetCustomDecalFrames(int nFrames)
{
	if (nFrames > 0 &&
		nFrames < 8)
		m_nCustomSprayFrames = nFrames;
	else
		m_nCustomSprayFrames = -1;
}

/*
=============
GetCustomDecalFrames

  Returns the # of custom frames this player's custom clan logo contains.
=============
*/
int CBasePlayer::GetCustomDecalFrames()
{
	return m_nCustomSprayFrames;
}


//=========================================================
// DropPlayerItem - drop the named item, or if no name,
// the active item.
//=========================================================
void CBasePlayer::DropPlayerItem(char* pszItemName)
{
	if (!g_pGameRules->IsMultiplayer() || (weaponstay.value > 0))
	{
		// no dropping in single player.
		return;
	}

	if (0 == strlen(pszItemName))
	{
		// if this string has no length, the client didn't type a name!
		// assume player wants to drop the active item.
		// make the string null to make future operations in this function easier
		pszItemName = NULL;
	}

	CBasePlayerItem* pWeapon;
	int i;

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		pWeapon = m_rgpPlayerItems[i];

		while (pWeapon)
		{
			if (pszItemName)
			{
				// try to match by name.
				if (0 == strcmp(pszItemName, STRING(pWeapon->pev->classname)))
				{
					// match!
					break;
				}
			}
			else
			{
				// trying to drop active item
				if (pWeapon == m_pActiveItem)
				{
					// active item!
					break;
				}
			}

			pWeapon = pWeapon->m_pNext;
		}


		// if we land here with a valid pWeapon pointer, that's because we found the
		// item we want to drop and hit a BREAK;  pWeapon is the item.
		if (pWeapon)
		{
			// RC 1.9's (as SDK 2.1): drops it whatever GetNextBestWeapon says; the refusal (Valve's 2013
			// SDK, halflife-updated's seed) is a design change, what a player may drop (RemovePlayerItem
			// holsters it, so nothing keeps running)
			g_pGameRules->GetNextBestWeapon(this, pWeapon);

			UTIL_MakeVectors(pev->angles);

			ClearWeaponBit(pWeapon->m_iId); // take item off hud

			CWeaponBox* pWeaponBox = (CWeaponBox*)CBaseEntity::Create("weaponbox", pev->origin + gpGlobals->v_forward * 10, pev->angles, edict());
			pWeaponBox->pev->angles.x = 0;
			pWeaponBox->pev->angles.z = 0;
			pWeaponBox->PackWeapon(pWeapon);
			pWeaponBox->pev->velocity = gpGlobals->v_forward * 300 + gpGlobals->v_forward * 100;

			// drop half of the ammo for this weapon.
			int iAmmoIndex;

			iAmmoIndex = GetAmmoIndex(pWeapon->pszAmmo1()); // ???

			if (iAmmoIndex != -1)
			{
				// this weapon weapon uses ammo, so pack an appropriate amount.
				if ((pWeapon->iFlags() & ITEM_FLAG_EXHAUSTIBLE) != 0)
				{
					// pack up all the ammo, this weapon is its own ammo type
					pWeaponBox->PackAmmo(MAKE_STRING(pWeapon->pszAmmo1()), m_rgAmmo[iAmmoIndex]);
					m_rgAmmo[iAmmoIndex] = 0;
				}
				else
				{
					// pack half of the ammo
					pWeaponBox->PackAmmo(MAKE_STRING(pWeapon->pszAmmo1()), m_rgAmmo[iAmmoIndex] / 2);
					m_rgAmmo[iAmmoIndex] /= 2;
				}
			}

			return; // we're done, so stop searching with the FOR loop.
		}
	}
}

//=========================================================
// HasPlayerItem Does the player already have this item?
//=========================================================
bool CBasePlayer::HasPlayerItem(CBasePlayerItem* pCheckItem)
{
	CBasePlayerItem* pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while (pItem)
	{
		if (FClassnameIs(pItem->pev, STRING(pCheckItem->pev->classname)))
		{
			return true;
		}
		pItem = pItem->m_pNext;
	}

	return false;
}

//=========================================================
// HasNamedPlayerItem Does the player already have this item?
//=========================================================
// orig 0x1f5f10. RC 1.9's own: skips an item whose entity is gone or invalid
bool CBasePlayer::HasNamedPlayerItem(const char* pszItemName)
{
	CBasePlayerItem* pItem;
	int i;

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		pItem = m_rgpPlayerItems[i];

		while (pItem)
		{
			if (pItem->pev && !FNullEnt(pItem->pev) && UTIL_IsValidEntity(pItem->edict()) && 0 == strcmp(pszItemName, STRING(pItem->pev->classname)))
			{
				return true;
			}
			pItem = pItem->m_pNext;
		}
	}

	return false;
}

//=========================================================
// HasPlayerItemFromID
// Just compare IDs, rather than classnames
//=========================================================
bool CBasePlayer::HasPlayerItemFromID(int nID)
{
	CBasePlayerItem* pItem;
	int i;

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		pItem = m_rgpPlayerItems[i];

		while (pItem)
		{
			if (pItem->m_iId == nID)
			{
				return true;
			}

			pItem = pItem->m_pNext;
		}
	}

	return false;
}

//=========================================================
//
//=========================================================
bool CBasePlayer::SwitchWeapon(CBasePlayerItem* pWeapon)
{
	if (pWeapon && !pWeapon->CanDeploy())
	{
		return false;
	}

	ResetAutoaim();

	if (m_pActiveItem)
	{
		m_pActiveItem->Holster();
	}

	m_pActiveItem = pWeapon;

	if (pWeapon)
	{
		pWeapon->m_ForceSendAnimations = true;
		pWeapon->Deploy();
		pWeapon->m_ForceSendAnimations = false;
	}

	return true;
}

void CBasePlayer::EquipWeapon()
{
	if (m_pActiveItem)
	{
		if ((!FStringNull(pev->viewmodel) || !FStringNull(pev->weaponmodel)))
		{
			//Already have a weapon equipped and deployed.
			return;
		}

		//Have a weapon equipped, but not deployed.
		if (m_pActiveItem->CanDeploy() && m_pActiveItem->Deploy())
		{
			return;
		}
	}

	//No weapon equipped or couldn't deploy it, find a suitable alternative.
	g_pGameRules->GetNextBestWeapon(this, m_pActiveItem, true);
}

void CBasePlayer::SetPrefsFromUserinfo(char* infobuffer)
{
	const char* value = g_engfuncs.pfnInfoKeyValue(infobuffer, "cl_autowepswitch");

	if ('\0' != *value)
	{
		m_iAutoWepSwitch = atoi(value);
	}
	else
	{
		m_iAutoWepSwitch = 1;
	}
}

//=========================================================
// Dead HEV suit prop
//=========================================================
class CDeadHEV : public CBaseMonster
{
public:
	void Spawn() override;
	int Classify() override { return CLASS_HUMAN_MILITARY; }

	bool KeyValue(KeyValueData* pkvd) override;

	int m_iPose; // which sequence to display	-- temporary, don't need to save
	static const char* m_szPoses[4];
};

const char* CDeadHEV::m_szPoses[] = {"deadback", "deadsitting", "deadstomach", "deadtable"};

bool CDeadHEV::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);
		return true;
	}

	return CBaseMonster::KeyValue(pkvd);
}

LINK_ENTITY_TO_CLASS(monster_hevsuit_dead, CDeadHEV);

//=========================================================
// ********** DeadHEV SPAWN **********
//=========================================================
void CDeadHEV::Spawn()
{
	PRECACHE_MODEL("models/player.mdl");
	SET_MODEL(ENT(pev), "models/player.mdl");

	pev->effects = 0;
	pev->yaw_speed = 8;
	pev->sequence = 0;
	pev->body = 1;
	m_bloodColor = BLOOD_COLOR_RED;

	pev->sequence = LookupSequence(m_szPoses[m_iPose]);

	if (pev->sequence == -1)
	{
		ALERT(at_console, "Dead hevsuit with bad pose\n");
		pev->sequence = 0;
		pev->effects = EF_BRIGHTFIELD;
	}

	// Corpses have less health
	pev->health = 8;

	MonsterInitDead();
}


class CStripWeapons : public CPointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;

private:
};

LINK_ENTITY_TO_CLASS(player_weaponstrip, CStripWeapons);

void CStripWeapons::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBasePlayer* pPlayer = NULL;

	if (pActivator && pActivator->IsPlayer())
	{
		pPlayer = (CBasePlayer*)pActivator;
	}
	else if (!g_pGameRules->IsDeathmatch())
	{
		pPlayer = (CBasePlayer*)UTIL_GetLocalPlayer();
	}

	if (pPlayer)
		pPlayer->RemoveAllItems(false);
}


class CRevertSaved : public CPointEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT MessageThink();
	void EXPORT LoadThink();
	bool KeyValue(KeyValueData* pkvd) override;

	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	// out of line, as gcc 2.95 emitted them (orig 0x1f6cc0-0x1f6d60)
	float Duration();
	float HoldTime();
	float MessageTime();
	float LoadTime();

	void SetDuration(float duration);
	void SetHoldTime(float hold);
	void SetMessageTime(float time);
	void SetLoadTime(float time);

private:
	float m_messageTime;
	float m_loadTime;
};

float CRevertSaved::Duration() { return pev->dmg_take; }						  // orig 0x1f6d60
float CRevertSaved::HoldTime() { return pev->dmg_save; }						  // orig 0x1f6d40
float CRevertSaved::MessageTime() { return m_messageTime; }					  // orig 0x1f6d30
float CRevertSaved::LoadTime() { return m_loadTime; }							  // orig 0x1f6d20
void CRevertSaved::SetDuration(float duration) { pev->dmg_take = duration; }	  // orig 0x1f6d00
void CRevertSaved::SetHoldTime(float hold) { pev->dmg_save = hold; }			  // orig 0x1f6ce0
void CRevertSaved::SetMessageTime(float time) { m_messageTime = time; }		  // orig 0x1f6cd0
void CRevertSaved::SetLoadTime(float time) { m_loadTime = time; }				  // orig 0x1f6cc0

LINK_ENTITY_TO_CLASS(player_loadsaved, CRevertSaved);

TYPEDESCRIPTION CRevertSaved::m_SaveData[] =
	{
		DEFINE_FIELD(CRevertSaved, m_messageTime, FIELD_FLOAT), // These are not actual times, but durations, so save as floats
		DEFINE_FIELD(CRevertSaved, m_loadTime, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CRevertSaved, CPointEntity);

bool CRevertSaved::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration(atof(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		SetHoldTime(atof(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "messagetime"))
	{
		SetMessageTime(atof(pkvd->szValue));
		return true;
	}
	else if (FStrEq(pkvd->szKeyName, "loadtime"))
	{
		SetLoadTime(atof(pkvd->szValue));
		return true;
	}

	return CPointEntity::KeyValue(pkvd);
}

void CRevertSaved::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	UTIL_ScreenFadeAll(pev->rendercolor, Duration(), HoldTime(), pev->renderamt, FFADE_OUT);
	pev->nextthink = gpGlobals->time + MessageTime();
	SetThink(&CRevertSaved::MessageThink);
}


void CRevertSaved::MessageThink()
{
	UTIL_ShowMessageAll(STRING(pev->message));
	float nextThink = LoadTime() - MessageTime();
	if (nextThink > 0)
	{
		pev->nextthink = gpGlobals->time + nextThink;
		SetThink(&CRevertSaved::LoadThink);
	}
	else
		LoadThink();
}


void CRevertSaved::LoadThink()
{
	if (0 == gpGlobals->deathmatch)
	{
		SERVER_COMMAND("reload\n");
	}
}


//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission : public CPointEntity
{
	void Spawn() override;
	void Think() override;
};

void CInfoIntermission::Spawn()
{
	UTIL_SetOrigin(pev, pev->origin);
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	pev->nextthink = gpGlobals->time + 2; // let targets spawn!
}

void CInfoIntermission::Think()
{
	edict_t* pTarget;

	// find my target
	pTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(pev->target));

	if (!FNullEnt(pTarget))
	{
		pev->v_angle = UTIL_VecToAngles((pTarget->v.origin - pev->origin).Normalize());
		pev->v_angle.x = -pev->v_angle.x;
	}
}

LINK_ENTITY_TO_CLASS(info_intermission, CInfoIntermission);


