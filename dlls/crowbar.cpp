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

// Rocket Crowbar 1.9: the rocket crowbar. Secondary attack swings and fires a CCrowbarRocket
// (a drunk, sometimes dishonest rocket that can turn back on its owner); reload is a shove that
// knocks what it hits away, or the player back off a wall. Every number comes from rc.ini
// [CROWBAR]. Reconstructed from rc_i386.so (addresses are file addresses, reference/funcfile.tsv);
// see reference/dossiers/w2-shotgun-egon-crowbar.md.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "Preff.h"


#define CROWBAR_BODYHIT_VOLUME 128
#define CROWBAR_WALLHIT_VOLUME 512

// The RC cap a crowbar reload shoves (HasRCCap). VERIFY name: the original has only the bit.
#define RC_CAP_CROWBAR_RELOAD_MOVE 0x40

// RC's bot object, as the original reads it: a CBaseMonster-derived class the game library never
// defines (it lived in the bot library), so these are the original i386 bot's offsets and hold
// only for that bot binary (reference/DEVIATIONS.md, "Bot interface").
static CBaseMonster* RCBotEnt(CBot* pBot) { return reinterpret_cast<CBaseMonster*>(pBot); }
static bool RCBotHasAllies(CBot* pBot) { return *(reinterpret_cast<byte*>(pBot) + 0x69c) != 0; }			   // VERIFY name: gates the R_AL checks
static int RCBotSkill(CBot* pBot) { return *reinterpret_cast<int*>(reinterpret_cast<byte*>(pBot) + 0x6a0); } // VERIFY name: 0..3, higher reaches farther

//=========================================================
// CCrowbarRocket: the rocket the crowbar's secondary attack fires. No entity factory in the
// original (made only by CreateRpgRocket); its classname is "crowbar", so kills read as crowbar.
//=========================================================
class CCrowbarRocket : public CGrenade
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	void ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce) override;
	virtual bool ProMagEffected(CBaseEntity* pMagnet);

	void EXPORT FollowThink();
	void EXPORT RocketTouch(CBaseEntity* pOther);
	static CCrowbarRocket* CreateRpgRocket(Vector vecOrigin, Vector vecAngles, CBaseEntity* pOwner);

	int m_iTrail;		  // orig +0x280, the smoke sprite
	bool m_blDisHonest;	  // orig +0x284, turns back on its owner at m_flDisTime
	float m_flDisTime;	  // orig +0x288
	float m_flDisOwnTime; // orig +0x28c, pev->owner is cleared then so the rocket can hit its owner
	float m_flAngSpread;  // orig +0x290, drunk wobble
	bool m_blDrunk;		  // orig +0x294
	edict_t* m_pRealOwn;  // orig +0x298, the player who fired it
	Vector vecRealDir;	  // orig +0x29c, the launch angles the drunk wobble is around
};

TYPEDESCRIPTION CCrowbarRocket::m_SaveData[] =
	{
		DEFINE_FIELD(CCrowbarRocket, m_blDisHonest, FIELD_BOOLEAN),
		DEFINE_FIELD(CCrowbarRocket, m_flDisTime, FIELD_TIME),
		DEFINE_FIELD(CCrowbarRocket, m_flDisOwnTime, FIELD_TIME),
		DEFINE_FIELD(CCrowbarRocket, m_flAngSpread, FIELD_FLOAT),
		DEFINE_FIELD(CCrowbarRocket, m_blDrunk, FIELD_BOOLEAN),
		DEFINE_FIELD(CCrowbarRocket, m_pRealOwn, FIELD_EDICT),
		DEFINE_FIELD(CCrowbarRocket, vecRealDir, FIELD_VECTOR),
};
IMPLEMENT_SAVERESTORE(CCrowbarRocket, CGrenade); // orig 0x1126d0, 0x112720

// orig 0x112770
CCrowbarRocket* CCrowbarRocket::CreateRpgRocket(Vector vecOrigin, Vector vecAngles, CBaseEntity* pOwner)
{
	CCrowbarRocket* pRocket = GetClassPtr((CCrowbarRocket*)NULL);

	UTIL_SetOrigin(pRocket->pev, vecOrigin);
	pRocket->pev->angles = vecAngles;
	pRocket->vecRealDir = vecAngles;
	pRocket->Spawn();
	pRocket->SetTouch(&CCrowbarRocket::RocketTouch);
	pRocket->pev->owner = pOwner->edict();
	pRocket->m_pRealOwn = pRocket->pev->owner;

	return pRocket;
}

