# Dossier: curses-misc

## 1. Scope

Original source files: curses.cpp, nm.cpp, wog.cpp, deathballs.cpp, flag.cpp, levelcustom.cpp,
pointexplo.cpp, corpse.cpp (168 functions, compiler-generated ones excluded).

Written: `dlls/curses.cpp`, `nm.cpp`, `wog.cpp`, `deathballs.cpp`, `flag.cpp`, `levelcustom.cpp`,
`pointexplo.cpp`, `corpse.cpp`, and new headers `curses.h`, `nm.h`, `wog.h`, `deathballs.h`,
`flag.h` (CBaseFlag declaration), `levelcustom.h`, `pointexplo.h` (CPointExplo declaration),
`headcrab.h`.

Touched outside the package: `dlls/headcrab.cpp`. Its CHeadCrab class declaration moved verbatim
into the new `dlls/headcrab.h` (CBaseCurse derives from CHeadCrab), and headcrab.cpp now includes
it. No other change to that file. Whoever reconstructs headcrab.cpp's RC additions (CKillerBabyCrab,
CHeadCrab::GetBotShootOrg/GetRCCaps/GetGuass*) adds those to the header.

## 2. Status

`scripts/check_file.py <file> --missing`, i386 build of this worktree:

```
curses.cpp       82 of 82 present, 0 missing
nm.cpp           10 of 10 present, 0 missing
wog.cpp          12 of 12 present, 0 missing
deathballs.cpp   23 of 23 present, 0 missing
flag.cpp         15 of 15 present, 0 missing
levelcustom.cpp  20 of 20 present, 0 missing
pointexplo.cpp    2 of  2 present, 0 missing
corpse.cpp        4 of  4 present, 0 missing
```

