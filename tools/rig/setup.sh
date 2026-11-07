#!/usr/bin/env bash
# setup.sh - set up the side-by-side test: the original RC 1.9 and our rebuild on two servers,
# and the client folder to join them with. Idempotent: re-running refreshes our hl.so and
# leaves everything else alone. Undo: tools/rig/README.md.
#
#   tools/rig/setup.sh            needs ~/Downloads/rockcrowbarv19.zip, build-i386/dlls/hl.so
#                                 (scripts/build.sh i386), and the 32-bit rig below
#
# Rig: ~/Containers/hlds/hlds-linux32-test, a 32-bit ReHLDS install with valve/ content.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ZIP="${RC_ZIP:-$HOME/Downloads/rockcrowbarv19.zip}"
RIG="${RIG:-$HOME/Containers/hlds/hlds-linux32-test}"   # runs the original, port 27031
RIG2="${RIG2:-$HOME/Containers/hlds/hlds-linux32-rcours}" # runs ours, port 27032
CLIENT="${CLIENT:-$HOME/.local/share/Steam/steamapps/common/Half-Life}"
OURS="$ROOT/build-i386/dlls/hl.so"
ORIG="$ROOT/reference/rc_i386.so"

for f in "$ZIP" "$OURS" "$RIG/hlds_linux" "$CLIENT/valve/config.cfg"; do
	[ -e "$f" ] || { echo "missing: $f" >&2; exit 1; }
done

# the egcs exception-handler shim the original needs to load (built in the buildchain image)
if [ ! -e "$ROOT/tools/rig/egcs_eh_shim.so" ]; then
	docker run --rm --platform=linux/amd64 --user "$(id -u):$(id -g)" -v "$ROOT/tools/rig":/w \
		hlds64-buildchain:debian10 bash -lc 'cd /w && gcc -m32 -shared -fPIC -O2 -o egcs_eh_shim.so egcs_eh_shim.c'
fi

STAGE="$(mktemp -d)"
trap 'rm -rf -- "${STAGE:?}"' EXIT
(cd "$STAGE" && unzip -q "$ZIP")
SRC="$STAGE/RockCrowbar"
# The rig's filesystem_stdio.so crashes on pak files (README.md), so servers get pak0.pak unpacked.
mkdir "$STAGE/unpacked"
python3 -I - "$SRC/pak0.pak" "$STAGE/unpacked" <<'EOF'
import os, struct, sys
b = open(sys.argv[1], "rb").read(); out = sys.argv[2]
off, ln = struct.unpack_from("<ii", b, 4)
for i in range(ln // 64):
    name, o, s = struct.unpack_from("<56sii", b, off + 64 * i)
    p = os.path.join(out, name.split(b"\0")[0].decode())
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "wb").write(b[o:o + s])
EOF

# Replace a library only when it changed, through a new file renamed into place: a running
# server keeps its mapping of the old one (cp over a mapped .so truncates it under the server).
put_lib() { # <src> <dst>
	cmp -s "$1" "$2" 2>/dev/null && return 0
	cp "$1" "$2.new" && mv -f "$2.new" "$2"
}

server_gamedir() { # <dir> <gamedll path>
	local d="$1"
	if [ ! -d "$d" ]; then
		mkdir -p "$d/dlls"
		rsync -a --exclude 'dlls/' --exclude 'pak0.pak' --exclude 'sort_maps.exe' --exclude 'liblist.gam' "$SRC/" "$d/"
		rsync -a "$STAGE/unpacked/" "$d/"
		sed -i 's/^sv_cheats 0\r\?$/sv_cheats 1\r/' "$d/server.cfg" # cheats on for testing (stock: 0)
	fi
	printf 'game "Rocket Crowbar"\nversion "19"\nsvonly "1"\ncldll "0"\nmpentity "info_player_deathmatch"\ngamedll_linux "%s"\n' "$2" > "$d/liblist.gam"
}

# server 1: the original
server_gamedir "$RIG/rockcrowbar" "dlls/rc_i386.so"
put_lib "$ORIG" "$RIG/rockcrowbar/dlls/rc_i386.so"

# server 2: ours, in a folder of links to the rig with its own rockcrowbar/
if [ ! -d "$RIG2" ]; then
	mkdir "$RIG2"
	for e in "$RIG"/*; do
		b="$(basename "$e")"
		case "$b" in rockcrowbar | rcours) continue ;; esac
		ln -s "$e" "$RIG2/$b"
	done
fi
server_gamedir "$RIG2/rockcrowbar" "dlls/hl.so"
put_lib "$OURS" "$RIG2/rockcrowbar/dlls/hl.so"

# client: the v19 content (pak and all; the Steam client reads paks), with the player's valve binds
if [ ! -d "$CLIENT/rockcrowbar" ]; then
	mkdir "$CLIENT/rockcrowbar"
	rsync -a --exclude 'dlls/' --exclude 'sort_maps.exe' --exclude 'config.cfg' "$SRC/" "$CLIENT/rockcrowbar/"
	cp "$CLIENT/valve/config.cfg" "$CLIENT/rockcrowbar/config.cfg" # v19's has 1 bind
fi

echo "original: $(md5sum "$RIG/rockcrowbar/dlls/rc_i386.so" | cut -c1-12)  ours: $(md5sum "$RIG2/rockcrowbar/dlls/hl.so" | cut -c1-12)"
echo "run: tools/rig/run.sh original | ours"
