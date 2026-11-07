# w2-gauss-trip-squeak-satchel: gauss.cpp, tripmine.cpp, squeakgrenade.cpp, satchel.cpp

## 1. Scope

Original files: `gauss.cpp`, `tripmine.cpp`, `squeakgrenade.cpp`, `satchel.cpp`, plus the
header-inline `CGaussAmmo` methods funcfile.tsv files under `(header)`.

Written here: `dlls/gauss.cpp`, `dlls/tripmine.cpp`, `dlls/squeakgrenade.cpp`, `dlls/satchel.cpp`
(each rewritten from halflife-updated's stock version), and in `dlls/weapons.h` the declarations
of `CGauss`, `CTripmine`, `CSqueak`, `CSatchel` only.

What RC did to these weapons:

- **Gauss** is a teleporter. Secondary throws a beacon (`CTeleBecon`, factory `telebecon`,
  runtime classname `gauss_tele`) that sticks where it lands. With a beacon out: secondary
  teleports the player to it, primary fires a beam that teleports what it hits (RC cap 0x1, and
  `CanPlayerAnnoy`) to it, reload retrieves it. Without one: reload teleports the player to a spawn
  point. The stock charge/fire code is gone.
- **Tripmine**: `monster_tripmine` is `CTripmineLaser` (no `CTripmineGrenade`). The laser burns
  what crosses it and bounces off RC cap 0x10000; the mine lives `TRIPMINES Life` seconds and blows
  up when its planter leaves. Secondary throws `CHeadcrabTeather` (`headcrab_teather`), a headcrab
  on a rope that sticks to the world or to something with cap 0x80 and pulls the thrower
  (`GetPullOrg`). At most 30 mines per player.
- **Snarks** shoot a red laser at their enemy every 0.75-0.8 s. Secondary sticks a `CSqueakCam`
  (`snark_cam`) to a wall; reload looks through a cam and back, secondary in a cam cycles cams,
  primary in a cam fires the cam's laser (damage per second). At most 20 snarks and 30 cams in
  the world.
- **Satchel** is a roach bomb (`models/roach.mdl`): the thrown roach runs at the nearest living
  thing; the radio arms it (glow, beam trail), it goes off 3 s later. 2001 controls (no
  `MODERN_SATCHEL_CONTROLS`, no `Detonate`). Also `CKillerRat` (`killerrat`), a rat its owner
  looks through; nothing in these files spawns it.
- Everything numeric comes from rc.ini headings GAUSS, TRIPMINES, SNARKS, SATCHEL, ITEMS, EGON.

## 2. Status

Built, not run in an engine.

```
gauss.cpp: 32 of 32 present, 0 missing
tripmine.cpp: 40 of 40 present, 0 missing
squeakgrenade.cpp: 45 of 45 present, 0 missing
satchel.cpp: 36 of 36 present, 0 missing
(header) CGaussAmmo: AddAmmo, GetBotWeight, Precache, Spawn all ok
```

i386 and amd64 build clean (no warnings from these files). Factories exported: telebecon,
headcrab_teather, snark_cam, killerrat, monster_tripmine, monster_snark, monster_satchel,
weapon_gauss, weapon_tripmine, weapon_snark, weapon_satchel, ammo_gaussclip.

Coordinator decisions applied: no `UseDecrement` override on the four weapons (base returns
false), and every weapon timer is absolute `gpGlobals->time + x` as in the original; crashes
guarded (section 8).

## 3. Function map

Every function was compared against its decompile, and against the disassembly where the
decompile was unclear (noted). "ok" = ported literally.

gauss.cpp

```
0x197ea0/0x197ef0 CTeleBecon::Save/Restore            ok (5 fields)
0x197f40 CTeleBecon::BecCreate                        ok
0x197f90 CTeleBecon::Spawn                            ok
0x1980e0 CTeleBecon::Killed                           ok
0x198390 CTeleBecon::ActiveThink                      ok
0x198650 CTeleBecon::BeconAttach                      ok (VecToAngles input read from disasm)
0x198ae0 TeleIsValidSpot                              ok
0x198cd0 TeleIsValidSpotCheckEnts                     ok
0x1990d0 CTeleBecon::BeconTeleport                    ok; fixangle 2 VERIFY
0x199ca0 CGauss::IsHornetGun                          ok (true)
0x199cb0 CGauss::Reload                               ok
0x19a190 CGauss::Spawn                                ok
0x19a210 CGauss::Precache                             ok
0x19a370 CGauss::AddToPlayer                          ok, see 5 (WeapPickup)
0x19a400 CGauss::GetItemInfo                          ok (weight -1, max ITEMS CellsMax)
0x19a4b0 CGauss::Deploy                               ok (no m_flPlayAftershock)
0x19a4f0 CGauss::Holster                              ok (null.wav, no event)
0x19a570 CGauss::RandomTeleEnt                        ok; GetGuassDistOut result dropped as in original; fixangle 1 then 2 VERIFY
0x19ac50 CGauss::PrimaryAttack                        ok
0x19b8a0 CGauss::SecondaryAttack                      ok
0x19c170 CGauss::WeaponIdle                           ok (never sends an anim, as the SDK)
0x19c260 CTeleBecon::EntIsBeingRemoved                ok
0x19c310..0x19c3a0 CTeleBecon GetBotShootOrg, GetRCCaps (0x802), BotTargetRealseable, GetBotTargetWeight, IsAlive  ok
0x19c400 CGauss::iItemSlot                            ok (header)
0x24c750..0x24c850 CGaussAmmo GetBotWeight, AddAmmo, Precache, Spawn  ok
```

tripmine.cpp

```
0x22a610 CHeadcrabTeather::Precache                   ok
0x22a640 CHeadcrabTeather::Spawn                      ok
0x22a8e0 CHeadcrabTeather::ActThink                   ok
0x22aaa0 CHeadcrabTeather::DetThink                   ok
0x22aad0 CHeadcrabTeather::Killed                     ok (contents == 0 test, see 8)
0x22ad40 CHeadcrabTeather::HTTouch                    ok
0x22b2a0 CHeadcrabTeather::CalcToOrigin               ok
0x22b310 CHeadcrabTeather::AttThink                   ok
0x22b910 CTripmineLaser::Spawn                        ok
0x22bb30 CTripmineLaser::Precache                     ok
0x22bbb0 CTripmineLaser::WarningThink                 ok
0x22bc00 CTripmineLaser::DoBeamTrace                  ok (tail from disasm; TraceAttack(dmg * ExtreemeDamageFract()) from disasm, Ghidra merged the two calls)
0x22c480 CTripmineLaser::PowerupThink                 ok (from disasm, decompile broken)
0x22c950 CTripmineLaser::KillBeam                     ok
0x22ca50 CTripmineLaser::MakeBeam                     ok
0x22cc10 CTripmineLaser::TakeDamage                   ok
0x22cc60 CTripmineLaser::Killed                       ok
0x22ced0 CTripmine::Spawn                             ok
0x22cfd0 CTripmine::Precache                          ok (no event)
0x22d020 CTripmine::GetItemInfo                       ok (weight -1)
0x22d0d0 CTripmine::Deploy                            ok
0x22d120 CTripmine::Holster                           ok
0x22d1f0 CTripmine::PrimaryAttack                     ok (no FL_CONVEYOR test, read from disasm)
0x22d4c0 CTripmine::SecondaryAttack                   ok, guard (8)
0x22d850 CTripmine::WeaponIdle                        ok (no pev->body = 0)
0x22d950 CTripmine::SetObjectCollisionBox             ok (header)
0x22da20 CTripmine::iItemSlot                         ok (header)
0x22da80 CTripmineLaser::GetRealOwner                 ok
0x22da90 CTripmineLaser::EntIsBeingRemoved            ok
0x22dbc0 CHeadcrabTeather::GetPullOrg                 ok
0x22dd90 CHeadcrabTeather::EntIsBeingRemoved          ok
0x22de20 CHeadcrabTeather::ProMagApplyForce           ok (VecToAngles input from disasm)
0x22ded0 CHeadcrabTeather::ProMagEffected             ok, guard (8)
0x22df60..0x22e060 CHeadcrabTeather BotTargetRealseable, GetBotTargetWeight (4 or 0), ShouldBotKillMe, GetBotShootOrg  ok
```

squeakgrenade.cpp

```
0x218460/0x2184b0 CSqueakCam::Save/Restore            ok (15 fields)
0x218500 CSqueakCam::CreateCam                        ok (fraction test from disasm)
0x2186e0 CSqueakCam::Spawn                            ok
0x218a60 CSqueakCam::Precache                         ok
0x218b20 CSqueakCam::Killed                           ok
0x218b40 CSqueakCam::SCamPop                          ok except the active-item write (8)
0x218f00 CSqueakCam::SCamThink                        ok
0x219890/0x2198e0 CSqueakGrenade::Save/Restore        ok (7 fields, m_flNextLaser added)
0x219930 CSqueakGrenade::Classify                     ok (stock)
0x2199c0 CSqueakGrenade::Spawn                        ok (owns itself without an owner)
0x219c20 CSqueakGrenade::Precache                     ok
0x219ce0 CSqueakGrenade::Killed                       ok (stock)
0x219e50 CSqueakGrenade::GibMonster                   ok (stock)
0x219ea0 CSqueakGrenade::ShootLaser                   ok
0x21a3c0 CSqueakGrenade::HuntThink                    ok (from disasm)
0x21ac80 CSqueakGrenade::SuperBounceTouch             ok (no FL_WORLDBRUSH test)
0x21b090 CSqueak::IsHornetGun                         ok (true)
0x21b0a0 CSqueak::Spawn                               ok
0x21b140 CSqueak::Precache                            ok
0x21b1d0 CSqueak::GetItemInfo                         ok (max 18, weight 7)
0x21b280 CSqueak::Deploy                              ok (no idle 1.7)
0x21b330 CSqueak::Holster                             ok (IsActingAsCam from disasm)
0x21b4a0 CSqueak::PrimaryAttack                       ok (2001 trace: origin+16 to +64, fraction > 0.25)
0x21b7e0 CSqueak::SecondaryAttack                     ok
0x21bd00 CSqueak::Reload                              ok
0x21bee0 CSqueak::WeaponIdle                          ok
0x21c030..0x21c220 CSqueakGrenade EntIsBeingRemoved, BotTargetRealseable, GetBotTargetWeight, IsEntEnemy, GetBotShootOrg, GetRCCaps (0x7fe3d5f), GetGuassDistOut (16), GetGuassHull (3), BloodColor  ok
0x21c280..0x21c320 CSqueakCam EntIsBeingRemoved, BloodColor, Classify (CLASS_INSECT)  ok
0x21c380 CSqueak::BotReloadWeapon                     ok (false)
0x21c390 CSqueak::iItemSlot                           ok (header)
```

satchel.cpp

```
0x201cc0 CKillerRat::Killed                           ok; Explode args VERIFY
0x201d90 CKillerRat::RatThink                         ok (from disasm in part)
0x202200 CKillerRat::Classify                         ok
0x202210 CKillerRat::Spawn                            ok
0x2023a0 CKillerRat::Precache                         ok
0x202400 CSatchelCharge::Deactivate                   ok
0x202430 CSatchelCharge::Classify                     ok (CLASS_PLAYER_BIOWEAPON)
0x202440 CSatchelCharge::Spawn                        ok
0x2025b0 CSatchelCharge::SatchelSlide                 ok (stock)
0x202710 CSatchelCharge::SatchelThink                 ok (all from disasm, decompile empty)
0x202fb0 CSatchelCharge::AtivateUse                   ok
0x203130 CSatchelCharge::Precache                     ok
0x2031d0 CSatchelCharge::BounceSound                  ok
0x203240/0x203290 CSatchel::Save/Restore              ok (table in weapons.cpp)
0x2032e0 CSatchel::AddDuplicate                       ok (2001 logic + PickupWhileOut)
0x203350 CSatchel::AddToPlayer                        ok
0x203470 CSatchel::Spawn                              ok
0x2034f0 CSatchel::Precache                           ok
0x203590 CSatchel::GetItemInfo                        ok (weight -1)
0x203640 CSatchel::IsUseable, 0x203690 CanDeploy      ok
0x2036e0 CSatchel::Deploy                             ok (sets models itself, no DefaultDeploy)
0x203830 CSatchel::Holster                            ok
0x203920 CSatchel::PrimaryAttack                      ok (detonate inline)
0x203aa0 CSatchel::SecondaryAttack                    ok
0x203ad0 CSatchel::Throw                              ok
0x203cd0 CSatchel::WeaponIdle                         ok
0x203e90 DeactivateSatchels                           ok, rat case VERIFY (8)
0x203f70 CSatchel::iItemSlot                          ok (header)
0x203fd0 CSatchelCharge::IsAlive, 0x203fe0 GetRCCaps (0x6080001c)  ok
0x204040 CKillerRat::IsGravGreandeMoveable            ok (new virtual, true)
```

Small static helpers in gauss.cpp (`RemoveTeleFlares`, `FindPlayersBecon`, `TeleFragBox`,
`RetrieveBecon`, `TeleTraceHitBlocks`) and squeakgrenade.cpp (`NextPlayersCam`, `EnterCam`,
`SnarkSetupVisibility`) fold loops the original repeats inline; same calls in the same order.

## 4. Classes

Offsets are the original i386 ones. Saved members (savefields.tsv) have their real names;
the rest are named from use (`VERIFY name` in the source).

- `CTeleBecon : CBaseMonster` (gauss.cpp). Saved: m_blAttached +0x27c, m_vecPlaneNormal +0x280,
  m_hOwner +0x28c (what it stuck to; pev->owner is the thrower), m_posOwner +0x294,
  m_angleOwner +0x2a0. No own Precache.
- `CGauss` (weapons.h): no RC members. Keeps `Save/Restore` and `m_fPrimaryFire` only because
  weapons.cpp (not mine) still defines halflife-updated's `CGauss::m_SaveData`; the original has
  no CGauss save table. Globals m_iEndTeleBall, m_iStartTeleFlash, m_iImpactSpr (global in the
  original's symbol table).
- `CHeadcrabTeather : CGrenade`: +0x280 rope beam (CBeam*, ours `EHANDLE m_hBeam`), +0x284
  m_hOwner, +0x28c m_hAttached, +0x294 m_vecVelocity, +0x2a0 m_flDieTime, +0x2a4 m_flLastThink.
  New virtual ProMagEffected (slot 151). No save table.
- `CTripmineLaser : CGrenade`: +0x280 m_vecDir, +0x28c m_iLaserSprite, +0x290 m_flPowerUp,
  +0x294 m_flDieTime, +0x298 m_hOwner, +0x2a0 laser beam, +0x2a4 bounce beam (CBeam*, ours
  EHANDLEs), +0x2a8 m_posOwner, +0x2b4 m_angleOwner, +0x2c0 m_pRealOwner. Names follow the SDK's
  CTripmineGrenade where it had them. No save table.
- `CSqueakCam : CBaseMonster`: all 15 members saved (+0x27c..+0x2e4).
- `CSqueakGrenade : CGrenade`: saved m_flDie +0x280, m_vecTarget, m_flNextHunt, m_flNextLaser
  +0x294 (RC), m_flNextHit, m_posPrev, m_hOwner +0x2a8; unsaved m_iMyClass +0x2b0.
- `CSqueak` (weapons.h): m_pCam +0x8c (CSqueakCam*), m_bInCam +0x90, m_flNextReload +0x94 (an
  absolute time), m_fJustThrown +0x98. m_usSnarkFire dropped (no events in the original).
- `CSatchelCharge : CGrenade`: m_flDetonateTime +0x280, m_flNextWalkSound +0x284, m_iArmed
  +0x288. Globals Roach_Ball_Model, g_iRatSPR.
- `CKillerRat : CBaseMonster`: m_pRatOwner +0x27c, m_flBeamStart +0x280 (-1 = not yet),
  m_flNextBeam +0x284. New virtual IsGravGreandeMoveable (slot 150).
- `CTripmine`: m_usTripFire dropped; SecondaryAttack added. `CSatchel`: Detonate dropped.

## 5. Cross-file needs

- **`CBasePlayerWeapon::ItemPostFrame` (weapons.cpp)** must call `Reload()` for no-clip weapons:
  the original's (0x23a250) tests only IN_RELOAD. CGauss::Reload (retrieve beacon / random
  teleport) and CSqueak::Reload (cam on/off) are dead without it.
- **Player-side timers (player.cpp)**: these weapons now set `m_pPlayer->m_flNextAttack`
  absolute; `CBasePlayer::ItemPreFrame` and the frametime decrement of m_flNextAttack under
  CLIENT_WEAPONS have to match the coordinator's absolute-timer decision.
- **`SetupVisibility` (client.cpp)**: the original passes NULL pvs/pas (snark cam). Called here
  with dummy pointers, as drivehoundeye does; VERIFY against the client.cpp reconstruction.
- **WeapPickup**: the 2001 SDK sends it from CGauss::AddToPlayer; halflife-updated sends it from
  CBasePlayer::AddPlayerItem, except while spawning. Ours relies on AddPlayerItem, so a player
  spawning with a gauss gets no pickup message, where the original sends one.
- Used, already in the tree: PREFS_* (Preff.h), CountEntsClaimedByEnt, CountEntsInWorld,
  GetPullCenter, EntSelectSpawnPoint (player.cpp, extern), CGameRules::CanPlayerAnnoy,
  CBasePlayer::m_fCurseRefireScale, IsActingAsCam, the RC CBaseEntity virtuals.
- Uses `ExtreemeDamageFract()` (CBaseEntity default 1).
- No shared-class members added, no TEMP STUBs.
- Client: cl_dll compiles these files too; the client target was not built (and the removed
  gauss/tripmine/snark members and events will not compile there). RC 1.9 is server-only. A
  stock client predicts its own gauss, tripmine and snark code, so firing will mispredict
  (view anims, gauss charge sound) until server messages arrive.

## 6. VERIFY list

- RC cap bit names (0x1 teleportable, 0x2 telefraggable, 0x80 teatherable, 0x10000 laser
  reflects, CTeleBecon 0x802, snark 0x7fe3d5f, roach 0x6080001c): meanings guessed from use.
- fixangle 2 (BeconTeleport, RandomTeleEnt after 1): literal; runtime test.
- iuser3 3/2 on the tripmine and its beams: literal, purpose unknown (probably read by RC code
  elsewhere, e.g. the gauss or a bot).
- CHeadcrabTeather::Spawn colors: unset locals if RANDOM_LONG(0,2) misbehaved; harmless.
- CKillerRat calls CGrenade::Explode on itself; Explode args (origin, g_vecZero) partly from
  the decompile only.
- SnarkSetupVisibility dummy pvs/pas (client.cpp).
- Member names marked `VERIFY name`.
- The guards in section 8.

## 7. Behavior worth testing

- Gauss: throw beacon (sticks, pings, red glow), secondary teleport to it with 15+ cells, beam on
  another player with 20+ cells teleports him to the beacon, reload retrieves for 5 cells,
  reload without beacon teleports to a spawn point; telefrag on landing; beacon dies if the brush
  it is on moves, in the sky, or when shot (BeconKillable).
- Tripmine: plant, laser burns, laser bounces off cap-0x10000 entities, mine dies after 120 s and
  when the planter disconnects; brush moving drops a weapon_tripmine. Teather sticks, ropes and
  pulls the thrower; second secondary detonates it.
- Snarks: lasers at enemies; cam placement, reload into cam, secondary cycling, primary laser
  damage per second, cam pop when the wall moves; ceasefire blocks bites, lasers and cam entry.
- Satchel: roach chases nearest DAMAGE_AIM target, radio arms (glow, beam, rising squeak),
  explosion 3 s later; DeactivateSatchels on death leaves armed roaches.
- Timers: refire times multiply by the refire curse.

## 8. Original bugs

Kept:

- CGaussAmmo::AddAmmo gives `EGON AmmoBoxGive` (40), not a GAUSS key.
- CGauss::WeaponIdle picks an idle animation it never sends (as the SDK).
- CGauss::SecondaryAttack sets the refire to 0.5 and immediately to 0.8.
- CGauss::RandomTeleEnt calls GetGuassDistOut and drops the result; writes fixangle 1 then 2.
- CHeadcrabTeather::Killed tests contents == 0, which never happens, so it always draws the
  fireball sprite.
- CHeadcrabTeather::HTTouch restores m_vecVelocity when touching its owner, which only the
  pro-magnet curse ever sets, so the teather stops dead (velocity zero) instead.
- CSqueakGrenade::HuntThink keeps the 2001 SDK's `else if (movetype = MOVETYPE_FLY)`: out of
  water it always ends up MOVETYPE_BOUNCE.
- CKillerRat::Killed deals its owner 9999 damage; it calls CGrenade::Explode on a CBaseMonster.
- CKillerRat::RatThink continues after exploding on attack2 (the beam code still runs).
- CTripmine::PrimaryAttack looks up the hit entity and ignores it (no conveyor test).
- Satchel: a killer rat has classname monster_satchel, so CSatchel::AddToPlayer counts rats as
  deployed charges and PrimaryAttack "uses" them.

Changed (guarded, coordinator decision; each marked `VERIFY: guard` or noted in the source):

- CTripmine::SecondaryAttack and CHeadcrabTeather::ProMagEffected dereference a teather's
  m_hOwner without a NULL check: guarded.
- CBeam pointers in CHeadcrabTeather and CTripmineLaser: the original's DispatchRemoveEnt is
  empty, so its CBeam* members outlive the beams (which remove themselves after 1 s) and its
  validity test reads freed memory; ours hold EHANDLEs, which read as NULL once the beam is gone.
- CSqueakCam::SCamPop writes `m_bInCam = false` into the owner's active weapon cast to CSqueak,
  whatever weapon it is (a stray byte write into another weapon's member). Ours writes only when
  the active weapon is the snark.
- DeactivateSatchels casts every monster_satchel to CSatchelCharge and reads m_iArmed, past the
  end of a CKillerRat. Ours treats a rat as unarmed and removes it, which is what the original
  does unless the garbage byte happens to be 1.

Halflife-updated changes dropped in favor of the original: GetNextAttackDelay (time creep),
UTIL_SharedRandomFloat idle times, `pev->body = 0` in CTripmine::WeaponIdle, FL_WORLDBRUSH and
FL_CONVEYOR tests, the modern satchel controls and AddDuplicate carry limit, DefaultDeploy in
CSatchel::Deploy, the 1.7 s snark deploy idle, all playback events (gauss, tripfire, snarkfire).
