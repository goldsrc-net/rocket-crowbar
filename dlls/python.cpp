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

// Rocket Crowbar 1.9: the python fires CPythRound rockets instead of bullets (the primary one,
// the secondary six, four of them swinging around the centre one), both push the shooter back,
// and its numbers come from rc.ini's [PYTHON]. No zoom and no events: effects are server-side,
// as in the 2001 SDK RC was built on. Reconstructed from rc_i386.so (file addresses,
// reference/funcfile.tsv); see reference/dossiers/w2-python-mp5-glock-weapons.md.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "monsters.h"
#include "player.h"
#include "gamerules.h"
#include "UserMessages.h"
#include "Preff.h"
#include "rcbot.h"

// The python's round: a small rocket (rpgrocket.mdl) with a laser trail and a blue flare that
// explodes on touch. A sub round (types 1-4) swings around its line of flight along one axis.
class CPythRound : public CGrenade
{
public:
	void Spawn() override;
	void EXPORT PFlareThink();
	void EXPORT PTouch(CBaseEntity* pOther);
	void EXPORT PRndThink();
	void ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce) override;
	virtual bool ProMagEffected(CBaseEntity* pOther);

	// iType: 0 straight; 1 swings along v_right, 2 along -v_right, 3 along v_up, 4 along -v_up
	static CPythRound* CreateRnd(Vector vecOrigin, Vector vecVelocity, edict_t* pOwner, int iType);

	// Unsaved, as in the original (it has no save table). Names chosen from use.
	Vector m_vecDir;	// orig +0x280, unit direction of flight. VERIFY name
	Vector m_vecSwing;	// orig +0x28c, the axis a sub round swings along. VERIFY name
	float m_flSwing;	// orig +0x298, the swing's phase. VERIFY name
	float m_flLastTime; // orig +0x29c, time of the last PRndThink. VERIFY name
	float m_flSpeed;	// orig +0x2a0. VERIFY name
	bool m_bSubRound;	// orig +0x2a4, true for types 1-4. VERIFY name
	int m_iType;		// orig +0x2a8. VERIFY name
};

LINK_ENTITY_TO_CLASS(PythRound, CPythRound); // orig 0x1f86b0

// orig 0x1f86e0. The flare's think: CPythRound::Spawn gives it to a plain CBaseEntity, which
// works because it only touches pev. The flare goes once its round has exploded (dmgtime -1)
// or is gone, or after its 30 seconds (pev->dmg).
void CPythRound::PFlareThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->aiment && !FNullEnt(pev->aiment) && pev->aiment->v.dmgtime != -1 && FClassnameIs(pev->aiment, "weapon_357") && !(pev->dmg < gpGlobals->time))
		return;

	SetThink(&CPythRound::SUB_Remove);
}

// orig 0x1f87f0
CPythRound* CPythRound::CreateRnd(Vector vecOrigin, Vector vecVelocity, edict_t* pOwner, int iType)
{
	CPythRound* pRnd = GetClassPtr((CPythRound*)NULL);

	UTIL_MakeVectors(UTIL_VecToAngles(vecVelocity));
	UTIL_SetOrigin(pRnd->pev, vecOrigin);
	pRnd->pev->velocity = vecVelocity;
	pRnd->m_vecDir = vecVelocity.Normalize();
	pRnd->m_flSpeed = vecVelocity.Length();
	pRnd->m_bSubRound = true;
	pRnd->m_iType = iType;

	switch (iType)
	{
	case 1:
		pRnd->m_vecSwing = gpGlobals->v_right;
		break;
	case 2:
		pRnd->m_vecSwing = -gpGlobals->v_right;
		break;
	case 3:
		pRnd->m_vecSwing = gpGlobals->v_up;
		break;
	case 4:
		pRnd->m_vecSwing = -gpGlobals->v_up;
		break;
	default:
		pRnd->m_vecSwing = gpGlobals->v_forward;
		pRnd->m_bSubRound = false;
		break;
	}

	pRnd->pev->angles = UTIL_VecToAngles(vecVelocity);
	pRnd->pev->owner = pOwner;
	pRnd->Spawn();
	return pRnd;
}

