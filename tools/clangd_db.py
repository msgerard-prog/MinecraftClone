#!/usr/bin/env python3
"""Make MSVC's compile_commands.json usable by the Linux clangd in WSL.

Reads out/build/<preset>/compile_commands.json (Windows paths, cl.exe flags), rewrites
paths to /mnt/c/..., adds the MSVC/Windows SDK include folders as -imsvc and a
windows-msvc target, and writes ./compile_commands.json (git-ignored).
Run automatically by tools/build.sh; run by hand with: tools/clangd_db.py [preset]
"""
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
preset = sys.argv[1] if len(sys.argv) > 1 else "debug"
src = ROOT / "out" / "build" / preset / "compile_commands.json"
cache = ROOT / "out" / "msvc_include.txt"


def to_wsl(path: str) -> str:
    m = re.match(r"^([A-Za-z]):[\\/](.*)$", path)
    if not m:
        return path.replace("\\", "/")
    return f"/mnt/{m.group(1).lower()}/" + m.group(2).replace("\\", "/")


WIN_PATH = re.compile(r"[A-Za-z]:[\\/][^\s\"]*")


def split_cl(command: str) -> list[str]:
    """Split a Windows command line: quotes group words, \\" is a literal quote."""
    command = command.replace('\\"', "\x00")
    tokens = re.findall(r'(?:[^\s"]|"[^"]*")+', command)
    return [t.replace('"', "").replace("\x00", '"') for t in tokens]


def msvc_includes() -> list[str]:
    if not cache.exists():
        out = subprocess.run(["cmd.exe", "/d", "/c", r"tools\win\print_include.cmd"],
                             cwd=ROOT, capture_output=True, text=True).stdout
        cache.write_text(out.strip().splitlines()[-1] if out.strip() else "")
    return [to_wsl(p) for p in cache.read_text().split(";") if p.strip()]


def main() -> int:
    if not src.exists():
        print(f"clangd_db: {src} missing (build first)", file=sys.stderr)
        return 0
    extra = ["--target=x86_64-pc-windows-msvc", "-fms-compatibility"]
    extra += [f"/imsvc{p}" for p in msvc_includes()]
    entries = []
    for e in json.loads(src.read_text()):
        args = [WIN_PATH.sub(lambda m: to_wsl(m.group(0)), a) for a in split_cl(e["command"])]
        args = [a for a in args[1:] if not a.startswith("@") and a != "/showIncludes"]
        entries.append({"directory": to_wsl(e["directory"]), "file": to_wsl(e["file"]),
                        "arguments": ["clang-cl"] + args + extra})
    (ROOT / "compile_commands.json").write_text(json.dumps(entries, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
