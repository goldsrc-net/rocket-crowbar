# chum-icky-hc: drivable chumtoad, ichthyosaur and headcrab

## Scope

Original source files: `drivechum.cpp` (34 functions), `driveicky.cpp` (52), `drivehc.cpp` (34).

Written here: `dlls/drivechum.cpp`, `dlls/driveicky.cpp`, `dlls/drivehc.cpp`. Touched:
`dlls/player.h` (one member, see Cross-file needs). All classes are declared in their `.cpp`; no
new header.

## Status

```
scripts/check_file.py drivechum.cpp --missing   34 of 34 present, 0 missing
scripts/check_file.py driveicky.cpp --missing   52 of 52 present, 0 missing
scripts/check_file.py drivehc.cpp --missing     34 of 34 present, 0 missing
scripts/build.sh i386                           builds, links
scripts/build.sh amd64                          builds, links
nm -D build-i386/dlls/hl.so                     drive_chumtoad drive_headcrab drive_icky
                                                pickup_drive_icky drive_icky_spawner exported
```

Built only. Nothing has run in an engine, so no behavior is checked against the original.

Every body was written from the typed decompile; the disassembly was read where Ghidra lost
track: `ZapBeam`'s parameter order and the pitch negation (FireZap's call site, ZapBeam's
prologue), `DCTChumtoadifyEnt` and `DHCHeadcrabifyEnt` in full (Ghidra split the second into a
`FUN_` fragment and mislabeled the DEClientLeave call as `vtbl[1].GetControllerName`), the
CDriveHC `DoYaw` time-twice quirk, chumtoad `DoJump`'s gravity term, `JumpTouch`'s trace field,
icky `DEClientPreThink`'s euser3 store and `Use`'s pCaller argument. The FPU compare idiom was
checked once against ZapBeam's `fcomp` (`(x < y ...) == 1` means x < y).

## Function map

`ok` = literal reading of decompile (plus disassembly where noted above). Addresses are file
addresses (funcfile.tsv).

### drivechum.cpp

```
0x146ce0 drive_chumtoad (factory)          ok
0x146d10 CDriveChumtoad::Spawn             ok
0x147000 Precache                          ok
0x1470b0 GetControllerName                 ok
0x147120 DBSThink                          ok
0x1475c0 DoZap                             ok
0x147720 DoJump                            ok  (no caller in the original)
0x147850 CheckDoor                         ok
0x147ab0 CheckEHop                         ok
0x147db0 RunWaterPhysics                   ok
0x148080 DoMovement                        ok
0x1486a0 DoYaw                             ok
0x148790 RunJumpPhysics                    ok  (no caller in the original)
0x148b40 UpdateOwnerHUD                    ok
0x148c60 ZapBeam                           ok  (signature from disassembly)
0x1493d0 FireZap                           ok
0x149780 HandleAnimEvent                   ok
0x149890 DyingThink                        ok
0x149950 SetDeathAnim                      ok
0x1499a0 TakeDamage                        ok
0x149b50 Killed                            ok
0x149e90 DEClientPreThink                  ok
0x14a400 DEClientPostThink                 ok
0x14a420 DEClientEnter                     ok
0x14a7d0 DEClientLeave                     ok
0x14aae0 DEClientEject                     ok
0x14acf0 DCTChumtoadifyEnt (free)          VERIFY  reads any drive monster's driver as CDriveChumtoad::m_pOwner
0x14b270 EntIsBeingRemoved                 ok
0x14b2f0 GetRCCaps / GetGuassDistOut / GetGuassHull / GetCamRotOrg / BloodColor / Classify   ok
```

### drivehc.cpp

```
0x15d4d0 drive_headcrab (factory)          ok
0x15d500 CDriveHC::Spawn                   ok
0x15d790 Precache                          ok
0x15d8a0 GetControllerName                 ok
0x15d910 DHCThink                          ok
0x15dcd0 JumpTouch                         ok
0x15dec0 DoJump                            ok
0x15e1d0 CheckDoor                         ok
0x15e420 CheckEHop                         ok
0x15e730 RunWaterPhysics                   ok
0x15eaa0 DoMovement                        VERIFY  in-air anim: run for 30-70 u/s, walk above (literal, reads swapped)
0x15f150 DoYaw                             ok  (time added twice, confirmed in disassembly)
0x15f2e0 UpdateOwnerHUD                    ok
0x15f400 SetRandomAnim                     ok  (returns float: the next-idle delay)
0x15f4e0 HandleAnimEvent                   ok
0x15f880 DyingThink                        ok
0x15f940 SetDeathAnim                      ok
0x15f990 TakeDamage                        ok
0x15fb30 Killed                            ok
0x15fec0 DEClientPreThink                  ok
0x160420 DEClientPostThink                 ok
0x160440 DEClientEnter                     ok
0x1607f0 DEClientLeave                     ok
0x160b00 DEClientEject                     ok
0x160d10 DHHeadcrabifyEffects (free)       ok
0x160f20 DHHeadcrabifyFade (free)          ok
0x161090 DHCHeadcrabifyEnt (free)          VERIFY  same driver read as DCTChumtoadifyEnt; no NULL check on the new monster_headcrab
0x161630 EntIsBeingRemoved                 ok
0x1616b0 GetRCCaps / GetGuassDistOut / GetGuassHull / GetCamRotOrg / BloodColor / Classify   ok
```