// orig 0x1f8a50
void CPythRound::Spawn()
{
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);
	pev->classname = MAKE_STRING("weapon_357"); // the flare and PTouch test for it

	SetTouch(&CPythRound::PTouch);
	SetThink(&CPythRound::PRndThink);
	pev->nextthink = gpGlobals->time + 0.1;

	UTIL_MakeVectors(pev->angles);
	pev->dmg = 60;
	pev->effects = 0;
	m_flSwing = 0;
	m_flLastTime = gpGlobals->time;
	pev->view_ofs = g_vecZero;
	m_hEnemy = NULL;
	pev->rendermode = kRenderTransColor;
	pev->renderamt = 0;
	pev->gravity = 1.0;
	pev->health = 0;
	pev->frame = 0;
	pev->dmgtime = 1.0; // PTouch sets -1, which ends the flare

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(5); // life
	WRITE_BYTE(2); // width
	WRITE_BYTE(128);
	WRITE_BYTE(128);
	WRITE_BYTE(255);
	WRITE_BYTE(128); // brightness
	MESSAGE_END();

	CBaseEntity* pFlare = GetClassPtr((CBaseEntity*)NULL);
	pFlare->Spawn();
	pFlare->pev->classname = MAKE_STRING("pythround_flare");
	pFlare->pev->movetype = MOVETYPE_FOLLOW;
	pFlare->pev->solid = SOLID_NOT;
	pFlare->pev->renderamt = 128;
	pFlare->pev->rendermode = kRenderTransAdd;
	pFlare->pev->renderfx = kRenderFxNone;
	pFlare->pev->scale = 0.4;
	pFlare->SetThink(static_cast<void (CBaseEntity::*)()>(&CPythRound::PFlareThink));
	pFlare->pev->nextthink = gpGlobals->time + 0.1;
	pFlare->pev->dmg = gpGlobals->time + 30; // the flare's own lifetime
	SET_MODEL(pFlare->edict(), "sprites/blueflare1.spr");
	UTIL_SetOrigin(pFlare->pev, pev->origin);
	pFlare->pev->aiment = edict();
}

// orig 0x1f8de0. Explodes: RadiusDamage's loop written out, with a radius of 2.7 times the
// damage and each victim's damage scaled by its VeryHeavyDamageFract().
void CPythRound::PTouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;

	// other rounds (and dropped pythons) are "weapon_357" too
	if (FClassnameIs(pOther->pev, "weapon_357"))
		return;

	CBaseEntity* pFlare = NULL;
	while ((pFlare = UTIL_FindEntityByClassname(pFlare, "pythround_flare")) != NULL)
	{
		if (FNullEnt(pFlare->edict()) || !pFlare->pev->aiment)
			continue;

		if (pFlare->pev->aiment == edict() || FNullEnt(pFlare->pev->aiment))
		{
			pFlare->SetThink(&CBaseEntity::SUB_Remove);
			pFlare->pev->nextthink = gpGlobals->time + 0.1;
		}
	}

	int iContents = UTIL_PointContents(pev->origin);

	if (!m_bSubRound)
		pev->dmg = PREFS_KeyLoopUpFloat("PYTHON", "CenterDamage", 80.0);
	else
		pev->dmg = PREFS_KeyLoopUpFloat("PYTHON", "SubDamage", 60.0);

	pev->dmgtime = -1;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(iContents == CONTENTS_WATER ? g_sModelIndexWExplosion : g_sModelIndexFireball);
	WRITE_BYTE(16); // scale * 10
	WRITE_BYTE(18); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	entvars_t* pevOwner;
	if (pev->owner)
		pevOwner = VARS(pev->owner);
	else
		pevOwner = pev;

	pev->owner = NULL;

	const float flDmg = pev->dmg;
	const bool bInWater = UTIL_PointContents(pev->origin) == CONTENTS_WATER;
	CBaseEntity* pEntity = NULL;
	TraceResult tr;

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, pev->dmg * 2.7)) != NULL)
	{
		if (pEntity->pev->takedamage == DAMAGE_NO)
			continue;

		// blasts don't travel into or out of water
		if (bInWater && pEntity->pev->waterlevel == 0)
			continue;
		if (!bInWater && pEntity->pev->waterlevel == 3)
			continue;

		Vector vecSpot = pEntity->BodyTarget(pev->origin);
		UTIL_TraceLine(pev->origin, vecSpot, dont_ignore_monsters, ENT(pev), &tr);

		if (tr.flFraction != 1.0 && tr.pHit != pEntity->edict())
			continue;

		if (0 != tr.fStartSolid)
		{
			tr.vecEndPos = pev->origin;
			tr.flFraction = 0.0;
		}

		float flAdjust = (pev->origin - tr.vecEndPos).Length() * (flDmg / (flDmg * 2.7));
		float flDamage = pEntity->VeryHeavyDamageFract() * (pev->dmg - flAdjust);

		if (flDamage < 0)
			continue;

		if (tr.flFraction == 1.0)
		{
			pEntity->TakeDamage(pev, pevOwner, flDamage, DMG_BLAST);
		}
		else
		{
			ClearMultiDamage();
			pEntity->TraceAttack(pev, flDamage, (tr.vecEndPos - pev->origin).Normalize(), &tr, DMG_BLAST);
			ApplyMultiDamage(pev, pevOwner);
		}
	}

	SetTouch(NULL);
	SetThink(NULL);
	pev->nextthink = gpGlobals->time + 0.01;
	pev->effects |= EF_NODRAW;
	UTIL_Remove(this);
}

