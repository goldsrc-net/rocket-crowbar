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
//=========================================================
// Rocket Crowbar 1.9: the hivehand, a freeze gun. It fires no hornets. Primary charges while
// held (eating ammo) and fires a freezing beam on release; the beam punches through thin walls
// and freezes what it hurts but doesn't kill (freeze_timer, CFreezeTimer). Hold it too long and
// it freezes and hurts the shooter. Secondary charges and throws a freeze grenade
// (CFreezeGreande). Reload toggles a zoom. Ammo regenerates. Tunables in rc.ini's [HORNETGUN].
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "hornet.h"
#include "gamerules.h"
#include "shake.h"
#include "UserMessages.h"
#include "Preff.h"
#include "hornetgun.h"

// the RC cap (CBaseEntity::HasRCCap) of what the freeze gun and grenade can freeze. VERIFY name
#define RC_CAP_FREEZE 0x100

int Freeze_Parts; // sprites/blueflare1.spr
int iFreezeBoom;  // sprites/FrzBoom1.spr
int iFreezeRing;  // sprites/shockwave.spr

#ifndef CLIENT_DLL

// The base the frozen thing's m_flNextAttack counts from: gpGlobals->time for players and
// monsters alike, as in the original (the server is built without CLIENT_WEAPONS).
static float NextAttackBase(CBaseEntity* pEntity)
{
	return gpGlobals->time;
}

//=========================================================
// CFreezeTimer: holds its owner frozen until pev->dmg, then thaws him.
//
// OVERLOADS SOME ENTVARS:
// owner - the frozen one, dmg - when he thaws, render* - his own render settings to restore
//=========================================================
class CFreezeTimer : public CBaseEntity
{
public:
	void Spawn() override;
	void PlayerWasKilled(edict_t* pPlayer) override;
	static void CreateOrUpdateTimer(entvars_t* pevTarget, float flTime);
	void EXPORT DoUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT DoThink();

	Vector m_vecAngles; // orig +0x34, the frozen one's view angles, held while frozen. VERIFY name
};

LINK_ENTITY_TO_CLASS(freeze_timer, CFreezeTimer); // orig 0x1b0900

// orig 0x1b0930: freeze pevTarget for flTime more seconds, or start
void CFreezeTimer::CreateOrUpdateTimer(entvars_t* pevTarget, float flTime)
{
	CBaseEntity* pEntity = NULL;
	bool bFound = false;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, g_vecZero, 4096)) != NULL)
	{
		if (FStrEq(STRING(pEntity->pev->classname), "freeze_timer") && pEntity->pev->owner == ENT(pevTarget))
		{
			pEntity->Use(CBaseEntity::Instance(ENT(pevTarget)), CBaseEntity::Instance(ENT(pevTarget)), USE_ON, flTime);
			bFound = true;
		}
	}

	if (bFound)
		return;

	CFreezeTimer* pTimer = static_cast<CFreezeTimer*>(CBaseEntity::Create("freeze_timer", g_vecZero, g_vecZero, ENT(pevTarget)));
	pTimer->pev->owner = ENT(pevTarget);
	pTimer->Spawn(); // a second time: Create already spawned it
	pTimer->pev->dmg = flTime + gpGlobals->time;
	pTimer->pev->renderamt = pevTarget->renderamt;
	pTimer->pev->renderfx = pevTarget->renderfx;
	pTimer->pev->rendermode = pevTarget->rendermode;
	pTimer->m_vecAngles = pevTarget->v_angle;

	if (CBaseEntity::Instance(ENT(pevTarget))->IsNetClient())
	{
		CBasePlayer* pPlayer = static_cast<CBasePlayer*>(CBaseEntity::Instance(ENT(pevTarget)));
		if (pPlayer)
		{
			UTIL_ScreenFade(pPlayer, Vector(128, 128, 255), 2.0, 0.0, 64, FFADE_STAYOUT);
			if (pPlayer->m_pActiveItem)
			{
				pPlayer->m_pActiveItem->Holster();
				pPlayer->pev->weaponmodel = 0;
			}
		}
	}

	// stop whatever he was saying or playing
	EMIT_SOUND(ENT(pevTarget), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM);
	EMIT_SOUND(ENT(pevTarget), CHAN_VOICE, "common/null.wav", 1, ATTN_NORM);
	EMIT_SOUND(ENT(pevTarget), CHAN_ITEM, "common/null.wav", 1, ATTN_NORM);
	EMIT_SOUND(ENT(pevTarget), CHAN_BODY, "common/null.wav", 1, ATTN_NORM);
}

// orig 0x1b0c60
void DestroyFreezeTimer(entvars_t* pevOwner)
{
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, g_vecZero, 4096)) != NULL)
	{
		if (FStrEq(STRING(pEntity->pev->classname), "freeze_timer") && pEntity->pev->owner == ENT(pevOwner))
			pEntity->Use(CBaseEntity::Instance(ENT(pevOwner)), CBaseEntity::Instance(ENT(pevOwner)), USE_OFF, 0);
	}
}

// orig 0x1b0d50
void CFreezeTimer::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->classname = MAKE_STRING("freeze_timer");
	SET_MODEL(ENT(pev), "");
	UTIL_SetOrigin(pev, g_vecZero);
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	SetThink(&CFreezeTimer::DoThink);
	SetUse(&CFreezeTimer::DoUse);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x1b0e60: USE_OFF thaws now; any other use adds value seconds