### driveicky.cpp

```
0x16a690 drive_icky (factory)              ok
0x16a6c0 CDriveIcky::Spawn                 ok
0x16a920 Precache                          ok
0x16aa10 GetControllerName                 ok
0x16aa80 DIThink                           ok
0x16ad50 DoBite                            ok
0x16af90 DoMovement                        ok
0x16b430 DoCharge                          ok
0x16b810 DoAngles                          ok
0x16ba10 DoEye                             ok
0x16bb20 UpdateOwnerHUD                    ok
0x16bc40 TakeDamage                        VERIFY  NULL guard added on the eject-time write (original has none)
0x16be30 DeathTouch                        ok
0x16be70 DyingThink                        ok
0x16c010 Killed                            ok
0x16c3a0 DoBiteDamage                      ok  (Ghidra's stack tracking is lost; read from the call sequence)
0x16d0b0 HandleAnimEvent                   ok
0x16d0e0 DEClientPreThink                  VERIFY  opaque case clears euser3, not euser2 (disassembly agrees)
0x16d570 DEClientPostThink                 ok
0x16d590 DEClientEnter                     ok
0x16d970 DEClientLeave                     ok
0x16dc80 DEClientEject                     ok
0x16e090 pickup_drive_icky (factory)       ok
0x16e0c0 CDriveIckyPickup::Save / Restore  ok
0x16e160 Spawn                             ok
0x16e390 Precache                          ok
0x16e3e0 GetSpawnOrg                       ok
0x16e460 TakeDamage                        ok
0x16e470 PAnimThink                        ok
0x16e610 Use                               ok  (uses pCaller, per disassembly)
0x16e810 DoRespawn                         ok
0x16e8d0 DeathNotice                       ok
0x16e940 CreateDrvIckySpawner (free)       ok
0x16e9a0 drive_icky_spawner (factory)      ok
0x16e9d0 CDrvIckySpawner::Save / Restore   ok
0x16ea70 DrvAThink                         ok
0x16eb00 Spawn                             ok
0x16eba0 ObjectCaps                        ok
0x16ec00 CDriveIckyPickup::ObjectCaps / GetRCCaps / IsTeleFraggabe / BloodColor / Classify   ok
0x16ecb0 CDriveIcky::EntIsBeingRemoved     ok
0x16ed30 GetRCCaps / GetGuassDistOut / GetGuassHull / GetCamRotOrg / BloodColor / Classify   ok
```

## Classes

Bases from `hierarchy.tsv`: CDriveChumtoad, CDriveHC, CDriveIcky : CBaseDriveEnt;
CDriveIckyPickup : CBaseMonster; CDrvIckySpawner : CBaseEntity. None of the three drive monsters
adds a virtual (`vtables.py --new`); CDriveIckyPickup adds `IsTeleFraggabe` (slot 150, as in
every other pickup class).

The three drive monsters have no save table. All their members were recovered from code. The
first two take the names other drive classes' save tables give the same slots (CDriveAGrunt:
`m_pOwner` +0x27c, `m_fFrameTime` +0x280), and so do `m_vecSudoVel`, `m_fLastMoveTime`,
`m_fNextEHopCheck`, `m_fNextDoorCheck`, `m_iClientHealth`, `m_fNextPainSound`, `m_bInZap`,
`m_fNextZap` (CDriveAGrunt, CDriveASlave). The rest are named from what they do and marked
`VERIFY name`. `m_pOwner` is typed CBasePlayer* (the code uses it as one); the siblings' save
tables declare it CBaseEntity*.

