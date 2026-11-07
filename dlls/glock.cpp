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

// Rocket Crowbar 1.9: the glock's primary throws a bouncing acid gib (CAcidGib) instead of a
// bullet; its secondary (the "fart") spends two rounds on a small blast out of the back that
// pushes the shooter forward. Numbers come from rc.ini's [GLOCK]. No events: effects are
// server-side, as in the 2001 SDK RC was built on. Reconstructed from rc_i386.so (file
// addresses, reference/funcfile.tsv); see reference/dossiers/w2-python-mp5-glock-weapons.md.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "Preff.h"
#include "rcbot.h"

#define RC_CAP_ACIDGIB_IMMUNE 0x4000 // an entity with this RC cap takes no acid gib damage. VERIFY name

// The glock's round: a human gib that bounces GibNumBounces times, trailing blood, and
// hurts the first damageable thing it touches.
class CAcidGib : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	int BloodColor() override { return BLOOD_COLOR_RED; } // orig 0x19eb00
	void EXPORT AnimThink();
	void EXPORT GibBounceTouch(CBaseEntity* pOther);
	void ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce) override;
	virtual bool ProMagEffected(CBaseEntity* pOther);

	static CAcidGib* AcidGibCreate();
};

LINK_ENTITY_TO_CLASS(acidgib, CAcidGib); // orig 0x19e330

// orig 0x19e360
CAcidGib* CAcidGib::AcidGibCreate()
{
	CAcidGib* pGib = GetClassPtr((CAcidGib*)NULL);
	pGib->pev->classname = MAKE_STRING("9mmhandgun");
	pGib->Spawn();
	return pGib;
}

// orig 0x19e3b0. pev->team counts the bounces left, dmgtime gates bounce effects, dmg_take the
// blood trail, dmg_save is the 20 second lifetime, and enemy remembers the shooter once AnimThink
// clears owner (so the shooter can be hit too).
void CAcidGib::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->gravity = PREFS_KeyLoopUpFloat("GLOCK", "GibGravScale", 0.5);
	pev->friction = PREFS_KeyLoopUpFloat("GLOCK", "GibFriction", 0.3);
	SET_MODEL(ENT(pev), "models/hgibs.mdl");
	pev->body = RANDOM_LONG(0, 10);
	pev->team = PREFS_KeyLoopUpInt("GLOCK", "GibNumBounces", 2) - 1;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	SetTouch(&CAcidGib::GibBounceTouch);
	SetThink(&CAcidGib::AnimThink);
	pev->nextthink = gpGlobals->time + 0.2;

	pev->dmg = PREFS_KeyLoopUpFloat("GLOCK", "GibDamage", 30.0);
	pev->dmgtime = gpGlobals->time;
	pev->dmg_take = gpGlobals->time;
	pev->dmg_save = gpGlobals->time + 20;
	pev->enemy = pev->owner;

	pev->angles = Vector(RANDOM_FLOAT(0, 360), RANDOM_FLOAT(0, 360), RANDOM_FLOAT(0, 360));
	pev->avelocity = Vector(RANDOM_FLOAT(-1000, 1000), RANDOM_FLOAT(-1000, 1000), RANDOM_FLOAT(-1000, 1000));
}

