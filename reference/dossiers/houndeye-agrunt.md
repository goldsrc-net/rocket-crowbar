# Dossier: houndeye-agrunt

Rocket Crowbar 1.9's drivable houndeye and alien grunt, reconstructed from `reference/rc_i386.so`
(decompile in `recovered/decomp/`, disassembly where Ghidra lost or garbled code).

## 1. Scope

Original source files: `drivehoundeye.cpp` (89 functions) and `driveagrunt.cpp` (63 functions).

Written or touched here:

| File | What |
|---|---|
| `dlls/drivehoundeye.cpp` | CDriveHE, CDriveHEPickup, CDrvHESpawner, CDHEEyeSprite, CDHESuckingSprite, DHEBeamDrawFader, the eye-beam free functions; TEMP STUBs of the three PREFS_KeyLoopUp* functions |
| `dlls/driveagrunt.cpp` | CDriveAGrunt, CDriveAGruntPickup, CDrvAGruntSpawner |
| `dlls/drivehoundeye.h`, `dlls/driveagrunt.h` | new: `CreateDrvHESpawner()` / `CreateDrvAGruntSpawner()` prototypes (world.cpp calls them) |
| `dlls/driveent.h` | `CBaseDriveEnt::m_pOwner`, per the coordinator's merge decision |
| `dlls/player.h` | `CBasePlayer::m_fDrvEntAlpha` in a `// houndeye-agrunt needs:` block |

Class declarations stay in the .cpp files: nothing outside these files uses them.

## 2. Status

```
scripts/check_file.py drivehoundeye.cpp --missing   89 of 89 present, 0 missing
scripts/check_file.py driveagrunt.cpp --missing     63 of 63 present, 0 missing
scripts/build.sh i386     builds
scripts/build.sh amd64    builds (only the pre-existing SDK header warnings)
```

All eight entity factories are exported (`nm -D`): drive_houndeye, drive_houndeye_spawner,
pickup_drive_houndeye, he_charge_flare, he_eye_flare, drive_agrunt, drive_agrunt_spawner,
pickup_drive_agrunt. DHEBeamDrawFader has no factory in the original either.

Built only. Nothing has been run in an engine.

