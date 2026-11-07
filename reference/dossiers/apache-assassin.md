# apache-assassin

## Scope

Original files: `driveapache.cpp` (70 functions: CDriveApache, CDrvApacheHVR, CDriveApachePickup,
CDrvASpawner) and `driveassassin.cpp` (58 functions: CDriveAssassin, CDriveAssassinPickup,
CDrvAssSpawner).

Written: `dlls/driveapache.cpp`, `dlls/driveassassin.cpp`, `dlls/driveapache.h` and
`dlls/driveassassin.h` (only the `CreateDrv*Spawner()` prototypes, for whatever calls them; the
classes stay in the .cpp files). Touched: `dlls/driveent.h`, `dlls/player.h`, `dlls/weapons.h`
(see Cross-file needs).

## Status

| file | check_file.py --missing |
|---|---|
| driveapache.cpp | 70 of 70 present, 0 missing |
| driveassassin.cpp | 58 of 58 present, 0 missing |

All 7 entity factories are exported (`nm -D`): drive_apache, drv_hvr_rocket, pickup_drive_apache,
drive_apache_spawner, drive_assassin, pickup_drive_assassin, drive_assassin_spawner. Builds on
i386 and amd64, no warnings from these files. Not run in an engine: built only, behavior unchecked.

Addresses in `// orig` comments are funcfile.tsv (file) addresses; the decompile headers are those
+ 0x10000.

## Function map

Every function was written from the typed decompile. Where Ghidra's output was garbled (stack
argument tracking, x87 compares, untyped `this`) the disassembly was read: CDriveApache::TakeDamage,
UpdateGun (both smoothing branches), DoFlight (yaw block), DEClientEject (the two vtable calls),
CDriveApachePickup::Use (which argument it reads), CDrvApacheHVR::AccelerateThink (VecToAngles
argument), CDriveAssassin::DoGun (EjectBrass and MakeVectors arguments) and HandleAnimEvent (the
grenade aim vector). Constants and strings came from .rodata.