// orig 0x19e630. Original bug, kept: nextthink is set to 0.1, not time + 0.1, so it thinks
// every server frame.
void CAcidGib::AnimThink()
{
	pev->nextthink = 0.1;

	if (pev->owner)
	{
		pev->enemy = pev->owner;
		pev->owner = NULL;
	}

	if (pev->enemy && 0 == pev->enemy->free && pev->enemy->pvPrivateData && !FNullEnt(pev->enemy) && !(pev->dmg_save < gpGlobals->time))
	{
		if (!(pev->dmg_take < gpGlobals->time))
			return;

		pev->dmg_take = gpGlobals->time + RANDOM_FLOAT(0.2, 0.4);

		if (CVAR_GET_FLOAT("sv_lowlag") == 1)
			return;

		UTIL_BloodDrips(pev->origin, pev->velocity.Normalize(), BloodColor(), 10);
		return;
	}

	SetThink(&CAcidGib::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.01;
}

// orig 0x19e810
void CAcidGib::GibBounceTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;

	TraceResult tr = UTIL_GetGlobalTrace();

	if (pOther->pev->takedamage != DAMAGE_NO && pev->enemy)
	{
		int iCap = RC_CAP_ACIDGIB_IMMUNE;
		if (!pOther->HasRCCap(iCap))
		{
			pOther->TakeDamage(pev, VARS(pev->enemy), pev->dmg, DMG_ACID);
			UTIL_BloodDrips(pev->origin, g_vecZero, BloodColor(), 60);
			EMIT_SOUND_DYN(edict(), CHAN_VOICE, "squeek/sqk_blast1.wav", 0.8, ATTN_NORM, 0, PITCH_NORM);
			UTIL_BloodDecalTrace(&tr, BloodColor());

			SetTouch(NULL);
			SetThink(&CAcidGib::SUB_Remove);
			pev->effects |= EF_NODRAW;
			pev->nextthink = gpGlobals->time + 0.01;
			return;
		}
	}

	if (pev->dmgtime <= gpGlobals->time)
	{
		pev->dmgtime = gpGlobals->time + 0.1;

		if (tr.pHit == pOther->edict())
		{
			EMIT_SOUND_DYN(edict(), CHAN_VOICE, "squeek/sqk_blast1.wav", 0.8, ATTN_NORM, 0, PITCH_NORM);
			UTIL_BloodDecalTrace(&tr, BloodColor());

			if (pev->team-- <= 0)
			{
				if (CVAR_GET_FLOAT("sv_lowlag") != 1)
					UTIL_BloodDrips(pev->origin, g_vecZero, BloodColor(), 60);

				SetTouch(NULL);
				SetThink(&CAcidGib::SUB_Remove);
				pev->nextthink = gpGlobals->time + 0.01;
			}
			else
			{
				if (CVAR_GET_FLOAT("sv_lowlag") != 1)
					UTIL_BloodDrips(pev->origin, g_vecZero, BloodColor(), 40);
			}
		}
	}
}

// orig 0x19eb10
void CAcidGib::Precache()
{
	PRECACHE_MODEL("models/hgibs.mdl");
}

// orig 0x1a0150. Turns the gib toward a pro-magnet at its current speed: fully when the gib is
// above the magnet's feet, otherwise only in the horizontal. CAcidGib has no RC caps, so the
// magnet curse never picks it (curses.cpp).
void CAcidGib::ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce)
{
	if (pMagnet->Center().z + pMagnet->pev->mins.z < pev->origin.z)
	{
		pev->velocity = vecForce * pev->velocity.Length();
	}
	else
	{
		float flSpeed = pev->velocity.Length2D();
		pev->velocity.x = vecForce.x * flSpeed;
		pev->velocity.y = vecForce.y * flSpeed;
	}
}

// orig 0x1a0250
bool CAcidGib::ProMagEffected(CBaseEntity* pOther)
{
	if (!pev->owner)
		return false;
	return pOther->edict() != pev->owner;
}


LINK_ENTITY_TO_CLASS(weapon_glock, CGlock);		 // orig 0x19eb40
LINK_ENTITY_TO_CLASS(weapon_9mmhandgun, CGlock); // orig 0x19eb70

// orig 0x19eba0
void CGlock::Spawn()
{
	pev->classname = MAKE_STRING("weapon_9mmhandgun"); // hack to allow for old names
	Precache();
	m_iId = WEAPON_GLOCK;
	SET_MODEL(ENT(pev), "models/w_9mmhandgun.mdl");

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("GLOCK", "PickupAmmo", 20, 0, 250);

	FallInit(); // get ready to fall down.
}