// orig 0x1f93d0. A straight round holds its velocity. A sub round flies m_vecSwing *
// 1300 sin(phase) / pi plus enough of m_vecDir to make up m_flSpeed.
void CPythRound::PRndThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_bSubRound)
	{
		pev->velocity = m_vecDir * m_flSpeed;
		return;
	}

	float flSwing = (1300.0 * sin(m_flSwing)) / M_PI;
	m_flSwing = (gpGlobals->time - m_flLastTime) * PREFS_KeyLoopUpFloat("PYTHON", "SecondaryDeviationFactor", 1300.0) + m_flSwing;

	pev->velocity = m_vecSwing * flSwing;
	pev->velocity = pev->velocity + m_vecDir * (m_flSpeed - pev->velocity.Length());
	m_flLastTime = gpGlobals->time;
}

// orig 0x1fae80. Steers the round toward a pro-magnet. Original bug, kept: m_vecDir becomes
// vecForce * m_flSpeed, not a unit vector, so PRndThink then flies it at speed squared. Moot in
// practice: CPythRound has no RC caps, so the magnet curse never picks it (curses.cpp).
void CPythRound::ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce)
{
	m_vecDir = vecForce * m_flSpeed;
	UTIL_MakeVectors(UTIL_VecToAngles(m_vecDir));

	switch (m_iType)
	{
	case 1:
		m_vecSwing = gpGlobals->v_right;
		break;
	case 2:
		m_vecSwing = -gpGlobals->v_right;
		break;
	case 3:
		m_vecSwing = gpGlobals->v_up;
		break;
	case 4:
		m_vecSwing = -gpGlobals->v_up;
		break;
	default:
		m_vecSwing = gpGlobals->v_forward;
		break;
	}
}

// orig 0x1fafe0
bool CPythRound::ProMagEffected(CBaseEntity* pOther)
{
	return pOther->edict() != pev->owner;
}


LINK_ENTITY_TO_CLASS(weapon_python, CPython); // orig 0x1f9570
LINK_ENTITY_TO_CLASS(weapon_357, CPython);	  // orig 0x1f95a0