```
CDriveChumtoad                         CDriveHC                               CDriveIcky
+0x27c CBasePlayer* m_pOwner           +0x27c CBasePlayer* m_pOwner           +0x27c CBasePlayer* m_pOwner
+0x280 float  m_fFrameTime             +0x280 float  m_fFrameTime             +0x280 float  m_fFrameTime
+0x284 bool   m_bCanIdle               +0x284 Vector m_vecSudoVel             +0x284 float  m_fNextPainSound
+0x285 bool   m_bWalking (never read)  +0x290 float  m_fLastMoveTime          +0x288 bool   m_bCanIdle
+0x286 bool   m_bCanMove               +0x294 float  m_fNextEHopCheck         +0x289 bool   m_bCanSwim
+0x287 bool   m_bInZap                 +0x298 float  m_fNextDoorCheck         +0x28a bool   m_bFastMove
+0x288 float  m_fNextZap               +0x29c bool   m_bCanIdle               +0x28c float  m_fNextIdleSound
+0x28c float  m_fZapFireTime           +0x29d bool   m_bCanTurnAnim           +0x290 float  m_fNextBlink
+0x290 bool   m_bInJump                +0x29e bool   m_bCanMove               +0x298 bool   m_bInBite
+0x294 Vector m_vecSudoVel             +0x2a0 float  m_fNextIdleAnim          +0x29c float  m_fNextBite
+0x2a0 float  m_fLastMoveTime          +0x2a4 float  m_fNextPainSound         +0x2a0 bool   m_bInCharge
+0x2a4 float  m_fNextEHopCheck         +0x2a8 float  m_fNextIdleSound         +0x2a4 float  m_fChargeOffTime
+0x2a8 float  m_fNextDoorCheck         +0x2ac float  m_fYawSpeedScale         +0x2a8 float  m_fNextCharge
+0x2ac float  m_fYawSpeedScale         +0x2b0 bool   m_bInJump                +0x2ac int    m_iClientHealth
+0x2b0 float  m_fNextBlink             +0x2b4 float  m_fJumpTimeout           +0x2b0 Vector m_vecEnterOrigin (never read)
+0x2b4 float  m_fNextIdle2             +0x2b8 float  m_fJumpChargeStart       +0x2bc float  m_fEnterTime (never read)
+0x2b8 float  m_fNextPainSound         +0x2bc float  m_fJumpVel
+0x2bc int    m_iClientHealth          +0x2c0 float  m_fNextJump
+0x2c0 float  m_fNextRegen             +0x2c4 float  m_fJumpStartTime
+0x2c4 int    m_iLightningSpr          +0x2c8 int    m_iClientHealth
```

CDriveIckyPickup saves `m_pGiven` (+0x27c, FIELD_CLASSPTR) and `m_bTakeable` (+0x280,
FIELD_BOOLEAN); CDrvIckySpawner saves `m_bDummy` (+0x34, FIELD_BOOLEAN, never used). Names and
order from `savefields.tsv`.

File-level data, names from the original's symbols: `g_sCTDieSounds`/`g_sCTPainSounds`
(sound plus pitch range), `g_szDHC{Die,Idle,Pain,Attack}Sounds`, `g_sDHCIdleAnims`
(`sDHCAnimT`: sequence, weight, min/max delay), `g_iHCTransFlare`/`g_iHCTransBalls`,
`g_pDI{Idle,Alert,Attack,Bite,Pain,Die}Sounds`. Contents read from the binary's .data.

## Cross-file needs

- **Member added to CBasePlayer** (`dlls/player.h`, block `// chum-icky-hc needs:`):
  `float m_fDrvEntAlpha` (original +0x674, unsaved). It is the client's `cl_de_alpha` (0-1),
  which client.cpp's ClientCommand and the userinfo handler set (decomp `_free.c` around 17585
  and 26422). Every drive monster's DEClientPreThink reads it, so the other drive packages will
  need the same member: one of them should keep it and the rest drop theirs at merge.
- **TEMP STUBs** at the bottom of `dlls/drivechum.cpp` (real ones belong in Preff.cpp):
  `float PREFS_KeyLoopUpFloat(const char*, const char*, float)`,
  `int PREFS_KeyLoopUpInt(const char*, const char*, int)`,
  `bool PREFS_KeyLoopUpBool(const char*, const char*, bool)`. Each returns its default. The
  original takes `char*`; these take `const char*` so string literals pass. The prefs package
  must use the same signatures or these callers won't link.