| orig | function | status |
|---|---|---|
| 0x123b20 | drive_apache (factory) | ok, see VERIFY 1 |
| 0x123b50, 0x123ba0 | CDriveApache::Save/Restore | ok (IMPLEMENT_SAVERESTORE, 28 fields) |
| 0x123bf0 | CDriveApache::Spawn | ok |
| 0x123ed0 | CDriveApache::Precache | ok |
| 0x123fa0 | CDriveApache::GetControllerName | ok |
| 0x124010 | CDriveApache::APRunThink | ok |
| 0x1241e0 | CDriveApache::DoControl | ok |
| 0x124950 | CDriveApache::DoFlight | VERIFY 4 (always-true condition, kept) |
| 0x124ff0 | CDriveApache::UpdateGun | VERIFY 5, 6 |
| 0x125ce0 | CDriveApache::FireGun | ok |
| 0x125f40 | CDriveApache::DoWeapons | ok |
| 0x1260f0 | CDriveApache::UpdateOwnerHUD | ok |
| 0x126210 | CDriveApache::FireRocket | ok |
| 0x126620 | CDriveApache::ShowDamage | ok |
| 0x1267c0 | CDriveApache::ApFireBullets | ok |
| 0x126cc0 | CDriveApache::TakeDamage | ok (from disassembly) |
| 0x126eb0 | CDriveApache::TraceAttack | ok |
| 0x126f90 | CDriveApache::DyingThink | ok |
| 0x127c10 | CDriveApache::FlyTouch | ok |
| 0x127eb0 | CDriveApache::CrashTouch | ok |
| 0x127f00 | CDriveApache::Killed | ok |
| 0x128100 | CDriveApache::DEClientPreThink | ok |
| 0x1286d0 | CDriveApache::DEClientPostThink | ok |
| 0x1286f0 | CDriveApache::DEClientEnter | ok |
| 0x128aa0 | CDriveApache::DEClientLeave | ok |
| 0x128d60 | CDriveApache::DEClientEject | ok |
| 0x129090 | drv_hvr_rocket (factory) | ok, VERIFY 1 |
| 0x1290c0, 0x129110 | CDrvApacheHVR::Save/Restore | ok |
| 0x129160 | CDrvApacheHVR::Spawn | ok |
| 0x1292c0 | CDrvApacheHVR::Precache | ok |
| 0x129320 | CDrvApacheHVR::IgniteThink | ok |
| 0x129490 | CDrvApacheHVR::AccelerateThink | ok |
| 0x129720 | CDrvApacheHVR::DHVRTouch | ok |
| 0x1297a0 | pickup_drive_apache (factory) | ok, VERIFY 1 |
| 0x1297d0, 0x129820 | CDriveApachePickup::Save/Restore | ok |
| 0x129870 | CDriveApachePickup::Spawn | ok |
| 0x129b80 | CDriveApachePickup::Precache | ok |
| 0x129bf0 | CDriveApachePickup::GetSpawnOrg | ok |
| 0x129c70 | CDriveApachePickup::TakeDamage | ok |
| 0x129c80 | CDriveApachePickup::TraceAttack | ok |
| 0x129cc0 | CDriveApachePickup::PDAAnimThink | ok |
| 0x129e40 | CDriveApachePickup::Use | ok (reads pCaller, per disassembly) |
| 0x12a090 | CDriveApachePickup::DoRespawn | ok |
| 0x12a240 | CDriveApachePickup::DeathNotice | ok |
| 0x12a2b0 | CreateDrvApachSpawner | ok |
| 0x12a310 | drive_apache_spawner (factory) | ok, VERIFY 1 |
| 0x12a340, 0x12a390 | CDrvASpawner::Save/Restore | ok |
| 0x12a3e0 | CDrvASpawner::DrvAThink | ok |
| 0x12a470 | CDrvASpawner::Spawn | ok |
| 0x12a510 | CDrvASpawner::ObjectCaps | ok |
| 0x12a570-0x12a6e0 | CDriveApachePickup::ObjectCaps, SetObjectCollisionBox, GetRCCaps, Center, BloodColor, Classify | ok |
| 0x12a740 | CDrvApacheHVR::GetRCCaps | ok |
| 0x12a7a0 | CDriveApache::EntIsBeingRemoved | ok |
| 0x12a850-0x12aa20 | CDriveApache::VeryHeavyDamageFract, ExtreemeDamageFract, GetCamRotOrg, SetObjectCollisionBox, GetRCCaps, Center, BloodColor, Classify | ok |
| 0x1347f0 | drive_assassin (factory) | ok, VERIFY 1 |
| 0x134820 | CDriveAssassin::Spawn | ok |
| 0x134bf0 | CDriveAssassin::Precache | ok |
| 0x134cb0 | CDriveAssassin::GetControllerName | ok |
| 0x134d20 | CDriveAssassin::DAThink | ok |
| 0x135180 | CDriveAssassin::DoJump | VERIFY 7 |
| 0x135a70 | CDriveAssassin::DoGrenade | ok |
| 0x135d30 | CDriveAssassin::DoKick | ok |
| 0x135e40 | CDriveAssassin::DoGun | ok |
| 0x136350 | CDriveAssassin::CheckDoor | ok |
| 0x1365b0 | CDriveAssassin::CheckEHop | ok |
| 0x1368e0 | CDriveAssassin::RunWaterPhysics | ok |
| 0x136c10 | CDriveAssassin::DoMovement | ok |
| 0x137270 | CDriveAssassin::DoCloak | VERIFY 8 (fade-out overshoot, kept) |
| 0x137800 | CDriveAssassin::DoHealthRegen | ok |
| 0x137960 | CDriveAssassin::UpdateOwnerHUD | ok |
| 0x137b70 | CDriveAssassin::KickDamage | ok |
| 0x137f20 | CDriveAssassin::AsFireBullets | VERIFY 9 (DMG_MORTAR bit) |
| 0x138420 | CDriveAssassin::HandleAnimEvent | ok |
| 0x138720 | CDriveAssassin::GetCamRotOrg | ok |
| 0x138830 | CDriveAssassin::DyingThink | ok |
| 0x1388f0 | CDriveAssassin::SetDeathAnim | ok |
| 0x1389e0 | CDriveAssassin::TakeDamage | ok |
| 0x138af0 | CDriveAssassin::Killed | ok |
| 0x138de0 | CDriveAssassin::DEClientPreThink | ok |
| 0x139410 | CDriveAssassin::DEClientPostThink | ok |
| 0x139430 | CDriveAssassin::DEClientEnter | ok |
| 0x1397f0 | CDriveAssassin::DEClientLeave | ok |
| 0x139b00 | CDriveAssassin::DEClientEject | ok |
| 0x139d10 | pickup_drive_assassin (factory) | ok, VERIFY 1 |
| 0x139d40, 0x139d90 | CDriveAssassinPickup::Save/Restore | ok |
| 0x139de0 | CDriveAssassinPickup::Spawn | ok |
| 0x13a050 | CDriveAssassinPickup::Precache | ok |
| 0x13a0a0 | CDriveAssassinPickup::GetSpawnOrg | ok |
| 0x13a120 | CDriveAssassinPickup::TakeDamage | ok |
| 0x13a130 | CDriveAssassinPickup::PAnimThink | ok |
| 0x13a2d0 | CDriveAssassinPickup::Use | ok |
| 0x13a4d0 | CDriveAssassinPickup::DoRespawn | ok |
| 0x13a590 | CDriveAssassinPickup::DeathNotice | ok |
| 0x13a600 | CreateDrvAssassinSpawner | ok |
| 0x13a660 | drive_assassin_spawner (factory) | ok, VERIFY 1 |
| 0x13a690, 0x13a6e0 | CDrvAssSpawner::Save/Restore | ok |
| 0x13a730 | CDrvAssSpawner::DrvAThink | ok |
| 0x13a7c0 | CDrvAssSpawner::Spawn | ok |
| 0x13a860 | CDrvAssSpawner::ObjectCaps | ok |
| 0x13a8c0-0x13a910 | CDriveAssassinPickup::ObjectCaps, GetRCCaps, IsTeleFraggabe, BloodColor, Classify | ok |
| 0x13a970 | CDriveAssassin::EntIsBeingRemoved | ok |
| 0x13a9f0-0x13aa30 | CDriveAssassin::GetRCCaps, GetGuassDistOut, GetGuassHull, BloodColor, Classify | ok |

