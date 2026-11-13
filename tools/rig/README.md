# tools/rig - running the original and the rebuild side by side

Two dedicated servers on this machine, both with gamedir `rockcrowbar`, so one client folder
joins either:

```
original RC 1.9 (reference/rc_i386.so)   tools/rig/run.sh original   port 27031
our rebuild (build-i386/dlls/hl.so)      tools/rig/run.sh ours       port 27032

steam -applaunch 70 -game rockcrowbar +connect 127.0.0.1:<port>   (or "connect" in the console)
```

Set up or refresh with `tools/rig/setup.sh` after `scripts/build.sh i386`. It copies our
latest `hl.so` every time and creates the rest only if missing.

## What it sets up

- `~/Containers/hlds/hlds-linux32-test/rockcrowbar/`: the v19 content and the original library.
  The rig is a 32-bit ReHLDS install (goldsrc-net buildchain drop-in) shared with other work;
  only this gamedir is added.
- `~/Containers/hlds/hlds-linux32-rcours/`: symlinks to every entry of that rig, plus its own
  real `rockcrowbar/` with our `hl.so`. Writes into `valve/` (logs, banned.cfg) land in the shared
  rig.
- `~/.local/share/Steam/steamapps/common/Half-Life/rockcrowbar/`: the client's mod folder, the
  v19 content with the player's own `valve/config.cfg` (v19 ships a server-style `config.cfg`
  with one bind, which leaves the client with no controls).
- Server-side changes from stock v19: `sv_cheats 1` in `server.cfg`, and `pak0.pak` unpacked
  into loose files (see below). Everything else is the release as shipped.

## Why the extra pieces

- **`egcs_eh_shim.so`** (source `egcs_eh_shim.c`, built by setup.sh in the buildchain image):
  the original was built with egcs 1.1.2 and imports `__start_cp_handler`, which old Linux
  libgcc exported and current systems don't, so the loader refuses it. The shim provides it;
  the original only calls it on an exception-spec violation path. Preloaded for the original
  only.
- **pak0.pak unpacked:** the rig's `filesystem_stdio.so` (ReHLDS, from the buildchain) crashes
  in `CBaseFileSystem::PreparePackFile` on any gamedir with a pak (`FS_fread` gets a bogus
  `FILE*`). That's a buildchain bug, not RC's; the rig's `valve/` has no pak, so nothing hit it
  before. The Steam client reads paks fine, so its folder keeps the pak.
- **`-nobreakpad`** in run.sh: without it the engine's breakpad handler uploads a minidump to
  Valve's crash server whenever the server crashes.

## Smoke test

`tools/rig/smoke.py` boots one server headless, changes map twice, quits, and saves the log;
`--clean` strips run-specific noise so two logs diff. The rig for it is the first one (gamedirs
`rockcrowbar` and `rcours` there; see its docstring).

## Booting a package

`tools/rig/boot-package.sh win|linux <package>/rockcrowbar <RC 1.9's rockcrowbar folder> <map>`
boots a `reRC-<rev>` package's server (BUILDING.md, Packaging) under its shipped name and passes
when it answers a server query on port 27045. `win` runs `rc.dll` in the Windows HLDS under Wine
(`HLDS_WIN`, a Windows HLDS install, e.g. fetched with steamcmd as app 90 for platform windows),
`linux` the i386 server on this rig. It makes a `rockcrowbar-boot` gamedir there and removes it
after. The rig's `rockcrowbar/` works as the content (its pak is unpacked) with the release zip's
liblist.gam (`LIBLIST=`), since setup.sh writes its own. RC 1.9 ships no maps of its own and plays
Half-Life's, so give it one from `valve/`, e.g. `crossfire`.

## Undo

Move these three away (nothing else is touched):

```
~/Containers/hlds/hlds-linux32-test/rockcrowbar
~/Containers/hlds/hlds-linux32-rcours
~/.local/share/Steam/steamapps/common/Half-Life/rockcrowbar
```
