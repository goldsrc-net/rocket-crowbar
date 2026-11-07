# Rocket Crowbar 1.9 (RE)

A fan-made reconstruction of Rocket Crowbar 1.9, the 2001 Half-Life deathmatch mod, as C++ source on the
Half-Life SDK. It rebuilds the mod's server library from RC 1.9's shipped binaries, so that it drops into
a stock RC 1.9 install and plays the same: the rocket crowbar, the drivable monsters, curses, deathballs,
CTF, the per-map level customization files and the rc.ini prefs. RC 1.9 has no client library; players
use Half-Life's own.

This reconstruction is not affiliated with or endorsed by Rocket Crowbar's author. Rocket Crowbar, its
name, models, sounds and configs are his; this replaces only the code. See [CREDITS.md](CREDITS.md).

## Installing

The rebuild is a server library for a stock RC 1.9 install: put `hl.so` in `rockcrowbar/dlls/` and point
`gamedll_linux` in `rockcrowbar/liblist.gam` at it. [INSTALL.md](INSTALL.md) covers servers, the test
setup that runs the original and the rebuild side by side, and going back.

## Servers

The rebuild reads RC 1.9's cvars, rc.ini and per-map `.lec` and `.cfg` files unchanged.
[docs/SERVER.md](docs/SERVER.md) lists every cvar, command, impulse and file RC adds to Half-Life,
[docs/GAMEPLAY.md](docs/GAMEPLAY.md) what RC changes in play, and [docs/RC_INI.md](docs/RC_INI.md)
every rc.ini key with its default. Where it
deliberately does something RC 1.9 does not, it is listed in
[reference/DEVIATIONS.md](reference/DEVIATIONS.md); the bugs found in the original, and which ones it
keeps, are in [reference/ORIGINAL_BUGS.md](reference/ORIGINAL_BUGS.md).

## Building

The Linux libraries build in the [goldsrc-net/buildchain](https://github.com/goldsrc-net/buildchain)
Docker image (`scripts/build.sh i386`, `scripts/build.sh amd64`). [BUILDING.md](BUILDING.md) has the
steps.

## How it was made

The original `rc_i386.so` kept its symbols, which gives every function's name, its class hierarchy,
vtables and save tables; the Windows `rc.dll` gives return types. The whole library was decompiled with
Ghidra, typed from those tables, and rewritten on top of
[Half-Life Updated](https://github.com/twhl-community/halflife-updated), then checked by building,
running and comparing against the original running in the same engine. `reference/` holds the originals
and the derived inventories; `scripts/` builds and checks them, and `tools/rig/` runs the two side by side.

## License

The rebuild is built on the Half-Life SDK and is distributed under its license ([LICENSE](LICENSE)): free
of charge only. Half-Life Updated's own changes are in [CHANGELOG.md](CHANGELOG.md) and
[FULL_UPDATED_CHANGELOG.md](FULL_UPDATED_CHANGELOG.md), and its contributors in [CREDITS.md](CREDITS.md).
