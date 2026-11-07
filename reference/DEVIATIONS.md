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
  `voice_serverdebug`. They post-date RC's 2001 SDK. With the code that would read them
  reconstructed, `allow_spectators`, `mp_chattime`, `mp_falldamage`, `sk_plr_hornet_dmg1`-`3`
  and `sv_pushable_fixed_tick_fudge` have no effect (`CPushable::Move` in `dlls/func_break.cpp` is
  SDK 2.1's again and nothing else reads it), `sv_alltalk` changes nothing (see the voice manager
  below) and `sv_busters` is off, so at their defaults these match the original.
  `CHalfLifeMultiplay::Think` keeps `mp_fragsleft` and `mp_timeleft` updated for server browsers;
  the original's Think writes neither. `sv_allowbunnyhopping` is below. docs/SERVER.md lists each.

## Commands and entities

- **Commands the original does not have, kept from halflife-updated:** the client commands
  `fullupdate` and `closemenus` (today's client sends them), the voice manager's `vban` and
  `VModEnable`, and the server commands `sv_load_all_maps` and `sv_stop_loading_all_maps`. The
  original answers the client ones with "Unknown command".
- **`func_vehicle` and `func_vehiclecontrols` exist.** halflife-updated added them; the original
  has neither, so a map using them gets nothing there on RC 1.9.

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

## Clients, rules and world

- **`ScoreInfo` is the 9-byte message everywhere**, including `CScientist::Killed`'s
  `mp_scikill` frag (the original writes its 5-byte form). `PlayerKilled`/`InitHUD` likewise.
- **The voice manager stays, set so everyone hears everyone.** The original has none, which
  meant no voice restrictions; halflife-updated's limited teamplay voice to teammates.
- **`UpdateClientData` sends `cd->fov = pev->fov`**, as the original does (always 0 in RC). Kept;
  check a current client's field of view in the first engine run.
- **Scripted sentences only go to monsters** (`CScriptedSentence::AcceptableSpeaker`), as in the
  original; halflife-updated's non-monster speakers (HL25) are left out.
- **`GetTeamNameForIndex` returns a static buffer.** The original copies into a read-only `""`
  literal, which crashed every CTF message on Linux.

## halflife-updated changes kept in stock functions

RC 1.9's stock-named functions have the original's behavior back wherever halflife-updated
changed the game's design or balance, or added a feature; halflife-updated's evident bug fixes
stay (audit of 2026-11-07, `reference/audit/r1.md`-`r6.md`, one row per function; the original
decides, since RC 1.9 was built on SDK 2.1 and may have changed a stock body; the fix or design
call per revert is in mod-rc2's `reference/audit/FIXES.md`, made the same way in both projects).
What halflife-updated changed there and we keep, besides restyling and the crash guards listed
under "Crashes in the original":

- **halflife-updated's bug fixes in stock functions**, kept by Steven, 2026-11-07 (each marked
  `// halflife-updated's fix, kept:` in the code; RC 1.9's own changes in the same functions stay):
  - Gagged monsters don't idle-talk and swimming ones do (`dlls/monsterstate.cpp` RunAI, gag bit
    in spawnflags); `ReportAIState` names PLAYDEAD and DEAD apart (`dlls/monsters.cpp`).
  - A blocked door stops its moving sound before the linked doors start again
    (`dlls/doors.cpp` Blocked); a `momentary_door` holds its value at 0, doesn't restart a move to
    where it is heading, and stops its moving sound on arrival (CMomentaryDoor::Use).
  - Buttons time their sparks on `pev->ltime`, spark at the button, and a `func_rot_button` with
    X Axis doesn't spark (`dlls/buttons.cpp` ButtonSpark, ButtonBackHome); a
    `momentary_rot_button` sends its target the value it is heading for (Use).
  - `func_breakable`'s "explosion" 1 is directed, it can't break twice, and its break volume uses
    `fabs` (`dlls/func_break.cpp` KeyValue, Die). Its directed gibs still fly along the attack
    direction, as the original (FIXES.md, unclear).
  - `fabs` where SDK 2.1 called the int `abs()` on a float: the fan's pitch, volume and full-speed
    test (`dlls/bmodels.cpp` RampPitchVol, SpinUp), the roach's food height (`dlls/roach.cpp`
    MonsterThink), the train's pitch (`dlls/plats.cpp` UpdateSound, which keeps RC's 0-255 clamp).
  - `env_blood`'s player blood with no player in slot 1 bleeds at the entity, not at the free
    edict's origin (`dlls/effects.cpp` BloodPosition).
  - A `func_tank` hides the controller's view model and gives his weapon back with `EquipWeapon`,
    so an empty one doesn't come back invisible (`dlls/func_tank.cpp` StartControl, StopControl).
  - The nihilanth's ending fires with no enemy (notarget) (`dlls/nihilanth.cpp` CommandUse).
  - `trigger_hurt` doesn't heal a dead player in multiplayer (`dlls/triggers.cpp` HurtTouch;
    RC's driven-monster CLIENTONLYFIRE test stays), and `trigger_camera` turns at a rate that
    doesn't follow the frame rate (FollowTarget).
  - Players (`dlls/player.cpp`): a dead player with an empty active satchel drops no box
    (PackDeadPlayerItems; dropped guns still aren't reloaded, as the original); RemoveAllItems lets
    go of a func_tank (the empty CurWeapon at its end stays, as the original); drowning gives back
    only the health lost, swim sounds play only with the head under water, and the server no
    longer applies water friction on top of pm_shared's (WaterMove); PostThink tracks the buttons
    on every path; ItemPostFrame runs no weapon frame on the frame a tank is taken; autoaim off
    resets the crosshair angle (GetAutoaimVector, still on `sv_aim` alone); ammo for an
    exhaustible weapon gives the weapon too (GiveAmmo, with RC's own count test kept), through the
    pickup flow below.

- **Players keep the engine's `FL_PROXY` and `FL_FAKECLIENT` flags** (`dlls/player.cpp` Spawn). The
  original sets `pev->flags` to `FL_CLIENT`, so HLTV proxies and bots stay marked as such.
- **The weapon bits are 64 bits for today's client** (`dlls/player.cpp` UpdateClientData's
  `Weapons` message, which the original doesn't send; HasSuit and the suit, flashlight and charger
  tests read `m_WeaponBits`, mirrored into `pev->weapons`).
- **`fullupdate` and impulse 204 run halflife-updated's fuller `ForceClientDllUpdate`**
  (`dlls/player.cpp`): it also resets the HUD, field of view, weapon-bits, room-type and ammo
  caches and resends ResetHUD, what today's client needs.
- **Node graphs go through the engine filesystem; on amd64 the file is
  `maps/graphs/<map>.nod64`** (`dlls/nodes.cpp`), since CGraph and CLink hold pointers. i386 keeps
  `.nod`, version 16, as the original. The `.nrp` report prints an int with `%d` where the
  original's `%f` printed garbage.
- **A second `worldspawn` removes itself** (`dlls/world.cpp` CWorld::Precache). In the original,
  any extra worldspawn (a map with two, or one a `.lec` creates) runs the whole map setup again:
  new game rules, rc.ini reload, votes reset, every spawner created a second time.
- **Save/restore is halflife-updated's** (`dlls/util.cpp`, `dlls/monsters.cpp`, `dlls/world.cpp`
  CGlobalState): save data held by reference, 1-byte booleans, `FIELD_INT64`, pointer-size
  function fields, the empty token table test and `\n`-ended messages; beams keep their attachment
  when their entity is set again on restore (`dlls/effects.cpp` SetStartEntity, SetEndEntity).
  Multiplayer doesn't save, and save files aren't compatible with the original's anyway.
