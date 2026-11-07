# Brief, wave 2: RC's changes inside the SDK's own files

Read `AGENT_BRIEF.md` (same folder) in full first: paths, the decompile, how to read it, style,
shared-file rules, checking and finishing all apply. This adds what is different in wave 2.

## What's already done

All 30 of RC's own source files are reconstructed (wave 1, merged into `main`). Their dossiers in
`reference/dossiers/*.md` are worth reading for your files: each one's **section 5 (cross-file
needs)** lists the calls their code expects from SDK files, with the original caller's address.
Those callers are yours if they're in your files. The RC functions are real now; call them, don't
stub them. Useful headers: `driveent.h` (`CBaseDriveEnt`, `m_pOwner`), `Preff.h`, `vote.h`,
`pregame.h`, `curses.h`, `flag.h`, `deathballs.h`, `levelcustom.h`, `nm.h`, `wog.h`,
`pointexplo.h`, `drive*.h` (each `CreateDrv*Spawner()`), `bot_prefs.h`, `mdhash.h`.

Also already in the tree: RC's player members (`player.h`: the saved ones plus the observer,
vote and `m_fDrvEntAlpha` members), the RC virtuals on `CBaseEntity`/`CBaseMonster`/weapons,
`LINK_ENTITY_TO_CLASS` calling `AddToPrecachedList`, and `CGrenade::ShootDETimed`.

## Your job is different this time

Your files already exist: they're halflife-updated's stock versions. For each one:

1. **Add every function RC added** (`check_file.py <file> --missing` lists them, as before).
2. **Bring every stock function's body in line with the original.** RC changed stock bodies too
   (the crowbar fires rockets from `CCrowbar::PrimaryAttack`, weapons read `rc.ini` through
   `PREFS_*`, monsters gained RC behavior). `reference/rc_touch.tsv` lists the stock functions
   whose original body uses an RC name; that's a lead list, not complete (a changed constant or
   a removed line references nothing new). For each function, compare the original's decompile
   against our source and port the differences. Keep halflife-updated's own fixes and modern
   style where the original's behavior is the same; where they differ, the original's behavior
   wins and the halflife-updated change goes in your dossier.
3. **Weapon files:** review every function. **Files where RC added little** (most monsters):
   review the functions in `rc_touch.tsv` and the ones RC added, then read the rest of the
   file's decompiles quickly for anything non-stock. Say in the dossier which functions you
   compared and which you only skimmed.
4. **Remove a `TEMP STUB` that belongs to your files** (listed in your package prompt) by
   writing the real function, and fix its callers' comments.

HL client weapon prediction: halflife-updated predicts weapons client-side (`cl_dll` shares the
weapon code). RC 1.9 is server-only and its players run Half-Life's own client, so predicted
weapon code isn't RC's concern; reconstruct the server behavior and note anything that would
mispredict on a stock client in the dossier.

## Shared files in wave 2

Several packages edit `weapons.h`, `player.h` and `cbase.h`. Keep each edit inside your own
classes' declarations, or in a `// <package> needs:` block before a shared class's `};` as
before; the merge is file by file. Don't reformat or reorder anything you don't change.
