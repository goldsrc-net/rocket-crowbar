/***
*
*	Rocket Crowbar 1.9: drivebm.cpp. The drivable Big Momma (Gonarch): drive_bm, the
*	pickup_drive_bm a player uses to get in, the drive_bm_spawner that places the pickups, and
*	the gonarch_headcrabifier ring it fires. Reconstructed from rc_i386.so (addresses are file
*	addresses, reference/funcfile.tsv); see reference/dossiers/bm-garg.md.
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
#include "decals.h"
#include "UserMessages.h"
#include "driveent.h"
#include "drivebm.h"

// Preff.cpp (prefs-votes package)
extern int PREFS_KeyLoopUpInt(const char* szHeading, const char* szKey, int iDefault);
extern float PREFS_KeyLoopUpFloat(const char* szHeading, const char* szKey, float flDefault);
extern bool PREFS_KeyLoopUpBool(const char* szHeading, const char* szKey, bool bDefault);
// drivehc.cpp (chum-icky-hc package): turn an entity into a headcrab pOwner drives
extern void DHCHeadcrabifyEnt(edict_t* pEnt, edict_t* pOwner);

const char* g_sDBMMeleeHitSounds[] = {"zombie/claw_strike1.wav", "zombie/claw_strike2.wav", "zombie/claw_strike3.wav"};
const char* g_sDBMMeleeMissSounds[] = {"zombie/claw_miss1.wav", "zombie/claw_miss2.wav"};
const char* g_sDBMWalkSounds[] = {"gonarch/gon_step1.wav", "gonarch/gon_step2.wav", "gonarch/gon_step3.wav"};
const char* g_sDBMIdleSounds[] = {"gonarch/gon_sack1.wav", "gonarch/gon_sack2.wav", "gonarch/gon_sack3.wav"};
const char* g_sDMBPainSounds[] = {"gonarch/gon_pain2.wav", "gonarch/gon_pain4.wav", "gonarch/gon_pain5.wav"};
const char* g_szDBMBlastSounds[] = {"houndeye/he_blast1.wav", "houndeye/he_blast2.wav", "houndeye/he_blast3.wav"};

//=========================================================
// CDriveBM: the Big Momma a player drives
//=========================================================
class CDriveBM : public CBaseDriveEnt
{
public:
	void Spawn() override;
	void Precache() override;
	void SetObjectCollisionBox() override;
	int Classify() override;
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int BloodColor() override;
	float ExtreemeDamageFract() override;
	float VeryHeavyDamageFract() override;
	const char* GetControllerName() override;
	void EntIsBeingRemoved(edict_t* pEdict) override;
	int GetRCCaps() override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;
	void DEClientPreThink() override;
	void DEClientPostThink() override;
	void DEClientEnter(CBasePlayer* pPlayer) override;
	void DEClientLeave() override;
	void DEClientEject() override;
	Vector GetCamRotOrg() override;

	void EXPORT DBMThink();
	void EXPORT DyingThink();
	void DoSlash();
	void DoConc();
	void DoHCF();
	void CheckEHop();
	void DoMovement();
	void DoYaw();
	void DoRegen();
	void UpdateOwnerHUD();
	CBaseEntity* BMCheckTraceHullAttack(float flDist, int iDamage, int iDmgType);
	void CallDoSlash(int iSide, bool bDouble);
	void ConcSplash();

	// Unsaved (the original has no save table for this class). Offsets are the original's.
	float m_flInterval;		  // orig +0x280, this think's StudioFrameAdvance interval
	int m_iLastHealth;		  // orig +0x284, health last sent to the driver's HUD
	Vector m_vecLastOrigin;	  // orig +0x288, for the driver's camera velocity
	float m_flLastOriginTime; // orig +0x294
	bool m_bCanMove;		  // orig +0x298, VERIFY name
	bool m_bMoving;			  // orig +0x299, VERIFY name
	bool m_bCanSlash;		  // orig +0x29a, VERIFY name
	bool m_bCanConc;		  // orig +0x29b, VERIFY name
	bool m_bCanHCF;			  // orig +0x29c, VERIFY name
	float m_flNextIdle;		  // orig +0x2a0
	bool m_bCanIdle;		  // orig +0x2a4, VERIFY name
	bool m_bInSlash;		  // orig +0x2a5
	float m_flNextSlash;	  // orig +0x2a8
	float m_flLastSlash;	  // orig +0x2ac
	bool m_bSlashSide;		  // orig +0x2b0, which arm swings next. VERIFY name
	bool m_bInConc;			  // orig +0x2b1
	float m_flNextConc;		  // orig +0x2b4
	bool m_bInHCF;			  // orig +0x2b8
	float m_flNextHCF;		  // orig +0x2bc
	int m_iHCFRingsLeft;	  // orig +0x2c0
	float m_flNextHCFRing;	  // orig +0x2c4
	Vector m_vecMoveVel;	  // orig +0x2c8, velocity of the last walk step
	float m_flMoveTime;		  // orig +0x2d4
	float m_flNextEHop;		  // orig +0x2d8
	float m_flPainTime;		  // orig +0x2e0
	int m_iShockwaveSprite;	  // orig +0x2e4
};

LINK_ENTITY_TO_CLASS(drive_bm, CDriveBM);

// orig 0x13aac0
void CDriveBM::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_bm");
	SET_MODEL(ENT(pev), "models/big_mom.mdl");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-24, -24, 0), Vector(24, 24, 64));
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	pev->health = PREFS_KeyLoopUpFloat("DRIVEBM", "BMHealth", 1000);
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	pev->sequence = LookupSequence("idle1");
	ResetSequenceInfo();
	InitBoneControllers();
	pev->frame = 0;
	m_flPainTime = gpGlobals->time;
	pev->framerate = 1;
	SetEyePosition();
	m_flNextIdle = RANDOM_FLOAT(3, 8) + gpGlobals->time;
	m_bCanIdle = true;
	m_bMoving = false;
	m_bCanMove = true;
	m_bCanSlash = true;
	m_bCanConc = true;
	m_bCanHCF = true;
	m_bInSlash = false;
	m_flNextSlash = gpGlobals->time;
	m_bInConc = false;
	m_flNextConc = gpGlobals->time;
	m_bInHCF = false;
	m_flNextHCF = gpGlobals->time;
	m_flNextHCFRing = -1;
	m_flLastSlash = -1;
	m_bSlashSide = RANDOM_LONG(0, 1) != 0;
	m_flMoveTime = gpGlobals->time;
	m_vecMoveVel = g_vecZero;
	m_flNextEHop = gpGlobals->time;
	SetThink(&CDriveBM::DBMThink);
	pev->nextthink = 0.1 + gpGlobals->time;
}

// orig 0x13ad80
void CDriveBM::Precache()
{
	PRECACHE_MODEL("models/big_mom.mdl");
	m_iShockwaveSprite = PRECACHE_MODEL("sprites/shockwave.spr");
	for (unsigned int i = 0; i < 3; i++)
		PRECACHE_SOUND(g_sDBMMeleeHitSounds[i]);
	for (unsigned int i = 0; i < 2; i++)
		PRECACHE_SOUND(g_sDBMMeleeMissSounds[i]);
	for (unsigned int i = 0; i < 3; i++)
		PRECACHE_SOUND(g_sDBMWalkSounds[i]);
	for (unsigned int i = 0; i < 3; i++)
		PRECACHE_SOUND(g_sDBMIdleSounds[i]);
	for (unsigned int i = 0; i < 3; i++)
		PRECACHE_SOUND(g_sDMBPainSounds[i]);
	for (unsigned int i = 0; i < 3; i++)
		PRECACHE_SOUND(g_szDBMBlastSounds[i]);
	PRECACHE_SOUND("gonarch/gon_die1.wav");
	PRECACHE_SOUND("weapons/explode5.wav");
	UTIL_PrecacheOther("gonarch_headcrabifier");
}

// orig 0x13aec0
const char* CDriveBM::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;
	return STRING(m_pOwner->pev->netname);
}

// orig 0x13af30
void CDriveBM::DBMThink()
{
	m_flInterval = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_pOwner)
	{
		const bool bNull = FNullEnt(m_pOwner->pev);
		edict_t* pent;
		if (!bNull && m_pOwner->pev && (pent = m_pOwner->pev->pContainingEntity) != NULL && pent->pvPrivateData && !pent->free && UTIL_PlayerByIndex(ENTINDEX(pent)))
		{
			if (pev->waterlevel >= 2)
			{
				// drowned: the original dies without letting go of the driver here
				pev->classname = MAKE_STRING("gonarch");
				Killed(pev, GIB_NORMAL);
				return;
			}

			if (pev->waterlevel != 0)
				pev->velocity.z = pev->velocity.z - m_flInterval * 80;

			DispatchAnimEvents(m_flInterval);

			m_pOwner->pev->rendermode = kRenderTransColor;
			m_pOwner->pev->renderamt = 0;
			m_pOwner->pev->effects |= EF_NODRAW;

			m_bCanIdle = true;
			m_bCanMove = true;
			m_bCanSlash = true;
			m_bCanConc = true;
			m_bCanHCF = true;
			m_bMoving = false;

			UpdateOwnerHUD();
			DoSlash();
			DoConc();
			DoHCF();
			DoMovement();
			DoYaw();
			DoRegen();

			pev->fuser4 = m_vecMoveVel.Length();

			if (!m_bCanIdle)
			{
				m_flNextIdle = RANDOM_FLOAT(3, 8) + gpGlobals->time;
				return;
			}

			if (m_flNextIdle < gpGlobals->time)
			{
				pev->sequence = LookupSequence("idle1") + RANDOM_LONG(0, 1);
				ResetSequenceInfo();
				pev->frame = 0;
				m_flNextIdle = RANDOM_FLOAT(4, 8) + gpGlobals->time;
				return;
			}

			if (pev->sequence == LookupSequence("idle1"))
				return;
			if (!m_fSequenceFinished)
				return;
			pev->sequence = LookupSequence("idle1");
			ResetSequenceInfo();
			pev->frame = 0;
			return;
		}
	}

	m_pOwner = NULL;
	pev->classname = MAKE_STRING("gonarch");
	Killed(pev, GIB_NORMAL);
}

// orig 0x13b230
void CDriveBM::DoSlash()
{
	if (!m_bCanSlash || m_bInConc || m_bInHCF)
		return;

	if (!m_bInSlash)
	{
		if (m_flNextSlash <= gpGlobals->time && ((m_pOwner->pev->button & IN_ATTACK) != 0 || (m_pOwner->pev->button & IN_ATTACK2) != 0))
		{
			m_bInSlash = true;
			m_bCanIdle = false;
			m_bCanMove = false;

			if ((m_pOwner->pev->button & IN_ATTACK) == 0)
			{
				// secondary alone: the two-armed slash
				m_bSlashSide = RANDOM_LONG(0, 1) != 0;
				pev->sequence = 5;
			}
			else if (m_flLastSlash + 2.0 <= gpGlobals->time)
			{
				m_bSlashSide = RANDOM_LONG(0, 1) != 0;
				pev->sequence = m_bSlashSide ? 7 : 6;
			}
			else
			{
				// alternate arms while slashing in a row
				pev->sequence = m_bSlashSide ? 6 : 7;
				m_bSlashSide = !m_bSlashSide;
			}
			ResetSequenceInfo();
			pev->frame = 0;
		}
	}
	else if (!m_fSequenceFinished)
	{
		m_bCanIdle = false;
		m_bCanMove = false;
	}
	else
	{
		m_bInSlash = false;
		m_flNextSlash = PREFS_KeyLoopUpFloat("DRIVEBM", "BMSlashRefire", 0.2) + gpGlobals->time;
		m_flLastSlash = gpGlobals->time;
	}
}

// orig 0x13b410
void CDriveBM::DoConc()
{
	if (!m_bCanConc || m_bInSlash || m_bInHCF)
		return;

	if (!m_bInConc)
	{
		if (m_flNextConc <= gpGlobals->time && (m_pOwner->pev->button & IN_DUCK) != 0)
		{
			m_bCanIdle = false;
			m_bCanMove = false;
			m_bInConc = true;
			pev->sequence = LookupSequence("spawn");
			ResetSequenceInfo();
			pev->frame = 0;
		}
	}
	else if (!m_fSequenceFinished)
	{
		m_bCanIdle = false;
		m_bCanMove = false;
	}
	else
	{
		m_bInConc = false;
		m_flNextConc = PREFS_KeyLoopUpFloat("DRIVEBM", "BMConcRefire", 0.9) + gpGlobals->time;
	}
}

// orig 0x13b510
void CDriveBM::DoHCF()
{
	if (!m_bCanHCF || m_bInSlash || m_bInConc)
		return;

	if (!m_bInHCF)
	{
		if (m_flNextHCF <= gpGlobals->time && (m_pOwner->pev->button & IN_JUMP) != 0)
		{
			m_bCanIdle = false;
			m_bCanMove = false;
			m_bInHCF = true;
			m_iHCFRingsLeft = PREFS_KeyLoopUpInt("DRIVEBM", "BMHCFCountRings", 2);
			m_flNextHCFRing = gpGlobals->time + 0.2;
			pev->sequence = LookupSequence("defend");
			ResetSequenceInfo();
			pev->frame = 0;
			m_fSequenceLoops = true;
		}
	}
	else if (m_iHCFRingsLeft < 1 && m_fSequenceFinished)
	{
		m_bInHCF = false;
		m_flNextHCF = PREFS_KeyLoopUpFloat("DRIVEBM", "BMHCFRefire", 5.0) + gpGlobals->time;
	}
	else
	{
		m_bCanIdle = false;
		m_bCanMove = false;
		if (m_iHCFRingsLeft > 0 && m_flNextHCFRing != -1 && m_flNextHCFRing < gpGlobals->time)
		{
			UTIL_MakeVectors(pev->angles);
			Vector vecSrc = pev->origin + Vector(0, 0, 64) + gpGlobals->v_forward * 16;
			BMHCFCreate(ENT(pev), m_pOwner->pev->pContainingEntity, vecSrc, gpGlobals->v_forward);
			m_flNextHCFRing = PREFS_KeyLoopUpFloat("DRIVEBM", "BMHCFRingDelay", 0.6) + gpGlobals->time;
			m_iHCFRingsLeft--;
			if (m_iHCFRingsLeft < 1)
				m_fSequenceLoops = false;
		}
	}
}

// orig 0x13b7c0: hop off a ledge in front when there is no ground under the next step
void CDriveBM::CheckEHop()
{
	if (m_flNextEHop > gpGlobals->time || (pev->flags & FL_ONGROUND) == 0)
		return;
	if (pev->velocity.z > 16 || !m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecFront = pev->origin + Vector(0, 0, 4) + gpGlobals->v_forward * 48;
	Vector vecDown = vecFront - Vector(0, 0, 28);

	TraceResult tr;
	UTIL_TraceLine(pev->origin + Vector(0, 0, 4), vecFront, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
	{
		UTIL_TraceLine(vecFront, vecDown, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
		{
			pev->velocity.z = pev->velocity.z + 50;
			pev->velocity = pev->velocity + gpGlobals->v_forward * 100;
			pev->flags &= ~FL_ONGROUND;
			UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
		}
	}
	m_flNextEHop = gpGlobals->time + 0.2;
}

// orig 0x13bae0: walk forward while the driver holds +forward
void CDriveBM::DoMovement()
{
	if ((pev->flags & FL_ONGROUND) == 0)
	{
		m_vecMoveVel = g_vecZero;
		m_flMoveTime = gpGlobals->time;
		if (!m_bCanIdle)
			return;

		// in the air: VERIFY run below 120 and walk above, as the original has it
		float flSpeed = pev->velocity.Make2D().Length();
		const char* pszSeq;
		if (flSpeed > 30 && flSpeed < 120)
		{
			pszSeq = "run";
			if (pev->sequence == LookupSequence("run"))
				return;
		}
		else
		{
			if (!(flSpeed >= 120))
				return;
			if (pev->sequence == LookupSequence("walk"))
				return;
			pszSeq = "walk";
		}
		pev->sequence = LookupSequence(pszSeq);
		ResetSequenceInfo();
		return;
	}

	if (m_bCanMove)
	{
		if ((m_pOwner->pev->button & IN_FORWARD) == 0 || (pev->flags & FL_ONGROUND) == 0)
		{
			if (pev->sequence == LookupSequence("run") || pev->sequence == LookupSequence("walk"))
				m_fSequenceFinished = true;
		}
		else
		{
			m_vecMoveVel = g_vecZero;
			const bool bWalk = (m_pOwner->pev->button & (IN_RUN | IN_USE)) != 0;
			const float flOldFrame = pev->frame;
			const int iOldSequence = pev->sequence;
			bool bWasOther = false;
			if (pev->sequence != LookupSequence("run"))
				bWasOther = pev->sequence != LookupSequence("walk");

			if (bWalk)
			{
				if (pev->sequence != LookupSequence("walk"))
				{
					pev->sequence = LookupSequence("walk");
					ResetSequenceInfo();
				}
			}
			else
			{
				if (pev->sequence != LookupSequence("run"))
				{
					pev->sequence = LookupSequence("run");
					ResetSequenceInfo();
				}
			}

			m_bCanIdle = false;
			m_bMoving = true;
			UTIL_MakeVectors(pev->angles);
			Vector vecGoal = Center() + gpGlobals->v_forward * 512;
			float flMoved = 0;
			Vector vecStart = pev->origin;

			// how far the walk can go this think, checked in 20 unit steps
			float flDist = m_flGroundSpeed * pev->framerate * m_flInterval;
			while (flDist > 0.001)
			{
				float flStep = flDist;
				if (flStep > 20)
					flStep = 20;
				Vector vecOld = pev->origin;
				for (; !WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY) && flStep > 2; flStep -= 2)
					;
				UTIL_SetOrigin(pev, vecOld);
				if (!WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY))
				{
					if (flMoved < 0.01)
					{
						if (bWasOther)
						{
							pev->sequence = iOldSequence;
							ResetSequenceInfo();
							pev->frame = flOldFrame;
						}
						else
							m_fSequenceFinished = true;
						m_bCanIdle = true;
						m_bMoving = false;
						UTIL_SetOrigin(pev, vecStart);
						m_vecMoveVel = g_vecZero;
						m_flMoveTime = gpGlobals->time;
						if (!bWalk)
							CheckEHop();
						return;
					}
					break;
				}
				flDist -= flStep;
				flMoved += flStep;
			}

			UTIL_SetOrigin(pev, vecStart);

			// room for the body ahead?
			Vector vecSrc = gpGlobals->v_forward * 32 + pev->origin + Vector(0, 0, 100);
			Vector vecEnd = vecSrc + flMoved * gpGlobals->v_forward;
			TraceResult tr;
			UTIL_TraceHull(vecSrc, vecEnd, dont_ignore_monsters, human_hull, ENT(pev), &tr);
			if (tr.fAllSolid)
				tr.flFraction = 0;
			flMoved = flMoved * tr.flFraction;
			if (flMoved < 0.1 || tr.flFraction < 0.01 || m_flGroundSpeed * pev->framerate * tr.flFraction < 64)
			{
				if (bWasOther)
				{
					pev->sequence = iOldSequence;
					ResetSequenceInfo();
					pev->frame = flOldFrame;
				}
				else
					m_fSequenceFinished = true;
				m_bCanIdle = true;
				m_bMoving = false;
				return;
			}

			Vector vecPrev = pev->origin;
			const float flOldIdealYaw = pev->ideal_yaw;
			pev->ideal_yaw = pev->angles.y;
			while (flMoved > 0.001)
			{
				float flStep = flMoved;
				if (flStep > 16)
					flStep = 16;
				UTIL_MoveToOrigin(ENT(pev), vecGoal, flStep, MOVE_NORMAL);
				flMoved -= flStep;
			}
			pev->ideal_yaw = flOldIdealYaw;

			Vector vecMoved = pev->origin - vecPrev;
			if (vecMoved.Length() > 0.01 && m_flInterval > 0.001)
			{
				m_vecMoveVel = vecMoved / m_flInterval;
				m_flMoveTime = gpGlobals->time;
				return;
			}
		}
	}

	m_vecMoveVel = g_vecZero;
	m_flMoveTime = gpGlobals->time;
}

// orig 0x13c3a0: turn toward the driver's view yaw, faster when standing
void CDriveBM::DoYaw()
{
	float flSpeed = m_flInterval * 1600;
	const float flYaw = pev->angles.y;
	const float flDelta = fabs(ChangeYaw(0x2000)); // measures the turn left, then undoes it
	pev->angles.y = flYaw;
	if (flDelta < 0.001f)
		return;
	if (flDelta > 70 && !m_bMoving)
		flSpeed = m_flInterval * 2800;
	ChangeYaw((int)flSpeed);
}

// orig 0x13c460: heal for every living driven headcrab in the world
void CDriveBM::DoRegen()
{
	int iCrabs = 0;
	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityByClassname(pEnt, "drive_headcrab")) != NULL)
	{
		if (pEnt->pev && UTIL_IsValidEntity(pEnt->edict()) && pEnt->IsAlive())
			iCrabs++;
	}
	if (iCrabs > 0)
	{
		pev->health = PREFS_KeyLoopUpFloat("DRIVEBM", "BMHealthPerSecPerCrab", 10) * iCrabs * m_flInterval + pev->health;
		if (pev->health > pev->max_health)
			pev->health = pev->max_health;
	}
}

// orig 0x13c540: the driver's HUD health is the monster's, in percent
void CDriveBM::UpdateOwnerHUD()
{
	if (!m_pOwner)
		return;
	if (pev->health == (float)m_iLastHealth)
		return;

	float flPercent = pev->health / pev->max_health * 100;
	int iPercent = flPercent > 0 ? (int)flPercent : 0;
	if (iPercent < 1)
		iPercent = 1;
	else if (iPercent > 100)
		iPercent = 100;

	MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_SHORT(iPercent);
	MESSAGE_END();
	m_iLastHealth = (int)pev->health;
}

// orig 0x13c660: only head hits hurt; everything else ricochets
void CDriveBM::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType)
{
	if (ptr->iHitgroup == HITGROUP_HEAD)
	{
		if (m_flPainTime < gpGlobals->time)
		{
			m_flPainTime = RANDOM_LONG(1, 3) + gpGlobals->time;
			const int iPitch = RANDOM_LONG(95, 105);
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_sDMBPainSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, iPitch);
		}
		CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
		return;
	}

	if (pev->dmgtime == gpGlobals->time && RANDOM_LONG(0, 10) >= 1)
		return;
	UTIL_Ricochet(ptr->vecEndPos, RANDOM_FLOAT(1, 2));
	pev->dmgtime = gpGlobals->time;
}

// orig 0x13c7d0
bool CDriveBM::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if ((bitsDamageType & DMG_ACID) != 0)
		return false;

	if (m_pOwner)
	{
		if (CBaseEntity::Instance(pevAttacker) && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, CBaseEntity::Instance(pevAttacker)))
			return false;
		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	if ((bitsDamageType & DMG_MORTAR) != 0)
		flDamage = flDamage * 1.3;

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x13c8f0: the monster dies and takes its driver with it
void CDriveBM::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->deadflag != DEAD_NO)
		return;

	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_BYTE(4);
		MESSAGE_END();
		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_SHORT(0);
		MESSAGE_END();
	}

	// tell the pickup that made this one
	// VERIFY: guard, the original crashes here: it calls through Instance()'s result unchecked
	// (NULL once the pickup's edict is freed)
	if (pev->owner && CBaseEntity::Instance(pev->owner))
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	pev->classname = MAKE_STRING("gonarch");
	DEClientLeave();

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		UTIL_SetOrigin(m_pOwner->pev, m_pOwner->m_pCurDriveEnt->pev->origin + Vector(0, 0, 64));
		m_pOwner->pev->effects |= EF_NODRAW;
		m_pOwner->pev->health = -1;
		m_pOwner->Killed(pevAttacker, GIB_NEVER);
	}

	pev->sequence = LookupSequence("death");
	ResetSequenceInfo();
	pev->frame = 0;
	SetThink(&CDriveBM::DyingThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = gpGlobals->time + 5;
}

// orig 0x13cbc0: play the death animation, then blow up into gibs
void CDriveBM::DyingThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	if (!m_fSequenceFinished && gpGlobals->time < pev->dmgtime)
	{
		DispatchAnimEvents(StudioFrameAdvance());
		return;
	}

	pev->deadflag = DEAD_DEAD;
	if (pev->dmgtime <= gpGlobals->time || (pev->flags & FL_ONGROUND) != 0)
	{
		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_EXPLOSION);
		WRITE_COORD(RANDOM_FLOAT(-20, 20) + pev->origin.x);
		WRITE_COORD(RANDOM_FLOAT(-20, 20) + pev->origin.y);
		WRITE_COORD(RANDOM_FLOAT(64, 72) + pev->origin.z);
		WRITE_SHORT(g_sModelIndexFireball);
		WRITE_BYTE(RANDOM_LONG(0, 10) + 40);
		WRITE_BYTE(12);
		WRITE_BYTE(TE_EXPLFLAG_NONE);
		MESSAGE_END();

		pev->health = -500;
		g_vecAttackDir = Vector(0, 0, 1);
		GibMonster();
	}
}

// orig 0x13cdd0. Nothing in the original calls it.
CBaseEntity* CDriveBM::BMCheckTraceHullAttack(float flDist, int iDamage, int iDmgType)
{
	TraceResult tr;
	UTIL_MakeAimVectors(pev->angles);
	Vector vecStart = pev->origin;
	vecStart.z += 48;
	Vector vecEnd = vecStart + gpGlobals->v_forward * flDist;
	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);

	if (!tr.pHit)
		return NULL;

	CBaseEntity* pEntity = CBaseEntity::Instance(tr.pHit);
	if (iDamage > 0)
		pEntity->TakeDamage(pev, pev, iDamage, iDmgType);
	return pEntity;
}

// orig 0x13cee0: one arm (iSide 1 or 2, from the anim events) or both (bDouble, iSide 0)
void CDriveBM::CallDoSlash(int iSide, bool bDouble)
{
	float flDamage;
	if (bDouble)
	{
		pev->classname = MAKE_STRING("gonarch_double_slash");
		flDamage = PREFS_KeyLoopUpFloat("DRIVEBM", "BMDmgDoubleSlash", 95);
	}
	else
	{
		pev->classname = MAKE_STRING("gonarch_slash");
		flDamage = PREFS_KeyLoopUpFloat("DRIVEBM", "BMDmgSlash", 75);
	}

	// VERIFY: the original computes its own forward/right here but aims the hit and the knock
	// with gpGlobals->v_forward/v_right (whatever the last MakeVectors left there)
	Vector vecForward, vecRight;
	g_engfuncs.pfnAngleVectors(pev->angles, vecForward, vecRight, NULL);

	CBaseEntity* pHit = NULL;
	Vector vecSrc = pev->origin + Vector(0, 0, 64);
	Vector vecEnd = vecSrc + vecForward * 180;
	Vector vecStart = vecSrc + vecForward * 32;
	TraceResult tr;
	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, human_hull, ENT(pev), &tr);
	if (tr.pHit)
		pHit = CBaseEntity::Instance(tr.pHit);

	entvars_t* pevAttacker = pev;
	if (m_pOwner)
		pevAttacker = m_pOwner->pev;

	if (!pHit)
	{
		const int iPitch = RANDOM_LONG(0, 15) + 70;
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_sDBMMeleeMissSounds[RANDOM_LONG(0, 1)], 1.0, 0.8, 0, iPitch);
	}
	else if (pHit->IsBSPModel())
	{
		ClearMultiDamage();
		pHit->TraceAttack(pevAttacker, flDamage, gpGlobals->v_forward, &tr, DMG_CRUSH | DMG_SLASH);
		ApplyMultiDamage(pev, pevAttacker);
		TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + gpGlobals->v_forward * 128, BULLET_NONE);
	}
	else
	{
		int iCap = 0x400000;
		if (pHit->HasRCCap(iCap))
		{
			if (bDouble)
				pHit->pev->velocity = pHit->pev->velocity - gpGlobals->v_forward * 150 - Vector(0, 0, 120);
			else if (iSide == 0)
				pHit->pev->velocity = pHit->pev->velocity - gpGlobals->v_right * 200 + gpGlobals->v_forward * 300 + Vector(0, 0, 200);
			else
				pHit->pev->velocity = pHit->pev->velocity + gpGlobals->v_right * 200 + gpGlobals->v_forward * 300 + Vector(0, 0, 200);

			if (pHit->IsPlayer())
			{
				pHit->pev->punchangle.x = -30;
				pHit->pev->punchangle.y = iSide * 40;
				pHit->pev->punchangle.z = 20;
			}
		}

		const int iPitch = RANDOM_LONG(-5, 5) + 100;
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, g_sDBMMeleeHitSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, iPitch);

		// headcrabs (hers included) are knocked about but not hurt
		if (strcmp(STRING(pHit->pev->classname), "monster_headcrab") != 0 && strcmp(STRING(pHit->pev->classname), "monster_baby_headcrab") != 0 && strcmp(STRING(pHit->pev->classname), "drive_headcrab") != 0)
		{
			ClearMultiDamage();
			pHit->TraceAttack(pevAttacker, flDamage, gpGlobals->v_forward, &tr, DMG_CRUSH | DMG_SLASH);
			ApplyMultiDamage(pev, pevAttacker);
		}
	}

	pev->classname = MAKE_STRING("drive_bm");
}

// orig 0x13d730: the stomp shockwave, damage and knockback falling off with distance
void CDriveBM::ConcSplash()
{
	CBaseEntity* pEntity = NULL;
	Vector vecSrc = pev->origin + Vector(0, 0, 8);
	const float flDmgMax = PREFS_KeyLoopUpFloat("DRIVEBM", "BMDmgConcMax", 60);
	const float flMaxVel = PREFS_KeyLoopUpFloat("DRIVEBM", "BMConcMaxVel", 2500);
	const float flVelBoost = PREFS_KeyLoopUpFloat("DRIVEBM", "BMConcVelBoost", 600);
	const float flRadius = PREFS_KeyLoopUpFloat("DRIVEBM", "BMConcRad", 550);
	pev->classname = MAKE_STRING("gonarch_shockwave");

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecSrc, flRadius)) != NULL)
	{
		if (!pEntity->IsAlive())
			continue;
		int iCap = 0x800000;
		if (!pEntity->HasRCCap(iCap))
			continue;

		float flFraction = 1.0 - (pEntity->pev->origin - vecSrc).Length() / flRadius;
		float flVel = flFraction * flMaxVel + flVelBoost;
		float flDamage = flFraction * flDmgMax;
		if (flVel < 100.0)
			flVel = 100;
		if (flVel > 2000)
			flVel = 2000;

		TraceResult tr;
		UTIL_TraceLine(vecSrc, pEntity->pev->origin, ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEntity->pev->pContainingEntity)
		{
			// behind a wall: punch through thin ones only, weaker
			TraceResult trBack;
			UTIL_TraceLine(pEntity->pev->origin, vecSrc, ignore_monsters, pEntity->pev->pContainingEntity, &trBack);
			float flThickness = (tr.vecEndPos - trBack.vecEndPos).Length();
			if (flThickness > PREFS_KeyLoopUpFloat("DRIVEBM", "BMConcMaxWallPunch", 72))
				continue;
			flDamage = PREFS_KeyLoopUpFloat("DRIVEBM", "BMConcWallPunchDmgFraction", 0.4) * flDamage;
			flVel = PREFS_KeyLoopUpFloat("DRIVEBM", "BMConcWallPunchVelFraction", 0.7) * flVel;
		}

		entvars_t* pevAttacker = pev;
		if (m_pOwner)
			pevAttacker = m_pOwner->pev;

		if (flDamage > 1.0 && strcmp(STRING(pEntity->pev->classname), "monster_headcrab") != 0 && strcmp(STRING(pEntity->pev->classname), "monster_baby_headcrab") != 0 && strcmp(STRING(pEntity->pev->classname), "drive_headcrab") != 0)
		{
			ClearMultiDamage();
			pEntity->TraceAttack(pevAttacker, flDamage, gpGlobals->v_forward, &tr, DMG_NEVERGIB | DMG_SONIC);
			ApplyMultiDamage(pev, pevAttacker);
		}

		pEntity->pev->velocity = pEntity->pev->velocity + (pEntity->pev->origin - vecSrc).Normalize() * flVel;
		if ((pEntity->pev->flags & FL_ONGROUND) != 0)
		{
			pEntity->pev->flags &= ~FL_ONGROUND;
			pEntity->pev->velocity.z = pEntity->pev->velocity.z + 50;
			UTIL_SetOrigin(pEntity->pev, pEntity->pev->origin + Vector(0, 0, 1));
		}
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/explode5.wav", 0.7, 0.8, 0, RANDOM_LONG(-5, 5) + 130);
	const int iPitch = RANDOM_LONG(-5, 5) + 70;
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, g_szDBMBlastSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, iPitch);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z + 16);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z + 2750 + 16);
	WRITE_SHORT(m_iShockwaveSprite);
	WRITE_BYTE(0);	 // startframe
	WRITE_BYTE(0);	 // framerate
	WRITE_BYTE(2);	 // life
	WRITE_BYTE(25);	 // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(230); // r
	WRITE_BYTE(240); // g
	WRITE_BYTE(255); // b
	WRITE_BYTE(250); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	pev->classname = MAKE_STRING("drive_bm");
}

// orig 0x13df90
void CDriveBM::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 1:
	case 3:
	{
		const int iPitch = RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, g_sDBMWalkSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, iPitch);
		break;
	}
	case 2:
	case 4:
	{
		const int iPitch = RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_sDBMWalkSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, iPitch);
		break;
	}
	case 5:
	{
		if (RANDOM_LONG(0, 100) > 29)
			break;
		const int iPitch = RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_sDBMIdleSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, iPitch);
		break;
	}
	case 6:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "gonarch/gon_die1.wav", 1.0, 0.8, 0, 100);
		break;
	case 8:
		CallDoSlash(1, false);
		break;
	case 9:
		CallDoSlash(2, false);
		break;
	case 10:
		CallDoSlash(0, true);
		break;
	case 12:
		ConcSplash();
		break;
	default:
		break;
	}
}

// orig 0x13e160: the driver's view orbits behind the monster; the player entity is moved to the
// camera spot (it is invisible and non-solid while driving)
void CDriveBM::DEClientPreThink()
{
	if ((m_pOwner->pev->button & IN_RELOAD) == 0)
	{
		pev->ideal_yaw = UTIL_AngleMod(m_pOwner->pev->v_angle.y);
		pev->idealpitch = m_pOwner->pev->v_angle.x;
	}

	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;

	UTIL_MakeVectors(m_pOwner->pev->v_angle);
	Vector vecOrg = GetCamRotOrg();
	if (m_vecMoveVel.Length() > 0.01 || pev->velocity.Length() > 0.01)
	{
		// lead the camera by the time since the last walk step
		if (m_flMoveTime < gpGlobals->time)
			vecOrg = GetCamRotOrg() + (gpGlobals->time - m_flMoveTime) * m_vecMoveVel;
	}

	Vector vecEnd = vecOrg - gpGlobals->v_forward * 300;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	TraceResult tr;
	UTIL_TraceLine(vecOrg, vecEnd, ignore_monsters, ENT(pev), &tr);

	Vector vecCam = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;

	int i = 0;
	if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
	{
		// step the camera back toward the monster until it is out of the wall
		for (i = 1; i < 48; i++)
		{
			if (UTIL_PointContents(vecCam) == CONTENTS_EMPTY)
				break;
			vecCam = tr.vecEndPos + (float)i * gpGlobals->v_forward * 8;
		}
	}
	if (i == 48)
	{
		if (tr.flFraction >= 1.0)
			vecCam = tr.vecEndPos;
		else
			vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;
	}
	if (UTIL_PointContents(vecCam) == CONTENTS_SOLID)
		vecCam = vecOrg;

	m_pOwner->m_afPhysicsFlags = 0;
	UTIL_SetOrigin(m_pOwner->pev, vecCam);

	// the player's velocity follows the monster's, for client prediction
	Vector vecDelta = pev->origin - m_vecLastOrigin;
	float flDelta = gpGlobals->time - m_flLastOriginTime;
	m_flLastOriginTime = gpGlobals->time;
	m_vecLastOrigin = pev->origin;
	if (flDelta > 0.001 && vecDelta.Length() > 0.001)
		m_pOwner->pev->velocity = (1.0f / flDelta) * vecDelta;
	else
		m_pOwner->pev->velocity = g_vecZero;

	if (m_pOwner->m_fDrvEntAlpha < 1.0 && m_pOwner->m_fDrvEntAlpha > 0)
	{
		m_pOwner->pev->euser2 = ENT(pev);
		m_pOwner->pev->iuser3 = 4;
		m_pOwner->pev->fuser1 = 2;
		float flAlpha = 0;
		if (m_pOwner->m_fDrvEntAlpha * 255 >= 0)
		{
			flAlpha = 255;
			if (m_pOwner->m_fDrvEntAlpha * 255 <= 255)
				flAlpha = m_pOwner->m_fDrvEntAlpha * 255;
		}
		m_pOwner->pev->fuser2 = flAlpha;
	}
	else
	{
		m_pOwner->pev->euser3 = NULL; // VERIFY: the original clears euser3 here but sets euser2 above
		m_pOwner->pev->iuser3 = 0;
		m_pOwner->pev->fuser1 = 0;
		m_pOwner->pev->fuser2 = 0;
	}
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
}

// orig 0x13e7f0
void CDriveBM::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x13e810: the player becomes an invisible camera and takes the controls
void CDriveBM::DEClientEnter(CBasePlayer* pPlayer)
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

	m_iLastHealth = -1;
	m_vecLastOrigin = pev->origin;
	m_flLastOriginTime = gpGlobals->time;

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
}

// orig 0x13ebf0: give the player back its body
void CDriveBM::DEClientLeave()
{
	edict_t* pent;
	if (!m_pOwner || !m_pOwner->pev || (pent = m_pOwner->pev->pContainingEntity) == NULL || pent->free || !m_pOwner->m_bInDriveEnt)
		return;

	CLIENT_COMMAND(pent, "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");
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
	m_pOwner->pev->euser2 = NULL;
	m_pOwner->pev->iuser3 = 0;
	m_pOwner->pev->fuser1 = 0;
	m_pOwner->pev->fuser2 = 0;
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
	m_pOwner->m_iHideHUD &= ~(HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT);
	SET_VIEW(m_pOwner->pev->pContainingEntity, m_pOwner->pev->pContainingEntity);
	SetupVisibility(m_pOwner->pev->pContainingEntity, m_pOwner->pev->pContainingEntity, NULL, NULL);
}

// orig 0x13ef00: bail out backward; the monster dies
void CDriveBM::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVEBM", "AllowEject", false))
	{
		if (CVAR_GET_FLOAT("sv_allowglobaleject") != 1 && g_pGameRules->IsMultiplayer() && !g_pGameRules->IsCoOp())
			return;
	}

	if (!m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev || !(pev->health > 0))
		return;
	if (!(m_pOwner->m_fDrvEntEjectTime < gpGlobals->time))
		return;

	UTIL_MakeAimVectors(pev->angles);
	Vector vecEnd = pev->origin + Vector(0, 0, 100) - gpGlobals->v_forward * 128;
	TraceResult tr;
	UTIL_TraceLine(pev->origin + Vector(0, 0, 100), vecEnd, ignore_monsters, ENT(pev), &tr);

	DEClientLeave();
	m_pOwner->UpdateClientData();
	m_pOwner->m_bInDriveEnt = false;
	Killed(pev, GIB_ALWAYS);

	Vector vecPos = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecPos = tr.vecEndPos + tr.vecPlaneNormal * 64;
	UTIL_SetOrigin(m_pOwner->pev, vecPos);
	m_pOwner->pev->velocity = gpGlobals->v_forward * -900 + Vector(0, 0, 300);
	m_pOwner->pev->v_angle = pev->angles;
	m_pOwner->pev->angles = m_pOwner->pev->v_angle;
	const float flPitch = m_pOwner->pev->v_angle.x;
	m_pOwner->pev->v_angle.x = -flPitch;
	m_pOwner->pev->angles.x = -flPitch;
	m_pOwner->pev->v_angle.z = 0;
	m_pOwner->pev->angles.z = 0;
	m_pOwner->pev->fixangle = 1;
	m_pOwner->m_pCurDriveEnt = NULL;
	EMIT_SOUND_DYN(m_pOwner->pev->pContainingEntity, CHAN_BODY, "ambience/biggun3.wav", 1.0, 0.8, 0, 100);
}

// orig 0x141970: an entity is going away; if it is the driver, the monster dies
void CDriveBM::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;
	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

// orig 0x1419f0
int CDriveBM::GetRCCaps()
{
	return 0x38088;
}

// orig 0x141a00
float CDriveBM::VeryHeavyDamageFract()
{
	return 0.4;
}

// orig 0x141a20
float CDriveBM::ExtreemeDamageFract()
{
	return 0.05;
}

// orig 0x141a40
Vector CDriveBM::GetCamRotOrg()
{
	return Center();
}

// orig 0x141a80
void CDriveBM::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector(-95, -95, 0);
	pev->absmax = pev->origin + Vector(95, 95, 190);
}

// orig 0x141b50
int CDriveBM::BloodColor()
{
	return BLOOD_COLOR_YELLOW;
}

// orig 0x141b60
int CDriveBM::Classify()
{
	return CLASS_ALIEN_MONSTER;
}

//=========================================================
// CDriveBMPickup: the Big Momma statue a player uses to get in
//=========================================================
class CDriveBMPickup : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	int ObjectCaps() override;
	void SetObjectCollisionBox() override;
	int Classify() override;
	void DeathNotice(entvars_t* pevChild) override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	int BloodColor() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	int GetRCCaps() override;
	virtual bool IsTeleFraggabe(); // VERIFY return type: returns 0, nothing calls it

	CBaseEntity* GetSpawnOrg();
	void EXPORT PDGAnimThink();
	void EXPORT DoRespawn();

	CBaseEntity* m_pGivenBM; // orig +0x27c, the drive_bm made for the current driver
	bool m_bTakeable;		 // orig +0x280
};

LINK_ENTITY_TO_CLASS(pickup_drive_bm, CDriveBMPickup);

TYPEDESCRIPTION CDriveBMPickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveBMPickup, m_pGivenBM, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveBMPickup, m_bTakeable, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDriveBMPickup, CBaseMonster); // orig 0x13f340 (Save), 0x13f390 (Restore)

// orig 0x13f3e0
void CDriveBMPickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), "models/big_mom.mdl");
	UTIL_SetSize(pev, Vector(-64, -64, 0), Vector(64, 64, 190));
	UTIL_SetOrigin(pev, pev->origin);
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->sequence = LookupSequence("idle1");
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);
	InitBoneControllers();
	SetUse(&CDriveBMPickup::Use);
	SetThink(&CDriveBMPickup::PDGAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_bTakeable = true;
	m_pGivenBM = NULL;

	CBaseEntity* pSpot = GetSpawnOrg();
	if (!pSpot)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	UTIL_SetOrigin(pev, pSpot->Center());
	pev->angles = Vector(0, pSpot->pev->angles.y, 0);
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	pev->renderfx = kRenderFxDistort;
	DoRespawn();
	UTIL_Remove(pSpot); // each spawn spot holds one pickup
}

// orig 0x13f620
void CDriveBMPickup::Precache()
{
	PRECACHE_MODEL("models/big_mom.mdl");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_bm");
}

// orig 0x13f670: the first map-placed spawn spot still standing
CBaseEntity* CDriveBMPickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_bm_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (FNullEnt(pSpot->pev))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x13f6f0
bool CDriveBMPickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return false;
}

// orig 0x13f700: fade in while waiting; kill the given drive_bm once its time is up
void CDriveBMPickup::PDGAnimThink()
{
	const float flInterval = StudioFrameAdvance();
	pev->nextthink = 0.1 + gpGlobals->time;

	if (!m_bTakeable)
	{
		if (m_pGivenBM && m_pGivenBM->pev->deadflag == DEAD_NO && pev->dmgtime < gpGlobals->time)
		{
			g_pevLastInflictor = pev;
			m_pGivenBM->Killed(pev, GIB_NORMAL);
			pev->dmgtime = gpGlobals->time + 1000000;
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

// orig 0x13f8a0: a player (the caller) gets in
void CDriveBMPickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!m_bTakeable)
		return;
	// VERIFY: the original calls IsPlayer() before the null test
	if (!pCaller->IsPlayer() || !pCaller)
		return;

	CBasePlayer* pPlayer = (CBasePlayer*)pCaller;
	if (!(pPlayer->m_fDrcEntTransTime <= gpGlobals->time) || pPlayer->m_bInDriveEnt)
		return;
	if (pPlayer->Classify() != CLASS_PLAYER)
		return;
	if (strcmp(STRING(pPlayer->pev->classname), "player") != 0)
		return;

	// the pickup owns the monster, so its death comes back here (DeathNotice)
	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_bm", pev->origin, pev->angles, ENT(pev));
	pPlayer->pev->angles = Vector(0, pev->angles.y, 0);
	pPlayer->pev->v_angle = pPlayer->pev->angles;
	pPlayer->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGivenBM = pPlayer->m_pCurDriveEnt;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDriveBMPickup::PDGAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat("DRIVEBM", "BMMaxDriveTime", 600) + gpGlobals->time;
}

// orig 0x13faa0
void CDriveBMPickup::DoRespawn()
{
	m_bTakeable = true;
	m_pGivenBM = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);
	SetThink(&CDriveBMPickup::PDGAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x13fb60: the drive_bm it gave out died
void CDriveBMPickup::DeathNotice(entvars_t* pevChild)
{
	if (m_bTakeable)
		return;
	pev->nextthink = PREFS_KeyLoopUpFloat("DRIVEBM", "BMRespawnDelayAfterDeath", 8) + gpGlobals->time;
	SetThink(&CDriveBMPickup::DoRespawn);
}

// orig 0x1417f0
int CDriveBMPickup::ObjectCaps()
{
	if (m_bTakeable)
		return FCAP_IMPULSE_USE | FCAP_ACROSS_TRANSITION;
	return FCAP_ACROSS_TRANSITION;
}

// orig 0x141810
void CDriveBMPickup::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector(-95, -95, 0);
	pev->absmax = pev->origin + Vector(95, 95, 190);
}

// orig 0x1418e0
int CDriveBMPickup::GetRCCaps()
{
	return 0;
}

// orig 0x1418f0
bool CDriveBMPickup::IsTeleFraggabe()
{
	return false;
}

// orig 0x141900
int CDriveBMPickup::BloodColor()
{
	return DONT_BLEED;
}

// orig 0x141910
int CDriveBMPickup::Classify()
{
	return CLASS_NONE;
}

//=========================================================
// CDrvBSpawner: places mp_drivebm pickups a while after the map starts
//=========================================================
class CDrvBSpawner : public CBaseEntity
{
public:
	void Spawn() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];
	int ObjectCaps() override;

	void EXPORT DrvAThink();

	bool m_bDummy; // orig +0x34
};

// orig 0x13fbd0: called from CWorld (world.cpp) on map start
void CreateDrvBMSpawner()
{
	if (g_pGameRules->IsMultiplayer() && !UTIL_FindEntityByClassname(NULL, "drive_bm_spawner"))
		CBaseEntity::Create("drive_bm_spawner", g_vecZero, g_vecZero, NULL);
}

LINK_ENTITY_TO_CLASS(drive_bm_spawner, CDrvBSpawner);

TYPEDESCRIPTION CDrvBSpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvBSpawner, m_bDummy, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDrvBSpawner, CBaseEntity); // orig 0x13fc60 (Save), 0x13fcb0 (Restore)

// orig 0x13fd00
void CDrvBSpawner::DrvAThink()
{
	const int iCount = (int)CVAR_GET_FLOAT("mp_drivebm");
	for (int i = 0; i < iCount; i++)
		CBaseEntity::Create("pickup_drive_bm", g_vecZero, g_vecZero, NULL);
	UTIL_Remove(this);
}

// orig 0x13fd90
void CDrvBSpawner::Spawn()
{
	pev->classname = MAKE_STRING("drive_bm_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvBSpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15;
}

// orig 0x141790
int CDrvBSpawner::ObjectCaps()
{
	return FCAP_DONT_SAVE;
}

//=========================================================
// CBMHeadcrabifyer: the ring of eight points and lightning the Big Momma throws; what it
// catches becomes a headcrab the driver owns (DHCHeadcrabifyEnt)
//=========================================================
class CBMHeadcrabifyer : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;

	static CBMHeadcrabifyer* CreateHCF(entvars_t* pevOwner, entvars_t* pevEnemy, Vector vecOrigin, Vector vecDir);
	void SetupPoints();
	void KillEnts();
	void EXPORT BMHCFTouch(CBaseEntity* pOther);
	void EXPORT BMHCFThink();

	// Unsaved. Offsets are the original's.
	CBeam* m_pBeams[8];			 // orig +0x34, beam i joins ring point i to i+1 (7 joins 7 to 0)
	CBaseEntity* m_pRingEnts[8]; // orig +0x54, invisible ents the beams hang on
	Vector m_vecPoints[8];		 // orig +0x74, ring points around the flight direction
	float m_flRadius;			 // orig +0xd4
	bool m_bTraceToggle;		 // orig +0xd8, alternates the two sets of cross traces. VERIFY name
};

LINK_ENTITY_TO_CLASS(gonarch_headcrabifier, CBMHeadcrabifyer);

// orig 0x13fe60: eight points on a circle across the flight direction
void CBMHeadcrabifyer::SetupPoints()
{
	Vector vecAngles = UTIL_VecToAngles(pev->movedir);
	vecAngles.x = 90;
	UTIL_MakeVectors(vecAngles);
	const float r = m_flRadius;
	m_vecPoints[0] = pev->origin + r * gpGlobals->v_forward;
	m_vecPoints[1] = pev->origin + r * gpGlobals->v_forward * 0.70711 + r * gpGlobals->v_right * 0.70711;
	m_vecPoints[2] = pev->origin + r * gpGlobals->v_right;
	m_vecPoints[3] = pev->origin - r * gpGlobals->v_forward * 0.70711 + r * gpGlobals->v_right * 0.70711;
	m_vecPoints[4] = pev->origin - r * gpGlobals->v_forward;
	m_vecPoints[5] = pev->origin - r * gpGlobals->v_forward * 0.70711 - r * gpGlobals->v_right * 0.70711;
	m_vecPoints[6] = pev->origin - r * gpGlobals->v_right;
	m_vecPoints[7] = pev->origin + r * gpGlobals->v_forward * 0.70711 - r * gpGlobals->v_right * 0.70711;
}

// orig 0x140380
void CBMHeadcrabifyer::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);
	pev->rendermode = kRenderTransColor;
	pev->renderamt = 0;
	pev->classname = MAKE_STRING("gonarch_headcrabifier");
	pev->speed = 800;
	pev->velocity = pev->speed * pev->movedir;
	SetThink(&CBMHeadcrabifyer::BMHCFThink);
	SetTouch(&CBMHeadcrabifyer::BMHCFTouch);
	pev->nextthink = gpGlobals->time + 0.1;
	m_flRadius = 64;
	SetupPoints();
	m_bTraceToggle = false;

	for (int i = 0; i < 8; i++)
	{
		m_pRingEnts[i] = GetClassPtr((CBaseEntity*)NULL);
		m_pRingEnts[i]->Spawn();
		m_pRingEnts[i]->pev->classname = MAKE_STRING("bm_hcfy_ring_ent");
		SET_MODEL(m_pRingEnts[i]->pev->pContainingEntity, "models/rpgrocket.mdl");
		m_pRingEnts[i]->pev->movetype = MOVETYPE_NOCLIP;
		m_pRingEnts[i]->pev->solid = SOLID_NOT;
		m_pRingEnts[i]->pev->velocity = pev->velocity;
		m_pRingEnts[i]->pev->owner = ENT(pev);
		m_pRingEnts[i]->pev->renderamt = 0;
		m_pRingEnts[i]->pev->rendermode = kRenderTransColor;
		m_pRingEnts[i]->pev->effects |= EF_LIGHT;
		m_pRingEnts[i]->pev->iuser3 = 2;
		UTIL_SetOrigin(m_pRingEnts[i]->pev, m_vecPoints[i]);
	}

	for (int i = 0; i < 7; i++)
	{
		m_pBeams[i] = CBeam::BeamCreate("sprites/lgtning.spr", 20);
		const int iEnd = ENTINDEX(m_pRingEnts[i + 1]->pev->pContainingEntity);
		m_pBeams[i]->EntsInit(ENTINDEX(m_pRingEnts[i]->pev->pContainingEntity), iEnd);
		m_pBeams[i]->SetWidth(20);
		m_pBeams[i]->SetColor(64, 128, 255);
		m_pBeams[i]->SetBrightness(240);
		m_pBeams[i]->SetNoise(10);
		m_pBeams[i]->SetScrollRate(30);
	}
	m_pBeams[7] = CBeam::BeamCreate("sprites/lgtning.spr", 20);
	const int iEnd = ENTINDEX(m_pRingEnts[0]->pev->pContainingEntity);
	m_pBeams[7]->EntsInit(ENTINDEX(m_pRingEnts[7]->pev->pContainingEntity), iEnd);
	m_pBeams[7]->SetWidth(20);
	m_pBeams[7]->SetColor(64, 128, 255);
	m_pBeams[7]->SetBrightness(240);
	m_pBeams[7]->SetNoise(10);
	m_pBeams[7]->SetScrollRate(30);

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/xbow_fly1.wav", 0.7, 0.8, 0, 110);
}

// orig 0x140830
void CBMHeadcrabifyer::Precache()
{
	PRECACHE_MODEL("sprites/lgtning.spr");
	PRECACHE_SOUND("weapons/xbow_fly1.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("debris/beamstart9.wav");
}

// orig 0x140890
CBMHeadcrabifyer* CBMHeadcrabifyer::CreateHCF(entvars_t* pevOwner, entvars_t* pevEnemy, Vector vecOrigin, Vector vecDir)
{
	CBMHeadcrabifyer* pHCF = GetClassPtr((CBMHeadcrabifyer*)NULL);
	pHCF->pev->movedir = vecDir;
	pHCF->pev->owner = pevOwner->pContainingEntity;
	pHCF->pev->enemy = pevEnemy->pContainingEntity;
	pHCF->pev->dmg = gpGlobals->time + 32; // when it gives up
	UTIL_SetOrigin(pHCF->pev, vecOrigin);
	EMIT_SOUND_DYN(pHCF->pev->pContainingEntity, CHAN_WEAPON, "debris/beamstart9.wav", 1.0, 0.8, 0, 130);
	pHCF->Spawn();
	return pHCF;
}

// orig 0x140960
void CBMHeadcrabifyer::KillEnts()
{
	for (int i = 0; i < 8; i++)
	{
		if (m_pBeams[i])
		{
			m_pBeams[i]->SetThink(&CBaseEntity::SUB_Remove);
			m_pBeams[i]->pev->nextthink = gpGlobals->time + 0.01;
		}
		m_pBeams[i] = NULL;
		if (m_pRingEnts[i])
			m_pRingEnts[i]->pev->flags |= FL_KILLME;
		m_pRingEnts[i] = NULL;
	}
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "weapons/xbow_fly1.wav", 0, 0, SND_STOP, 100);
}

// orig 0x140a30: hit the world: scorch where the ring was and go away
void CBMHeadcrabifyer::BMHCFTouch(CBaseEntity* pOther)
{
	if (!pOther->IsBSPModel())
		return;

	for (int i = 0; i < 8; i++)
	{
		Vector vecEnd = m_vecPoints[i] + m_flRadius * pev->movedir * 4;
		Vector vecStart = m_vecPoints[i] - (m_flRadius * pev->movedir + m_flRadius * pev->movedir);
		TraceResult tr;
		UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, ENT(pev), &tr);
		UTIL_DecalTrace(&tr, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));
	}
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/electro4.wav", 1.0, 0.9, 0, 140);
	KillEnts();
	SetTouch(NULL);
	SetThink(NULL);
	pev->flags |= FL_KILLME;
}

// The test BMHCFThink makes on each trace hit, written inline there in the original: a client
// or monster that can be headcrabified.
static bool BMHCFCanHeadcrabify(edict_t* pHit)
{
	if (!pHit || (VARS(pHit)->flags & (FL_CLIENT | FL_MONSTER)) == 0)
		return false;
	CBaseEntity* pEntity = CBaseEntity::Instance(pHit);
	if (!pEntity)
		return false;
	int iCap = 0x1000000;
	return pEntity->HasRCCap(iCap) != 0;
}

// orig 0x140c20: fly on, drag the ring along, and when a target is near, sweep traces across
// the ring and headcrabify what they hit
void CBMHeadcrabifyer::BMHCFThink()
{
	pev->nextthink = 0.05 + gpGlobals->time;
	if (pev->dmg < gpGlobals->time)
	{
		KillEnts();
		SetTouch(NULL);
		SetThink(NULL);
		pev->flags |= FL_KILLME;
		return;
	}

	pev->velocity = pev->speed * pev->movedir;
	SetupPoints();
	for (int i = 0; i < 8; i++)
	{
		if (m_pRingEnts[i])
		{
			UTIL_SetOrigin(m_pRingEnts[i]->pev, m_vecPoints[i]);
			m_pRingEnts[i]->pev->velocity = pev->velocity;
		}
	}

	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, m_flRadius * 4)) != NULL)
	{
		if (!pEntity->pev || !UTIL_IsValidEntity(pEntity->pev->pContainingEntity) || pEntity->pev->pContainingEntity == pev->owner || !pEntity->IsAlive())
			continue;
		int iCap = 0x1000000;
		if (pEntity->HasRCCap(iCap))
			break;
	}
	if (!pEntity)
		return;

	// the last two traces lag a twentieth of a second behind the ring
	TraceResult tr;
	m_bTraceToggle = !m_bTraceToggle;
	if (!m_bTraceToggle)
	{
		UTIL_TraceHull(m_vecPoints[4], m_vecPoints[0], dont_ignore_monsters, head_hull, ENT(pev), &tr);
		if (!BMHCFCanHeadcrabify(tr.pHit))
			UTIL_TraceHull(m_vecPoints[0], m_vecPoints[6], dont_ignore_monsters, head_hull, ENT(pev), &tr);
		if (!BMHCFCanHeadcrabify(tr.pHit))
		{
			Vector vecEnd = m_vecPoints[2] - pev->speed * pev->movedir * 0.05;
			Vector vecStart = m_vecPoints[6] - pev->speed * pev->movedir * 0.05;
			UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);
		}
		if (!BMHCFCanHeadcrabify(tr.pHit))
		{
			Vector vecEnd = m_vecPoints[0] - pev->speed * pev->movedir * 0.05;
			Vector vecStart = m_vecPoints[4] - pev->speed * pev->movedir * 0.05;
			UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);
		}
	}
	else
	{
		UTIL_TraceHull(m_vecPoints[0], m_vecPoints[4], dont_ignore_monsters, head_hull, ENT(pev), &tr);
		if (!BMHCFCanHeadcrabify(tr.pHit))
			UTIL_TraceHull(m_vecPoints[2], m_vecPoints[6], dont_ignore_monsters, head_hull, ENT(pev), &tr);
		if (!BMHCFCanHeadcrabify(tr.pHit))
		{
			Vector vecEnd = m_vecPoints[6] - pev->speed * pev->movedir * 0.05;
			Vector vecStart = m_vecPoints[2] - pev->speed * pev->movedir * 0.05;
			UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);
		}
		if (!BMHCFCanHeadcrabify(tr.pHit))
		{
			Vector vecEnd = m_vecPoints[4] - pev->speed * pev->movedir * 0.05;
			Vector vecStart = m_vecPoints[0] - pev->speed * pev->movedir * 0.05;
			UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);
		}
	}

	// VERIFY: whatever the last trace hit goes in, qualified or not (DHCHeadcrabifyEnt may check)
	if (tr.pHit)
		DHCHeadcrabifyEnt(tr.pHit, pev->enemy);
}

// orig 0x141650: fire a headcrabifier ring from vecOrigin along vecDir (flattened)
void BMHCFCreate(edict_t* pOwner, edict_t* pEnemy, Vector vecOrigin, Vector vecDir)
{
	entvars_t* pevOwner = VARS(pOwner);
	// VERIFY: with no enemy the original passes the owner's entvars, not NULL
	entvars_t* pevEnemy = pEnemy ? VARS(pEnemy) : pevOwner;
	vecDir.z = 0;
	vecDir = vecDir.Normalize();
	CBMHeadcrabifyer::CreateHCF(pevOwner, pevEnemy, vecOrigin, vecDir);
}


