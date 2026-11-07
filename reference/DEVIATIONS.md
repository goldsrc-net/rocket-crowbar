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
