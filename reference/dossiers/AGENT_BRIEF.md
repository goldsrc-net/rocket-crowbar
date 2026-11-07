# Brief: reconstructing a Rocket Crowbar 1.9 work package

You are rebuilding part of the original Rocket Crowbar 1.9 server game library (`rc_i386.so`,
2001, gcc 2.95, not stripped) as modern C++ on the halflife-updated SDK. Your package is a set of
the original's own source files; other agents do the other files in parallel, each in its own
git worktree. The bar is identical behavior. Exact binary layout is NOT a goal.

## Where things are

- **Your worktree** (edit only here): `~/Projects/goldsrc.net/mod-rockcrowbar-wt/<package>`, branch
  `wt/<package>`. The main checkout is `~/Projects/goldsrc.net/mod-rockcrowbar`; don't edit it.
- **Read first, in full:** the worktree's `CLAUDE.md`, `reference/README.md`,
  `reference/DEVIATIONS.md`, `reference/dossiers/README.md` (the dossier format), and your
  files' sections of `reference/RECONSTRUCTION.md`.
- **The decompile** (typed, read-only, shared): `~/Projects/goldsrc.net/mod-rockcrowbar/recovered/decomp/`
  — one `<Class>.c` per class, free functions in `_free.c`. Each function is headed
  `/* Class::Method  @ <ghidra addr>  (<bytes>) */` then `/* <original file>.cpp */`. Free
  functions keep their gcc2 names (`UTIL_IsValidEntity__FP7edict_s`). Do NOT run Ghidra
  (`scripts/ghidra/re.sh`): one project, one lock, eight agents.
- **Tables** in `reference/`: `funcfile.tsv` (every original function: address, size, file, class,
  method, signature; filter on your files), `members.tsv`/`savefields.tsv` (saved members: real
  names, types, the original's offsets), `vtables.tsv` (class, slot, method, implementer, args),
  `hierarchy.tsv` (each class's base), `signatures-gcc2.txt`, `signatures-rc.txt` (MSVC exports
  with return types), `cvars.tsv`. `python3 scripts/vtables.py --new <Class>` lists the virtuals a
  class introduces over its base.
- **The binary** for when the decompile is unclear: `objdump -d --no-show-raw-insn -C
  --start-address=0x<a> --stop-address=0x<b> reference/rc_i386.so`. Ghidra address = file address
  + 0x10000 (funcfile.tsv has file addresses).

## Reading the decompile

- `this->vtbl->Method(this, ...)` is a virtual call, already resolved. Where it isn't (an untyped
  pointer): the vtable pointer sits at `+0x30` in every entity, and a call through `vptr + o` is
  slot `o/4 - 2` in `vtables.tsv` for the pointer's class.
- `this->field_0xNN` is an unsaved member; name it from what it does, keep the original offset in
  a comment (`// orig +0x2a4`), and add `VERIFY name` when it's a guess. Saved members have their
  real names already (`this->m_pOwner`).
- `(*g_engfuncs_pfnXxx)(...)` is the engine call: write the SDK macro (`pfnMessageBegin` →
  `MESSAGE_BEGIN`, `pfnPrecacheModel` → `PRECACHE_MODEL`, `pfnEmitSound` via `EMIT_SOUND_DYN`...).
  `gpGlobals->pStringBase + x` is `STRING(x)`.
- `CONCAT31(extraout_var, b)` is a bool or byte return; take the low byte.
- An edict loop stepping `+ 0x324` is `pEdict++` from `INDEXENT(1)`; `*(int*)edict == 0` is
  `!pEdict->free`; `*(int*)(edict + 0x7c)` is `pvPrivateData` (use `CBaseEntity::Instance`).
- Pointer-to-member think/touch/use are two words: `this->m_pfnThink = X; field_0x10 = 0` is
  `SetThink(X)`.
- FP constants are labelled with their value (`dc_0_1` = 0.1). Booleans in RC are 1-byte `bool`.
- Ghidra sometimes splits a huge function into `FUN_` fragments or loses the end of one; read the
  disassembly for those, don't guess.

## How to write it

- Same file names as the original (your package's `.cpp` files exist as placeholders and are
  already in `dlls/CMakeLists.txt`). Put class declarations in a header of your own when other
  files will need them (`dlls/<file>.h`), else in the `.cpp`.
- Modern SDK style, matching the surrounding halflife-updated code: `bool` returns,
  `override`, `SetThink`/`SetTouch`, `pev->`, `LINK_ENTITY_TO_CLASS`, `DEFINE_FIELD` save tables
  in the original's order with its field types (`savefields.tsv`), `IMPLEMENT_SAVERESTORE`.
- Original names everywhere they exist: classes, methods, saved members, entity classnames,
  keyvalue names, sound/model paths, cvar names. Their typos too (`IsDriveent`, `Dissconecting`).
- Each class's base from `hierarchy.tsv`; its new virtuals in the original's slot order.
- Every function gets a one-line provenance comment with its original address
  (`// orig 0x15a2f0`). Mark every uncertain spot `// VERIFY: <why>`.
- Never invent behavior. If the decompile and the disassembly don't settle it, write the most
  literal reading, mark it VERIFY, and put it in the dossier.
- Already in the tree (don't redo): RC's 46 cvars (`game.h`), the 23 virtuals RC added to
  `CBaseEntity`, `CBaseMonster::IsBot/IsEntEnemy`, the weapon bot interface, `CBaseDriveEnt`
  (`dlls/driveent.h`), `CGameRules::CanPlayerAnnoy`, the saved RC members of `CBasePlayer`
  (`player.h`, incl. `m_pCurDriveEnt`, `m_bInDriveEnt`, `m_pCam`, the curse scales), and the
  cbase.cpp helpers (`GetPullCenter`, `CountEntsClaimedByEnt`, `DispatchDissconnect`, ...).

## Shared files

You own only your package's files and any new header you create. Do not touch `CMakeLists.txt`,
`game.cpp`, `client.cpp`, `multiplay_gamerules.cpp` or other RC packages' files. When you need
something that isn't there:

- **A member on a shared class** (`CBasePlayer`, `CBaseMonster`, ...): add it in a block
  `// <package> needs:` right before the class's closing `};`, and list it in the dossier.
- **A function, global or user message from elsewhere** (an SDK function RC changed, another RC
  file's function, a `gmsg*`): declare it `extern` in your file and, if needed to link, give it a
  temporary definition at the bottom of your own `.cpp` in a block marked
  `// TEMP STUB (<package>): real one belongs in <file>`. List each in the dossier.

## Checking

```sh
cd ~/Projects/goldsrc.net/mod-rockcrowbar-wt/<package>
scripts/build.sh i386            # Docker; a few minutes; run before amd64
scripts/build.sh amd64
python3 scripts/check_file.py <file>.cpp --missing   # must reach 0 missing, or explain each
nm -D build-i386/dlls/hl.so | grep -w <classname>    # each entity factory exported
```

Both arches must build. Eight agents share this machine: one build at a time, no `make -j`
beyond what `build.sh` does. Nothing runs in an engine yet, so "built" is the most you can claim.

## Finishing

- **Don't commit** (commits are gpg-signed by Steven's key and would prompt him). Leave your
  changes uncommitted in the worktree.
- Write `reference/dossiers/<package>.md` in the worktree, in the format of the dossier README.
- Your final message: the worktree path, check_file results per file, both build results, the
  shared-file edits and stubs you made, and the VERIFY count. Keep it short; the dossier has the
  detail.