// orig 0x19ec30. The silenced sounds (pl_gun1, pl_gun2) are not precached; nothing sets the
// silencer body.
void CGlock::Precache()
{
	PRECACHE_MODEL("models/v_9mmhandgun.mdl");
	PRECACHE_MODEL("models/w_9mmhandgun.mdl");
	PRECACHE_MODEL("models/p_9mmhandgun.mdl");

	m_iShell = PRECACHE_MODEL("models/shell.mdl"); // brass shell

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("items/9mmclip2.wav");

	PRECACHE_SOUND("weapons/pl_gun3.wav"); //handgun
	PRECACHE_SOUND("squeek/sqk_blast1.wav");
	PRECACHE_SOUND("ambience/biggun2.wav");
}

// orig 0x19ed00
bool CGlock::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("ITEMS", "MaxBullets", _9MM_MAX_CARRY, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = PREFS_KeyLoopUpIntClamp("GLOCK", "ClipSize", 20, 0, 250);
	p->iSlot = 1;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLOCK;
	p->iWeight = 9;

	return true;
}

// orig 0x19edc0
bool CGlock::Deploy()
{
	return DefaultDeploy("models/v_9mmhandgun.mdl", "models/p_9mmhandgun.mdl", GLOCK_DRAW, "onehanded", pev->body);
}