// orig 0x1f95d0
bool CPython::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "357";
	p->iMaxAmmo1 = PREFS_KeyLoopUpIntClamp("PYTHON", "MaxAmmo", 48, 0, 250);
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = PREFS_KeyLoopUpIntClamp("PYTHON", "ClipSize", PYTHON_MAX_CLIP, 0, 250);
	p->iFlags = 0;
	p->iSlot = 1;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_PYTHON;
	p->iWeight = PYTHON_WEIGHT;

	return true;
}

// orig 0x1f9690. The original sends WeapPickup here once the weapon is added. In this tree
// CBasePlayer::AddPlayerItem sends it for every weapon, so this only adds the weapon.
void CPython::AddToPlayer(CBasePlayer* pPlayer)
{
	CBasePlayerWeapon::AddToPlayer(pPlayer);
	// As the original: this weapon tells the player it was picked up (AddPlayerItem doesn't)
	MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
	WRITE_BYTE(m_iId);
	MESSAGE_END();
}

// orig 0x1f9720
void CPython::Spawn()
{
	pev->classname = MAKE_STRING("weapon_357"); // hack to allow for old names
	Precache();
	m_iId = WEAPON_PYTHON;
	SET_MODEL(ENT(pev), "models/w_357.mdl");

	m_iDefaultAmmo = PREFS_KeyLoopUpIntClamp("PYTHON", "PickupAmmo", 6, 0, 250);

	FallInit(); // get ready to fall down.
}

// orig 0x1f97b0
void CPython::Precache()
{
	PRECACHE_MODEL("models/v_357.mdl");
	PRECACHE_MODEL("models/w_357.mdl");
	PRECACHE_MODEL("models/p_357.mdl");
	PRECACHE_MODEL("models/rpgrocket.mdl");
	PRECACHE_MODEL("sprites/blueflare1.spr");

	PRECACHE_MODEL("models/w_357ammobox.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("weapons/357_reload1.wav");
	PRECACHE_SOUND("weapons/357_cock1.wav");
	PRECACHE_SOUND("weapons/357_shot1.wav");
	PRECACHE_SOUND("weapons/357_shot2.wav");
}

// orig 0x1f9890
bool CPython::Deploy()
{
	if (g_pGameRules->IsMultiplayer())
	{
		// enable laser sight geometry.
		pev->body = 1;
	}
	else
	{
		pev->body = 0;
	}

	return DefaultDeploy("models/v_357.mdl", "models/p_357.mdl", PYTHON_DRAW, "python", pev->body);
}

// orig 0x1f9910
void CPython::Holster()
{
	m_fInReload = false; // cancel any reload in progress.

	if (m_fInZoom)
	{
		SecondaryAttack();
	}

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;
	m_flTimeWeaponIdle = gpGlobals->time + 10 + RANDOM_FLOAT(0, 5);
	SendWeaponAnim(PYTHON_HOLSTER, pev->body);
}

// Both fire modes start their rounds here: beside the player's origin, not at the gun.
static Vector PythonRoundOrigin(CBasePlayer* pPlayer)
{
	return pPlayer->pev->origin + gpGlobals->v_right * 8 + gpGlobals->v_forward * 15;
}

static void PythonShotSound(CBasePlayer* pPlayer)
{
	switch (RANDOM_LONG(0, 1))
	{
	case 0:
		EMIT_SOUND_DYN(pPlayer->edict(), CHAN_WEAPON, "weapons/357_shot1.wav", RANDOM_FLOAT(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM);
		break;
	case 1:
		EMIT_SOUND_DYN(pPlayer->edict(), CHAN_WEAPON, "weapons/357_shot2.wav", RANDOM_FLOAT(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM);
		break;
	}
}

// orig 0x1f99b0. Six rounds: a straight one, four sub rounds swinging right, left, up and down
// around it, and a straight one at half speed, for SecondaryAmmoUssage rounds of the clip.
void CPython::SecondaryAttack()
{
	m_pPlayer->GetGunPosition(); // the original calls it and ignores the result

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecVelocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("PYTHON", "SecondarySpeed", 2000.0);

	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
	}
	else if (m_iClip >= PREFS_KeyLoopUpInt("PYTHON", "SecondaryAmmoUssage", 6))
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

		m_iClip -= PREFS_KeyLoopUpInt("PYTHON", "SecondaryAmmoUssage", 6);

		SendWeaponAnim(PYTHON_FIRE1, pev->body);
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

		CPythRound::CreateRnd(PythonRoundOrigin(m_pPlayer), vecVelocity, m_pPlayer->edict(), 0);
		CPythRound::CreateRnd(PythonRoundOrigin(m_pPlayer), vecVelocity, m_pPlayer->edict(), 1);
		CPythRound::CreateRnd(PythonRoundOrigin(m_pPlayer), vecVelocity, m_pPlayer->edict(), 2);
		CPythRound::CreateRnd(PythonRoundOrigin(m_pPlayer), vecVelocity, m_pPlayer->edict(), 3);
		CPythRound::CreateRnd(PythonRoundOrigin(m_pPlayer), vecVelocity, m_pPlayer->edict(), 4);
		CPythRound::CreateRnd(PythonRoundOrigin(m_pPlayer), vecVelocity * 0.5, m_pPlayer->edict(), 0);

		PythonShotSound(m_pPlayer);

		m_pPlayer->pev->punchangle.x -= 10;
		UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
		m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * (float)PREFS_KeyLoopUpInt("PYTHON", "SecondaryPush", 800);

		m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("PYTHON", "SecondaryRefire", 2.0) * m_pPlayer->m_fCurseRefireScale;
		m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("PYTHON", "SecondaryRefire", 2.0) * m_pPlayer->m_fCurseRefireScale;
		return;
	}
	else
	{
		if (!m_fFireOnEmpty)
		{
			Reload();
			return;
		}

		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM, 0, PITCH_NORM);
	}

	m_flNextPrimaryAttack = gpGlobals->time + 0.15;
	m_flNextSecondaryAttack = gpGlobals->time + 0.15;
}

