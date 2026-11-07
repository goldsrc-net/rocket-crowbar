# Dossier: panth-squid-turret

## 1. Scope

Original source files: `drivepanth.cpp` (58 functions), `drivebullsquid.cpp` (56), `driveturret.cpp` (57),
not counting the 7 compiler-generated typeinfo functions.

Written here: `dlls/drivepanth.cpp`, `dlls/drivepanth.h`, `dlls/drivebullsquid.cpp`,
`dlls/drivebullsquid.h`, `dlls/driveturret.cpp`, `dlls/driveturret.h`.
Touched (shared): `dlls/driveent.h`, `dlls/player.h`, `dlls/effects.h` (section 5).

Method: decompile (`recovered/decomp/CDrive*.c`, `CDrv*Spawner.c`, `_free.c`) read in full; the
disassembly read where the decompile is garbled: `CDrivePanth::ClawDamage`,
`CDrivePanth::HandleAnimEvent` (jump tables), `CDrivePanth::DoMovement`/`DoYaw` (two spot checks),
`CDriveBullsquid::CheckEHop` (decompile is a stub), `CDriveBullsquid::HandleAnimEvent`,
`DBSFireChumtoadifer`, `CDriveTurret::TuFireBullets`, `CDriveTurret::DoGun` constants,
`CDriveTurret::GetCamRotOrg`, `CDriveTurretPickup::Spawn` (the VecToAngles argument).
Data tables (sound lists, idle anim table) dumped from `.data` of `rc_i386.so`.

## 2. Status

| file | check_file.py --missing | i386 | amd64 |
|---|---|---|---|
| drivepanth.cpp | 58 of 58, 0 missing | builds | builds |
| drivebullsquid.cpp | 56 of 56, 0 missing | builds | builds |
| driveturret.cpp | 57 of 57, 0 missing | builds | builds |

All nine entity factories are exported from `build-i386/dlls/hl.so` (`drive_panth`,
`pickup_drive_panth`, `drive_panth_spawner` and the bullsquid and turret equivalents).
Nothing has been run in an engine. "Built" is the most that is claimed.

## 3. Function map

Addresses are file addresses (`reference/funcfile.tsv`, the `// orig` comments). Save/Restore
pairs are `IMPLEMENT_SAVERESTORE`.

