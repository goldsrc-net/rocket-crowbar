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

// Rocket Crowbar 1.9: the MP5's primary fires a short lightning beam that forks into sub beams
// when it hits nothing; its secondary (the "fart") spends an AR grenade on a blast out of the
// back that throws the shooter forward and hurts whatever is right behind. Numbers come from
// rc.ini's [MP5]. No events: effects are server-side, as in the 2001 SDK RC was built on.
// Reconstructed from rc_i386.so (file addresses, reference/funcfile.tsv); see
// reference/dossiers/w2-python-mp5-glock-weapons.md.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"
#include "decals.h"
#include "UserMessages.h"
#include "Preff.h"
#include "rcbot.h"

// Sprite indexes from CMP5::Precache. The two flares are precached and never drawn.
int MP5_Red_Ball;
int MP5_Blue_Ball;
int iMP5_BeamSpr;

LINK_ENTITY_TO_CLASS(weapon_mp5, CMP5);	  // orig 0x1cbd00
LINK_ENTITY_TO_CLASS(weapon_9mmAR, CMP5); // orig 0x1cbd30

// orig 0x1cbd60
int CMP5::SecondaryAmmoIndex()
{
	return m_iSecondaryAmmoType;
}

// orig 0x1cbd70
void CMP5::Spawn()
{
	pev->classname = MAKE_STRING("weapon_9mmAR"); // hack to allow for old names
	Precache();
	SET_MODEL(ENT(pev), "models/w_9mmAR.mdl");
	m_iId = WEAPON_MP5;

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("MP5", "PickupAmmo", 50, 0, 250);

	FallInit(); // get ready to fall down.

	m_flSpawnTime = gpGlobals->time;
}

// orig 0x1cbe10
void CMP5::Precache()
{
	PRECACHE_MODEL("models/v_9mmAR.mdl");
	PRECACHE_MODEL("models/w_9mmAR.mdl");
	PRECACHE_MODEL("models/p_9mmAR.mdl");

	m_iShell = PRECACHE_MODEL("models/shell.mdl"); // brass shellTE_MODEL

	PRECACHE_MODEL("models/grenade.mdl"); // grenade

	PRECACHE_MODEL("models/w_9mmARclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("items/clipinsert1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	PRECACHE_SOUND("weapons/glauncher.wav");
	PRECACHE_SOUND("weapons/glauncher2.wav");

	PRECACHE_SOUND("weapons/357_cock1.wav");

	PRECACHE_SOUND("debris/beamstart11.wav");
	PRECACHE_SOUND("ambience/biggun3.wav");

	MP5_Red_Ball = PRECACHE_MODEL("sprites/redflare1.spr");
	MP5_Blue_Ball = PRECACHE_MODEL("sprites/blueflare1.spr");
	iMP5_BeamSpr = PRECACHE_MODEL("sprites/lgtning.spr");
	PRECACHE_MODEL("sprites/blueflare1.spr");
}

// orig 0x1cbf90
bool CMP5::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("ITEMS", "MaxBullets", _9MM_MAX_CARRY, 0, 250);
	p->pszAmmo2 = "ARgrenades";
	p->iMaxAmmo2 = 15; // not the [MP5] MaxGrenades pref the grenade pickup uses
	p->iMaxClip = PREFS_KeyLoopUpIntClamp("MP5", "ClipSize", MP5_MAX_CLIP, 0, 250);
	p->iSlot = 2;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_MP5;
	p->iWeight = 20;

	return true;
}

// orig 0x1cc050. The original sends WeapPickup here once the weapon is added. In this tree
// CBasePlayer::AddPlayerItem sends it for every weapon, so this only adds the weapon.
void CMP5::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	m_iBeamState = 0;
	// As the original: this weapon tells the player it was picked up (AddPlayerItem doesn't)
	MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
	WRITE_BYTE(m_iId);
	MESSAGE_END();
}