// orig 0x19ee00. The fart: two rounds for a blast behind the player that pushes it forward
// (under water just bubbles and noise).
void CGlock::SecondaryAttack()
{
	Vector vecOrigin = m_pPlayer->pev->origin;

	if (m_iClip < 2)
	{
		if (!m_fFireOnEmpty)
		{
			Reload();
			return;
		}

		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		m_flNextSecondaryAttack = gpGlobals->time + 0.15;
		return;
	}

	m_iClip -= 2;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	if (m_pPlayer->pev->waterlevel < 2)
	{
		vecOrigin = vecOrigin - gpGlobals->v_forward * 16;

		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "ambience/biggun2.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);

		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_EXPLOSION);
		WRITE_COORD(vecOrigin.x);
		WRITE_COORD(vecOrigin.y);
		WRITE_COORD(vecOrigin.z);
		WRITE_SHORT(g_sModelIndexFireball);
		WRITE_BYTE(5);	// scale * 10
		WRITE_BYTE(18); // framerate
		WRITE_BYTE(TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
		MESSAGE_END();

		m_pPlayer->SetAnimation(PLAYER_JUMP);
		m_pPlayer->pev->velocity = m_pPlayer->pev->velocity + gpGlobals->v_forward * PREFS_KeyLoopUpFloat("GLOCK", "FartPush", 500.0);

		m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("GLOCK", "FartRefire", 0.75) * m_pPlayer->m_fCurseRefireScale;
		m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("GLOCK", "FartRefire", 0.75) * m_pPlayer->m_fCurseRefireScale;
		m_flTimeWeaponIdle = gpGlobals->time + 5;

		// Original bug: the glock has no secondary ammo (index -1), so the original reads the
		// int before m_rgAmmo, which is m_pLastItem in its layout. That read is reproduced
		// rather than indexing m_rgAmmo[-1]. VERIFY
		bool bOut = m_iSecondaryAmmoType < 0 ? m_pPlayer->m_pLastItem == NULL : m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] == 0;
		if (bOut)
			m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

		m_pPlayer->pev->punchangle.x -= PREFS_KeyLoopUpInt("GLOCK", "FartAngleKick", 4);
		return;
	}

	Vector vecBehind = vecOrigin - gpGlobals->v_forward * 16;
	UTIL_Bubbles(vecBehind - Vector(10, 10, 10), vecBehind + Vector(10, 10, 10), 20);
	EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "ambience/biggun2.wav", 0.4, ATTN_NORM, 0, PITCH_NORM);

	// on the weapon's own edict, as the original does
	switch (RANDOM_LONG(0, 3))
	{
	case 0:
		EMIT_SOUND_DYN(edict(), CHAN_BODY, "player/pl_swim1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
		break;
	case 1:
		EMIT_SOUND_DYN(edict(), CHAN_BODY, "player/pl_swim2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
		break;
	case 2:
		EMIT_SOUND_DYN(edict(), CHAN_BODY, "player/pl_swim3.wav", 1, ATTN_NORM, 0, PITCH_NORM);
		break;
	case 3:
		EMIT_SOUND_DYN(edict(), CHAN_BODY, "player/pl_swim4.wav", 1, ATTN_NORM, 0, PITCH_NORM);
		break;
	}

	m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("GLOCK", "FartRefire", 0.75) * m_pPlayer->m_fCurseRefireScale;
	m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("GLOCK", "FartRefire", 0.75) * m_pPlayer->m_fCurseRefireScale;
}

// orig 0x19f3d0
void CGlock::PrimaryAttack()
{
	GlockFire(0.05234, 0.3, true);
}

// orig 0x19f410. Throws an acid gib from the gun along the view; flSpread is unused, flCycleTime
// is only PrimaryRefire's default.
void CGlock::GlockFire(float flSpread, float flCycleTime, bool fUseAutoAim)
{
	if (m_iClip <= 0)
	{
		if (m_fFireOnEmpty)
		{
			PlayEmptySound();
			// halflife-updated's fix, kept (Steven 2026-11-07): GetNextAttackDelay evens out the fire-rate creep
			m_flNextPrimaryAttack = GetNextAttackDelay(0.2);
		}

		return;
	}

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	SendWeaponAnim(m_iClip == 0 ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT);

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	Vector vecShellVelocity = m_pPlayer->pev->velocity + gpGlobals->v_right * RANDOM_FLOAT(50, 70);
	vecShellVelocity = vecShellVelocity + gpGlobals->v_up * RANDOM_FLOAT(100, 150) + gpGlobals->v_forward * 25;
	EjectBrass(pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_up * -12 + gpGlobals->v_forward * 32 + gpGlobals->v_right * 6, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL);

	if (pev->body == 1)
	{
		// silenced
		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND(m_pPlayer->edict(), CHAN_WEAPON, "weapons/pl_gun1.wav", RANDOM_FLOAT(0.9, 1.0), ATTN_NORM);
			break;
		case 1:
			EMIT_SOUND(m_pPlayer->edict(), CHAN_WEAPON, "weapons/pl_gun2.wav", RANDOM_FLOAT(0.9, 1.0), ATTN_NORM);
			break;
		}
	}
	else
	{
		// non-silenced
		m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

		int iPitch = 98 + RANDOM_LONG(0, 3);
		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/pl_gun3.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, iPitch);
	}

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming;

	// the aim is computed and unused (the gib flies along v_forward), as in the original
	if (fUseAutoAim)
	{
		vecAiming = m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);
	}
	else
	{
		vecAiming = gpGlobals->v_forward;
	}

	CAcidGib* pGib = CAcidGib::AcidGibCreate();
	UTIL_SetOrigin(pGib->pev, vecSrc);
	pGib->pev->velocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("GLOCK", "GibFireSpeed", 800.0);

	if (m_pPlayer->IsNetClient())
		pGib->pev->velocity.z = PREFS_KeyLoopUpFloat("GLOCK", "PrimaryZBoost", 100.0) + pGib->pev->velocity.z;

	pGib->pev->owner = m_pPlayer->edict();
	pGib->pev->enemy = pGib->pev->owner;

	// halflife-updated's fix, kept (Steven 2026-11-07): GetNextAttackDelay evens out the fire-rate creep
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(PREFS_KeyLoopUpFloat("GLOCK", "PrimaryRefire", flCycleTime) * m_pPlayer->m_fCurseRefireScale);

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);

	m_pPlayer->pev->punchangle.x -= PREFS_KeyLoopUpFloat("GLOCK", "PrimaryAngleKick", 2.0);
}

// orig 0x19fa50. Always the empty-clip reload animation.
void CGlock::Reload()
{
	if (DefaultReload(PREFS_KeyLoopUpIntClamp("GLOCK", "ClipSize", 20, 0, 250), GLOCK_RELOAD, 1.5))
	{
		m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15);
	}
}