void CFreezeTimer::DoUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (useType != USE_OFF)
	{
		if (pev->dmg <= gpGlobals->time)
			pev->dmg = value + gpGlobals->time;
		else
			pev->dmg = value + pev->dmg;
		return;
	}

	CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
	if (pOwner)
	{
		// VERIFY: guard, the original crashes here: it reads m_pActiveItem from whatever it froze
		// without checking IsPlayer(); only players reach USE_OFF (PlayerWasKilled, DestroyFreezeTimer)
		CBasePlayer* pPlayer = pOwner->IsPlayer() ? static_cast<CBasePlayer*>(pOwner) : NULL;
		if (pPlayer && pPlayer->IsAlive() && pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->pev &&
			!FNullEnt(pPlayer->m_pActiveItem->pev) && pPlayer->m_pActiveItem->edict() &&
			UTIL_IsValidEntity(pPlayer->m_pActiveItem->edict()))
		{
			pPlayer->m_pActiveItem->Deploy();
		}

		UTIL_ScreenFade(pOwner, Vector(128, 128, 255), 2.0, 0.0, 64, FFADE_IN);
	}

	SetThink(&CFreezeTimer::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;

	// VERIFY: guard, the original crashes here (it writes through pev->owner without a NULL check)
	if (!pev->owner)
		return;
	pev->owner->v.renderamt = pev->renderamt;
	pev->owner->v.renderfx = pev->renderfx;
	pev->owner->v.rendermode = pev->rendermode;
	pev->owner->v.flags &= ~FL_FROZEN;

	CBaseMonster* pMonster = GetClassPtr((CBaseMonster*)VARS(pev->owner));
	if (pMonster->IsNetClient())
		pev->owner->v.fixangle = 0;
	pMonster->m_flNextAttack = NextAttackBase(pMonster) + 0.5;
}

// orig 0x1b10e0: keep the owner frozen and glowing until pev->dmg, then thaw him
void CFreezeTimer::DoThink()
{
	pev->nextthink = gpGlobals->time + 0.01;

	if (pev->owner && !FNullEnt(pev->owner) && 0 == pev->owner->free && pev->owner->pvPrivateData)
	{
		CBaseEntity* pOwner = CBaseEntity::Instance(pev->owner);
		if (pOwner)
		{
			static_cast<CBaseToggle*>(pOwner)->SentenceStop();

			entvars_t* pevOwner = VARS(pev->owner);
			if (pevOwner->health > 0 && pevOwner->deadflag == DEAD_NO && !(pev->dmg < gpGlobals->time) && pOwner->IsAlive())
			{
				pevOwner->renderfx = kRenderFxGlowShell;
				pevOwner->renderamt = 64;
				pevOwner->rendercolor = Vector(128, 128, 255);

				// CBasePlayer::IsOnLadder, which the original calls on whatever it froze
				if ((pevOwner->flags & FL_ONGROUND) != 0 || pevOwner->waterlevel != 0 || pevOwner->movetype == MOVETYPE_FLY)
					pevOwner->flags |= FL_FROZEN;

				if (pOwner->IsNetClient())
				{
					pevOwner->v_angle = m_vecAngles;
					pevOwner->fixangle = 1;
				}

				static_cast<CBaseMonster*>(pOwner)->m_flNextAttack = NextAttackBase(pOwner) + 1000000;
				return;
			}

			// thaw
			UTIL_ScreenFade(pOwner, Vector(128, 128, 255), 2.0, 0.0, 64, FFADE_IN);
			if (pOwner->IsPlayer() && pOwner->IsAlive())
			{
				CBasePlayer* pPlayer = static_cast<CBasePlayer*>(pOwner);
				if (pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->pev && !FNullEnt(pPlayer->m_pActiveItem->pev) &&
					pPlayer->m_pActiveItem->edict() && UTIL_IsValidEntity(pPlayer->m_pActiveItem->edict()) && pPlayer->IsAlive())
				{
					pPlayer->m_pActiveItem->Deploy();
				}
			}

			pevOwner->flags &= ~FL_FROZEN;
			pevOwner->fixangle = 0;
			static_cast<CBaseMonster*>(pOwner)->m_flNextAttack = NextAttackBase(pOwner) + 0.5;
			pevOwner->renderamt = pev->renderamt;
			pevOwner->renderfx = pev->renderfx;
			pevOwner->rendermode = pev->rendermode;
		}
	}

	SetThink(&CFreezeTimer::SUB_Remove);
}

// orig 0x1b4ef0
void CFreezeTimer::PlayerWasKilled(edict_t* pPlayer)
{
	if (pev->owner == pPlayer)
	{
		Use(this, this, USE_OFF, 0);
		pev->owner = NULL;
	}
}

//=========================================================
// CFreezeGreande: the freeze grenade. It bounces, then blows a freezing ring that hurts what it
// reaches and freezes what that doesn't kill.
//=========================================================
// CFreezeGreande is declared in weapons.h (handgrenade.cpp throws it too)

