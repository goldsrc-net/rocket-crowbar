#!/usr/bin/env python3
"""smoke.py - boot a gamedir on the 32-bit HLDS rig, drive its console, save the log.

Runs the original RC (gamedir rockcrowbar, with egcs_eh_shim.so preloaded) or our build
(gamedir rcours) the same way, so the two logs can be diffed:

  tools/rig/smoke.py rockcrowbar out-orig.log
  tools/rig/smoke.py rcours out-ours.log
  diff <(tools/rig/smoke.py --clean out-orig.log) <(tools/rig/smoke.py --clean out-ours.log)

The script: start hlds_linux (no breakpad, developer 1, LAN), wait for the map, then send each
console command in SCRIPT after its delay, end with "quit", and wait for the exit. Commands go
in on stdin, which HLDS reads as console input.

Rig: ~/Containers/hlds/hlds-linux32-test (32-bit ReHLDS). Its filesystem_stdio crashes on pak
files, so the gamedirs there have RC's pak0.pak unpacked. See tools/rig/README.md.
"""
import argparse, os, re, subprocess, sys, threading, time

RIG = os.path.expanduser("~/Containers/hlds/hlds-linux32-test")
HERE = os.path.dirname(os.path.abspath(__file__))
SHIM = os.path.join(HERE, "egcs_eh_shim.so")

# (seconds after the previous command, command)
SCRIPT = [
    (8, "status"),
    (2, "changelevel stalkyard"),
    (10, "status"),
    (2, "changelevel bounce"),
    (10, "status"),
    (2, "quit"),
]


def clean(path):
    """A log with run-specific noise removed, for diffing two runs."""
    out = []
    for line in open(path, errors="replace"):
        line = re.sub(r"\d\d/\d\d \d\d:\d\d:\d\d", "", line)
        line = re.sub(r"\b(rockcrowbar|rcours)\b", "GAME", line)
        line = re.sub(r"(rc_i386|hl)\.so", "GAMEDLL.so", line)
        line = re.sub(r"\d+(\.\d+)?ms", "Nms", line)
        if re.search(r"ELFCLASS32|minidump|^\s*$", line):
            continue
        out.append(line.rstrip("\n"))
    print("\n".join(out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("game", nargs="?")
    ap.add_argument("log", nargs="?")
    ap.add_argument("--map", default="crossfire")
    ap.add_argument("--clean", metavar="LOG")
    a = ap.parse_args()
    if a.clean:
        return clean(a.clean)
    env = dict(os.environ, LD_LIBRARY_PATH=".:" + os.environ.get("LD_LIBRARY_PATH", ""))
    if a.game == "rockcrowbar":
        env["LD_PRELOAD"] = SHIM
    cmd = ["./hlds_linux", "-nobreakpad", "-game", a.game, "+developer", "1", "+maxplayers", "8",
           "+sv_lan", "1", "-port", "27031", "+map", a.map]
    log = open(a.log, "w")
    p = subprocess.Popen(cmd, cwd=RIG, env=env, stdin=subprocess.PIPE, stdout=log,
                         stderr=subprocess.STDOUT, text=True)

    def feed():
        for delay, c in SCRIPT:
            time.sleep(delay)
            if p.poll() is not None:
                return
            p.stdin.write(c + "\n")
            p.stdin.flush()
    threading.Thread(target=feed, daemon=True).start()
    try:
        rc = p.wait(timeout=sum(d for d, _ in SCRIPT) + 30)
    except subprocess.TimeoutExpired:
        p.kill()
        rc = "killed (no exit after quit)"
    log.close()
    print(f"{a.game}: exit {rc}")


if __name__ == "__main__":
    sys.exit(main())