`scripts/build.sh i386` and `scripts/build.sh amd64` both build with no warnings from these files
(the only warnings are the SDK's CUSTOM_SCHEDULES macro, now seen through headcrab.h). All 18
factories (curse_spawner, basecurse, reversecurse, slowcurse, slowrefirecurse, dmgmirrorcurse,
dmgscalecurse, drunkcurse, promagcurse, sdcurse, nuke_rocket, db_spawner, deathball, flag,
lc_init_ent, lc_respawner, lc_spawner, player_corpse) are exported. Built only: nothing has run in
an engine.

## 3. Function map

Addresses are file addresses (funcfile.tsv); each function carries `// orig 0x...` in the source.
"ok" means read from the decompile, with the disassembly checked where Ghidra was unclear
(DBThink's control flow, FireWOG's damage loop, FlagTouch's branch order and sprintf arguments,
NukeRadus's damage argument, NukeThink's tail and constants, CDrunk's FOV formula, AttachedThink,
WOGCommand, LC_ExecConf's sscanf argument order, CreateWSed, FireThink).

curses.cpp (dlls/curses.cpp)

```
0x00114f80  curse_spawner                              ok (LINK_ENTITY_TO_CLASS)
0x00114fb0  CreateCurseSpawner                         ok
0x00115030  CBaseCurseSpawner::Spawn                   ok
0x00115090  CBaseCurseSpawner::Precache                ok
0x001150c0  CBaseCurseSpawner::CSThink                 ok; 7 Create calls, no drunkcurse (checked in disasm)
0x001151b0  basecurse                                  ok (LINK_ENTITY_TO_CLASS)
0x001151e0  CBaseCurse::GetPrefsFloat                  ok
0x00115250  CBaseCurse::Spawn                          ok
0x00115380  CBaseCurse::MonsterThink                   ok
0x00115760  CheckForCurseNearOrg                       ok
0x00115840  CBaseCurse::SetSpawnOrg                    ok
0x00115a40  CBaseCurse::LeapyTouch                     ok
0x00115b20  CBaseCurse::StartTask                      ok
0x00115ba0  CBaseCurse::Precache                       ok
0x00115be0  CBaseCurse::SetYawSpeed                    ok
0x00115c20  CBaseCurse::IRelationship                  ok
0x00115c70  CBaseCurse::CheckRangeAttack1              ok
0x00115d00  CBaseCurse::TakeDamage                     ok
0x00115ed0  CBaseCurse::TraceAttack                    ok
0x00115f20  CBaseCurse::AttachToHost                   ok
0x00115fc0  CBaseCurse::AttachedThink                  ok
0x00116070  reversecurse                               ok (LINK_ENTITY_TO_CLASS)
0x001160a0  CCurseBackwards::InherAttach               ok
0x001160f0  CCurseBackwards::InherDetach               ok
0x00116160  CCurseBackwards::InherATThink              ok
0x001162b0  slowcurse                                  ok (LINK_ENTITY_TO_CLASS)
0x001162e0  CCurseSlow::InherAttach                    ok
0x00116330  CCurseSlow::InherDetach                    ok
0x001163a0  CCurseSlow::InherATThink                   ok
0x001164d0  slowrefirecurse                            ok (LINK_ENTITY_TO_CLASS)
0x00116500  CCurseRefire::InherAttach                  ok
0x00116540  CCurseRefire::InherDetach                  ok
0x00116560  dmgmirrorcurse                             ok (LINK_ENTITY_TO_CLASS)
0x00116590  CCurseDamageMirror::InherAttach            ok
0x001165d0  CCurseDamageMirror::InherDetach            ok
0x001165f0  dmgscalecurse                              ok (LINK_ENTITY_TO_CLASS)
0x00116620  CCurseDamageScale::InherAttach             ok
0x00116660  CCurseDamageScale::InherDetach             ok
0x00116680  drunkcurse                                 ok (LINK_ENTITY_TO_CLASS)
0x001166b0  CCurseDrunk::InherAttach                   ok
0x001166e0  CCurseDrunk::InherDetach                   ok
0x00116750  CCurseDrunk::InherATThink                  ok
0x00116990  promagcurse                                ok (LINK_ENTITY_TO_CLASS)
0x001169c0  CCurseProMagnet::InherATThink              ok; cap 0x20000000 named RC_CAP_PROMAGNET (VERIFY name)
0x00116bb0  sdcurse                                    ok (LINK_ENTITY_TO_CLASS)
0x00116be0  CCurseSDeath::InherAttach                  ok
0x00116c10  CCurseSDeath::InherATThink                 ok
0x00116cf0  DrawCursedMsg                              ok
0x00117090  DropCurses                                 ok
0x001170a0  CCurseSDeath::InherGetCurseNiceName        ok
0x001170c0  CCurseSDeath::InherGetCurseName            ok
0x001170e0  CCurseSDeath::InherGetShellColor           ok
0x00117170  CCurseProMagnet::InherGetCurseNiceName     ok
0x00117190  CCurseProMagnet::InherGetCurseName         ok
0x001171b0  CCurseProMagnet::InherGetShellColor        ok
0x00117240  CCurseDrunk::InherGetCurseNiceName         ok
0x00117260  CCurseDrunk::InherGetCurseName             ok
0x00117280  CCurseDrunk::InherGetShellColor            ok
0x00117310  CCurseDamageScale::InherGetCurseNiceName   ok
0x00117330  CCurseDamageScale::InherGetCurseName       ok
0x00117350  CCurseDamageScale::InherGetShellColor      ok
0x001173e0  CCurseDamageMirror::InherGetCurseNiceName  ok
0x00117400  CCurseDamageMirror::InherGetCurseName      ok
0x00117420  CCurseDamageMirror::InherGetShellColor     ok
0x001174a0  CCurseRefire::InherGetCurseNiceName        ok
0x001174c0  CCurseRefire::InherGetCurseName            ok
0x001174e0  CCurseRefire::InherGetShellColor           ok
0x00117560  CCurseSlow::InherGetCurseNiceName          ok
0x00117580  CCurseSlow::InherGetCurseName              ok
0x001175a0  CCurseSlow::InherGetShellColor             ok
0x00117630  CCurseBackwards::InherGetCurseNiceName     ok
0x00117650  CCurseBackwards::InherGetCurseName         ok
0x00117670  CCurseBackwards::InherGetShellColor        ok
0x00117700  CBaseCurse::EntIsBeingRemoved              ok
0x00117740  CBaseCurse::InherGetCurseNiceName          ok
0x00117760  CBaseCurse::InherGetCurseName              ok
0x00117780  CBaseCurse::InherGetShellColor             ok
0x001177b0  CBaseCurse::InherThink                     ok
0x001177c0  CBaseCurse::InherSpawn                     ok
0x001177d0  CBaseCurse::InherATThink                   ok
0x001177e0  CBaseCurse::InherDetach                    ok
0x001177f0  CBaseCurse::InherAttach                    ok
```

nm.cpp (dlls/nm.cpp)

```
0x001d64a0  nuke_rocket                                ok (LINK_ENTITY_TO_CLASS)
0x001d64d0  CNukeM::Spawn                              VERIFY glow sprite iuser3/fuser1-4 values written, nothing here reads them
0x001d6970  CNukeM::PunchTrace                         ok
0x001d6c60  CNukeM::NukeRadus                          ok; full flDamage to all hit, falloff only gates (literal)
0x001d6e20  CNukeM::NukeFade                           ok
0x001d6f20  CNukeM::ShockwaveShake                     ok
0x001d7120  CNukeM::RocketTouch                        ok
0x001d7bc0  CNukeM::NukeThink                          ok; stops thinking at full speed, pitch wrap uses 180 twice (both literal)
0x001d8180  CNukeM::Precache                           ok
0x001d8220  FireNuke                                   ok
```

wog.cpp (dlls/wog.cpp)

```
0x0023c440  CWOGBeam::CreateFader                      ok
0x0023c550  CWOGBeam::FadeThink                        ok
0x0023c650  FireWOG                                    ok
0x0023d7a0  WOGPrecache                                ok
0x0023d850  CWOGSpawner::CreateWSpt                    ok
0x0023d900  CWOGSpawner::CreateWSed                    ok
0x0023d960  CWOGSpawner::FireThink                     ok
0x0023dac0  WOGCalledFireForward                       ok
0x0023dbc0  WOGCalledFireOnPlayer                      ok
0x0023dc40  WOGCalledFireOnAll                         ok
0x0023dcb0  WOGInstantFire                             ok
0x0023dd90  WOGCommand                                 VERIFY `CMD_ARGC() <= 41` branch (disasm calls pfnCmd_Argc)
```

deathballs.cpp (dlls/deathballs.cpp)

```
0x001178a0  CreateDBSpawner                            ok
0x00117900  db_spawner                                 ok (LINK_ENTITY_TO_CLASS)
0x00117930  CDBSpawner::Save                           ok
0x00117980  CDBSpawner::Restore                        ok
0x001179d0  CDBSpawner::DBSThink                       ok
0x00117ae0  CDBSpawner::Spawn                          ok
0x00117b80  deathball                                  ok (LINK_ENTITY_TO_CLASS)
0x00117bb0  CDeathBall::Precache                       ok
0x00117c40  CDeathBall::Spawn                          ok
0x00117e20  CDeathBall::CheckCanSpawnHere              ok
0x00118010  CDeathBall::DBInitThink                    ok
0x00118460  CDeathBall::DBDoSpawnEffects               ok
0x001187a0  CDeathBall::DBThink                        ok
0x00118ae0  CDeathBall::DBTouch                        ok; cap 0x2000 named RC_CAP_DEATHBALL_CRUSH (VERIFY name)
0x001190e0  CDeathBall::TakeDamage                     ok
0x001193e0  CDeathBall::TraceAttack                    ok
0x001194f0  CDeathBall::EntIsBeingRemoved              ok
0x00119520  CDeathBall::SetObjectCollisionBox          ok
0x001195e0  CDeathBall::IsDriveent                     ok
0x001195f0  CDeathBall::GetControllerName              ok
0x00119610  CDeathBall::GetRCCaps                      VERIFY cap bit names unknown (0x1cc48)
0x00119620  CDeathBall::BloodColor                     ok
0x00119680  CDBSpawner::ObjectCaps                     ok
```

flag.cpp (dlls/flag.cpp)

```
0x00188490  flag                                       ok (LINK_ENTITY_TO_CLASS)
0x001884c0  CBaseFlag::Spawn                           ok
0x00188630  CBaseFlag::Precache                        ok
0x00188670  CBaseFlag::CreateFlag                      ok
0x00188720  CBaseFlag::FlagTouch                       ok
0x00188d30  CBaseFlag::FlagThink                       ok
0x00188f40  CBaseFlag::FlagPickUp                      ok
0x00188fc0  CBaseFlag::FlagDrop                        ok
0x00189110  CBaseFlag::FlagReturn                      ok
0x001891d0  DoesEntHaveFlag                            ok
0x00189240  GetFlagStatusString                        ok
0x00189340  GetFlagNameString                          ok
0x001893e0  ReportFlagStatus                           ok
0x00189460  GetFlagTeamColor                           VERIFY default case drops the sprintf into a read-only "" (original would fault)
0x00189500  GetFlagBaseColor                           ok
```

levelcustom.cpp (dlls/levelcustom.cpp)

```
0x001bd840  lc_init_ent                                ok (LINK_ENTITY_TO_CLASS)
0x001bd870  CLCEnt::LCInitThink                        ok
0x001bd910  CLCEnt::Spawn                              ok
0x001bd9b0  CreateLCEnt                                ok
0x001bda60  lc_respawner                               ok (LINK_ENTITY_TO_CLASS)
0x001bda90  FindOtherSameSpawner                       ok
0x001bdb00  CLCReSpawner::LCRSThink                    ok
0x001bdc40  CLCReSpawner::DeathNotice                  ok
0x001bdc80  CLCReSpawner::Spawn                        ok
0x001bdd60  lc_spawner                                 ok (LINK_ENTITY_TO_CLASS)
0x001bdd90  CLCSpawner::LCSThink                       ok
0x001bde30  CLCSpawner::Spawn                          ok
0x001bdef0  LC_ReadLine                                ok
0x001bdfc0  LC_CheckStartEqu                           ok
0x001be010  LC_ConfOppAll                              ok
0x001be1e0  LC_ConfOppOnEnt                            ok
0x001be2e0  LC_ConfMoveEnt                             ok
0x001be400  LC_ConfUseEnt                              ok
0x001be480  LC_ExecConf                                ok
0x001be9e0  LC_PrecacheConf                            ok
```

pointexplo.cpp (dlls/pointexplo.cpp)

```
0x001f7200  CPointExplo::ExploCreate                   ok
0x001f7300  CPointExplo::Spawn                         ok
```

corpse.cpp (dlls/corpse.cpp)

```
0x0010de90  player_corpse                              ok (LINK_ENTITY_TO_CLASS)
0x0010dec0  CPlayerCorpse::Spawn                       ok
0x0010df80  CPlayerCorpse::CThink                      ok
0x0010dff0  CPlayerCorpse::ObjectCaps                  ok
```

## 4. Classes

No class here has a save table except CDBSpawner (`m_bDummy`, FIELD_BOOLEAN, orig +0x34, from
savefields.tsv; never read or written). Every other member is unsaved, named from what the code
does, original offset in a comment, and marked VERIFY name. Bases from hierarchy.tsv.

```
CBaseCurse : CHeadCrab           +0x27c m_pHost (CBaseEntity*), +0x280 m_flRespawnTime,
                                 +0x284 m_flNextMoveTime, +0x288 m_flSpawnOrgTime (-1 once done)
  new virtuals, slots 153-160:   InherAttach InherDetach InherATThink InherSpawn InherThink
                                 InherGetShellColor (returns Vector) InherGetCurseName
                                 InherGetCurseNiceName
CCurseBackwards, CCurseSlow      +0x28c/0x290/0x294 m_flForward/Back/SideSpeed (1000 each),
                                 +0x298 m_flNextSpeedTime
CCurseDrunk                      +0x28c m_flAttachTime
CCurseSDeath                     +0x28c m_flNextDmgTime
CCurseRefire/DamageMirror/DamageScale, CCurseProMagnet: no members
CBaseCurseSpawner : CBaseEntity  none
CNukeM : CGrenade                +0x280 m_pGlow, +0x284 m_pCam (CBaseEntity*), +0x288 m_iTrail,
                                 +0x28c m_iExplode, +0x290 m_iBeam (model indexes)
CWOGBeam : CBaseEntity           +0x34 m_pBeam (CBeam*)
CWOGSpawner : CBaseEntity        none (pev->button = mode, pev->enemy = target)
CDeathBall : CBaseMonster        +0x27c m_flSpawnTime, +0x280 m_flHitterTime, +0x284 m_pHitter
                                 (edict_t*), +0x288 m_flNextSpawnFXTime, +0x28c
                                 m_flSpawnFXDeadline, +0x290 m_iSpawnFXLeft, +0x294
                                 m_bSpawnFXStarted, +0x295 m_bOnGround, +0x296 m_bRolling,
                                 +0x298 m_iShockwave
CDBSpawner : CBaseEntity         +0x34 m_bDummy (saved)
CBaseFlag : CBaseMonster         +0x27c m_iAtBase, +0x280 m_iCarried (4-byte ints in the
                                 original), +0x284 m_vecBaseOrigin, +0x290 m_vecBaseAngles,
                                 +0x2a0 m_iTeam (1-based), +0x2a4 m_flNextMsgTime
CLCEnt, CLCSpawner : CBaseEntity none
CLCReSpawner : CBaseEntity       +0x34 m_bSpawned, +0x38 m_flRespawnTime
CPointExplo : CGrenade           none (no factory; ExploCreate only)
CPlayerCorpse : CBaseMonster     none
```

Globals defined here: `g_iCurseMSGColor[4]` = {255, 64, 64, 160} (curses.cpp, from .data),
`g_iWOGImpactDisk/Balls/Glow`, `g_iWOGFireBall`, `g_pCorrect[20]` (wog.cpp; the WOG password
digest from .data 0x2aa7d8; its .data position between weapons.o and world.o puts it in wog.o).

## 5. Cross-file needs

Used from elsewhere, declared `extern` in the using file:

- Preff.cpp (prefs-votes): `PREFS_KeyLoopUpFloat`, `PREFS_KeyLoopUpInt`, `PREFS_KeyLoopUpBool`
  with the coordinator's const char* signatures. TEMP STUBs returning the default, in curses.cpp.
- mdhash.cpp (prefs-votes): `unsigned char* RMD(unsigned char*)`. TEMP STUB in wog.cpp returning
  an all-zero digest (so WOGCommand never accepts a password until merged).
- util.cpp: `FixedUnsigned16`, `FixedSigned16`. Static in our util.cpp, exported in RC's (curses.cpp
  and nm.cpp call them). TEMP STUBs (copies of util.cpp's bodies) in curses.cpp; at merge, drop the
  `static` in util.cpp and the stubs.
- teamplay_gamerules.cpp (RC additions, no package yet): `GetTeamNameForIndex(int)`,
  `AddTeamScore(int, int)`. TEMP STUBs in flag.cpp (return "" / do nothing).
- SDK, unchanged: CGameRules::IsMultiplayer/IsDeathmatch/GetTeamIndex, CBasePlayer::GetAmmoIndex,
  SendAmmoUpdate, AddPoints, m_rgAmmo, m_iFOV, m_szTeamName, gmsgShake, CGrenade::Explode,
  CBeam, SetupVisibility, CBaseMonster's AI.
- RC members already in the tree: CBasePlayer m_fCurseMSGTime/RefireScale/DmgMirror/DmgScale,
  m_bInDriveEnt, m_pCurDriveEnt (WOGCalledFireOnPlayer, through CBaseDriveEnt), the RC CBaseEntity
  virtuals (HasRCCap, ProMagApplyForce, EntIsBeingRemoved, IsDriveent, GetControllerName,
  GetRCCaps).

No members added to shared classes. No user messages of RC's own.

Called from other files (callers not in this package; the headers declare them): CreateCurseSpawner,
DrawCursedMsg, DropCurses, CheckForCurseNearOrg; FireNuke; FireWOG, WOGPrecache, WOGCommand and
the WOGCalled* helpers; CreateDBSpawner; CBaseFlag::CreateFlag, FlagDrop, DoesEntHaveFlag,
ReportFlagStatus, GetFlag*; CreateLCEnt; CPointExplo::ExploCreate. info_deathball_spawn,
info_flagspawn and info_teamspawn are subs.cpp's, fading_corpse is world.cpp's.

## 6. VERIFY list

45 VERIFY marks: 38 guessed member names (section 4), 2 guessed RC cap names, and 5 more (the glow sprite has two marks):

1. `WOGCommand`: for a non-player, non-negative `n`, it strikes the spot looked at when
   `CMD_ARGC() <= 41`, and only with more arguments reaches `n == 42` (strike every argv(3)) or the
   instant strike. The disassembly calls pfnCmd_Argc (offset 0x150, the same call as the `argc > 2`
   check) and compares with 0x29. Literal; the source probably meant something else. A runtime
   test with the real password would settle what players saw.
2. `GetFlagTeamColor` default case: the original sprintf()s "ERROR: Team %d does not exist" into
   the "" literal it returns, which is in .rodata (0x265d66), so the original would fault on a team
   outside 1-6. Ours skips the write and returns "". A candidate for DEVIATIONS.md.
3. `CNukeM::Spawn`: the glow sprite gets iuser3 = 32 and fuser1-4 = 240, 90, 0.9, 0.05. Nothing in
   the package reads them; maybe the client or a removed effect did.
4. `CDeathBall::GetRCCaps` 0x1cc48, the 0x2000 cap a deathball crushes and the 0x20000000 cap the
   projectile magnet pulls: bit names unknown. Whoever reconstructs the RC cap set (rcbot's games.h
   may have them) should name them.

Literal readings worth knowing, not uncertain:

- curse_spawner never makes drunkcurse (seven Create calls in CSThink).
- CNukeM::NukeRadus gives everyone it hits the full damage; the distance falloff only decides who
  is hit.
- CNukeM::NukeThink stops thinking (steering, camera, speed cap) on any think where forward speed
  is already at the maximum. Its camera pitch wrap compares with 180 both times.
- CWOGSpawner is never removed after it fires. FireWOG's inflictor entity is freed by FL_KILLME.
- FireWOG Killed()s, with GIB_NEVER, anything within 256 units still alive after 9000 damage.
- CBaseCurse::TakeDamage sets the damage conditions twice and never changes health from 50.
- CCurseDrunk's FOV uses cos(3t) squared, so it only reaches halfway from FOVMin to FOVMax.
- FindOtherSameSpawner can return the calling respawner itself.
- LC_ReadLine has no caller.

## 7. Behavior worth testing

- Curses with mp_curses 1: spawn 15 s in, hop every Min/MaxMoveTime, attach on a leap to a player,
  the "Cursed by:" text (channel 8, top centre, red), Reversal plus Slowness on one player giving
  negative slow speeds, respawn after the host dies, cl_*speed restored to 1000 on detach.
- A drunkcurse placed by a .lec: view sway and FOV.
- Nuke (FireNuke's caller): ammo cost, the camera ride and the moment it stops steering, wall
  punching (MaxPunch, MaxPunchWalls), the white flash and shake.
- WOG with the real password once RMD is merged, especially the argument-count branch.
- Deathballs with mp_deathballs: spawn rings and fade-in, rolling sound volume, crush damage and
  kill credit to the last pusher or shooter, BouncyDeathball.
- CTF: pick up, drop (mp_flag_return_time), return, capture messages and log lines, carrier glow.
- A map's .lec (e.g. /media/Data3/Projects/mods/rockcrowbar/maps/Dam.lec): create, respawncreate
  (respawn 1 s after death), removeall with a `*` prefix, move, useent.