| file | orig | function | status |
|---|---|---|---|
| drivebullsquid.cpp | 0x141bc0 | LINK_ENTITY_TO_CLASS(drive_bullsquid) | ok |
| drivebullsquid.cpp | 0x141bf0 | CDriveBullsquid::Spawn | ok |
| drivebullsquid.cpp | 0x141ec0 | CDriveBullsquid::Precache | ok |
| drivebullsquid.cpp | 0x142030 | CDriveBullsquid::GetControllerName | ok |
| drivebullsquid.cpp | 0x1420a0 | CDriveBullsquid::DBSThink | ok |
| drivebullsquid.cpp | 0x1423f0 | CDriveBullsquid::DoBite | ok |
| drivebullsquid.cpp | 0x142570 | CDriveBullsquid::DoWhip | ok |
| drivebullsquid.cpp | 0x142690 | CDriveBullsquid::DoChumtoadifier | ok |
| drivebullsquid.cpp | 0x1427b0 | CDriveBullsquid::CheckDoor | ok |
| drivebullsquid.cpp | 0x142a10 | CDriveBullsquid::CheckEHop | ok |
| drivebullsquid.cpp | 0x142d10 | CDriveBullsquid::RunWaterPhysics | VERIFY: buoyancy adds |
| drivebullsquid.cpp | 0x143080 | CDriveBullsquid::DoMovement | VERIFY: restore-anim test inverted vs panth |
| drivebullsquid.cpp | 0x143700 | CDriveBullsquid::DoYaw | ok |
| drivebullsquid.cpp | 0x143850 | CDriveBullsquid::UpdateOwnerHUD | ok |
| drivebullsquid.cpp | 0x143970 | CDriveBullsquid::DoMeleeDamage | VERIFY: cap bit 0x4000000 unnamed |
| drivebullsquid.cpp | 0x144240 | CDriveBullsquid::HandleAnimEvent | ok |
| drivebullsquid.cpp | 0x1446f0 | CDriveBullsquid::DyingThink | ok |
| drivebullsquid.cpp | 0x1447b0 | CDriveBullsquid::SetDeathAnim | ok |
| drivebullsquid.cpp | 0x144810 | CDriveBullsquid::TakeDamage | ok |
| drivebullsquid.cpp | 0x1449f0 | CDriveBullsquid::Killed | ok |
| drivebullsquid.cpp | 0x144da0 | CDriveBullsquid::DEClientPreThink | ok |
| drivebullsquid.cpp | 0x145310 | CDriveBullsquid::DEClientPostThink | ok |
| drivebullsquid.cpp | 0x145330 | CDriveBullsquid::DEClientEnter | ok |
| drivebullsquid.cpp | 0x1456e0 | CDriveBullsquid::DEClientLeave | ok |
| drivebullsquid.cpp | 0x1459f0 | CDriveBullsquid::DEClientEject | ok |
| drivebullsquid.cpp | 0x145c00 | LINK_ENTITY_TO_CLASS(pickup_drive_bullsquid) | ok |
| drivebullsquid.cpp | 0x145c30 | CDriveBullsquidPickup::Save | ok |
| drivebullsquid.cpp | 0x145c80 | CDriveBullsquidPickup::Restore | ok |
| drivebullsquid.cpp | 0x145cd0 | CDriveBullsquidPickup::Spawn | ok |
| drivebullsquid.cpp | 0x145f10 | CDriveBullsquidPickup::Precache | ok |
| drivebullsquid.cpp | 0x145f60 | CDriveBullsquidPickup::GetSpawnOrg | ok |
| drivebullsquid.cpp | 0x145fe0 | CDriveBullsquidPickup::TakeDamage | ok |
| drivebullsquid.cpp | 0x145ff0 | CDriveBullsquidPickup::PAnimThink | ok |
| drivebullsquid.cpp | 0x146190 | CDriveBullsquidPickup::Use | VERIFY: tests pCaller |
| drivebullsquid.cpp | 0x146390 | CDriveBullsquidPickup::DoRespawn | ok |
| drivebullsquid.cpp | 0x146450 | CDriveBullsquidPickup::DeathNotice | ok |
| drivebullsquid.cpp | 0x1464c0 | CreateDrvBullsquidSpawner | ok |
| drivebullsquid.cpp | 0x146520 | LINK_ENTITY_TO_CLASS(drive_bullsquid_spawner) | ok |
| drivebullsquid.cpp | 0x146550 | CDrvBullsquidSpawner::Save | ok |
| drivebullsquid.cpp | 0x1465a0 | CDrvBullsquidSpawner::Restore | ok |
| drivebullsquid.cpp | 0x1465f0 | CDrvBullsquidSpawner::DrvAThink | ok |
| drivebullsquid.cpp | 0x146680 | CDrvBullsquidSpawner::Spawn | ok |
| drivebullsquid.cpp | 0x146720 | DBSFireChumtoadifer | VERIFY: FadeThink params; calls stubs |
| drivebullsquid.cpp | 0x146a50 | CDrvBullsquidSpawner::ObjectCaps | ok |
| drivebullsquid.cpp | 0x146ab0 | CDriveBullsquidPickup::ObjectCaps | ok |
| drivebullsquid.cpp | 0x146ad0 | CDriveBullsquidPickup::GetRCCaps | ok |
| drivebullsquid.cpp | 0x146ae0 | CDriveBullsquidPickup::IsTeleFraggabe | ok |
| drivebullsquid.cpp | 0x146af0 | CDriveBullsquidPickup::BloodColor | ok |
| drivebullsquid.cpp | 0x146b00 | CDriveBullsquidPickup::Classify | ok |
| drivebullsquid.cpp | 0x146b60 | CDriveBullsquid::EntIsBeingRemoved | ok |
| drivebullsquid.cpp | 0x146be0 | CDriveBullsquid::GetRCCaps | ok |
| drivebullsquid.cpp | 0x146bf0 | CDriveBullsquid::GetGuassDistOut | ok |
| drivebullsquid.cpp | 0x146c00 | CDriveBullsquid::GetGuassHull | ok |
| drivebullsquid.cpp | 0x146c10 | CDriveBullsquid::GetCamRotOrg | ok |
| drivebullsquid.cpp | 0x146c70 | CDriveBullsquid::BloodColor | ok |
| drivebullsquid.cpp | 0x146c80 | CDriveBullsquid::Classify | ok |
| drivepanth.cpp | 0x16ee10 | LINK_ENTITY_TO_CLASS(drive_panth) | ok |
| drivepanth.cpp | 0x16ee40 | CDrivePanth::Spawn | ok |
| drivepanth.cpp | 0x16f100 | CDrivePanth::Precache | ok |
| drivepanth.cpp | 0x16f260 | CDrivePanth::GetControllerName | ok |
| drivepanth.cpp | 0x16f2d0 | CDrivePanth::DPThink | VERIFY: classname alien_slave on driver loss (kept) |
| drivepanth.cpp | 0x16f740 | CDrivePanth::CheckGround | ok |
| drivepanth.cpp | 0x16f760 | CDrivePanth::JumpTouch | VERIFY: cap bit 0x80000 unnamed |
| drivepanth.cpp | 0x16f980 | CDrivePanth::DoJump | ok |
| drivepanth.cpp | 0x16fba0 | CDrivePanth::DoClaw | ok |
| drivepanth.cpp | 0x16fe20 | CDrivePanth::CheckDoor | ok |
| drivepanth.cpp | 0x170070 | CDrivePanth::CheckEHop | ok |
| drivepanth.cpp | 0x170390 | CDrivePanth::RunWaterPhysics | VERIFY: SlaveWaterFriction key; buoyancy adds |
| drivepanth.cpp | 0x1706f0 | CDrivePanth::DoMovement | VERIFY: airborne run/walk speed bands; restore-anim test |
| drivepanth.cpp | 0x170f60 | CDrivePanth::DoYaw | VERIFY: time added twice |
| drivepanth.cpp | 0x1711d0 | CDrivePanth::DoRegen | ok |
| drivepanth.cpp | 0x1712e0 | CDrivePanth::UpdateOwnerHUD | ok |
| drivepanth.cpp | 0x171400 | CDrivePanth::ClawDamage | VERIFY: cap bit 0x100000 unnamed |
| drivepanth.cpp | 0x171ab0 | CDrivePanth::HandleAnimEvent | VERIFY: events 5/6 case 0 sound |
| drivepanth.cpp | 0x171fa0 | CDrivePanth::SetRandomAnim | ok |
| drivepanth.cpp | 0x172080 | CDrivePanth::DyingThink | ok |
| drivepanth.cpp | 0x172140 | CDrivePanth::SetDeathAnim | ok |
| drivepanth.cpp | 0x1722e0 | CDrivePanth::TakeDamage | ok |
| drivepanth.cpp | 0x1724c0 | CDrivePanth::Killed | VERIFY: classname alien_slave |
| drivepanth.cpp | 0x172870 | CDrivePanth::DEClientPreThink | ok |
| drivepanth.cpp | 0x172da0 | CDrivePanth::DEClientPostThink | ok |
| drivepanth.cpp | 0x172dc0 | CDrivePanth::DEClientEnter | ok |
| drivepanth.cpp | 0x173170 | CDrivePanth::DEClientLeave | ok |
| drivepanth.cpp | 0x173480 | CDrivePanth::DEClientEject | ok |
| drivepanth.cpp | 0x173690 | LINK_ENTITY_TO_CLASS(pickup_drive_panth) | ok |
| drivepanth.cpp | 0x1736c0 | CDrivePanthPickup::Save | ok |
| drivepanth.cpp | 0x173710 | CDrivePanthPickup::Restore | ok |
| drivepanth.cpp | 0x173760 | CDrivePanthPickup::Spawn | ok |
| drivepanth.cpp | 0x173990 | CDrivePanthPickup::Precache | ok |
| drivepanth.cpp | 0x1739e0 | CDrivePanthPickup::GetSpawnOrg | ok |
| drivepanth.cpp | 0x173a60 | CDrivePanthPickup::TakeDamage | ok |
| drivepanth.cpp | 0x173a70 | CDrivePanthPickup::PAnimThink | ok |
| drivepanth.cpp | 0x173c10 | CDrivePanthPickup::Use | VERIFY: tests pCaller |
| drivepanth.cpp | 0x173e10 | CDrivePanthPickup::DoRespawn | ok |
| drivepanth.cpp | 0x173ed0 | CDrivePanthPickup::DeathNotice | ok |
| drivepanth.cpp | 0x173f40 | CreateDrvPanthSpawner | ok |
| drivepanth.cpp | 0x173fa0 | LINK_ENTITY_TO_CLASS(drive_panth_spawner) | ok |
| drivepanth.cpp | 0x173fd0 | CDrvPanthSpawner::Save | ok |
| drivepanth.cpp | 0x174020 | CDrvPanthSpawner::Restore | ok |
| drivepanth.cpp | 0x174070 | CDrvPanthSpawner::DrvAThink | ok |
| drivepanth.cpp | 0x174100 | CDrvPanthSpawner::Spawn | ok |
| drivepanth.cpp | 0x1741a0 | CDrvPanthSpawner::ObjectCaps | ok |
| drivepanth.cpp | 0x174200 | CDrivePanthPickup::ObjectCaps | ok |
| drivepanth.cpp | 0x174220 | CDrivePanthPickup::GetRCCaps | ok |
| drivepanth.cpp | 0x174230 | CDrivePanthPickup::IsTeleFraggabe | ok |
| drivepanth.cpp | 0x174240 | CDrivePanthPickup::BloodColor | ok |
| drivepanth.cpp | 0x174250 | CDrivePanthPickup::Classify | ok |
| drivepanth.cpp | 0x1742b0 | CDrivePanth::EntIsBeingRemoved | ok |
| drivepanth.cpp | 0x174330 | CDrivePanth::GetRCCaps | ok |
| drivepanth.cpp | 0x174340 | CDrivePanth::GetGuassDistOut | ok |
| drivepanth.cpp | 0x174350 | CDrivePanth::GetGuassHull | ok |
| drivepanth.cpp | 0x174360 | CDrivePanth::GetCamRotOrg | ok |
| drivepanth.cpp | 0x1743c0 | CDrivePanth::BloodColor | ok |
| drivepanth.cpp | 0x1743d0 | CDrivePanth::Classify | ok |
| driveturret.cpp | 0x174430 | LINK_ENTITY_TO_CLASS(drive_turret) | ok |
| driveturret.cpp | 0x174460 | CDriveTurret::Spawn | ok |
| driveturret.cpp | 0x174aa0 | CDriveTurret::Precache | ok |
| driveturret.cpp | 0x174b90 | CDriveTurret::GetControllerName | ok |
| driveturret.cpp | 0x174c00 | CDriveTurret::Classify | ok |
| driveturret.cpp | 0x174c20 | CDriveTurret::GetCamRotOrg | ok |
| driveturret.cpp | 0x174c90 | CDriveTurret::KillThink | ok |
| driveturret.cpp | 0x174d00 | CDriveTurret::DTThink | ok |
| driveturret.cpp | 0x174e90 | CDriveTurret::DoGun | ok |
| driveturret.cpp | 0x1753c0 | CDriveTurret::DoPing | ok |
| driveturret.cpp | 0x175580 | CDriveTurret::EyeUpdate | ok |
| driveturret.cpp | 0x1757c0 | CDriveTurret::UpdateOwnerHUD | ok |
| driveturret.cpp | 0x175a30 | CDriveTurret::UpdateBarrel | ok |
| driveturret.cpp | 0x175b50 | CDriveTurret::UpdateSpin | ok |
| driveturret.cpp | 0x175bf0 | CDriveTurret::CheckRetract | ok |
| driveturret.cpp | 0x175da0 | CDriveTurret::MoveTurret | ok |
| driveturret.cpp | 0x175e80 | CDriveTurret::RunBarrel | ok |
| driveturret.cpp | 0x1760e0 | CDriveTurret::TuFireBullets | ok |
| driveturret.cpp | 0x176650 | CDriveTurret::AttachToPlane | ok |
| driveturret.cpp | 0x1766e0 | CDriveTurret::TraceAttack | ok |
| driveturret.cpp | 0x176860 | CDriveTurret::TakeDamage | VERIFY: unguarded m_pOwner |
| driveturret.cpp | 0x176a30 | CDriveTurret::DyingThink | VERIFY: ExploCreate is a stub |
| driveturret.cpp | 0x177030 | CDriveTurret::SetTurretAnim | ok |
| driveturret.cpp | 0x177100 | CDriveTurret::Killed | ok |
| driveturret.cpp | 0x177410 | CDriveTurret::DEClientPreThink | VERIFY: PlayerDoID is a stub |
| driveturret.cpp | 0x177a60 | CDriveTurret::DEClientPostThink | ok |
| driveturret.cpp | 0x177a80 | CDriveTurret::DEClientEnter | ok |
| driveturret.cpp | 0x178020 | CDriveTurret::DEClientLeave | VERIFY: no SET_VIEW back |
| driveturret.cpp | 0x1782f0 | CDriveTurret::DEClientEject | ok |
| driveturret.cpp | 0x1784a0 | LINK_ENTITY_TO_CLASS(pickup_drive_turret) | ok |
| driveturret.cpp | 0x1784d0 | CDriveTurretPickup::Save | ok |
| driveturret.cpp | 0x178520 | CDriveTurretPickup::Restore | ok |
| driveturret.cpp | 0x178570 | CDriveTurretPickup::Spawn | ok |
| driveturret.cpp | 0x178a80 | CDriveTurretPickup::Precache | ok |
| driveturret.cpp | 0x178ad0 | CDriveTurretPickup::GetSpawnOrg | ok |
| driveturret.cpp | 0x178b50 | CDriveTurretPickup::TakeDamage | ok |
| driveturret.cpp | 0x178b60 | CDriveTurretPickup::PAnimThink | ok |
| driveturret.cpp | 0x178d00 | CDriveTurretPickup::Use | VERIFY: tests pCaller |
| driveturret.cpp | 0x178f10 | CDriveTurretPickup::DoRespawn | ok |
| driveturret.cpp | 0x178fd0 | CDriveTurretPickup::DeathNotice | ok |
| driveturret.cpp | 0x179040 | CreateDrvTurretSpawner | ok |
| driveturret.cpp | 0x1790a0 | LINK_ENTITY_TO_CLASS(drive_turret_spawner) | ok |
| driveturret.cpp | 0x1790d0 | CDrvTurretSpawner::Save | ok |
| driveturret.cpp | 0x179120 | CDrvTurretSpawner::Restore | ok |
| driveturret.cpp | 0x179170 | CDrvTurretSpawner::DrvAThink | ok |
| driveturret.cpp | 0x179200 | CDrvTurretSpawner::Spawn | ok |
| driveturret.cpp | 0x1792a0 | CDrvTurretSpawner::ObjectCaps | ok |
| driveturret.cpp | 0x179300 | CDriveTurretPickup::ObjectCaps | ok |
| driveturret.cpp | 0x179320 | CDriveTurretPickup::GetRCCaps | ok |
| driveturret.cpp | 0x179330 | CDriveTurretPickup::IsTeleFraggabe | ok |
| driveturret.cpp | 0x179340 | CDriveTurretPickup::BloodColor | ok |
| driveturret.cpp | 0x179350 | CDriveTurretPickup::Classify | ok |
| driveturret.cpp | 0x1793b0 | CDriveTurret::EntIsBeingRemoved | ok |
| driveturret.cpp | 0x179480 | CDriveTurret::VeryHeavyDamageFract | ok |
| driveturret.cpp | 0x1794a0 | CDriveTurret::ExtreemeDamageFract | ok |
| driveturret.cpp | 0x1794c0 | CDriveTurret::GetRCCaps | ok |
| driveturret.cpp | 0x1794d0 | CDriveTurret::BloodColor | ok |