// orig 0x1cc0e0
bool CMP5::Deploy()
{
	return DefaultDeploy("models/v_9mmAR.mdl", "models/p_9mmAR.mdl", MP5_DEPLOY, "mp5");
}

// orig 0x1cc120. Whether pEntity lies within SubBeamDOTFov of the aim direction, seen from vecSrc.
bool CMP5::IsGoodTarget(Vector vecSrc, Vector vecAngles, CBaseEntity* pEntity)
{
	UTIL_MakeVectors(vecAngles);
	Vector vecDir = (pEntity->Center() - vecSrc).Normalize();
	float flDot = DotProduct(vecDir, gpGlobals->v_forward);
	return PREFS_KeyLoopUpFloat("MP5", "SubBeamDOTFov", 0.9) < flDot;
}

// orig 0x1cc250
void CMP5::DoBeamEffects(Vector vecStart, Vector vecEnd, int iWidth)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecStart);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(vecStart.x);
	WRITE_COORD(vecStart.y);
	WRITE_COORD(vecStart.z);
	WRITE_COORD(vecEnd.x);
	WRITE_COORD(vecEnd.y);
	WRITE_COORD(vecEnd.z);
	WRITE_SHORT(iMP5_BeamSpr);
	WRITE_BYTE(0);	// starting frame
	WRITE_BYTE(50); // framerate
	WRITE_BYTE(1);	// life
	WRITE_BYTE(iWidth);
	WRITE_BYTE(10); // noise
	WRITE_BYTE(128);
	WRITE_BYTE(128);
	WRITE_BYTE(255);
	WRITE_BYTE(255); // brightness
	WRITE_BYTE(50);	 // scroll speed
	MESSAGE_END();
}

