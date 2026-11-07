/***
*
*	Rocket Crowbar 1.9: drivecontroll.cpp. The drivable alien controller (drive_controller), its
*	pickup and spawner, and its projectiles: the thrown ball, the charge-up balls that orbit it,
*	the charged ball it fires and the health ball that ball sheds back to its driver.
*
*	Reconstructed from rc_i386.so; "orig 0x..." gives each function's file address
*	(reference/funcfile.tsv), "orig +0x..." the original's member offsets.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "effects.h"
#include "gamerules.h"
#include "decals.h"
#include "client.h"
#include "UserMessages.h"
#include "driveent.h"

// Preff.cpp (rc.ini prefs). VERIFY: the original takes char*; const char* here, match Preff.cpp at merge.
extern float PREFS_KeyLoopUpFloat(const char* pszHeading, const char* pszKey, float flDefault);
extern int PREFS_KeyLoopUpInt(const char* pszHeading, const char* pszKey, int iDefault);
extern bool PREFS_KeyLoopUpBool(const char* pszHeading, const char* pszKey, bool bDefault);

void CreateDrvControllerSpawner();
bool DCPunchTrace(Vector vecSrc, Vector vecEnd, float flMaxPunch, int iMaxPunches, entvars_t* pevIgnore);

const char* g_pDCIdleSounds[] =
	{
		"controller/con_idle1.wav",
		"controller/con_idle2.wav",
		"controller/con_idle3.wav",
		"controller/con_idle4.wav",
		"controller/con_idle5.wav",
};

const char* g_pDCAttackSounds[] =
	{
		"controller/con_attack1.wav",
		"controller/con_attack2.wav",
		"controller/con_attack3.wav",
};

const char* g_pDCPainSounds[] =
	{
		"controller/con_pain1.wav",
		"controller/con_pain2.wav",
		"controller/con_pain3.wav",
};

const char* g_pDCDieSounds[] =
	{
		"controller/con_die1.wav",
		"controller/con_die2.wav",
};

// util.cpp
unsigned short FixedUnsigned16(float value, float scale);

// TE_BEAMENTS with the laser sprite, as every beam in this file sends it (not an original function)
static void DCBeamEnts(const Vector& vecMsgOrg, int iStart, int iEnd, int life, int width, int noise, int r, int g, int b, int brightness, int speed)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecMsgOrg);
	WRITE_BYTE(TE_BEAMENTS);
	WRITE_SHORT(iStart);
	WRITE_SHORT(iEnd);
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(0); // start frame
	WRITE_BYTE(0); // framerate
	WRITE_BYTE(life);
	WRITE_BYTE(width);
	WRITE_BYTE(noise);
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(brightness);
	WRITE_BYTE(speed);
	MESSAGE_END();
}

// TE_ELIGHT at vecOrg following an attachment (not an original function)
static void DCELight(const Vector& vecOrg, int iEntAttach, float flRadius, int life, float flDecay)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecOrg);
	WRITE_BYTE(TE_ELIGHT);
	WRITE_SHORT(iEntAttach);
	WRITE_COORD(vecOrg.x);
	WRITE_COORD(vecOrg.y);
	WRITE_COORD(vecOrg.z);
	WRITE_COORD(flRadius);
	WRITE_BYTE(255);
	WRITE_BYTE(192);
	WRITE_BYTE(64);
	WRITE_BYTE(life);
	WRITE_COORD(flDecay);
	MESSAGE_END();
}

//=========================================================
// CDriveController
//=========================================================
class CDriveController : public CBaseDriveEnt
{
public:
	void Spawn() override;
	void Precache() override;
	const char* GetControllerName() override;

	void EXPORT DCThink();
	void DoThrowBall();
	void DoChargeAttack();
	void DoKamikaze();
	int LookupMoveSeq(Vector vecDir);
	void DoMovement();
	void DoAngles();
	void UpdateOwnerHUD();
	void ThrowBall();
	void EmmitChargeBalls();
	void ChargeBigBall();
	void FireChargedBall();
	void KamikazeFrame();
	void KamikazeSeqFinished();
	void KamikazeDetonate();

	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void EXPORT DyingThink();
	void Killed(entvars_t* pevAttacker, int iGib) override;
	void HandleAnimEvent(MonsterEvent_t* pEvent) override;

	void DEClientPreThink() override;
	void DEClientPostThink() override;
	void DEClientEnter(CBasePlayer* pPlayer) override;
	void DEClientLeave() override;
	void DEClientEject() override;

	void EntIsBeingRemoved(edict_t* pEdict) override;
	int GetRCCaps() override { return 0x6fe3dff; }			 // orig 0x156390
	int GetGuassDistOut() override { return 56; }			 // orig 0x1563a0
	int GetGuassHull() override { return human_hull; }		 // orig 0x1563b0
	Vector GetCamRotOrg() override { return Center(); }		 // orig 0x1563c0
	int BloodColor() override { return BLOOD_COLOR_YELLOW; } // orig 0x156400
	int Classify() override { return CLASS_ALIEN_MILITARY; } // orig 0x156410

	// Nothing here is saved (the original has no save table for this class); names are ours.
	// m_pOwner (orig +0x27c, the driver) is CBaseDriveEnt's
	float m_fFrameTime;				// orig +0x280, this think's StudioFrameAdvance interval
	float m_fNextPainSound;			// orig +0x284
	bool m_bSetIdleAnim;			// orig +0x288, nothing else claimed the sequence this frame. VERIFY name
	bool m_bCanMove;				// orig +0x289, movement input is honoured this frame. VERIFY name
	bool m_bMoving;					// orig +0x28a, moved this frame (slower turning). VERIFY name
	bool m_bInThrow;				// orig +0x28b
	float m_fNextThrow;				// orig +0x28c
	float m_fThrowReleaseTime;		// orig +0x290
	bool m_bInCharge;				// orig +0x294
	bool m_bChargeDone;				// orig +0x295
	float m_fNextCharge;			// orig +0x298
	float m_fChargeEventTime;		// orig +0x29c
	int m_iChargeStage;				// orig +0x2a0
	bool m_bInKamikaze;				// orig +0x2a4
	float m_fKamikazeEventTime;		// orig +0x2a8
	int m_iKamikazeStage;			// orig +0x2ac
	float m_fNextIdleSound;			// orig +0x2b0
	CSprite* m_pHandGlow[2];		// orig +0x2b8, on attachments 3 and 4
	float m_fHandGlowTarget[2];		// orig +0x2c0, renderamt the glows approach
	int m_iHandGlowFrames[2];		// orig +0x2c8
	CBaseEntity* m_pChargeBalls[4]; // orig +0x2d0, controller_charge_ball
	CBaseEntity* m_pChargedBall;	// orig +0x2e0, controller_charged_ball
	float m_fChargeSoundVol;		// orig +0x2e4
	int m_iChargeSoundPitch;		// orig +0x2e8
	float m_fNextKamikazeZap;		// orig +0x2ec
	float m_fNextKamikazeBeam;		// orig +0x2f0
	float m_fKamikazeGlowTarget;	// orig +0x2f4
	float m_fNextKamikaze;			// orig +0x2f8
	int m_iLastHUDHealth;			// orig +0x2fc
	Vector m_vecEnterOrigin;		// orig +0x300, written on enter, never read. VERIFY name
	float m_fEnterTime;				// orig +0x30c, written on enter, never read. VERIFY name
};

LINK_ENTITY_TO_CLASS(drive_controller, CDriveController); // orig 0x14b3f0

// orig 0x14b420
void CDriveController::Spawn()
{
	Precache();
	pev->classname = MAKE_STRING("drive_controller");
	SET_MODEL(ENT(pev), "models/controller.mdl");
	UTIL_SetSize(pev, Vector(-20, -20, 0), Vector(20, 20, 72));
	UTIL_SetOrigin(pev, pev->origin);
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_AIM;
	pev->health = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerHealth", 170);
	pev->movetype = MOVETYPE_BOUNCE;
	pev->gravity = 0.0001;
	pev->friction = 1.1;
	pev->flags = FL_MONSTER;
	pev->effects = 0;
	pev->ideal_yaw = pev->angles.y;
	pev->max_health = pev->health;
	pev->deadflag = DEAD_NO;
	InitBoneControllers();
	pev->sequence = LookupSequence("float");
	ResetSequenceInfo();
	pev->frame = 0;
	m_fNextPainSound = gpGlobals->time;
	pev->framerate = 1.0;
	m_bInThrow = false;
	m_fNextThrow = gpGlobals->time;
	m_fThrowReleaseTime = -1;
	m_bInCharge = false;
	m_fNextCharge = gpGlobals->time;
	m_fChargeEventTime = -1;
	m_bChargeDone = false;
	m_iChargeStage = 0;
	m_bInKamikaze = false;
	m_fKamikazeEventTime = -1;
	m_iKamikazeStage = 0;
	m_bSetIdleAnim = true;
	m_fNextIdleSound = gpGlobals->time + RANDOM_FLOAT(0.8, 2.0);

	for (int i = 0; i < 2; i++)
	{
		m_pHandGlow[i] = CSprite::SpriteCreate("sprites/xspark4.spr", pev->origin, false);
		m_pHandGlow[i]->SetTransparency(kRenderTransAdd, 255, 255, 255, 0, kRenderFxNone);
		m_pHandGlow[i]->SetAttachment(edict(), 3 + i);
		m_pHandGlow[i]->pev->iuser3 = 2; // VERIFY: literal store to the sprite's pev +0x24c (iuser3)
		m_pHandGlow[i]->TurnOff();
		m_pHandGlow[i]->pev->owner = edict();
		m_fHandGlowTarget[i] = 0;
		m_iHandGlowFrames[i] = MODEL_FRAMES(m_pHandGlow[i]->pev->modelindex);
	}

	for (int i = 0; i < 4; i++)
		m_pChargeBalls[i] = NULL;
	m_pChargedBall = NULL;
	m_fNextKamikaze = gpGlobals->time + 2.0;

	SetThink(&CDriveController::DCThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x14b900
void CDriveController::Precache()
{
	PRECACHE_MODEL("models/controller.mdl");
	PRECACHE_MODEL("sprites/xspark4.spr");

	for (unsigned int i = 0; i < 5; i++)
		PRECACHE_SOUND(g_pDCIdleSounds[i]);
	for (unsigned int i = 0; i < 3; i++)
		PRECACHE_SOUND(g_pDCAttackSounds[i]);
	for (unsigned int i = 0; i < 3; i++)
		PRECACHE_SOUND(g_pDCPainSounds[i]);
	for (unsigned int i = 0; i < 2; i++)
		PRECACHE_SOUND(g_pDCDieSounds[i]);

	PRECACHE_SOUND("x/x_ballattack1.wav");
	PRECACHE_SOUND("debris/beamstart1.wav");
	PRECACHE_SOUND("debris/beamstart9.wav");
	PRECACHE_SOUND("debris/beamstart10.wav");
	PRECACHE_SOUND("debris/beamstart15.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("weapons/mortarhit.wav");
	PRECACHE_SOUND("ambience/alienfazzle1.wav");
	PRECACHE_SOUND("ambience/alien_minddrill.wav");

	UTIL_PrecacheOther("controller_throw_ball");
	UTIL_PrecacheOther("controller_charge_ball");
	UTIL_PrecacheOther("controller_charged_ball");
	UTIL_PrecacheOther("controller_health_ball");
}

// orig 0x14baa0
const char* CDriveController::GetControllerName()
{
	if (!m_pOwner || !m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt || m_pOwner->m_pCurDriveEnt->pev != pev)
		return NULL;

	return STRING(m_pOwner->pev->netname);
}

// orig 0x14bb10
void CDriveController::DCThink()
{
	m_fFrameTime = StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.01;

	if (m_pOwner)
	{
		bool bNull = FNullEnt(m_pOwner->pev);
		if (!bNull && m_pOwner->pev && m_pOwner->edict() && m_pOwner->edict()->pvPrivateData && !m_pOwner->edict()->free)
		{
			if (UTIL_PlayerByIndex(ENTINDEX(m_pOwner->edict())))
			{
				DispatchAnimEvents(m_fFrameTime);

				m_pOwner->pev->rendermode = kRenderTransColor;
				m_pOwner->pev->renderamt = 0;
				m_pOwner->pev->effects |= EF_NODRAW;

				m_bSetIdleAnim = true;
				m_bCanMove = true;
				m_bMoving = false;

				UpdateOwnerHUD();

				if ((pev->flags & FL_FROZEN) != 0)
				{
					pev->velocity = g_vecZero;
					pev->velocity.z = pev->velocity.z + CVAR_GET_FLOAT("sv_gravity") * pev->gravity * m_fFrameTime;
					return;
				}

				DoThrowBall();
				DoChargeAttack();
				DoKamikaze();
				DoMovement();
				DoAngles();

				for (int i = 0; i < 2; i++)
				{
					m_pHandGlow[i]->pev->renderamt = UTIL_Approach(m_fHandGlowTarget[i], m_pHandGlow[i]->pev->renderamt, m_fFrameTime * 380);
					if (m_pHandGlow[i]->pev->renderamt > 0)
					{
						m_pHandGlow[i]->pev->effects &= ~EF_NODRAW;
						m_pHandGlow[i]->pev->frame = m_fFrameTime * 10 + m_pHandGlow[i]->pev->frame;
						if (m_pHandGlow[i]->pev->frame > (float)m_iHandGlowFrames[i])
							m_pHandGlow[i]->pev->frame = 0;
						UTIL_SetOrigin(m_pHandGlow[i]->pev, pev->origin);
					}
					else
					{
						m_pHandGlow[i]->pev->renderamt = 0;
						m_pHandGlow[i]->pev->effects |= EF_NODRAW;
					}
				}

				if (!m_bSetIdleAnim)
					return;

				if (m_fNextIdleSound < gpGlobals->time)
				{
					m_fNextIdleSound = gpGlobals->time + RANDOM_FLOAT(3.0, 4.8);
					int pitch = RANDOM_LONG(95, 105);
					EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDCIdleSounds[RANDOM_LONG(0, 4)], 1.0, 0.8, 0, pitch);
				}

				if (pev->sequence == LookupSequence("float"))
					return;
				if (!m_fSequenceFinished && !m_fSequenceLoops)
					return;

				pev->sequence = LookupSequence("float");
				ResetSequenceInfo();
				return;
			}
		}
	}

	// the driver is gone
	m_pOwner = NULL;
	pev->classname = MAKE_STRING("controller");
	g_pevLastInflictor = pev;
	Killed(pev, GIB_NORMAL);
}

// orig 0x14bf50
void CDriveController::DoThrowBall()
{
	if (m_bInCharge || m_bInKamikaze)
		return;

	if (!m_bInThrow)
	{
		if (m_fNextThrow <= gpGlobals->time && (m_pOwner->pev->button & IN_ATTACK) != 0)
		{
			m_bInThrow = true;
			m_bSetIdleAnim = false;
			m_bCanMove = false;
			pev->sequence = LookupSequence("throw");
			ResetSequenceInfo();
			pev->frame = 0;
			m_fHandGlowTarget[0] = 200;
			m_fThrowReleaseTime = gpGlobals->time + 1.2;

			int pitch = RANDOM_LONG(95, 105);
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDCAttackSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, pitch);

			Vector vecHand, vecAngles;
			GetAttachment(2, vecHand, vecAngles);
			DCELight(vecHand, ENTINDEX(edict()) + 0x3000, 28, 15, -32);
		}
	}
	else
	{
		if (m_fThrowReleaseTime > 0 && m_fThrowReleaseTime < gpGlobals->time)
		{
			ThrowBall();
			m_pHandGlow[0]->pev->renderamt = 0;
			m_fHandGlowTarget[0] = 0;
			m_fThrowReleaseTime = -1;
		}

		if (!m_fSequenceFinished)
		{
			m_bSetIdleAnim = false;
		}
		else
		{
			m_bInThrow = false;
			m_fNextThrow = gpGlobals->time + PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerThrowBallRefire", 0.33);
			m_fHandGlowTarget[0] = 0;
		}
	}
}

// orig 0x14c260
void CDriveController::DoChargeAttack()
{
	if (m_bInThrow || m_bInKamikaze)
		return;

	if (!m_bInCharge)
	{
		if (gpGlobals->time < m_fNextCharge)
			return;
		if ((m_pOwner->pev->button & IN_ATTACK2) == 0)
			return;

		m_bInCharge = true;
		m_bSetIdleAnim = false;
		m_bCanMove = false;
		m_bChargeDone = false;
		m_iChargeStage = 1;
		pev->sequence = LookupSequence("shoot");
		ResetSequenceInfo();
		pev->frame = 0;
		m_fChargeEventTime = gpGlobals->time + 1.5;

		int pitch = RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDCAttackSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, pitch);

		m_fHandGlowTarget[0] = 200;
		m_fHandGlowTarget[1] = 200;
		return;
	}

	// the charged ball rides attachment 1 while it grows
	if (m_pChargedBall)
	{
		Vector vecOrg, vecAngles;
		GetAttachment(1, vecOrg, vecAngles);
		UTIL_SetOrigin(m_pChargedBall->pev, vecOrg);
	}

	if (m_fChargeEventTime > 0 && m_fChargeEventTime < gpGlobals->time)
	{
		m_fChargeEventTime = -1;

		if (m_iChargeStage == 2)
		{
			ChargeBigBall();
		}
		else if (m_iChargeStage == 1)
		{
			EmmitChargeBalls();
			m_fChargeSoundVol = 0.6;
			m_iChargeSoundPitch = 80;
			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alienfazzle1.wav", 0.6, 0.85, 0, 80);
		}
		else if (m_iChargeStage == 4)
		{
			FireChargedBall();
		}
		else
		{
			ALERT(at_console, "Drive controller: bad stage in anim event: %i\n", m_iChargeStage);
		}
	}

	if (m_fSequenceFinished)
	{
		switch (m_iChargeStage)
		{
		case 1:
			m_iChargeStage = 2;
			pev->sequence = LookupSequence("block");
			ResetSequenceInfo();
			pev->frame = 0;
			m_fChargeEventTime = gpGlobals->time + 1.2;
			break;
		case 2:
			m_iChargeStage = 3;
			pev->sequence = LookupSequence("attack2");
			ResetSequenceInfo();
			pev->frame = 0;
			break;
		case 3:
			if ((m_pOwner->pev->button & IN_ATTACK2) == 0)
			{
				// released: fire
				m_iChargeStage = 4;
				pev->sequence = LookupSequence("attack1");
				ResetSequenceInfo();
				pev->frame = 0;
				m_fChargeEventTime = gpGlobals->time + 1.4;
			}
			else
			{
				// still held: charge another burst
				m_iChargeStage = 2;
				pev->sequence = LookupSequence("block");
				ResetSequenceInfo();
				pev->frame = 0;
				m_fChargeEventTime = gpGlobals->time + 1.2;
			}
			break;
		case 4:
			m_bChargeDone = true;
			break;
		default:
			ALERT(at_console, "Drive controller: bad stage in seq finished: %i\n", m_iChargeStage);
			break;
		}
	}

	if (m_fSequenceFinished && m_bChargeDone)
	{
		m_bInCharge = false;
		m_fNextCharge = gpGlobals->time + PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargeRefire", 2.0);
		m_fHandGlowTarget[0] = 0;
		m_fHandGlowTarget[1] = 0;
	}
	else
	{
		m_bSetIdleAnim = false;
		m_bCanMove = false;
	}
}

// orig 0x14c6b0
void CDriveController::DoKamikaze()
{
	if (m_bInThrow || m_bInCharge)
		return;

	if (!m_bInKamikaze)
	{
		if (gpGlobals->time < m_fNextKamikaze)
			return;
		if ((m_pOwner->pev->button & IN_USE) == 0)
			return;

		m_bInKamikaze = true;
		m_bSetIdleAnim = false;
		m_bCanMove = false;
		m_iKamikazeStage = 1;
		pev->sequence = LookupSequence("shoot");
		ResetSequenceInfo();
		pev->frame = 0;
		m_fKamikazeEventTime = gpGlobals->time + 1.5;
		m_fHandGlowTarget[0] = 200;
		m_fHandGlowTarget[1] = 200;
		m_fNextKamikazeZap = gpGlobals->time;
		m_fNextKamikazeBeam = gpGlobals->time;
		m_fKamikazeGlowTarget = 10;
		pev->renderamt = 0;
		pev->renderfx = kRenderFxGlowShell;
		pev->rendercolor = Vector(0, 0, 0);
		return;
	}

	if (m_fKamikazeEventTime != -1 && m_fKamikazeEventTime < gpGlobals->time)
	{
		m_fKamikazeEventTime = -1;

		if (m_iKamikazeStage == 1)
		{
			EmmitChargeBalls();
			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alien_minddrill.wav", 0.8, 0.7, 0, 100);
		}
		else if (m_iKamikazeStage == 5)
		{
			KamikazeDetonate();
			return;
		}
		else
		{
			ALERT(at_console, "Drive controller: bad stage in anim event: %i\n", m_iKamikazeStage);
		}
	}

	if (m_fSequenceFinished)
	{
		switch (m_iKamikazeStage)
		{
		case 1:
			m_iKamikazeStage = 2;
			pev->sequence = LookupSequence("attack2");
			ResetSequenceInfo();
			pev->frame = 0;
			m_fNextKamikazeZap = gpGlobals->time + RANDOM_FLOAT(0.1, 0.2);
			m_fNextKamikazeBeam = gpGlobals->time + RANDOM_FLOAT(0.8, 0.1); // sic, low > high
			m_fKamikazeGlowTarget = 30;
			KamikazeSeqFinished();
			break;
		case 2:
			m_iKamikazeStage = 3;
			pev->sequence = LookupSequence("idle2");
			ResetSequenceInfo();
			pev->frame = 0;
			m_fKamikazeGlowTarget = 50;
			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alien_minddrill.wav", 0.9, 0.85, SND_CHANGE_VOL | SND_CHANGE_PITCH, 110);
			KamikazeSeqFinished();
			break;
		case 3:
			m_iKamikazeStage = 4;
			pev->sequence = LookupSequence("attack2");
			ResetSequenceInfo();
			pev->frame = 0;
			m_fKamikazeGlowTarget = 60;
			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alien_minddrill.wav", 1.0, 0.85, SND_CHANGE_VOL | SND_CHANGE_PITCH, 120);
			KamikazeSeqFinished();
			break;
		case 4:
			m_iKamikazeStage = 5;
			pev->sequence = LookupSequence("attack1");
			ResetSequenceInfo();
			pev->frame = 0;
			m_fKamikazeGlowTarget = 80;
			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alien_minddrill.wav", 1.0, 0.85, SND_CHANGE_VOL | SND_CHANGE_PITCH, 130);
			KamikazeSeqFinished();
			m_fKamikazeEventTime = gpGlobals->time + 1.8;
			DCBeamEnts(pev->origin, ENTINDEX(edict()) + 0x3000, ENTINDEX(edict()) + 0x4000, 19, 15, 60, 255, 0, 0, 255, 30);
			break;
		default:
			ALERT(at_console, "Drive controller: bad stage in seq finished: %i\n", m_iKamikazeStage);
			break;
		}
	}

	if (m_iKamikazeStage > 1)
		KamikazeFrame();

	m_bSetIdleAnim = false;
	m_bCanMove = false;
}

// orig 0x14cca0
int CDriveController::LookupMoveSeq(Vector vecDir)
{
	UTIL_MakeAimVectors(pev->angles);

	float flForward = DotProduct(gpGlobals->v_forward, vecDir);
	float flRight = DotProduct(gpGlobals->v_right, vecDir);
	float flUp = DotProduct(gpGlobals->v_up, vecDir);

	const char* pszSeq;
	if (fabs(flForward) > fabs(flRight) && fabs(flForward) > fabs(flUp))
		pszSeq = flForward > 0 ? "forward" : "backward";
	else if (fabs(flRight) > fabs(flUp))
		pszSeq = flRight > 0 ? "right" : "left";
	else
		pszSeq = flUp > 0 ? "up" : "down";

	return LookupSequence(pszSeq);
}

// orig 0x14cdd0
void CDriveController::DoMovement()
{
	float flFriction = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerFriction", 2.4);
	if (pev->waterlevel > 1)
		flFriction = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerWaterFriction", 3.8);

	pev->velocity.z = pev->velocity.z + CVAR_GET_FLOAT("sv_gravity") * pev->gravity * m_fFrameTime;

	// friction
	float flSpeed = pev->velocity.Length();
	if (flSpeed > 0.1)
	{
		float flControl = flSpeed < 6.0 ? 6.0 : flSpeed;
		float flNewSpeed = flSpeed - flControl * flFriction * m_fFrameTime;
		if (flNewSpeed < 0)
			flNewSpeed = 0;
		flNewSpeed = flNewSpeed / flSpeed;
		pev->velocity = pev->velocity * flNewSpeed;
	}

	const int iMoveButtons = IN_JUMP | IN_DUCK | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT;

	if (pev->velocity.Length() > 80 && (m_pOwner->pev->button & iMoveButtons) == 0)
	{
		// coasting
		int iSeq = LookupMoveSeq(pev->velocity);
		if (pev->sequence != iSeq && m_bSetIdleAnim)
		{
			pev->sequence = iSeq;
			ResetSequenceInfo();
		}
		m_bMoving = true;
		m_bSetIdleAnim = false;
	}
	else if (m_bCanMove && (m_pOwner->pev->button & iMoveButtons) != 0)
	{
		Vector vecWish = g_vecZero;
		UTIL_MakeAimVectors(pev->angles);

		float flWaterFrac = 1.0;
		if (pev->waterlevel > 1)
			flWaterFrac = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerWaterSpeedFrac", 0.75);

		float flMaxSpeed = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerSpeed", 280);
		float flAccel = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerAccel", 300);
		pev->framerate = 1.0;

		// forward/back follow the aim (pitch included), strafe and up/down only the yaw
		Vector vecYaw = Vector(0, pev->angles.y, 0);
		Vector vecRight, vecUp;
		g_engfuncs.pfnAngleVectors(vecYaw, NULL, vecRight, vecUp);

		if ((m_pOwner->pev->button & IN_FORWARD) != 0)
			vecWish = vecWish + gpGlobals->v_forward;
		if ((m_pOwner->pev->button & IN_BACK) != 0)
			vecWish = vecWish - gpGlobals->v_forward;
		if ((m_pOwner->pev->button & IN_MOVERIGHT) != 0)
			vecWish = vecWish + vecRight;
		if ((m_pOwner->pev->button & IN_MOVELEFT) != 0)
			vecWish = vecWish - vecRight;
		if ((m_pOwner->pev->button & IN_JUMP) != 0)
			vecWish = vecWish + vecUp;
		if ((m_pOwner->pev->button & IN_DUCK) != 0)
			vecWish = vecWish - vecUp;

		vecWish = vecWish.Normalize();

		float flWishSpeed = flMaxSpeed * flWaterFrac;
		float flAddSpeed = flWishSpeed - DotProduct(pev->velocity, vecWish);
		if (flAddSpeed > 0)
		{
			float flAccelSpeed = flAccel * flWaterFrac * m_fFrameTime * flWishSpeed;
			if (flAddSpeed < flAccelSpeed)
				flAccelSpeed = flAddSpeed;

			pev->velocity = pev->velocity + vecWish * flAccelSpeed;

			int iSeq = LookupMoveSeq(vecWish);
			if (pev->sequence != iSeq && m_bSetIdleAnim)
			{
				pev->sequence = iSeq;
				ResetSequenceInfo();
			}
			m_bSetIdleAnim = false;
			m_bMoving = true;
		}
	}
}

// orig 0x14d380
void CDriveController::DoAngles()
{
	float flYawSpeed = 1500;
	float flPitchSpeed = 800;
	if (m_bMoving)
	{
		flYawSpeed = 1000;
		flPitchSpeed = 600;
	}
	flYawSpeed = flYawSpeed * m_fFrameTime;

	pev->angles.x = UTIL_Approach(-pev->idealpitch, pev->angles.x, flPitchSpeed * m_fFrameTime);

	float flPitch = -40;
	if (pev->angles.x >= -40)
	{
		flPitch = 40;
		if (!(pev->angles.x > 40))
			flPitch = pev->angles.x;
	}
	pev->angles.x = flPitch;

	ChangeYaw((int)flYawSpeed);
}

// orig 0x14d470
void CDriveController::UpdateOwnerHUD()
{
	if (!m_pOwner)
		return;

	if (pev->health == (float)m_iLastHUDHealth)
		return;

	float flPercent = (pev->health / pev->max_health) * 100;
	int iHealth = flPercent > 0 ? (int)flPercent : 0;
	if (iHealth < 1)
		iHealth = 1;
	else if (iHealth > 100)
		iHealth = 100;

	MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->edict());
	WRITE_SHORT(iHealth);
	MESSAGE_END();

	m_iLastHUDHealth = (int)pev->health;
}

// orig 0x14d590
void CDriveController::ThrowBall()
{
	if (!m_pOwner)
		return;

	Vector vecHand, vecAngles;
	GetAttachment(2, vecHand, vecAngles);

	CBaseEntity* pBall = CBaseEntity::Create("controller_throw_ball", vecHand, g_vecZero, edict());
	pBall->pev->dmg_inflictor = m_pOwner->edict();

	Vector vecAim;
	vecAim.y = pev->angles.y;
	vecAim.z = pev->angles.z;
	vecAim.x = -60;
	if (pev->idealpitch >= -60)
	{
		vecAim.x = 60;
		if (!(pev->idealpitch > 60))
			vecAim.x = pev->idealpitch;
	}
	UTIL_MakeVectors(vecAim);

	pBall->pev->speed = 800;
	pBall->pev->movedir = gpGlobals->v_forward;
	pBall->pev->velocity = pBall->pev->movedir * pBall->pev->speed;
}

// orig 0x14d6d0
void CDriveController::EmmitChargeBalls()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "x/x_ballattack1.wav", 0.9, 0.9, 0, RANDOM_LONG(120, 130));

	Vector vecHand, vecAngles;
	GetAttachment(0, vecHand, vecAngles);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecHand);
	WRITE_BYTE(TE_ELIGHT);
	WRITE_SHORT(ENTINDEX(edict()) + 0x1000);
	WRITE_COORD(vecHand.x);
	WRITE_COORD(vecHand.y);
	WRITE_COORD(vecHand.z + 16);
	WRITE_COORD(32);
	WRITE_BYTE(255);
	WRITE_BYTE(192);
	WRITE_BYTE(64);
	WRITE_BYTE(13);
	WRITE_COORD(32);
	MESSAGE_END();

	for (int i = 0; i < 4; i++)
	{
		// orphan the previous ball, it dies off on its own
		if (m_pChargeBalls[i])
			m_pChargeBalls[i]->pev->owner = NULL;

		m_pChargeBalls[i] = CBaseEntity::Create("controller_charge_ball", vecHand, g_vecZero, edict());
		entvars_t* pevBall = m_pChargeBalls[i]->pev;

		float x = RANDOM_FLOAT(-1, 1);
		float y = RANDOM_FLOAT(-1, 1);
		float z = RANDOM_FLOAT(0.5, 1);
		Vector vecDir = Vector(x, y, z).Normalize();
		pevBall->velocity = vecDir * RANDOM_FLOAT(200, 300);

		m_pChargeBalls[i]->pev->renderamt = 60;
	}
}

// orig 0x14d9f0
void CDriveController::ChargeBigBall()
{
	if (!m_pOwner)
		return;

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/beamstart1.wav", 1.0, 0.9, 0, RANDOM_LONG(110, 120));

	Vector vecHand, vecAngles;
	GetAttachment(2, vecHand, vecAngles);
	DCELight(vecHand, ENTINDEX(edict()) + 0x3000, 20, 5, 32);
	GetAttachment(3, vecHand, vecAngles);
	DCELight(vecHand, ENTINDEX(edict()) + 0x4000, 20, 5, 32);

	if (!m_pChargedBall)
	{
		m_pChargedBall = CBaseEntity::Create("controller_charged_ball", pev->origin, g_vecZero, edict());
		m_pChargedBall->pev->dmg_inflictor = m_pOwner->edict();
		m_pChargedBall->pev->dmg = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallStartDamage", 80);
		GetAttachment(1, vecHand, vecAngles);
		UTIL_SetOrigin(m_pChargedBall->pev, vecHand);
	}
	else
	{
		m_pChargedBall->pev->dmg = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallDamageGainPerBurst", 40) + m_pChargedBall->pev->dmg;

		if (m_pChargedBall->pev->dmg > PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallMaxDamage", 320))
		{
			// overcharged: the excess lashes back at the controller
			pev->classname = MAKE_STRING("controller_overcharge");
			float flMax = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallMaxDamage", 320);
			float flFract = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallDamageOverflowBacklashFract", 0.5);
			TakeDamage(pev, m_pOwner->pev, (m_pChargedBall->pev->dmg - flMax) * flFract, DMG_SHOCK);
			pev->classname = MAKE_STRING("drive_controller");

			EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "weapons/electro4.wav", 0.8, 0.9, 0, RANDOM_LONG(140, 150));

			DCBeamEnts(pev->origin, ENTINDEX(edict()) + 0x1000, ENTINDEX(m_pChargedBall->edict()), 2, 25, 40, 255, 128, 64, 250, 30);

			m_pChargedBall->pev->dmg = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallMaxDamage", 320);
		}

		m_fChargeSoundVol = m_fChargeSoundVol + 0.6666666;
		m_iChargeSoundPitch = m_iChargeSoundPitch + 6;
		if (m_fChargeSoundVol > 1.0)
			m_fChargeSoundVol = 1.0;
		if (m_iChargeSoundPitch > 120)
			m_iChargeSoundPitch = 120;
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alienfazzle1.wav", m_fChargeSoundVol, 0.85, SND_CHANGE_VOL | SND_CHANGE_PITCH, m_iChargeSoundPitch);

		m_pChargedBall->pev->renderamt = m_pChargedBall->pev->renderamt + 33.333332;
		m_pChargedBall->pev->scale = m_pChargedBall->pev->scale + 0.1;
		if (m_pChargedBall->pev->renderamt > 250)
			m_pChargedBall->pev->renderamt = 250;
		if (m_pChargedBall->pev->scale > 1.4)
			m_pChargedBall->pev->scale = 1.4;
	}

	for (int i = 0; i < 4; i++)
	{
		if (m_pChargeBalls[i])
		{
			DCBeamEnts(pev->origin, ENTINDEX(m_pChargeBalls[i]->edict()), ENTINDEX(m_pChargedBall->edict()), 3, 17, 80, 255, 128, 64, 190, 30);
			m_pChargeBalls[i]->pev->renderamt = 130;
		}
	}
}

// orig 0x14e320
void CDriveController::FireChargedBall()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alienfazzle1.wav", 0, 0, SND_STOP, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/beamstart9.wav", 1.0, 0.9, 0, RANDOM_LONG(120, 130));
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart10.wav", 1.0, 0.9, 0, RANDOM_LONG(80, 90));

	for (int i = 0; i < 4; i++)
	{
		if (!m_pChargeBalls[i])
			continue;

		m_pChargeBalls[i]->pev->weapons = 1; // released: CBThink lets it go
		if (m_pChargedBall)
			DCBeamEnts(pev->origin, ENTINDEX(m_pChargeBalls[i]->edict()), ENTINDEX(m_pChargedBall->edict()), 4, 15, 100, 255, 128, 64, 210, 30);
		m_pChargeBalls[i] = NULL;
	}

	if (m_pChargedBall)
		m_pChargedBall->pev->weapons = 2; // fired: CFThink arms it

	if (m_pOwner)
	{
		Vector vecAim;
		vecAim.y = pev->angles.y;
		vecAim.z = pev->angles.z;
		vecAim.x = -70;
		if (pev->idealpitch >= -70)
		{
			vecAim.x = 70;
			if (!(pev->idealpitch > 70))
				vecAim.x = pev->idealpitch;
		}
		UTIL_MakeVectors(vecAim);

		// VERIFY: the original does not check m_pChargedBall here (stage 4 always follows a ChargeBigBall)
		m_pChargedBall->pev->speed = 500;
		m_pChargedBall->pev->movedir = gpGlobals->v_forward;
		m_pChargedBall->pev->velocity = m_pChargedBall->pev->movedir * m_pChargedBall->pev->speed;
		m_pChargedBall->pev->nextthink = gpGlobals->time;
		DispatchThink(m_pChargedBall->edict());
		m_pChargedBall = NULL;
	}
}

// orig 0x14e700
void CDriveController::KamikazeFrame()
{
	if (m_fNextKamikazeZap < gpGlobals->time)
	{
		// a zap from a random hand to whatever a random direction hits
		int iAttach = 1;
		switch (RANDOM_LONG(1, 3))
		{
		case 1:
			iAttach = 1;
			break;
		case 2:
			iAttach = 3;
			break;
		case 3:
			iAttach = 4;
			break;
		}

		float x = RANDOM_FLOAT(-1, 1);
		float y = RANDOM_FLOAT(-1, 1);
		float z = RANDOM_FLOAT(-1, 1);
		Vector vecDir = Vector(x, y, z).Normalize();

		TraceResult tr;
		UTIL_TraceLine(pev->origin, pev->origin + vecDir * 2048, ignore_monsters, ENT(pev), &tr);

		MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_BEAMENTPOINT);
		WRITE_SHORT(ENTINDEX(edict()) + iAttach * 0x1000);
		WRITE_COORD(tr.vecEndPos.x);
		WRITE_COORD(tr.vecEndPos.y);
		WRITE_COORD(tr.vecEndPos.z);
		WRITE_SHORT(g_sModelIndexLaser);
		WRITE_BYTE(0);	 // start frame
		WRITE_BYTE(0);	 // framerate
		WRITE_BYTE(3);	 // life
		WRITE_BYTE(30);	 // width
		WRITE_BYTE(180); // noise
		WRITE_BYTE(230);
		WRITE_BYTE(230);
		WRITE_BYTE(255);
		WRITE_BYTE(240); // brightness
		WRITE_BYTE(50);	 // speed
		MESSAGE_END();

		UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart15.wav", 1.0, 0.4, 0, RANDOM_LONG(120, 130));

		switch (m_iKamikazeStage)
		{
		case 2:
			m_fNextKamikazeZap = gpGlobals->time + RANDOM_FLOAT(0.3, 0.4);
			break;
		case 3:
			m_fNextKamikazeZap = gpGlobals->time + RANDOM_FLOAT(0.2, 0.3);
			break;
		case 4:
			m_fNextKamikazeZap = gpGlobals->time + RANDOM_FLOAT(0.1, 0.2);
			break;
		case 5:
			m_fNextKamikazeZap = gpGlobals->time + RANDOM_FLOAT(0.07, 0.15);
			break;
		}
	}

	if (m_fNextKamikazeBeam < gpGlobals->time)
	{
		for (int i = 0; i < 4; i++)
		{
			if (!m_pChargeBalls[i])
				continue;

			int iAttach = 1;
			switch (RANDOM_LONG(1, 3))
			{
			case 1:
				iAttach = 1;
				break;
			case 2:
				iAttach = 3;
				break;
			case 3:
				iAttach = 4;
				break;
			}

			DCBeamEnts(pev->origin, ENTINDEX(edict()) + iAttach * 0x1000, ENTINDEX(m_pChargeBalls[i]->edict()), 1, 15, 80, 255, 128, 64, 180, 30);
		}

		float flSoundChance = 0.4;
		switch (m_iKamikazeStage)
		{
		case 2:
			m_fNextKamikazeBeam = gpGlobals->time + RANDOM_FLOAT(0.1, 0.15);
			flSoundChance = 0.4;
			break;
		case 3:
			m_fNextKamikazeBeam = gpGlobals->time + RANDOM_FLOAT(0.06, 0.08);
			flSoundChance = 0.2;
			break;
		case 4:
			m_fNextKamikazeBeam = gpGlobals->time + RANDOM_FLOAT(0.05, 0.07);
			flSoundChance = 0.12;
			break;
		case 5:
			m_fNextKamikazeBeam = gpGlobals->time + RANDOM_FLOAT(0.04, 0.06);
			flSoundChance = 0.08;
			break;
		}

		if (RANDOM_FLOAT(0, 1) < flSoundChance)
			EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "debris/beamstart1.wav", 1.0, 1.0, 0, RANDOM_LONG(140, 150));
	}

	pev->renderamt = UTIL_Approach(m_fKamikazeGlowTarget, pev->renderamt, m_fFrameTime * 80);
	pev->rendercolor.x = (pev->renderamt / 80) * 205 + 50;
}

// orig 0x14ef50
void CDriveController::KamikazeSeqFinished()
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/beamstart10.wav", 1.0, 0.9, 0, RANDOM_LONG(80, 90));

	for (int i = 0; i < 4; i++)
	{
		if (!m_pChargeBalls[i])
			continue;

		int iAttach = 1;
		switch (RANDOM_LONG(1, 3))
		{
		case 1:
			iAttach = 1;
			break;
		case 2:
			iAttach = 3;
			break;
		case 3:
			iAttach = 4;
			break;
		}

		DCBeamEnts(pev->origin, iAttach * 0x1000 + ENTINDEX(edict()), ENTINDEX(m_pChargeBalls[i]->edict()), 6, 20, 120, 255, 0, 0, 230, 30);
		m_pChargeBalls[i]->pev->renderamt = 130;
	}
}

// Does a line from vecSrc reach vecEnd, punching through at most iMaxPunches walls whose total
// thickness is at most flMaxPunch? orig 0x14f1b0
bool DCPunchTrace(Vector vecSrc, Vector vecEnd, float flMaxPunch, int iMaxPunches, entvars_t* pevIgnore)
{
	Vector vecStart = vecSrc;
	Vector vecDir = (vecEnd - vecSrc).Normalize();

	if (flMaxPunch >= 0)
	{
		while (iMaxPunches > 0)
		{
			TraceResult tr;
			UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, pevIgnore->pContainingEntity, &tr);
			if (tr.flFraction == 1.0)
				return true;
			if (tr.fAllSolid)
				return false;
			if (tr.fStartSolid)
				return false;

			// trace back from flMaxPunch in to find the wall's far side
			TraceResult trBack;
			UTIL_TraceLine(tr.vecEndPos + vecDir * flMaxPunch, tr.vecEndPos + vecDir, ignore_monsters, pevIgnore->pContainingEntity, &trBack);
			if (trBack.fAllSolid)
				return false;

			flMaxPunch = flMaxPunch - (trBack.vecEndPos - tr.vecEndPos).Length();
			vecStart = trBack.vecEndPos + vecDir;
			iMaxPunches--;

			if (!(flMaxPunch >= 0))
				return false;
		}
	}

	return false;
}

// orig 0x14f490
void CDriveController::KamikazeDetonate()
{
	if (!m_pOwner)
		return;

	Vector vecSrc = pev->origin + Vector(0, 0, 32);
	float flMaxDist = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerKamikazeMaxBlastDist", 1400);
	float flMaxDamage = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerKamikazeMaxBlastDamage", 1500);
	pev->classname = MAKE_STRING("controller_kamikaze");

	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityInSphere(pEnt, vecSrc, 8192)) != NULL)
	{
		if (pEnt->pev == pev || pEnt->pev == m_pOwner->pev)
			continue;

		if (pEnt->pev->takedamage == DAMAGE_NO && !pEnt->IsNetClient())
			continue;

		Vector vecEnt = pEnt->Center();
		float flDist = (vecEnt - vecSrc).Length();

		if (flDist > flMaxDist && !pEnt->IsNetClient())
			continue;

		if (flDist > flMaxDist)
		{
			// out of reach: players who can see it get the flash
			TraceResult tr;
			UTIL_TraceLine(vecSrc, pEnt->pev->origin + pEnt->pev->view_ofs, ignore_monsters, ENT(pev), &tr);
			if (tr.flFraction >= 1.0 || tr.pHit == pEnt->edict())
				UTIL_ScreenFade(pEnt, Vector(255, 255, 255), 0.8, 0.1, 240, 0);
			continue;
		}

		if (!DCPunchTrace(vecSrc, vecEnt, PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerKamikazeMaxPunchDist", 64), 5, pev))
			continue;

		if (pEnt->IsNetClient())
			UTIL_ScreenFade(pEnt, Vector(255, 255, 255), 0.9, 0.2, 255, 0);

		float flDamage = (1.0 - flDist / flMaxDist) * flMaxDamage;
		if (flDamage > 0)
			pEnt->TakeDamage(pev, m_pOwner->pev, flDamage, DMG_BLAST);
	}

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgShake, NULL);
	WRITE_SHORT(FixedUnsigned16(250, 1 << 12)); // amplitude
	WRITE_SHORT(FixedUnsigned16(0.9, 1 << 12)); // duration
	WRITE_SHORT(FixedUnsigned16(40, 1 << 8));	  // frequency
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
	WRITE_BYTE(TE_BEAMTORUS);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z + 4096);
	WRITE_SHORT(g_sModelIndexLaser);
	WRITE_BYTE(0);	 // start frame
	WRITE_BYTE(0);	 // framerate
	WRITE_BYTE(8);	 // life
	WRITE_BYTE(180); // width
	WRITE_BYTE(0);	 // noise
	WRITE_BYTE(255);
	WRITE_BYTE(160);
	WRITE_BYTE(100);
	WRITE_BYTE(200); // brightness
	WRITE_BYTE(0);	 // speed
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexFireball);
	WRITE_BYTE(190); // scale
	WRITE_BYTE(12);	 // framerate
	WRITE_BYTE(TE_EXPLFLAG_NONE);
	MESSAGE_END();

	for (int i = 0; i < 4; i++)
	{
		if (m_pChargeBalls[i])
			m_pChargeBalls[i]->pev->weapons = 1;
		m_pChargeBalls[i] = NULL;
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/mortarhit.wav", 1.0, 0.4, 0, 100);
	UTIL_ScreenFade(m_pOwner, Vector(255, 255, 255), 1.1, 0.3, 255, 0);

	pev->health = -9999;
	g_pevLastInflictor = pev;
	Killed(m_pOwner->pev, GIB_ALWAYS);
}

// orig 0x14fc00
bool CDriveController::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_pOwner)
	{
		CBaseEntity* pAttacker = CBaseEntity::Instance(pevAttacker);
		if (pAttacker && !g_pGameRules->FPlayerCanTakeDamage(m_pOwner, CBaseEntity::Instance(pevAttacker)))
			return false;

		if (m_pOwner && (m_pOwner->pev->flags & FL_GODMODE) != 0)
			return false;
	}

	// VERIFY: the original writes this through m_pOwner unchecked (a null write when driverless)
	if (m_pOwner)
		m_pOwner->m_fDrvEntEjectTime = gpGlobals->time + 2.0;

	bool bResult = CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
	if (bResult && m_fNextPainSound < gpGlobals->time)
	{
		int pitch = RANDOM_LONG(95, 105);
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDCPainSounds[RANDOM_LONG(0, 2)], 1.0, 0.8, 0, pitch);
		m_fNextPainSound = gpGlobals->time + RANDOM_FLOAT(0.3, 0.6);
	}

	return bResult;
}

// orig 0x14fdc0
void CDriveController::DyingThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	float flInterval = StudioFrameAdvance();
	DispatchAnimEvents(flInterval);

	for (int i = 0; i < 2; i++)
	{
		if (!m_pHandGlow[i])
			continue;

		m_pHandGlow[i]->pev->renderamt = UTIL_Approach(0, m_pHandGlow[i]->pev->renderamt, flInterval * 424.99997);
		if (m_pHandGlow[i]->pev->renderamt <= 0)
		{
			m_pHandGlow[i]->SUB_Remove();
			m_pHandGlow[i] = NULL;
		}
	}

	if (((pev->flags & FL_ONGROUND) == 0 || !m_fSequenceFinished) && gpGlobals->time < pev->dmgtime)
		return;

	for (int i = 0; i < 2; i++)
	{
		if (m_pHandGlow[i])
			m_pHandGlow[i]->SUB_Remove();
	}

	FadeMonster();
}

// orig 0x14ff30
void CDriveController::Killed(entvars_t* pevAttacker, int iGib)
{
	if (pev->deadflag != DEAD_NO)
		return;

	pev->sequence = LookupSequence("die1");
	ResetSequenceInfo();
	pev->frame = 0;
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->flags &= ~FL_ONGROUND;
	pev->gravity = 0;
	pev->velocity.z = -100;
	pev->renderamt = 0;
	pev->renderfx = kRenderFxNone;

	for (int i = 0; i < 4; i++)
	{
		if (m_pChargeBalls[i])
			m_pChargeBalls[i]->pev->owner = NULL;
	}
	if (m_pChargedBall)
		m_pChargedBall->pev->owner = NULL;

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alienfazzle1.wav", 0, 0, SND_STOP, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "ambience/alien_minddrill.wav", 0, 0, SND_STOP, 100);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);

	// tell the pickup it may respawn
	if (pev->owner)
		CBaseEntity::Instance(pev->owner)->DeathNotice(pev);

	int pitch = RANDOM_LONG(95, 105);
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, g_pDCDieSounds[RANDOM_LONG(0, 1)], 1.0, 0.8, 0, pitch);

	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->edict());
		WRITE_BYTE(HIDEHUD_ALL);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, m_pOwner->edict());
		WRITE_SHORT(0);
		MESSAGE_END();
	}

	DEClientLeave();

	// the driver dies with it
	if (m_pOwner && m_pOwner->m_bInDriveEnt && m_pOwner->m_pCurDriveEnt && m_pOwner->m_pCurDriveEnt->pev == pev)
	{
		UTIL_SetOrigin(m_pOwner->pev, pev->origin + Vector(0, 0, 64));
		m_pOwner->pev->effects |= EF_NODRAW;
		m_pOwner->pev->health = -1;
		m_pOwner->Killed(pevAttacker, GIB_NEVER);
	}

	if ((pev->health < -30 && iGib != GIB_NEVER) || iGib == GIB_ALWAYS)
	{
		m_pHandGlow[0]->pev->effects |= EF_NODRAW;
		UTIL_Remove(m_pHandGlow[0]);
		m_pHandGlow[1]->pev->effects |= EF_NODRAW;
		UTIL_Remove(m_pHandGlow[1]);
		GibMonster();
	}
	else
	{
		pev->dmgtime = gpGlobals->time + 8.0;
		m_pHandGlow[0]->SUB_StartFadeOut();
		m_pHandGlow[1]->SUB_StartFadeOut();
		SetThink(&CDriveController::DyingThink);
		SetTouch(NULL);
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

// orig 0x150390
void CDriveController::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	switch (pEvent->event)
	{
	case 2001:
		if ((pev->flags & FL_ONGROUND) == 0)
			return;
		if (RANDOM_LONG(0, 1) == 0)
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/bodydrop3.wav", 1, ATTN_NORM, 0, 100);
		else
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/bodydrop4.wav", 1, ATTN_NORM, 0, 100);
		break;
	case 2002:
		if ((pev->flags & FL_ONGROUND) == 0)
			return;
		if (RANDOM_LONG(0, 1) == 0)
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/bodydrop3.wav", 1, ATTN_NORM, 0, 90);
		else
			EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "common/bodydrop4.wav", 1, ATTN_NORM, 0, 90);
		break;
	}
}

// Steer from the driver's view and put the driver's camera behind the controller. orig 0x1504a0
void CDriveController::DEClientPreThink()
{
	if ((m_pOwner->pev->button & IN_RELOAD) == 0)
	{
		// reload held frees the camera from the controller's heading
		pev->ideal_yaw = UTIL_AngleMod(m_pOwner->pev->v_angle.y);
		pev->idealpitch = m_pOwner->pev->v_angle.x;
	}

	m_pOwner->pev->rendermode = kRenderTransColor;
	m_pOwner->pev->renderamt = 0;
	m_pOwner->pev->effects |= EF_NODRAW;

	UTIL_MakeVectors(m_pOwner->pev->v_angle);
	Vector vecRot = GetCamRotOrg();
	Vector vecEnd = vecRot - gpGlobals->v_forward * 200;

	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	TraceResult tr;
	UTIL_TraceLine(vecRot, vecEnd, ignore_monsters, ENT(pev), &tr);

	Vector vecCam = tr.vecEndPos;
	if (tr.flFraction < 1.0)
		vecCam = tr.vecEndPos + tr.vecPlaneNormal * 12;

	// out of water, pull the camera forward until it is in open air
	int i = 0;
	if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
	{
		for (i = 1; i < 48; i++)
		{
			if (UTIL_PointContents(vecCam) == CONTENTS_EMPTY)
				break;
			vecCam = tr.vecEndPos + gpGlobals->v_forward * 8 * (float)i;
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
		vecCam = vecRot;

	m_pOwner->m_afPhysicsFlags = 0;
	UTIL_SetOrigin(m_pOwner->pev, vecCam);

	if (m_pOwner->m_fDrvEntAlpha >= 1.0 || !(m_pOwner->m_fDrvEntAlpha > 0))
	{
		m_pOwner->pev->euser3 = NULL; // VERIFY: the original clears euser3 here but sets/clears euser2 elsewhere
		m_pOwner->pev->iuser3 = 0;
		m_pOwner->pev->fuser1 = 0;
		m_pOwner->pev->fuser2 = 0;
	}
	else
	{
		m_pOwner->pev->euser2 = edict();
		m_pOwner->pev->iuser3 = 4;
		m_pOwner->pev->fuser1 = 2.0;

		float flAlpha = 0;
		if (m_pOwner->m_fDrvEntAlpha * 255 >= 0)
		{
			flAlpha = 255;
			if (m_pOwner->m_fDrvEntAlpha * 255 <= 255)
				flAlpha = m_pOwner->m_fDrvEntAlpha * 255;
		}
		m_pOwner->pev->fuser2 = flAlpha;
	}

	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;
}

// orig 0x150930
void CDriveController::DEClientPostThink()
{
	m_pOwner->m_afPhysicsFlags = 0;
}

// orig 0x150950
void CDriveController::DEClientEnter(CBasePlayer* pPlayer)
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

	m_iLastHUDHealth = -1;
	m_vecEnterOrigin = pev->origin;
	m_fEnterTime = gpGlobals->time;

	MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, m_pOwner->edict());
	WRITE_BYTE(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, m_pOwner->edict());
	WRITE_BYTE(m_pOwner->m_iHideHUD);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgBattery, NULL, m_pOwner->edict());
	WRITE_SHORT(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgFlashBattery, NULL, m_pOwner->edict());
	WRITE_BYTE(0);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ONE, gmsgTrain, NULL, m_pOwner->edict());
	WRITE_BYTE(0);
	MESSAGE_END();

	pev->ideal_yaw = m_pOwner->pev->v_angle.y;
	pev->idealpitch = m_pOwner->pev->v_angle.x;

	CLIENT_COMMAND(m_pOwner->edict(), "cl_forwardspeed 0\ncl_backspeed 0\ncl_sidespeed 0\n");
}

// Give the driver back a normal player body. orig 0x150d30
void CDriveController::DEClientLeave()
{
	if (!m_pOwner || !m_pOwner->pev || !m_pOwner->edict() || m_pOwner->edict()->free || !m_pOwner->m_bInDriveEnt)
		return;

	CLIENT_COMMAND(m_pOwner->edict(), "cl_forwardspeed 1000\ncl_backspeed 1000\ncl_sidespeed 1000\n");

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

	if (m_pOwner->m_pActiveItem)
		m_pOwner->m_pActiveItem->Deploy();

	m_pOwner->pev->euser2 = NULL;
	m_pOwner->pev->iuser3 = 0;
	m_pOwner->pev->fuser1 = 0;
	m_pOwner->pev->fuser2 = 0;
	m_pOwner->pev->fuser3 = 0;
	m_pOwner->pev->vuser1 = g_vecZero;

	m_pOwner->m_iHideHUD &= ~(HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT);

	SET_VIEW(m_pOwner->edict(), m_pOwner->edict());
	SetupVisibility(m_pOwner->edict(), m_pOwner->edict(), NULL, NULL);
}

// Throw the driver out the back. orig 0x151040
void CDriveController::DEClientEject()
{
	if (!PREFS_KeyLoopUpBool("DRIVECONTROLLER", "AllowEject", false))
	{
		if (CVAR_GET_FLOAT("sv_allowglobaleject") != 1 && g_pGameRules->IsMultiplayer() && !g_pGameRules->IsCoOp())
			return;
	}

	if (!m_pOwner->m_bInDriveEnt || !m_pOwner->m_pCurDriveEnt)
		return;

	entvars_t* pevDrive = m_pOwner->m_pCurDriveEnt->pev;
	if (pevDrive != pev || m_bInKamikaze || !(pevDrive->health > 0))
		return;

	if (!(m_pOwner->m_fDrvEntEjectTime < gpGlobals->time))
		return;

	UTIL_MakeAimVectors(pevDrive->angles);

	Vector vecSrc = pev->origin + Vector(0, 0, 32);
	Vector vecEnd = vecSrc - gpGlobals->v_forward * 128;

	TraceResult tr;
	UTIL_TraceLine(pev->origin + Vector(0, 0, 32), vecEnd, ignore_monsters, ENT(pev), &tr);

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
	m_pOwner->pev->angles.x = m_pOwner->pev->v_angle.x = -m_pOwner->pev->v_angle.x;
	m_pOwner->pev->angles.z = m_pOwner->pev->v_angle.z = 0;
	m_pOwner->pev->fixangle = 1;
	m_pOwner->m_pCurDriveEnt = NULL;

	EMIT_SOUND_DYN(m_pOwner->edict(), CHAN_BODY, "ambience/biggun3.wav", 1.0, 0.8, 0, 100);
}

// orig 0x156310
void CDriveController::EntIsBeingRemoved(edict_t* pEdict)
{
	if (m_pOwner && m_pOwner->pev && m_pOwner->edict() == pEdict)
		m_pOwner = NULL;

	if (IsAlive() && !m_pOwner)
		Killed(pev, GIB_NORMAL);
}

//=========================================================
// CDriveControllerPickup: the dormant controller a player +uses to drive one
//=========================================================
class CDriveControllerPickup : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	CBaseEntity* GetSpawnOrg();
	bool TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType) override;
	void EXPORT PAnimThink();
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void EXPORT DoRespawn();
	void DeathNotice(entvars_t* pevChild) override;

	int ObjectCaps() override { return m_bTakeable ? FCAP_ACROSS_TRANSITION | FCAP_IMPULSE_USE : FCAP_ACROSS_TRANSITION; } // orig 0x156260
	int GetRCCaps() override { return 0; }													  // orig 0x156280
	virtual bool IsTeleFraggabe() { return false; }											  // orig 0x156290
	int BloodColor() override { return DONT_BLEED; }										  // orig 0x1562a0
	int Classify() override { return CLASS_HUMAN_MILITARY; }								  // orig 0x1562b0

	CBaseEntity* m_pGiven; // orig +0x27c, the drive_controller it made
	bool m_bTakeable;	   // orig +0x280
};

LINK_ENTITY_TO_CLASS(pickup_drive_controller, CDriveControllerPickup); // orig 0x151460

TYPEDESCRIPTION CDriveControllerPickup::m_SaveData[] =
	{
		DEFINE_FIELD(CDriveControllerPickup, m_pGiven, FIELD_CLASSPTR),
		DEFINE_FIELD(CDriveControllerPickup, m_bTakeable, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDriveControllerPickup, CBaseMonster); // orig 0x151490, 0x1514e0

// orig 0x151530
void CDriveControllerPickup::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), "models/controller.mdl");
	UTIL_SetSize(pev, Vector(-24, -24, -24), Vector(24, 24, 24));
	pev->takedamage = DAMAGE_NO;
	pev->health = 1;
	pev->sequence = LookupSequence("float");
	ResetSequenceInfo();
	pev->frame = RANDOM_LONG(0, 255);
	InitBoneControllers();

	SetUse(&CDriveControllerPickup::Use);
	SetThink(&CDriveControllerPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;

	m_bTakeable = true;
	m_pGiven = NULL;

	CBaseEntity* pSpot = GetSpawnOrg();
	if (!pSpot)
	{
		SetThink(&CDriveControllerPickup::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	UTIL_SetOrigin(pev, pSpot->Center());
	pev->angles = Vector(0, pSpot->pev->angles.y, 0);
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	pev->renderfx = kRenderFxDistort;
	DoRespawn();
	UTIL_Remove(pSpot);
}

// orig 0x151760
void CDriveControllerPickup::Precache()
{
	PRECACHE_MODEL("models/controller.mdl");
	PRECACHE_SOUND("ambience/particle_suck1.wav");
	UTIL_PrecacheOther("drive_controller");
}

// The first usable info_drive_controller_spawn. orig 0x1517b0
CBaseEntity* CDriveControllerPickup::GetSpawnOrg()
{
	CBaseEntity* pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_drive_controller_spawn")) != NULL)
	{
		if ((pSpot->pev->flags & FL_KILLME) != 0)
			continue;
		if (FNullEnt(pSpot->pev))
			continue;
		return pSpot;
	}
	return NULL;
}

// orig 0x151830
bool CDriveControllerPickup::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return false;
}

// Fade in when takeable; end the drive when its time is up. orig 0x151840
void CDriveControllerPickup::PAnimThink()
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

// orig 0x1519e0
void CDriveControllerPickup::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!m_bTakeable)
		return;
	if (!pCaller->IsPlayer() || !pCaller)
		return;

	CBasePlayer* pPlayer = (CBasePlayer*)pCaller;
	if (pPlayer->m_fDrcEntTransTime > gpGlobals->time || pPlayer->m_bInDriveEnt)
		return;
	if (pPlayer->Classify() != CLASS_PLAYER)
		return;
	if (strcmp(STRING(pPlayer->pev->classname), "player") != 0)
		return;

	pPlayer->m_pCurDriveEnt = (CBaseDriveEnt*)CBaseEntity::Create("drive_controller", pev->origin, pev->angles, edict());
	pPlayer->pev->angles = Vector(0, pev->angles.y, 0);
	pPlayer->pev->v_angle = pPlayer->pev->angles;
	pPlayer->pev->fixangle = 1;
	pPlayer->m_pCurDriveEnt->DEClientEnter(pPlayer);
	pPlayer->m_bInDriveEnt = true;

	m_bTakeable = false;
	m_pGiven = pPlayer->m_pCurDriveEnt;
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	SetThink(&CDriveControllerPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmgtime = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerMaxDriveTime", 420) + gpGlobals->time;
}

// orig 0x151be0
void CDriveControllerPickup::DoRespawn()
{
	m_bTakeable = true;
	m_pGiven = NULL;
	pev->effects &= ~EF_NODRAW;
	pev->solid = SOLID_TRIGGER;
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "ambience/particle_suck1.wav", 1.0, 0.3, 0, 90);
	SetThink(&CDriveControllerPickup::PAnimThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// The controller it gave out died. orig 0x151ca0
void CDriveControllerPickup::DeathNotice(entvars_t* pevChild)
{
	if (m_bTakeable)
		return;

	pev->nextthink = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerRespawnDelayAfterDeath", 5.0) + gpGlobals->time;
	SetThink(&CDriveControllerPickup::DoRespawn);
}

//=========================================================
// CDrvControllerSpawner: makes mp_drivecontroller pickups 15 seconds into a multiplayer map
//=========================================================
class CDrvControllerSpawner : public CBaseEntity
{
public:
	void Spawn() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void EXPORT DrvAThink();
	int ObjectCaps() override { return FCAP_DONT_SAVE; } // orig 0x156200

	bool m_bDummy; // orig +0x34
};

// Called from CWorld::Precache. orig 0x151d10
void CreateDrvControllerSpawner()
{
	if (g_pGameRules->IsMultiplayer() && !UTIL_FindEntityByClassname(NULL, "drive_controller_spawner"))
		CBaseEntity::Create("drive_controller_spawner", g_vecZero, g_vecZero, NULL);
}

LINK_ENTITY_TO_CLASS(drive_controller_spawner, CDrvControllerSpawner); // orig 0x151d70

TYPEDESCRIPTION CDrvControllerSpawner::m_SaveData[] =
	{
		DEFINE_FIELD(CDrvControllerSpawner, m_bDummy, FIELD_BOOLEAN),
};

IMPLEMENT_SAVERESTORE(CDrvControllerSpawner, CBaseEntity); // orig 0x151da0, 0x151df0

// orig 0x151e40
void CDrvControllerSpawner::DrvAThink()
{
	for (int i = 0; i < (int)CVAR_GET_FLOAT("mp_drivecontroller"); i++)
		CBaseEntity::Create("pickup_drive_controller", g_vecZero, g_vecZero, NULL);

	UTIL_Remove(this);
}

// orig 0x151ed0
void CDrvControllerSpawner::Spawn()
{
	pev->classname = MAKE_STRING("drive_controller_spawner");
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetThink(&CDrvControllerSpawner::DrvAThink);
	pev->nextthink = gpGlobals->time + 15.0;
}

//=========================================================
// CDCThrowBall: the primary attack's homing ball; forks in two when its fork time comes.
// pev->weapons: forks left. pev->dmg: damage scale. pev->dmg_inflictor: the driver.
//=========================================================
class CDCThrowBall : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT TBThink();
	void EXPORT TBTouch(CBaseEntity* pOther);
};

LINK_ENTITY_TO_CLASS(controller_throw_ball, CDCThrowBall); // orig 0x151f70

// orig 0x151fa0
void CDCThrowBall::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->classname = MAKE_STRING("controller_throw_ball");
	SET_MODEL(ENT(pev), "sprites/xspark4.spr");
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor = Vector(255, 255, 255);
	pev->renderamt = 240;
	pev->scale = 1.0;
	pev->dmg = 1.0;
	pev->weapons = PREFS_KeyLoopUpInt("DRIVECONTROLLER", "ControllerThrowBallMaxForks", 3);

	float flForkMax = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerThrowBallForkMaxTime", 0.4);
	float flForkMin = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerThrowBallForkMinTime", 0.2);
	pev->dmgtime = gpGlobals->time + RANDOM_FLOAT(flForkMin, flForkMax);

	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);
	pev->armorvalue = gpGlobals->time;
	pev->enemy = NULL;
	SetThink(&CDCThrowBall::TBThink);
	SetTouch(&CDCThrowBall::TBTouch);
	pev->nextthink = gpGlobals->time + 0.1;
}

// orig 0x152190
void CDCThrowBall::Precache()
{
	PRECACHE_MODEL("sprites/xspark4.spr");
	PRECACHE_SOUND("debris/beamstart10.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
}

// orig 0x1521e0
void CDCThrowBall::TBThink()
{
	if (pev->dmgtime < gpGlobals->time && pev->weapons > 0)
	{
		// fork: two smaller balls take over and this one goes
		pev->effects |= EF_NODRAW;
		pev->flags |= FL_KILLME;
		SetTouch(NULL);
		SetThink(NULL);

		Vector vecAngles = UTIL_VecToAngles(pev->velocity.Normalize());
		float flSpread = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerThrowBallForkSpread", 0.17);

		for (int iFork = 0; iFork < 2; iFork++)
		{
			CBaseEntity* pFork = CBaseEntity::Create("controller_throw_ball", pev->origin, g_vecZero, pev->owner);
			pFork->pev->dmg_inflictor = pev->dmg_inflictor;
			pFork->pev->weapons = pev->weapons - 1;
			if (iFork == 0)
				pFork->pev->dmg = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerThrowBallForkDamageFrac", 0.6) * pev->dmg;
			else
				pFork->pev->dmg = pev->dmg * 0.5; // VERIFY: the second fork uses a constant, not the pref
			pFork->pev->scale = pev->scale * 0.75;
			pFork->pev->armorvalue = gpGlobals->time + 0.2;

			UTIL_MakeAimVectors(vecAngles);

			float x, y, z;
			do
			{
				x = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
				y = RANDOM_FLOAT(-0.5, 0.5) + RANDOM_FLOAT(-0.5, 0.5);
				z = x * x + y * y;
			} while (z > 1);

			Vector vecDir = gpGlobals->v_forward + x * flSpread * gpGlobals->v_right + y * flSpread * gpGlobals->v_up;

			pFork->pev->speed = RANDOM_FLOAT(-100, 100) + pev->speed;
			pFork->pev->movedir = vecDir;
			pFork->pev->velocity = pFork->pev->movedir * pFork->pev->speed;
		}

		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/beamstart10.wav", 0.75, 1.0, 0, RANDOM_LONG(120, 130));
		return;
	}

	// home on the visible target closest to straight ahead
	CBaseEntity* pBest = NULL;
	float flBestDot = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerThrowBallMaxDot", 0.9);

	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityInSphere(pEnt, pev->origin, PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerThrowBallHommingRanage", 384))) != NULL)
	{
		if (pEnt->pev->takedamage != DAMAGE_AIM || pEnt->pev == pev || pEnt->edict() == pev->dmg_inflictor)
			continue;

		Vector vecTarget = pEnt->BodyTarget(pev->origin);
		Vector vecToTarget = (vecTarget - pev->origin).Normalize();
		float flDot = DotProduct(vecToTarget, pev->velocity.Normalize());
		if (flDot < flBestDot)
			continue;

		TraceResult tr;
		UTIL_TraceLine(pev->origin, vecTarget, ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction >= 1.0 || tr.pHit == pEnt->edict())
		{
			flBestDot = flDot;
			pBest = pEnt;
		}
	}

	if (pBest)
		pev->enemy = pBest->edict();
	else
		pev->enemy = NULL;
	pev->armorvalue = gpGlobals->time + 0.2;

	if (pev->enemy)
	{
		if (UTIL_IsValidEntity(pev->enemy) && CBaseEntity::Instance(pev->enemy) && CBaseEntity::Instance(pev->enemy)->IsAlive())
		{
			// aim halfway between the target and where the ball will be
			Vector vecTarget = CBaseEntity::Instance(pev->enemy)->BodyTarget(pev->origin);
			Vector vecAhead = pev->origin + pev->velocity * 0.25;
			Vector vecAim = vecTarget + (vecAhead - vecTarget) * 0.5;
			pev->velocity = (vecAim - pev->origin).Normalize() * pev->speed;
		}
		else
		{
			pev->enemy = NULL;
		}
	}

	pev->nextthink = gpGlobals->time + 0.1;
	pev->frame = ((int)pev->frame + 1) % 11;
}

// orig 0x152e10
void CDCThrowBall::TBTouch(CBaseEntity* pOther)
{
	TraceResult tr = UTIL_GetGlobalTrace();

	if (pOther->pev->takedamage != DAMAGE_NO)
	{
		entvars_t* pevAttacker = pev;
		if (pev->dmg_inflictor)
			pevAttacker = VARS(pev->dmg_inflictor);

		ClearMultiDamage();
		pOther->TraceAttack(pevAttacker, PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerThrowBallMaxImpactDamage", 50) * pev->dmg, pev->velocity.Normalize(), &tr, DMG_SHOCK);
		ApplyMultiDamage(pev, pevAttacker);
	}

	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "weapons/electro4.wav", 0.8, 0.9, 0, RANDOM_LONG(85, 95));

	if (pev->scale > 0.55)
		UTIL_DecalTrace(&tr, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));
	else
		UTIL_DecalTrace(&tr, DECAL_BIGSHOT1 + RANDOM_LONG(0, 4));

	pev->effects |= EF_NODRAW;
	pev->flags |= FL_KILLME;
}

//=========================================================
// CDCChargeBall: the small balls that orbit the controller while it charges.
// pev->weapons: set by the controller when it lets the ball go.
//=========================================================
class CDCChargeBall : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT CBThink();
	void EXPORT CBDieQuickThink();

	int m_iFrames;		// orig +0x27c, frame count of the sprite (set to 1). VERIFY name
	float m_fLastThink; // orig +0x280
};

LINK_ENTITY_TO_CLASS(controller_charge_ball, CDCChargeBall); // orig 0x152ff0

// orig 0x153020
void CDCChargeBall::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;
	SET_MODEL(ENT(pev), "sprites/muzzleflash3.spr");
	m_iFrames = 1;
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor = Vector(255, 224, 192);
	pev->renderamt = 0;
	pev->scale = 0.5;
	pev->weapons = 0;
	pev->avelocity.z = 1600;
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);
	SetThink(&CDCChargeBall::CBThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_fLastThink = gpGlobals->time;
}

// orig 0x153170
void CDCChargeBall::Precache()
{
	PRECACHE_MODEL("sprites/muzzleflash3.spr");
}

// Orbit 80 units above the owner. orig 0x1531a0
void CDCChargeBall::CBThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	float flInterval = gpGlobals->time - m_fLastThink;
	m_fLastThink = gpGlobals->time;
	pev->frame = ((int)pev->frame + 1) % m_iFrames;

	if (pev->weapons != 0)
	{
		pev->owner = NULL;
		SetThink(&CDCChargeBall::CBDieQuickThink);
		return;
	}

	if (!pev->owner || !UTIL_IsValidEntity(pev->owner) || !CBaseEntity::Instance(pev->owner) || !CBaseEntity::Instance(pev->owner)->IsAlive())
	{
		SetThink(&CDCChargeBall::CBDieQuickThink);
		return;
	}

	float flPull = flInterval * 1000;
	float flOrbit = flInterval * 800;

	Vector vecCenter = VARS(pev->owner)->origin + Vector(0, 0, 80);
	Vector vecToCenter = (vecCenter - pev->origin).Normalize();
	Vector vecFromNext = vecCenter - (pev->origin + pev->velocity);
	float flDist = vecFromNext.Length();

	Vector vecAngles = UTIL_VecToAngles(vecFromNext);
	Vector vecRight;
	g_engfuncs.pfnAngleVectors(vecAngles, NULL, vecRight, NULL);

	if (flDist - 8 <= 128)
	{
		if (flDist + 8 < 128)
			pev->velocity = pev->velocity - vecToCenter * flPull; // too close: push out
		else
			pev->velocity = pev->velocity + vecRight * flOrbit; // on the ring: go round
	}
	else
	{
		pev->velocity = pev->velocity + vecToCenter * flPull; // too far: pull in
	}

	float x = RANDOM_FLOAT(-10, 10);
	float y = RANDOM_FLOAT(-10, 10);
	float z = RANDOM_FLOAT(-10, 10);
	pev->velocity = pev->velocity + Vector(x, y, z);

	if (pev->velocity.Length() > 400)
		pev->velocity = pev->velocity.Normalize() * 400;

	if (pev->renderamt < 240)
		pev->renderamt = flInterval * 300 + pev->renderamt;
	if (pev->renderamt > 240)
		pev->renderamt = 240;
}

// Swell and fade out. orig 0x153740
void CDCChargeBall::CBDieQuickThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	float flInterval = gpGlobals->time - m_fLastThink;
	m_fLastThink = gpGlobals->time;
	pev->frame = ((int)pev->frame + 1) % m_iFrames;
	pev->renderamt = pev->renderamt - flInterval * 480;
	pev->scale = flInterval * 4 + pev->scale;

	if (pev->renderamt <= 0)
	{
		pev->renderamt = 0;
		pev->effects |= EF_NODRAW;
		SetThink(&CDCChargeBall::SUB_Remove);
	}
}

//=========================================================
// CDCChargeFire: the big charged ball. Lasers the 8 best targets ahead of it in flight and
// sheds health balls back to its driver as the lasers hit; blasts on impact.
// pev->weapons: 0 charging, 2 just fired, 1 in flight. pev->dmg: its charge.
// pev->dmg_inflictor: the driver. pev->air_finished: next health ball.
//=========================================================
class CDCChargeFire : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT CFThink();
	void EXPORT CFTtouch(CBaseEntity* pOther);

	int m_iFrames;		   // orig +0x27c
	float m_fLastThink;	   // orig +0x280
	float m_fReturnDamage; // orig +0x284, laser damage banked toward the next health ball. VERIFY name
	int m_iTeleSprite;	   // orig +0x288, sprites/b-tele1.spr (unused). VERIFY name
	int m_iFlareSprite;	   // orig +0x28c, sprites/flare6.spr
	int m_iTrailSprite;	   // orig +0x290, sprites/blueflare2.spr
	CBeam* m_pBeams[8];	   // orig +0x294
};

LINK_ENTITY_TO_CLASS(controller_charged_ball, CDCChargeFire); // orig 0x153850

// orig 0x153880
void CDCChargeFire::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_NOT;
	SET_MODEL(ENT(pev), "sprites/nhth1.spr");
	m_iFrames = 11;
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor = Vector(255, 255, 255);
	pev->renderamt = 50;
	pev->scale = 0.8;
	pev->avelocity.z = -900;
	pev->air_finished = gpGlobals->time;
	pev->weapons = 0;
	m_fReturnDamage = 0;
	UTIL_SetOrigin(pev, pev->origin);
	SetThink(&CDCChargeFire::CFThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_fLastThink = gpGlobals->time;

	for (int i = 0; i < 8; i++)
	{
		m_pBeams[i] = CBeam::BeamCreate("sprites/zbeam6.spr", 40);
		int iEnd = ENTINDEX(edict());
		m_pBeams[i]->EntsInit(ENTINDEX(edict()), iEnd);
		m_pBeams[i]->SetColor(128, 128, 255);
		m_pBeams[i]->SetBrightness(240);
		m_pBeams[i]->SetNoise(0);
		m_pBeams[i]->SetScrollRate(30);
		m_pBeams[i]->pev->effects |= EF_NODRAW;
	}
}

// orig 0x153aa0
void CDCChargeFire::Precache()
{
	PRECACHE_MODEL("sprites/nhth1.spr");
	PRECACHE_MODEL("sprites/zbeam6.spr");
	m_iTeleSprite = PRECACHE_MODEL("sprites/b-tele1.spr");
	m_iFlareSprite = PRECACHE_MODEL("sprites/flare6.spr");
	m_iTrailSprite = PRECACHE_MODEL("sprites/blueflare2.spr");
	PRECACHE_SOUND("debris/beamstart10.wav");
	PRECACHE_SOUND("debris/beamstart7.wav");
	PRECACHE_SOUND("x/x_teleattack1.wav");
}

// orig 0x153b60
void CDCChargeFire::CFThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	float flInterval = gpGlobals->time - m_fLastThink;
	m_fLastThink = gpGlobals->time;
	pev->frame = ((int)pev->frame + 1) % m_iFrames;

	if (!pev->owner || !UTIL_IsValidEntity(pev->owner) || !CBaseEntity::Instance(pev->owner) || !CBaseEntity::Instance(pev->owner)->IsAlive())
	{
		if (pev->weapons == 0)
		{
			// lost the controller before it was fired
			UTIL_Remove(this);
			pev->effects |= EF_NODRAW;
			for (int i = 0; i < 8; i++)
			{
				m_pBeams[i]->pev->effects |= EF_NODRAW;
				m_pBeams[i]->SetThink(&CBeam::SUB_Remove);
				m_pBeams[i]->pev->nextthink = gpGlobals->time + 0.01;
			}
			return;
		}

		pev->owner = edict();
	}

	if (!pev->dmg_inflictor || !UTIL_IsValidEntity(pev->dmg_inflictor))
		pev->dmg_inflictor = edict();

	if (pev->weapons == 0)
		return;

	if (pev->weapons == 2)
	{
		pev->solid = SOLID_BBOX;
		SetTouch(&CDCChargeFire::CFTtouch);
		m_fReturnDamage = 0;
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "x/x_teleattack1.wav", 1.0, 0.8, 0, 100);
		pev->weapons = 1;
	}

	pev->velocity = pev->movedir * pev->speed;

	// pick up to 8 targets ahead; when all slots are full a nearer one takes the first weaker slot
	float flSearchRad = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallLaserSearchRad", 1024);
	float flMinDot = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallLaserMinDot", -0.85);

	CBaseEntity* pTargets[8];
	float flWeights[8];
	TraceResult trTargets[8]; // written, never read: the damage below uses the last trace. VERIFY
	TraceResult tr;

	for (int i = 0; i < 8; i++)
	{
		pTargets[i] = NULL;
		flWeights[i] = -9999;
	}

	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityInSphere(pEnt, pev->origin, flSearchRad)) != NULL)
	{
		if (pEnt->IsBSPModel())
			continue;

		// VERIFY: DAMAGE_AIM things, or anything damageable that is DEAD_DYING
		if (pEnt->pev->takedamage != DAMAGE_AIM && (pEnt->pev->takedamage == DAMAGE_NO || pEnt->pev->deadflag != DEAD_DYING))
			continue;
		if (pEnt->edict() == pev->owner)
			continue;

		Vector vecTarget = pEnt->Center();
		Vector vecToTarget = (vecTarget - pev->origin).Normalize();
		if (DotProduct(vecToTarget, pev->velocity.Normalize()) < flMinDot)
			continue;

		float flWeight = 1.0 - (vecTarget - pev->origin).Length() / flSearchRad;

		int iSlot;
		for (iSlot = 0; iSlot < 8; iSlot++)
		{
			if (!pTargets[iSlot])
				break;
		}
		if (iSlot > 7)
		{
			for (iSlot = 0; iSlot < 8; iSlot++)
			{
				if (flWeights[iSlot] < flWeight)
					break;
			}
			if (iSlot > 7)
				continue;
		}

		UTIL_TraceLine(pev->origin, vecTarget, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction >= 1.0 || tr.pHit == pEnt->edict())
		{
			pTargets[iSlot] = pEnt;
			flWeights[iSlot] = flWeight;
			trTargets[iSlot] = tr;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		if (!pTargets[i])
		{
			m_pBeams[i]->SetEndEntity(ENTINDEX(edict()));
			m_pBeams[i]->pev->effects |= EF_NODRAW;
			continue;
		}

		m_pBeams[i]->SetEndEntity(ENTINDEX(pTargets[i]->edict()));
		m_pBeams[i]->pev->effects &= ~EF_NODRAW;

		// damage per second scales with the ball's charge
		float flMin = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallLaserDamageMinPerSec", 20) * flInterval;
		float flMax = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallLaserDamageMaxPerSec", 180) * flInterval;
		float flMaxCharge = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallMaxDamage", 320);
		float flDamage = (flMax - flMin) * (pev->dmg / flMaxCharge) + flMin;

		m_pBeams[i]->pev->renderamt = (int)(pev->renderamt * 0.8 + 48);

		ClearMultiDamage();
		Vector vecDir = (pev->origin - pTargets[i]->Center()).Normalize();
		pTargets[i]->TraceAttack(pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL, flDamage, vecDir, &tr, DMG_SHOCK);
		ApplyMultiDamage(pev, pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL);

		m_fReturnDamage = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallLaserDamageReturnFract", 0.5) * flDamage + m_fReturnDamage;
	}

	float flThreshold = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallLaserAccumDamageToLaunchRegenBall", 50);
	if (flThreshold <= m_fReturnDamage && pev->air_finished < gpGlobals->time)
	{
		m_fReturnDamage = m_fReturnDamage - flThreshold;

		CBaseEntity* pHealth = CBaseEntity::Create("controller_health_ball", pev->origin, g_vecZero, pev->owner);
		float x = RANDOM_FLOAT(-0.2, 0.2);
		float y = RANDOM_FLOAT(-0.2, 0.2);
		float z = RANDOM_FLOAT(-0.2, 0.2);
		Vector vecBack = (-pev->velocity + Vector(x, y, z)).Normalize();
		pHealth->pev->velocity = vecBack * RANDOM_FLOAT(300, 400);
		pHealth->pev->dmg = flThreshold;
		pHealth->pev->air_finished = pHealth->pev->dmg;

		pev->air_finished = gpGlobals->time + RANDOM_FLOAT(0.2, 0.3);
		EMIT_SOUND_DYN(pHealth->edict(), CHAN_WEAPON, "debris/beamstart7.wav", 1.0, 0.9, 0, RANDOM_LONG(120, 130));
	}
}

// orig 0x154930
void CDCChargeFire::CFTtouch(CBaseEntity* pOther)
{
	if (pOther->edict() == pev->owner)
		return;

	if (!pev->owner || !UTIL_IsValidEntity(pev->owner))
		pev->owner = edict();
	if (!pev->dmg_inflictor || !UTIL_IsValidEntity(pev->dmg_inflictor))
		pev->dmg_inflictor = edict();

	TraceResult tr;
	Vector vecEnd = pev->origin + pev->velocity.Normalize() * 64;
	Vector vecStart = pev->origin - pev->velocity.Normalize();
	UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, ENT(pev), &tr);

	EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "x/x_teleattack1.wav", 0, 0, SND_STOP, 100);
	EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "debris/beamstart10.wav", 1.0, 0.9, 0, RANDOM_LONG(70, 80));
	EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "debris/beamstart15.wav", 1.0, 0.9, 0, RANDOM_LONG(140, 150));

	Vector vecPos = pev->origin + tr.vecPlaneNormal * 4;

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecPos);
	WRITE_BYTE(TE_GLOWSPRITE);
	WRITE_COORD(vecPos.x);
	WRITE_COORD(vecPos.y);
	WRITE_COORD(vecPos.z);
	WRITE_SHORT(m_iFlareSprite);
	WRITE_BYTE(6);	 // life
	WRITE_BYTE(20);	 // scale
	WRITE_BYTE(200); // brightness
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecPos);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(vecPos.x);
	WRITE_COORD(vecPos.y);
	WRITE_COORD(vecPos.z);
	WRITE_COORD(tr.vecEndPos.x + tr.vecPlaneNormal.x);
	WRITE_COORD(tr.vecEndPos.y + tr.vecPlaneNormal.y);
	WRITE_COORD(tr.vecEndPos.z + tr.vecPlaneNormal.z);
	WRITE_SHORT(m_iTrailSprite);
	WRITE_BYTE(10); // count
	WRITE_BYTE(2);	// life
	WRITE_BYTE(3);	// scale
	WRITE_BYTE(18); // velocity
	WRITE_BYTE(23); // randomness
	MESSAGE_END();

	if (pev->scale >= 1.0)
		UTIL_DecalTrace(&tr, DECAL_SCORCH1 + RANDOM_LONG(0, 1));
	else
		UTIL_DecalTrace(&tr, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));

	// shock blast
	float flRadius = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallRadiusForDamage", 1.4) * pev->dmg;
	Vector vecCenter = pev->origin + tr.vecPlaneNormal * 8;

	CBaseEntity* pEnt = NULL;
	while ((pEnt = UTIL_FindEntityInSphere(pEnt, vecCenter, 2048)) != NULL)
	{
		if (pEnt->pev->takedamage == DAMAGE_NO && !pEnt->IsNetClient())
			continue;
		if (pEnt->edict() == pev->owner)
			continue;

		Vector vecTarget = pEnt->BodyTarget(vecCenter);
		float flDist = (vecTarget - vecCenter).Length();

		UTIL_TraceLine(vecCenter, vecTarget, dont_ignore_monsters, ENT(pev), &tr);
		if (tr.flFraction < 1.0 && tr.pHit != pEnt->edict())
			continue;

		if (pEnt->IsNetClient())
			UTIL_ScreenFade(pEnt, Vector(128, 128, 255), 0.5, 0.1, 210, 0);

		if (flDist <= flRadius && pEnt->pev->takedamage != DAMAGE_NO)
		{
			float flDmg = pev->dmg;
			ClearMultiDamage();
			Vector vecDir = (vecTarget - vecCenter).Normalize();
			pEnt->TraceAttack(pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL, (1.0 - flDist / flRadius) * flDmg, vecDir, &tr, DMG_SHOCK);
			ApplyMultiDamage(pev, pev->dmg_inflictor ? VARS(pev->dmg_inflictor) : NULL);
		}
	}

	for (int i = 0; i < 8; i++)
	{
		m_pBeams[i]->pev->effects |= EF_NODRAW;
		m_pBeams[i]->SetThink(&CBeam::SUB_Remove);
		m_pBeams[i]->pev->nextthink = gpGlobals->time + 0.01;
	}

	SetTouch(NULL);
	pev->effects |= EF_NODRAW;
	UTIL_Remove(this);
}

//=========================================================
// CDCHealthBall: shed by the charged ball; flies home to its owner and heals it through a beam.
// pev->dmg: health left to give. pev->air_finished: health it started with.
//=========================================================
class CDCHealthBall : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	void EXPORT HBThink();
	void EXPORT HBDieQuickThink();

	int m_iFrames;		// orig +0x27c
	float m_fLastThink; // orig +0x280
	CBeam* m_pBeam;		// orig +0x284, ball to owner
};

LINK_ENTITY_TO_CLASS(controller_health_ball, CDCHealthBall); // orig 0x155210

// orig 0x155240
void CDCHealthBall::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;
	SET_MODEL(ENT(pev), "sprites/muz7.spr");
	m_iFrames = 1;
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor = Vector(255, 64, 255);
	pev->renderamt = 240;
	pev->scale = 0.25;
	pev->weapons = 0;
	pev->avelocity.z = 3000;
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin(pev, pev->origin);
	SetThink(&CDCHealthBall::HBThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->armorvalue = gpGlobals->time;
	m_fLastThink = gpGlobals->time;

	m_pBeam = CBeam::BeamCreate("sprites/plasma.spr", 40);
	int iOwner = ENTINDEX(pev->owner);
	m_pBeam->EntsInit(ENTINDEX(edict()), iOwner);
	m_pBeam->SetEndAttachment(1);
	m_pBeam->SetColor(255, 30, 230);
	m_pBeam->SetBrightness(0);
	m_pBeam->SetNoise(40);
	m_pBeam->SetScrollRate(30);
	m_pBeam->pev->effects |= EF_NODRAW;
}

// orig 0x155490
void CDCHealthBall::Precache()
{
	PRECACHE_MODEL("sprites/muz7.spr");
	PRECACHE_MODEL("sprites/plasma.spr");
}

// orig 0x1554d0
void CDCHealthBall::HBThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	float flInterval = gpGlobals->time - m_fLastThink;
	m_fLastThink = gpGlobals->time;
	pev->frame = ((int)pev->frame + 1) % m_iFrames;

	if (!pev->owner || !UTIL_IsValidEntity(pev->owner) || !CBaseEntity::Instance(pev->owner) || !CBaseEntity::Instance(pev->owner)->IsAlive())
	{
		SetThink(&CDCHealthBall::HBDieQuickThink);
		return;
	}

	float flPull = flInterval * 1000;
	float flOrbit = flInterval * 800;
	float flSwirl = flInterval * 700;
	float flPhase = (gpGlobals->time - pev->armorvalue) * 1.7;
	float flCos = cos(flPhase);
	float flSin = sin(flPhase);

	Vector vecCenter = VARS(pev->owner)->origin + Vector(0, 0, 80);
	Vector vecToCenter = (vecCenter - pev->origin).Normalize();
	float flCenterDist = (vecCenter - pev->origin).Length();
	Vector vecFromNext = vecCenter - (pev->origin + pev->velocity);
	float flDist = vecFromNext.Length();

	Vector vecAngles = UTIL_VecToAngles(vecFromNext);
	Vector vecRight, vecUp;
	g_engfuncs.pfnAngleVectors(vecAngles, NULL, vecRight, vecUp);

	if (flDist - 8 <= 92)
	{
		if (flDist + 8 < 92)
			pev->velocity = pev->velocity - vecToCenter * flPull;
		else
			pev->velocity = pev->velocity + vecRight * flOrbit;
	}
	else
	{
		pev->velocity = pev->velocity + vecToCenter * flPull;
	}

	if (flDist - 32 > 92)
		pev->velocity = pev->velocity + vecRight * (flSwirl * flCos) + vecUp * (flSwirl * flSin);

	float x = RANDOM_FLOAT(-10, 10);
	float y = RANDOM_FLOAT(-10, 10);
	float z = RANDOM_FLOAT(-10, 10);
	pev->velocity = pev->velocity + Vector(x, y, z);

	if (pev->velocity.Length() > 400)
		pev->velocity = pev->velocity.Normalize() * 400;

	// heal through the beam when close
	float flMaxDist = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallRechargerMaxDist", 160);
	float flHeal = PREFS_KeyLoopUpFloat("DRIVECONTROLLER", "ControllerChargedBallRechargerHealthePerSec", 25) * flInterval;
	float flBeam = (float)(int)m_pBeam->pev->renderamt;
	if (!(flHeal < pev->dmg))
		flHeal = pev->dmg;

	float flNewBeam;
	if (flCenterDist <= flMaxDist && pev->dmg > 0)
	{
		flNewBeam = UTIL_Approach(pev->renderamt, flBeam, (pev->renderamt / 0.8) * flInterval);
		CBaseEntity::Instance(pev->owner)->TakeHealth(flHeal, DMG_GENERIC);
		pev->dmg = pev->dmg - flHeal;
		pev->renderamt = pev->renderamt - (flHeal / pev->air_finished) * 200;
		if (pev->renderamt < 10)
			pev->renderamt = 10;
	}
	else
	{
		flNewBeam = UTIL_Approach(0, flBeam, flInterval * 799.99994);
	}

	m_pBeam->pev->renderamt = (int)flNewBeam;
	if (flNewBeam > 0)
		m_pBeam->pev->effects &= ~EF_NODRAW;
	else
		m_pBeam->pev->effects |= EF_NODRAW;

	if (pev->dmg > 0)
		return;

	// spent: swell and fade
	pev->renderamt = pev->renderamt - flInterval * 480;
	pev->scale = flInterval * 4 + pev->scale;
	if (pev->renderamt > 0)
		return;

	pev->renderamt = 0;
	pev->effects |= EF_NODRAW;
	if (m_pBeam)
	{
		m_pBeam->pev->effects |= EF_NODRAW;
		m_pBeam->SetThink(&CBeam::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 0.01;
		m_pBeam = NULL;
	}
	SetThink(&CDCHealthBall::SUB_Remove);
}

// orig 0x155f50
void CDCHealthBall::HBDieQuickThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	float flInterval = gpGlobals->time - m_fLastThink;
	m_fLastThink = gpGlobals->time;

	if (m_pBeam)
	{
		m_pBeam->pev->effects |= EF_NODRAW;
		m_pBeam->SetThink(&CBeam::SUB_Remove);
		m_pBeam->pev->nextthink = gpGlobals->time + 0.01;
		m_pBeam = NULL;
	}

	pev->frame = ((int)pev->frame + 1) % m_iFrames;
	pev->renderamt = pev->renderamt - flInterval * 480;
	pev->scale = flInterval * 4 + pev->scale;

	if (pev->renderamt <= 0)
	{
		pev->renderamt = 0;
		pev->effects |= EF_NODRAW;
		SetThink(&CDCHealthBall::SUB_Remove);
	}
}

