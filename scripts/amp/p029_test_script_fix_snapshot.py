#!/usr/bin/env python3
"""Host-only checks of repair FD snapshot rejection; never call install()."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
INSTALLER = ROOT / "scripts/board/p029_install_script_fix.py"


def test(report, installer=INSTALLER):
    spec = importlib.util.spec_from_file_location("repair", installer)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    cases = 0
    for fault in ["regular", "symlink", "fifo", "hardlink", "empty", "oversize", "missing"]:
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp); member = p/"member"; data = b"verified in-memory bytes"
            if fault == "symlink":
                (p/"other").write_bytes(data); member.symlink_to(p/"other")
            elif fault == "fifo":
                os.mkfifo(member)
            elif fault != "missing":
                member.write_bytes(b"" if fault == "empty" else data)
                if fault == "hardlink": os.link(member, p/"other")
                if fault == "oversize": member.write_bytes(b"x"*65)
            fd = os.open(p, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
            try:
                try: snapshot = module.read_member(fd, "member", 64)
                except (OSError, AssertionError):
                    assert fault != "regular"
                else:
                    assert fault == "regular" and snapshot == data
                    member.write_bytes(b"later changed source")
                    assert snapshot == data
            finally: os.close(fd)
            cases += 1
    denied = subprocess.run(["python3", str(installer)], capture_output=True, text=True, timeout=3)
    assert denied.returncode != 0 and "pinned manifest SHA required" in denied.stderr
    cases += 1
    result = {"evidence": "HOST_TESTED", "snapshot_and_no_approval_cases": cases,
              "board_access": False, "persistent_installer_executed": False}
    report.write_text(json.dumps(result, indent=2)+"\n"); print(json.dumps(result, indent=2))


if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("--report", type=Path, required=True)
    ap.add_argument("--installer", type=Path, default=INSTALLER)
    args = ap.parse_args(); test(args.report, args.installer)
