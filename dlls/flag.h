/***
*
*	Rocket Crowbar 1.9: flag.cpp's CTF flag, for the team game rules that place and drop it.
*
****/

#pragma once

#include "basemonster.h"

class CBasePlayer;

class CBaseFlag : public CBaseMonster
{
public:
	void Spawn() override;
	void Precache() override;
	static CBaseFlag* CreateFlag(Vector vecOrigin, Vector vecAngles, int iTeam);
	void EXPORT FlagTouch(CBaseEntity* pOther);
	void EXPORT FlagThink();
	void FlagPickUp(CBaseEntity* pCarrier);
	void FlagDrop(entvars_t* pevCarrier);
	void FlagReturn();

	int m_iAtBase;			// orig +0x27c, 1 while at its base. VERIFY name
	int m_iCarried;			// orig +0x280, 1 while a player carries it (pev->owner). VERIFY name
	Vector m_vecBaseOrigin; // orig +0x284. VERIFY name
	Vector m_vecBaseAngles; // orig +0x290. VERIFY name
	int m_iTeam;			// orig +0x2a0, 1-based team index. VERIFY name
	float m_flNextMsgTime;	// orig +0x2a4, the "more than 1 flag" warning. VERIFY name
};

CBaseFlag* DoesEntHaveFlag(CBaseEntity* pEntity); // the flag pEntity carries, or NULL
char* GetFlagStatusString(CBaseFlag* pFlag, CBasePlayer* pPlayer);
char* GetFlagNameString(CBaseFlag* pFlag, CBasePlayer* pPlayer);
char* ReportFlagStatus(CBasePlayer* pPlayer); // says every flag's status to pPlayer
const char* GetFlagTeamColor(int iTeam);
Vector GetFlagBaseColor(int iTeam);