## 4. Classes

None of the three drive monsters has a save table, so every member of theirs is unsaved and
named from what the code does. The driver at orig +0x27c is `CBaseDriveEnt::m_pOwner`
(coordinator decision: it moved to the base class). Every member carries its original offset in a
comment in the header.

- `CDrivePanth` (base `CBaseDriveEnt`). Members 0x280-0x2a8 take the names of `CDriveASlave`'s
  save table, which has the same layout there (`m_fFrameTime`, `m_vecSudoVel`, `m_fLastMoveTime`,
  `m_fNextEHopCheck`, `m_fNextDoorCheck`, `m_fNextIdleTime`, `m_fNextPainSound`,
  `m_fIdleSoundTime`); 0x29c and 0x2ac-0x2e4 named from use (`m_bCan*` gating flags, head-yaw
  controller, jump/claw state, regen timers, `m_iClientHealth`). 0x2cc and 0x2e0 are only set in
  Spawn (`m_fUnknown2cc`, `m_fUnknown2e0`), 0x2d0 is set and never read (`m_bMainClaw`).
  Types `sDPSoundT` (sound, min pitch, max pitch) and `sDPAnimT` (sequence, weight, min, max
  time); `sDPAnimT` is the original's name (from `SetRandomAnim`'s mangled signature),
  `sDPSoundT` is ours.