// orig 0x1fa3d0
void CPython::PrimaryAttack()
{
	m_pPlayer->GetGunPosition(); // the original calls it and ignores the result

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	Vector vecVelocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("PYTHON", "PrimarySpeed", 2000.0);

	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		m_flNextSecondaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		if (!m_fFireOnEmpty)
		{
			Reload();
			return;
		}

		EMIT_SOUND_DYN(m_pPlayer->edict(), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM, 0, PITCH_NORM);
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		m_flNextSecondaryAttack = gpGlobals->time + 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_iClip--;

	SendWeaponAnim(PYTHON_FIRE1, pev->body);

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	m_flNextPrimaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("PYTHON", "PrimaryRefire", 0.8) * m_pPlayer->m_fCurseRefireScale;
	m_flNextSecondaryAttack = gpGlobals->time + PREFS_KeyLoopUpFloat("PYTHON", "PrimarySecRefire", 1.0) * m_pPlayer->m_fCurseRefireScale;

	CPythRound::CreateRnd(PythonRoundOrigin(m_pPlayer), vecVelocity, m_pPlayer->edict(), 0);

	PythonShotSound(m_pPlayer);

	m_pPlayer->pev->punchangle.x -= 3;
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);
	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * PREFS_KeyLoopUpFloat("PYTHON", "PrimaryPunchFactor", 125.0);
}

// orig 0x1fa810
void CPython::Reload()
{
	if (m_fInZoom)
	{
		m_fInZoom = false;
		m_pPlayer->m_iFOV = 0; // 0 means reset to default fov
	}

	if (DefaultReload(PREFS_KeyLoopUpIntClamp("PYTHON", "ClipSize", PYTHON_MAX_CLIP, 0, 250), PYTHON_RELOAD, 2.0, pev->body))
	{
		m_flSoundDelay = gpGlobals->time + 1.5;
	}
}

