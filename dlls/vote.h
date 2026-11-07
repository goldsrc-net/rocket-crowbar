/***
*
*	Rocket Crowbar 1.9: vote_kick.cpp and vote_map.cpp, the kick and map votes.
*
*	A vote is started from a console command (VK_InitVote / VM_InitVote return the text to show
*	the caller), runs from StartFrame (VK_VoteFrame / VM_VoteFrame) for 45 seconds, and is
*	decided as votes come in (VK_RegisterVote / VM_RegisterVote, from the vkyes/vkno and
*	vmyes/vmno commands that keys 1 and 2 are bound to while a player may vote). Only one of the
*	two can run at a time.
*
****/

#pragma once

class CBaseEntity;

#define VOTE_MAX_VOTERS 32 // g_pVKVoters, g_pVMVoters: 0x100 bytes of EHANDLE each

// vote_kick.cpp
extern bool g_blInVoteKick;
extern float g_flVKFractForKick;
extern float g_flVKEndTime;
extern float g_flVKUpdateTime;
extern int g_iVKFor;
extern int g_iVKAgainst;
extern int g_iVKTotal;
extern int g_iVKMSGColor[4];
extern EHANDLE g_pVKBoot;
extern EHANDLE g_pVKInit;
extern EHANDLE g_pVKVoters[VOTE_MAX_VOTERS];

const char* VK_InitVote(CBaseEntity* pInit, int iIndex);
void VK_VoteDissconnect(edict_t* pEntity);
void VK_ValidatePTRS();
void VK_VoteFail();
bool VK_IsValidIp(char* szIp);
void VK_VoteSucceede();
void VK_VKKeysBind(edict_t* pEntity, bool bReset);
void VK_SendVKMsg(edict_t* pEntity, bool bVoter);
void VK_VoteFrame();
void VK_RegisterVote(CBaseEntity* pVoter, bool bFor);

// vote_map.cpp
extern bool g_blInVoteMap;
extern float g_flFractForMap;
extern float g_flVMEndTime;
extern float g_flVMChangeTime;
extern float g_flVMUpdateTime;
extern int g_iVMFor;
extern int g_iVMAgainst;
extern int g_iVMTotal;
extern int g_iVMMSGColor[4];
extern char g_szMapName[65];
extern EHANDLE g_pVMInit;
extern EHANDLE g_pVMVoters[VOTE_MAX_VOTERS];

const char* VM_InitVote(CBaseEntity* pInit, const char* szMap);
void VM_ValidatePTRS();
void VM_VoteFail();
void VM_VoteSucceede();
void VM_KeysBind(edict_t* pEntity, bool bReset);
void VM_SendVMMsg(edict_t* pEntity, bool bVoter);
void VM_VoteFrame();
void VM_RegisterVote(CBaseEntity* pVoter, bool bFor);
