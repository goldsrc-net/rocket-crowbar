# Dossier: prefs-votes

Addresses are file addresses (funcfile.tsv; Ghidra's are 0x10000 higher), and every function's
`// orig` comment uses them.

## 1. Scope

Original source files: `Preff.cpp` (17), `mdhash.cpp` (4), `vote_kick.cpp` (10), `vote_map.cpp`
(8), `pregame.cpp` (9), `bot_prefs.cpp` (4), `auth_stuff.cpp` (1), plus the `CVEC<T>` template
that funcfile.tsv files under `(header)` (8 rows: `Resize`, `operator[]`, 3 constructors, 3
destructors).

Written here:

| File | What |
|---|---|
| `dlls/Preff.cpp`, `dlls/Preff.h` | rc.ini reader/writer, `g_pHeadings`, `PREFS_*` |
| `dlls/mdhash.cpp`, `dlls/mdhash.h` | RIPEMD-160 (`MDinit`, `compress`, `MDfinish`, `RMD`) |
| `dlls/vote_kick.cpp`, `dlls/vote_map.cpp`, `dlls/vote.h` | kick and map votes, `VK_*`, `VM_*` and their globals |
| `dlls/pregame.cpp`, `dlls/pregame.h` | observer mode and the version banner |
| `dlls/bot_prefs.cpp`, `dlls/bot_prefs.h` | `BP_*` line readers, `CVEC<T>`, `sEllem`, `sHeading`, `g_vTypeDefs` |
| `dlls/auth_stuff.cpp` | `ClientConnecting` |
| `dlls/player.h` | members added in a `// prefs-votes needs:` block (section 5) |

## 2. Status

Built: both arches compile and link (`scripts/build.sh i386`, `scripts/build.sh amd64`, no
warnings from these files). Nothing has run in an engine.

```
Preff.cpp       17 of 17 present, 0 missing
mdhash.cpp       4 of 4  present, 0 missing
vote_kick.cpp   10 of 10 present, 0 missing
vote_map.cpp     8 of 8  present, 0 missing
pregame.cpp      9 of 9  present, 0 missing
bot_prefs.cpp    2 of 4  present, 2 missing
auth_stuff.cpp   1 of 1  present, 0 missing
```

The two bot_prefs.cpp misses are `_GLOBAL_.I.BP_GetConfArgLine` and `_GLOBAL_.D.BP_GetConfArgLine`:
gcc 2.95's static constructor and destructor for the file (they construct and destroy
`g_vTypeDefs`). funcfile.tsv has them as kind `rc` rather than `generated`. Our build has the same
thing as `_GLOBAL__sub_I_bot_prefs.cpp` with the destructor registered through atexit, so nothing
is missing; check_file.py would need those two rows reclassified to report 0.

`PREFS_LookUpKey` is exported unmangled (`nm -D`: `T PREFS_LookUpKey` on both arches), as the
original's.

Checked off-engine:

- `mdhash.cpp` against the RIPEMD-160 test vectors ("", "a", "abc", "message digest", a-z, the
  448-bit, 62-char and 80-digit strings, a million "a"): all match, built 64-bit. The original's
  compress has the RIPEMD-160 constants (16 uses each) and its final combine.