// orig 0x1b1560
void CFreezeGreande::FreezeDet()
{
	CBaseEntity* pEntity = NULL;
	float flRadius = PREFS_KeyLoopUpFloat("HORNETGUN", "FreezeGrenRadius", 256.0);

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/gauss2.wav", 1, ATTN_NORM, 0, PITCH_NORM);

	pev->origin.z += 16;
	Vector vecRing = pev->origin + Vector(0, 0, flRadius / 0.2);

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(iFreezeBoom);
	WRITE_BYTE(15); // scale * 10
	WRITE_BYTE(20); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(vecRing.x);
	WRITE_COORD(vecRing.y);
	WRITE_COORD(vecRing.z);
	WRITE_SHORT(iFreezeRing);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(0);	 // framerate
	WRITE_BYTE(2);	 // life
	WRITE_BYTE(20);	 // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(180); // r, g, b
	WRITE_BYTE(180);
	WRITE_BYTE(255);
	WRITE_BYTE(200); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 1);
	WRITE_SHORT(Freeze_Parts);
	WRITE_BYTE(5);	// count
	WRITE_BYTE(5);	// life
	WRITE_BYTE(2);	// scale
	WRITE_BYTE(20); // velocity
	WRITE_BYTE(15); // randomness
	MESSAGE_END();

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, flRadius)) != NULL)
	{
		const int iCap = RC_CAP_FREEZE;
		if (!pEntity->HasRCCap(iCap) || pEntity->pev->takedamage == DAMAGE_NO)
			continue;

		Vector vecCenter = pEntity->Center();
		TraceResult tr;
		UTIL_TraceLine(pev->origin, vecCenter, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction != 1.0 && tr.pHit != pEntity->edict())
			continue;

		float flMaxDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "FreezeGrenMaxDamage", 180.0);
		float flDist = (pev->origin - vecCenter).Length();
		float flDamage = ((flRadius - flDist) / flRadius) * flMaxDamage;
		if (flDamage <= 0)
			continue;

		if (PREFS_KeyLoopUpFloat("HORNETGUN", "FreezeGrenMaxDamage", 180.0) <= flDamage)
			flDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "FreezeGrenMaxDamage", 180.0);

		float flMinTime = PREFS_KeyLoopUpFloat("HORNETGUN", "FreezeGrenMinTime", 6.0);
		float flMaxExtraTime = PREFS_KeyLoopUpFloat("HORNETGUN", "FreezeGrenMaxExtraTime", 12.0);
		float flHealth = pEntity->pev->health;

		if (FNullEnt(pev->owner))
		{
			// VERIFY: with no owner the original divides by FreezeRealDmgFract where it otherwise multiplies
			float flFract = PREFS_KeyLoopUpFloat("HORNETGUN", "FreezeRealDmgFract", 0.5);
			pEntity->TakeDamage(pev, pev, flDamage / flFract, DMG_FREEZE | DMG_NEVERGIB);
		}
		else
		{
			float flFract = PREFS_KeyLoopUpFloat("HORNETGUN", "FreezeRealDmgFract", 0.5);
			pEntity->TakeDamage(pev, VARS(pev->owner), flFract * flDamage, DMG_FREEZE | DMG_NEVERGIB);
		}

		// freeze what the unscaled damage would have killed but the real hit didn't
		if (pEntity->pev->deadflag == DEAD_NO && pEntity->IsAlive() && flHealth < flDamage && pEntity->pev->health > 0)
			CFreezeTimer::CreateOrUpdateTimer(pEntity->pev, (flDamage / flMaxDamage) * flMaxExtraTime + flMinTime);
	}

	pev->effects |= EF_NODRAW;
	SetThink(&CFreezeGreande::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x1b1e60
void CFreezeGreande::PreDetThink()
{
	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->dmgtime <= gpGlobals->time)
		SetThink(&CFreezeGreande::FreezeDet);

	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2;
	}
}

// orig 0x1b1f50: CGrenade::BounceTouch with its spin scaled from m_vecMaxAVel
void CFreezeGreande::FreezeBounceTouch(CBaseEntity* pOther)
{
	// don't hit the guy that launched this grenade
	if (pOther->edict() == pev->owner)
		return;

	// only do damage if we're moving fairly fast
	if (m_flNextAttack < gpGlobals->time && pev->velocity.Length() > 100)
	{
		entvars_t* pevOwner = VARS(pev->owner);
		if (pevOwner)
		{
			TraceResult tr = UTIL_GetGlobalTrace();
			ClearMultiDamage();
			pOther->TraceAttack(pevOwner, 1, gpGlobals->v_forward, &tr, DMG_CLUB);
			ApplyMultiDamage(pev, pevOwner);
		}
		m_flNextAttack = gpGlobals->time + 1.0; // debounce
	}

	// The original works out the stock vecTestVelocity (velocity with z * 0.45) and never uses it.

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.8;
	}
	else
	{
		// play bounce sound
		BounceSound();
	}

	pev->avelocity = m_vecMaxAVel * (pev->velocity.Length() / 200);

	if (pev->avelocity.x < m_vecMaxAVel.x)
		pev->avelocity.x = m_vecMaxAVel.x;
	if (pev->avelocity.y < m_vecMaxAVel.y)
		pev->avelocity.y = m_vecMaxAVel.y;
	if (pev->avelocity.z < m_vecMaxAVel.z)
		pev->avelocity.z = m_vecMaxAVel.z;

	if (m_vecMaxAVel.x * 0.5 < pev->avelocity.x)
		pev->avelocity.x = 0;
	if (m_vecMaxAVel.y * 0.5 < pev->avelocity.y)
		pev->avelocity.y = 0;
	if (m_vecMaxAVel.z * 0.5 < pev->avelocity.z)
		pev->avelocity.z = 0;
}