- **Singleplayer-only paths are halflife-updated's** (`dlls/singleplay_gamerules.cpp` spserver.cfg
  and GetNextBestWeapon, `dlls/triggers.cpp` trigger_camera's `m_hViewEntity` and the changelevel
  transition code, `dlls/player.cpp` Restore, UpdateClientData's restore block and CStripWeapons'
  local player).

Decided by Steven, 2026-11-07 (`reference/audit/r7.md`):

- **env_sound's room type reaches the client from `CBasePlayer::UpdateClientData`**
  (`dlls/sound.cpp` CEnvSound::Think, `dlls/player.cpp`): resent on every spawn and full update,
  and reset to 0 at a new map. The original sends `SVC_ROOMTYPE` once, when an env_sound takes
  over, so its room effect carries into the next map. Kept: the carry-over is a bug, fixed in
  both RC 1.9 and mod-rc2.
- **`CBasePlayer::TakeDamage` sends HLTV an `SVC_DIRECTOR` damage event** (`dlls/player.cpp`), which
  the original doesn't. Only an HLTV proxy's auto-director sees it. Kept.
- **A pickup's `AmmoPickup` follows its `WeapPickup`** (`dlls/player.cpp` AddPlayerItem runs
  ExtractAmmo after the weapon joins the list); the original gives the ammo inside AddToPlayer,
  before. Kept: halflife-updated moved it so a give of an exhaustible weapon's ammo also gives the
  weapon (its 6951bf1, issues #153 and #157).
- **A `func_tank` whose target is nearer than its barrel offset aims without the barrel
  correction** (`dlls/func_tank.cpp` AdjustAnglesForBarrel). The original takes the square root of
  a negative there and aims with NaN angles. Kept as a guard.

Kept as the original until Steven decides (FIXES.md "Unclear"): `UpdatePlayerSound`'s
`UTIL_MakeVectors`, the roach's 1-unit height, `LinearMoveDone`'s snap, and `func_breakable`'s gib
direction.

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

In stock functions, halflife-updated's guards kept (audit of 2026-11-07; `ORIGINAL_BUGS.md` lists
each, and the audit rows in `reference/audit/` have the reasoning):