// orig 0x112820
void CCrowbarRocket::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;
	pev->flags |= FL_MONSTER;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);

	pev->classname = MAKE_STRING("crowbar");

	SetTouch(&CCrowbarRocket::RocketTouch);

	m_blDrunk = PREFS_KeyLoopUpBool("CROWBAR", "DoDrunkness", true);

	UTIL_MakeVectors(pev->angles);
	pev->velocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("CROWBAR", "DefualtSpeed", 600.0);
	pev->speed = PREFS_KeyLoopUpFloat("CROWBAR", "DefualtSpeed", 600.0);
	pev->gravity = 0.5;
	pev->dmg = PREFS_KeyLoopUpFloat("CROWBAR", "BlastDamage", 120.0);
	m_flDisOwnTime = gpGlobals->time + 0.3;
	m_flAngSpread = PREFS_KeyLoopUpFloat("CROWBAR", "DrunkSpread", 0.45);

	// with DoDishonesty off m_blDisHonest stays as GetClassPtr zeroed it (false)
	if (PREFS_KeyLoopUpBool("CROWBAR", "DoDishonesty", true))
	{
		if (m_blDrunk)
		{
			if (RANDOM_LONG(0, PREFS_KeyLoopUpInt("CROWBAR", "DrunkTurnChance", 4)) == 1)
			{
				m_blDisHonest = true;
				pev->speed = PREFS_KeyLoopUpFloat("CROWBAR", "DishonestSpeed", 1000.0);
				float flMax = PREFS_KeyLoopUpFloat("CROWBAR", "DishonestMaxTime", 1.0);
				m_flDisTime = RANDOM_FLOAT(PREFS_KeyLoopUpFloat("CROWBAR", "DishonestMinTime", 0.3), flMax) + gpGlobals->time;
			}
			else
				m_blDisHonest = false;
		}
		else
		{
			if (RANDOM_LONG(0, PREFS_KeyLoopUpInt("CROWBAR", "SoberTurnChance", 2)) == 1)
			{
				m_blDisHonest = true;
				float flMax = PREFS_KeyLoopUpFloat("CROWBAR", "DishonestMaxTime", 1.0);
				m_flDisTime = RANDOM_FLOAT(PREFS_KeyLoopUpFloat("CROWBAR", "DishonestMinTime", 0.3), flMax) + gpGlobals->time;
				pev->speed = PREFS_KeyLoopUpFloat("CROWBAR", "DishonestSpeed", 1000.0);
			}
			else
				m_blDisHonest = false;
		}
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0.2, 0.5, 0, PITCH_NORM);

	// a red, green or blue trail
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex()); // entity
	WRITE_SHORT(m_iTrail);	 // model
	WRITE_BYTE(40);			 // life
	WRITE_BYTE(5);			 // width
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		WRITE_BYTE(255);
		WRITE_BYTE(0);
		WRITE_BYTE(0);
		break;
	case 1:
		WRITE_BYTE(0);
		WRITE_BYTE(255);
		WRITE_BYTE(0);
		break;
	case 2:
		WRITE_BYTE(0);
		WRITE_BYTE(0);
		WRITE_BYTE(255);
		break;
	}
	WRITE_BYTE(255); // brightness
	MESSAGE_END();

	SetThink(&CCrowbarRocket::FollowThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x112da0
void CCrowbarRocket::RocketTouch(CBaseEntity* pOther)
{
	// credit the kill to whoever fired it, even after FollowThink cleared pev->owner
	if (m_pRealOwn)
		pev->owner = m_pRealOwn;
	else
		pev->owner = ENT(pev);

	STOP_SOUND(edict(), CHAN_VOICE, "weapons/rocket1.wav");
	ExplodeTouch(pOther);
}

// orig 0x112e10
void CCrowbarRocket::Precache()
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND("weapons/rocket1.wav");
}

