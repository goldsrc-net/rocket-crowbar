# Building

CMake builds the server library for every arch. The Linux builds run in the
[goldsrc-net/buildchain](https://github.com/goldsrc-net/buildchain) Docker image (Debian 10, glibc 2.28),
so the result runs on any Linux with glibc 2.28 or newer. Build the image once from a buildchain checkout:

```sh
( cd ../hlds64-buildchain && make image )      # -> hlds64-buildchain:debian10
```

Then, from this repository:

```sh
scripts/build.sh i386      # -> build-i386/dlls/hl.so, for a 32-bit HLDS (and the original's engine)
scripts/build.sh amd64     # -> build-amd64/dlls/hl.so, for a 64-bit HLDS
scripts/build.sh aarch64   # -> build-aarch64/dlls/hl.so, for a 64-bit ARM server
BUILDTYPE=Debug scripts/build.sh i386   # -O0 with debug info
```

`scripts/build.sh` only consumes the image; this repository isn't a buildchain submodule. The server is
built without `CLIENT_WEAPONS`, as RC 1.9's was ([reference/DEVIATIONS.md](reference/DEVIATIONS.md)).

Rocket Crowbar has no client library of its own, so there is nothing client-side to build; the `cl_dll`
tree is Half-Life Updated's and isn't part of the mod.

## Windows

`scripts/build-win.sh` builds the Win32 server on Linux with clang-cl and lld-link, which give MSVC's
ABI, against the MSVC runtime and Windows SDK that xwin downloads (about 300 MB, once):

```sh
cargo install xwin
xwin --accept-license --arch x86 splat --output ~/.cache/xwin/splat-x86
scripts/build-win.sh       # -> build-win32-clangcl/dlls/hl.dll
```

On Windows, MSVC builds the same from the same CMake files: `cmake -B build -A Win32`, then
`cmake --build build --config Release --target hl` (`-A x64` for Win64). CI does that.

## Packaging

`scripts/package.sh` builds the i386 and Win32 servers at the current commit and lays out `reRC-<rev>`
in `dist/`: a patch for a stock RC 1.9 install, the server under RC 1.9's own file names
(`rockcrowbar/dlls/rc.so`, `rc_i386.so`, `rc_i686.so` and `rc.dll`; the README.txt in it says which
engine loads which), with a MANIFEST.tsv of sha256s, and a zip of the lot. It refuses a tree with
uncommitted changes, so a package is a commit; `<rev>` is the `v*` tag from `git describe`, or the
commit. The Linux libraries are stripped to their dynamic exports. `scripts/package.sh --debug` builds
the Debug profile instead, not stripped and with the dll's PDB, as `reRC-<rev>-debug`.

`scripts/assemble-dropin.sh` is the layout step alone, from libraries already built; CI gives it the
64-bit servers as well (`rc_amd64.so`, `rc_arm64.so`, `rc_amd64.dll`, the names Xash builds from the
liblist).

`tools/rig/boot-package.sh win|linux <package>/rockcrowbar <RC 1.9's rockcrowbar folder> <map>` boots a
package's server in the Windows HLDS under Wine or on the 32-bit Linux rig, and passes when it answers
a server query.

## CI

`.github/workflows/ci-cd.yml` builds Linux i386, amd64 and aarch64 in the goldsrc-net build image
(`ghcr.io/goldsrc-net/build-containers/debian10`, the buildchain's), and Win32 and Win64 with MSVC,
on every push that touches more than docs. The package job then runs `scripts/assemble-dropin.sh` on
them and uploads `reRC-<commit>` (the patch, every arch) and `rockcrowbar-gamelibs-all` (the bare
libraries). Run it by hand (Actions, CI/CD, Run workflow) with debug ticked to get the Debug package
too.

`.github/workflows/release.yml`: pushing a tag (`git tag v0.1.0 && git push origin v0.1.0`) builds that
commit through ci-cd.yml and creates a GitHub release with `reRC-<tag>.zip`; a tag with a `-` is a
prerelease. `cmake/Version.cmake` stamps the tag, or `git describe`, into the Windows version
resource.

## Checking a build

```sh
python3 scripts/check_file.py <original source file>.cpp --missing   # every original function present
scripts/prefs_keys.py --ini <RC 1.9's rc.ini>                       # regenerate docs/RC_INI.md; fails if the
                                                                    # original reads a key ours doesn't
scripts/check_docs.py                                               # every cvar and command is in docs/SERVER.md
scripts/check_clientdata.py                                         # UpdateClientData sets every field upstream's does
tools/rig/smoke.py rcours out.log                                   # boot, change maps, quit (tools/rig/README.md)
```

A function being present doesn't mean RC's changes to it are: a stock SDK function RC edited
can still have the stock body. `prefs_keys.py` catches that for anything reading rc.ini (it
compares against the decompile in `recovered/decomp/`).

`scripts/` also regenerates every table in `reference/` from the original library (see each script's
header, and the parent goldsrc.net folder's `DECOMPILE.md` for the order).
