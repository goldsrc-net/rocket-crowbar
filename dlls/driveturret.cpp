/***
*
*	Rocket Crowbar 1.9: driveturret.cpp. The drivable turret (drive_turret), the pickup a player
*	uses to get in (pickup_drive_turret) and the spawner that makes the pickups
*	(drive_turret_spawner). Reconstructed from rc_i386.so; addresses are file addresses
*	(reference/funcfile.tsv).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"
#include "effects.h"
#include "gamerules.h"
#include "client.h"
#include "UserMessages.h"
#include "driveturret.h"
#include "pointexplo.h"

// Preff.cpp (prefs-votes package)
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
extern bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault);


const char* g_szDTDieSounds[] =
	{
		"turret/tu_die.wav",
		"turret/tu_die2.wav",
		"turret/tu_die3.wav",
};

const char* g_szDTShootSounds[] =
	{
		"hassault/hw_shoot1.wav",
		"hassault/hw_shoot2.wav",
		"hassault/hw_shoot3.wav",
};

// makes the turret's eye: a red glow on the model's attachment 2
static CSprite* DTCreateEyeGlow(CBaseEntity* pTurret)
{
	CSprite* pGlow = CSprite::SpriteCreate("sprites/flare3.spr", pTurret->pev->origin, false);
	if (pGlow)
		pGlow->SetTransparency(kRenderGlow, 255, 0, 0, 0, kRenderFxNoDissipation);
	return pGlow;
}

// the gun's world aim from the turret's angles and its gun angles (DoGun and DEClientPreThink)
static Vector DTGunAngles(entvars_t* pev, const Vector& vecCurAngles)
{
	float flPitch;
	if (vecCurAngles.x < -90)
		flPitch = -90;
	else if (vecCurAngles.x > 15)
		flPitch = 15;
	else
		flPitch = vecCurAngles.x;

	Vector vecAngles;
	vecAngles.x = flPitch - pev->angles.x;
	if (fabs(pev->angles.x) >= 90)
		vecAngles.y = -vecCurAngles.y + pev->angles.y; // hung upside down
	else
		vecAngles.y = vecCurAngles.y + pev->angles.y;
	vecAngles.z = pev->angles.z;

	if (vecAngles.x > 180)
		vecAngles.x -= 360;
	if (vecAngles.x < -180)
		vecAngles.x += 360;

	return vecAngles;
}

// orig 0x174430
LINK_ENTITY_TO_CLASS(drive_turret, CDriveTurret);

// orig 0x174460. The pickup creates it with its plane normal as pev->angles; the turret only
// lives on a world floor or ceiling, else it destroys itself.
void CDriveTurret::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_turret");
	SET_MODEL(ENT(pev), "models/turret.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, -32), Vector(32, 32, 32));
	UTIL_SetOrigin(pev, pev->origin);
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_FLY;
	pev->takedamage = DAMAGE_AIM;
	pev->effects = 0;
	pev->friction = 1;
	pev->health = PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretHealth", 350);
	pev->max_health = pev->health;
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	SetTurretAnim(DTURRET_ANIM_NONE);
	InitBoneControllers();

	m_bWantDeploy = false;
	m_bDeployed = false;
	m_bWantSpin = false;
	m_bSpunUp = false;
	m_vecCurAngles = g_vecZero;
	m_vecGoalAngles = g_vecZero;
	m_vecTurnRate = Vector(290, 400, 0);
	m_fLastAimTime = gpGlobals->time;
	m_fLastBarrelTime = gpGlobals->time;
	m_fSpinDownTime = gpGlobals->time;
	m_iSpinSound = 0;
	m_fNextShot = gpGlobals->time;
	m_iShotCount = 0;
	m_fLastShotTime = gpGlobals->time - 10;
	m_flSpread = 0.001;

	m_pEyeGlow = DTCreateEyeGlow(this);
	m_pEyeGlow->SetAttachment(edict(), 2);
	m_flEyeBrightness = 0;
	m_fNextPing = gpGlobals->time;
	m_fPingEnd = 0;
	m_flPingAmount = 0;

	SetThink(&CDriveTurret::DTThink);
	pev->nextthink = gpGlobals->time + 0.01;
	pev->armorvalue = PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretInitialBullets", 240);

	// pev->angles is still the pickup's plane normal here: look into the surface
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin - pev->angles * 8, ignore_monsters, ENT(pev), &tr);

	if (tr.flFraction < 1.0)
	{
		bool bAttach = false;
		if (tr.vecPlaneNormal == Vector(0, 0, 1))
			bAttach = true;
		else if (tr.vecPlaneNormal == Vector(0, 0, -1))
			bAttach = true;

		if (bAttach)
		{
			AttachToPlane(&tr);
			if (!tr.pHit)
				return;
			if (FClassnameIs(tr.pHit, "worldspawn"))
				return;
			SetThink(&CDriveTurret::KillThink);
			pev->nextthink = gpGlobals->time + 0.01;
			return;
		}
	}

	if (tr.flFraction >= 1.0)
		pev->angles = Vector(0, RANDOM_FLOAT(0, 360), 0);
	else
		AttachToPlane(&tr);

	SetThink(&CDriveTurret::KillThink);
	pev->nextthink = gpGlobals->time + 0.01;
}

// orig 0x174aa0
void CDriveTurret::Precache()
{
	unsigned int i;

	PRECACHE_SOUND("turret/tu_fire1.wav");
	PRECACHE_SOUND("turret/tu_ping.wav");
	PRECACHE_SOUND("turret/tu_deploy.wav");
	PRECACHE_SOUND("turret/tu_spinup.wav");
	PRECACHE_SOUND("turret/tu_spindown.wav");
	PRECACHE_SOUND("turret/tu_active2.wav");
	for (i = 0; i < ARRAYSIZE(g_szDTDieSounds); i++)
		PRECACHE_SOUND(g_szDTDieSounds[i]);
	for (i = 0; i < ARRAYSIZE(g_szDTShootSounds); i++)
		PRECACHE_SOUND(g_szDTShootSounds[i]);
	PRECACHE_MODEL("models/turret.mdl");
	PRECACHE_MODEL("sprites/flare3.spr");
}

// orig 0x174b90
const char* CDriveTurret::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;

	return STRING(m_pOwner->pev->netname);
}

// orig 0x174c00: a machine only while deployed
int CDriveTurret::Classify()
{
	return m_bDeployed ? CLASS_MACHINE : CLASS_NONE;
}

// orig 0x174c20
Vector CDriveTurret::GetCamRotOrg()
{
	return pev->origin + m_vecNormal * 14;
}

// orig 0x174c90: not on a world floor or ceiling
void CDriveTurret::KillThink()
{
	g_pevLastInflictor = pev;
	pev->classname = MAKE_STRING("turret_self_dest");
	if (!m_pOwner)
		Killed(pev, GIB_NORMAL);
	else
		Killed(m_pOwner->pev, GIB_NORMAL);
}

// orig 0x174d00
void CDriveTurret::DTThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.01;

	if (m_pOwner && !FNullEnt(m_pOwner->pev) && m_pOwner->pev->pContainingEntity && m_pOwner->pev->pContainingEntity->pvPrivateData && !m_pOwner->pev->pContainingEntity->free && UTIL_PlayerByIndex(ENTINDEX(m_pOwner->pev->pContainingEntity)))
	{
		DispatchAnimEvents(m_fFrameTime);

		// keep the driver hidden
		m_pOwner->pev->rendermode = kRenderTransColor;
		m_pOwner->pev->renderamt = 0;
		m_pOwner->pev->effects |= EF_NODRAW;

		UpdateOwnerHUD();
		DoGun();
		DoPing();
		EyeUpdate();
		UpdateSpin();
		MoveTurret();
		RunBarrel();
		CheckRetract();
		return;
	}

	// the driver is gone
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("turret");
	g_pevLastInflictor = pev;
	Killed(pev, GIB_NORMAL);
}

// orig 0x174e90: fire while deployed and spun up; retracted, the ammo regenerates
void CDriveTurret::DoGun()
{
	if (!m_bDeployed)
	{
		if (!m_bWantDeploy)
		{
			float flMax = PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretMaxBullets", 240);
			pev->armorvalue = (flMax / PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretBulletFullRegenTime", 30)) * m_fFrameTime + pev->armorvalue;
			if (flMax < pev->armorvalue)
				pev->armorvalue = flMax;
			return;
		}
		if (!m_bDeployed)
			return;
	}

	if (!m_bWantDeploy || m_bWantDeploy != m_bDeployed)
		return;

	if (pev->armorvalue <= PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretBulletsPerShot", 1))
	{
		// out of ammo: retract
		m_bWantDeploy = false;
		return;
	}

	if (m_bWantSpin != m_bSpunUp)
		return;

	if ((m_pOwner->pev->button & IN_ATTACK) == 0)
	{
		SetTurretAnim(DTURRET_ANIM_SPIN);
		return;
	}

	if (m_fNextShot > gpGlobals->time)
		return;

	m_fSpinDownTime = gpGlobals->time + 6;
	if (!m_bSpunUp)
	{
		m_bWantSpin = true;
		return;
	}

	m_bWantSpin = true;
	if (m_fLastShotTime + 3 < gpGlobals->time)
	{
		m_flSpread = 0.001;
	}
	else
	{
		m_flSpread = m_flSpread + 0.002;
		if (m_flSpread > 0.025)
			m_flSpread = 0.025;
	}

	SetTurretAnim(DTURRET_ANIM_FIRE);
	m_fLastShotTime = gpGlobals->time;

	Vector vecSrc = pev->origin + m_vecNormal * 28;
	UTIL_MakeVectors(DTGunAngles(pev, m_vecCurAngles));

	pev->classname = MAKE_STRING("turret_gun");
	TuFireBullets(vecSrc, gpGlobals->v_forward, Vector(m_flSpread, m_flSpread, m_flSpread));
	pev->classname = MAKE_STRING("drive_turret");

	pev->armorvalue = pev->armorvalue - PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretBulletsPerShot", 1);
	if (pev->armorvalue <= 0)
		pev->armorvalue = 0;

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_szDTShootSounds[RANDOM_LONG(0, 2)], 1.0, 0.4, 0, 100);
	pev->effects |= EF_MUZZLEFLASH;
	m_fNextShot = PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretGunRefire", 0.05) + gpGlobals->time;
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 2;
}

// orig 0x1753c0: the radar ping, drawn on the driver's screen through his pev user fields
void CDriveTurret::DoPing()
{
	m_pOwner->pev->iuser3 = 8;
	m_pOwner->pev->euser2 = ENT(pev);
	m_pOwner->pev->euser3 = m_pEyeGlow->pev->pContainingEntity;
	m_pOwner->pev->fuser1 = 0;

	if (!m_bDeployed || !m_bWantDeploy)
		return;

	float flTarget, flSpeed;
	if (m_fPingEnd <= gpGlobals->time)
	{
		m_flEyeBrightness = 0;
		if (m_fNextPing < gpGlobals->time)
		{
			m_fNextPing = gpGlobals->time + 1;
			m_fPingEnd = gpGlobals->time + 0.25;
			m_flPingAmount = 0;
			m_pOwner->pev->fuser1 = 0;
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "turret/tu_ping.wav", 1.0, 0.6, 0, 100);
			return;
		}
		flTarget = 0;
		flSpeed = m_fFrameTime * 800;
	}
	else
	{
		m_flEyeBrightness = 240;
		flTarget = 200;
		flSpeed = m_fFrameTime * 1200;
	}

	m_flPingAmount = UTIL_Approach(flTarget, m_flPingAmount, flSpeed);
	m_pOwner->pev->fuser1 = m_flPingAmount;
}

// orig 0x175580: keep the eye glow alive, on the model, and fading to its brightness
void CDriveTurret::EyeUpdate()
{
	bool bValid = false;
	if (m_pEyeGlow && m_pEyeGlow->pev && m_pEyeGlow->pev->pContainingEntity && !FNullEnt(m_pEyeGlow->pev))
	{
		edict_t* pent = m_pEyeGlow->pev->pContainingEntity;
		if (pent->pvPrivateData && !pent->free)
			bValid = true;
	}

	if (!bValid)
	{
		m_pEyeGlow = DTCreateEyeGlow(this);
		if (!m_pEyeGlow)
			return;
	}

	m_pEyeGlow->SetAttachment(edict(), 2);

	if (!m_bDeployed || !m_bWantDeploy)
	{
		m_pEyeGlow->pev->renderamt = 0;
		m_pEyeGlow->pev->effects |= EF_NODRAW;
		return;
	}

	m_pEyeGlow->pev->renderamt = UTIL_Approach(m_flEyeBrightness, m_pEyeGlow->pev->renderamt, m_fFrameTime * 1200);
	if (m_pEyeGlow->pev->renderamt == 0)
		m_pEyeGlow->pev->effects |= EF_NODRAW;
	else
		m_pEyeGlow->pev->effects &= ~EF_NODRAW;
	UTIL_SetOrigin(m_pEyeGlow->pev, pev->origin);
}

// orig 0x1757c0: the driver's HUD shows health and, as armor, ammo in percent
void CDriveTurret::UpdateOwnerHUD()
{
	if (!m_pOwner)
		return;

	if (pev->health != (float)m_iClientHealth)
	{
		float flPercent = (pev->health / pev->max_health) * 100;
		int iHealth = (flPercent > 0) ? (int)flPercent : 0;
		if (iHealth < 1)
			iHealth = 1;
		else if (iHealth > 100)
			iHealth = 100;

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_SHORT(iHealth);
		MESSAGE_END();

		m_iClientHealth = (int)pev->health;
	}

	if (pev->armorvalue != (float)m_iClientArmor)
	{
		int iAmmo = 0;
		if ((pev->armorvalue / PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretMaxBullets", 240)) * 100 > 0)
			iAmmo = (int)((pev->armorvalue / PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretMaxBullets", 240)) * 100);
		if (iAmmo < 0)
			iAmmo = 0;
		else if (iAmmo > 100)
			iAmmo = 100;

		MESSAGE_BEGIN(MSG_ONE, gmsgBattery, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_SHORT(iAmmo);
		MESSAGE_END();

		m_iClientArmor = (int)pev->armorvalue;
	}
}

// orig 0x175a30: the driver's view turns are used as deltas on the gun's goal, then reset
void CDriveTurret::UpdateBarrel()
{
	float flPitchDelta = m_pOwner->pev->v_angle.x;
	float flYawDelta = m_pOwner->pev->v_angle.y;

	m_pOwner->pev->v_angle = g_vecZero;
	m_pOwner->pev->angles = m_pOwner->pev->v_angle;
	m_pOwner->pev->fixangle = 1;

	if (!m_bDeployed || !m_bWantDeploy)
		return;

	m_vecGoalAngles.y = UTIL_AngleMod(m_vecGoalAngles.y + flYawDelta);
	m_vecGoalAngles.x = m_vecGoalAngles.x + flPitchDelta;
	if (m_vecGoalAngles.x > 180)
		m_vecGoalAngles.x -= 360;
	if (m_vecGoalAngles.x < -180)
		m_vecGoalAngles.x += 360;
}

// orig 0x175b50: attack keeps the barrel spinning for 6 seconds
void CDriveTurret::UpdateSpin()
{
	if (!m_bDeployed || !m_bWantDeploy || m_bWantSpin != m_bSpunUp)
		return;

	if ((m_pOwner->pev->button & IN_ATTACK) != 0)
	{
		m_bWantSpin = true;
		m_fSpinDownTime = gpGlobals->time + 6;
	}

	if (m_fSpinDownTime < gpGlobals->time)
		m_bWantSpin = false;
}

// orig 0x175bf0: attack2 deploys or retracts
void CDriveTurret::CheckRetract()
{
	if ((m_pOwner->pev->button & IN_ATTACK2) != 0 && m_bDeployed == m_bWantDeploy)
		m_bWantDeploy = !m_bWantDeploy;

	if (m_bDeployed == m_bWantDeploy)
		return;

	if (!m_bWantDeploy)
	{
		// retracting: centre the gun and wind down, then fold away
		if (pev->sequence == DTURRET_ANIM_NONE)
		{
			m_bDeployed = false;
			return;
		}

		m_vecGoalAngles = g_vecZero;
		m_bWantSpin = false;

		if (pev->sequence != DTURRET_ANIM_RETIRE)
		{
			SetTurretAnim(DTURRET_ANIM_SPIN);
			if (!MoveTurret() && !RunBarrel())
			{
				SetTurretAnim(DTURRET_ANIM_RETIRE);
				EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "turret/tu_deploy.wav", 0.6, 0.8, 0, 120);
			}
			if (pev->sequence != DTURRET_ANIM_RETIRE)
				return;
		}

		if (m_fSequenceFinished)
			SetTurretAnim(DTURRET_ANIM_NONE);
	}
	else
	{
		if (pev->sequence != DTURRET_ANIM_DEPLOY)
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "turret/tu_deploy.wav", 0.6, 0.8, 0, 100);

		SetTurretAnim(DTURRET_ANIM_DEPLOY);
		if (m_fSequenceFinished)
		{
			SetTurretAnim(DTURRET_ANIM_SPIN);
			m_bWantSpin = true;
			m_bSpunUp = false;
			m_bDeployed = true;
			m_fSpinDownTime = gpGlobals->time + 6;
		}
	}
}

// orig 0x175da0: clamps the goal; true while the gun is not yet on it
bool CDriveTurret::MoveTurret()
{
	if (m_vecGoalAngles.x < -90)
		m_vecGoalAngles.x = -90;
	else if (m_vecGoalAngles.x > 15)
		m_vecGoalAngles.x = 15;
	m_vecGoalAngles.y = UTIL_AngleMod(m_vecGoalAngles.y);

	float flPitchDist = UTIL_AngleDistance(m_vecGoalAngles.x, m_vecCurAngles.x);
	float flYawDist = UTIL_AngleDistance(m_vecGoalAngles.y, m_vecCurAngles.y);

	if (fabs(flPitchDist) < 0.001 && fabs(flYawDist) < 0.001)
		return false;
	return true;
}

// orig 0x175e80: spin the barrel up or down; true while it is spinning or changing
bool CDriveTurret::RunBarrel()
{
	float flInterval = gpGlobals->time - m_fLastBarrelTime;
	m_fLastBarrelTime = gpGlobals->time;

	if (pev->sequence != DTURRET_ANIM_SPIN)
		return false;

	if (m_bSpunUp == m_bWantSpin)
	{
		if (!m_bSpunUp)
		{
			pev->framerate = 0.1;
			m_iSpinSound = 0;
			return false;
		}
		pev->framerate = 1;
		m_iSpinSound = 1;
	}
	else if (flInterval >= 0.001)
	{
		if (!m_bWantSpin)
		{
			if (m_iSpinSound == 1)
			{
				EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav", 0, 0, SND_STOP, 100);
				EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "turret/tu_spindown.wav", 0.55, 0.8, 0, 100);
				pev->framerate = 1;
				m_iSpinSound = 0;
			}
			pev->framerate = pev->framerate - flInterval * 1.3;
			if (pev->framerate <= 0.1)
			{
				m_bSpunUp = false;
				pev->framerate = 0.1;
				return false;
			}
		}
		else
		{
			if (m_iSpinSound == 0)
			{
				EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "turret/tu_spinup.wav", 0.55, 0.8, 0, 100);
				pev->framerate = 0.1;
				m_iSpinSound = 1;
			}
			pev->framerate = flInterval * 1.7 + pev->framerate;
			if (pev->framerate >= 1)
			{
				m_bSpunUp = true;
				pev->framerate = 1;
				EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav", 0.55, 0.8, 0, 100);
				m_fNextPing = gpGlobals->time + 1;
			}
		}
	}
	else if (!m_bSpunUp && !m_bWantSpin)
	{
		return false;
	}

	return true;
}

// orig 0x1760e0: one bullet, CBaseEntity::FireBullets style
void CDriveTurret::TuFireBullets(Vector vecSrc, Vector vecDir, Vector vecSpread)
{
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;

	ClearMultiDamage();
	gMultiDamage.type = DMG_BULLET | DMG_NEVERGIB;

	float x, y, z;
	do
	{
		x = RANDOM_FLOAT(-0.5, 0.5);
		x += RANDOM_FLOAT(-0.5, 0.5);
		y = RANDOM_FLOAT(-0.5, 0.5);
		y += RANDOM_FLOAT(-0.5, 0.5);
		z = x * x + y * y;
	} while (z > 1);

	Vector vecDirShooting = vecDir + x * vecSpread.x * vecRight + y * vecSpread.y * vecUp;
	Vector vecEnd = vecSrc + vecDirShooting * 8192;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

	m_iShotCount++;
	bool bTracer = (m_iShotCount & 1) == 0;
	if (bTracer)
	{
		Vector vecTracerSrc = vecSrc - vecUp * 4;
		MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecTracerSrc);
		WRITE_BYTE(TE_TRACER);
		WRITE_COORD(vecTracerSrc.x);
		WRITE_COORD(vecTracerSrc.y);
		WRITE_COORD(vecTracerSrc.z);
		WRITE_COORD(tr.vecEndPos.x);
		WRITE_COORD(tr.vecEndPos.y);
		WRITE_COORD(tr.vecEndPos.z);
		MESSAGE_END();
		m_iShotCount = 0;
	}

	if (tr.flFraction != 1.0)
	{
		CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
		if (pEntity)
		{
			if (!m_pOwner)
				pEntity->TraceAttack(pev, PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretDamagePerBullet", 13), vecDirShooting, &tr, DMG_BULLET | DMG_NEVERGIB);
			else
				pEntity->TraceAttack(m_pOwner->pev, PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretDamagePerBullet", 13), vecDirShooting, &tr, DMG_BULLET | DMG_NEVERGIB);

			if (!bTracer && pEntity->IsBSPModel() && CVAR_GET_FLOAT("sv_lowlag") != 1)
			{
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, BULLET_MONSTER_12MM);
				UTIL_GunshotDecalTrace(&tr, pEntity->DamageDecal(DMG_BULLET));
			}
		}
	}

	UTIL_BubbleTrail(vecSrc, tr.vecEndPos, (int)(tr.flFraction * 8192 * 0.015625));

	if (!m_pOwner)
		ApplyMultiDamage(pev, pev);
	else
		ApplyMultiDamage(pev, m_pOwner->pev);
}

// orig 0x176650: stand on the hit plane
void CDriveTurret::AttachToPlane(TraceResult* pTrace)
{
	if (pTrace->flFraction >= 1.0)
		return;

	m_vecNormal = pTrace->vecPlaneNormal;
	pev->angles = UTIL_VecToAngles(m_vecNormal);
	pev->angles.x = pev->angles.x - 90;
}

// orig 0x1766e0: bullets, clubs and blasts ricochet (anything while retracted)
void CDriveTurret::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (IsAlive())
	{
		if (((bitsDamageType & (DMG_BULLET | DMG_CLUB | DMG_BLAST)) != 0 || !m_bDeployed) && (pev->dmg_save != gpGlobals->time || RANDOM_LONG(0, 100) < 20))
		{
			UTIL_Ricochet(ptr->vecEndPos, RANDOM_FLOAT(0.6, 0.9));
			pev->dmg_save = gpGlobals->time;
		}
		CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
		return;
	}

	if (pev->dmg_save == gpGlobals->time && RANDOM_LONG(0, 100) > 19)
		return;

	UTIL_Ricochet(ptr->vecEndPos, RANDOM_FLOAT(0.6, 0.9));
	pev->dmg_save = gpGlobals->time;
}

// orig 0x176860: armored; retracted it shrugs off bullets entirely
bool CDriveTurret::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (!IsAlive())
		return false;

	if (m_pOwner)
	{
		CBaseEntity* pAttacker = CBaseEntity::Instance(pevAttacker->pContainingEntity);
		if (pAttacker && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, pAttacker))
			return false;
		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	if (!m_bDeployed)
	{
		flDamage *= 0.2;
		if ((bitsDamageType & DMG_BULLET) != 0)
			return false;
	}
	else if ((bitsDamageType & DMG_BULLET) != 0)
	{
		flDamage *= 0.6;
	}

	if ((bitsDamageType & DMG_MORTAR) != 0)
		flDamage *= 0.7;
	if ((bitsDamageType & DMG_BLAST) != 0)
		flDamage *= 0.61;
	if ((bitsDamageType & DMG_BURN) != 0)
		flDamage *= 0.4;
	if ((bitsDamageType & DMG_ACID) != 0)
		flDamage *= 0.2;
	if ((bitsDamageType & DMG_ENERGYBEAM) != 0)
		flDamage *= 0.38;
	if ((bitsDamageType & (DMG_SLASH | DMG_CLUB)) != 0)
		flDamage *= 0.48;

	// VERIFY: guard, the original crashes here: it does not test m_pOwner (no driver)
	if (m_pOwner)
		m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 2;

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x176a30: smoke and sparks, then it explodes and goes
void CDriveTurret::DyingThink()
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->deadflag != DEAD_DEAD)
	{
		pev->deadflag = DEAD_DEAD;

		float flRndSound = RANDOM_FLOAT(0, 1);
		const char* pszSound;
		if (flRndSound <= 0.33)
			pszSound = "turret/tu_die.wav";
		else if (flRndSound <= 0.66)
			pszSound = "turret/tu_die2.wav";
		else
			pszSound = "turret/tu_die3.wav";
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, pszSound, 1.0, 0.8, 0, 100);
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "turret/tu_active2.wav", 0, 0, SND_STOP, 100);

		m_vecGoalAngles = g_vecZero;
		SetTurretAnim(DTURRET_ANIM_DIE);
	}

	if (RANDOM_FLOAT(0, 2) + pev->dmgtime > gpGlobals->time)
	{
		Vector vecSrc;
		vecSrc.x = RANDOM_FLOAT(-16, 16);
		vecSrc.y = RANDOM_FLOAT(-16, 16);
		vecSrc.z = RANDOM_FLOAT(-16, 16);
		vecSrc = pev->origin + vecSrc + m_vecNormal * 32;

		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
		WRITE_BYTE(TE_SMOKE);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_SHORT(g_sModelIndexSmoke);
		WRITE_BYTE(RANDOM_LONG(22, 25)); // scale * 10
		WRITE_BYTE(7);					 // framerate
		MESSAGE_END();
	}

	if (RANDOM_FLOAT(0, 5) + pev->dmgtime > gpGlobals->time)
	{
		Vector vecSrc;
		vecSrc.x = RANDOM_FLOAT(-24, 24);
		vecSrc.y = RANDOM_FLOAT(-24, 24);
		vecSrc.z = RANDOM_FLOAT(-24, 24);
		vecSrc = pev->origin + vecSrc + m_vecNormal * 28;
		UTIL_Sparks(vecSrc);
	}

	m_vecGoalAngles = g_vecZero;

	if (!m_fSequenceFinished || pev->dmgtime + 5 >= gpGlobals->time)
		return;

	SetThink(&CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->takedamage = DAMAGE_NO;

	float flDamage = PREFS_KeyLoopUpFloat("DRIVETURRET", "DeathExplodeDamage", 100);
	edict_t* pentOwner = m_pOwner ? m_pOwner->pev->pContainingEntity : pev->pContainingEntity;
	CPointExplo::ExploCreate(pentOwner, pev->origin + m_vecNormal * 24, -m_vecNormal, flDamage, MAKE_STRING("turret"));
}

// orig 0x177030
void CDriveTurret::SetTurretAnim(DTURRET_ANIM anim)
{
	if (pev->sequence == anim)
		return;

	bool bSpinSlow = false;
	if (anim > DTURRET_ANIM_NONE && anim < DTURRET_ANIM_DEPLOY)
	{
		// FIRE and SPIN carry on each other's frame
		if ((unsigned int)(pev->sequence - 1) > 1)
		{
			pev->frame = 0;
			bSpinSlow = anim == DTURRET_ANIM_SPIN;
		}
	}
	else
	{
		pev->frame = 0;
	}

	pev->sequence = anim;
	ResetSequenceInfo();

	if (anim == DTURRET_ANIM_RETIRE)
	{
		pev->frame = 255;
		pev->framerate = -1.2;
	}
	else if (anim == DTURRET_ANIM_DIE)
	{
		pev->framerate = 1;
	}

	if (bSpinSlow)
		pev->framerate = 0.1;
}

// orig 0x177100: the turret dies, and the driver with it
void CDriveTurret::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->deadflag != DEAD_NO)
		return;

	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	if (m_pEyeGlow && !FNullEnt(m_pEyeGlow->pev))
	{
		edict_t* pent = m_pEyeGlow->pev->pContainingEntity;
		if (pent && pent->pvPrivateData && !pent->free)
		{
			m_pEyeGlow->pev->effects |= EF_NODRAW;
			m_pEyeGlow->SUB_Remove();
		}
	}

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_BYTE(4);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_SHORT(0);
		MESSAGE_END();
	}

	// tell the pickup it can respawn
	// VERIFY: guard, the original crashes here: it calls through Instance()'s result unchecked
	// (NULL once the pickup's edict is freed)
	if (pev->owner && CBaseEntity::Instance(pev->owner))
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	pev->classname = MAKE_STRING("turret");
	DEClientLeave();

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		UTIL_SetOrigin(m_pOwner->pev, m_pOwner->m_pCurDriveEnt->pev->origin + Vector(0, 0, 64));
		m_pOwner->pev->effects |= EF_NODRAW;
		m_pOwner->pev->health = -1;
		m_pOwner->Killed(pevAttacker, GIB_NEVER);
	}

	SetTurretAnim(DTURRET_ANIM_DIE);
	SetThink(&CDriveTurret::DyingThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = gpGlobals->time;
}

// orig 0x177410: from the driver's PreThink: aim the gun and look through the view entity
void CDriveTurret::DEClientPreThink()
{
	UpdateBarrel();

	float flInterval = gpGlobals->time - m_fLastAimTime;
	if (m_vecGoalAngles.x < -90)
		m_vecGoalAngles.x = -90;
	else if (m_vecGoalAngles.x > 15)
		m_vecGoalAngles.x = 15;
	m_vecGoalAngles.y = UTIL_AngleMod(m_vecGoalAngles.y);
	m_fLastAimTime = gpGlobals->time;

	float flPitchStep = flInterval * m_vecTurnRate.x;
	float flYawStep = flInterval * m_vecTurnRate.y;
	m_vecCurAngles.x = UTIL_ApproachAngle(m_vecGoalAngles.x, m_vecCurAngles.x, flPitchStep);
	m_vecCurAngles.y = UTIL_ApproachAngle(m_vecGoalAngles.y, m_vecCurAngles.y, flYawStep);

	float flPitchDist = UTIL_AngleDistance(m_vecGoalAngles.x, m_vecCurAngles.x);
	float flYawDist = UTIL_AngleDistance(m_vecGoalAngles.y, m_vecCurAngles.y);
	if (fabs(flPitchDist) >= 0.001 || !(fabs(flYawDist) < 0.001))
	{
		m_vecCurAngles.y = SetBoneController(0, m_vecCurAngles.y);
		m_vecCurAngles.x = SetBoneController(1, m_vecCurAngles.x);
	}
	else
	{
		m_vecCurAngles.x = m_vecGoalAngles.x;
		m_vecCurAngles.y = m_vecGoalAngles.y;
		SetBoneController(1, m_vecGoalAngles.x);
		SetBoneController(0, m_vecGoalAngles.y);
	}

	if (flPitchStep == 0 || flYawStep == 0)
		pev->fuser4 = 0;
	else
		pev->fuser4 = (flPitchDist / flPitchStep + flYawDist / flYawStep) * 0.5 * 200;

	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;
	m_pOwner->pev->iuser3 = 8;
	m_pOwner->pev->euser2 = ENT(pev);
	m_pOwner->pev->euser3 = m_pEyeGlow->pev->pContainingEntity;
	m_pOwner->pev->view_ofs = g_vecZero;

	if (!m_bDeployed)
	{
		UTIL_SetOrigin(m_pViewEnt->pev, pev->origin + m_vecNormal * 16);
		UTIL_SetOrigin(m_pOwner->pev, m_pViewEnt->pev->origin);
		m_pViewEnt->pev->v_angle = pev->angles;
		m_pViewEnt->pev->angles = m_pViewEnt->pev->v_angle;
	}
	else
	{
		UTIL_SetOrigin(m_pViewEnt->pev, pev->origin + m_vecNormal * 28);
		UTIL_SetOrigin(m_pOwner->pev, m_pViewEnt->pev->origin);
		m_pViewEnt->pev->v_angle = DTGunAngles(pev, m_vecCurAngles);
		m_pViewEnt->pev->angles = m_pViewEnt->pev->v_angle;

		// the ID readout looks along the gun
		Vector vecSaved = m_pOwner->pev->v_angle;
		m_pOwner->pev->v_angle = m_pViewEnt->pev->angles;
		if (m_pOwner->m_flIDTime < gpGlobals->time)
		{
			m_pOwner->PlayerDoID();
			m_pOwner->m_flIDTime = gpGlobals->time + 0.3;
		}
		m_pOwner->pev->v_angle = vecSaved;
	}

	SetupVisibility(m_pViewEnt->pev->pContainingEntity, m_pOwner->pev->pContainingEntity, NULL, NULL);
	SET_VIEW(m_pOwner->pev->pContainingEntity, m_pViewEnt->pev->pContainingEntity);
}

// orig 0x177a60
void CDriveTurret::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x177a80: the player gets in; his view moves to a camera entity at the gun
void CDriveTurret::DEClientEnter(CBasePlayer* pPlayer)
{
	m_pOwner = pPlayer;

	pPlayer->pev->deadflag = DEAD_DEAD;
	m_pOwner->pev->health = 1;
	m_pOwner->pev->takedamage = DAMAGE_NO;
	m_pOwner->pev->movetype = MOVETYPE_NOCLIP;
	m_pOwner->pev->solid = SOLID_NOT;
	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;
	m_pOwner->pev->flags = FL_SPECTATOR | FL_NOTARGET | FL_CLIENT | FL_FLY;
	m_pOwner->pev->classname = MAKE_STRING("client_in_cam");
	m_pOwner->pev->view_ofs = g_vecZero;
	UTIL_SetSize(m_pOwner->pev, g_vecZero, g_vecZero);
	m_pOwner->pev->armorvalue = 0;
	m_pOwner->pev->v_angle = g_vecZero;
	m_pOwner->pev->angles = m_pOwner->pev->v_angle;
	m_pOwner->pev->fixangle = 1;
	m_pOwner->RemoveAllItems(false);

	m_pOwner->m_bInDriveEnt = true;
	m_pOwner->m_pCurDriveEnt = this;
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 5;

	if (m_pOwner->m_pActiveItem)
	{
		m_pOwner->m_pActiveItem->Holster();
		m_pOwner->pev->weaponmodel = 0;
	}

	m_pOwner->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT;
	m_pOwner->m_afPhysicsFlags = 0;
	m_iClientHealth = -1;
	m_iClientArmor = -1;

	MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_BYTE(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_BYTE(m_pOwner->m_iHideHUD);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgBattery, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_SHORT(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgFlashBattery, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_BYTE(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgTrain, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_BYTE(0);
	MESSAGE_END();

	pev->ideal_yaw = m_pOwner->pev->v_angle.y;
	pev->idealpitch = m_pOwner->pev->v_angle.x;

	CLIENT_COMMAND(m_pOwner->pev->pContainingEntity, "cl_forwardspeed 0\ncl_backspeed 0\ncl_sidespeed 0\n");

	m_pViewEnt = GetClassPtr((CBaseEntity*)NULL);
	m_pViewEnt->Spawn();
	UTIL_SetOrigin(m_pViewEnt->pev, pev->origin + m_vecNormal * 4);
	UTIL_SetSize(m_pViewEnt->pev, g_vecZero, g_vecZero);
	m_pViewEnt->pev->angles = pev->angles;
	m_pViewEnt->pev->v_angle = m_pViewEnt->pev->angles;
	m_pViewEnt->pev->view_ofs = g_vecZero;
	m_pViewEnt->pev->movetype = MOVETYPE_NONE;
	m_pViewEnt->pev->solid = SOLID_NOT;
	SET_MODEL(m_pViewEnt->pev->pContainingEntity, "models/w_isotopebox.mdl");
	m_pViewEnt->pev->classname = MAKE_STRING("turret_viewent");
	m_pViewEnt->pev->owner = ENT(pev);
	m_pViewEnt->pev->rendermode = kRenderTransColor;
	m_pViewEnt->pev->renderamt = 0;
}

// orig 0x178020: the player gets out (or dies with the turret): make him a player again
void CDriveTurret::DEClientLeave()
{
	if (!m_pOwner || !m_pOwner->pev || !m_pOwner->pev->pContainingEntity || m_pOwner->pev->pContainingEntity->free || !m_pOwner->m_bInDriveEnt)
		return;

	CLIENT_COMMAND(m_pOwner->pev->pContainingEntity, "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");

	m_pOwner->pev->deadflag = DEAD_NO;
	m_pOwner->pev->health = 100;
	m_pOwner->pev->takedamage = DAMAGE_AIM;
	m_pOwner->pev->movetype = MOVETYPE_WALK;
	m_pOwner->pev->solid = SOLID_SLIDEBOX;
	m_pOwner->pev->rendermode = kRenderNormal;
	m_pOwner->pev->renderamt = 255;
	m_pOwner->pev->effects &= ~EF_NODRAW;
	m_pOwner->pev->flags = FL_CLIENT;
	m_pOwner->pev->classname = MAKE_STRING("player");
	m_pOwner->pev->view_ofs = Vector(0, 0, 28);
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 20;
	m_pOwner->m_fDrcEntTransTime = gpGlobals->time + 15;
	UTIL_SetSize(m_pOwner->pev, Vector(-16, -16, -36), Vector(16, 16, 36));

	if (m_pOwner->m_pActiveItem)
		m_pOwner->m_pActiveItem->Deploy();

	// VERIFY: no SET_VIEW back to the player here, unlike the other drive monsters; removing
	// the view entity is what returns his view
	m_pViewEnt->SUB_Remove();

	m_pOwner->pev->euser2 = NULL;
	m_pOwner->pev->iuser3 = 0;
	m_pOwner->pev->fuser1 = 0;
	m_pOwner->pev->fuser2 = 0;
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;

	m_pOwner->m_iHideHUD &= ~(HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT);
}

// orig 0x1782f0: the driver asks out; he lands in front of the turret and it is destroyed
void CDriveTurret::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVETURRET", "AllowEject", false))
	{
		if (CVAR_GET_FLOAT("sv_allowglobaleject") != 1 && g_pGameRules->IsMultiplayer() && !g_pGameRules->IsCoOp())
			return;
	}

	CBasePlayer* pOwner = m_pOwner;
	if (!pOwner->m_bInDriveEnt || !pOwner->m_pCurDriveEnt || pOwner->m_pCurDriveEnt->pev != pev || pOwner->m_pCurDriveEnt->pev->health <= 0)
		return;

	if (pOwner->m_fDrvEntEjectTime >= gpGlobals->time)
		return;

	DEClientLeave();
	m_pOwner->UpdateClientData();
	m_pOwner->m_bInDriveEnt = false;
	Killed(pev, GIB_ALWAYS);
	UTIL_SetOrigin(m_pOwner->pev, pev->origin + m_vecNormal * 72);
	m_pOwner->m_pCurDriveEnt = NULL;
}

// orig 0x1784a0
LINK_ENTITY_TO_CLASS(pickup_drive_turret, CDriveTurretPickup);

TYPEDESCRIPTION CDriveTurretPickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveTurretPickup, m_pGiven, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveTurretPickup, m_bTakeable, FIELD_BOOLEAN),
};

// orig 0x1784d0 (Save), 0x178520 (Restore)
IMPLEMENT_SAVERESTORE(CDriveTurretPickup, CBaseMonster);

// orig 0x178570: sits on the floor or ceiling nearest above or below an
// info_drive_turret_spawn, which it removes
void CDriveTurretPickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), "models/turret.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, -32), Vector(32, 32, 32));
	UTIL_SetOrigin(pev, pev->origin);
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->sequence = 0;
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);
	InitBoneControllers();

	SetUse(&CDriveTurretPickup::Use);
	SetThink(&CDriveTurretPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_bTakeable = true;
	m_pGiven = NULL;

	CBaseEntity* pSpot = GetSpawnOrg();
	if (!pSpot)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	UTIL_Remove(pSpot); // flags it; its pev is still readable below
	pev->angles = g_vecZero;

	TraceResult trUp, trDown;
	UTIL_TraceLine(pSpot->pev->origin, pSpot->pev->origin + Vector(0, 0, 256), ignore_monsters, ENT(pev), &trUp);
	UTIL_TraceLine(pSpot->pev->origin, pSpot->pev->origin - Vector(0, 0, 256), ignore_monsters, ENT(pev), &trDown);

	// only a level ceiling or floor counts
	if (trUp.vecPlaneNormal != Vector(0, 0, -1))
		trUp.flFraction = 2;
	if (trDown.vecPlaneNormal != Vector(0, 0, 1))
		trDown.flFraction = 2;

	if (trUp.flFraction >= 1.0 && trDown.flFraction >= 1.0)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	if (trUp.flFraction < trDown.flFraction)
	{
		m_vecNormal = trUp.vecPlaneNormal;
		UTIL_SetOrigin(pev, trUp.vecEndPos + m_vecNormal);
		pev->angles.y = UTIL_AngleMod(180 - pSpot->pev->angles.y);
	}
	else
	{
		m_vecNormal = trDown.vecPlaneNormal;
		UTIL_SetOrigin(pev, trDown.vecEndPos + m_vecNormal);
		pev->angles.y = pSpot->pev->angles.y;
	}

	pev->angles.x = UTIL_VecToAngles(m_vecNormal).x - 90;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	pev->renderfx = kRenderFxDistort;
	DoRespawn();
}

// orig 0x178a80
void CDriveTurretPickup::Precache()
{
	PRECACHE_MODEL("models/turret.mdl");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_turret");
}

// orig 0x178ad0: the first spawn spot not already being removed
CBaseEntity* CDriveTurretPickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_turret_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (FNullEnt(pSpot->pev))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x178b50
bool CDriveTurretPickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return false;
}

// orig 0x178b60: fades the ghost in; while given out, ends the drive when its time is up
void CDriveTurretPickup::PAnimThink()
{
	float flInterval = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (!m_bTakeable)
	{
		if (m_pGiven && m_pGiven->pev->deadflag == DEAD_NO && pev->dmgtime < gpGlobals->time)
		{
			g_pevLastInflictor = pev;
			m_pGiven->Killed(pev, GIB_NORMAL);
			pev->dmgtime = gpGlobals->time + 1e+06;
		}
		if (!m_bTakeable)
			return;
	}

	if (pev->rendermode != kRenderTransTexture || pev->renderamt >= 150)
	{
		pev->rendermode = kRenderTransTexture;
		pev->renderamt = 150;
		pev->renderfx = kRenderFxDistort;
	}
	else
	{
		pev->renderamt = flInterval * 56.689342 + pev->renderamt;
		if (pev->renderamt >= 150)
		{
			pev->rendermode = kRenderTransTexture;
			pev->renderamt = 150;
			pev->renderfx = kRenderFxDistort;
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 0, 0, SND_STOP, 100);
		}
	}
}

// orig 0x178d00: a player uses the ghost: make a drive_turret and put him in it
void CDriveTurretPickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!m_bTakeable)
		return;

	// VERIFY: the original tests pCaller (not pActivator), and calls IsPlayer before the null test
	if (!pCaller->IsPlayer() || !pCaller)
		return;

	CBasePlayer* pPlayer = (CBasePlayer*)pCaller;
	if (pPlayer->m_fDrcEntTransTime > gpGlobals->time || pPlayer->m_bInDriveEnt)
		return;
	if (pPlayer->Classify() != CLASS_PLAYER)
		return;
	if (strcmp(STRING(pPlayer->pev->classname), "player") != 0)
		return;

	// the plane normal goes in as the angles: CDriveTurret::Spawn reads it from there
	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_turret", pev->origin, m_vecNormal, ENT(pev));
	pPlayer->m_pCurDriveEnt->pev->angles.y = pev->angles.y;
	pPlayer->pev->angles = Vector(0, pev->angles.y, 0);
	pPlayer->pev->v_angle = pPlayer->pev->angles;
	pPlayer->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGiven = pPlayer->m_pCurDriveEnt;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDriveTurretPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretMaxDriveTime", 420) + gpGlobals->time;
}

// orig 0x178f10
void CDriveTurretPickup::DoRespawn()
{
	m_bTakeable = true;
	m_pGiven = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);
	SetThink(&CDriveTurretPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x178fd0: the drive_turret it made has died
void CDriveTurretPickup::DeathNotice(entvars_t* pevChild)
{
	if (m_bTakeable)
		return;

	pev->nextthink = PREFS_KeyLoopUpFloat("DRIVETURRET", "TurretRespawnDelayAfterDeath", 5) + gpGlobals->time;
	SetThink(&CDriveTurretPickup::DoRespawn);
}

// orig 0x179040: in multiplayer, one drive_turret_spawner per map
void CreateDrvTurretSpawner()
{
	if (!g_pGameRules->IsMultiplayer())
		return;

	if (!UTIL_FindEntityByClassname(NULL, "drive_turret_spawner"))
		CBaseEntity::Create("drive_turret_spawner", g_vecZero, g_vecZero, NULL);
}

// orig 0x1790a0
LINK_ENTITY_TO_CLASS(drive_turret_spawner, CDrvTurretSpawner);

TYPEDESCRIPTION CDrvTurretSpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvTurretSpawner, m_bDummy, FIELD_BOOLEAN),
};

// orig 0x1790d0 (Save), 0x179120 (Restore)
IMPLEMENT_SAVERESTORE(CDrvTurretSpawner, CBaseEntity);

// orig 0x179170: makes mp_driveturret pickups, then goes
void CDrvTurretSpawner::DrvAThink()
{
	int iCount = (int)CVAR_GET_FLOAT("mp_driveturret");
	for (int i = 0; i < iCount; i++)
		CBaseEntity::Create("pickup_drive_turret", g_vecZero, g_vecZero, NULL);

	UTIL_Remove(this);
}

// orig 0x179200
void CDrvTurretSpawner::Spawn()
{
	pev->classname = MAKE_STRING("drive_turret_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvTurretSpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15;
}

// orig 0x1792a0
int CDrvTurretSpawner::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}

// orig 0x179300
int CDriveTurretPickup::ObjectCaps()
{
	return m_bTakeable ? (FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE) : FCAP_ACROSS_TRANSITION;
}

// orig 0x179320
int CDriveTurretPickup::GetRCCaps()
{
	return 0;
}

// orig 0x179330
bool CDriveTurretPickup::IsTeleFraggabe()
{
	return false;
}

// orig 0x179340
int CDriveTurretPickup::BloodColor()
{
	return DONT_BLEED;
}

// orig 0x179350
int CDriveTurretPickup::Classify()
{
	return CLASS_NONE;
}

// orig 0x1793b0
void CDriveTurret::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;
	if (m_pViewEnt && m_pViewEnt->pev && m_pViewEnt->pev->pContainingEntity == pEdict)
		m_pViewEnt = NULL;
	if (m_pEyeGlow && m_pEyeGlow->pev && m_pEyeGlow->pev->pContainingEntity == pEdict)
		m_pEyeGlow = NULL;

	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

// orig 0x179480
float CDriveTurret::VeryHeavyDamageFract()
{
	return 0.6;
}

// orig 0x1794a0
float CDriveTurret::ExtreemeDamageFract()
{
	return 0.04;
}

// orig 0x1794c0
int CDriveTurret::GetRCCaps()
{
	return 0x2082;
}

// orig 0x1794d0
int CDriveTurret::BloodColor()
{
	return DONT_BLEED;
}


