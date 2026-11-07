# w2-python-mp5-glock-weapons

## 1. Scope

Original files: python.cpp, mp5.cpp, glock.cpp, weapons.cpp, ggrenade.cpp, func_tank.cpp.

Touched here: `dlls/python.cpp`, `dlls/mp5.cpp`, `dlls/glock.cpp`, `dlls/weapons.cpp`,
`dlls/weapons_shared.cpp` (halflife-updated keeps the original weapons.cpp functions
`CBasePlayerWeapon::ItemPostFrame`, `DefaultReload`, `CanDeploy`, `ResetEmptySound` and
`AddAmmoNameToAmmoRegistry` there), `dlls/ggrenade.cpp`, `dlls/func_tank.cpp`, `dlls/weapons.h`
(only CGrenade, CBasePlayerItem, CBasePlayerWeapon, CWeaponBox, CGlock, CPython, CMP5), and the
new header `dlls/rcbot.h`.

## 2. Status

Built, i386 and amd64. Not run in an engine.

| file | check_file.py |
|---|---|
| python.cpp | 26 of 26, 0 missing |
| mp5.cpp | 25 of 25, 0 missing |
| glock.cpp | 27 of 27, 0 missing |
| weapons.cpp | 109 of 109, 0 missing (9 `inl`: the CBasePlayerItem item-info getters, inline in weapons.h) |
| ggrenade.cpp | 22 of 22, 0 missing |
| func_tank.cpp | 58 of 58, 0 missing |

Entity exports checked with nm: PythRound, acidgib, func_tankgib, func_tankgib_gib and the stock ones.

Compared function by function against the decompile (and the disassembly where noted): every
function of python.cpp, mp5.cpp, glock.cpp (including the ammo classes CPythonAmmo, CGlockAmmo,
CMP5AmmoClip, CMP5Chainammo, CMP5AmmoGrenade), every function of weapons.cpp and ggrenade.cpp,
and in func_tank.cpp the RC functions, CFuncTankMortar::Fire, CFuncTankRocket::Fire and the
inline helpers. The rest of func_tank.cpp (KeyValue, TrackTarget, Fire, the laser, the controls)
was skimmed: same keys, no RC names, nothing non-stock found.

## 3. Function map

Addresses are file addresses. `ok` = ported from the decompile, checked against the disassembly
where marked (d).

python.cpp
- 0x1f86b0 PythRound factory, ok
- 0x1f86e0 CPythRound::PFlareThink, ok (d)
- 0x1f87f0 CPythRound::CreateRnd, ok (d)
- 0x1f8a50 CPythRound::Spawn, ok
- 0x1f8de0 CPythRound::PTouch, ok (d, call list)
- 0x1f93d0 CPythRound::PRndThink, ok (d)
- 0x1fae80 CPythRound::ProMagApplyForce, ok (d)
- 0x1fafe0 CPythRound::ProMagEffected, ok
- 0x1f95d0 GetItemInfo, 0x1f9690 AddToPlayer (see 5), 0x1f9720 Spawn, 0x1f97b0 Precache,
  0x1f9890 Deploy, 0x1f9910 Holster, 0x1f99b0 SecondaryAttack, 0x1fa3d0 PrimaryAttack (d),
  0x1fa810 Reload, 0x1fa8a0 WeaponIdle, ok
- 0x1faa70 GetBotWeight, 0x1fac00 BotFireButton, 0x1fac50 BotFireSecondary (d), 0x1fad70
  BotFirePrimary (d), ok
- CPythonAmmo 0x251880/0x251930/0x251970, ok

mp5.cpp
- 0x1cbd60 SecondaryAmmoIndex, 0x1cbd70 Spawn, 0x1cbe10 Precache, 0x1cbf90 GetItemInfo,
  0x1cc050 AddToPlayer (see 5), 0x1cc0e0 Deploy, ok
- 0x1cc120 IsGoodTarget (d), 0x1cc250 DoBeamEffects, 0x1cc450 FireBeam (d, full), ok
- 0x1cceb0 PrimaryAttack (d, full), 0x1cd4d0 SecondaryAttack (d, full), 0x1ce3a0 Reload,
  0x1ce3f0 WeaponIdle, ok