// orig 0x112e70
void CCrowbarRocket::FollowThink()
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_flDisOwnTime < gpGlobals->time)
		pev->owner = NULL;

	if (m_blDisHonest && m_flDisTime < gpGlobals->time)
	{
		// turn on the player who fired it
		Vector vecDir = (m_pRealOwn->v.origin - pev->origin).Normalize();
		pev->velocity = vecDir * pev->speed;
		return;
	}

	if (m_blDrunk)
	{
		float flUp = RANDOM_FLOAT(-m_flAngSpread, m_flAngSpread);
		float flRight = RANDOM_FLOAT(-m_flAngSpread, m_flAngSpread);

		// Original bug, kept: int abs() truncates each offset, so the forward part is 1 for any
		// spread below 1 instead of shrinking as the wobble grows (dossier, "Original bugs").
		float flForward = 1 - (abs(static_cast<int>(flUp)) + abs(static_cast<int>(flRight)));

		UTIL_MakeVectors(vecRealDir);
		pev->velocity = gpGlobals->v_forward * flForward + gpGlobals->v_up * flUp + gpGlobals->v_right * flRight;
		pev->velocity = pev->velocity.Normalize() * pev->speed;
		pev->angles = UTIL_VecToAngles(pev->velocity);
	}
}

// orig 0x114ed0. Original oddity, kept: stores the magnet's pull direction where FollowThink
// expects angles, so a magnetised drunk rocket wobbles around a meaningless heading.
void CCrowbarRocket::ProMagApplyForce(CBaseEntity* pMagnet, Vector vecForce)
{
	vecRealDir = vecForce;
}

// orig 0x114f00
bool CCrowbarRocket::ProMagEffected(CBaseEntity* pMagnet)
{
	if (!m_pRealOwn)
		return false;
	return pMagnet->edict() != m_pRealOwn;
}

//=========================================================
// CCrowbar
//=========================================================
LINK_ENTITY_TO_CLASS(weapon_crowbar, CCrowbar); // orig 0x113200

// orig 0x113230
int CCrowbar::GetBotWeight(CBot* pBot, CBaseEntity* pEnemy)
{
	if (!pEnemy)
		return ItemInfoArray[m_iId].iWeight;

	CBaseMonster* pBotEnt = RCBotEnt(pBot);
	float flDist = (pEnemy->Center() - pBotEnt->Center()).Length();
	Vector vecDiff = pEnemy->Center() - pBotEnt->Center();

	if (fabs(vecDiff.z) >= 128)
		return 1;

	if (RCBotHasAllies(pBot) && pBotEnt->IRelationship(pEnemy) == R_AL && flDist < 768)
		return 9999;

	if (flDist <= 64)
		return 6000;

	if (flDist < (3 - RCBotSkill(pBot)) * 128 + 96.0)
		return 1000;

	if (flDist > 256 && flDist < 768 && RCBotSkill(pBot) > 1)
		return 10;

	return 1;
}

// orig 0x1133f0
void CCrowbar::BotWeaponFrame(CBot* pBot, CBaseEntity* pEnemy)
{
	CBaseMonster* pBotEnt = RCBotEnt(pBot);
	float flDist = (pEnemy->Center() - pBotEnt->Center()).Length();

	if (flDist > 48)
		pBotEnt->pev->button |= IN_FORWARD;

	// the original computes an aim point here (GetBotShootOrg, raised 64 units for an ally) and
	// never uses it; the calls are kept in case an override has side effects
	(void)pEnemy->GetBotShootOrg();

	if (RCBotHasAllies(pBot) && pBotEnt->IRelationship(pEnemy) == R_AL)
	{
		if (RANDOM_LONG(0, 4) != 0)
			pBotEnt->pev->button |= IN_ATTACK;
		else
			pBotEnt->pev->button |= IN_RELOAD;
		return;
	}

	(void)pEnemy->GetBotShootOrg();
}

// orig 0x113540
bool CCrowbar::BotFirePrimary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (pEnemy && (pEnemy->Center() - RCBotEnt(pBot)->Center()).Length() >= 128)
		return false;
	return true;
}

