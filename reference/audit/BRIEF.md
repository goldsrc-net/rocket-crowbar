# Audit: halflife-updated's changes inside RC 1.9's stock-named functions

Our tree is a fork of halflife-updated. A function the original `rc_i386.so` has under an SDK name
counted as "stock" here when halflife-updated has a function of that name
(`scripts/sdk_baseline.sh`), so nobody compared its body with what RC 1.9 actually shipped. RC 1.9
was built on Half-Life SDK 2.1 (its strings match 2.1's code, not 2.2's or 2.3's). `sdk-drift.tsv`
here lists the 668 functions the original has whose body in our tree differs from SDK 2.1's
(`scripts/stock_drift.py --sdk <SDK 2.1 src> --loose
reference/audit/names-rc19.tsv`; columns rva, file, name). Each package `rN` owns the files in
`rN.files`.

Decided by Steven, 2026-11-07, the same pass mod-rc2 ran (`../mod-rc2/reference/audit/BRIEF.md`,
read it): put the original's behavior back wherever halflife-updated's shows in multiplayer play;
keep the rest, documented.

## The difference from mod-rc2's audit: the original decides, not the SDK

RC2's stock functions are byte for byte SDK 2.3, so there SDK 2.3 is the original. RC 1.9 was
built with gcc 2.95, which we can't byte-match, so a function with an SDK name may carry RC's own
changes. The authority is the original's typed decompile, `recovered/decomp/<Class>.c` (one file
per class, every function by name; `scripts/ghidra/re.sh at <addr>` for one, with the address
from `sdk-drift.tsv`, which is the `.so`'s). Read three bodies for each function: SDK 2.1's
(`<SDK 2.1 src>/<same path>`), ours, and the original's.

```
the original does          ours does                  class     action
what SDK 2.1 does          halflife-updated's change  play/...  as mod-rc2's classes: style, guard,
                                                                sp, engine leave; play: put SDK
                                                                2.1's behavior back
something else (RC's own)  SDK 2.1's or halflife-     missed    reconstruct the original's (it is
                           updated's                            RC's change nobody did)
what ours does             (RC's change, already      done      leave
                           reconstructed)
```

A function carrying `// orig 0x...` was reconstructed from the original by an earlier package;
check it the same way, since it may have kept halflife-updated's code in places. `missed` ones are
new reconstruction: follow `reference/dossiers/AGENT_BRIEF.md` (tags, `// VERIFY`, keep the
original's bugs; crashes get guarded, as `reference/ORIGINAL_BUGS.md`'s policy says).

A `play` revert or a `missed` fix is written in the surrounding halflife-updated style with the
original's logic, and a comment at the change: `// RC 1.9's (as SDK 2.1); halflife-updated <what it
did>` or `// RC 1.9's own: <what>`. Keep existing tags.

Not yours to change, already decided (read `reference/DEVIATIONS.md` in full first): today's
client's message sizes, the kept halflife-updated cvars and commands, `func_vehicle`, the crashes
already guarded. If a function touches these, classify it `engine` and point at the entry.

When it isn't clear whether a change shows in play, say so (`unclear`), keep what's there, and give
the reason; Steven decides those.

## Rules

- Work only in your worktree `~/Projects/goldsrc.net/mod-rockcrowbar-wt/<rN>` and only in your
  files (a shared header only if a fix forces it; say so). No commits: commits are gpg-signed, the
  merge commits.
- Don't edit `reference/DEVIATIONS.md` or `ORIGINAL_BUGS.md`: put what you propose for them in your
  report.
- `build-*/`, `recovered/` and the SDK trees exist only in the main checkout
  (`~/Projects/goldsrc.net/mod-rockcrowbar`); read them from there by absolute path.

## Finish line

- `scripts/build.sh i386` and `scripts/build.sh amd64` pass (one build at a time; build.sh takes a
  lock).
- `python3 scripts/check_file.py <each file you changed>` reports nothing missing.
- `reference/audit/<rN>.md`: one row per function (rva, file, name, class, what differs in one
  line, action); then "For DEVIATIONS.md" and "For ORIGINAL_BUGS.md" lines; then anything odd.
- A short report back: counts per class, each `play` revert and `missed` fix in a line, the
  `unclear` ones.