- 0x1ce5c0 GetBotWeight, 0x1ce640 BotFireButton (d), 0x1ce730 BotFireSecondary (d), 0x1ce800
  BotFirePrimary (d), ok
- ammo classes 0x250320-0x250740, ok

glock.cpp
- 0x19e330 acidgib, 0x19e360 AcidGibCreate, 0x19e3b0 CAcidGib::Spawn, 0x19e630 AnimThink (d),
  0x19e810 GibBounceTouch, 0x19eb00 BloodColor, 0x19eb10 Precache, 0x1a0150 ProMagApplyForce (d),
  0x1a0250 ProMagEffected, ok
- 0x19eba0 Spawn, 0x19ec30 Precache, 0x19ed00 GetItemInfo, 0x19edc0 Deploy, 0x19ee00
  SecondaryAttack (d, call list), 0x19f3d0 PrimaryAttack, 0x19f410 GlockFire (d, part),
  0x19fa50 Reload, 0x19fad0 WeaponIdle, ok
- 0x19fc30 GetBotWeight, 0x19fce0 BotWeaponFrame (d, full), 0x19ff40 BotFireSecondary (d),
  0x1a0010 BotFirePrimary (d), ok
- CGlockAmmo 0x24cbe0/0x24cc90/0x24ccd0, ok

weapons.cpp
- free functions 0x2390e0-0x2397e0: MaxAmmoCarry, Clear/Apply/AddMultiDamage, SpawnBlood,
  DamageDecal, DecalGunshot, EjectBrass, AddAmmoNameToAmmoRegistry unchanged;
  UTIL_PrecacheOtherWeapon and W_Precache ported (precache list, healthkit, egonclip, flag.mdl)
- CBasePlayerItem: FallThink, AttemptToMaterialize, Drop, AttachToPlayer ported; the rest unchanged
- CBasePlayerWeapon: ItemPostFrame (d, branch structure) rewritten whole; AddToPlayer,
  AddWeapon (new, unused), UpdateClientData, SendWeaponAnim, AddPrimaryAmmo, IsUseable,
  DefaultDeploy, DefaultReload, PlayEmptySound, SecondaryAmmoIndex, ExtractAmmo, RetireWeapon,
  GetBotWeight (new) ported
- CWeaponBox: Spawn ported (30 s Kill); IsGravGreandeMoveable, BotCanHaveItem, GetBotWeight new
- CBasePlayerAmmo: unchanged (matches)

ggrenade.cpp
- 0x19c7d0 Explode(TraceResult*) (d), 0x19cd70 Smoke, 0x19d100 DetonateSnark (new),
  0x19d1e0 ExplodeTouch, 0x19d500 BounceTouch (d), 0x19d920 TumbleThink, 0x19db60 ShootContact (d),
  0x19dc60 ShootTimed (d) ported; the rest unchanged

func_tank.cpp
- 0x191b00 BarrelPosition, 0x191c10 CanFire, 0x191c50 TankDeactivate, 0x191c80 TankActivate,
  0x191cd0 IsActive: moved out of line, bodies unchanged
- 0x18fe90 CFuncTankMortar::Fire (d), ok
- CGibTankGib 0x18ffa0-0x1902e0, CFuncTankGib 0x1905d0-0x1912f0 (KeyValue from the disassembly:
  Ghidra split it), ok

## 4. Classes

- **CPythRound** : CGrenade (python.cpp). No save table. +0x280 m_vecDir, +0x28c m_vecSwing,
  +0x298 m_flSwing, +0x29c m_flLastTime, +0x2a0 m_flSpeed, +0x2a4 m_bSubRound, +0x2a8 m_iType.
  All named from use (VERIFY). New virtual ProMagEffected.
- **CAcidGib** : CGrenade (glock.cpp). No members. New virtual ProMagEffected.
- **CGibTankGib** : CGrenade, **CFuncTankGib** : CFuncTank (func_tank.cpp). No save tables.
  Members named from their keyvalues (CFuncTankGib) or from what Fire copies into them
  (CGibTankGib); offsets in comments.
