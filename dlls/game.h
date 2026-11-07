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

#pragma once

extern void GameDLLInit( void );
void GameDLLShutdown();


extern cvar_t	displaysoundlist;

// multiplayer server rules
extern cvar_t fragsleft;
extern cvar_t timeleft;
extern cvar_t teamplay;
extern cvar_t fraglimit;
extern cvar_t timelimit;
extern cvar_t friendlyfire;
extern cvar_t falldamage;
extern cvar_t weaponstay;
extern cvar_t forcerespawn;
extern cvar_t flashlight;
extern cvar_t aimcrosshair;
extern cvar_t decalfrequency;
extern cvar_t teamlist;
extern cvar_t teamoverride;
extern cvar_t defaultteam;
extern cvar_t allowmonsters;
extern cvar_t allow_spectators;
extern cvar_t mp_chattime;

extern cvar_t sv_allowbunnyhopping;

extern cvar_t sv_busters;

// Rocket Crowbar 1.9
extern cvar_t rockbarversion;
extern cvar_t prefsfile;
extern cvar_t allowrifts;
extern cvar_t allowiso;
extern cvar_t allownuke;
extern cvar_t ctf;
extern cvar_t ctfreturntime;
extern cvar_t ctfppc;
extern cvar_t curses;
extern cvar_t tannoy;
extern cvar_t tvelo;
extern cvar_t monsterspawn;
extern cvar_t chlevelwks;
extern cvar_t noexit;
extern cvar_t allowgive;
extern cvar_t ceasefire;
extern cvar_t scikillfest;
extern cvar_t deathballs;
extern cvar_t deathballsonly;
extern cvar_t pregameobserver;
extern cvar_t ingameobserver;
extern cvar_t kickfract;
extern cvar_t kickbantime;
extern cvar_t mapfract;
extern cvar_t votefreq;
extern cvar_t pcst;
extern cvar_t pcall;
extern cvar_t loyalnpcs;
extern cvar_t ignplayer;
extern cvar_t serverclient;
extern cvar_t mapcfg;
extern cvar_t lowlag;
extern cvar_t drvapache;
extern cvar_t drvgarg;
extern cvar_t drvassassin;
extern cvar_t drvicky;
extern cvar_t drvpanth;
extern cvar_t drvhe;
extern cvar_t drvslave;
extern cvar_t drvturret;
extern cvar_t drvbm;
extern cvar_t drvagrunt;
extern cvar_t drvcontroll;
extern cvar_t drvsquid;
extern cvar_t transform;
extern cvar_t globaleject;

// Engine Cvars
inline cvar_t* g_psv_gravity;
inline cvar_t* g_psv_aim;
inline cvar_t* g_psv_allow_autoaim;
inline cvar_t* g_footsteps;
inline cvar_t* g_psv_cheats;