// orig 0x1cc450. The main beam runs MainBeamRange from vecSrc (drawn from vecBeamStart). If it
// hits something, that takes the damage. If it hits nothing, up to NumSubBeams sub beams fork
// from its end to damageable things in SubBeamRange and in front; any left over go out at
// random within 10 degrees, 384 units, and only mark walls.
void CMP5::FireBeam(Vector vecSrc, Vector vecAngles, Vector vecBeamStart)
{
	CBaseEntity* pEntity = NULL;
	int iBeams = 0;
	TraceResult tr;

	UTIL_MakeVectors(vecAngles);
	Vector vecDir = gpGlobals->v_forward;
	Vector vecEnd = vecSrc + gpGlobals->v_forward * PREFS_KeyLoopUpFloat("MP5", "MainBeamRange", 512.0);

	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr);
	Vector vecHit = tr.vecEndPos;

	DoBeamEffects(vecBeamStart, vecHit, 20);

	if (tr.flFraction != 1.0)
	{
		if (!tr.pHit || CBaseEntity::Instance(tr.pHit)->IsBSPModel())
		{
			// the world (Instance(NULL) is worldspawn) and brush entities
			CBaseEntity::Instance(tr.pHit)->TakeDamage(pev, m_pPlayer->pev, PREFS_KeyLoopUpFloat("MP5", "MainBeamDmgPerSec", 180.0) / 10, DMG_ENERGYBEAM);

			if (CVAR_GET_FLOAT("sv_lowlag") != 1)
			{
				UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));
				UTIL_Sparks(tr.vecEndPos);
			}
		}
		else if (tr.pHit && tr.pHit->v.takedamage != DAMAGE_NO)
		{
			UTIL_BloodDrips(vecHit, g_vecZero, CBaseEntity::Instance(tr.pHit)->BloodColor(), (int)PREFS_KeyLoopUpFloat("MP5", "MainBeamDmgPerSec", 180.0));

			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			ClearMultiDamage();
			pHit->TraceAttack(m_pPlayer->pev, PREFS_KeyLoopUpFloat("MP5", "MainBeamDmgPerSec", 180.0) / 10, (tr.vecEndPos - vecSrc).Normalize(), &tr, DMG_ENERGYBEAM);
			ApplyMultiDamage(pev, m_pPlayer->pev);
		}
		return;
	}

	const int iNumSubBeams = (int)PREFS_KeyLoopUpFloat("MP5", "NumSubBeams", 3.0);

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecHit, PREFS_KeyLoopUpFloat("MP5", "SubBeamRange", 384.0))) != NULL)
	{
		if (iBeams >= iNumSubBeams)
			continue;
		if (pEntity->pev->takedamage == DAMAGE_NO)
			continue;
		if (pEntity == m_pPlayer || pEntity->pev == pev)
			continue;
		if (!IsGoodTarget(vecHit, vecAngles, pEntity))
			continue;

		UTIL_TraceLine(vecHit, pEntity->Center(), dont_ignore_monsters, m_pPlayer->edict(), &tr);
		if (tr.flFraction != 1.0 && tr.pHit != pEntity->edict())
			continue;

		UTIL_BloodDrips(tr.vecEndPos, g_vecZero, pEntity->BloodColor(), (int)(PREFS_KeyLoopUpFloat("MP5", "SubBeamDmgPerSec", 130.0) / 5));

		// Original oddity, kept: a sub beam does the main beam's damage; SubBeamDmgPerSec only
		// sizes the blood.
		ClearMultiDamage();
		pEntity->TraceAttack(m_pPlayer->pev, PREFS_KeyLoopUpFloat("MP5", "MainBeamDmgPerSec", 180.0) / 10, (tr.vecEndPos - vecHit).Normalize(), &tr, DMG_ENERGYBEAM);
		ApplyMultiDamage(pev, m_pPlayer->pev);

		DoBeamEffects(vecHit, tr.vecEndPos, 15);
		iBeams++;
	}

	UTIL_MakeVectors(vecAngles);

	if (iBeams >= iNumSubBeams - 1)
		return;

	for (int i = iBeams; i < iNumSubBeams; i++)
	{
		// FireBullets' spread, at VECTOR_CONE_10DEGREES
		float x, y, z;
		do
		{
			x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
			y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
			z = x * x + y * y;
		} while (z > 1);

		Vector vecShot = vecDir + x * VECTOR_CONE_10DEGREES.x * gpGlobals->v_up + y * VECTOR_CONE_10DEGREES.y * gpGlobals->v_right;
		Vector vecSubEnd = vecHit + vecShot * 384;

		UTIL_TraceLine(vecHit, vecSubEnd, dont_ignore_monsters, m_pPlayer->edict(), &tr);
		// the original also looks up CBaseEntity::Instance(tr.pHit) here and ignores it

		if (tr.flFraction != 1.0 && CVAR_GET_FLOAT("sv_lowlag") != 1)
		{
			UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));
			UTIL_Sparks(tr.vecEndPos);
		}

		DoBeamEffects(vecHit, tr.vecEndPos, 15);
	}
}