## Classes

Bases from hierarchy.tsv. Offsets are the original's.

- **CDriveApache : CBaseDriveEnt.** Save table (savefields.tsv, 28 fields, original order and
  types) from m_pOwner 0x27c to m_fSmokeTime 0x2f4. m_pOwner is the inherited
  CBaseDriveEnt::m_pOwner (coordinator decision), still the first DEFINE_FIELD. Unsaved, from code:
  m_iExplode 0x2f8 (fexplo.spr), m_iBodyGibs 0x2fc (metalplategibs_green.mdl), m_iSpriteTexture 0x300
  (white.spr), named as in CApache whose DyingThink this copies; m_iFlare 0x304 (flare4.spr, VERIFY
  name). No new virtuals (vtables.py --new: none).
- **CDrvApacheHVR : CGrenade.** Saved m_vecForward 0x284. Unsaved: m_iTrail 0x280 (smoke.spr,
  CApacheHVR's name), m_vecOrgForward 0x290 (launch heading, VERIFY name). Save/Restore chain to
  CGrenade, which has none, so CBaseMonster's as in the original.
- **CDriveApachePickup : CBaseMonster.** Saved m_bHoverSpawn 0x27c, m_vecBaseAng 0x280,
  m_pGivenApache 0x28c, m_bTakeable 0x290.
- **CDrvASpawner, CDrvAssSpawner : CBaseEntity.** Saved m_bDummy 0x34.
- **CDriveAssassin : CBaseDriveEnt.** No save table in the original (no Save/Restore). m_pOwner 0x27c
  inherited. Everything else (0x280-0x2fc, 38 members) recovered from code and named from use, each
  with a VERIFY: frame time, the per-think permission flags 0x284-0x28d (0x285, 0x289, 0x28a are
  written but never read in this file), jump/grenade charge timers, walk velocity m_vecMoveVel
  0x2a0 (also sent as pev->fuser4 = its length), cloak state 0x2e0-0x2e8 (cloak power is
  pev->armorvalue), idle sequence, m_iShell 0x2f4 (CHAssassin's name), HUD caches 0x2f8/0x2fc.
- **CDriveAssassinPickup : CBaseMonster.** Saved m_pGiven 0x27c, m_bTakeable 0x280. Adds one
  virtual, IsTeleFraggabe (slot 150, returns false; nothing in the original calls it).

## Cross-file needs

- `dlls/driveent.h`: `CBasePlayer* m_pOwner` on CBaseDriveEnt, as the coordinator specified.
- `dlls/player.h`, `// apache-assassin needs:` block: `float m_fDrvEntAlpha; // 0x674` (the client's
  cl_de_alpha, set in ClientCommand; both DEClientPreThinks read it to draw the vehicle see-through
  via euser2/iuser3/fuser1/fuser2).
- `dlls/weapons.h`, `// apache-assassin needs:` block in CGrenade:
  `static CGrenade* ShootDETimed(entvars_t* pevOwner, entvars_t* pevDriver, Vector, Vector, float)`.
  TEMP STUB at the bottom of driveapache.cpp: ShootTimed plus `dmg_inflictor = ENT(pevDriver)`, which
  is what the original (0x19de20, ggrenade.cpp) does. Real one belongs in ggrenade.cpp.
- Preff.cpp: PREFS_KeyLoopUpFloat/Int/Bool declared extern with main's Preff.h signatures. TEMP
  STUBs returning the default at the bottom of driveapache.cpp; drop at merge.
- Used as-is from the SDK/main: gmsgHealth, gmsgHideWeapon, gmsgSetFOV, gmsgBattery,
  gmsgFlashBattery, gmsgTrain (UserMessages.h), g_pevLastInflictor, SetupVisibility (client.h),
  EjectBrass, UTIL_MoveToOrigin, the CBasePlayer RC members (m_pCurDriveEnt, m_bInDriveEnt,
  m_fDrvEntEjectTime, m_fDrcEntTransTime).
- `CreateDrvApachSpawner()` / `CreateDrvAssassinSpawner()` (in driveapache.h / driveassassin.h) are
  called in the original from Ghidra fragments FUN_0024f720 / FUN_0024ff00 (game rules or world
  setup, several call sites). Whoever reconstructs those needs to call them.

## VERIFY list

1. **Factories don't call AddToPrecachedList.** Every original factory here (as base_drive_ent)
   calls `AddToPrecachedList("<classname>")` after GetClassPtr. The tree's LINK_ENTITY_TO_CLASS
   doesn't, and driveent.cpp doesn't either; this is a project-wide macro decision, not done here.
2. **Assassin member names** (37 of the 44 VERIFY markers): all unsaved, named from use. Settled
   only by taste or by other files that read them (none known).
3. m_iFlare (CDriveApache 0x304), m_vecOrgForward (CDrvApacheHVR 0x290): names chosen here.
4. **DoFlight dead zone.** The yaw, pitch and roll "close enough" tests are `ideal < cur + 0.5 ||
   cur - 0.5 < ideal`, always true, so the avelocity-zeroing branch never runs. Read from the
   disassembly; kept literally.
5. **UpdateGun autoaim smoothing.** When m_vecWantGun equals m_vecGun the original leaves pitch/yaw
   uninitialized; this uses the wanted value (equal anyway).
6. **UpdateGun no-target z.** m_vecWantGun.z is left uninitialized in the original when no target is
   found; never read.
7. **DoJump standing jump** pushes along gpGlobals->v_forward/v_right without a MakeVectors of its
   own, so it uses whatever the last caller left there. Literal.
8. **DoCloak fade-out** never snaps to the target alpha (`fabs(diff) <= rate` with a negative rate),
   so it can overshoot. Literal.
9. **AsFireBullets damage type** includes 0x800000 (DMG_MORTAR in the SDK); RC may use the bit for
   something else. Written as DMG_MORTAR.

Other literal oddities kept: FlyTouch and DEClientEject dereference m_pOwner unchecked; the Apache
TakeDamage chains to CBaseEntity::TakeDamage (not CBaseMonster's); DoControl reuses the
ApacheForDecel/ApacheBackDecel keys for vertical decel with different defaults (200/150); DyingThink
and the assassin's Killed call `Instance(pev->owner)->DeathNotice` without a null check; the
assassin's DEClientEject kills it with GIB_ALWAYS.

## Behavior worth testing

- Use a pickup_drive_apache on ground and in the air: ground one lifts off (m_flGoalZ 200), hover
  one doesn't; rotor sound stops on the pickup.
- Fly: W/S/A/D/jump/duck goal speeds and tilt, view-follow yaw, reload to free-look.
- Chin gun autoaim locking onto players/monsters within 30 degrees, sticking to the last target;
  with ApacheGunAutoaim 0, follows view.
- Rocket volley (4, alternating pods), steering by looking; own rockets don't hurt the Apache;
  kill messages show apache_gun / apache_rocket.
- Damage smoke, death spin and fall, explosion when touching ground or after 15s, driver dies with
  it; pickup respawns 10s later at the same spot once clear of players and monsters.
- Eject (AllowEject default true for the Apache, false for the assassin unless
  sv_allowglobaleject 1 or not multiplayer): Apache throws the driver backward; assassin gibs.
- Assassin: charged jump, air steering, double-beat pistol with tightening spread, kick (0.24s
  delay, knockback on RC cap 0x1000), cooked grenade, cloak on +use draining armor, health regen
  after 4s idle, water swimming, ledge hop, door opening, first-person mode (UseFirstPerson).
- cl_de_alpha between 0 and 1 makes the client get euser2/iuser3/fuser1/fuser2 set.
- ApacheMaxDriveTime (360s) / AssassinMaxDriveTime (600s) kill the vehicle when exceeded.
