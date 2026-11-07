# Deviations from Rocket Crowbar 1.9

Everything this rebuild does differently from the original `rc_i386.so`, whether on purpose or
forced by the build, with its default and what it changes. Same convention as mod-ts's
`reference/DEVIATIONS.md`.

## Build and layout

- **Member offsets and vtable slots differ.** We build with a modern compiler on halflife-updated
  (decided 2026-11-06). gcc 2.95 put the vtable pointer after a class's base data; every modern
  compiler puts it at 0, and halflife-updated adds virtuals of its own (the virtual destructor,
  `MyTogglePointer`, the HL25 sentence virtuals on `CBaseToggle`, the client-prediction virtuals
  on `CBasePlayerItem`, `DoRetireWeapon`). Anything that reads the original's memory by offset or
  hooks it by vtable slot (Metamod or AMX Mod X plugins written for RC 1.9) needs our layout.

- **The server is built without `CLIENT_WEAPONS`, as the original was** (decided 2026-11-07;
  this is a change from halflife-updated, not from RC). The original's `GetWeaponData` zeroes the
  data and its `UTIL_WeaponTimeBase()` returns `gpGlobals->time`: no client weapon prediction, and
  every weapon timer absolute. Our server now compiles the SDK's non-prediction branches the same
  way; the client target keeps `CLIENT_WEAPONS`. Players' clients run no weapon prediction
  against an RC server, as in 2001.

## Cvars

The cvar set is checked by `scripts/cvars.py` (`reference/cvars.tsv`).

- **`servercfgfile` is not registered.** The original registers it (default `server.cfg`, no
  flags), as Valve's SDK did. halflife-updated dropped it because the current engine defines it
  itself; registering it again only logs a duplicate warning. Behavior is the same.
- **Cvars the original does not have, kept from halflife-updated:** `allow_spectators`,
  `mp_chattime`, `mp_falldamage`, `mp_fragsleft`, `mp_timeleft`, `sk_plr_hornet_dmg1`-`3`,
  `sv_allowbunnyhopping`, `sv_alltalk`, `sv_busters`, `sv_pushable_fixed_tick_fudge`,
  `voice_serverdebug`. They post-date RC's 2001 SDK. At their defaults they should match the
  original's behavior; each gets checked against it when the code that reads it is reconstructed.

## Messages and prediction

