# Building

The server library builds on Linux in the [goldsrc-net/buildchain](https://github.com/goldsrc-net/buildchain)
Docker image (Debian 10, glibc 2.28), so the result runs on any Linux with glibc 2.28 or newer. Build the
image once from a buildchain checkout:

```sh
( cd ../hlds64-buildchain && make image )      # -> hlds64-buildchain:debian10
```

Then, from this repository:

```sh
scripts/build.sh i386      # -> build-i386/dlls/hl.so, for a 32-bit HLDS (and the original's engine)
scripts/build.sh amd64     # -> build-amd64/dlls/hl.so, for a 64-bit HLDS
BUILDTYPE=Debug scripts/build.sh i386   # -O0 with debug info
```

`scripts/build.sh` only consumes the image; this repository isn't a buildchain submodule. CMake is the
build system (`dlls/CMakeLists.txt`). The server is built without `CLIENT_WEAPONS`, as RC 1.9's was
([reference/DEVIATIONS.md](reference/DEVIATIONS.md)).

Rocket Crowbar has no client library of its own, so there is nothing client-side to build; the `cl_dll`
tree is Half-Life Updated's and isn't part of the mod.

## Checking a build

```sh
python3 scripts/check_file.py <original source file>.cpp --missing   # every original function present
tools/rig/smoke.py rcours out.log                                   # boot, change maps, quit (tools/rig/README.md)
```

`scripts/` also regenerates every table in `reference/` from the original library (see each script's
header, and the parent goldsrc.net folder's `DECOMPILE.md` for the order).