// orig 0x19fad0
void CGlock::WeaponIdle()
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	// only idle if the slid isn't back
	if (m_iClip != 0)
	{
		int iAnim;
		float flRand = RANDOM_FLOAT(0.0, 1.0);

		if (flRand <= 0.3)
		{
			iAnim = GLOCK_IDLE3;
			m_flTimeWeaponIdle = gpGlobals->time + 49.0 / 16;
		}
		else if (flRand <= 0.6)
		{
			iAnim = GLOCK_IDLE1;
			m_flTimeWeaponIdle = gpGlobals->time + 60.0 / 16.0;
		}
		else
		{
			iAnim = GLOCK_IDLE2;
			m_flTimeWeaponIdle = gpGlobals->time + 40.0 / 16.0;
		}
		SendWeaponAnim(iAnim);
	}
}

// orig 0x19fc30. Strongly preferred against an enemy named "crossbow". VERIFY: which entity
// has that classname at runtime (the weapon itself is weapon_crossbow)
int CGlock::GetBotWeight(CBot* pBot, CBaseEntity* pEnemy)
{
	if (!pEnemy)
		return ItemInfoArray[m_iId].iWeight;
	if (FClassnameIs(pEnemy->pev, "crossbow"))
		return 7000;
	if (BotFireSecondary(pBot, pEnemy))
		return 40;
	return 10;
}

// orig 0x19fce0. Reload below two rounds; otherwise, once the fire gate allows, fart (jumping
// and ducking) and/or fire. The original computes an aim offset and a point 512 short of the
// enemy along the way and uses neither; the GetBotShootOrg calls behind them are kept.
void CGlock::BotWeaponFrame(CBot* pBot, CBaseEntity* pEnemy)
{
	if (!pEnemy) // VERIFY: guard, the original crashes here (it calls through pEnemy at once)
		return;

	pEnemy->GetBotShootOrg();

	if (m_iClip <= 1)
	{
		RCBotPev(pBot)->button |= IN_RELOAD;
		return;
	}

	pEnemy->GetBotShootOrg();

	if (!(RCBotFireGate(pBot) / 3.0 >= 999.0))
		return;

	if (BotFireSecondary(pBot, pEnemy))
	{
		RCBotPev(pBot)->button |= IN_ATTACK2 | IN_DUCK | IN_JUMP;
		Vector vecDir = (pEnemy->GetBotShootOrg() - Center()).Normalize();
		Vector vecUnused = pEnemy->GetBotShootOrg() - vecDir * 512;
	}

	if (BotFirePrimary(pBot, pEnemy))
	{
		RCBotPev(pBot)->button |= IN_ATTACK;
		pEnemy->GetBotShootOrg();
	}
}

// orig 0x19ff40. With an enemy: only past 768, with two rounds, and not in bot state 1.
bool CGlock::BotFireSecondary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (pEnemy)
	{
		float flDist = (pEnemy->Center() - RCBotCenter(pBot)).Length();
		if (m_iClip + m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 2 || RCBotState(pBot) == 1 || !(768 < flDist))
			return false;
	}
	return true;
}

// orig 0x1a0010. With an enemy: inside 768, or at any range against a "crossbow".
bool CGlock::BotFirePrimary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (pEnemy)
	{
		float flDist = (pEnemy->Center() - RCBotCenter(pBot)).Length();
		if (!FClassnameIs(pEnemy->pev, "crossbow") && !(768 > flDist))
			return false;
	}
	return true;
}


class CGlockAmmo : public CBasePlayerAmmo
{
	// orig 0x24ccd0
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_9mmclip.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x24cc90
	void Precache() override
	{
		PRECACHE_MODEL("models/w_9mmclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x24cbe0
	bool AddAmmo(CBaseEntity* pOther) override
	{
		int iMax = PREFS_KeyLoopUpIntClamp("ITEMS", "MaxBullets", _9MM_MAX_CARRY, 0, 250);
		if (pOther->GiveAmmo(PREFS_KeyLoopUpIntClamp("GLOCK", "PickupClipGive", 20, 0, 250), "9mm", iMax) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_glockclip, CGlockAmmo); // orig 0x19fbd0
LINK_ENTITY_TO_CLASS(ammo_9mmclip, CGlockAmmo);	  // orig 0x19fc00