// orig 0x1b2210
CFreezeGreande* CFreezeGreande::ShootFreezeTimed(entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float flTime)
{
	CFreezeGreande* pGrenade = GetClassPtr((CFreezeGreande*)NULL);
	pGrenade->Spawn();
	UTIL_SetOrigin(pGrenade->pev, vecStart);
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles(pGrenade->pev->velocity);
	pGrenade->pev->owner = ENT(pevOwner);
	UTIL_SetSize(pGrenade->pev, Vector(-4, -4, -4), Vector(4, 4, 4));
	pGrenade->pev->solid = SOLID_BBOX;

	pGrenade->SetTouch(&CFreezeGreande::FreezeBounceTouch); // Bounce if touched

	pGrenade->pev->dmgtime = gpGlobals->time + flTime;
	pGrenade->SetThink(&CFreezeGreande::PreDetThink);
	pGrenade->pev->nextthink = gpGlobals->time + 0.1;
	if (flTime < 0.1)
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = Vector(0, 0, 0);
	}

	pGrenade->pev->skin = 3;
	pGrenade->pev->renderfx = kRenderFxGlowShell;
	pGrenade->pev->renderamt = 32;
	pGrenade->pev->rendercolor = Vector(128, 128, 255);

	float flSpinX = RANDOM_FLOAT(-300, -500);
	float flSpinY = RANDOM_FLOAT(-300, -500);
	pGrenade->pev->avelocity = Vector(flSpinX, flSpinY, 0);
	pGrenade->m_vecMaxAVel = pGrenade->pev->avelocity;

	pGrenade->pev->gravity = PREFS_KeyLoopUpFloat("HORNETGUN", "GrenadeGravity", 0.6);
	pGrenade->pev->friction = PREFS_KeyLoopUpFloat("HORNETGUN", "GrenadeFriction", 0.8);
	pGrenade->pev->classname = MAKE_STRING("hornet"); // kills read "hornet"
	SET_MODEL(ENT(pGrenade->pev), "models/can.mdl");

	return pGrenade;
}

// orig 0x24e620: the pro-magnet curse steers the grenade at its speed; below the magnet's feet
// only sideways
void CFreezeGreande::ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce)
{
	float flBottom = pMagnet->Center().z + pMagnet->pev->mins.z;
	if (flBottom < pev->origin.z)
	{
		float flSpeed = pev->velocity.Length();
		pev->velocity = vecForce * flSpeed;
	}
	else
	{
		float flSpeed = pev->velocity.Length2D();
		pev->velocity.x = vecForce.x * flSpeed;
		pev->velocity.y = vecForce.y * flSpeed;
	}
}

// orig 0x24e720: the magnet steers a grenade someone else threw
bool CFreezeGreande::ProMagEffected(CBaseEntity* pMagnet)
{
	if (!pev->owner)
		return false;
	return pMagnet->edict() != pev->owner;
}

#endif // CLIENT_DLL

LINK_ENTITY_TO_CLASS(weapon_hornetgun, CHgun); // orig 0x1b24e0

// orig 0x1b2510
bool CHgun::IsHornetGun()
{
	return true;
}

// orig 0x1b2520
bool CHgun::IsUseable()
{
	return true;
}

// orig 0x1b2530
void CHgun::Spawn()
{
	Precache();
	m_iId = WEAPON_HORNETGUN;
	SET_MODEL(ENT(pev), "models/w_hgun.mdl");

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("HORNETGUN", "PickupAmmo", 24, 0, 250);
	m_iFirePhase = 0;

	FallInit(); // get ready to fall down.
}

// orig 0x1b25b0
void CHgun::Precache()
{
	PRECACHE_MODEL("models/v_hgun.mdl");
	PRECACHE_MODEL("models/w_hgun.mdl");
	PRECACHE_MODEL("models/p_hgun.mdl");
	PRECACHE_MODEL("models/can.mdl");

	Freeze_Parts = PRECACHE_MODEL("sprites/blueflare1.spr");
	iFreezeBoom = PRECACHE_MODEL("sprites/FrzBoom1.spr");
	iFreezeRing = PRECACHE_MODEL("sprites/shockwave.spr");
	PRECACHE_SOUND("weapons/gauss2.wav");

	UTIL_PrecacheOther("hornet");
}

// orig 0x1b2680. In multiplayer every hivehand comes full. The original sets the ammo after the
// base call, which extracted the pickup ammo; here AddPlayerItem extracts it afterwards, which
// GiveAmmo caps at the same maximum. The original also sends WeapPickup here; halflife-updated
// sends it from CBasePlayer::AddPlayerItem.
void CHgun::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);

#ifndef CLIENT_DLL
	if (g_pGameRules->IsMultiplayer())
		pPlayer->m_rgAmmo[PrimaryAmmoIndex()] = PREFS_KeyLoopUpIntClamp("HORNETGUN", "MaxAmmo", 24, 0, 250);
#endif
	// As the original: this weapon tells the player it was picked up (AddPlayerItem doesn't)
	MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
	WRITE_BYTE(m_iId);
	MESSAGE_END();
}

// orig 0x1b2760
bool CHgun::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Hornets";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("HORNETGUN", "MaxAmmo", 24, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_HORNETGUN;
	p->iFlags = ITEM_FLAG_NOAUTOSWITCHEMPTY | ITEM_FLAG_NOAUTORELOAD;
	p->iWeight = 10; // the SDK's HORNETGUN_WEIGHT is 15

	return true;
}

// orig 0x1b2810
bool CHgun::Deploy()
{
	m_flNextZoomTime = gpGlobals->time;
	return DefaultDeploy("models/v_hgun.mdl", "models/p_hgun.mdl", HGUN_UP, "hive");
}

// orig 0x1b2860
void CHgun::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(HGUN_DOWN);
	m_iFirePhase = 0;

	//!!!HACKHACK - can't select hornetgun if it's empty! no way to get ammo for it, either.
	if (0 == m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()])
	{
		m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] = 1;
	}

	if (0 != m_fInZoom)
	{
		m_fInZoom = 0;
		m_pPlayer->m_iFOV = 0;
		m_flNextZoomTime = gpGlobals->time + 0.5;
	}
}