- **`game_score` and `game_player_equip` ignore a missing activator** (`dlls/maprules.cpp`). The
  original calls `IsPlayer()` through the null pointer.
- **`trigger_counter` takes a NULL activator** (`dlls/triggers.cpp` CounterUse).
- **Keyvalue parsing stays in its buffer**: `UTIL_StripToken` takes its destination's size
  (`dlls/maprules.cpp` CGamePlayerEquip::KeyValue, `dlls/triggers.cpp` CMultiManager::KeyValue),
  and `light_environment`'s color values start at 0 when a keyvalue has no numbers (`dlls/lights.cpp`
  CEnvLight::KeyValue), where the original read uninitialized ints.
- **The nihilanth's target-name buffers are 128 bytes** (`dlls/nihilanth.cpp` NextActivity,
  HandleAnimEvent). The original built them in 64 and 36 bytes from 64-byte keyvalues plus a
  number.
- **The node graph's routing tables free with `delete[]`** (`dlls/nodes.cpp`
  ComputeStaticRoutingTables), where the original used scalar delete on four `new[]` arrays.
- **`func_breakable` material 9 (`matNone`) precaches and sounds as wood** (`dlls/func_break.cpp`
  Precache, DamageSound). KeyValue lets 9 through and the original then used an uninitialized
  model name and sound list.
- **A `func_plat`'s inside trigger holds its platform by handle and removes itself when the
  platform is gone** (`dlls/plats.cpp` CPlatTrigger); the original read the freed platform.
- **`CSoundEnt::ISoundsInList` counts 0 for an unknown list type** (`dlls/soundent.cpp`) instead of
  walking an uninitialized index.
- **A `speaker` with a preset outside 1-12 plays nothing** (`dlls/sound.cpp` SpeakerThink); the
  original used whatever its sound-name pointer held.
- **`env_beam`'s ring with a point entity returns before its message starts** (`dlls/effects.cpp`
  CLightning::StrikeThink). The original returned with the temp-entity message still open, so the
  next message start is an engine error.
- **Monster guards** (each marked VERIFY): the bullsquid's spit tests for an enemy
  (`dlls/bullsquid.cpp` HandleAnimEvent), `CHGrunt::CheckMeleeAttack1` fails with no enemy
  (`dlls/hgrunt.cpp`), a talking monster's stare skips the distance test with no player 1 and
  `CanFollow` checks `m_pCine` just after a script ends (`dlls/talkmonster.cpp` RunTask,
  CanFollow), a hornet without an owner does no damage (`dlls/hornet.cpp` DieTouch), and the
  osprey's flight checks its path corner (`dlls/osprey.cpp` FlyThink).
- **Player inventory** (`dlls/player.cpp`): `RemovePlayerItem` also clears the last item when the
  active one goes, where the original left it pointing at the removed item; `SwitchWeapon` takes a
  NULL weapon; `PackDeadPlayerItems` holds `MAX_WEAPONS` weapons where the original's list had 20.

## Layout

- **`m_pOwner` lives in `CBaseDriveEnt`.** Each original drive class declares its own driver
  member at +0x27c, and the chumtoad/headcrab code reads it through any drive monster at that
  offset. One base member gives the same behavior without depending on layout. Each class's save
  table still lists it, as the original's do.
