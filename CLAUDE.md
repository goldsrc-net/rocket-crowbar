# mod-rockcrowbar

**Mission: a functional, source-faithful reconstruction of the Rocket Crowbar 1.9 server game
library as modern C++ on the halflife-updated SDK, that builds for every arch the buildchain
targets, loads in the engine, and behaves identically to the original.**

RC 1.9 (2001) is a server-only Half-Life deathmatch mod: the rocket crowbar, drivable monsters
(`drive_garg`, `drive_apache` and a dozen more), curses, deathballs, CTF flags, per-map `.lec`
entity configs, map and kick voting, and `rc.ini` server prefs. Clients run Half-Life's own client
library, so there is no client to rebuild.

## Method

Same as `../mod-ts` (reTS); read its CLAUDE.md for the pipeline. The bar is behavior, verified by
build + run + behavior, on a modern compiler (decided 2026-11-06): we don't chase the original's
binary layout, so a modern build has different member offsets and vtable slots from the gcc 2.95
original. The swap contract is the external interface: the `LINK_ENTITY_TO_CLASS` entity set,
keyvalues, cvar and console command names, user message names and wire formats, and the file
formats RC reads (`rc.ini`, `.lec`).

## This repo

A fork of `goldsrc-net/halflife-updated` branch `64bit-demo` (the branch mod-ts uses: `64bit` plus
the commit that drops the valve-gamedir gate, which a mod in its own gamedir needs).

- `upstream` = `git@github.com:goldsrc-net/halflife-updated.git`, tracked by `main`.
- No `origin` yet. When it gets one it is a private goldsrc-net repo (`../REPOS.md`: reconstructions
  are private).

| Path | What |
|---|---|
| `game/rockcrowbar/` | the mod's gamedir manifest (`liblist.gam`) |
| `reference/` | the original libraries and everything derived from them (`reference/README.md`) |
| `reference/RECONSTRUCTION.md` | the work list (from `scripts/inventory.py`) |
| `reference/DEVIATIONS.md` | everything we do differently from the original |
| `reference/ORIGINAL_BUGS.md` | the original's bugs, kept or guarded |
| `reference/dossiers/` | one per reconstruction package; `AGENT_BRIEF.md` is the brief for parallel workers |
| `scripts/` | `build.sh`, the table generators (`inventory`, `hierarchy`, `vtables`, `savefields`, `cvars`, `sdk_baseline`), `check_file.py`, `ghidra/` (`re.sh`, `ApplyRC.java`) and small objdump helpers |

How the decompile and tables are set up, and how to repeat it for RC2: `../DECOMPILE.md`. The
typed decompile lives in `recovered/decomp/` (gitignored); rebuild it with
`scripts/ghidra/re.sh setup && scripts/ghidra/re.sh export` (~20 min).

## Sources outside the repo

- Original content: the v19 zip (`~/Downloads/rockcrowbarv19.zip`) and Steven's 2002 install at
  `/media/Data3/Projects/mods/rockcrowbar/` (136 maps, configs, demos).
- FWGS hlsdk-portable branch `RockCrowbar19`: RC's cvar list with defaults, stub files only.
- `~/Packages/rcbotold`: RCBot 1's Rocket Crowbar support.
- `../mod-rc2`: Rocket Crowbar 2, scaffolded, not started. RC2 is a near-total rewrite; the two
  share one custom class (`CCrowbarRocket`).

## Status (2026-11-07)

Built, not yet run in an engine. Builds on i386 and amd64.

- Done: RC's 46 cvars, its additions to the SDK base classes (`CBaseEntity`'s 23 virtuals,
  `CBaseDriveEnt`, the bot interface, `CanPlayerAnnoy`, the player's saved members), the
  `LINK_ENTITY_TO_CLASS` precache-list change, and all 30 of RC's own source files: 1,093 of 1,093
  original functions present (`scripts/check_file.py`). 281 `VERIFY` marks in `dlls/`.
- Wave 2, next: RC's changes inside SDK files (`RECONSTRUCTION.md` "SDK files with RC additions",
  791 functions): the weapons (egon, shotgun, rpg, crossbow, handgrenade, hornetgun, tripmine,
  gauss, squeak, crowbar, python, mp5, glock, satchel), `player.cpp` (the drive hand-off in
  PreThink/PostThink, `PlayerDoID`), `client.cpp` (RC's commands, `SetupVisibility` accepting NULL,
  `IsClientListenServer`), `world.cpp` (the drive spawners at map start), `teamplay_gamerules.cpp`
  (CTF scoring), `headcrab.cpp` (the killer babycrab), `combat.cpp` (`ThrowFakeGibs`). The callers
  each package needs are in its dossier's section 5. Four `TEMP STUB`s remain (`grep -n "TEMP
  STUB" dlls/*.cpp`).
- Then: run it next to the original (32-bit engine) and test the dossiers' section 7 items.