#ifndef CLIENT_DLL
// orig 0x1b2930: the beam from vecSrc to the trace's end and its effects there; damage and maybe
// freeze what it hit. True when the beam stops there (it hit a damageable non-brush entity).
bool CHgun::DoFreezeStuff(TraceResult* ptr, bool bFirst, Vector vecSrc, float flDamage)
{
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(ptr->vecEndPos.x);
	WRITE_COORD(ptr->vecEndPos.y);
	WRITE_COORD(ptr->vecEndPos.z);
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(50);	 // framerate
	WRITE_BYTE(1);	 // life
	WRITE_BYTE(80);	 // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(180); // r, g, b
	WRITE_BYTE(180);
	WRITE_BYTE(255);
	WRITE_BYTE(200); // brightness
	WRITE_BYTE(50);	 // speed
	MESSAGE_END();

	Vector vecDir = (ptr->vecEndPos - vecSrc).Normalize();

	if (!bFirst)
	{
		// coming out of a wall
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecSrc);
		WRITE_BYTE(TE_SPRITETRAIL);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_COORD(vecSrc.x + vecDir.x);
		WRITE_COORD(vecSrc.y + vecDir.y);
		WRITE_COORD(vecSrc.z + vecDir.z);
		WRITE_SHORT(Freeze_Parts);
		WRITE_BYTE(5);	// count
		WRITE_BYTE(5);	// life
		WRITE_BYTE(1);	// scale
		WRITE_BYTE(10); // velocity
		WRITE_BYTE(10); // randomness
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecSrc);
		WRITE_BYTE(TE_GLOWSPRITE);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_SHORT(Freeze_Parts);
		WRITE_BYTE(7);	 // life
		WRITE_BYTE(4);	 // scale
		WRITE_BYTE(200); // brightness
		MESSAGE_END();
	}

	if (ptr->pHit != NULL)
	{
		const int iCap = RC_CAP_FREEZE;
		if (VARS(ptr->pHit) != NULL && g_pGameRules->CanPlayerAnnoy(m_pPlayer->pev, VARS(ptr->pHit)) &&
			CBaseEntity::Instance(ptr->pHit)->HasRCCap(iCap))
		{
			// a freezeable target: hurt it, and freeze it when the hit would have killed it
			// but the real (scaled) damage didn't
			float flHealth = ptr->pHit->v.health;
			CBaseEntity* pEntity = CBaseEntity::Instance(ptr->pHit);

			pev->classname = MAKE_STRING("hornet");
			ClearMultiDamage();
			float flFract = PREFS_KeyLoopUpFloat("HORNETGUN", "FreezeRealDmgFract", 0.5);
			pEntity->TraceAttack(pev, flFract * flDamage, vecDir.Normalize(), ptr, DMG_FREEZE | DMG_NEVERGIB);
			ApplyMultiDamage(pev, m_pPlayer->pev);
			pev->classname = MAKE_STRING("weapon_hornetgun");

			float flMinTime = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMinTime", 3.0);
			float flExtraTime = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryExtraTime", 7.0);
			float flBaseDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "BaseDamage", 20.0);
			float flMaxDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxDamage", 120.0);
			if (pEntity->pev->deadflag == DEAD_NO && pEntity->IsAlive() && flHealth < flDamage && pEntity->pev->health > 0)
				CFreezeTimer::CreateOrUpdateTimer(pEntity->pev, (flDamage / (flMaxDamage + flBaseDamage)) * flExtraTime + flMinTime);
			return true;
		}

		// anything else that takes damage is just hurt
		if (ptr->pHit != NULL && ptr->pHit->v.takedamage != 0)
		{
			CBaseEntity* pEntity = CBaseEntity::Instance(ptr->pHit);
			pev->classname = MAKE_STRING("hornet");
			ClearMultiDamage();
			float flFract = PREFS_KeyLoopUpFloat("HORNETGUN", "FreezeRealDmgFract", 0.5);
			pEntity->TraceAttack(pev, flFract * flDamage, vecDir.Normalize(), ptr, DMG_FREEZE | DMG_NEVERGIB);
			ApplyMultiDamage(pev, m_pPlayer->pev);
			pev->classname = MAKE_STRING("weapon_hornetgun");
		}
	}

	// frost where it landed
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, ptr->vecEndPos);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(ptr->vecEndPos.x);
	WRITE_COORD(ptr->vecEndPos.y);
	WRITE_COORD(ptr->vecEndPos.z);
	WRITE_COORD(ptr->vecEndPos.x + ptr->vecPlaneNormal.x);
	WRITE_COORD(ptr->vecEndPos.y + ptr->vecPlaneNormal.y);
	WRITE_COORD(ptr->vecEndPos.z + ptr->vecPlaneNormal.z);
	WRITE_SHORT(Freeze_Parts);
	WRITE_BYTE(5);	// count
	WRITE_BYTE(5);	// life
	WRITE_BYTE(1);	// scale
	WRITE_BYTE(10); // velocity
	WRITE_BYTE(10); // randomness
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, ptr->vecEndPos);
	WRITE_BYTE(TE_GLOWSPRITE);
	WRITE_COORD(ptr->vecEndPos.x);
	WRITE_COORD(ptr->vecEndPos.y);
	WRITE_COORD(ptr->vecEndPos.z);
	WRITE_SHORT(Freeze_Parts);
	WRITE_BYTE(7);	 // life
	WRITE_BYTE(4);	 // scale
	WRITE_BYTE(200); // brightness
	MESSAGE_END();

	if (ptr->pHit != NULL && VARS(ptr->pHit) != NULL && ptr->pHit->v.takedamage != 0)
	{
		if (!CBaseEntity::Instance(ptr->pHit)->IsBSPModel())
			return true;
	}
	return false;
}

