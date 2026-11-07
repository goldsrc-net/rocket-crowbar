# Installing

The rebuild replaces one file of a Rocket Crowbar 1.9 server: its game library. Everything else (rc.ini,
the per-map `.lec` and `.cfg` files, the models, sounds and sprites) is RC 1.9's own and stays as it is.
Players need nothing new: RC 1.9 has no client library, so they join with Half-Life's client and the
stock RC 1.9 content.

## A server

1. Install RC 1.9 on the server as `rockcrowbar/` next to `valve/` (the release zip's `RockCrowbar`
   folder; on Linux the folder name is lowercase).
2. Copy the rebuild's `hl.so` (`build-i386/dlls/hl.so` for a 32-bit HLDS, `build-amd64/dlls/hl.so` for a
   64-bit one) to `rockcrowbar/dlls/hl.so`.
3. In `rockcrowbar/liblist.gam`, set `gamedll_linux "dlls/hl.so"` (RC 1.9's is `dlls/rc_i386.so`).
4. Start the server with `-game rockcrowbar`.

Going back: point `gamedll_linux` at `dlls/rc_i386.so` again.

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