// orig 0x1fa8a0
void CPython::WeaponIdle()
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	if (m_flSoundDelay != 0 && m_flSoundDelay <= gpGlobals->time)
	{
		EMIT_SOUND(m_pPlayer->edict(), CHAN_WEAPON, "weapons/357_reload1.wav", RANDOM_FLOAT(0.8, 0.9), ATTN_NORM);
		m_flSoundDelay = 0;
	}

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.5)
	{
		iAnim = PYTHON_IDLE1;
		m_flTimeWeaponIdle = gpGlobals->time + (70.0 / 30.0);
	}
	else if (flRand <= 0.7)
	{
		iAnim = PYTHON_IDLE2;
		m_flTimeWeaponIdle = gpGlobals->time + (60.0 / 30.0);
	}
	else if (flRand <= 0.9)
	{
		iAnim = PYTHON_IDLE3;
		m_flTimeWeaponIdle = gpGlobals->time + (88.0 / 30.0);
	}
	else
	{
		iAnim = PYTHON_FIDGET;
		m_flTimeWeaponIdle = gpGlobals->time + (170.0 / 30.0);
	}

	SendWeaponAnim(iAnim, pev->body);
}

// orig 0x1faa70. Rates the python against pEnemy: highest for the secondary, a bit lower for the
// primary (both higher the faster the enemy moves), 75 when neither would fire.
int CPython::GetBotWeight(CBot* pBot, CBaseEntity* pEnemy)
{
	if (!pEnemy)
		return ItemInfoArray[m_iId].iWeight;

	if (BotFireSecondary(pBot, pEnemy))
	{
		float flFrac = pEnemy->pev->velocity.Length() * (1.0f / 64);
		if (flFrac >= 1.0)
			return 1200;
		return (int)(300 + flFrac * 900);
	}

	if (BotFirePrimary(pBot, pEnemy))
	{
		float flFrac = pEnemy->pev->velocity.Length() / 96.0f;
		if (flFrac >= 1.0)
			return 1000;
		return (int)(500 + flFrac * 500);
	}

	return 75;
}

// orig 0x1fac00
int CPython::BotFireButton(CBot* pBot, CBaseEntity* pEnemy)
{
	if (BotFireSecondary(pBot, pEnemy))
		return IN_ATTACK2;
	return IN_ATTACK;
}

// orig 0x1fac50. Secondary when there's ammo for it, the bot is in state 3, the enemy is
// 256-2048 away and moving at 256 or less. With no enemy it says yes, as the original does.
bool CPython::BotFireSecondary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (!pEnemy)
		return true;

	float flDist = (pEnemy->Center() - RCBotCenter(pBot)).Length();

	if (m_iClip + m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 5 && RCBotState(pBot) == 3 && !(256 > flDist) && !(2048 < flDist))
	{
		if (!(256 < pEnemy->pev->velocity.Length()))
			return true;
	}

	return false;
}

// orig 0x1fad70. Primary when the enemy is more than 256 and less than 1920 away (or none).
bool CPython::BotFirePrimary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (pEnemy)
	{
		float flDist = (pEnemy->Center() - RCBotCenter(pBot)).Length();
		if (!(256 < flDist) || !(1920 > flDist))
			return false;
	}
	return true;
}


class CPythonAmmo : public CBasePlayerAmmo
{
	// orig 0x251970
	void Spawn() override
	{
		Precache();
		SET_MODEL(ENT(pev), "models/w_357ammobox.mdl");
		CBasePlayerAmmo::Spawn();
	}
	// orig 0x251930
	void Precache() override
	{
		PRECACHE_MODEL("models/w_357ammobox.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	// orig 0x251880
	bool AddAmmo(CBaseEntity* pOther) override
	{
		int iMax = PREFS_KeyLoopUpIntClamp("PYTHON", "MaxAmmo", 48, 0, 250);
		if (pOther->GiveAmmo(PREFS_KeyLoopUpIntClamp("PYTHON", "BoxGive", AMMO_357BOX_GIVE, 0, 250), "357", iMax) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return true;
		}
		return false;
	}
};
LINK_ENTITY_TO_CLASS(ammo_357, CPythonAmmo); // orig 0x1faa40