// orig 0x1b3400: fire the beam; it kicks the shooter back, and while it hits walls it punches on
// through up to PrimaryMaxPunchWalls of them, losing damage with the thickness
void CHgun::FireFreeze(float flDamage)
{
	Vector vecSrc = m_pPlayer->GetGunPosition();
	float flMaxPunch = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxPunchDist", 32.0);
	float flPunch = flMaxPunch;
	int iWalls = PREFS_KeyLoopUpInt("HORNETGUN", "PrimaryMaxPunchWalls", 16);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	// recoil, in proportion to the charge
	float flMaxDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxDamage", 120.0);
	float flBaseDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "BaseDamage", 20.0);
	Vector vecKick = gpGlobals->v_forward * (flDamage / (flBaseDamage + flMaxDamage));
	float flKick = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxKick", 560.0);
	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - vecKick * flKick;

	Vector vecDir = gpGlobals->v_forward;

	TraceResult tr;
	Vector vecEnd = m_pPlayer->GetGunPosition() + vecDir.Normalize() * 4096;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr);

	vecSrc = vecSrc + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12;

	if (DoFreezeStuff(&tr, true, vecSrc, flDamage) || !(flPunch > 0))
		return;

	while (iWalls > 0 && 0 == tr.fAllSolid && 0 == tr.fStartSolid)
	{
		// how thick is the wall?
		vecSrc = tr.vecEndPos + vecDir;
		UTIL_TraceLine(vecSrc, vecSrc + vecDir * flPunch, ignore_monsters, m_pPlayer->edict(), &tr);
		if (0 != tr.fAllSolid)
			return;

		TraceResult trBack;
		UTIL_TraceLine(tr.vecEndPos + vecDir * flPunch, vecSrc, ignore_monsters, m_pPlayer->edict(), &trBack);
		if (0 != trBack.fStartSolid)
			return;

		flPunch -= (trBack.vecEndPos - vecSrc).Length();
		float flPunchDamage = ((flPunch / flMaxPunch) * 0.5 + 0.5) * flDamage;
		if (flPunchDamage < 1.0)
			return;
		if (flPunch < 0)
			return;

		// out the other side
		vecSrc = trBack.vecEndPos + vecDir;
		UTIL_TraceLine(vecSrc, vecSrc + vecDir * 4096, dont_ignore_monsters, m_pPlayer->edict(), &tr);
		if (DoFreezeStuff(&tr, false, vecSrc, flPunchDamage))
			return;

		iWalls--;
		if (flPunch <= 0)
			return;
	}
}
#endif

// orig 0x1b3c50: start a charge, keep charging (one ammo per PrimaryChargeTime / PrimaryMaxAmmo
// seconds, up to PrimaryChargeTime), fire when the ammo runs out, and overload when held past
// PrimaryOverloadTime more
void CHgun::PrimaryAttack()
{
	if (m_iFirePhase == 0)
	{
		ReloadAmmo();

		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
		{
			float flChargeTime = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryChargeTime", 2.0);
			float flMaxAmmo = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxAmmo", 6.0);
			m_flNextAmmoDrain = flChargeTime / flMaxAmmo + gpGlobals->time;

			m_flNextPrimaryAttack = gpGlobals->time + 1.0;
			m_flNextSecondaryAttack = gpGlobals->time + 1.0;
			m_flRechargeTime = gpGlobals->time + 1.0;
			m_flTimeWeaponIdle = gpGlobals->time + 1.0;

			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
			m_flChargeStart = gpGlobals->time;
			m_iFirePhase = 1;
			m_bSecondaryCharge = false;
			SendWeaponAnim(HGUN_IDLE1);
			m_flNextAnimTime = gpGlobals->time + 1.875;
		}
		return;
	}

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 1)
	{
		// out of ammo: fire what's charged. VERIFY: here the charge is 0.5 * elapsed, where
		// WeaponAttackIdle divides by PrimaryChargeTime (the same at its default of 2)
		float flMaxDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxDamage", 120.0);
		float flElapsed = gpGlobals->time - m_flChargeStart;
		float flDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "BaseDamage", 20.0) + (float)(0.5 * flElapsed * flMaxDamage);
		flMaxDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxDamage", 120.0);
		float flTop = PREFS_KeyLoopUpFloat("HORNETGUN", "BaseDamage", 20.0) + flMaxDamage;
		if (flTop < flDamage)
		{
			flMaxDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxDamage", 120.0);
			flDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "BaseDamage", 20.0) + flMaxDamage;
		}
		m_iFirePhase = 0;
		StartPrimaryFire(flDamage);
		return;
	}

	if (m_flNextAmmoDrain < gpGlobals->time && m_flNextAmmoDrain != -1)
	{
		float flChargeTime = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryChargeTime", 2.0);
		float flMaxAmmo = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxAmmo", 6.0);
		m_flNextAmmoDrain = flChargeTime / flMaxAmmo + gpGlobals->time;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	}

	// fully charged: no more ammo drain
	if (PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryChargeTime", 2.0) < gpGlobals->time - m_flChargeStart)
		m_flNextAmmoDrain = -1;

	float flChargeTime = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryChargeTime", 2.0);
	float flOverload = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryOverloadTime", 5.0) + flChargeTime;
	if (!(flOverload < gpGlobals->time - m_flChargeStart))
		return;

	// overload: it freezes and hurts the shooter
	float flMaxDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxDamage", 120.0);
	float flBaseDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "BaseDamage", 20.0);