// orig 0x1cceb0
void CMP5::PrimaryAttack()
{
	// don't fire underwater, or empty
	if (m_pPlayer->pev->waterlevel == 3 || m_iClip <= 0)
	{
		PlayEmptySound();

		if (m_iBeamState == 1)
		{
			m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
			m_flBeamStateTime = gpGlobals->time;
			m_iBeamState = 0;
		}

		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	SendWeaponAnim(MP5_FIRE1 + RANDOM_LONG(0, 2));
	m_flNextAnimTime = gpGlobals->time + 0.2;

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "debris/beamstart11.wav", 1.0, ATTN_NORM, 0, 94 + RANDOM_LONG(0, 15));

	if (m_iBeamState == 0)
	{
		m_flBeamStateTime = gpGlobals->time + 1.0;
		m_iBeamState = 1;
	}
	else if (m_flBeamStateTime < gpGlobals->time)
	{
		m_flBeamStateTime = gpGlobals->time + 7.534;
		m_iBeamState = 1;
	}

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	// The stock shell velocity, left behind when RC dropped the EjectBrass call: computed and
	// never used. The RANDOM_FLOAT calls stay for the random sequence.
	Vector vecShellVelocity = m_pPlayer->pev->velocity + gpGlobals->v_right * RANDOM_FLOAT(50, 70);
	vecShellVelocity = vecShellVelocity + gpGlobals->v_up * RANDOM_FLOAT(100, 150) + gpGlobals->v_forward * 25;

	Vector vecSrc = m_pPlayer->GetGunPosition();
	m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES); // result unused, as in the original
	Vector vecBeamStart = vecSrc + gpGlobals->v_right * 2.5 + gpGlobals->v_forward * 15 - gpGlobals->v_up * 4;

	FireBeam(vecSrc, m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle, vecBeamStart);

	if (0 == m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);

	// halflife-updated's fix, kept (Steven 2026-11-07): GetNextAttackDelay evens out the fire-rate creep
	m_flNextPrimaryAttack = GetNextAttackDelay(0.1 * m_pPlayer->m_fCurseRefireScale);
	m_flNextSecondaryAttack = gpGlobals->time + (float)PREFS_KeyLoopUpInt("MP5", "PrimaryFartRefire", 0) * m_pPlayer->m_fCurseRefireScale;
	m_flTimeWeaponIdle = gpGlobals->time + 0.1;

	// Original bug, kept: both bounds are -PrimaryAngleKick, so the kick is always exactly that
	m_pPlayer->pev->punchangle.x = RANDOM_FLOAT(-PREFS_KeyLoopUpInt("MP5", "PrimaryAngleKick", 1), -PREFS_KeyLoopUpInt("MP5", "PrimaryAngleKick", 1));
	m_pPlayer->pev->punchangle.y = RANDOM_FLOAT(-PREFS_KeyLoopUpInt("MP5", "PrimaryAngleKick", 1), -PREFS_KeyLoopUpInt("MP5", "PrimaryAngleKick", 1));
}

// Hurt whatever the trace from vecSrc to vecEnd hits (the fart's kill zone)
static void MP5FartTrace(CMP5* pWeapon, const Vector& vecSrc, const Vector& vecEnd, float flDamage)
{
	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, pWeapon->m_pPlayer->edict(), &tr);

	if (tr.pHit && tr.pHit->v.takedamage != DAMAGE_NO)
		CBaseEntity::Instance(tr.pHit)->TakeDamage(pWeapon->pev, pWeapon->m_pPlayer->pev, flDamage, DMG_NERVEGAS | DMG_NEVERGIB);
}

