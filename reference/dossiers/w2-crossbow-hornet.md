# Dossier: w2-crossbow-hornet

Addresses are file addresses (funcfile.tsv; Ghidra's are 0x10000 higher), and every function's
`// orig` comment uses them.

## 1. Scope

Original source files: `crossbow.cpp` (53 functions), `hornetgun.cpp` (34), `hornet.cpp` (22),
plus the `(header)` rows whose classes live in these files: `CCrossbowAmmo` (3, ammo_crossbow) and
`CFreezeGreande::ProMagApplyForce`/`ProMagEffected` (2).

| File | What |
|---|---|
| `dlls/crossbow.cpp` | rewritten: `CImpArrow` (impail_arrow), `CCrossbowSparkTracer`, `CXBowSmokeTracer`, `CCrossbow`, `CCrossbowAmmo`. The SDK's `CCrossbowBolt` (crossbow_bolt) is gone: the original has neither the class nor the entity. |
| `dlls/hornetgun.cpp` | rewritten: `CFreezeTimer` (freeze_timer), `DestroyFreezeTimer`, `CFreezeGreande`, `CHgun` as RC's freeze gun |
| `dlls/hornetgun.h` | new: `DestroyFreezeTimer` for player.cpp |
| `dlls/hornet.cpp`, `dlls/hornet.h` | the 7 `CHornet::Is*` virtuals; Spawn's fire sound back; provenance comments |
| `dlls/weapons.h` | `CCrossbow` and `CHgun` declarations only (members, RC methods, bot overrides); the event members `m_usCrossbow`, `m_usCrossbow2`, `m_usHornetFire` removed (the original plays sounds, no events) |

## 2. Status

Built: `scripts/build.sh i386` and `scripts/build.sh amd64` both compile and link, no warnings
from these files. Nothing has run in an engine.

```
crossbow.cpp    53 of 53 present, 0 missing
hornetgun.cpp   34 of 34 present, 0 missing
hornet.cpp      22 of 22 present, 0 missing
```

`nm -D build-i386/dlls/hl.so`: `impail_arrow`, `freeze_timer`, `weapon_crossbow`,
`weapon_hornetgun`, `ammo_crossbow`, `hornet` exported; `crossbow_bolt` no longer is.

The client target (`scripts/build.sh i386 client`) already fails on this branch before any of
these changes (`cl_dll/particleman/CMiniMem.h`: no `<memory_resource>` in the buildchain's
compiler). The RC-only server code in these files sits under `#ifndef CLIENT_DLL` where it calls
server-only code, but the client build was not tried with it.

### Timers

Coordinator decision (wave 2): the original has no `UseDecrement` (its SDK predates client weapon
prediction), so `CCrossbow` and `CHgun` no longer override it (the base returns false) and their
weapon timers (`m_flNextPrimaryAttack`, `m_flNextSecondaryAttack`, `m_flTimeWeaponIdle`) and
private timers are absolute `gpGlobals->time + x`, as the original writes them. The one exception
is the player's `m_flNextAttack`: `CBasePlayer::PostThink` decrements it every frame under
`CLIENT_WEAPONS` whatever the weapon, and the shared `DefaultReload` writes it relative, so the
Holster writes here stay `UTIL_WeaponTimeBase() + 0.5` (same behavior). `CFreezeTimer` sets a
frozen thing's `m_flNextAttack` relative for a player and absolute for a monster
(`NextAttackBase`). If player.cpp's package makes `m_flNextAttack` absolute, those writes change
with it.

## 3. Function map

All functions were compared against the decompile; the ones marked (asm) were also read in the
disassembly. halflife-updated differences are listed under the function.

### crossbow.cpp

| orig | function | status |
|---|---|---|
| 0x10e050 | `impail_arrow` (LINK_ENTITY_TO_CLASS) | ok |
| 0x10e080 / 0x10e0d0 | `CImpArrow::Save` / `Restore` | ok; base is CGrenade (= CBaseMonster's, as the original calls) |
| 0x10e120 | `CImpArrow::Precache` | ok |
| 0x10e180 | `CImpArrow::PlayHitSound` | ok |
| 0x10e200 | `CImpArrow::Spawn` | ok |
| 0x10e4d0 | `CImpArrow::ForceVeloThink` | ok |
| 0x10e550 | `CImpArrow::IABecomeSolid` | ok |
| 0x10e5d0 | `CImpArrow::IABoom` | ok; keeps the discarded `RANDOM_FLOAT(0,1)` |
| 0x10e930 | `CImpArrow::Killed` | ok |
| 0x10e950 | `CImpArrow::GetNextAttachedToEnt` | ok; no caller in the original |
| 0x10ea10 | `CImpArrow::CalcToOrigin` | ok (returns a Vector; the decompile shows the hidden return pointer) |
| 0x10eaf0 | `CImpArrow::IAAttchedThink` | ok (asm: Ghidra split it into `FUN_` fragments) |
| 0x10f1b0 | `CImpArrow::IATouch` | ok |
| 0x10f800 | `CCrossbowSparkTracer::CreateST` | ok; no caller in the original |
| 0x10f880 | `CCrossbowSparkTracer::Spawn` | ok |
| 0x10f930 | `CCrossbowSparkTracer::MakeSparks` | ok |
| 0x10fac0 | `CCrossbowSparkTracer::STThink` | ok; keeps the "El: %f WS: %i" console spam |
| 0x10fdb0 | `CXBowSmokeTracer::Spawn` | ok |
| 0x10fe60 | `CXBowSmokeTracer::Precache` | ok |
| 0x10fea0 | `CXBowSmokeTracer::BounceThink` | ok |
| 0x10ffe0 | `CXBowSmokeTracer::CreateTR` | ok |
| 0x110280 | `CXBowSmokeTracer::CreateTRBounce` | ok |
| 0x110550 | `weapon_crossbow` | ok |
| 0x110580 | `CCrossbow::Spawn` | ok; `PickupAmmo` pref |
| 0x110600 | `CCrossbow::AddToPlayer` | changed: the original sends WeapPickup here; halflife-updated sends it from `CBasePlayer::AddPlayerItem` for every weapon (and not while spawning), so only the base call is left |
| 0x110690 | `CCrossbow::Precache` | ok; no events, precaches impail_arrow and the hit sounds |
| 0x110760 | `CCrossbow::GetItemInfo` | ok; `MaxAmmo` (80) and `ClipSize` (5) prefs |
| 0x110810 | `CCrossbow::Deploy` | ok (stock) |
| 0x110860 | `CCrossbow::Holster` | ok; zoom state from `m_fInZoom` |
| 0x1108e0 | `CCrossbow::PrimaryAttack` | ok; zoom picks the sniper shot in every game mode |
| 0x110920 | `CCrossbow::PaintDecal` | ok, crash guard |
| 0x110a50 | `CCrossbow::FireSniperBolt` | ok |
| 0x110ac0 | `CCrossbow::DoSniperFire` | ok (asm, whole function), VERIFY (bounce), crash guard |
| 0x111930 | `CCrossbow::FireBolt` | ok |
| 0x111db0 | `CCrossbow::SecondaryAttack` | ok |
| 0x111e50 | `CCrossbow::Reload` | ok; no `ammo_bolts` early-out (halflife-updated's), DefaultReload(5) |
| 0x111f00 | `CCrossbow::WeaponIdle` | ok; RANDOM_FLOAT, not the shared seed |
| 0x112050 | `ammo_crossbow` | ok |
| 0x112080 | `CCrossbow::GetBotWeight` | ok |
| 0x1120e0 | `CCrossbow::BotFireButton` | ok |
| 0x112120 | `CCrossbow::BotFireSecondary` | ok |
| 0x112130 | `CCrossbow::BotFirePrimary` | ok, VERIFY (bot field +0x6a0) |
| 0x1121e0 | `CCrossbow::iItemSlot` | ok (stock, inline) |
| 0x1122e0 | `CImpArrow::GetRealOwner` | ok |
| 0x1122f0 | `CImpArrow::GetPullOrg` | ok |
| 0x112380 | `CImpArrow::EntIsBeingRemoved` | ok |
| 0x112440 | `CImpArrow::BotTargetRealseable` | ok |
| 0x1124a0 | `CImpArrow::GetBotTargetWeight` | ok |
| 0x112510 | `CImpArrow::IsAttachedTo` | ok |
| 0x112570 | `CImpArrow::ProMagApplyForce` | ok (asm: angles from m_vecVelo) |
| 0x112620 | `CImpArrow::ProMagEffected` | ok |
| 0x112670 | `CImpArrow::IsAlive` | ok |
| 0x248490 / 0x248450 / 0x2483a0 | `CCrossbowAmmo::Spawn` / `Precache` / `AddAmmo` | ok; `AddAmmo` gives `ClipGive` (5) up to `MaxAmmo` (80) |

### hornetgun.cpp

| orig | function | status |
|---|---|---|
| 0x1b0900 | `freeze_timer` | ok |
| 0x1b0930 | `CFreezeTimer::CreateOrUpdateTimer` | ok (static) |
| 0x1b0c60 | `DestroyFreezeTimer` | ok |
| 0x1b0d50 | `CFreezeTimer::Spawn` | ok |
| 0x1b0e60 | `CFreezeTimer::DoUse` | ok, two crash guards |
| 0x1b10e0 | `CFreezeTimer::DoThink` | ok (asm: virtual slots) |
| 0x1b1560 | `CFreezeGreande::FreezeDet` | ok (asm: the damage/freeze tail), VERIFY (fract) |
| 0x1b1e60 | `CFreezeGreande::PreDetThink` | ok |
| 0x1b1f50 | `CFreezeGreande::FreezeBounceTouch` | ok |
| 0x1b2210 | `CFreezeGreande::ShootFreezeTimed` | ok |
| 0x24e620 / 0x24e720 | `CFreezeGreande::ProMagApplyForce` / `ProMagEffected` | ok |
| 0x1b24e0 | `weapon_hornetgun` | ok |
| 0x1b2510 | `CHgun::IsHornetGun` | ok |
| 0x1b2520 | `CHgun::IsUseable` | ok (stock) |
| 0x1b2530 | `CHgun::Spawn` | ok; `PickupAmmo` (24) |
| 0x1b25b0 | `CHgun::Precache` | ok; freeze sprites, can.mdl, gauss2.wav, no event |
| 0x1b2680 | `CHgun::AddToPlayer` | changed: WeapPickup as for the crossbow; the multiplayer refill is set after the base call (the original's order), see section 7 |
| 0x1b2760 | `CHgun::GetItemInfo` | ok; `MaxAmmo` (24), weight 10 (halflife-updated's HORNETGUN_WEIGHT is 15) |
| 0x1b2810 | `CHgun::Deploy` | ok |
| 0x1b2860 | `CHgun::Holster` | ok; resets the charge and zoom |
| 0x1b2930 | `CHgun::DoFreezeStuff` | ok |
| 0x1b3400 | `CHgun::FireFreeze` | ok (asm: the wall-punch loop's compares) |
| 0x1b3c50 | `CHgun::PrimaryAttack` | ok (asm: the 0.5 constant), VERIFY (charge formula) |
| 0x1b4180 | `CHgun::StartPrimaryFire` | ok |
| 0x1b4320 | `CHgun::WeaponAttackIdle` | ok |
| 0x1b4520 | `CHgun::FinishSecondaryAttack` | ok |
| 0x1b48d0 | `CHgun::SecondaryAttack` | ok |
| 0x1b49d0 | `CHgun::ReloadAmmo` | ok; no TabulateAmmo (halflife-updated's Reload had one) |
| 0x1b4af0 | `CHgun::Reload` | ok; RC: zoom toggle |
| 0x1b4b80 | `CHgun::WeaponIdle` | ok |
| 0x1b4c70 | `CHgun::BotWeaponFrame` | ok (asm: IN_FORWARD at >= 1024), crash guard, VERIFY (bot gate) |
| 0x1b4db0 | `CHgun::BotFireSecondary` | ok |
| 0x1b4dc0 | `CHgun::BotFirePrimary` | ok |
| 0x1b4e90 | `CHgun::iItemSlot` | ok (stock, inline) |
| 0x1b4ef0 | `CFreezeTimer::PlayerWasKilled` | ok |

### hornet.cpp

All stock functions compared against the decompile; `TrackTarget` (Ghidra kept only a 16-byte
stub) was compared by its disassembly's call and compare sequence.

| orig | function | status |
|---|---|---|
| 0x1af4c0 | `hornet` | ok |
| 0x1af4f0 / 0x1af540 | `CHornet::Save` / `Restore` | ok (IMPLEMENT_SAVERESTORE, 3 fields as savefields.tsv) |
| 0x1af590 | `CHornet::TakeDamage` | ok (stock) |
| 0x1af5d0 | `CHornet::Spawn` | changed: the original plays agrunt/ag_fire1-3 on the owner (CHAN_WEAPON); halflife-updated had removed it. Restored. |
| 0x1af840 | `CHornet::Precache` | ok (stock) |
| 0x1af950 | `CHornet::IRelationship` | ok (stock) |
| 0x1af990 | `CHornet::Classify` | ok (stock) |
| 0x1af9c0 | `CHornet::StartTrack` | ok (stock) |
| 0x1afa30 | `CHornet::StartDart` | ok (stock) |
| 0x1afaa0 | `CHornet::IgniteTrail` | ok (stock) |
| 0x1afbf0 | `CHornet::TrackTarget` | ok (stock, asm) |
| 0x1b0540 | `CHornet::TrackTouch` | ok (stock) |
| 0x1b06e0 | `CHornet::DartTouch` | ok (stock) |
| 0x1b0710 | `CHornet::DieTouch` | crash guard: halflife-updated's owner check (section 8) |
| 0x1b0840-0x1b08a0 | `CHornet::IsFreezeable`, `IsTeatherToable`, `IsCrowbarReloadMoveable`, `IsImpailable` (false), `IsTractorBeamMoveable`, `IsShotgunRiftMoveable`, `IsGravGreandeMoveable` (true) | ok; new virtuals on CHornet in the original's slot order (150-156) |

## 4. Classes

| Class | Base | Members (orig offset) | Source of names |
|---|---|---|---|
| `CImpArrow` | CGrenade | `m_pAttched` EHANDLE +0x280, `m_vecVelo` +0x288, `m_flBDTime` +0x298, `m_vecAttOrgVAng` +0x29c, `m_vecAttOrgAng` +0x2a8, `m_pOwn` edict +0x2b4, `m_flLastFrame` +0x2bc, `m_blHaveLinkedBeam` +0x2c0, `m_flDieTime` +0x2c4 | save table |
| | | `m_pBeam` CBeam* +0x294 | code (the rope beam). VERIFY name |
| `CCrossbowSparkTracer` | CBaseEntity | `m_flLastThink` float +0x34 | code. VERIFY name |
| `CXBowSmokeTracer` | CBaseEntity | `m_iSmoke` int +0x34 | code (smoke.spr index) |
| `CCrossbow` | CBasePlayerWeapon | `m_fInZoom` int +0x8c (SDK 2.0's name), `m_flSniperFireTime` +0x90, `m_bSniperFiring` bool +0x94, `m_iSmoke` +0x98 | code; the last three VERIFY names |
| `CFreezeTimer` | CBaseEntity | `m_vecAngles` +0x34 (the frozen one's v_angle) | code. VERIFY name |
| `CFreezeGreande` | CGrenade | `m_vecMaxAVel` +0x280 | code. VERIFY name |
| `CHgun` | CBasePlayerWeapon | stock `m_flNextAnimTime` +0x8c, `m_flRechargeTime` +0x90, `m_iFirePhase` +0x94 (RC: charging flag); RC `m_flNextAmmoDrain` +0x98, `m_flChargeStart` +0x9c, `m_flSecondaryEnd` +0xa0, `m_bSecondaryCharge` bool +0xa4, `m_fInZoom` +0xa8, `m_flNextZoomTime` +0xac | code; the RC six VERIFY names except `m_fInZoom` |
| `CHornet` | CBaseMonster | stock | save table |

Virtuals introduced: `CImpArrow::ProMagEffected` and `CFreezeGreande::ProMagEffected` (each its own
slot 151, nothing outside calls them), the seven `CHornet::Is*`. The original has no
`CHgun::Save`/`Restore`; halflife-updated's (saving `m_flRechargeTime`, in `weapons.cpp`) is left,
since weapons.cpp isn't this package's.

RC caps tested: 0x20 (`RC_CAP_IMPALE`: the arrow can carry it), 0x8000 (`RC_CAP_SNIPER_REFLECT`),
0x100 (`RC_CAP_FREEZE`). Names are guesses (VERIFY in the defines).

## 5. Cross-file needs

- **`player.cpp` (w2-player-core):** `CBasePlayer::Killed` calls `DestroyFreezeTimer(pev)` (orig
  0x1b0c60); its worktree calls it with no declaration. It needs `#include "hornetgun.h"`.
- **`agrunt.cpp` (w2-monsters):** halflife-updated plays agrunt/ag_fire1-3 in
  `CAGrunt::HandleAnimEvent`; the original agrunt doesn't (the hornet's Spawn does, restored
  here). Unless agrunt.cpp drops its copy, alien grunts fire with the sound twice.
- **`weapons.cpp`:** `CHgun::m_SaveData`/`IMPLEMENT_SAVERESTORE(CHgun, ...)` aren't in the
  original. Harmless; noted.
- **`player.cpp` (w2-player-core):** `m_flNextAttack` stays relative there (section 2); the
  Holster and freeze writes here follow it.
- Used from elsewhere, all already in the tree: `PREFS_*` (Preff.h), `CGameRules::CanPlayerAnnoy`,
  `GetPullCenter`, `CBaseEntity::HasRCCap`/`ExtreemeDamageFract`, `CBasePlayer::m_fCurseRefireScale`.
- No shared-class members added, no `TEMP STUB`s.

## 6. VERIFY list

| Where | What would settle it |
|---|---|
| crossbow.cpp `RC_CAP_IMPALE`, `RC_CAP_SNIPER_REFLECT`; hornetgun.cpp `RC_CAP_FREEZE` | names only; the RC-cap table (whoever reconstructs `GetRCCaps` overrides) |
| `CCrossbow::DoSniperFire`, reflector second hit: body sound on the reflector and the first trace to TraceAttack | read twice in the asm (`-0x8(%ebp)`, `-0x38(%ebp)`); in-game: what the shot sounds and bleeds like after a bounce |
| `CCrossbow` members `m_flSniperFireTime`, `m_bSniperFiring`, `m_iSmoke`; the bot int at +0x6a0 | names; nothing sets the first two past Spawn |
| `CFreezeGreande::FreezeDet`, ownerless grenade divides by FreezeRealDmgFract | read in the asm (`fdivrs` vs `fmuls`); kept |
| `CHgun::PrimaryAttack` out-of-ammo charge `0.5 * elapsed` vs WeaponAttackIdle's `elapsed / PrimaryChargeTime` | read in the asm (0.5 constant); equal at the default |
| `CHgun` member names (`m_flNextAmmoDrain` ...) and `CFreezeTimer::m_vecAngles`, `CFreezeGreande::m_vecMaxAVel`, `CCrossbowSparkTracer::m_flLastThink` | names |
| hornetgun.cpp `RCBotFireGate` | as weapons.cpp's (same +0x698 float) |
| 8 `VERIFY: guard, the original crashes here` | section 8; runtime against the original only to confirm the crash |

19 VERIFY marks in these files: 9 in crossbow.cpp, 9 in hornetgun.cpp, 1 in hornet.cpp (8 of them
crash guards), plus 8 name marks in the weapons.h declarations.

## 7. Behavior worth testing

- Impaler: fire at a player; he is skewered, bleeds every 0.3-0.5 s (`ImpailerBleedDamage`),
  dragged at the arrow's speed with a rope drawn to him; the arrow explodes after 240 s
  (`ImpailerLife`, 20 for a curse), when he dies, goes invisible, or the shooter leaves. Into a wall
  while carrying: the arrow stops 18 units out, becomes shootable (`ImpailerHealth` 100), and holds
  him there. Shooter is kicked back 600 u/s and the view punched up 7.
- An empty arrow flies 30 s and explodes. Hitting the world or a non-RC-cap thing explodes it.
- Sniper (zoomed fire): smoke line red on a damage hit, blue-purple on a miss; the shooter's spray
  logo appears on the surface just past the hit point. Off a reflector (RC cap 0x8000) the line
  bends and the second leg does 4000 damage.
- Out of bolts after the last shot: the crossbow idles immediately (FireBolt's 0.75 bug).
- Freeze gun: hold primary; ammo drains over 2 s, release fires a blue beam (20-140 damage, half of
  it real) that punches through up to 16 walls of up to 32 units. What it hurts but doesn't kill
  is frozen 3-10 s: blue glow shell, can't move or shoot, view locked, screen tinted. Hold 7 s:
  the shooter freezes himself 10 s and takes 70.
- Freeze grenade: secondary costs 18 ammo; releasing sooner gives a longer fuse (up to 2 s) and a
  slower throw. The blast hurts and freezes within 256 units.
- Hivehand reload toggles a 30 fov zoom. In multiplayer a picked-up hivehand comes full (24).
- Alien grunt hornets: one fire sound per hornet (needs the agrunt.cpp change in section 5).
- Stock HL client with `cl_lw 1`: the client predicts the SDK crossbow and hivehand (bolt and hornet
  fire events, stock animations), which these weapons no longer match; expect mispredicted
  animations and sounds for the shooter, depending on what client.cpp sends as weapon data. With
  `UseDecrement()` false the server sends weapon animations to the shooter too, as the original
  did. Compare against the original with `cl_lw 0` and `1`.

## 8. Original bugs

Guarded (coordinator decision: the smallest check that avoids the crash; each marked
`// VERIFY: guard, the original crashes here`):

- **`CImpArrow` save table lists `m_vecVelo` twice**, the second as `FIELD_CLASSPTR`: saving reads
  the velocity's bits as an entity pointer and dereferences it (crash unless velocity.x is 0), and
  restoring would write an entity pointer over it. Guarded: that entry is left out (commented in
  the table).
- **`CImpArrow::IAAttchedThink`** links the rope through `m_pBeam` without a NULL check
  (`EntIsBeingRemoved` can clear it). Guarded: returns before the link.
- **`CCrossbow::PaintDecal`** calls `IsNetClient()` before its own NULL check of the player.
  Guarded (unreachable in practice: the caller passes the shooter).
- **`CCrossbow::DoSniperFire`** reads `tr.pHit->v.takedamage` with no NULL check (as the SDK's
  FireSniperBolt). Guarded: a NULL pHit takes the no-damage branch.
- **`CFreezeTimer::DoUse` (USE_OFF)** reads `m_pActiveItem` from whatever it froze as if it were a
  player. Guarded by `IsPlayer()`; only players reach USE_OFF, so behavior is the same.
- **`CFreezeTimer::DoUse` (USE_OFF)** writes the render settings and flags through `pev->owner`
  unchecked. Guarded: returns when there is no owner.
- **`CHgun::BotWeaponFrame`** calls `pEnemy->Center()` with no NULL check. Guarded.
- **`CHornet::DieTouch`** passes a NULL attacker to TakeDamage for an ownerless hornet (one placed
  by a map or `.lec`, or carried over a level change), which crashes. Guarded by
  halflife-updated's owner check (such a hornet hits for no damage).

Kept:

- **`CCrossbow::FireBolt` sets `m_flTimeWeaponIdle = 0.75`** without the current time when the
  clip empties, so the idle (and auto-reload check) comes at once.
- **`CCrossbow::DoSniperFire` after a reflector bounce** plays the body hit sound on the reflector,
  not the victim, and passes the first trace (the reflector's hit point and hitgroup) to the
  victim's TraceAttack.
- **`CCrossbow::m_bSniperFiring` is never set true**, so WeaponIdle's delayed sniper shot and the
  guards in Primary/Secondary/Reload are dead code.
- **`CCrossbowSparkTracer` is never created**, and its think prints "El: %f WS: %i" to the console
  every 0.01 s.
- **`CXBowSmokeTracer` sets models/rpgrocket.mdl without precaching it**; the RPG, which
  multiplayer always precaches, does.
- **`CFreezeGreande::FreezeDet` with no owner** divides the damage by FreezeRealDmgFract (doubling
  it at 0.5) where an owned grenade multiplies.
- **`CFreezeTimer::CreateOrUpdateTimer` spawns the new timer twice** (Create, then Spawn again).
  Harmless.
- **`CFreezeTimer::DoThink`** calls CBasePlayer::IsOnLadder on any frozen thing; it only reads
  `pev->movetype`, written inline here. Same behavior.
- **`CHgun::PrimaryAttack` out of ammo while charging** computes the charge as `0.5 * elapsed`
  instead of `elapsed / PrimaryChargeTime`; they differ only when PrimaryChargeTime isn't 2.
