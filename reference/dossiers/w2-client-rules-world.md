# w2-client-rules-world

Wave 2: Rocket Crowbar's changes inside client.cpp, the game rules (gamerules, multiplay,
teamplay with CTF), world.cpp, effects.cpp and the touch/trigger files.

## 1. Scope

Original files: client.cpp, multiplay_gamerules.cpp, teamplay_gamerules.cpp, gamerules.cpp,
world.cpp, effects.cpp, maprules.cpp, triggers.cpp, func_break.cpp, buttons.cpp, doors.cpp,
plats.cpp, game.cpp.

Edited: those files, plus `client.h`, `gamerules.h` (SendMOTDToClient public, GetNextBestWeapon on
CHalfLifeMultiplay), `teamplay_gamerules.h` (CTF members and functions), `effects.h` (CBeam),
`cbase.h`/`cbase.cpp` (ResetPrecachedList, coordinator's request), `flag.cpp` and `vote_kick.cpp`
(TEMP STUBs removed), top-level `CMakeLists.txt` (CLIENT_WEAPONS client-only, coordinator's
request), `scripts/check_file.py` (see 2).

## 2. Status

Built, not run in an engine. i386 and amd64 build (from clean, after the CMakeLists change).

`check_file.py --missing`: 0 missing in all 13 files (client 61/61, multiplay 52/52, teamplay
26/26, gamerules 23/23, world 33/33, effects 186/186, maprules 75/75, triggers 133/133,
func_break 33/33, buttons 65/65, doors 29/29, plats 111/111, game 1/1).

check_file.py change: its inline-body test now also reads the file itself, not only headers. The
SDK declares many classes in their .cpp (CGameCounter, CShake, CBlood, CMultiManager...); their
in-class accessors (gcc 2.95 emitted them out of line) were reported missing though present. That
cleared all of maprules' 28, effects' 16 and the others. plats' `Fix`/`FixupAngles` lost their
`static` (the original's are global) so they get symbols.

Reviewed in full (every function against the decompile, disassembly where Ghidra split or lost
code): client.cpp, multiplay_gamerules.cpp, teamplay_gamerules.cpp, gamerules.cpp, world.cpp,
game.cpp (GameDLLInit: the original only registers cvars and runs skill.cfg; ours keeps
halflife-updated's SV_InitServer, cvar pointers and map-loading commands, behavior otherwise the
same). Compared: rc_touch functions and RC additions in effects, triggers, func_break, buttons,
doors, plats. Skimmed only: the rest of effects, maprules, triggers, func_break, buttons, doors,
plats (stock-looking decompiles; no RC names). CLightning::KeyValue and CBreakable::Die were in
rc_touch only because of inline names; both match.

## 3. Function map

All RC additions (orig addresses in each function's comment):

| orig | function | note |
|---|---|---|
| 0xfdbf0 | SayPrivate | ok (std::string build, see 8) |
| 0xfe080 | IsClientListenServer | ok, replaces vote_kick.cpp's stub |
| 0xfe130 / 0xfe350 | IsClientServer / IsClientAllowedToUse | ok, share IsClientInIndexList (sv_serverclient / sv_kersplatis) |
| 0xfe570 | LogClientServerCommand | ok |
| 0xfe690 | LCSFindEntityForward | ok |
| 0xfe8b0-0xfed60 | EManip* (8) | ok; GetTempEnt, PrintBits, SetBits, SCGetEqualTok* have no caller |
| 0xfedb0 | SendTextAsMOTD | ok, no caller |
| 0xfeec0 | CheckItemStrEqu | ok |
| 0xfef10 / 0xff0a0 | CPopOMatic::CreatePOM / POMThink | VERIFY HasHumanGibs cast; nothing creates one |
| 0x103f50 | GetEntHoundeyeVel | ok, no caller |
| 0x1cec40 | CHalfLifeMultiplay::GetNextBestWeapon | ok, 2001 SDK body |
| 0x221dd0 | CHalfLifeTeamplay::SetFlagSpawns | VERIFY guard (8) |
| 0x222180 | GetTeamNameForIndex | guarded (8), replaces flag.cpp stub |
| 0x2221c0 / 0x222290 / 0x222370 | AddTeamScore / UpdateTeamScores / SendCTFInitMSG | ok, AddTeamScore replaces flag.cpp stub |
| 0x23e6b0-0x23ec20 | fading_corpse, CFaderCorpse (Spawn, ObjectCaps, CopyFromEnt, WaitToLand, MyFadeOut), CreateFadingBodyCopy, InitBodyQue | VERIFY SET_MODEL, MyFadeOut drop; CreateFadingBodyCopy has no caller |
| 0x17a660 / 0x181400 | CBeam::EntIsBeingRemoved / StartFade | ok |
| 0x1800f0 / 0x180120 | env_fadeone, CFadeOne::Use | guarded (8) |

Stock functions changed to the original (as the package left them; halflife-updated's fixes in
them went back after Steven's rule of 2026-11-07, listed in mod-rc2's `reference/audit/FIXES.md`
and in `reference/DEVIATIONS.md`, which is current):

- **client.cpp**: ClientDisconnect (flag drop + message, DispatchDissconnect, VK_VoteDissconnect,
  resets cl_forward/back/sidespeed; no m_pTank release), ClientPutInServer (clears
  m_fGameHUDInitialized; no iuser1/2), Host_Say (2001: no flood timer, no SPEC, no voice mute, no
  log; 192-byte text), ClientCommand (whole RC command set, read from the disassembly), 
  ClientUserInfoChanged (extragibs, driveentalpha, RC log format, no name % filter),
  StartFrame (votes, g_flLastTime, g_iSkillLevel = skill each frame), ClientPrecache,
  GetGameDescription ("Rocket Crowbar"), SetupVisibility (NULL pvs/pas), AddToFullPack (RC's iuser3
  draw instructions, 2001 SLERP rule, no m_EFlags/flesh sound), GetWeaponData (zeros only),
  UpdateClientData (no spectator/weapon data, pev->fov, noclip not ducking), AllowLagCompensation (0).
- **multiplay**: constructor (no config exec), voice helper (everyone hears everyone), Think
  (6 s intermission + 120 s max, no mp_chattime), FShouldSwitchWeapon (no cl_autowepswitch),
  InitHUD (RC log, ServerName "Rocket Crowbar v19"), ClientDisconnected (RC log), FlPlayerFallDamage
  (0), PlayerSpawn (rc.ini starting weapons and ammo), PlayerKilled (no func_vehicle credit),
  DeathNotice (RC names/logs/dedicated echo, no HLTV director msg), FlHealth/HEVChargerRechargeTime
  (rc.ini), GoToIntermission, ChangeLevel (2001 map cycle + sv_mapcfg), SendMOTDToClient (motd.txt,
  64/1024, no hostname).
- **teamplay**: constructor/Think (CTF), ClientCommand (flagstatus), InitHUD (no TeamNames msg, CTF
  colour, scores), ChangePlayerTeam (no ScoreInfo, world pev), ClientUserInfoChanged (CTF colour, RC
  log), RecountTeams (num_flags, CTF scores kept, no resend), GetGameDescription.
- **gamerules**: InstallGameRules (no game.cfg, SendCTFInitMSG before teamplay).
- **world**: CopyToBodyQue (frame 255, no modelindex test), CWorld::Spawn (g_flWeaponCheat,
  g_iCntFakeClassnames), CWorld::Precache (resets, configs once, sv_mapcfg, rc.ini, RC precaches,
  gib model indices, all 16 spawner/LC calls).
- **triggers**: HurtTouch, MultiTouch, TouchChangeLevel, UseChangeLevel (IsDriveent, sv_noexit).
  **buttons/doors/plats**: ButtonTouch, DoorTouch, CPlatTrigger::Touch (IsDriveent).

## 4. Classes

- CPopOMatic : CBaseEntity (client.cpp), no members; pev fields: armorvalue = last think, dmg =
  0.25 s, dmgtime = end, renderfx/rendercolor = victim's saved look, owner, enemy = victim.
- CFaderCorpse : CBaseMonster (world.cpp), no members; dmg/dmgtime timers, dmg_take = landed z,
  dmg_save = sink flag.
- CFadeOne : CFade (effects.cpp), Use only.
- CHalfLifeTeamplay: `m_vecFlagSpawns[13]` (orig +0x218 origins, +0x260 angles; one array so the
  original's off-by-one write lands where it did, plus a spare slot), with pointers
  m_vecFlagOrigin/m_vecFlagAngles. VERIFY name. File globals m_blCTF, m_flInitCTF, num_flags
  (named as in the original's symbols).
- sEManipEnum {szName, iValue} (client.h), g_esUseTypes = off/on/set/toggle.

## 5. Cross-file needs

TEMP STUBs left (drop at merge, main has the real ones per the coordinator):

| what | where | real |
|---|---|---|
| `int g_iPlayerShockwave` | bottom of client.cpp | main's player.cpp (the original's sits in client.cpp's data, 0x30b280) |
| `bool g_bUnsafeCreate` | bottom of client.cpp | main's cbase.cpp (cbase.h) |
| `ThrowFakeGibs(entvars_t*, bool)` (empty) | bottom of client.cpp | main's combat.cpp (cbase.h) |
| `int g_iCntFakeClassnames` | bottom of world.cpp | main's cbase.cpp (cbase.h) |
| `CreateSciSpawner()` (empty) | bottom of world.cpp | scientist.cpp (w2-monsters, orig 0x206460) |

Real definitions here that others extern: `g_flWeaponCheat` (world.cpp; player.cpp's
CheatImpulseCommands reads it), `short g_iHGibModel, g_iAGibModel` (world.cpp owns them now: drop
player-core's TEMP STUB in combat.cpp), `g_iEMCountUsedPCs/SCStrs` (client.h), IsClientListenServer
and the other client.cpp functions (client.h), GetTeamNameForIndex/AddTeamScore/UpdateTeamScores/
SendCTFInitMSG and m_blCTF (teamplay_gamerules.h).

Stubs removed: IsClientListenServer (vote_kick.cpp), GetTeamNameForIndex and AddTeamScore
(flag.cpp). Externs declared locally: CreateDrvIckySpawner, CreateDrvSlaveSpawner,
CreateDrvControllerSpawner (no headers), g_bUnsafeCreate, g_iCntFakeClassnames, ThrowFakeGibs,
GetTeamNameForIndex (client.cpp).

cbase: `ResetPrecachedList()` added to cbase.cpp, declared in cbase.h next to AddToPrecachedList;
CWorld::Precache calls it before W_Precache (the original sets g_iCntPrecached = 0 directly, an
exported global there).

SetupVisibility now takes NULL pvs/pas. The drive monsters' DEClientLeave still pass dummy
pointers (their files, not changed here); they can pass NULL like the original.

Shared-file edits: CMakeLists.txt (CLIENT_WEAPONS only for the client target), gamerules.h,
cbase.h/cbase.cpp, check_file.py.

Not done, for the coordinator: `sv_allowbunnyhopping` defaults to 0 and halflife-updated's
pm_shared caps bunny hopping unless physinfo "bj" is 1; the original's pm_shared has no cap, so
at defaults ours differs (StartFrame sends "bj" from that cvar; pm_shared isn't in this package).

## 6. VERIFY list (22 in this package's files)

- Guards (the original crashes or overruns): GetTeamNameForIndex, SetFlagSpawns' pBest, CFadeOne
  NULL activator, UseChangeLevel NULL caller, SayPrivate buffer, EManipMakeSCStrConst strcpy, LCS
  unfilled vector (prints zeros). Settled by design; listed in 8.
- client.cpp: spectate's `iuser4 = 1` (what player.cpp's Killed does with it), EManipGetTempEnt's
  classname pointing at the caller's buffer, CPopOMatic HasHumanGibs cast, useent's world
  activator, showmaps' unfreed 1-byte file and the `cBuf[0] <= 12` test, lcputent's coordinates
  with other arg counts (zeros). UpdateClientData's `cd->fov` is `m_iFOV` now (halflife-updated's,
  Steven 2026-11-07).
- multiplay: InitHUD/PlayerKilled ScoreInfo kept at halflife-updated's 5 fields (the original
  sends 3; a current client reads 5: decide), ChangeLevel's 33-byte buffers.
- world: CFaderCorpse SET_MODEL with its own empty model, MyFadeOut's 24-unit drop. The server
  config now runs from CWorld::Precache, once, as the original (f8627d1, found in the first
  engine run).
- teamplay.h: m_vecFlagSpawns name.

## 7. Behavior worth testing

- Admin commands with sv_serverclient set: give/giveitem/givefor (with and without fcheats), lc*
  writing maps/<map>.lec, remall/rement/useent/setpa*, setpref/saveprefs/loadprefs, sndcmd, wog.
- spectate/observe, eject, nuke (RefireTime), cl_gibcount, cl_de_alpha (userinfo keys survive a
  reconnect), say_private by name and index, votekick/votemap.
- CTF: mp_teamplay 1, mp_ctf 1, 2-6 teams; flags at info_flagspawn or spread over deathmatch
  spawns; flagstatus; team score messages; GetTeamNameForIndex no longer crashing.
- Observer/drive views (AddToFullPack iuser3 bits 4 and 8, glow shells).
- First weapon prediction: none on a current client (GetWeaponData zero, m_iId 0), no lag comp.
- Map change: 6 s intermission, mapcycle order, sv_mapcfg exec, game.cfg once.
- No fall damage in multiplayer; rc.ini starting weapons/ammo; charger recharge times.
- sv_noexit 1 gibs players at trigger_changelevel; driven monsters work buttons, doors, plats,
  trigger_multiple, trigger_hurt flags.

## 8. Original bugs

- **GetTeamNameForIndex writes into a "" string literal** (read-only; crashes every CTF message on
  Linux). Guarded: static buffer.
- **SetFlagSpawns reads through a stale or NULL best spawn** when no unused deathmatch spawn is
  left. Guarded: stops placing.
- **SetFlagSpawns takes info_flagspawn button iFlags+1**: one past the flag origins (lands on flag
  1's angles) and, with 6 teams, one past the object for the angles. Kept the first (array layout),
  the second goes to a spare slot.
- **CFadeOne::Use and CChangeLevel::UseChangeLevel call through a NULL activator/caller**. Guarded.
- **UseChangeLevel's sv_noexit damages the caller, not the activator.** Kept.
- **SayPrivate strncat's up to 192 chars per word into 256 bytes** (stack overrun with long text).
  Guarded; same 191-char result.
- **EManipMakeSCStrConst strcpy's unchecked into 64-byte slots.** Guarded (cut at 63).
- **LCSFindEntityForward leaves the end vector unset on the sphere path; lcrement/lckillent/
  lcuseent print it.** Guarded (zeros). lckillent and lcuseent both say "Using".
- **showmaps' " %64s" into 64 bytes** writes its terminator into the output buffer's first byte.
  Kept (same layout). A file of one byte is never freed. Kept.
- **The "cl_gibcount" range error says "cl_extragibs"**. Kept.
- **"wog" with a wrong password prints "Unknown command: wog"** (skips the game rules). Kept.
- **CFaderCorpse::CopyFromEnt SET_MODEL's its own empty model** before copying the body's. Kept.
- **CBeam::EntIsBeingRemoved may UTIL_Remove twice.** Kept (harmless).
- **ParmsChangeLevel's `if (g_fGameOver) g_fGameOver = 1`** no-op. Kept as a comment.
- **GetHullBounds never writes mins/maxs** (2001 SDK). Not kept: halflife-updated's writes the
  standard sizes the engine defaults to.
- **InstallGameRules calls SendCTFInitMSG with the previous map's num_teams**; the teamplay
  constructor decides CTF again. Kept.

halflife-updated changes dropped for the original's behavior are noted at each function (3);
kept ones: voice manager (all hear all), mp_fragsleft/mp_timeleft updates, fullupdate/closemenus
commands, % filter on "Unknown command", UTF-8 check in say, MAX_EDICTS test, HLTV proxy test in
SetupVisibility, CWorld duplicate check, sv_busters, m_WeaponBits sync in UpdateClientData,
5-field ScoreInfo.