// orig 0x1cd4d0. The fart: under water a weak push and bubbles; in air an explosion behind the
// player, FartKillDamage to whatever five short traces backward hit, and a FartVelo push
// forward (with FartStaticVert, flat along the yaw and FartStaticVertVelo straight up).
void CMP5::SecondaryAttack()
{
	Vector vecOrigin = m_pPlayer->pev->origin;
	float flScale = 1.0;

	Vector vecAngles = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	if (PREFS_KeyLoopUpBool("MP5", "FartStaticVert", true))
	{
		vecAngles.x = 0;
		vecAngles.z = 0;
	}
	UTIL_MakeVectors(vecAngles);

	// halflife-updated's fix, kept (Steven 2026-11-07): GetNextAttackDelay evens out the fire-rate creep
	m_flNextPrimaryAttack = GetNextAttackDelay(PREFS_KeyLoopUpFloat("MP5", "FartRefire", 1.0) * m_pPlayer->m_fCurseRefireScale);
	m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("MP5", "FartRefire", 1.0) * m_pPlayer->m_fCurseRefireScale;
	m_flTimeWeaponIdle = gpGlobals->time + 5;

	if (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] == 0)
	{
		PlayEmptySound();
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);
		return;
	}

	m_pPlayer->pev->punchangle.x -= PREFS_KeyLoopUpInt("MP5", "FartAngleKick", 10);
	m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType]--;

	if (m_pPlayer->pev->waterlevel > 1)
	{
		flScale = 0.1;

		Vector vecBehind = vecOrigin - gpGlobals->v_forward * 16;
		UTIL_Bubbles(vecBehind - Vector(10, 10, 10), vecBehind + Vector(10, 10, 10), 20);
		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "ambience/biggun3.wav", 0.2, ATTN_NORM, 0, PITCH_NORM);

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
	}
	else
	{
		vecOrigin = vecOrigin - gpGlobals->v_forward * 16;

		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "ambience/biggun3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM);

		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_EXPLOSION);
		WRITE_COORD(vecOrigin.x);
		WRITE_COORD(vecOrigin.y);
		WRITE_COORD(vecOrigin.z);
		WRITE_SHORT(g_sModelIndexFireball);
		WRITE_BYTE(10); // scale * 10
		WRITE_BYTE(18); // framerate
		WRITE_BYTE(TE_EXPLFLAG_NOSOUND);
		MESSAGE_END();

		const float flLen = PREFS_KeyLoopUpFloat("MP5", "FartKillLen", 48.0);
		const float flDamage = PREFS_KeyLoopUpFloat("MP5", "FartKillDamage", 3000.0);

		MP5FartTrace(this, vecOrigin, vecOrigin - gpGlobals->v_forward * flLen, flDamage);
		MP5FartTrace(this, vecOrigin, vecOrigin - gpGlobals->v_forward * flLen + gpGlobals->v_right * 16, flDamage);
		MP5FartTrace(this, vecOrigin, vecOrigin - gpGlobals->v_forward * flLen - gpGlobals->v_right * 16, flDamage);
		MP5FartTrace(this, vecOrigin, vecOrigin - gpGlobals->v_forward * flLen + gpGlobals->v_up * 16, flDamage);
		MP5FartTrace(this, vecOrigin, vecOrigin - gpGlobals->v_forward * flLen - gpGlobals->v_up * 16, flDamage);
	}

	m_pPlayer->SetAnimation(PLAYER_SUPERJUMP);

	if (PREFS_KeyLoopUpBool("MP5", "FartStaticVert", true))
	{
		const int iOldZ = (int)m_pPlayer->pev->velocity.z;
		m_pPlayer->pev->velocity = m_pPlayer->pev->velocity + flScale * (gpGlobals->v_forward * PREFS_KeyLoopUpFloat("MP5", "FartVelo", 2000.0));
		m_pPlayer->pev->velocity.z = PREFS_KeyLoopUpFloat("MP5", "FartStaticVertVelo", 300.0) * flScale + iOldZ;
	}
	else
	{
		m_pPlayer->pev->velocity = m_pPlayer->pev->velocity + flScale * (gpGlobals->v_forward * PREFS_KeyLoopUpFloat("MP5", "FartVelo", 2000.0));
	}
}

// orig 0x1ce3a0
void CMP5::Reload()
{
	DefaultReload(PREFS_KeyLoopUpIntClamp("MP5", "ClipSize", MP5_MAX_CLIP, 0, 250), MP5_RELOAD, 1.5);
}

// orig 0x1ce3f0
void CMP5::WeaponIdle()
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_iBeamState == 1)
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_flBeamStateTime = gpGlobals->time;
		m_iBeamState = 0;
	}

	SendWeaponAnim(RANDOM_LONG(0, 1) != 0 ? MP5_IDLE1 : MP5_LONGIDLE);

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT(10, 15); // how long till we do this again.
}

// orig 0x1ce5c0
int CMP5::GetBotWeight(CBot* pBot, CBaseEntity* pEnemy)
{
	if (!pEnemy)
		return ItemInfoArray[m_iId].iWeight;
	if (BotFirePrimary(pBot, pEnemy))
		return 5000;
	if (BotFireSecondary(pBot, pEnemy))
		return 200;
	return 100;
}