- `Preff.cpp` linked into a small harness: loading LF copies of the install's `rc.ini`,
  `Copy of rc.ini`, `vertmap.ini` and `smallmap.ini` gives 30 headings, every key found by
  `PREFS_LookUpKey`, and `PREFS_WriteFile` writes back byte-identical files (those files were
  themselves written by the original's PREFS_WriteFile). Int/float/bool/clamp lookups and
  default creation behave as described below. The CRLF originals load no headings (section 7).

## 3. Function map

| Orig | Function | File | |
|---|---|---|---|
| 0xe1350 | PREFS_BinSearchHead | Preff.cpp | ok |
| 0xe14c0 | PREFS_BinSearchKey | Preff.cpp | ok |
| 0xe1670 | PREFS_SearchAll | Preff.cpp | ok |
| 0xe16e0 | PREFS_LookUpKey (extern "C", exported) | Preff.cpp | ok |
| 0xe1730 | PREFS_LookUpWithDefualtCreate | Preff.cpp | ok |
| 0xe17d0 | PREFS_SortAll | Preff.cpp | ok |
| 0xe19f0 | PREFS_AddHeading | Preff.cpp | ok |
| 0xe1b40 | PREFS_SetKey | Preff.cpp | ok |
| 0xe1c10 | PREFS_AddKey | Preff.cpp | ok |
| 0xe1e60 | PREFS_LoadFile | Preff.cpp | VERIFY: terminates the token, not the key copy (V1) |
| 0xe21a0 | PREFS_WriteFile | Preff.cpp | ok |
| 0xe22e0 | PREFS_KeyLoopUpString | Preff.cpp | ok (returns the string; Ghidra shows void) |
| 0xe2310 | PREFS_KeyLoopUpInt | Preff.cpp | ok (the extra 220 bytes are atoi's exception-spec code) |
| 0xe2450 | PREFS_KeyLoopUpFloat | Preff.cpp | ok |
| 0xe25a0 | PREFS_KeyLoopUpBool | Preff.cpp | ok |
| 0xe2650 | PREFS_KeyLoopUpIntClamp | Preff.cpp | ok |
| 0xe29e0 | PREFS_KeyLoopUpFloatClamp | Preff.cpp | ok |
| 0x1c2080 | MDinit | mdhash.cpp | ok |
| 0x1c20b0 | compress | mdhash.cpp | ok (table-driven; same digest) |
| 0x1c3960 | MDfinish | mdhash.cpp | ok |
| 0x1c3a40 | RMD | mdhash.cpp | ok |
| 0x236760 | VK_InitVote | vote_kick.cpp | VERIFY: dereferences pInit when NULL (V5) |
| 0x236c30 | VK_VoteDissconnect | vote_kick.cpp | ok |
| 0x236cc0 | VK_ValidatePTRS | vote_kick.cpp | ok (resets to the world entity, V6) |
| 0x236e80 | VK_VoteFail | vote_kick.cpp | ok |
| 0x2370d0 | VK_IsValidIp | vote_kick.cpp | VERIFY: wants 4 dots, checks only the first number (V7) |
| 0x2372a0 | VK_VoteSucceede | vote_kick.cpp | ok |
| 0x237620 | VK_VKKeysBind | vote_kick.cpp | ok |
| 0x2376a0 | VK_SendVKMsg | vote_kick.cpp | ok |
| 0x237ae0 | VK_VoteFrame | vote_kick.cpp | ok |
| 0x237cc0 | VK_RegisterVote | vote_kick.cpp | ok |
| 0x237e70 | VM_InitVote | vote_map.cpp | VERIFY: tests the kick vote with the map message (V8); allowedmaps.txt stop at char <= 12 (V9); NULL pInit (V5) |
| 0x238360 | VM_ValidatePTRS | vote_map.cpp | ok |
| 0x238450 | VM_VoteFail | vote_map.cpp | ok |
| 0x238620 | VM_VoteSucceede | vote_map.cpp | ok |
| 0x238840 | VM_KeysBind | vote_map.cpp | ok |
| 0x2388c0 | VM_SendVMMsg | vote_map.cpp | ok |
| 0x238cc0 | VM_VoteFrame | vote_map.cpp | ok |
| 0x238f30 | VM_RegisterVote | vote_map.cpp | ok |
| 0x1f7490 | PrintRocketCrowbarMsg | pregame.cpp | ok |
| 0x1f75b0 | PrintObserverMessage | pregame.cpp | ok |
| 0x1f7660 | PrintModeMessage | pregame.cpp | ok |
| 0x1f78b0 | GetRandomObserverSpawn | pregame.cpp | VERIFY: first spot twice as likely, last never (V10) |
| 0x1f7950 | MoveObserverToRandomSpot | pregame.cpp | ok |
| 0x1f7a10 | InitObserver | pregame.cpp | ok |
| 0x1f7b80 | GetNextObserverEnt | pregame.cpp | ok |
| 0x1f7ca0 | ObserverChaseCamThink | pregame.cpp | ok (from the disassembly; Ghidra kept 112 of 2015 bytes) |
| 0x1f8480 | ObserverFrame | pregame.cpp | ok (checked against the disassembly) |
| 0xf4170 | BP_GetConfArgLine | bot_prefs.cpp | VERIFY: stores every token as "" (V11) |
| 0xf4470 | BP_ReadGenLine | bot_prefs.cpp | ok |
| 0xf4570 / 0xf45a0 | _GLOBAL_.D / .I (g_vTypeDefs) | bot_prefs.cpp | ok, compiler-generated |
| 0xebea0 | ClientConnecting | auth_stuff.cpp | VERIFY: reads sv_reserved and always allows (V12) |
| 0x246fd0 | CVEC<char*>::Resize | bot_prefs.h | ok (grows by n % grow, as the original) |
| 0x247130 | CVEC<char*>::operator[] | bot_prefs.h | ok (inlined in our build) |
| 0x2472c0, 0x247340, 0x2474b0 | CVEC<char*/sEllem/sHeading>::CVEC | bot_prefs.h | ok (memsets the constructed elements, as the original) |
| 0x247190, 0x2471e0, 0x247250 | CVEC<...>::~CVEC | bot_prefs.h | deviation: delete[] for the original's scalar delete (V13) |

## 4. Classes and structs

No entity classes. Structs (POD, no names in the binary for the Preff ones):

- `sPrefKey` (8 bytes): `szKey` +0x0, `szValue` +0x4. VERIFY name.
- `sPrefHeading` (12 bytes): `pKeys` +0x0, `iCount` +0x4, `szName` +0x8. VERIFY name. The
  offsets come from every PREFS_ function's `g_pHeadings + i*0xc` arithmetic.
- `CVEC<T>` (20 bytes): `m_pData` +0x0, `m_iAlloc` +0x4, `m_iCount` +0x8, `m_iGrow` +0xc,
  `m_iMax` +0x10, from the constructor (8, 16, 0, 0x8000) and Resize/operator[]. The class name
  and element type names are from the mangled symbols; member names are ours.
- `sEllem` (28 bytes): `szName` +0x0, `iCount` (short) +0x4, `vArgs` (CVEC<char*>) +0x8, from
  BP_GetConfArgLine's writes.
- `sHeading` (28 bytes): CVEC<sEllem> at +0x8 (from CVEC<sHeading>'s constructor). Nothing reads
  +0x0..+0x7; laid out as sEllem. VERIFY.

Globals and their initial values (from the .data section): `g_flVKFractForKick` 0.51,
`g_flVKEndTime`/`g_flVKUpdateTime` -1, `g_iVKMSGColor` {255,64,64,160}; `g_flFractForMap` 0.51,
`g_flVMEndTime`/`g_flVMChangeTime`/`g_flVMUpdateTime` -1, `g_iVMMSGColor` {64,64,255,160};
`g_szMapName` 65 bytes; `g_pVKVoters`/`g_pVMVoters` 32 EHANDLEs each (0x100 bytes).

## 5. Cross-file needs

### Members added to CBasePlayer (`dlls/player.h`, `// prefs-votes needs:` block)

All unsaved, named from use (VERIFY each name). `m_blIsObserver` (0x600) was already there.

| Orig | Member | Used for |
|---|---|---|
| 0x601 | `bool m_blWasObserver` | set entering observer mode; CBasePlayer::Spawn's pregame test skips players with it set |
| 0x602 | `bool m_blObserverRespawn` | PreThink (0x1ef420) calls Spawn() and clears it once out of observer mode |
| 0x604 | `int m_iObserverMode` | 0 free roam, 1 chase free look, 2 chase target view |
| 0x608 | `int m_iObserverDrawMode` | 0-8, reload cycles it |
| 0x60c | `CBaseEntity* m_pObserverTarget` | the chased entity (raw pointer, as the original) |
| 0x610 | `float m_flObserverMsgTime` | HUD message timer |
| 0x614 | `float m_flObserverChaseDist` | chase distance |
| 0x620 | `float m_flNextVoteTime` | vote cooldown (sv_votefreq); CBasePlayer code also writes it (CBasePlayer.c line 5292) |

`InitObserver` clears `m_WeaponBits` as well as `pev->weapons` (the original's only weapon mask).

### Temporary stubs (bottom of `dlls/vote_kick.cpp`)

- `short FixedSigned16(float, float)`, `unsigned short FixedUnsigned16(float, float)`: real ones
  belong in `util.cpp`, where the SDK has them `static`; the original's are external (called
  through the PLT from vote_kick.cpp and vote_map.cpp). Merge: drop `static` in util.cpp and
  delete the stubs (same bodies).
- `bool IsClientListenServer(edict_t*)`: real one belongs in `client.cpp` (orig 0xfe080). The
  stub returns false, so the "Cannot kick the host" check never fires until the real one lands.

### Used from elsewhere (no stub needed)

`UTIL_PlayerByIndex`, `UTIL_VarArgs`, `UTIL_ClientPrintAll`, `UTIL_LogPrintf`, `UTIL_HudMessage`,
`UTIL_FindEntityByClassname`, `UTIL_IsValidEntity`, `UTIL_TraceLine`, `UTIL_PointContents`,
`CBaseEntity::GetControllerName`, `Center`, `IsPlayer`, `IsNetClient`, `CBasePlayer::m_pCurDriveEnt`
/ `m_bInDriveEnt`. Cvars read by name: `sv_kickfract`, `sv_votefreq`, `sv_kickbantime`,
`sv_mapfract`, `sv_mapcfg` (all registered in game.cpp) and `sv_reserved` (registered nowhere, in
the original too).

### Call sites in other files that must call these (original caller, its address)

SDK files:

- `client.cpp`: `ClientCommand` 0xff3f0 (InitObserver, PREFS_BinSearchHead, PREFS_BinSearchKey,
  PREFS_KeyLoopUpFloat, PREFS_LoadFile, PREFS_LookUpKey, PREFS_SetKey, PREFS_WriteFile, reads
  g_pHeadings/g_iCountHeadings directly, VK_InitVote, VK_RegisterVote, VM_InitVote,
  VM_RegisterVote), `ClientDisconnect` 0xfd490 (VK_VoteDissconnect), `StartFrame` 0x103590
  (VK_VoteFrame, VM_VoteFrame).
- `world.cpp`: `CWorld::Precache` 0x23f450 (PREFS_LoadFile; resets g_blInVoteKick, g_blInVoteMap,
  g_flVKEndTime, g_flVMChangeTime, g_flVMEndTime).
- `player.cpp`: `CBasePlayer::PreThink` 0x1ef420 (ObserverFrame, KLU F/I/IC), `CBasePlayer::Spawn`
  0x1f1000 (InitObserver, KLU B/F/I), `CBasePlayer::UpdateClientData` 0x1f44a0
  (PrintRocketCrowbarMsg), `CBasePlayer::Jump` 0x1ed140 (F), `CBasePlayer::ButStompTouch` 0x1ef190
  (F), `CBasePlayer::PostThink` 0x1f0520 (F).
- `multiplay_gamerules.cpp`: `CHalfLifeMultiplay::PlayerSpawn` 0x1cf1c0 (KLU I/IC/S),
  `FlHealthChargerRechargeTime` 0x1d0090 (F), `FlHEVChargerRechargeTime` 0x1d00c0 (F).

PREFS_KeyLoopUp* in the weapon and item files (F Float, I Int, IC IntClamp, B Bool, S String):

- `(header)`: CCrossbowAmmo::AddAmmo 0x2483a0 (IC), CEgonAmmo::AddAmmo 0x24bae0 (IC), CGaussAmmo::AddAmmo 0x24c760 (IC), CGlockAmmo::AddAmmo 0x24cbe0 (IC), CItemBattery::MyTouch 0x24f040 (F/IC), CMP5AmmoClip::AddAmmo 0x250650 (IC), CMP5AmmoGrenade::AddAmmo 0x250320 (IC), CMP5Chainammo::AddAmmo 0x2504b0 (IC), CPythonAmmo::AddAmmo 0x251880 (IC), CRpgAmmo::AddAmmo 0x251fd0 (IC), CShotgunAmmo::AddAmmo 0x252ac0 (IC)
- `crossbow.cpp`: CCrossbow::DoSniperFire 0x110ac0 (F), CCrossbow::FireBolt 0x111930 (F), CCrossbow::FireSniperBolt 0x110a50 (F), CCrossbow::GetItemInfo 0x110760 (IC), CCrossbow::Spawn 0x110580 (I), CImpArrow::IAAttchedThink 0x10eaf0 (F), CImpArrow::IATouch 0x10f1b0 (F), CImpArrow::Spawn 0x10e200 (F)
- `crowbar.cpp`: CCrowbar::PrimaryAttack 0x113ae0 (F), CCrowbar::Reload 0x114480 (F), CCrowbar::SecondaryAttack 0x113b50 (F), CCrowbar::Swing 0x113c70 (F), CCrowbarRocket::Spawn 0x112820 (B/F/I)
- `egon.cpp`: CEgon::Fire 0x186910 (F), CEgon::GetItemInfo 0x1864c0 (IC), CEgon::Reload 0x187260 (F/I), CEgon::SecondaryAttack 0x185f60 (F/I), CEgon::Spawn 0x1858d0 (IC), CEgon::UpdateTractor 0x185c50 (F), CEgonMirv::MFTouch 0x182120 (F/I), CEgonMirv::Spawn 0x181cd0 (F/I), CIsoBomb::IsoRadus 0x183dd0 (F)
- `gauss.cpp`: CGauss::GetItemInfo 0x19a400 (IC), CGauss::PrimaryAttack 0x19ac50 (F/I), CGauss::Reload 0x199cb0 (F/I), CGauss::SecondaryAttack 0x19b8a0 (F/I), CGauss::Spawn 0x19a190 (IC), CTeleBecon::Spawn 0x197f90 (B/F)
- `glock.cpp`: CAcidGib::Spawn 0x19e3b0 (F/I), CGlock::GetItemInfo 0x19ed00 (IC), CGlock::GlockFire 0x19f410 (F), CGlock::Reload 0x19fa50 (IC), CGlock::SecondaryAttack 0x19ee00 (F/I), CGlock::Spawn 0x19eba0 (IC)
- `h_battery.cpp`: CRecharge::BotCanHaveItem 0x1a1950 (IC), CRecharge::Recharge 0x1a17f0 (F), CRecharge::Spawn 0x1a1370 (F), CRecharge::Use 0x1a14b0 (F/IC)
- `handgrenade.cpp`: CBubleGrenade::SlippyGoBoom 0x1a40f0 (F), CFictionTimer::CreateOrUpdateTimer 0x1a3600 (F), CHandGrenade::GetItemInfo 0x1a76e0 (IC), CHandGrenade::Spawn 0x1a7550 (IC), CHandGrenade::WeaponIdle 0x1a7a40 (F), CImplosionGrenade::PreImplo 0x1a6bd0 (F), CImplosionGrenade::ShootImpTimed 0x1a7350 (F)
- `healthkit.cpp`: CHealthKit::MyTouch 0x1ab180 (F), CWallHealth::Recharge 0x1ab9c0 (F), CWallHealth::Spawn 0x1ab630 (F), CWallHealth::Use 0x1ab770 (F)
- `hornetgun.cpp`: CFreezeGreande::FreezeDet 0x1b1560 (F), CFreezeGreande::ShootFreezeTimed 0x1b2210 (F), CHgun::AddToPlayer 0x1b2680 (IC), CHgun::DoFreezeStuff 0x1b2930 (F), CHgun::FinishSecondaryAttack 0x1b4520 (F), CHgun::FireFreeze 0x1b3400 (F/I), CHgun::GetItemInfo 0x1b2760 (IC), CHgun::PrimaryAttack 0x1b3c50 (F), CHgun::ReloadAmmo 0x1b49d0 (F/IC), CHgun::SecondaryAttack 0x1b48d0 (I), CHgun::Spawn 0x1b2530 (IC), CHgun::StartPrimaryFire 0x1b4180 (F), CHgun::WeaponAttackIdle 0x1b4320 (F)
- `mp5.cpp`: CMP5::FireBeam 0x1cc450 (F), CMP5::GetItemInfo 0x1cbf90 (IC), CMP5::IsGoodTarget 0x1cc120 (F), CMP5::PrimaryAttack 0x1cceb0 (I), CMP5::Reload 0x1ce3a0 (IC), CMP5::SecondaryAttack 0x1cd4d0 (B/F/I), CMP5::Spawn 0x1cbd70 (IC)
- `python.cpp`: CPythRound::PRndThink 0x1f93d0 (F), CPythRound::PTouch 0x1f8de0 (F), CPython::GetItemInfo 0x1f95d0 (IC), CPython::PrimaryAttack 0x1fa3d0 (F), CPython::Reload 0x1fa810 (IC), CPython::SecondaryAttack 0x1f99b0 (F/I), CPython::Spawn 0x1f9720 (IC)
- `rpg.cpp`: CBarneyBomb::SciMineActiveThink 0x1fef00 (F), CBarneyBomb::Spawn 0x1feb90 (F), CRpg::GetItemInfo 0x2005e0 (IC), CRpg::PrimaryAttack 0x2007f0 (F/I), CRpg::Reload 0x200410 (IC), CRpg::SecondaryAttack 0x200d30 (F/I), CRpg::Spawn 0x2004b0 (IC), CScientistBomb::SciMineActiveThink 0x1fe390 (F), CScientistBomb::Spawn 0x1fe000 (F), CSuckMine::LookForTarget 0x1fc490 (B), CSuckMine::Spawn 0x1fc180 (F)
- `satchel.cpp`: CSatchel::AddDuplicate 0x2032e0 (B), CSatchel::Deploy 0x2036e0 (F), CSatchel::GetItemInfo 0x203590 (IC), CSatchel::PrimaryAttack 0x203920 (F), CSatchel::Spawn 0x203470 (IC), CSatchel::Throw 0x203ad0 (F), CSatchelCharge::SatchelThink 0x202710 (B/F), CSatchelCharge::Spawn 0x202440 (F)
- `shotgun.cpp`: CFlyinScientist::Spawn 0x20bd70 (F), CShotgun::GetItemInfo 0x210050 (IC), CShotgun::PrimaryAttack 0x210150 (F/I), CShotgun::Reload 0x211660 (F/IC), CShotgun::SecondaryAttack 0x2107e0 (F), CShotgun::Spawn 0x20fc30 (IC), CShotgun::WeaponIdle 0x2118f0 (IC), CSpaceRift::Decay 0x20f000 (F), CSpaceRift::Grow 0x20f8f0 (F), CSpaceRift::Spawn 0x20edc0 (F), CSpaceRift::StartSuck 0x20e5f0 (F)
- `squeakgrenade.cpp`: CSqueak::GetItemInfo 0x21b1d0 (IC), CSqueak::PrimaryAttack 0x21b4a0 (F), CSqueak::Spawn 0x21b0a0 (IC), CSqueakCam::SCamThink 0x218f00 (F), CSqueakCam::Spawn 0x2186e0 (F), CSqueakGrenade::HuntThink 0x21a3c0 (F), CSqueakGrenade::ShootLaser 0x219ea0 (F), CSqueakGrenade::Spawn 0x2199c0 (F), CSqueakGrenade::SuperBounceTouch 0x21ac80 (F)
- `tripmine.cpp`: CHeadcrabTeather::ActThink 0x22a8e0 (F), CHeadcrabTeather::GetPullOrg 0x22dbc0 (F), CHeadcrabTeather::HTTouch 0x22ad40 (F), CTripmine::GetItemInfo 0x22d020 (IC), CTripmine::PrimaryAttack 0x22d1f0 (F), CTripmine::SecondaryAttack 0x22d4c0 (F), CTripmine::Spawn 0x22ced0 (IC), CTripmineLaser::DoBeamTrace 0x22bc00 (F), CTripmineLaser::Killed 0x22cc60 (F), CTripmineLaser::Spawn 0x22b910 (F)

RC files (other packages; counts of calling functions): curses.cpp 3 (F), deathballs.cpp 5
(F/B), driveagrunt.cpp 14, driveapache.cpp 10, driveaslave.cpp 13, driveassassin.cpp 13,
drivebm.cpp 10, drivebullsquid.cpp 9, drivechum.cpp 8, drivecontroll.cpp 15, drivegarg.cpp 12,
drivehc.cpp 6, drivehoundeye.cpp 15, driveicky.cpp 8, drivepanth.cpp 9, driveturret.cpp 8 (all
KLU F/I/B), nm.cpp 4 (F/I), wog.cpp `WOGCommand` 0x23dd90 (RMD).

No caller anywhere: VK_IsValidIp, BP_GetConfArgLine, BP_ReadGenLine, ClientConnecting,
PREFS_KeyLoopUpFloatClamp, PREFS_AddHeading/AddKey/SortAll outside Preff.cpp, and g_vTypeDefs.

The list came from scanning the original's disassembly for PLT calls and GOT reads of these
symbols (`~/stignored/rc-tools/xrefs.py reference/rc_i386.so 2f1314 '<regex>'`;
`annot.py` there annotates a function's strings, GOT symbols and constants).

### Signature changes

The PREFS_ string parameters are `const char*` (the original's `char*`), so the ~300 callers can
pass literals; `PREFS_LookUpWithDefualtCreate` and `PREFS_KeyLoopUpString` return `const char*`.
`PREFS_SearchAll`, `PREFS_LoadFile`, `PREFS_WriteFile`, `PREFS_KeyLoopUpBool` return `bool` (the
original returns a byte). mdhash uses `uint32_t` for the original's `unsigned long`, which is 64
bits on amd64 and would break the hash. `VK_InitVote`/`VM_InitVote` return `const char*`.

## 6. VERIFY list

- V1 `PREFS_LoadFile`: `pTok[255] = 0` after `strncpy(szKey, pTok, 256)` (0xe211c writes through
  the strtok result), and a line of only `=` characters gives a NULL token and crashes the
  original. Kept literally. A runtime test with a long key line or `==` line settles whether to
  keep the crash.
- V2-V4 names: `sPrefKey`, `sPrefHeading` (no names in the binary); `sHeading`'s first two
  members; the CBasePlayer members in section 5. Settled by any other source of RC's names (the
  rcbot.dll interface, RC2's source).
- V5 `VK_InitVote` dereferences `pInit` in the voter loop and the messages, and `VM_InitVote` in
  its messages, after testing it for NULL. Literal. A server-console caller (NULL) would crash;
  check client.cpp's ClientCommand reconstruction for whether NULL is ever passed.
- V6 `VK_ValidatePTRS`/`VM_ValidatePTRS` reset a gone handle to `CBaseEntity::Instance(ENT(0))`,
  the world, not NULL (the original's inlined Instance(int 0)); `VK_VoteFrame` fails the vote when
  the target is the world. Read from the disassembly; behavior relies on it (the fail messages
  then print the world's empty netname instead of crashing).
- V7 `VK_IsValidIp`: needs 4 dots and 4-12 digits, and range-checks `atoi(szCopy)` (the first
  number) once per field. Literal; no caller.
- V8 `VM_InitVote` tests `g_blInVoteKick` and answers "Vote map in progress, cannot vote map".
  Literal (0x237e85).
- V9 `VM_InitVote`'s allowedmaps.txt scan stops at a name whose first character is <= 12
  (`cmpb $0xc,-0x4c(%ebp); jle`). Literal. Its name buffer is 65 bytes here (the original's 64
  overflows by sscanf's terminator).
- V10 `GetRandomObserverSpawn` skips `RANDOM_LONG(0, n-1)` spots: spot 1 for 0 and 1, never spot n.
- V11 `BP_GetConfArgLine` copies each token from its write pointer, which sits on the terminator
  (0xf4290, 0xf43b8), so every stored token is "" and the last one is stored only after an unknown
  escape. Literal; no caller.
- V12 `ClientConnecting` reads `sv_reserved` (not a registered cvar) and returns true. Looks like
  a reserved-slot check left empty; nothing calls it.
- V13 `CVEC::~CVEC` and `Resize` use `delete[]` where the original uses scalar delete on new[]
  memory (heap corruption at shutdown on the original for CVEC<sHeading>). Same visible behavior
  otherwise. A deviation for reference/DEVIATIONS.md at merge.

## 7. Behavior worth testing

- rc.ini with CRLF line endings (the shipped and installed files are CRLF): on Linux the original
  finds no `[HEADING]` (it wants `]` just before the last character) and recreates every key with
  its default as the game asks for them. On Windows (text-mode fopen) they load. Check which one
  the original Linux server actually did, then decide.
- Kick vote: `sv_kickfract` 0.66; passes on yes/total > fract, fails on no/total > fract or
  (yes + not voted)/total < fract; success runs `banid <sv_kickbantime> #<userid> kick`. Neither
  the target nor the initiator votes. Keys 1/2 get rebound to vkyes/vkno every half second for
  voters who have not voted, and back to slot1/slot2 after.
- Map vote: passes at yes/total >= `sv_mapfract`, everyone votes, changelevel 5 seconds later
  with `exec <map>.cfg` first when `sv_mapcfg` is set; allowedmaps.txt filtering.
- Vote HUD text: TE_TEXTMESSAGE, channel 5 centred for voters, tally at the right (kick channel 4,
  map channel 5), colours g_iVKMSGColor / g_iVMMSGColor, held 0.5 s and resent every 0.5 s.
- Observer: jump to play, attack cycles free/chase/target view, attack2 next target or random
  spot, reload cycles draw modes 0-8 (iuser3/fuser1-3/vuser1/euser2 to the client), forward/back
  zoom limits 32/512 on foot, 256/734 for a driven monster, 128/734 otherwise.
- `VK_VoteFail` called while the target is gone runs twice (ValidatePTRS calls it again after
  resetting the target), printing the fail messages twice, as the original.
- WOG password: `RMD` must match the original's digest for the same input (g_pCorrect in wog.cpp).