- **CPython**: +0x8c m_flSoundDelay, +0x90 m_fInZoom, the 2001 SDK's members halflife-updated
  dropped (names from that SDK). m_usFirePython removed (no events).
- **CMP5**: +0x8c m_flNextAnimTime, +0x90 m_iShell (stock), +0x94 m_flSpawnTime, +0x98
  m_iBeamState, +0x9c m_flBeamStateTime (VERIFY names). Event members removed.
- **CGlock**: +0x8c m_iShell. Event members removed.
- **CWeaponBox**: new virtuals IsGravGreandeMoveable, BotCanHaveItem, GetBotWeight (slots 80-82).
- **CBasePlayerWeapon**: new virtual AddWeapon (after ExtractClipAmmo), GetBotWeight override.

UseDecrement: removed from CPython, CMP5, CGlock (coordinator decision); all their timers and the
shared weapon code in my files are absolute gpGlobals->time, as in the original.

## 5. Cross-file needs

- **player.cpp (w2-player-core)**: with absolute timers, `CBasePlayer::m_flNextAttack` must not
  be decremented any more (player.cpp ~2794 `m_flNextAttack -= gpGlobals->frametime` and the
  -0.001 clamp), and the weapon DecrementTimers path must stay off (it is, with UseDecrement
  false). ItemPostFrame, DefaultDeploy, DefaultReload and CPython::Holster set and compare
  m_flNextAttack against gpGlobals->time.
- **player.cpp AddPlayerItem**: in the original, CBasePlayerWeapon::AddToPlayer ends with
  AddWeapon() (ExtractAmmo), and only weapons that override AddToPlayer (python, mp5, and others
  in other packages) send WeapPickup. halflife-updated's AddPlayerItem does ExtractAmmo and
  WeapPickup for every non-exhaustible weapon not spawning. I kept AddToPlayer void and left that
  to AddPlayerItem; CPython/CMP5::AddToPlayer exist but send nothing. Differences: the glock
  and crowbar get a pickup icon in ours; at spawn the original showed one for the python and mp5.
  Decide for all weapons together.
- **world.cpp (w2-client-rules-world)**: CWorld::Precache must reset the precache list
  (`g_iCntPrecached = 0`, cbase.cpp) before W_Precache, as the original does. Otherwise
  UTIL_PrecacheOtherWeapon skips every weapon on the second map and ItemInfoArray stays zeroed.
- **ItemPostFrame** (other weapon packages): the IsHornetGun reload is in, before the fire checks,
  as the original has it (confirmed: test of IN_RELOAD then IsHornetGun()==1 then Reload, at
  0x23a2ff, right after the reload completion).
- **rcbot.h** (new): RCBotPev, RCBotFireGate (moved from weapons.cpp), RCBotState (+0x6a0),
  RCBotCenter. Other weapon packages need the same reads; if they wrote their own, merge into
  this header.
- Used, already in the tree: PREFS_* (Preff.h), CPointExplo::ExploCreate (pointexplo.h),
  IsClassnamePrecachedNoInvalid / AddToPrecachedList (cbase.cpp), m_fCurseRefireScale (player.h),
  sv_ceasefire / sv_lowlag via CVAR_GET_FLOAT, CGrenade::ShootDETimed.
- No shared-class member blocks, no TEMP STUBs added or removed.
- Client build (cl_dll compiles python/mp5/glock.cpp): not built and certainly broken by the
  PREFS calls; RC is server-only.

## 6. VERIFY list (26 in my files)

- Member names of CPythRound, CMP5 extras, CGibTankGib, CFuncTankGib: chosen from use.
- RC_CAP_ACIDGIB_IMMUNE (0x4000) name; RCBotState meaning (bot state 1 and 3); RCBotFireGate name.
- RCBotCenter: the original calls the bot's virtual Center(); ours computes CBaseEntity::Center
  on its entvars. Only matters for the never-loadable bot.
- CGlock::GetBotWeight / BotFirePrimary test the enemy's classname "crossbow": which entity has it
  is unknown.