#ifndef CLIENT_DLL
	if (m_pPlayer->pev->deadflag == DEAD_NO && m_pPlayer->pev->health > 0)
		CFreezeTimer::CreateOrUpdateTimer(m_pPlayer->pev, 10.0);
#endif

	pev->classname = MAKE_STRING("hornet");
	m_pPlayer->TakeDamage(pev, m_pPlayer->pev, (flBaseDamage + flMaxDamage) * 0.5, DMG_FREEZE | DMG_NEVERGIB);
	pev->classname = MAKE_STRING("weapon_hornetgun");

	m_iFirePhase = 0;
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	m_flRechargeTime = PREFS_KeyLoopUpFloat("HORNETGUN", "PostfireNextRecharge", 0.75) + gpGlobals->time;
	m_flNextPrimaryAttack = gpGlobals->time + 0.3;
	m_flNextSecondaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "agrunt/ag_fire1.wav", 1, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "agrunt/ag_fire2.wav", 1, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "agrunt/ag_fire3.wav", 1, ATTN_NORM);
		break;
	}
}

// orig 0x1b4180
void CHgun::StartPrimaryFire(float flDamage)
{
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	m_flRechargeTime = PREFS_KeyLoopUpFloat("HORNETGUN", "PostfireNextRecharge", 0.75) + gpGlobals->time;

	SendWeaponAnim(HGUN_SHOOT);
	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryRefire", 0.3) * m_pPlayer->m_fCurseRefireScale;
	m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryRefire", 0.3) * m_pPlayer->m_fCurseRefireScale;
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "agrunt/ag_fire1.wav", 1, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "agrunt/ag_fire2.wav", 1, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "agrunt/ag_fire3.wav", 1, ATTN_NORM);
		break;
	}

#ifndef CLIENT_DLL
	FireFreeze(flDamage);
#endif
}

// orig 0x1b4320: a charge is held (WeaponIdle runs when no attack button is down, so a released
// button lands here): fire it, or keep holding
void CHgun::WeaponAttackIdle()
{
	if (m_flNextAnimTime < gpGlobals->time)
	{
		SendWeaponAnim(HGUN_IDLE1);
		m_flNextAnimTime = gpGlobals->time + 1.875;
	}

	if (!m_bSecondaryCharge)
	{
		if ((m_pPlayer->pev->button & IN_ATTACK) == 0)
		{
			float flMaxDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxDamage", 120.0);
			float flChargeTime = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryChargeTime", 2.0);
			float flElapsed = gpGlobals->time - m_flChargeStart;
			float flDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "BaseDamage", 20.0) + (float)((flElapsed / flChargeTime) * flMaxDamage);
			flMaxDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxDamage", 120.0);
			float flTop = PREFS_KeyLoopUpFloat("HORNETGUN", "BaseDamage", 20.0) + flMaxDamage;
			if (flTop < flDamage)
			{
				flMaxDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "PrimaryMaxDamage", 120.0);
				flDamage = PREFS_KeyLoopUpFloat("HORNETGUN", "BaseDamage", 20.0) + flMaxDamage;
			}
			m_iFirePhase = 0;
			StartPrimaryFire(flDamage);
			return;
		}

		m_flNextPrimaryAttack = gpGlobals->time + 1.0;
		m_flNextSecondaryAttack = gpGlobals->time + 1.0;
	}
	else
	{
		if ((m_pPlayer->pev->button & IN_ATTACK2) == 0)
		{
			FinishSecondaryAttack();
			return;
		}

		m_flNextPrimaryAttack = gpGlobals->time + 1000000;
		m_flNextSecondaryAttack = gpGlobals->time + 1000000;
	}

	m_flRechargeTime = gpGlobals->time + 1.0;
	m_flTimeWeaponIdle = gpGlobals->time + 0.1;
}

// orig 0x1b4520: throw the freeze grenade. The longer the charge was held, the shorter its fuse
// and the faster the throw.
void CHgun::FinishSecondaryAttack()
{
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	m_flRechargeTime = PREFS_KeyLoopUpFloat("HORNETGUN", "PostfireNextRecharge", 0.75) + gpGlobals->time;

	SendWeaponAnim(HGUN_SHOOT);
	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("HORNETGUN", "SecondaryRefire", 0.7) * m_pPlayer->m_fCurseRefireScale;
	m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("HORNETGUN", "SecondaryRefire", 0.7) * m_pPlayer->m_fCurseRefireScale;
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);

	float flFuse;
	float flLeft = m_flSecondaryEnd - gpGlobals->time;
	if (PREFS_KeyLoopUpFloat("HORNETGUN", "SecondaryMaxTime", 2.0) - 0.01 <= flLeft)
		flFuse = PREFS_KeyLoopUpFloat("HORNETGUN", "SecondaryMaxTime", 2.0) - 0.01;
	else
		flFuse = m_flSecondaryEnd - gpGlobals->time;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecSrc = m_pPlayer->GetGunPosition();

	double flBoost = PREFS_KeyLoopUpFloat("HORNETGUN", "SecondaryBoostVel", 100.0);
	double flBase = PREFS_KeyLoopUpFloat("HORNETGUN", "SecondaryBaseVel", 800.0);
	float flSpeed;
	if (1.0 <= 1.0 - flFuse / PREFS_KeyLoopUpFloat("HORNETGUN", "SecondaryMaxTime", 2.0))
		flSpeed = flBase + flBoost;
	else
		flSpeed = (1.0 - flFuse / PREFS_KeyLoopUpFloat("HORNETGUN", "SecondaryMaxTime", 2.0)) * flBoost + flBase;