Provenance comments (`// orig 0x...`) use the file addresses of `reference/funcfile.tsv`
(Ghidra's address minus 0x10000).

## 3. Function map

All functions are in the .cpp of the same name. "ok" means a literal reading of the decompile,
checked against the disassembly where the note says so.


`driveagrunt.cpp`

```
address    function                                   status
0x11fff0   CDriveAGrunt::BigGunBeamLine               ok
0x123ab0   CDriveAGrunt::BloodColor                   ok
0x11ef40   CDriveAGrunt::BurstGunFire                 ok
0x11d610   CDriveAGrunt::CheckDoor                    ok
0x11d870   CDriveAGrunt::CheckEHop                    VERIFY: never resets m_fNextEHopCheck (confirmed in the disassembly)
0x123ac0   CDriveAGrunt::Classify                     ok
0x11cea0   CDriveAGrunt::DAGThink                     ok
0x122a90   CDriveAGrunt::DEClientEject                ok
0x1223c0   CDriveAGrunt::DEClientEnter                ok
0x122780   CDriveAGrunt::DEClientLeave                VERIFY: SetupVisibility gets dummy pvs/pas pointers (original passes NULL)
0x1223a0   CDriveAGrunt::DEClientPostThink            ok
0x121e20   CDriveAGrunt::DEClientPreThink             ok
0x11d4c0   CDriveAGrunt::DoBouncyGun                  ok
0x11d1f0   CDriveAGrunt::DoBurstGun                   ok
0x11e6f0   CDriveAGrunt::DoHyper                      ok
0x11dee0   CDriveAGrunt::DoMovement                   ok
0x11d330   CDriveAGrunt::DoSlash                      ok
0x11e590   CDriveAGrunt::DoYaw                        ok
0x121540   CDriveAGrunt::DyingThink                   ok
0x1238d0   CDriveAGrunt::EntIsBeingRemoved            ok
0x120560   CDriveAGrunt::FireBigGun                   VERIFY: `flags &= FL_ONGROUND` kept literally; loop read from the disassembly
0x123980   CDriveAGrunt::GetCamRotOrg                 ok
0x11ce30   CDriveAGrunt::GetControllerName            ok
0x123960   CDriveAGrunt::GetGuassDistOut              ok
0x123970   CDriveAGrunt::GetGuassHull                 ok
0x123950   CDriveAGrunt::GetRCCaps                    ok
0x121200   CDriveAGrunt::HandleAnimEvent              ok
0x121a10   CDriveAGrunt::Killed                       ok
0x11cc40   CDriveAGrunt::Precache                     ok
0x11c8c0   CDriveAGrunt::Restore                      ok
0x11db70   CDriveAGrunt::RunWaterPhysics              ok
0x11c870   CDriveAGrunt::Save                         ok
0x121600   CDriveAGrunt::SetDeathAnim                 ok
0x1239e0   CDriveAGrunt::SetObjectCollisionBox        ok
0x11fae0   CDriveAGrunt::SlashFire                    VERIFY: both hands knock the target the same way (original, read from the disassembly)
0x11c910   CDriveAGrunt::Spawn                        VERIFY: m_bloodColor red but BloodColor() yellow, as in the original
0x121700   CDriveAGrunt::TakeDamage                   ok
0x1218c0   CDriveAGrunt::TraceAttack                  ok
0x11ed30   CDriveAGrunt::UpdateOwnerHUD               ok
0x123860   CDriveAGruntPickup::BloodColor             ok
0x123870   CDriveAGruntPickup::Classify               ok
0x1234f0   CDriveAGruntPickup::DeathNotice            ok
0x123430   CDriveAGruntPickup::DoRespawn              ok
0x123840   CDriveAGruntPickup::GetRCCaps              ok
0x123000   CDriveAGruntPickup::GetSpawnOrg            ok
0x123850   CDriveAGruntPickup::IsTeleFraggabe         ok
0x123820   CDriveAGruntPickup::ObjectCaps             ok
0x123090   CDriveAGruntPickup::PAnimThink             ok
0x122fb0   CDriveAGruntPickup::Precache               ok
0x122d20   CDriveAGruntPickup::Restore                ok
0x122cd0   CDriveAGruntPickup::Save                   ok
0x122d70   CDriveAGruntPickup::Spawn                  ok
0x123080   CDriveAGruntPickup::TakeDamage             ok
0x123230   CDriveAGruntPickup::Use                    VERIFY: NULL pCaller tested before IsPlayer (the original calls IsPlayer first)
0x123690   CDrvAGruntSpawner::DrvAThink               ok
0x1237c0   CDrvAGruntSpawner::ObjectCaps              ok
0x123640   CDrvAGruntSpawner::Restore                 ok
0x1235f0   CDrvAGruntSpawner::Save                    ok
0x123720   CDrvAGruntSpawner::Spawn                   ok
0x123560   CreateDrvAGruntSpawner                     ok
0x11c840   drive_agrunt                               ok
0x1235c0   drive_agrunt_spawner                       ok
0x122ca0   pickup_drive_agrunt                        ok
```

`drivehoundeye.cpp`

```
address    function                                   status
0x167850   AddEntToItterList                          ok
0x167440   CDHEEyeSprite::CreateES                    ok
0x167490   CDHEEyeSprite::ESThink                     VERIFY: no null-owner check, as in the original
0x167410   CDHEEyeSprite::Precache                    ok
0x167310   CDHEEyeSprite::Spawn                       ok
0x166f10   CDHESuckingSprite::CreateSS                ok
0x166ee0   CDHESuckingSprite::Precache                ok
0x166f90   CDHESuckingSprite::SSThink                 ok
0x166db0   CDHESuckingSprite::Spawn                   ok
0x16a580   CDriveHE::BloodColor                       ok
0x162910   CDriveHE::CheckDoor                        ok
0x162b60   CDriveHE::CheckEHop                        ok
0x16a590   CDriveHE::Classify                         ok
0x166ba0   CDriveHE::DEClientEject                    ok
0x1664e0   CDriveHE::DEClientEnter                    ok
0x166890   CDriveHE::DEClientLeave                    VERIFY: SetupVisibility gets dummy pvs/pas pointers (original passes NULL; needs RC's client.cpp)
0x1664c0   CDriveHE::DEClientPostThink                ok
0x165f60   CDriveHE::DEClientPreThink                 ok
0x161d50   CDriveHE::DHEThink                         ok
0x163bb0   CDriveHE::DoEye                            ok
0x1649d0   CDriveHE::DoEyeAttackFrame                 ok
0x163210   CDriveHE::DoMovement                       ok, TraceHull and WALK_MOVE args from the disassembly
0x162500   CDriveHE::DoSleep                          ok
0x1621c0   CDriveHE::DoSonicAttack                    ok
0x1641c0   CDriveHE::DoSonicAttackBlast               ok
0x162370   CDriveHE::DoStartEyeAttack                 ok
0x163a20   CDriveHE::DoYaw                            ok, keeps the original's time+time+RANDOM_FLOAT(3,8)
0x1657c0   CDriveHE::DyingThink                       ok
0x16a450   CDriveHE::EntIsBeingRemoved                ok
0x164830   CDriveHE::EyeAttackStop                    ok
0x16a520   CDriveHE::GetCamRotOrg                     ok
0x161ce0   CDriveHE::GetControllerName                ok
0x16a500   CDriveHE::GetGuassDistOut                  ok
0x16a510   CDriveHE::GetGuassHull                     ok
0x16a4f0   CDriveHE::GetRCCaps                        ok
0x162e80   CDriveHE::GetRunSeq                        ok
0x162e90   CDriveHE::GetWalkSeq                       ok
0x163e40   CDriveHE::HandleAnimEvent                  ok
0x162ee0   CDriveHE::IsWalk                           ok
0x165bd0   CDriveHE::Killed                           ok
0x161b10   CDriveHE::Precache                         ok
0x162f00   CDriveHE::RunWaterPhysics                  ok
0x165880   CDriveHE::SetDeathAnim                     ok, from the disassembly
0x163d60   CDriveHE::SetRandomAnim                    ok
0x161840   CDriveHE::Spawn                            ok
0x165980   CDriveHE::TakeDamage                       ok, from the disassembly
0x163c40   CDriveHE::UpdateOwnerHUD                   ok
0x16a3e0   CDriveHEPickup::BloodColor                 ok
0x16a3f0   CDriveHEPickup::Classify                   ok
0x16a070   CDriveHEPickup::DeathNotice                ok
0x169fb0   CDriveHEPickup::DoRespawn                  ok
0x16a3c0   CDriveHEPickup::GetRCCaps                  ok
0x169b80   CDriveHEPickup::GetSpawnOrg                ok
0x16a3d0   CDriveHEPickup::IsTeleFraggabe             ok
0x16a3a0   CDriveHEPickup::ObjectCaps                 ok
0x169c10   CDriveHEPickup::PAnimThink                 ok
0x169b30   CDriveHEPickup::Precache                   ok
0x1698a0   CDriveHEPickup::Restore                    ok
0x169850   CDriveHEPickup::Save                       ok
0x1698f0   CDriveHEPickup::Spawn                      ok
0x169c00   CDriveHEPickup::TakeDamage                 ok
0x169db0   CDriveHEPickup::Use                        VERIFY: NULL pCaller tested before IsPlayer (the original calls IsPlayer first)
0x16a210   CDrvHESpawner::DrvAThink                   ok
0x16a340   CDrvHESpawner::ObjectCaps                  ok
0x16a1c0   CDrvHESpawner::Restore                     ok
0x16a170   CDrvHESpawner::Save                        ok
0x16a2a0   CDrvHESpawner::Spawn                       VERIFY: original's classname is drive_panth_spawner (copy-paste bug, kept)
0x1677c0   CheckIfItteredOn                           ok
0x16a0e0   CreateDrvHESpawner                         ok
0x169530   DHEBeamDrawFader::CreateFader              ok
0x169680   DHEBeamDrawFader::FadeThink                ok
0x167970   DoBeamCylDamage                            ok, from the disassembly (center/feet/head selection); inner bounds r1*r2, r1*r3 as in the original
0x167ea0   DoBeamPVSItter                             ok
0x168af0   DoDHEBeamGlobalTracer                      ok
0x1689d0   DoDHEBeamMakeGlow                          ok
0x168850   DoDHEBeamThrowGreenBalls                   ok
0x168e70   DoDHEEyeBeamTraces                         ok, from the disassembly
0x168450   DoEyeBeamLine                              ok, from the disassembly
0x167610   DoItterOnAll                               ok
0x1681e0   DoItterPointExplo                          ok
0x169450   FireDHEEyeBeam                             ok, from the disassembly
0x167590   ResetItteredEnts                           ok
0x169780   TestPointInCycl                            ok
0x164170   WriteDHEBeamColor                          ok
0x161810   drive_houndeye                             ok
0x16a140   drive_houndeye_spawner                     ok
0x1617b0   he_charge_flare                            ok
0x1617e0   he_eye_flare                               ok
0x169820   pickup_drive_houndeye                      ok
```

## 4. Classes

Bases from `hierarchy.tsv`: CDriveHE and CDriveAGrunt derive from CBaseDriveEnt; both pickups
from CBaseMonster; the spawners, sprites and DHEBeamDrawFader from CBaseEntity. No class adds
virtuals except the pickups' `IsTeleFraggabe` (slot 150, `bool`, returns false).

**CBaseDriveEnt::m_pOwner** (+0x27c in both drive classes): the driving player, declared once in
`driveent.h` as `CBasePlayer*` (the original's CDriveAGrunt save table types it `CBaseEntity*`).

**CDriveHE** has no save table in the original (it inherits CBaseMonster's Save/Restore), so none
of its members have real names. Offsets and the names chosen:

```
+0x280 m_fFrameTime        +0x2a8 m_bCanBlink          +0x2c4 m_fSleepRegenTime
+0x284 m_vecSudoVel        +0x2a9 m_bInSonicAttack     +0x2c8 m_fNextIdleAnim
+0x290 m_fLastMoveTime     +0x2ac m_fSonicRefire       +0x2cc m_fNextBlink
+0x294 m_fNextEHopCheck    +0x2b0 m_fLastAttack        +0x2d0 m_fNextJumpBack
+0x298 m_fNextDoorCheck    +0x2b4 m_bInEyeAttack       +0x2d4 m_fNextPainSound
+0x29c..0x2a2 m_bCan*      +0x2b8 m_fEyeAttackRefire   +0x2d8 m_fNextIdleSound
+0x2a4 m_fYawSpeedScale    +0x2bc m_fEyeAttackFireTime +0x2dc m_iSpriteTexture
                           +0x2c0 m_bSleeping          +0x2e0 m_pEyeSprite
                           +0x2c1 m_bWantSleep         +0x2e4 m_iClientHealth
```

Names that mirror a saved CDriveAGrunt member (m_fFrameTime, m_vecSudoVel, m_fLastMoveTime,
m_fNextEHopCheck, m_fNextDoorCheck, m_iClientHealth) are taken from it, same role. The seven
`m_bCan*` bytes (Idle, Move, Turn, FastTurn, JumpBack, Sleep, EyeAttack) are per-frame permission
flags DHEThink sets and each action step clears; named from what they gate, all VERIFY.
m_bCanEyeAttack is written but never read.

**CDriveAGrunt** saved members come from its save table (25 entries, the original's duplicates
of m_fSlashRefire, m_iSlashSide and m_fLastSlash kept). `m_iSlashSide` is an `int` (the code reads
and writes 4 bytes) saved as FIELD_BOOLEAN, as in the original. Unsaved, from code:

```
+0x284 m_bCanIdle   +0x285 m_bMoving (never read)   +0x286 m_bCanMove   +0x287 m_bCanFastTurn
+0x2d4 m_fYawSpeedScale   +0x2d8 m_fNextIdleSound   +0x2dc m_fNextPainSound
+0x2e8..0x2fc sprite indices: m_iMuzzleFlash, m_iWallGlow, m_iRedFlare, m_iBurstBeam, m_iBigGunBeam, m_iMuzzleTrail
```

**Pickups**: m_pGiven (FIELD_CLASSPTR) and m_bTakeable (FIELD_BOOLEAN), saved. **Spawners**:
m_bDummy (FIELD_BOOLEAN), saved, unused. **DHEBeamDrawFader**: m_pBeam (+0x34, CBeam*). The sprites
keep their state in entvars: CDHESuckingSprite's air_finished is its max brightness, dmg its life;
DHEBeamDrawFader's dmg is its life, dmgtime its end.

File statics: the sound tables (`g_szDHE*`, `g_sDAG*`), the anim tables `g_sDHEIdleAnims` /
`g_sDHEAttackAnims` (`sDHEAnimT`: sequence, weight, min/max time; read from .data),
`g_iDHEGreenBallSpray`, `g_iDHEWallFlare`, and `g_sDHEItteredEnts[512]` (entity index, damage,
fade time, fade hold).

## 5. Cross-file needs

- **`CBaseDriveEnt::m_pOwner`** added to `dlls/driveent.h` (coordinator decision 1).
- **`CBasePlayer::m_fDrvEntAlpha`** (+0x674, unsaved) in a `// houndeye-agrunt needs:` block in
  `dlls/player.h` (coordinator decision 2). client.cpp's `cl_de_alpha` command sets it 0..1.
- **Preff.cpp**: `PREFS_KeyLoopUpFloat`, `PREFS_KeyLoopUpInt`, `PREFS_KeyLoopUpBool`, declared
  `extern` with the coordinator's signatures in both files. TEMP STUBs returning the default are at
  the bottom of `drivehoundeye.cpp` (one copy for both files); drop at merge, include `Preff.h`.
- **client.cpp `SetupVisibility`**: RC's version accepts NULL `pvs`/`pas`; the SDK's writes
  through them. Both DEClientLeave pass dummy pointers for now (VERIFY). Once client.cpp carries
  RC's version they can pass NULL like the original.
- **world.cpp** calls `CreateDrvHESpawner()` and `CreateDrvAGruntSpawner()` (headers above).
- **subs.cpp** owns the `info_drive_houndeye_spawn` / `info_drive_agrunt_spawn` entities the
  pickups look for.
- Used as they are in the SDK: gmsgHealth, gmsgHideWeapon, gmsgSetFOV, gmsgBattery,
  gmsgFlashBattery, gmsgTrain, g_pevLastInflictor, g_pModelNameLaser, g_sModelIndexLaser,
  UTIL_MoveToOrigin, CBasePlayer::RemoveAllItems / UpdateClientData, the RC CBaseEntity virtuals
  (HasRCCap, GetRCCaps, EntIsBeingRemoved, GetControllerName, GetGuass*).
- `FireDHEEyeBeam`, `WriteDHEBeamColor` and the other eye-beam functions are non-static globals
  with their original names; nothing outside drivehoundeye.cpp calls them.

## 6. VERIFY list

22 VERIFY marks in the two .cpp files.

1. Names of all unsaved members (both drive classes, section 4). Settled only by other RC code or
   a symbol source; none exists.
2. DEClientLeave (both): dummy pointers to SetupVisibility, see section 5.
3. `CDrvHESpawner::Spawn` names itself `drive_panth_spawner`, read from the binary's string
   (0x2636e3). Kept: CreateDrvHESpawner therefore never finds an existing spawner, but each
   spawner removes itself after 15 s.
4. `CDriveAGrunt::CheckEHop` never sets m_fNextEHopCheck (no store to +0x2cc in 0x11d870-0x11db62),
   unlike the houndeye's.
5. `CDriveAGrunt::SlashFire`: both hands add `forward*150 + right*160` (disassembly 0x11fde8 on).
6. `CDriveAGrunt::FireBigGun`: `flags &= FL_ONGROUND` (`andl $0x200` at 0x120f0e), probably
   meant `&= ~FL_ONGROUND`. Kept.
7. `CDriveAGrunt::Spawn`: m_bloodColor BLOOD_COLOR_RED, BloodColor() returns yellow. Kept.
8. Pickup Use (both): the original calls `pCaller->IsPlayer()` before its NULL test; ours tests
   NULL first (same behavior for any real caller).
9. `CDHEEyeSprite::ESThink` dereferences its owner unchecked, as the original does.
10. m_bCanEyeAttack (houndeye) and m_bMoving (grunt) are written, never read.

Original quirks kept without a VERIFY, read off the disassembly: CDriveHE::DoYaw sets
`m_fNextIdleAnim = time + time + RANDOM_FLOAT(3,8)`; DoBeamCylDamage's inner bounds are
`r1*r1`, `r1*r2`, `r1*r3`; the grunt's Killed sets classname `AGrunt` (capital).

## 7. Behavior worth testing

- Houndeye: attack (sonic blast, 460 range, houndeyes immune), attack2 (3.5 s eye charge with
  sucked-in sparks, then a beam that punches through up to 200 units of wall, a kick back, and
  green screen fades on everyone hit), duck (sleep, regenerates; damage wakes it), back (hop
  back), walk/use (slow walk), reload (look around without steering).
- Agrunt: attack (burst gun), attack2 (alternating slashes that knock targets and punch players'
  view), jump (bouncing big gun, 5 bounces, through monsters), duck (hyper: red glow, faster,
  regenerates health, drains armor; hangover damage when empty).
- Pickups at `info_drive_*_spawn` fade in, give one monster per player, kill it after
  HoundMaxDriveTime / AGMaxDriveTime (600 s), respawn 5 s after it dies.
- Driver HUD: health as a percentage, the grunt's hyper charge as armor; `cl_de_alpha`
  translucency of the camera; eject (sv_allowglobaleject or AllowEject in rc.ini).
- Compare all of these against the original with an rc.ini that has none of the keys (the stubs
  return defaults).
