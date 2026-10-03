#!/usr/bin/env python3
"""P028 safety regression: the failed P026 writer must never reach I/O.

The historical P027 write tests remain in commit ebb666a. Current production
is withdrawn; these tests run the unmodified production script with a PATH
whose external commands record any access. No board or block device is used.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).with_name("p027_manual_uboot.sh")


class WithdrawnCandidateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="p028-withdrawn-")
        self.root = Path(self.temp.name)
        self.trace = self.root / "external-access.log"
        self.guard = self.root / "commands"
        self.guard.mkdir()
        for command in ("cat", "readlink", "stat", "sha256sum", "blockdev",
                        "findmnt", "uname", "tr", "cp", "dd", "cmp", "mktemp",
                        "flock", "rm", "rmdir"):
            wrapper = self.guard / command
            wrapper.write_text('#!/bin/sh\nprintf "%s\\n" "$0" >> "$P028_TRACE"\nexit 99\n')
            wrapper.chmod(0o755)
        self.env = dict(os.environ, PATH=str(self.guard), P028_TRACE=str(self.trace))

    def tearDown(self):
        self.temp.cleanup()

    def assertWithdrawn(self, args, extra=None):
        result = subprocess.run(["/bin/bash", str(SCRIPT), *args],
                                env=dict(self.env, **(extra or {})),
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("P026_CANDIDATE_WITHDRAWN", result.stderr)
        self.assertNotIn("CHECK_PASS", result.stdout)
        self.assertNotIn("WRITE_VERIFIED", result.stdout)
        self.assertFalse(self.trace.exists(), "external file/device operation reached")

    def test_write_stops_before_nonexistent_file_or_device_access(self):
        self.assertWithdrawn(["--write", "/nonexistent/uboot-host-unsigned.img"])

    def test_check_also_refuses_the_withdrawn_candidate(self):
        self.assertWithdrawn(["--check", "/nonexistent/uboot-host-unsigned.img"])

    def test_environment_cannot_override_withdrawal(self):
        self.assertWithdrawn(["--write", "/nonexistent/candidate.img"],
                             {"AMP_TEST": "1", "P028_ALLOW_WRITE": "1",
                              "amp_mode": "--check"})

    def test_existing_regular_file_is_unchanged(self):
        candidate = self.root / "candidate.img"
        before = b"do not modify recovery input"
        candidate.write_bytes(before)
        self.assertWithdrawn(["--write", str(candidate)])
        self.assertEqual(candidate.read_bytes(), before)

    def test_no_arguments_has_no_external_effects(self):
        self.assertWithdrawn([])


if __name__ == "__main__":
    unittest.main(verbosity=2)
