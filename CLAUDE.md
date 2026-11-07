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
| `reference/` | the original libraries, signature dumps, source file list, provenance (`reference/README.md`) |
| `scripts/gnv2demangle.py` | gcc 2.x demangler, copied from mod-ts |

## Sources outside the repo

- Original content: the v19 zip (`~/Downloads/rockcrowbarv19.zip`) and Steven's 2002 install at
  `/media/Data3/Projects/mods/rockcrowbar/` (136 maps, configs, demos).
- FWGS hlsdk-portable branch `RockCrowbar19`: RC's cvar list with defaults, stub files only.
- `~/Packages/rcbotold`: RCBot 1's Rocket Crowbar support.
- `../mod-rc2`: Rocket Crowbar 2, scaffolded, not started. RC2 is a near-total rewrite; the two
  share one custom class (`CCrowbarRocket`).

## Status

Scaffold only (2026-11-06): SDK base, gamedir manifest, reference binaries and dumps. No game code
reconstructed yet.