- Glock fart HEV check reproduces the original's out-of-bounds read (see 8).
- Two crash guards (see 8).

## 7. Behavior worth testing

- Python primary: one rocket with a laser trail and blue flare, 80 damage, kick back 125;
  secondary: six rockets (four corkscrewing), 6 rounds, 800 push, 2 s refire.
- MP5 primary lightning beam, 512 range; when it hits nothing, sub beams fork to targets within
  384 in front, leftover beams fly at random and mark walls. Secondary (needs AR grenades):
  backward blast, 3000 damage to anything just behind, launches the player (flat with
  FartStaticVert). Under water a weak push and bubbles.
- Glock: bouncing gib, 30 acid damage, 2 bounces, blood trail; secondary small backward blast.
- func_tankgib with each gib_* keyvalue; func_tankmortar explosions credited to the controller.
- Grenades: TE_KILLBEAM, scorch size, owner kept after explosion, drive-monster grenades
  crediting the driver, contact grenade damage 250.
- sv_ceasefire 1 stops all fire; weaponboxes vanish after 30 s.
- On a stock client with cl_lw 1 the client will predict Half-Life's own python/mp5/glock
  (bullets, sounds, anims) while the server does RC's: expect doubled or wrong effects; the
  original had the same issue with any client running prediction.

## 8. Original bugs

- CPythRound::ProMagApplyForce sets m_vecDir to vecForce * speed, so PRndThink flies at speed
  squared. Kept (moot: no RC caps, the magnet never picks it).
- CMP5 sub beams deal MainBeamDmgPerSec/10; SubBeamDmgPerSec only sizes the blood. Kept.
- CMP5::PrimaryAttack punch angle is RANDOM_FLOAT(-kick, -kick): always exactly -kick. Kept.
- CMP5::PrimaryAttack computes the stock shell velocity (two RANDOM_FLOATs) and drops it. Kept.
- CMP5::GetItemInfo hardcodes iMaxAmmo2 15 while the grenade pickup uses [MP5] MaxGrenades. Kept.
- CAcidGib::AnimThink sets nextthink = 0.1, not time + 0.1: thinks every frame. Kept.
- CGlock::SecondaryAttack HEV check indexes m_rgAmmo[-1] (the glock has no secondary ammo); the
  original reads m_pLastItem there. Reproduced as a read of m_pLastItem, not an array overrun.
- CGlock precaches no silenced sounds (nothing sets the silencer body). Kept.
- CGlock::BotWeaponFrame dereferences a NULL enemy: guarded (`// VERIFY: guard`).
- CFuncTankGib: gib_damagetype is parsed and never used (gibs always DMG_ACID). Kept.
- CFuncTankGib::Fire dereferences a failed CBaseEntity::Create: guarded.
- CBasePlayerWeapon::GetBotWeight calls BotFirePrimary twice (surely meant Secondary). Kept.
- CBasePlayerItem::AttemptToMaterialize adds gpGlobals->time to FlWeaponTryRespawn's already
  absolute time. Kept (halflife-updated fixed it).
- CBasePlayerItem::FallThink / AttachToPlayer leave FallThink running on a held weapon (2001 SDK);
  halflife-updated stops it. Kept as the original.
- CGrenade::BounceTouch: no ResetSequenceInfo, frame not reset; ShootTimed no ResetSequenceInfo
  (2001 SDK). Kept.

halflife-updated changes replaced by the original's behavior (all in weapons.cpp unless noted):
UpdateClientData (net clients only, every weapon resends on a weapon change), SendWeaponAnim
(always sent), IsUseable (2001 SDK test), PlayEmptySound (not predicted), SecondaryAmmoIndex (-1),
ExtractAmmo (secondary result replaces primary), RetireWeapon (immediate), ItemPostFrame (no
TabulateAmmo / m_flLastFireTime, absolute times), grenade Explode (no 1-unit lowering, scaled pull
out of the wall), ShootContact damage 250 instead of the skill cvar. DefaultDeploy keeps
halflife-updated's TabulateAmmo and m_flLastFireTime (no behavior difference). Events
(PLAYBACK_EVENT) are gone from the three weapons.
