/***
*
*	Rocket Crowbar 1.9: pregame.cpp, the observer ("SPECTATOR mode") a player is put in before
*	playing, and the version banner.
*
*	InitObserver turns the player into a noclip, invisible "spectator"; from then on PreThink
*	runs ObserverFrame instead of the normal think while CBasePlayer::m_blIsObserver is set.
*	Jump leaves observer mode (PreThink then respawns the player), attack cycles free roam /
*	chase with free look / chase with the target's view, attack2 picks the next target (or a
*	random spawn point when roaming), reload cycles how the target is drawn, and forward/back
*	move the chase camera in and out.
*
****/

#pragma once

class CBaseEntity;
class CBasePlayer;

void PrintRocketCrowbarMsg(CBaseEntity* pPlayer);
void PrintObserverMessage(CBaseEntity* pPlayer);
void PrintModeMessage(CBasePlayer* pPlayer);
CBaseEntity* GetRandomObserverSpawn();
void MoveObserverToRandomSpot(CBaseEntity* pPlayer);
void InitObserver(CBasePlayer* pPlayer);
CBaseEntity* GetNextObserverEnt(CBaseEntity* pCurrent, CBasePlayer* pPlayer);
void ObserverChaseCamThink(CBasePlayer* pPlayer);
void ObserverFrame(CBasePlayer* pPlayer);