- **`Health` is a 2-byte message, `ScoreInfo` 9 bytes.** The original registers them as 1 and 5
  bytes (the 2001 client's formats); today's Half-Life client (HL25), which RC players use, reads
  `Health` as a short and `ScoreInfo` with class and team. We keep halflife-updated's
  registrations and every writer matches them (the drive monsters' 28 `Health` writes are shorts).
- **`sv_allowbunnyhopping` defaults to 1.** RC 1.9's server movement has no bunny-hop cap
  (`pm_shared`, server side). halflife-updated clients cap unless the server allows it, so the
  default makes them predict what the server does. Valve's Steam client always caps (not
  checked against its binary), so a Steam player bunny-hopping on an RC server can mispredict, as
  any old-SDK mod's would.

## Weapons

- **The scientist and Barney bombs speak** (`dlls/rpg.cpp`). The original builds their sentence
  names with `"%!SC_HELLO%d"`: Windows' printf drops the unknown `%!` and gives `!SC_HELLO3`, a
  real sentence, while glibc keeps `%!`, so the original Linux server played none of their lines.
  Ours plays them, as Windows servers did (same reasoning as the CRLF `rc.ini`).
- **Two out-of-bounds memory accesses are bounded** (`dlls/squeakgrenade.cpp`, `dlls/satchel.cpp`).
  `SCamPop` writes a byte into whatever weapon is active (ours only when it is the snark), and
  `DeactivateSatchels` reads past the end of a `CKillerRat` (ours treats a rat as unarmed).
- **`CImpArrow`'s save table drops a duplicate `m_vecVelo` entry** that the original saves a second
  time as an entity pointer (`dlls/crossbow.cpp`).
- **`WeapPickup` is sent as the original sends it**: by eight weapons' own `AddToPlayer`
  (crossbow, egon, gauss, hornetgun, mp5, python, rpg, shotgun), not by halflife-updated's
  `AddPlayerItem` for every weapon. halflife-updated's immediate ammo HUD update in
  `AddPlayerItem` stays (extra `AmmoX` messages the original doesn't send; nothing reads them
  differently).

## Prefs

- **`rc.ini` may use CRLF line ends** (decided 2026-11-07). The original Linux build keeps the
  `\r`, so the CRLF `rc.ini` RC shipped matched no heading and loaded nothing: Linux servers ran
  on built-in defaults. Windows servers' text-mode reads drop the `\r` and did load it, which is
  what players got. `PREFS_LoadFile` now turns a `\r\n` line end into `\n`. Checked against the
  shipped v19 `rc.ini` (633 CRLF lines): 30 headings, same values as its LF copy.

## Save data

- **`CBasePlayer::m_iGibExtra` is saved as `FIELD_INTEGER`.** The original declares it
  `FIELD_POINTER` (4 bytes on i386). It's an int, and on 64-bit a pointer field would read 8 bytes
  into it. Save files aren't compatible with the original's anyway (different layout).

## Bot interface

- **The `Bot*` virtuals have no caller in this build.** RC 1.9 gave `CBasePlayerWeapon`,
  `CBasePlayerItem`, `CBasePlayerAmmo` and `CBaseEntity` an interface its bot drives (the bot
  shipped as a separate library, `rcbot.dll`; the public RCBot 1 source doesn't use it). The bot
  object `CBot` is never defined in the game library; `BotWeaponFrame` reads the original i386
  bot's object at its offsets (its entvars at +0, a float at +0x698), so it is only meaningful
  for that bot, which can't load against our layout anyway. Kept so the interface is complete.
- **`BotWeaponFrame` drops two discarded calls.** The original calls `pEnemy->GetBotShootOrg()`
  twice and throws both results away; the base version has no side effects.

## Crashes in the original

- **`CDriveIcky::TakeDamage` checks for a driver.** The original writes through its driver
  pointer even when nobody is driving, which crashes a server whose driverless ichthyosaur takes
  damage. Ours skips that write when there is no driver (`dlls/driveicky.cpp`, marked VERIFY).

- **Three more guards in the controller and aslave** (`dlls/drivecontroll.cpp`,
  `dlls/driveaslave.cpp`, each marked VERIFY): `CDriveController::TakeDamage` writes through a null
  owner, `CASLRechargeBall::FlyToOwner` calls through a null target, and `CASLEngBall::EBThink`
  reads a trace it never filled (ours zeroes it first; `CASLEngBall` is unused anyway, since
  `CDriveASlave::DoBallAttack` is empty in the original).
- **The shotgun rift and the egon's IsoBomb check for a gone owner** (`dlls/shotgun.cpp`
  `CSpaceRift::FinalBoom`, `dlls/egon.cpp` `CIsoBomb::IsoBecomeSolid`). Both read through their
  owner after the owner disconnected, which crashes the original.
- **`GetFlagTeamColor` doesn't write into a string literal.** For a team outside 1-6 the original
  copies an error message into a read-only `""`, which crashes. Ours returns `""` without the write
  (`dlls/flag.cpp`, marked VERIFY).

- **CVEC frees with `delete[]`** (`dlls/bot_prefs.h`). The original uses scalar delete on memory
  from `new[]`, which corrupts its heap at shutdown for `CVEC<sHeading>`.
- **The houndeye and alien grunt pickups test for NULL before calling `IsPlayer()`.** The original
  calls it first; same behavior for any real caller.

## Layout

- **`m_pOwner` lives in `CBaseDriveEnt`.** Each original drive class declares its own driver
  member at +0x27c, and the chumtoad/headcrab code reads it through any drive monster at that
  offset. One base member gives the same behavior without depending on layout. Each class's save
  table still lists it, as the original's do.