// orig 0x1ce640. The secondary is fired jumping and ducking; past 896 the bot also walks forward.
int CMP5::BotFireButton(CBot* pBot, CBaseEntity* pEnemy)
{
	if (BotFireSecondary(pBot, pEnemy))
		return IN_ATTACK2 | IN_DUCK | IN_JUMP;

	float flDist = (pEnemy->Center() - RCBotCenter(pBot)).Length();
	if (896 < flDist)
		RCBotPev(pBot)->button |= IN_FORWARD;

	if (BotFirePrimary(pBot, pEnemy))
		return IN_ATTACK;
	return 0;
}

// orig 0x1ce730. With an enemy: only past 896, with grenades, and not in bot state 1.
bool CMP5::BotFireSecondary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (pEnemy)
	{
		float flDist = (pEnemy->Center() - RCBotCenter(pBot)).Length();
		if (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] == 0 || RCBotState(pBot) == 1 || !(896 < flDist))
			return false;
	}
	return true;
}

// orig 0x1ce800. With an enemy: only with a loaded clip and inside 832.
bool CMP5::BotFirePrimary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (pEnemy)
	{
		float flDist = (pEnemy->Center() - RCBotCenter(pBot)).Length();
		if (m_iClip == 0 || (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0 && m_iClip == 0) || !(832 > flDist))
			return false;
	}
	return true;
}


class CMP5AmmoClip : public CBasePlayerAmmo
{
	// orig 0x250740
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_9mmARclip.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x250700
	void Precache() override
	{
		PRECACHE_MODEL("models/w_9mmARclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x250650
	bool AddAmmo(CBaseEntity* pOther) override
	{
		int iMax = PREFS_KeyLoopUpIntClamp("ITEMS", "MaxBullets", _9MM_MAX_CARRY, 0, 250);
		bool bResult = (pOther->GiveAmmo(PREFS_KeyLoopUpIntClamp("MP5", "PickupClipGive", AMMO_MP5CLIP_GIVE, 0, 250), "9mm", iMax) != -1);
		if (bResult)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
		return bResult;
	}
};
LINK_ENTITY_TO_CLASS(ammo_mp5clip, CMP5AmmoClip); // orig 0x1ce4d0
LINK_ENTITY_TO_CLASS(ammo_9mmAR, CMP5AmmoClip);	  // orig 0x1ce500



class CMP5Chainammo : public CBasePlayerAmmo
{
	// orig 0x2505b0
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_chainammo.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x250570
	void Precache() override
	{
		PRECACHE_MODEL("models/w_chainammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x2504b0
	bool AddAmmo(CBaseEntity* pOther) override
	{
		int iMax = PREFS_KeyLoopUpIntClamp("ITEMS", "MaxBullets", _9MM_MAX_CARRY, 0, 250);
		bool bResult = (pOther->GiveAmmo(PREFS_KeyLoopUpIntClamp("MP5", "BoxGive", AMMO_CHAINBOX_GIVE, 0, 250), "9mm", iMax) != -1);
		if (bResult)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
		return bResult;
	}
};
LINK_ENTITY_TO_CLASS(ammo_9mmbox, CMP5Chainammo); // orig 0x1ce530


class CMP5AmmoGrenade : public CBasePlayerAmmo
{
	// orig 0x250410
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_ARgrenade.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x2503d0
	void Precache() override
	{
		PRECACHE_MODEL("models/w_ARgrenade.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x250320
	bool AddAmmo(CBaseEntity* pOther) override
	{
		int iMax = PREFS_KeyLoopUpIntClamp("MP5", "MaxGrenades", 15, 0, 250);
		bool bResult = (pOther->GiveAmmo(PREFS_KeyLoopUpIntClamp("MP5", "GrenadesGive", AMMO_M203BOX_GIVE, 0, 250), "ARgrenades", iMax) != -1);

		if (bResult)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
		return bResult;
	}
};
LINK_ENTITY_TO_CLASS(ammo_mp5grenades, CMP5AmmoGrenade); // orig 0x1ce560
LINK_ENTITY_TO_CLASS(ammo_ARgrenades, CMP5AmmoGrenade);	 // orig 0x1ce590