// orig 0x1135e0
bool CCrowbar::BotFireSecondary(CBot* pBot, CBaseEntity* pEnemy)
{
	if (pEnemy && (pEnemy->Center() - RCBotEnt(pBot)->Center()).Length() > 256)
		return true;
	return false;
}

// orig 0x113680
void CCrowbar::Spawn()
{
	Precache();
	m_iId = WEAPON_CROWBAR;
	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	m_iClip = -1;

	FallInit(); // get ready to fall down.
}


// orig 0x1136e0. No crowbar event: RC 1.9's server plays every crowbar sound itself.
void CCrowbar::Precache()
{
	PRECACHE_MODEL("models/v_crowbar.mdl");
	PRECACHE_MODEL("models/w_crowbar.mdl");
	PRECACHE_MODEL("models/p_crowbar.mdl");
	PRECACHE_SOUND("weapons/cbar_hit1.wav");
	PRECACHE_SOUND("weapons/cbar_hit2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod1.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod3.wav");
	PRECACHE_SOUND("weapons/cbar_miss1.wav");
}

// orig 0x1137a0
bool CCrowbar::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 0;
	p->iId = WEAPON_CROWBAR;
	p->iWeight = CROWBAR_WEIGHT;
	return true;
}


// orig 0x113820
bool CCrowbar::Deploy()
{
	return DefaultDeploy("models/v_crowbar.mdl", "models/p_crowbar.mdl", CROWBAR_DRAW, "crowbar");
}

// orig 0x113860
void CCrowbar::Holster()
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim(CROWBAR_HOLSTER);
}


// orig 0x1138b0. The original takes float* mins/maxs; same traces.
void FindHullIntersection(const Vector& vecSrc, TraceResult& tr, const Vector& mins, const Vector& maxs, edict_t* pEntity)
{
	int i, j, k;
	float distance;
	const Vector* minmaxs[2] = {&mins, &maxs};
	TraceResult tmpTrace;
	Vector vecHullEnd = tr.vecEndPos;
	Vector vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc) * 2);
	UTIL_TraceLine(vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace);
	if (tmpTrace.flFraction < 1.0)
	{
		tr = tmpTrace;
		return;
	}

	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 2; j++)
		{
			for (k = 0; k < 2; k++)
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i]->x;
				vecEnd.y = vecHullEnd.y + minmaxs[j]->y;
				vecEnd.z = vecHullEnd.z + minmaxs[k]->z;

				UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace);
				if (tmpTrace.flFraction < 1.0)
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if (thisDistance < distance)
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}


// orig 0x113ae0
void CCrowbar::PrimaryAttack()
{
	if (!Swing(true))
	{
		SetThink(&CCrowbar::SwingAgain);
		pev->nextthink = gpGlobals->time + PREFS_KeyLoopUpFloat("CROWBAR", "PrimarySebHitRefire", 0.1);
	}
}

// orig 0x113b50. A swing, and a rocket from the gun position along the view angles.
void CCrowbar::SecondaryAttack()
{
	Swing(true);
	CCrowbarRocket::CreateRpgRocket(m_pPlayer->GetGunPosition(), m_pPlayer->pev->v_angle, m_pPlayer);

	m_flNextPrimaryAttack = PREFS_KeyLoopUpFloat("CROWBAR", "SecondaryPrimaryRefire", 0.25) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;
	m_flNextSecondaryAttack = PREFS_KeyLoopUpFloat("CROWBAR", "SecondaryRefire", 1.5) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;
}


// orig 0x113c10
void CCrowbar::Smack()
{
	DecalGunshot(&m_trHit, BULLET_PLAYER_CROWBAR);
}


// orig 0x113c40
void CCrowbar::SwingAgain()
{
	Swing(false);
}


