# Installing

The rebuild replaces one thing of a Rocket Crowbar 1.9 server: its game library. Everything else (rc.ini,
the per-map `.lec` and `.cfg` files, the models, sounds and sprites) is RC 1.9's own and stays as it is.
Players need nothing new: RC 1.9 has no client library, so they join with Half-Life's client and the
stock RC 1.9 content.

## A server

1. Install RC 1.9 on the server as `rockcrowbar/` next to `valve/` (the release zip's `RockCrowbar`
   folder; on Linux the folder name is lowercase).
2. Copy the `rockcrowbar/` folder of a `reRC-<rev>` package (CI's artifact or release, or
   `scripts/package.sh`, [BUILDING.md](BUILDING.md)) over the server's `rockcrowbar/`. It holds the
   server under RC 1.9's own names: `dlls/rc.so`, `rc_i386.so` and `rc_i686.so` for Linux (Steam's
   current engine loads `rc.so`, older ones `rc_i386.so`, Metamod-R `rc_i686.so`), `dlls/rc.dll` for
   Windows, and the 64-bit servers. RC 1.9's `liblist.gam` already names `dlls\rc.dll` and
   `dlls/rc_i386.so`, so it stays as it is. The exception is a 64-bit ReHLDS, which loads
   `gamedll_linux` as written: on one, point it at `dlls/rc_amd64.so` (x86-64) or
   `dlls/rc_arm64.so` (ARM). Xash3D finds the 64-bit name from the line by itself.
3. Start the server with `-game rockcrowbar`.

Going back: put the original `rc_i386.so` and `rc.dll` back in `rockcrowbar/dlls/`, and delete `rc.so`,
`rc_i686.so` and the 64-bit servers, which RC 1.9 never had (Steam's engine would go on loading
`rc.so`).

Two things to know about current engines:

- On a 32-bit HLDS the original `rc_i386.so` itself only loads with the `tools/rig/egcs_eh_shim.so`
  shim preloaded (it imports a symbol old Linux libgcc exported); the rebuild needs nothing.
- The goldsrc-net ReHLDS file system crashes on a gamedir with a `pak0.pak`; unpack RC's pak into loose
  files there (tools/rig/README.md). Steam's HLDS and the Steam client read it fine.

## Running the original and the rebuild side by side

`tools/rig/setup.sh` sets up two servers on one machine, the original on port 27031 and the rebuild on
27032, both as `rockcrowbar`, and the client's mod folder; `tools/rig/run.sh original|ours` starts
them. Join with:

```
steam -applaunch 70 -game rockcrowbar +connect 127.0.0.1:<port>
```

[tools/rig/README.md](tools/rig/README.md) has the details and how to undo it.