- **Defined here, used elsewhere** (the other packages will declare these extern):
  `void DCTChumtoadifyEnt(edict_t* pent, edict_t* pentAttacker)` (drivebullsquid.cpp's spit),
  `void DHCHeadcrabifyEnt(edict_t*, edict_t*)` (drivebm.cpp's gonarch headcrabifier),
  `void DHHeadcrabifyEffects(Vector)`, `void DHHeadcrabifyFade(Vector, CBaseEntity*)`,
  `void CreateDrvIckySpawner()` (called from the gamerules, like the other `CreateDrv*Spawner`).
  A package that stubbed any of these must drop its stub at merge.
- **Used from the SDK as is**: `SetupVisibility` (client.cpp), `gmsgHealth`, `gmsgHideWeapon`,
  `gmsgSetFOV`, `gmsgBattery`, `gmsgFlashBattery`, `gmsgTrain`, `g_pevLastInflictor`,
  `CBasePlayer::UpdateClientData` (the original's slot 154 call), CBaseDriveEnt and
  CBasePlayer's RC members, the `mp_driveicky` cvar (game.cpp).
- `info_drive_icky_spawn` (the spot entity GetSpawnOrg looks for) is not in these files; its
  factory is in another file (`_free.c` @ 0x22c610).

## VERIFY list

38 in all: 28 member names (27 `VERIFY name` in the three files plus `m_fDrvEntAlpha`, settled
only by a source), and these 10 (items 1 and 2 count once per file):

1. `DCTChumtoadifyEnt` / `DHCHeadcrabifyEnt` drive-monster case: the original reads +0x27c of
   whatever drive monster was hit. Our layout puts every drive class's first member at the same
   offset, so the cast to CDriveChumtoad*/CDriveHC* reads the right member as long as every
   drive class keeps `m_pOwner` first. Settle at merge: check each drive class, or move
   `m_pOwner` into CBaseDriveEnt (which is probably where the original had it).
2. RC cap bits by number with guessed meanings: 0x1000000 (chumtoadify/headcrabify target),
   0x40000 (knocked about by the ichthyosaur's bite), 0x800 (bite heals the ichthyosaur).
   Settle when the cap enum is named (other packages' GetRCCaps/HasRCCap uses).
3. `DHCHeadcrabifyEnt`: no NULL check on the created monster_headcrab before its sound. Literal.
4. `CDriveIcky::TakeDamage`: the original writes `m_fDrvEntEjectTime` through `m_pOwner`
   unconditionally (a crash with no driver); we guard it. Same result whenever the original
   didn't crash.
5. `CDriveIcky::DEClientPreThink`: the opaque branch clears euser3 where every other drive
   monster clears euser2. Confirmed in the disassembly, kept.
6. `CDriveHC::DoMovement` in-air animation: "run" for 30-70 u/s and "walk" above 70, the
   reverse of the chumtoad's. Literal from the string references.

## Behavior worth testing

- Chumtoad: attack zaps a lightning beam that forks up to ChumtoadZapMaxForks (4) times when it
  hits nothing; kills read `chumtoad_zap`. Under water (waterlevel 2+) the zap instead discharges:
  radius shock damage 100, the chumtoad gibs, kills read `chumtoad_discharge`. It heals 5 hp
  every 1.5 s in water. Jump does nothing (DoJump is never called).
- Headcrab: hold attack or jump, release to leap; leap speed grows with charge time from 600 to
  1200 at 4 s and keeps growing past it. A leap hit does 40 slash damage (kills read
  `headcrab_leap`), heals 20, and skips `drive_bm`. Fast turns standing still play 180_left/right.
- Ichthyosaur: attack bites (70), attack2 double-bites (45), jump charges (bites then do 95/55,
  kills `ichthyosaur_charge_bite`). A bite throws players aside and spins their view. It takes
  0.15x mortar and 0.66x burn damage and no drown damage. Eject throws the driver backward with
  a big gun sound.
- Pickup: `mp_driveicky` pickups appear 15 s into a multiplayer map on info_drive_icky_spawn
  points (fading in with a distort effect); using one puts the player in; it kills the icky
  after IckyMaxDriveTime (420 s) and respawns 5 s after the icky dies.
- Driver camera: holding reload frees the view from steering; `cl_de_alpha` between 0 and 1 sets
  the euser2/iuser3/fuser1/fuser2 fields on the driver (what reads them is in client.cpp's
  AddToFullPack, other package).
- Bullsquid spit (chumtoadify) and gonarch headcrabifier on a player, a monster and a driven
  monster: the player or driver becomes a chumtoad/headcrab and the attacker gets the kill
  through PlayerKilled with the `bullsquid_chumtoadifier`/`gonarch_headcrabifier` inflictor; a
  chumtoadified monster just gibs, a headcrabified one becomes a monster_headcrab.

## Merge notes (coordinator, 2026-11-07)

- `m_pOwner` moved into `CBaseDriveEnt` (`dlls/driveent.h`) for every drive class; the three
  per-class declarations are gone and DCTChumtoadifyEnt/DHCHeadcrabifyEnt read it through
  `CBaseDriveEnt*`. That settles the +0x27c VERIFY.
- `m_fDrvEntAlpha` is in main's RC member block in `player.h`.
- The PREFS stubs are gone; these files link against the real `Preff.cpp`.