// orig 0x113c70. RC 1.9's SDK version (server-side sounds, a miss animation) with rc.ini numbers:
// every hit does PrimaryHitDamage, with no half-damage follow-up swing.
bool CCrowbar::Swing(bool fFirst)
{
	bool fDidHit = false;

	TraceResult tr;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

	if (tr.flFraction >= 1.0)
	{
		UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT(m_pPlayer->pev), &tr);
		if (tr.flFraction < 1.0)
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (!pHit || pHit->IsBSPModel())
				FindHullIntersection(vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict());
			vecEnd = tr.vecEndPos; // This is the point on the actual surface (the hull could have hit space)
		}
	}

	if (tr.flFraction >= 1.0)
	{
		if (fFirst)
		{
			// miss
			switch ((m_iSwing++) % 3)
			{
			case 0:
				SendWeaponAnim(CROWBAR_ATTACK1MISS);
				break;
			case 1:
				SendWeaponAnim(CROWBAR_ATTACK2MISS);
				break;
			case 2:
				SendWeaponAnim(CROWBAR_ATTACK3MISS);
				break;
			}
			m_flNextPrimaryAttack = PREFS_KeyLoopUpFloat("CROWBAR", "PrimaryNoHitRefire", 0.5) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;

			// play wiff or swish sound
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0, 0xF));

			// player "shoot" animation
			m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		}
	}
	else
	{
		// hit
		fDidHit = true;
		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);

		switch (((m_iSwing++) % 2) + 1)
		{
		case 0:
			SendWeaponAnim(CROWBAR_ATTACK1HIT);
			break;
		case 1:
			SendWeaponAnim(CROWBAR_ATTACK2HIT);
			break;
		case 2:
			SendWeaponAnim(CROWBAR_ATTACK3HIT);
			break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		ClearMultiDamage();
		pEntity->TraceAttack(m_pPlayer->pev, PREFS_KeyLoopUpFloat("CROWBAR", "PrimaryHitDamage", 35.0), gpGlobals->v_forward, &tr, DMG_CLUB);
		ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);

		m_flNextPrimaryAttack = PREFS_KeyLoopUpFloat("CROWBAR", "PrimaryHitRefire", 0.2) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		bool fHitWorld = true;

		if (pEntity)
		{
			if (pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE)
			{
				// play thwack or smack sound
				switch (RANDOM_LONG(0, 2))
				{
				case 0:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_hitbod1.wav", 1, ATTN_NORM);
					break;
				case 1:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_hitbod2.wav", 1, ATTN_NORM);
					break;
				case 2:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_hitbod3.wav", 1, ATTN_NORM);
					break;
				}
				m_pPlayer->m_iWeaponVolume = CROWBAR_BODYHIT_VOLUME;
				if (!pEntity->IsAlive())
					return true;
				else
					flVol = 0.1;

				fHitWorld = false;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd - vecSrc) * 2, BULLET_PLAYER_CROWBAR);

			if (g_pGameRules->IsMultiplayer())
			{
				// override the volume here, cause we don't play texture sounds in multiplayer,
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play crowbar strike
			switch (RANDOM_LONG(0, 1))
			{
			case 0:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
				break;
			}
		}

		// delay the decal a bit
		m_trHit = tr;
		SetThink(&CCrowbar::Smack);
		pev->nextthink = gpGlobals->time + PREFS_KeyLoopUpFloat("CROWBAR", "PrimaryHitWallRefire", 0.18);

		m_pPlayer->m_iWeaponVolume = flVol * CROWBAR_WALLHIT_VOLUME;
	}
	return fDidHit;
}

// orig 0x114470: lets ItemPostFrame call Reload although the crowbar has no clip
bool CCrowbar::IsHornetGun()
{
	return true;
}

