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
//
// teamplay_gamerules.h
//

#pragma once

#define MAX_TEAMNAME_LENGTH 16
#define MAX_TEAMS 32

#define TEAMPLAY_TEAMLISTLENGTH MAX_TEAMS* MAX_TEAMNAME_LENGTH

class CHalfLifeTeamplay : public CHalfLifeMultiplay
{
public:
	CHalfLifeTeamplay();

	bool ClientCommand(CBasePlayer* pPlayer, const char* pcmd) override;
	void ClientUserInfoChanged(CBasePlayer* pPlayer, char* infobuffer) override;
	bool IsTeamplay() override;
	bool FPlayerCanTakeDamage(CBasePlayer* pPlayer, CBaseEntity* pAttacker) override;
	int PlayerRelationship(CBaseEntity* pPlayer, CBaseEntity* pTarget) override;
	const char* GetTeamID(CBaseEntity* pEntity) override;
	bool ShouldAutoAim(CBasePlayer* pPlayer, edict_t* target) override;
	int IPointsForKill(CBasePlayer* pAttacker, CBasePlayer* pKilled) override;
	void InitHUD(CBasePlayer* pl) override;
	void DeathNotice(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pevInflictor) override;
	const char* GetGameDescription() override; // this is the game name that gets seen in the server browser
	void UpdateGameMode(CBasePlayer* pPlayer) override;					// the client needs to be informed of the current game mode
	void PlayerKilled(CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor) override;
	void Think() override;
	int GetTeamIndex(const char* pTeamName) override;
	const char* GetIndexedTeamName(int teamIndex) override;
	bool IsValidTeam(const char* pTeamName) override;
	const char* SetDefaultPlayerTeam(CBasePlayer* pPlayer) override;
	void ChangePlayerTeam(CBasePlayer* pPlayer, const char* pTeamName, bool bKill, bool bGib) override;

	// Rocket Crowbar 1.9: where the CTF flags start (flag n at index n-1)
	void SetFlagSpawns(int iFlags);

private:
	void RecountTeams();
	const char* TeamWithFewestPlayers();

	bool m_DisableDeathMessages;
	bool m_DisableDeathPenalty;
	bool m_teamLimit; // This means the server set only some teams as valid
	char m_szTeamList[TEAMPLAY_TEAMLISTLENGTH];

	// Rocket Crowbar 1.9 (unsaved; the game rules aren't saved)
	// orig +0x218: flag n's base origin at [n-1], its angles at [6 + n-1] (+0x260). One array, so
	// SetFlagSpawns' write one past the origins lands on flag 1's angles as in the original; the
	// spare last slot takes its write one past the angles, which ran off the original's object.
	Vector m_vecFlagSpawns[13]; // VERIFY name
	Vector* m_vecFlagOrigin = &m_vecFlagSpawns[0];
	Vector* m_vecFlagAngles = &m_vecFlagSpawns[6];
};

// Rocket Crowbar 1.9 CTF state (file-scope globals in the original, despite the m_ names)
extern int m_blCTF;		  // orig 0x31a520, 1 while CTF is on
extern float m_flInitCTF; // orig 0x31a524, when Think places the flags (0 once done)
extern int num_flags;	  // orig 0x2a5924, one flag a team

const char* GetTeamNameForIndex(int iTeam); // 1-based
void AddTeamScore(int iTeam, int iScore);	// 1-based, CTF only
void UpdateTeamScores();
void SendCTFInitMSG();
