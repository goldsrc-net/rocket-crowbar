# Dossier: controll-aslave

## 1. Scope

Original source files: `drivecontroll.cpp` (79 functions) and `driveaslave.cpp` (85 functions).

Written here: `dlls/drivecontroll.cpp`, `dlls/driveaslave.cpp` (all class declarations are local to
those files; nothing else needs them). Touched: `dlls/driveent.h` (coordinator's `m_pOwner`),
`dlls/player.h` (one member, section 5).

## 2. Status

```
check_file.py drivecontroll.cpp --missing   79 of 79 present, 0 missing
check_file.py driveaslave.cpp --missing     85 of 85 present, 0 missing
scripts/build.sh i386                       builds (no warnings from these files)
scripts/build.sh amd64                      builds
entity exports (i386 nm -D)                 all 13 present
```

Built only. Nothing is checked in an engine against the original.

## 3. Function map

Every function is in the file of the same name. `ok` means read from the decompile (and from the
disassembly where Ghidra lost it) with no open question.

**driveaslave.cpp** (all in `dlls/driveaslave.cpp`)

```
0x0012aa80  asl_charge_flare                             ok
0x0012aab0  alien_slave_energy_ball                      ok
0x0012aae0  alien_slave_sapper                           ok
0x0012ab10  drive_asl                                    ok
0x0012ab40  CDriveASlave::Save                           ok
0x0012ab90  CDriveASlave::Restore                        ok
0x0012abe0  CDriveASlave::Spawn                          VERIFY hand-glow iuser3 = 2 (literal)
0x0012af90  CDriveASlave::Precache                       ok
0x0012b120  CDriveASlave::GetControllerName              ok
0x0012b190  CDriveASlave::DASLThink                      ok
0x0012b6f0  CDriveASlave::DoZap                          ok
0x0012bae0  CDriveASlave::DoBallAttack                   ok
0x0012bb00  CDriveASlave::DoSapAttack                    ok
0x0012bf20  CDriveASlave::DoClaw                         ok
0x0012bfe0  CDriveASlave::CheckDoor                      ok
0x0012c230  CDriveASlave::CheckEHop                      ok
0x0012c520  CDriveASlave::RunWaterPhysics                VERIFY gravity is added (rises in water)
0x0012c880  CDriveASlave::DoMovement                     VERIFY airborne anim picks walk1 when fast; bWasWalking only for walk1
0x0012cf40  CDriveASlave::DoYaw                          VERIFY next idle time adds gpGlobals->time twice
0x0012d0d0  CDriveASlave::UpdateOwnerHUD                 ok
0x0012d1f0  CDriveASlave::AllocBeam                      ok
0x0012d280  CDriveASlave::ClearBeams                     ok
0x0012d320  CDriveASlave::CreateBeamToGeom               VERIFY read from disassembly (Ghidra fragment); decal test uses last trace
0x0012d8b0  CDriveASlave::ChargeupThrust                 ok
0x0012dce0  CDriveASlave::DoZapBeamFire                  ok, returns the bolt end (hidden return in gcc2)
0x0012e020  CDriveASlave::FireZap                        ok
0x0012e7a0  CDriveASlave::EmmitSapBalls                  VERIFY uses stale gpGlobals->v_up
0x0012ebf0  CDriveASlave::ClawDamage                     ok, read from disassembly
0x0012ef00  CDriveASlave::HandleAnimEvent                ok
0x0012f220  CDriveASlave::SetRandomAnim                  ok
0x0012f300  CDriveASlave::DyingThink                     ok
0x0012f3c0  CDriveASlave::SetDeathAnim                   ok
0x0012f4b0  CDriveASlave::TakeDamage                     ok
0x0012f640  CDriveASlave::Killed                         ok
0x0012fa10  CDriveASlave::DEClientPreThink               ok
0x0012ff40  CDriveASlave::DEClientPostThink              ok
0x0012ff60  CDriveASlave::DEClientEnter                  ok
0x00130310  CDriveASlave::DEClientLeave                  ok
0x00130620  CDriveASlave::DEClientEject                  ok
0x00130830  CASLZapChargeSprite::Spawn                   ok
0x00130960  CASLZapChargeSprite::Precache                ok
0x00130990  CASLZapChargeSprite::CreateSS                ok
0x00130a10  CASLZapChargeSprite::SSThink                 ok
0x00130d60  CASLEngBall::Spawn                           ok
0x00130eb0  CASLEngBall::Precache                        ok
0x00130ef0  CASLEngBall::CreateEB                        ok
0x00130fd0  CASLEngBall::EBThink                         VERIFY tr zeroed before use; hit arc goes to last traced point
0x00131b40  CASLEngBall::EBTouch                         ok
0x00131cf0  CASLRechargeBall::Spawn                      ok
0x00131e10  CASLRechargeBall::Precache                   ok
0x00131e60  CASLRechargeBall::CreateRB                   ok
0x00131ef0  CASLRechargeBall::InitThink                  ok
0x001322c0  CASLRechargeBall::CircleLeaveVictem          VERIFY enemy = owner on a dead slave, as written
0x00132a20  CASLRechargeBall::FlyToOwner                 VERIFY null-target guard added where the original calls through it
0x00133010  CASLRechargeBall::RechargeOwner              ok
0x001338e0  pickup_drive_slave                           ok
0x00133910  CDriveSlavePickup::Save                      ok
0x00133960  CDriveSlavePickup::Restore                   ok
0x001339b0  CDriveSlavePickup::Spawn                     ok
0x00133bf0  CDriveSlavePickup::Precache                  ok
0x00133c40  CDriveSlavePickup::GetSpawnOrg               ok
0x00133cc0  CDriveSlavePickup::TakeDamage                ok
0x00133cd0  CDriveSlavePickup::PAnimThink                ok
0x00133e70  CDriveSlavePickup::Use                       ok
0x00134070  CDriveSlavePickup::DoRespawn                 ok
0x00134130  CDriveSlavePickup::DeathNotice               ok
0x001341a0  CreateDrvSlaveSpawner                        ok, caller is CWorld::Precache (world.cpp)
0x00134200  drive_slave_spawner                          ok
0x00134230  CDrvSlaveSpawner::Save                       ok
0x00134280  CDrvSlaveSpawner::Restore                    ok
0x001342d0  CDrvSlaveSpawner::DrvAThink                  ok
0x00134360  CDrvSlaveSpawner::Spawn                      ok
0x00134400  CDrvSlaveSpawner::ObjectCaps                 ok
0x00134460  CDriveSlavePickup::ObjectCaps                ok
0x00134480  CDriveSlavePickup::GetRCCaps                 ok
0x00134490  CDriveSlavePickup::IsTeleFraggabe            ok
0x001344a0  CDriveSlavePickup::BloodColor                ok
0x001344b0  CDriveSlavePickup::Classify                  ok
0x00134510  CDriveASlave::EntIsBeingRemoved              ok
0x00134600  CDriveASlave::GetRCCaps                      ok
0x00134610  CDriveASlave::GetGuassDistOut                ok
0x00134620  CDriveASlave::GetGuassHull                   ok
0x00134630  CDriveASlave::GetCamRotOrg                   ok
0x00134690  CDriveASlave::BloodColor                     ok
0x001346a0  CDriveASlave::Classify                       ok
```

**drivecontroll.cpp** (all in `dlls/drivecontroll.cpp`)

```
0x0014b3f0  drive_controller                             ok
0x0014b420  CDriveController::Spawn                      VERIFY hand-glow sprites get pev->iuser3 = 2 (literal)
0x0014b900  CDriveController::Precache                   ok
0x0014baa0  CDriveController::GetControllerName          ok
0x0014bb10  CDriveController::DCThink                    ok
0x0014bf50  CDriveController::DoThrowBall                ok
0x0014c260  CDriveController::DoChargeAttack             ok
0x0014c6b0  CDriveController::DoKamikaze                 ok (RANDOM_FLOAT(0.8, 0.1) kept as written)
0x0014cca0  CDriveController::LookupMoveSeq              ok
0x0014cdd0  CDriveController::DoMovement                 ok
0x0014d380  CDriveController::DoAngles                   ok
0x0014d470  CDriveController::UpdateOwnerHUD             ok
0x0014d590  CDriveController::ThrowBall                  ok
0x0014d6d0  CDriveController::EmmitChargeBalls           ok
0x0014d9f0  CDriveController::ChargeBigBall              ok
0x0014e320  CDriveController::FireChargedBall            VERIFY unchecked m_pChargedBall, as the original
0x0014e700  CDriveController::KamikazeFrame              ok
0x0014ef50  CDriveController::KamikazeSeqFinished        ok
0x0014f1b0  DCPunchTrace                                 ok, read from disassembly
0x0014f490  CDriveController::KamikazeDetonate           ok, read from disassembly
0x0014fc00  CDriveController::TakeDamage                 VERIFY null-owner guard added on the eject-time write
0x0014fdc0  CDriveController::DyingThink                 ok
0x0014ff30  CDriveController::Killed                     ok
0x00150390  CDriveController::HandleAnimEvent            ok
0x001504a0  CDriveController::DEClientPreThink           VERIFY clears euser3 where the rest uses euser2
0x00150930  CDriveController::DEClientPostThink          ok
0x00150950  CDriveController::DEClientEnter              ok
0x00150d30  CDriveController::DEClientLeave              ok
0x00151040  CDriveController::DEClientEject              ok
0x00151460  pickup_drive_controller                      ok
0x00151490  CDriveControllerPickup::Save                 ok
0x001514e0  CDriveControllerPickup::Restore              ok
0x00151530  CDriveControllerPickup::Spawn                ok
0x00151760  CDriveControllerPickup::Precache             ok
0x001517b0  CDriveControllerPickup::GetSpawnOrg          ok
0x00151830  CDriveControllerPickup::TakeDamage           ok
0x00151840  CDriveControllerPickup::PAnimThink           ok
0x001519e0  CDriveControllerPickup::Use                  ok
0x00151be0  CDriveControllerPickup::DoRespawn            ok
0x00151ca0  CDriveControllerPickup::DeathNotice          ok
0x00151d10  CreateDrvControllerSpawner                   ok, caller is CWorld::Precache (world.cpp)
0x00151d70  drive_controller_spawner                     ok
0x00151da0  CDrvControllerSpawner::Save                  ok
0x00151df0  CDrvControllerSpawner::Restore               ok
0x00151e40  CDrvControllerSpawner::DrvAThink             ok
0x00151ed0  CDrvControllerSpawner::Spawn                 ok
0x00151f70  controller_throw_ball                        ok
0x00151fa0  CDCThrowBall::Spawn                          ok
0x00152190  CDCThrowBall::Precache                       ok
0x001521e0  CDCThrowBall::TBThink                        VERIFY second fork's damage is dmg*0.5, not the pref
0x00152e10  CDCThrowBall::TBTouch                        ok
0x00152ff0  controller_charge_ball                       ok
0x00153020  CDCChargeBall::Spawn                         ok
0x00153170  CDCChargeBall::Precache                      ok
0x001531a0  CDCChargeBall::CBThink                       ok
0x00153740  CDCChargeBall::CBDieQuickThink               ok
0x00153850  controller_charged_ball                      ok
0x00153880  CDCChargeFire::Spawn                         ok
0x00153aa0  CDCChargeFire::Precache                      ok
0x00153b60  CDCChargeFire::CFThink                       VERIFY target test (DAMAGE_AIM or damageable+DEAD_DYING); per-slot traces unused
0x00154930  CDCChargeFire::CFTtouch                      ok
0x00155210  controller_health_ball                       ok
0x00155240  CDCHealthBall::Spawn                         ok
0x00155490  CDCHealthBall::Precache                      ok
0x001554d0  CDCHealthBall::HBThink                       ok
0x00155f50  CDCHealthBall::HBDieQuickThink               ok
0x00156200  CDrvControllerSpawner::ObjectCaps            ok
0x00156260  CDriveControllerPickup::ObjectCaps           ok
0x00156280  CDriveControllerPickup::GetRCCaps            ok
0x00156290  CDriveControllerPickup::IsTeleFraggabe       ok
0x001562a0  CDriveControllerPickup::BloodColor           ok
0x001562b0  CDriveControllerPickup::Classify             ok
0x00156310  CDriveController::EntIsBeingRemoved          ok
0x00156390  CDriveController::GetRCCaps                  ok
0x001563a0  CDriveController::GetGuassDistOut            ok
0x001563b0  CDriveController::GetGuassHull               ok
0x001563c0  CDriveController::GetCamRotOrg               ok
0x00156400  CDriveController::BloodColor                 ok
0x00156410  CDriveController::Classify                   ok
```

Not original functions: `DCFixedUnsigned16` (a copy of util.cpp's static `FixedUnsigned16`, which
the original calls as a global), `DCBeamEnts`/`DCELight` (message helpers), and the wrappers
`CASLZapChargeSprite_CreateSS`/`CASLRechargeBall_CreateRB` (so CDriveASlave can reach static
creators of classes declared after it).

## 4. Classes

Bases are from `hierarchy.tsv`. New virtuals: only the pickups add one (`IsTeleFraggabe`, slot 150,
each pickup declares its own as the original does). `m_pOwner` (+0x27c, the driver) is
`CBaseDriveEnt`'s now; CDriveASlave still saves it first, as the original.

**CDriveController** (CBaseDriveEnt). No save table in the original, so no member is saved and all
names are ours, recovered from code:

```
+0x280 m_fFrameTime          +0x2a8 m_fKamikazeEventTime   +0x2e0 m_pChargedBall
+0x284 m_fNextPainSound      +0x2ac m_iKamikazeStage       +0x2e4 m_fChargeSoundVol
+0x288 m_bSetIdleAnim        +0x2b0 m_fNextIdleSound       +0x2e8 m_iChargeSoundPitch
+0x289 m_bCanMove            +0x2b8 m_pHandGlow[2]         +0x2ec m_fNextKamikazeZap
+0x28a m_bMoving             +0x2c0 m_fHandGlowTarget[2]   +0x2f0 m_fNextKamikazeBeam
+0x28b m_bInThrow            +0x2c8 m_iHandGlowFrames[2]   +0x2f4 m_fKamikazeGlowTarget
+0x28c m_fNextThrow          +0x2d0 m_pChargeBalls[4]      +0x2f8 m_fNextKamikaze
+0x290 m_fThrowReleaseTime   +0x294 m_bInCharge            +0x2fc m_iLastHUDHealth
+0x295 m_bChargeDone         +0x298 m_fNextCharge          +0x300 m_vecEnterOrigin (unread)
+0x29c m_fChargeEventTime    +0x2a0 m_iChargeStage         +0x30c m_fEnterTime (unread)
+0x2a4 m_bInKamikaze
```

**CDriveASlave** (CBaseDriveEnt). Saved members (savefields.tsv, original order, 24 fields incl.
`m_pBeams[16]`). Unsaved, from code: +0x29c `m_fYawSpeedScale`, +0x2ac..0x2b3 the per-think
permission flags `m_bCanIdle, m_bCanMove, m_bCanTurn, m_bCanTurnAnim, m_bCanZap, m_bCanBall,
m_bCanSap` (+0x2b2), `m_bCanClaw` (+0x2b3), +0x2cc `m_bInBallAttack`, +0x2d8 `m_fNextBallAttack`,
+0x330 `m_iLastHUDHealth`. `m_pBeams` is typed `CBeam*`, `m_pHandGlow` `CSprite*` (original
`CBaseEntity*`, same save type).

**CDriveControllerPickup / CDriveSlavePickup** (CBaseMonster): saved `m_pGiven` +0x27c,
`m_bTakeable` +0x280. **CDrvControllerSpawner / CDrvSlaveSpawner** (CBaseEntity): saved `m_bDummy`
+0x34.

**CDCThrowBall, CDCChargeBall, CDCChargeFire, CDCHealthBall** (CBaseMonster): no save tables.
Members from code: ChargeBall +0x27c `m_iFrames` (always 1), +0x280 `m_fLastThink`; ChargeFire
+0x27c `m_iFrames` (11), +0x280 `m_fLastThink`, +0x284 `m_fReturnDamage`, +0x288/28c/290 sprite
indexes (b-tele1 unused, flare6, blueflare2), +0x294 `m_pBeams[8]`; HealthBall +0x27c, +0x280 as
ChargeBall, +0x284 `m_pBeam`. They keep their state in pev fields (weapons as a stage flag, dmg,
dmg_inflictor as the attacker, air_finished, armorvalue); each class comment says which.

**CASLZapChargeSprite, CASLEngBall** (CBaseEntity): no members. **CASLRechargeBall** (CBaseEntity):
+0x34 `m_pBeam`. Their pev field uses are in the class comments.

## 5. Cross-file needs

- `CBasePlayer::m_fDrvEntAlpha` (0x674, unsaved), in a `// controll-aslave needs:` block in
  player.h, as the coordinator asked. It is set by the `cl_de_alpha` client command (client.cpp).
- `CBaseDriveEnt::m_pOwner` added to driveent.h as the coordinator specified.
- Prefs (Preff.cpp): `PREFS_KeyLoopUpFloat/Int/Bool` declared with the coordinator's signatures.
  TEMP STUB bodies returning the default sit at the bottom of `drivecontroll.cpp`
  (`// TEMP STUB (controll-aslave)`); drop them at merge.
- `TEXTURETYPE_PlaySound` declared extern in driveaslave.cpp (sound.cpp, unchanged SDK).
- `CreateDrvControllerSpawner()` and `CreateDrvSlaveSpawner()` are defined here; the original
  calls them from `CWorld::Precache` (world.cpp). Nothing calls them in this tree yet.
- `info_drive_controller_spawn` / `info_drive_slave_spawn` (the pickups' spawn points) are
  subs.cpp's.
- Cvars used by name (`CVAR_GET_FLOAT`, as the original): `mp_drivecontroller`, `mp_driveslave`,
  `sv_allowglobaleject`, `sv_lowlag`, `sv_gravity`; all exist in game.cpp.
- User messages: gmsgHealth, gmsgHideWeapon, gmsgSetFOV, gmsgBattery, gmsgFlashBattery, gmsgTrain,
  gmsgShake (all stock).
- `CBasePlayer::UpdateClientData` is called on eject (original vtable slot 154).

## 6. VERIFY list

37 marks in the code (16 drivecontroll, 21 driveaslave). The ones that change or may change
behavior:

- **Null guards added** where the original dereferences unchecked: CDriveController::TakeDamage
  (eject-time write through a null `m_pOwner`), CASLRechargeBall::FlyToOwner (calls through a
  null target), CASLEngBall::EBThink (`tr` zeroed: the original reads an untouched trace when no
  target was traced). Settle: none; these were crashes or garbage in the original.
- **Kept as the original, looks like a bug**: euser3 cleared instead of euser2
  (DEClientPreThink, controller); slave next-idle time adds `gpGlobals->time` twice (DoYaw); slave
  gains `+1.1*sv_gravity` in water (RunWaterPhysics); airborne anim picks walk1 above 70 and run1
  at 30-70 (DoMovement, checked in the disassembly); `RANDOM_FLOAT(0.8, 0.1)` and
  `RANDOM_FLOAT(0.3, -0.1)` with low > high; second thrown-ball fork uses `dmg*0.5` while the
  first uses the `ControllerThrowBallForkDamageFrac` pref; the charged ball's lasers pass the last
  trace to TraceAttack and store per-target traces nobody reads; CreateBeamToGeom tests the last
  trace's pHit but decals the best trace; EmmitSapBalls uses whatever v_up the last MakeVectors
  left; the charged ball's target test accepts DAMAGE_AIM, or any damageable thing that is
  DEAD_DYING. Settle: run against the original.
- **Hand glows get `pev->iuser3 = 2`** (both monsters): a literal store at the sprite's pev +0x24c.
  Settle: find what reads iuser3 in RC (likely nothing server-side).
- **Prefs signature** `const char*` (the original's are `char*`): match Preff.cpp at merge.
- **Unsaved member names** (section 4) are guesses from use.

## 7. Behavior worth testing

- +use a controller/slave pickup: driver hidden, camera 200 (controller) / 160 (slave) units behind,
  HUD health shows the monster's percentage, movement keys are zeroed client side and restored on
  leave.
- Controller: attack1 throws a homing ball that forks up to 3 times; attack2 charge (hold for more
  bursts, overcharge above 320 hurts the controller) then release a laser ball that sheds health
  balls back to the driver; +use kamikaze: zaps, beams, then a blast of up to 1500 damage within
  1400 units through up to 5 walls 64 units thick, screen shake and fades, driver dies.
- Slave: attack1 hold-to-charge zap (flares fly into the hands, 40 + 40/cycle up to 320, split
  over two bolts, auto-aim cone 0.964); jump sap attack (5 balls per target, each drains 8 and
  carries 10 health back); attack2 claw (20, head hull trace, punchangle on hit); walks with
  WALK_MOVE, hops small ledges, opens doors it walks into.
- Eject (only if `AllowEject` pref or `sv_allowglobaleject 1` or not deathmatch): controller throws
  the driver back at 900 u/s; slave puts the driver 48 units above it. Both monsters then gib.
- Pickups respawn 5 s after their monster dies; the drive ends at 420 s (controller) / 600 s
  (slave). Spawners place `mp_drivecontroller`/`mp_driveslave` pickups 15 s into a multiplayer map.
