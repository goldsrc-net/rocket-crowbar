/***
*
*	Rocket Crowbar 1.9: drivechum.cpp. The chumtoad a player drives (drive_chumtoad): it hops
*	around, zaps forking lightning on attack and discharges itself under water. Also the
*	"chumtoadify" hit of the drivable bullsquid's spit (DCTChumtoadifyEnt).
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "client.h"
#include "UserMessages.h"
#include "driveent.h"

// Preff.cpp (rc.ini server prefs)
extern float PREFS_KeyLoopUpFloat(const char* pszHeading, const char* pszKey, float flDefault);
extern int PREFS_KeyLoopUpInt(const char* pszHeading, const char* pszKey, int iDefault);
extern bool PREFS_KeyLoopUpBool(const char* pszHeading, const char* pszKey, bool bDefault);

// drivehc.cpp
extern void DHHeadcrabifyEffects(Vector vecOrigin);
extern void DHHeadcrabifyFade(Vector vecOrigin, CBaseEntity* pPlayer);

// A sound with its pitch range (RandomLong(iPitchMin, iPitchMax)).
struct sCTSoundT
{
	const char* pszSound;
	int iPitchMin;
	int iPitchMax;
};

sCTSoundT g_sCTDieSounds[] = {
	{"headcrab/hc_die2.wav", 130, 140},
	{"aslave/slv_die1.wav", 130, 140},
	{"aslave/slv_die2.wav", 130, 140},
};

sCTSoundT g_sCTPainSounds[] = {
	{"aslave/slv_pain1.wav", 130, 140},
};

class CDriveChumtoad : public CBaseDriveEnt
{
public:
	void Spawn() override;
	void Precache() override;
	int Classify() override;
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void Killed(entvars_t* pevAttacker, int iGib) override;
	int BloodColor() override;
	int GetGuassHull() override;
	int GetGuassDistOut() override;
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

	void EXPORT DBSThink();
	void DoZap();
	void DoJump();
	void CheckDoor();
	void CheckEHop();
	void RunWaterPhysics();
	void DoMovement();
	void DoYaw();
	void RunJumpPhysics();
	void UpdateOwnerHUD();
	void ZapBeam(Vector vecSrc, bool bUseDirAngles, Vector vecDirAngles, Vector vecAimAngles, float flDamage, float flLength, float flWidth, float flBrightness, int iForks);
	void FireZap();
	void EXPORT DyingThink();
	void SetDeathAnim();

	// Unsaved (the original has no save table for this class). m_pOwner and m_fFrameTime are
	// the names the other drive monsters' save tables give these two slots (CDriveAGrunt); the
	// rest are named from what they do.
	// m_pOwner (orig +0x27c, the driving player) is CBaseDriveEnt's
	float m_fFrameTime;		// orig +0x280, this think's StudioFrameAdvance interval
	bool m_bCanIdle;		// orig +0x284, idle animations may play. VERIFY name
	bool m_bWalking;		// orig +0x285, set while walking, never read. VERIFY name
	bool m_bCanMove;		// orig +0x286, walking/swimming allowed (not zapping). VERIFY name
	bool m_bInZap;			// orig +0x287
	float m_fNextZap;		// orig +0x288
	float m_fZapFireTime;	// orig +0x28c, when the zap animation fires the beam, -1 once fired. VERIFY name
	bool m_bInJump;			// orig +0x290
	Vector m_vecSudoVel;	// orig +0x294, how fast the last walk moved (camera lead). name as CDriveAGrunt
	float m_fLastMoveTime;	// orig +0x2a0. name as CDriveAGrunt
	float m_fNextEHopCheck; // orig +0x2a4. name as CDriveAGrunt
	float m_fNextDoorCheck; // orig +0x2a8. name as CDriveAGrunt
	float m_fYawSpeedScale; // orig +0x2ac, scales the turn rate. VERIFY name
	float m_fNextBlink;		// orig +0x2b0. VERIFY name
	float m_fNextIdle2;		// orig +0x2b4, when the idle2 animation plays next. VERIFY name
	float m_fNextPainSound; // orig +0x2b8
	int m_iClientHealth;	// orig +0x2bc, the health last sent to the driver's HUD
	float m_fNextRegen;		// orig +0x2c0. VERIFY name
	int m_iLightningSpr;	// orig +0x2c4, sprites/lgtning.spr
};

LINK_ENTITY_TO_CLASS(drive_chumtoad, CDriveChumtoad); // orig 0x146ce0

// orig 0x146d10
void CDriveChumtoad::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_chumtoad");
	SET_MODEL(ENT(pev), "models/chumtoad.mdl");
	UTIL_SetSize(pev, Vector(-10, -10, 0), Vector(10, 10, 16));
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->takedamage = DAMAGE_AIM;
	m_bloodColor = BLOOD_COLOR_YELLOW;
	pev->effects = 0;
	pev->friction = 1.0;
	m_HackedGunPos = Vector(0, 24, 48);
	pev->health = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadHealth", 40.0);
	pev->max_health = pev->health;
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	pev->sequence = LookupSequence("idle");
	ResetSequenceInfo();
	InitBoneControllers();
	pev->frame = 0;
	pev->framerate = 1.0;
	SetEyePosition();

	m_fNextBlink = RANDOM_LONG(6, 8) + gpGlobals->time;
	m_fNextIdle2 = RANDOM_LONG(9, 10) + gpGlobals->time;
	m_fNextPainSound = gpGlobals->time;
	m_bCanIdle = true;
	m_bWalking = false;
	m_bCanMove = true;
	m_bInJump = false;
	m_bInZap = false;
	m_fNextZap = gpGlobals->time;
	m_fZapFireTime = -1;
	m_vecSudoVel = g_vecZero;
	m_fNextRegen = gpGlobals->time;
	m_fNextEHopCheck = gpGlobals->time;
	m_fNextDoorCheck = gpGlobals->time;

	SetThink(&CDriveChumtoad::DBSThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x147000
void CDriveChumtoad::Precache()
{
	PRECACHE_MODEL("models/chumtoad.mdl");
	m_iLightningSpr = PRECACHE_MODEL("sprites/lgtning.spr");
	PRECACHE_SOUND("debris/beamstart10.wav");
	PRECACHE_SOUND("debris/zap8.wav");
	for (const sCTSoundT& s : g_sCTDieSounds)
		PRECACHE_SOUND(s.pszSound);
	PRECACHE_SOUND(g_sCTPainSounds[0].pszSound);
}

// orig 0x1470b0
const char* CDriveChumtoad::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;
	return STRING(m_pOwner->pev->netname);
}

// orig 0x147120
void CDriveChumtoad::DBSThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;
	if (m_fFrameTime < 0.001)
		m_fFrameTime = 0.001;

	if (m_pOwner)
	{
		bool bGone = FNullEnt(m_pOwner->pev);
		if (!bGone && m_pOwner->pev && m_pOwner->pev->pContainingEntity && m_pOwner->pev->pContainingEntity->pvPrivateData && !m_pOwner->pev->pContainingEntity->free && UTIL_PlayerByIndex(ENTINDEX(m_pOwner->pev->pContainingEntity)))
		{
			if (pev->waterlevel > 1)
				pev->flags &= ~FL_ONGROUND;
			DispatchAnimEvents(m_fFrameTime);

			m_pOwner->pev->rendermode = kRenderTransColor;
			m_pOwner->pev->renderamt = 0;
			m_pOwner->pev->effects |= EF_NODRAW;
			UpdateOwnerHUD();

			if ((pev->flags & FL_FROZEN) != 0)
			{
				m_vecSudoVel = g_vecZero;
				m_fLastMoveTime = gpGlobals->time;
				return;
			}

			m_bCanIdle = true;
			m_bWalking = false;
			m_bCanMove = true;
			m_fYawSpeedScale = 1.0;
			DoZap();
			DoMovement();
			DoYaw();

			// it heals while in water
			if (pev->waterlevel < 2)
			{
				m_fNextRegen = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadRegenUnitDelay", 1.5) + gpGlobals->time;
			}
			else if (m_fNextRegen < gpGlobals->time)
			{
				m_fNextRegen = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadRegenUnitDelay", 1.5) + gpGlobals->time;
				pev->health = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadRegenPerUnit", 5.0) + pev->health;
				if (pev->health > pev->max_health)
					pev->health = pev->max_health;
			}

			pev->fuser4 = m_vecSudoVel.Length();

			// blink: skin 2 is eyes closed, counting down to 0
			if (pev->skin < 1)
				pev->skin = 0;
			else
				pev->skin--;

			if (m_bCanIdle)
			{
				if (m_fNextIdle2 < gpGlobals->time)
				{
					pev->sequence = LookupSequence("idle2");
					ResetSequenceInfo();
					pev->frame = 0;
					m_fNextIdle2 = RANDOM_LONG(9, 10) + gpGlobals->time;
				}
				else if (pev->sequence != LookupSequence("idle") && (m_fSequenceFinished || m_fSequenceLoops))
				{
					pev->sequence = LookupSequence("idle");
					ResetSequenceInfo();
					pev->frame = 0;
				}

				if (m_fNextBlink < gpGlobals->time)
				{
					pev->skin = 2;
					m_fNextBlink = RANDOM_LONG(6, 8) + gpGlobals->time;
				}
				return;
			}

			m_fNextBlink = RANDOM_LONG(6, 8) + gpGlobals->time;
			m_fNextIdle2 = RANDOM_LONG(9, 10) + gpGlobals->time;
			return;
		}
	}

	// the driver is gone
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("chumtoad");
	g_pevLastInflictor = pev;
	Killed(pev, GIB_NORMAL);
}

// orig 0x1475c0
void CDriveChumtoad::DoZap()
{
	if (m_bInJump)
		return;

	if (!m_bInZap)
	{
		if (m_fNextZap <= gpGlobals->time && (m_pOwner->pev->button & IN_ATTACK) != 0)
		{
			m_bInZap = true;
			m_bCanIdle = false;
			m_bCanMove = false;
			m_fYawSpeedScale = 0.6;
			pev->sequence = LookupSequence("idle3");
			ResetSequenceInfo();
			pev->frame = 0;
			m_fZapFireTime = gpGlobals->time + 0.6;
		}
		return;
	}

	if (m_fZapFireTime > 0 && m_fZapFireTime < gpGlobals->time)
	{
		m_fZapFireTime = -1;
		FireZap();
	}

	if (!m_fSequenceFinished)
	{
		m_bCanIdle = false;
		m_bCanMove = false;
		m_fYawSpeedScale = 0.6;
	}
	else
	{
		m_bInZap = false;
		m_fNextZap = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadZapRefire", 0.2) + gpGlobals->time;
	}
}

// orig 0x147720. Nothing calls it in the original.
void CDriveChumtoad::DoJump()
{
	if (m_bInZap)
		return;

	if (!m_bInJump)
	{
		if ((m_pOwner->pev->button & IN_JUMP) != 0 && (pev->flags & FL_ONGROUND) != 0)
		{
			m_bInJump = true;
			m_bCanIdle = false;
			m_bCanMove = false;
			m_fYawSpeedScale = 0.6;
			pev->sequence = LookupSequence("flinch1");
			ResetSequenceInfo();
			pev->frame = 0;
			pev->flags &= ~FL_ONGROUND;
			pev->velocity.z = CVAR_GET_FLOAT("sv_gravity") * 0.5 * 0.7 + pev->velocity.z;
		}
	}
	else if ((pev->flags & FL_ONGROUND) == 0 && pev->waterlevel == 0)
	{
		m_bCanIdle = false;
		m_bCanMove = false;
		m_fYawSpeedScale = 0.6;
	}
	else
	{
		m_bInJump = false;
	}
}

// orig 0x147850: walking into a door opens it (touches it)
void CDriveChumtoad::CheckDoor()
{
	if (gpGlobals->time < m_fNextDoorCheck)
		return;
	if ((pev->flags & FL_ONGROUND) == 0)
		return;
	if (!m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecSrc = pev->origin + Vector(0, 0, 32);
	Vector vecEnd = vecSrc + gpGlobals->v_forward * (m_fFrameTime * pev->framerate * m_flGroundSpeed);
	TraceResult tr;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

	if (tr.pHit && (!strcmp(STRING(VARS(tr.pHit)->classname), "func_door") || !strcmp(STRING(VARS(tr.pHit)->classname), "func_door_rotating")))
	{
		CBaseEntity* pDoor = CBaseEntity::Instance(tr.pHit);
		if (pDoor)
			pDoor->Touch(this);
	}

	m_fNextDoorCheck = gpGlobals->time + 0.3;
}

// orig 0x147ab0: hop up a small ledge in front
void CDriveChumtoad::CheckEHop()
{
	if (m_fNextEHopCheck > gpGlobals->time)
		return;
	if (pev->velocity.z > 16 || (pev->flags & FL_ONGROUND) == 0 || !m_bCanMove)
		return;

	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	Vector vecFwd = pev->origin + Vector(0, 0, 4) + gpGlobals->v_forward * 48;
	Vector vecDown = vecFwd - Vector(0, 0, 28);
	TraceResult tr;
	UTIL_TraceLine(pev->origin + Vector(0, 0, 4), vecFwd, dont_ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
	{
		UTIL_TraceLine(vecFwd, vecDown, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction >= 1.0 && !tr.fAllSolid && !tr.fStartSolid)
		{
			pev->velocity.z = pev->velocity.z + 50;
			pev->velocity = pev->velocity + gpGlobals->v_forward * 100;
			pev->flags &= ~FL_ONGROUND;
			UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));
		}
	}
}

// orig 0x147db0
void CDriveChumtoad::RunWaterPhysics()
{
	float flFriction = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadWaterFriction", 6.0);
	float flControl = 6.0;
	m_fYawSpeedScale = 0.7;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->gravity = 0.001;

	float flSpeed = pev->velocity.Length();
	if (flSpeed > 0.1)
	{
		if (flSpeed >= flControl)
			flControl = flSpeed;
		float flNewSpeed = flSpeed - flControl * flFriction * m_fFrameTime;
		if (flNewSpeed < 0)
			flNewSpeed = 0;
		pev->velocity = pev->velocity * (flNewSpeed / flSpeed);
	}

	// it floats up
	if (pev->waterlevel > 1)
	{
		float flGravity = CVAR_GET_FLOAT("sv_gravity");
		if (pev->gravity > 0)
			flGravity = flGravity * pev->gravity;
		pev->velocity.z = flGravity * m_fFrameTime + pev->velocity.z;
	}

	m_bCanIdle = false;
	if (!m_bCanMove)
		return;

	if (pev->sequence != LookupSequence("swim") && !m_bInJump)
	{
		pev->sequence = LookupSequence("swim");
		ResetSequenceInfo();
		pev->framerate = 1.5;
	}

	if ((m_pOwner->pev->button & IN_FORWARD) != 0)
	{
		UTIL_MakeAimVectors(pev->angles);
		float flMaxSpeed = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadWaterSpeed", 120.0);
		float flAccel = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadWaterAccel", 150.0);
		Vector vecDir = gpGlobals->v_forward;
		float flAdd = flMaxSpeed - DotProduct(pev->velocity, vecDir);
		if (flAdd > 0)
		{
			float flAccelSpeed = flAccel * m_fFrameTime * flMaxSpeed;
			if (flAccelSpeed > flAdd)
				flAccelSpeed = flAdd;
			pev->velocity = pev->velocity + vecDir * flAccelSpeed;
		}
	}
}

// orig 0x148080
void CDriveChumtoad::DoMovement()
{
	pev->movetype = MOVETYPE_STEP;
	pev->gravity = 0;

	if ((pev->flags & FL_ONGROUND) != 0)
	{
		if (m_bCanMove)
		{
			if ((m_pOwner->pev->button & IN_FORWARD) != 0)
			{
				bool bWalk = (m_pOwner->pev->button & (IN_USE | IN_RUN)) != 0;
				float flOldFrame = pev->frame;
				int iOldSequence = pev->sequence;
				float flOldFramerate = pev->framerate;

				// sequence 6 walks, 5 runs
				if (bWalk)
				{
					if (pev->sequence != 6)
					{
						pev->sequence = 6;
						ResetSequenceInfo();
						pev->frame = 0;
						pev->framerate = 1.2;
					}
				}
				else if (iOldSequence != 5)
				{
					pev->sequence = 5;
					ResetSequenceInfo();
					pev->frame = 0;
					pev->framerate = 1.3;
				}

				CheckDoor();
				m_bCanIdle = false;
				m_bWalking = true;

				UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
				Vector vecGoal = Center() + gpGlobals->v_forward * 512;
				float flMoved = 0;
				Vector vecStart = pev->origin;
				float flDist = m_flGroundSpeed * pev->framerate * m_fFrameTime;

				// how far can it walk this frame (WALKMOVE_CHECKONLY, then put back)
				while (flDist > 0.001)
				{
					float flStep = flDist;
					if (flStep > 16)
						flStep = 16;
					Vector vecPrev = pev->origin;
					while (!WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY) && flStep > 2)
						flStep -= 2;
					UTIL_SetOrigin(pev, vecPrev);

					if (!WALK_MOVE(ENT(pev), pev->angles.y, flStep, WALKMOVE_CHECKONLY))
					{
						if (flMoved < 0.01)
						{
							// blocked: stop
							if ((unsigned int)(iOldSequence - 5) < 2)
							{
								m_fSequenceFinished = true;
								pev->framerate = flOldFramerate;
							}
							else
							{
								pev->sequence = iOldSequence;
								ResetSequenceInfo();
								pev->frame = flOldFrame;
							}
							m_bCanIdle = true;
							m_bWalking = false;
							m_vecSudoVel = g_vecZero;
							UTIL_SetOrigin(pev, vecStart);
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
				Vector vecOld = pev->origin;
				while (flMoved > 0.001)
				{
					float flStep = flMoved;
					if (flStep > 16)
						flStep = 16;
					UTIL_MoveToOrigin(ENT(pev), vecGoal, flStep, MOVE_NORMAL);
					flMoved -= flStep;
				}

				Vector vecDelta = pev->origin - vecOld;
				if (vecDelta.Length() > 0.01 && m_fFrameTime > 0.001)
					m_vecSudoVel = vecDelta / m_fFrameTime;
				else
					m_vecSudoVel = g_vecZero;
				m_fLastMoveTime = gpGlobals->time;
				m_fYawSpeedScale = 0.8;
				return;
			}

			if ((unsigned int)(pev->sequence - 5) < 2)
				m_fSequenceFinished = true;
		}
	}
	else if (pev->waterlevel != 0)
	{
		RunWaterPhysics();
	}
	else if (m_bCanIdle)
	{
		// in the air: walk or run in place by how fast it flies
		float flSpeed = pev->velocity.Length2D();
		if (flSpeed > 10 && flSpeed < 70)
		{
			if (pev->sequence != LookupSequence("walk"))
			{
				pev->sequence = LookupSequence("walk");
				ResetSequenceInfo();
			}
			m_bCanIdle = false;
		}
		else if (flSpeed >= 70)
		{
			if (pev->sequence != LookupSequence("run"))
			{
				pev->sequence = LookupSequence("run");
				ResetSequenceInfo();
			}
			m_bCanIdle = false;
		}
	}

	m_vecSudoVel = g_vecZero;
	m_fLastMoveTime = gpGlobals->time;
}

// orig 0x1486a0
void CDriveChumtoad::DoYaw()
{
	float flFrameTime = m_fFrameTime;
	float flScale = m_fYawSpeedScale;
	ChangeYaw((int)(flFrameTime * 1900 * m_fYawSpeedScale));

	if ((pev->flags & FL_ONGROUND) == 0)
	{
		float flPitch = -80;
		if (-pev->idealpitch >= -80)
		{
			flPitch = 80;
			if (!(-pev->idealpitch > 80))
				flPitch = -pev->idealpitch;
		}
		pev->angles.x = UTIL_Approach(flPitch, pev->angles.x, flFrameTime * 800 * flScale);
	}
	else
	{
		pev->angles.x = 0;
	}
}

// orig 0x148790. Nothing calls it in the original.
void CDriveChumtoad::RunJumpPhysics()
{
	float flFriction = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadJumpAirFriction", 1.0);
	float flControl = 6.0;
	m_fYawSpeedScale = 0.7;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->gravity = 0;

	float flSpeed = pev->velocity.Length2D();
	if (flSpeed > 0.1)
	{
		if (flSpeed >= flControl)
			flControl = flSpeed;
		float flNewSpeed = flSpeed - flControl * flFriction * m_fFrameTime;
		if (flNewSpeed < 0)
			flNewSpeed = 0;
		pev->velocity.x = (flNewSpeed / flSpeed) * pev->velocity.x;
		pev->velocity.y = (flNewSpeed / flSpeed) * pev->velocity.y;
	}

	m_bCanIdle = false;
	if (!m_bCanMove)
		return;

	if (pev->sequence != LookupSequence("swim") && m_fSequenceFinished)
	{
		pev->sequence = LookupSequence("swim");
		ResetSequenceInfo();
	}

	if ((m_pOwner->pev->button & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)) == 0)
		return;

	Vector vecWish = g_vecZero;
	UTIL_MakeVectors(Vector(0, pev->angles.y, 0));
	float flMaxSpeed = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadJumpAirSpeed", 80.0);
	float flAccel = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadJumpAirAccel", 40.0);
	if ((m_pOwner->pev->button & IN_FORWARD) != 0)
		vecWish = vecWish + gpGlobals->v_forward;
	if ((m_pOwner->pev->button & IN_BACK) != 0)
		vecWish = vecWish - gpGlobals->v_forward;
	if ((m_pOwner->pev->button & IN_MOVELEFT) != 0)
		vecWish = vecWish - gpGlobals->v_right;
	if ((m_pOwner->pev->button & IN_MOVERIGHT) != 0)
		vecWish = vecWish + gpGlobals->v_right;

	float flAdd = flMaxSpeed - DotProduct(pev->velocity, vecWish);
	if (flAdd > 0)
	{
		float flAccelSpeed = flAccel * m_fFrameTime * flMaxSpeed;
		if (flAccelSpeed > flAdd)
			flAccelSpeed = flAdd;
		pev->velocity = pev->velocity + vecWish * flAccelSpeed;
	}
}

// orig 0x148b40: the driver's HUD shows the chumtoad's health in percent
void CDriveChumtoad::UpdateOwnerHUD()
{
	if (!m_pOwner)
		return;
	if (pev->health == (float)m_iClientHealth)
		return;

	float flPercent = (pev->health / pev->max_health) * 100;
	int iHealth = flPercent > 0 ? (int)flPercent : 0;
	if (iHealth < 1)
		iHealth = 1;
	else if (iHealth > 100)
		iHealth = 100;

	MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
	WRITE_BYTE(iHealth);
	MESSAGE_END();
	m_iClientHealth = (int)pev->health;
}

// orig 0x148c60: one segment of the zap. A segment that runs its full length forks into two
// shorter, weaker ones; one that hits something damages it.
void CDriveChumtoad::ZapBeam(Vector vecSrc, bool bUseDirAngles, Vector vecDirAngles, Vector vecAimAngles, float flDamage, float flLength, float flWidth, float flBrightness, int iForks)
{
	if (iForks < 1)
		return;

	if (vecAimAngles.x > 0)
		vecAimAngles.x = -vecAimAngles.x;

	Vector vecDirForward;
	g_engfuncs.pfnAngleVectors(vecDirAngles, vecDirForward, NULL, NULL);
	UTIL_MakeVectors(vecAimAngles);

	Vector vecDir;
	if (!bUseDirAngles)
		vecDir = gpGlobals->v_forward;
	else
		vecDir = vecDirForward;

	Vector vecEnd = vecSrc + vecDir * (RANDOM_FLOAT(0.9, 1.1) * flLength);
	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSrc);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(vecSrc.x);
	WRITE_COORD(vecSrc.y);
	WRITE_COORD(vecSrc.z);
	WRITE_COORD(tr.vecEndPos.x);
	WRITE_COORD(tr.vecEndPos.y);
	WRITE_COORD(tr.vecEndPos.z);
	WRITE_SHORT(m_iLightningSpr);
	WRITE_BYTE(RANDOM_LONG(0, 2)); // starting frame
	WRITE_BYTE(200);			   // frame rate
	WRITE_BYTE(2);				   // life
	int iWidth = (int)flWidth;
	if (iWidth < 1)
		iWidth = 1;
	WRITE_BYTE(iWidth);
	WRITE_BYTE(32); // noise
	WRITE_BYTE(230);
	WRITE_BYTE(230);
	WRITE_BYTE(255);
	int iBrightness = (int)flBrightness;
	if (iBrightness < 180)
		iBrightness = 180;
	WRITE_BYTE(iBrightness);
	WRITE_BYTE(50); // scroll speed
	MESSAGE_END();

	if (tr.flFraction >= 1.0)
	{
		float flForkDamage = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadZapForkDamageFract", 0.75);
		float flForkDist = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadZapForkDistFract", 0.8);

		// one fork to the right of the aim, one to the left, neither pointing back
		Vector vecFork;
		do
		{
			float flSide = RANDOM_FLOAT(0.1, 0.5);
			vecFork = (gpGlobals->v_forward + gpGlobals->v_right * flSide).Normalize();
		} while (DotProduct(vecFork, vecDirForward) < 0);
		ZapBeam(tr.vecEndPos, false, vecDirAngles, UTIL_VecToAngles(vecFork), flForkDamage * flDamage, flForkDist * flLength, flWidth * 0.6, flBrightness * 0.75, iForks - 1);

		UTIL_MakeVectors(vecAimAngles);
		do
		{
			float flSide = RANDOM_FLOAT(-0.5, -0.1);
			vecFork = (gpGlobals->v_forward + gpGlobals->v_right * flSide).Normalize();
		} while (DotProduct(vecFork, vecDirForward) < 0);
		ZapBeam(tr.vecEndPos, false, vecDirAngles, UTIL_VecToAngles(vecFork), flForkDamage * flDamage, flForkDist * flLength, flWidth * 0.6, flBrightness * 0.75, iForks - 1);
		return;
	}

	if (!tr.pHit)
		return;
	if (VARS(tr.pHit)->takedamage == DAMAGE_NO)
		return;
	CBaseEntity* pHit = CBaseEntity::Instance(tr.pHit);
	if (!pHit)
		return;

	ClearMultiDamage();
	pHit->TraceAttack(m_pOwner->pev, flDamage, vecDir, &tr, DMG_SHOCK | DMG_ENERGYBEAM);
	ApplyMultiDamage(pev, m_pOwner->pev);
}

// orig 0x1493d0
void CDriveChumtoad::FireZap()
{
	if (pev->waterlevel < 2)
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/beamstart10.wav", 1.0, 0.8, 0, 120);
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "debris/zap8.wav", 1.0, 0.8, 0, 140);
		UTIL_MakeAimVectors(pev->angles);
		Vector vecSrc = pev->origin + gpGlobals->v_forward * 8 + gpGlobals->v_up * 4;

		float flPitch = -40;
		if (pev->idealpitch >= -40)
		{
			flPitch = 0;
			if (!(pev->idealpitch > 0))
				flPitch = pev->idealpitch;
		}
		Vector vecAngles = Vector(flPitch, pev->angles.y, 0);

		// kills by the zap read "chumtoad_zap"
		pev->classname = MAKE_STRING("chumtoad_zap");
		int iForks = PREFS_KeyLoopUpInt("DRIVECHUMTOAD", "ChumtoadZapMaxForks", 4);
		float flLength = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadZapBaseBeamLength", 128.0);
		float flDamage = PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadZapBaseBeamDamage", 50.0);
		ZapBeam(vecSrc, true, vecAngles, vecAngles, flDamage, flLength, 16, 255, iForks);
		pev->classname = MAKE_STRING("drive_chumtoad");
		return;
	}

	// under water it discharges, killing itself
	CBaseEntity* pSound = GetClassPtr((CBaseEntity*)NULL);
	pSound->Spawn();
	pSound->pev->classname = MAKE_STRING("sound_emiiter");
	pSound->SetThink(&CBaseEntity::SUB_Remove);
	pSound->pev->nextthink = gpGlobals->time + 0.1;
	UTIL_SetOrigin(pSound->pev, pev->origin);
	EMIT_SOUND_DYN(pSound->pev->pContainingEntity, CHAN_WEAPON, "weapons/electro4.wav", 1.0, 0.8, 0, 130);

	UTIL_ScreenFade(m_pOwner, Vector(220, 220, 255), 0.7, 0.2, 255, 0);
	pev->classname = MAKE_STRING("chumtoad_discharge");
	pev->takedamage = DAMAGE_NO;
	RadiusDamage(pev, m_pOwner->pev, PREFS_KeyLoopUpFloat("DRIVECHUMTOAD", "ChumtoadZapDischargeRadiusDamage", 100.0), CLASS_NONE, DMG_SHOCK);
	pev->takedamage = DAMAGE_AIM;
	pev->health = -500;
	g_pevLastInflictor = pev;
	Killed(m_pOwner->pev, GIB_ALWAYS);
}

// orig 0x149780
void CDriveChumtoad::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 2001:
		if ((pev->flags & FL_ONGROUND) == 0)
			return;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, RANDOM_LONG(0, 1) == 0 ? "common/bodydrop3.wav" : "common/bodydrop4.wav", 1.0, 0.8, 0, 100);
		break;

	case 2002:
		if ((pev->flags & FL_ONGROUND) == 0)
			return;
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, RANDOM_LONG(0, 1) == 0 ? "common/bodydrop3.wav" : "common/bodydrop4.wav", 1.0, 0.8, 0, 90);
		break;
	}
}

// orig 0x149890
void CDriveChumtoad::DyingThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	if (!m_fSequenceFinished && gpGlobals->time < pev->dmgtime)
	{
		DispatchAnimEvents(StudioFrameAdvance());
		return;
	}

	pev->deadflag = DEAD_DEAD;
	if (pev->dmgtime <= gpGlobals->time || (pev->flags & FL_ONGROUND) != 0)
		FadeMonster();
}

// orig 0x149950
void CDriveChumtoad::SetDeathAnim()
{
	pev->sequence = LookupSequence("die3");
	ResetSequenceInfo();
	pev->frame = 0;
}

// orig 0x1499a0
bool CDriveChumtoad::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_pOwner)
	{
		if (CBaseEntity::Instance(pevAttacker->pContainingEntity) && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, CBaseEntity::Instance(pevAttacker->pContainingEntity)))
			return false;
		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	if ((bitsDamageType & DMG_SHOCK) != 0)
		return false;

	if (m_fNextPainSound < gpGlobals->time)
	{
		m_fNextPainSound = RANDOM_FLOAT(3, 4) + gpGlobals->time;
		int i = RANDOM_LONG(0, 0);
		int iPitch = RANDOM_LONG(g_sCTPainSounds[i].iPitchMin, g_sCTPainSounds[i].iPitchMax);
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, g_sCTPainSounds[i].pszSound, 0.7, 1.1, 0, iPitch);
	}

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// orig 0x149b50
void CDriveChumtoad::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->deadflag != DEAD_NO)
		return;

	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->renderamt = 0;
	pev->renderfx = kRenderFxNone;

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_BYTE(HIDEHUD_HEALTH);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->pev->pContainingEntity);
		WRITE_BYTE(0);
		MESSAGE_END();
	}

	if (pev->owner)
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	DEClientLeave();

	// the driver dies with it
	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		UTIL_SetOrigin(m_pOwner->pev, m_pOwner->m_pCurDriveEnt->pev->origin + Vector(0, 0, 64));
		m_pOwner->pev->effects |= EF_NODRAW;
		m_pOwner->pev->health = -1;
		m_pOwner->Killed(pevAttacker, GIB_NEVER);
	}

	if ((pev->health < -30 && iGib != GIB_NEVER) || iGib == GIB_ALWAYS)
	{
		GibMonster();
		return;
	}

	SetDeathAnim();
	int i = RANDOM_LONG(0, 2);
	int iPitch = RANDOM_LONG(g_sCTDieSounds[i].iPitchMin, g_sCTDieSounds[i].iPitchMax);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_sCTDieSounds[i].pszSound, 0.7, 1.1, 0, iPitch);
	SetThink(&CDriveChumtoad::DyingThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = gpGlobals->time + 5.0;
}

// orig 0x149e90: the driver's camera, behind the chumtoad
void CDriveChumtoad::DEClientPreThink()
{
	// holding reload frees the camera from steering
	if ((m_pOwner->pev->button & IN_RELOAD) == 0)
	{
		pev->ideal_yaw = UTIL_AngleMod(m_pOwner->pev->v_angle.y);
		pev->idealpitch = m_pOwner->pev->v_angle.x;
	}

	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;
	SetEyePosition();
	UTIL_MakeVectors(m_pOwner->pev->v_angle);

	Vector vecOrg = GetCamRotOrg();
	if (m_vecSudoVel.Length() > 0.01 && m_fLastMoveTime < gpGlobals->time)
		vecOrg = GetCamRotOrg() + m_vecSudoVel * (gpGlobals->time - m_fLastMoveTime);

	Vector vecEnd = vecOrg - gpGlobals->v_forward * 72;
	TraceResult tr;
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	UTIL_TraceLine(vecOrg, vecEnd, ignore_monsters, ENT(pev), &tr);

	Vector vecCam = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;

	// out of water, pull the camera in until it is in open air
	int i = 0;
	if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
	{
		for (i = 1; i < 48; i++)
		{
			if (UTIL_PointContents(vecCam) == CONTENTS_EMPTY)
				break;
			vecCam = tr.vecEndPos + gpGlobals->v_forward * (i * 8);
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

	// cl_de_alpha: the driver can see the chumtoad translucent
	float flAlpha = m_pOwner->m_fDrvEntAlpha;
	if (flAlpha >= 1.0 || !(flAlpha > 0))
	{
		m_pOwner->pev->euser2 = NULL;
		m_pOwner->pev->iuser3 = 0;
		m_pOwner->pev->fuser1 = 0;
		m_pOwner->pev->fuser2 = 0;
	}
	else
	{
		m_pOwner->pev->euser2 = ENT(pev);
		m_pOwner->pev->iuser3 = 4;
		m_pOwner->pev->fuser1 = 2.0;
		float flAmt = 0;
		if (m_pOwner->m_fDrvEntAlpha * 255 >= 0)
		{
			flAmt = 255;
			if (m_pOwner->m_fDrvEntAlpha * 255 <= 255)
				flAmt = m_pOwner->m_fDrvEntAlpha * 255;
		}
		m_pOwner->pev->fuser2 = flAmt;
	}
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
}

// orig 0x14a400
void CDriveChumtoad::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x14a420: the player becomes an invisible camera; the chumtoad takes its input
void CDriveChumtoad::DEClientEnter(CBasePlayer* pPlayer)
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
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 5.0;
	if (m_pOwner->m_pActiveItem)
	{
		m_pOwner->m_pActiveItem->Holster();
		m_pOwner->pev->weaponmodel = 0;
	}
	m_pOwner->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT;
	m_pOwner->m_afPhysicsFlags = 0;
	m_iClientHealth = -1;

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

// orig 0x14a7d0: give the player back its body
void CDriveChumtoad::DEClientLeave()
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
	m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 20.0;
	m_pOwner->m_fDrcEntTransTime = gpGlobals->time + 15.0;
	UTIL_SetSize(m_pOwner->pev, Vector(-16, -16, -36), Vector(16, 16, 36));
	m_pOwner->pev->euser2 = NULL;
	m_pOwner->pev->iuser3 = 0;
	m_pOwner->pev->fuser1 = 0;
	m_pOwner->pev->fuser2 = 0;
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
	if (m_pOwner->m_pActiveItem)
		m_pOwner->m_pActiveItem->Deploy();
	m_pOwner->m_iHideHUD &= ~(HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT);
	SET_VIEW(m_pOwner->pev->pContainingEntity, m_pOwner->pev->pContainingEntity);
	SetupVisibility(m_pOwner->pev->pContainingEntity, m_pOwner->pev->pContainingEntity, NULL, NULL);
}

// orig 0x14aae0: the driver asks to get out; the chumtoad gibs
void CDriveChumtoad::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVECHUMTOAD", "AllowEject", false))
	{
		if (CVAR_GET_FLOAT("sv_allowglobaleject") != 1 && g_pGameRules->IsMultiplayer() && !g_pGameRules->IsCoOp())
			return;
	}

	if (!m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev || !(m_pOwner->m_pCurDriveEnt->pev->health > 0))
		return;
	if (!(m_pOwner->m_fDrvEntEjectTime < gpGlobals->time))
		return;

	DEClientLeave();
	m_pOwner->UpdateClientData();
	m_pOwner->m_bInDriveEnt = false;
	Killed(pev, GIB_ALWAYS);
	UTIL_SetOrigin(m_pOwner->pev, pev->origin + Vector(0, 0, 48));
	m_pOwner->pev->v_angle = pev->angles;
	m_pOwner->pev->angles = m_pOwner->pev->v_angle;
	m_pOwner->pev->v_angle.x = 0;
	m_pOwner->pev->angles.x = 0;
	m_pOwner->pev->v_angle.z = 0;
	m_pOwner->pev->angles.z = 0;
	m_pOwner->pev->fixangle = 1;
	m_pOwner->m_pCurDriveEnt = NULL;
}

// orig 0x14acf0: the drivable bullsquid's chumtoadifier hit pent. A monster gibs; a player, or
// the driver of a drive monster, is put in a new drive_chumtoad. pentAttacker gets the kill.
void DCTChumtoadifyEnt(edict_t* pent, edict_t* pentAttacker)
{
	if (!UTIL_IsValidEntity(pent))
		return;
	if ((pent->v.flags & (FL_CLIENT | FL_MONSTER)) == 0)
		return;

	CBaseEntity* pEntity = CBaseEntity::Instance(pent);
	if (!pEntity || !pEntity->IsAlive() || pEntity->IsBSPModel())
		return;
	int iCap = 0x1000000; // VERIFY: name of RC cap bit 24 (can be chumtoadified or headcrabified)
	if (!pEntity->HasRCCap(iCap))
		return;

	// the inflictor the kill is credited to
	CBaseEntity* pInflictor = GetClassPtr((CBaseEntity*)NULL);
	pInflictor->Spawn();
	pInflictor->pev->classname = MAKE_STRING("bullsquid_chumtoadifier");
	pInflictor->pev->flags |= FL_KILLME;

	EMIT_SOUND_DYN(pEntity->pev->pContainingEntity, CHAN_BODY, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(pEntity->pev->pContainingEntity, CHAN_WEAPON, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(pEntity->pev->pContainingEntity, CHAN_VOICE, "common/null.wav", 0, 0, 0, 100);
	EMIT_SOUND_DYN(pEntity->pev->pContainingEntity, CHAN_ITEM, "common/null.wav", 0, 0, 0, 100);

	if (pEntity->IsPlayer() && !((CBasePlayer*)pEntity)->m_bInDriveEnt)
	{
		CBasePlayer* pPlayer = (CBasePlayer*)pEntity;
		DHHeadcrabifyEffects(pPlayer->Center());
		DHHeadcrabifyFade(pPlayer->Center(), pPlayer);
		Vector vecAngles = Vector(0, pPlayer->pev->v_angle.y, 0);
		pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_chumtoad", pPlayer->Center(), vecAngles);
		if (pPlayer->m_pCurDriveEnt)
		{
			pPlayer->m_pCurDriveEnt->pev->angles = Vector(0, pPlayer->pev->v_angle.y, 0);
			pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
			pPlayer->m_bInDriveEnt = true;
			pPlayer->m_pCurDriveEnt->pev->enemy = pentAttacker;
			EMIT_SOUND_DYN(pPlayer->m_pCurDriveEnt->pev->pContainingEntity, CHAN_ITEM, "debris/beamstart7.wav", 1.0, 0.9, 0, 120);
		}
		if (pPlayer->pev->pContainingEntity != pentAttacker)
			g_pGameRules->PlayerKilled(pPlayer, pentAttacker ? VARS(pentAttacker) : NULL, pInflictor->pev);
		return;
	}

	if (!pEntity->IsPlayer() && !pEntity->IsDriveent())
	{
		// a monster just gibs (the original also computes origin + (0,0,1) here, unused)
		DHHeadcrabifyEffects(pEntity->Center());
		DHHeadcrabifyFade(pEntity->Center(), NULL);
		pEntity->Killed(pEntity->pev, GIB_ALWAYS);
		EMIT_SOUND_DYN(pEntity->pev->pContainingEntity, CHAN_ITEM, "debris/beamstart7.wav", 1.0, 0.9, 0, 120);
		return;
	}

	// a drive monster: its driver gets a chumtoad instead (the original reads the driver at +0x27c,
	// which every drive class has first: CBaseDriveEnt::m_pOwner here)
	if (!pEntity)
		return;
	CBaseDriveEnt* pDrive = static_cast<CBaseDriveEnt*>(pEntity);
	if (!pDrive->m_pOwner)
		return;

	CBasePlayer* pPlayer = pDrive->m_pOwner;
	Vector vecCenter = pDrive->Center();
	pDrive->DEClientLeave();
	pDrive->m_pOwner = NULL;
	pDrive->Killed(pDrive->pev, GIB_NORMAL);
	pDrive->pev->flags |= FL_KILLME;
	pPlayer->m_pCurDriveEnt = NULL;
	pPlayer->m_bInDriveEnt = false;

	DHHeadcrabifyEffects(vecCenter);
	DHHeadcrabifyFade(vecCenter, pPlayer);
	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_chumtoad", pDrive->pev->origin, Vector(0, pPlayer->pev->v_angle.y, 0));
	if (pPlayer->m_pCurDriveEnt)
	{
		pPlayer->m_pCurDriveEnt->pev->angles = Vector(0, pPlayer->pev->v_angle.y, 0);
		pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
		pPlayer->m_bInDriveEnt = true;
		pPlayer->m_pCurDriveEnt->pev->enemy = pentAttacker;
		EMIT_SOUND_DYN(pPlayer->m_pCurDriveEnt->pev->pContainingEntity, CHAN_ITEM, "debris/beamstart7.wav", 1.0, 0.9, 0, 120);
	}
	if (pPlayer->pev->pContainingEntity != pentAttacker)
		g_pGameRules->PlayerKilled(pPlayer, pentAttacker ? VARS(pentAttacker) : NULL, pInflictor->pev);
}

// orig 0x14b270
void CDriveChumtoad::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->pev->pContainingEntity == pEdict)
		m_pOwner = NULL;
	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

int CDriveChumtoad::GetRCCaps() { return 0x1aa2dfd; }	  // orig 0x14b2f0
int CDriveChumtoad::GetGuassDistOut() { return 56; }	  // orig 0x14b300
int CDriveChumtoad::GetGuassHull() { return human_hull; } // orig 0x14b310

// orig 0x14b320
Vector CDriveChumtoad::GetCamRotOrg()
{
	return pev->origin + Vector(0, 0, 16);
}

int CDriveChumtoad::BloodColor() { return BLOOD_COLOR_YELLOW; } // orig 0x14b380
int CDriveChumtoad::Classify() { return CLASS_ALIEN_PREY; }		// orig 0x14b390