- `CDriveBullsquid` (base `CBaseDriveEnt`). 0x280-0x2c4: same naming scheme; three attack
  states (bite, whip, chumtoadifier) each with an in-progress flag and a refire time.
- `CDriveTurret` (base `CBaseDriveEnt`). 0x280-0x2f4 and 0x308 (`m_pViewEnt`, the camera
  entity). 0x2f8-0x304 are not touched by any function in the file. Enum `DTURRET_ANIM` (the
  original's type name, from `SetTurretAnim`'s signature; member names ours, after turret.cpp).
- `CDrivePanthPickup`, `CDriveBullsquidPickup` (base `CBaseMonster`): saved `m_pGiven`
  (FIELD_CLASSPTR), `m_bTakeable` (FIELD_BOOLEAN), in the original's order. New virtual
  `IsTeleFraggabe` (slot 150, returns false).
- `CDriveTurretPickup` (base `CBaseMonster`): unsaved `m_vecNormal` (orig +0x27c) before the
  same two saved members (orig +0x288, +0x28c).
- `CDrvPanthSpawner`, `CDrvBullsquidSpawner`, `CDrvTurretSpawner` (base `CBaseEntity`): saved
  `m_bDummy` (FIELD_BOOLEAN).

## 5. Cross-file needs

| what | where it is now | belongs in |
|---|---|---|
| `CBaseDriveEnt::m_pOwner` | `dlls/driveent.h`, coordinator's exact line | main (coordinator) |
| `CBasePlayer::m_fDrvEntAlpha` (orig +0x674) | `dlls/player.h`, `// panth-squid-turret needs:` block, coordinator's exact line | main (coordinator) |
| `CBasePlayer::PlayerDoID()` declaration | same block | player.cpp (orig 0x1ee750) |
| `CBeam::FadeThink()` declaration | `dlls/effects.h`, `// panth-squid-turret needs:` block | effects.cpp (orig 0x17a540) |
| `PREFS_KeyLoopUpFloat`, `PREFS_KeyLoopUpBool` | extern in all three files; TEMP STUB (returns the default) at the bottom of `drivepanth.cpp` | Preff.cpp (prefs-votes; signatures match the coordinator's) |
| `DCTChumtoadifyEnt(edict_t*, edict_t*)` | extern + TEMP STUB (does nothing) in `drivebullsquid.cpp` | drivechum.cpp (chum-icky-hc) |
| `CBeam::FadeThink()` | TEMP STUB (does nothing) in `drivebullsquid.cpp` | effects.cpp |
| `CPointExplo::ExploCreate(edict_t*, Vector, Vector, float, int)` | minimal local `class CPointExplo` + TEMP STUB (returns NULL) in `driveturret.cpp` | pointexplo.cpp; drop the local class for its header |
| `CBasePlayer::PlayerDoID()` | TEMP STUB (does nothing) in `driveturret.cpp` | player.cpp |
| `FixedUnsigned16` | file-local copy `DBSFixedUnsigned16` in `drivebullsquid.cpp` (util.cpp's is static; RC's is global, orig 0x232300) | util.cpp, once it exports it |
| `CreateDrvPanthSpawner`, `CreateDrvBullsquidSpawner`, `CreateDrvTurretSpawner` | defined here, declared in the three new headers | callers (gamerules) include those headers |
| `DBSFireChumtoadifer` | defined here, declared in `drivebullsquid.h` | - |

User messages used, all already in `UserMessages.h`: `gmsgHealth`, `gmsgHideWeapon`, `gmsgSetFOV`,
`gmsgBattery`, `gmsgFlashBattery`, `gmsgTrain`, `gmsgShake`. Also `SetupVisibility` (client.h),
`gMultiDamage`, `g_sModelIndexSmoke`, `g_pevLastInflictor`.

## 6. VERIFY list

34 `VERIFY` marks in code. The ones that change behavior:

1. Pickup `Use` (all three) tests `pCaller`, not `pActivator`, and calls `IsPlayer()` before the
   null test. Read from the disassembly (`0x10(%ebp)`); literal.
2. `CDrivePanth` sets its classname to `alien_slave` when its driver is lost and when it dies,
   and reads the water friction from key `SlaveWaterFriction`: strings copied from
   `CDriveASlave` in the original. Kept.
3. `CDrivePanth::DoYaw` sets `m_fNextIdleTime = time + time + RANDOM_FLOAT(3,8)` (disassembly:
   `fadd %st(0)`). Kept.
4. `CDrivePanth::DoMovement`: airborne, 30-120 u/s plays `run` and 120+ plays `walk`; the
   restore-old-anim flag is false when the old sequence was `run`. `CDriveBullsquid::DoMovement`
   tests the other way (true when the old sequence is neither run nor walk). Both checked in the
   disassembly; kept literal.
5. `RunWaterPhysics` (panther, bullsquid) adds `1.1 * sv_gravity * dt` to velocity.z under water
   (floats up). Literal.
6. `CDriveBullsquid::CheckEHop` never advances `m_fNextEHopCheck`, unlike the panther's.
7. `CDriveTurret::TakeDamage` writes `m_pOwner->m_fDrvEntEjectTime` without a null test.
8. `CDriveTurret::DEClientLeave` removes the view entity but does not `SET_VIEW` the player back.
9. `CDrivePanth::HandleAnimEvent` events 5/6: case 0 of that jump table is misdisassembled;
   taken as `player/pl_step1.wav` like the other two tables (cases 1-3 read cleanly).
10. RC cap bits with no names yet: 0x80000 (pounce knockback), 0x100000 (claw knockback),
    0x4000000 (bite/whip knockback). Settled by whoever names the RC caps.
11. `DBSFireChumtoadifer` sets `armorvalue`, `teleport_time`, `armortype`, `team` for
    `CBeam::FadeThink`; their meaning is effects.cpp's to settle.
12. Member names throughout (all unsaved) and `sDPSoundT`, `DTURRET_ANIM_*`, `m_fDrvEntAlpha`.

Settled by: an engine run against the original for 1-9, effects.cpp / cap-name reconstruction
for 10-11.

## 7. Behavior worth testing

- Get in each pickup (`+use` on the ghost), drive, eject (`sv_allowglobaleject 1` or the
  `AllowEject` pref): the player reappears 48 units above (panther, bullsquid) or 72 out from the
  surface (turret), and the monster dies; respawn after `*RespawnDelayAfterDeath`.
- Max drive time (`PanthMaxDriveTime` 600, `BullsquidMaxDriveTime` 600, `TurretMaxDriveTime` 420)
  kills the monster.
- Panther: claws (attack, alternating sides), double claw (attack2), pounce (jump) with damage
  above 500 u/s, regen after 6 s still; swimming.
- Bullsquid: bite heals 25, whip knocks sideways, jump fires the chumtoadifier (needs drivechum).
- Turret: only survives on a world floor or ceiling (else self-destructs as `turret_self_dest`);
  attack2 deploys/retracts, attack spins up then fires, ammo shown as armor and regenerating
  while retracted; driver's view is the `turret_viewent`; death explosion (needs pointexplo).
- The camera: third person 160 (panther) / 180 (bullsquid) behind, `driveentalpha` setinfo
  making the monster see-through for its driver (needs the setinfo read in player.cpp).
