#!/usr/bin/env bash
# run.sh original|ours [map] - run one of the two test servers in the foreground (Ctrl-C stops it).
#
#   original  ~/Containers/hlds/hlds-linux32-test   port 27031   reference/rc_i386.so
#   ours      ~/Containers/hlds/hlds-linux32-rcours port 27032   build-i386/dlls/hl.so
#
# Join: steam -applaunch 70 -game rockcrowbar +connect 127.0.0.1:<port>
# -nobreakpad: the engine's crash handler otherwise uploads minidumps to Valve.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
MAP="${2:-crossfire}"
case "${1:-}" in
	original)
		cd "${RIG:-$HOME/Containers/hlds/hlds-linux32-test}"
		export LD_PRELOAD="$HERE/egcs_eh_shim.so"
		PORT=27031 ;;
	ours)
		cd "${RIG2:-$HOME/Containers/hlds/hlds-linux32-rcours}"
		PORT=27032 ;;
	*) sed -n '2,9p' "$0"; exit 2 ;;
esac
export LD_LIBRARY_PATH=".:${LD_LIBRARY_PATH:-}"
exec ./hlds_linux -nobreakpad -game rockcrowbar +developer 1 +sv_cheats 1 +maxplayers 8 +sv_lan 1 \
	-port "$PORT" +map "$MAP"