// orig 0x114480. The shove: a short swing that knocks what it hits away (anything with the
// reload-move cap), or bounces the player back off whatever it can't move.
void CCrowbar::Reload()
{
	if (gpGlobals->time < m_flNextPrimaryAttack)
		return;

	if (CVAR_GET_FLOAT("sv_ceasefire") == 1)
	{
		m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		return;
	}

	TraceResult tr;

	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr);

	if (tr.flFraction >= 1.0)
	{
		UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT(m_pPlayer->pev), &tr);
		if (tr.flFraction < 1.0)
		{
			CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
			if (!pHit || pHit->IsBSPModel())
				FindHullIntersection(vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict());
			vecEnd = tr.vecEndPos;
		}
	}

	if (tr.flFraction >= 1.0)
	{
		// miss
		switch ((m_iSwing++) % 3)
		{
		case 0:
			SendWeaponAnim(CROWBAR_ATTACK1MISS);
			break;
		case 1:
			SendWeaponAnim(CROWBAR_ATTACK2MISS);
			break;
		case 2:
			SendWeaponAnim(CROWBAR_ATTACK3MISS);
			break;
		}
		m_flNextPrimaryAttack = PREFS_KeyLoopUpFloat("CROWBAR", "ReloadNoHitRefire", 0.9) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0, 0xF));
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
		return;
	}

	CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);

	switch (((m_iSwing++) % 2) + 1)
	{
	case 0:
		SendWeaponAnim(CROWBAR_ATTACK1HIT);
		break;
	case 1:
		SendWeaponAnim(CROWBAR_ATTACK2HIT);
		break;
	case 2:
		SendWeaponAnim(CROWBAR_ATTACK3HIT);
		break;
	}

	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	ClearMultiDamage();
	pEntity->TraceAttack(m_pPlayer->pev, PREFS_KeyLoopUpFloat("CROWBAR", "ReloadHitDamage", 25.0), gpGlobals->v_forward, &tr, DMG_CLUB);
	ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);

	entvars_t* pevPushed;
	int iCap = RC_CAP_CROWBAR_RELOAD_MOVE;
	if (pEntity->HasRCCap(iCap))
	{
		// knock it away
		pEntity->pev->velocity = gpGlobals->v_forward * PREFS_KeyLoopUpFloat("CROWBAR", "ReloadHitVelocity", 800.0);
		pEntity->pev->velocity.z += 75;
		pevPushed = pEntity->pev;
	}
	else
	{
		// bounce the player back
		entvars_t* pevPlayer = m_pPlayer->pev;
		Vector vecBack = -gpGlobals->v_forward;
		float flKick;
		if (pEntity->IsBSPModel())
			flKick = PREFS_KeyLoopUpFloat("CROWBAR", "ReloadHitWallKickBack", 500.0);
		else
			flKick = PREFS_KeyLoopUpFloat("CROWBAR", "ReloadHitCantMoveKickBack", 300.0);
		pevPlayer->velocity = vecBack * flKick;
		m_pPlayer->pev->velocity.z += 75;
		pevPushed = m_pPlayer->pev;
	}
	pevPushed->flags &= ~FL_ONGROUND;

	m_flNextPrimaryAttack = PREFS_KeyLoopUpFloat("CROWBAR", "ReloadHitRefire", 0.75) * m_pPlayer->m_fCurseRefireScale + gpGlobals->time;

	float flVol = 1.0;
	bool fHitWorld = true;

	if (pEntity)
	{
		if (pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE)
		{
			switch (RANDOM_LONG(0, 2))
			{
			case 0:
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_hitbod1.wav", 1, ATTN_NORM);
				break;
			case 1:
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_hitbod2.wav", 1, ATTN_NORM);
				break;
			case 2:
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_hitbod3.wav", 1, ATTN_NORM);
				break;
			}
			m_pPlayer->m_iWeaponVolume = CROWBAR_BODYHIT_VOLUME;
			if (!pEntity->IsAlive())
				return;
			else
				flVol = 0.1;

			fHitWorld = false;
		}
	}

	if (fHitWorld)
	{
		float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd - vecSrc) * 2, BULLET_PLAYER_CROWBAR);

		if (g_pGameRules->IsMultiplayer())
			fvolbar = 1;

		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		case 1:
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0, 3));
			break;
		}
	}

	m_trHit = tr;
	SetThink(&CCrowbar::Smack);
	pev->nextthink = gpGlobals->time + PREFS_KeyLoopUpFloat("CROWBAR", "ReloadHitWallRefire", 0.2);

	m_pPlayer->m_iWeaponVolume = flVol * CROWBAR_WALLHIT_VOLUME;
}

// orig 0x114e40
bool CCrowbar::BotUseWeapon(CBot* pBot, CBaseEntity* pEnemy)
{
	return true;
}

// orig 0x114e50
bool CCrowbar::BotCanHaveItem(CBaseEntity* pBot)
{
	return true;
}

// orig 0x114e60
bool CCrowbar::BotReloadWeapon()
{
	return false;
}