#ifndef CLIENT_DLL
	CFreezeGreande::ShootFreezeTimed(m_pPlayer->pev, vecSrc, gpGlobals->v_forward * flSpeed, flFuse);
#endif

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "agrunt/ag_fire1.wav", 1, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "agrunt/ag_fire2.wav", 1, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "agrunt/ag_fire3.wav", 1, ATTN_NORM);
		break;
	}

	m_iFirePhase = 0;
	m_flSecondaryEnd = -1;
	m_bSecondaryCharge = false;
}

// orig 0x1b48d0: start a secondary charge, paid for up front
void CHgun::SecondaryAttack()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < PREFS_KeyLoopUpInt("HORNETGUN", "SecondaryAmmoUssage", 18))
		return;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= PREFS_KeyLoopUpInt("HORNETGUN", "SecondaryAmmoUssage", 18);
	m_iFirePhase = 1;
	m_flSecondaryEnd = gpGlobals->time + 2.0;
	SendWeaponAnim(HGUN_IDLE1);
	m_bSecondaryCharge = true;
	m_flNextAnimTime = gpGlobals->time;
	m_flNextPrimaryAttack = gpGlobals->time + 1000000;
	m_flNextSecondaryAttack = gpGlobals->time + 1000000;
}

// orig 0x1b49d0: regenerate one ammo per AmmoRefillRate seconds
void CHgun::ReloadAmmo()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= PREFS_KeyLoopUpIntClamp("HORNETGUN", "MaxAmmo", 24, 0, 250))
		return;

	while (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < PREFS_KeyLoopUpIntClamp("HORNETGUN", "MaxAmmo", 24, 0, 250))
	{
		if (!(m_flRechargeTime < gpGlobals->time))
			return;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]++;
		m_flRechargeTime = PREFS_KeyLoopUpFloat("HORNETGUN", "AmmoRefillRate", 0.75) + m_flRechargeTime;
	}
}

// orig 0x1b4af0: toggle the zoom
void CHgun::Reload()
{
	if (!(m_flNextZoomTime < gpGlobals->time))
		return;

	if (0 == m_fInZoom)
	{
		m_fInZoom = 1;
		m_pPlayer->m_iFOV = 30;
	}
	else
	{
		m_fInZoom = 0;
		m_pPlayer->m_iFOV = 0;
	}
	m_flNextZoomTime = gpGlobals->time + 0.5;
}

// orig 0x1b4b80
void CHgun::WeaponIdle()
{
	if (m_iFirePhase == 1)
	{
		WeaponAttackIdle();
		return;
	}

	ReloadAmmo();

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.75)
	{
		iAnim = HGUN_IDLE1;
		m_flTimeWeaponIdle = gpGlobals->time + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = HGUN_FIDGETSWAY;
		m_flTimeWeaponIdle = gpGlobals->time + 40.0 / 16.0;
	}
	else
	{
		iAnim = HGUN_FIDGETSHAKE;
		m_flTimeWeaponIdle = gpGlobals->time + 35.0 / 16.0;
	}
	SendWeaponAnim(iAnim);
}

// RC's bot object, as the original reads it: a CBaseEntity (its Center() is called) whose first
// member is its entvars, with a float at +0x698 that gates firing. These are the original i386
// bot's offsets in a class this library never defines (reference/DEVIATIONS.md, "Bot interface").
static CBaseEntity* RCBotEnt(CBot* pBot) { return reinterpret_cast<CBaseEntity*>(pBot); }
static entvars_t* RCBotPev(CBot* pBot) { return *reinterpret_cast<entvars_t**>(pBot); }
static float RCBotFireGate(CBot* pBot) { return *reinterpret_cast<float*>(reinterpret_cast<byte*>(pBot) + 0x698); } // VERIFY name

// orig 0x1b4c70: close in when far; fire as the base does, but never with secondary. The
// original also calls pEnemy->GetBotShootOrg() twice and discards both results.
void CHgun::BotWeaponFrame(CBot* pBot, CBaseEntity* pEnemy)
{
	// VERIFY: guard, the original crashes here on a NULL enemy
	if (pEnemy)
	{
		float flDist = (pEnemy->Center() - RCBotEnt(pBot)->Center()).Length();
		if (flDist >= 1024)
			RCBotPev(pBot)->button |= IN_FORWARD;
	}

	if (RCBotFireGate(pBot) / 3.0 >= 999.0)
	{
		if (BotFirePrimary(pBot, pEnemy))
			RCBotPev(pBot)->button |= IN_ATTACK;
	}
}

// orig 0x1b4db0
bool CHgun::BotFireSecondary(CBot* pBot, CBaseEntity* pEnemy)
{
	return false;
}

// orig 0x1b4dc0: hold the charge until it's full (2 s), and not from close up
bool CHgun::BotFirePrimary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (pEnemy == NULL)
		return true;

	if (m_iFirePhase != 0 && gpGlobals->time - m_flChargeStart > 2.0)
		return false;

	float flDist = (pEnemy->Center() - RCBotEnt(pBot)->Center()).Length();
	return flDist > 96;
}
