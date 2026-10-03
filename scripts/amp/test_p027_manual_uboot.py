#!/usr/bin/env python3
"""Exercise the real shell workflow on isolated Host files, never on devices.

Only a disposable script copy has its hardware paths, EUID/block checks and
expected fixture hashes substituted. Production offers no device/test override.
dd copies real files; its test wrapper records writes and injects failures.
This does not prove eMMC I/O, cold boot, or firmware compatibility.
"""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).with_name("p027_manual_uboot.sh")
MIB = 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


class ManualUbootTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="p027-host-test-")
        self.root = Path(self.temp.name)
        self.run = self.root / "run"
        (self.run / "lock").mkdir(parents=True)
        self.boot = self.root / "boot"
        (self.boot / "dtb").mkdir(parents=True)
        (self.boot / "uEnv").mkdir()
        self.device = self.root / "mock-partition.raw"
        self.original = b"O" * (4 * MIB) + bytes(4 * MIB)
        self.candidate = b"N" * (4 * MIB)
        self.device.write_bytes(self.original)
        self.image = self.root / "candidate.img"
        self.image.write_bytes(self.candidate)
        self.label = self.root / "uboot"
        self.label.symlink_to(self.device)
        self.model = self.root / "model"
        self.model.write_bytes(b"EmbedFire LubanCat-3-v2\0")
        self.start = self.root / "start"
        self.start.write_text("16384\n")
        self.size = self.root / "size"
        self.size.write_text("16384\n")
        fixtures = {
            "Image-6.1.99-rk3576": "e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e",
            "initrd.img-6.1.99-rk3576": "425d2a68a4a67e807518c067cfefc1111d0a33af02236761ddaa3794c3205397",
            "dtb/rk3576-lubancat-3-v2.dtb": "76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90",
            "uEnv/uEnv.txt": "4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64",
            "boot.scr": "c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325",
            "boot.cmd": "9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55",
        }
        source = SCRIPT.read_text()
        self.assertNotIn("AMP_TEST", source)
        self.assertNotIn("eval ", source)
        for filename, original_hash in fixtures.items():
            content = ("fixture:" + filename).encode()
            (self.boot / filename).write_bytes(content)
            source = source.replace(original_hash, digest(content))
        for link, target in {
            "Image": "Image-6.1.99-rk3576",
            "initrd": "initrd.img-6.1.99-rk3576",
            "rk-kernel.dtb": "dtb/rk3576-lubancat-3-v2.dtb",
        }.items():
            (self.boot / link).symlink_to(target)
        replacements = {
            "$EUID == 0": "1 == 1",
            '&& -b "$amp_device"': '&& -f "$amp_device"',
            "amp_model_file=/sys/firmware/devicetree/base/model": "amp_model_file=" + str(self.model),
            "amp_label=/dev/disk/by-partlabel/uboot": "amp_label=" + str(self.label),
            "amp_expected_device=/dev/mmcblk0p1": "amp_expected_device=" + str(self.device),
            "amp_start_file=/sys/class/block/mmcblk0p1/start": "amp_start_file=" + str(self.start),
            "amp_size_file=/sys/class/block/mmcblk0p1/size": "amp_size_file=" + str(self.size),
            "amp_boot=/boot": "amp_boot=" + str(self.boot),
            "amp_run=/run": "amp_run=" + str(self.run),
            "4b6e615beba307d95beff1cbbff76b06bfe794f9a2f269599d2ccd5026ca4f1f": digest(self.candidate),
            "ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a": digest(self.original),
            "2f6d9a427bbde0d658fbd55e9617a8d119f4d9e442f400ca126a21a095c08eea": digest(self.candidate + self.original[4 * MIB:]),
        }
        for old, new in replacements.items():
            self.assertEqual(source.count(old), 1, old)
            source = source.replace(old, new)
        self.test_script = self.root / "workflow.sh"
        self.test_script.write_text(source)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.trace = self.root / "writes.log"
        self.env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
                        MOCK_DEVICE=str(self.device), MOCK_WRITES=str(self.trace))
        helpers = {
            "uname": '#!/bin/sh\nprintf "%s\\n" 6.1.99-rk3576\n',
            "findmnt": "#!/usr/bin/env python3\nimport os,sys\na=sys.argv[1:]\n"
                        "if '-S' in a: sys.exit(0 if os.getenv('MOCK_MOUNTED') else 1)\n"
                        "if 'FSTYPE' in a: print('tmpfs')\n"
                        "elif a[-1]=='/': print('/dev/mmcblk0p3')\n"
                        "else: print('/dev/mmcblk0p2')\n",
            "blockdev": "#!/usr/bin/env python3\nimport os,sys\na=sys.argv[1]\n"
                        "if a=='--getss': print('512')\n"
                        "elif a=='--getsize64': print(os.path.getsize(sys.argv[2]))\n"
                        "elif a=='--getro': print('0')\n"
                        "else: sys.exit(9)\n",
            "dd": "#!/usr/bin/env python3\nimport os,sys,subprocess\na=sys.argv[1:]\n"
                  "p=dict(x.split('=',1) for x in a if '=' in x)\n"
                  "if p.get('of')==os.environ['MOCK_DEVICE']:\n"
                  " with open(os.environ['MOCK_WRITES'],'a') as f: f.write('write\\n')\n"
                  " if os.getenv('MOCK_DD_FAIL'): sys.exit(5)\n"
                  "a=[x.replace('fullblock,direct','fullblock') for x in a]\n"
                  "r=subprocess.run(['/bin/dd']+a)\n"
                  "if os.getenv('MOCK_BAD_READBACK') and p.get('of','').endswith('/after.raw'):\n"
                  " with open(p['of'],'r+b') as f: f.write(b'!')\n"
                  "if os.getenv('MOCK_BAD_TAIL') and p.get('of','').endswith('/after.raw'):\n"
                  " with open(p['of'],'r+b') as f: f.seek(4194304); f.write(b'!')\n"
                  "sys.exit(r.returncode)\n",
        }
        for name, content in helpers.items():
            path = self.bin / name
            path.write_text(content)
            path.chmod(0o755)

    def tearDown(self):
        self.temp.cleanup()

    def invoke(self, mode="--check", **extra):
        return subprocess.run(["bash", str(self.test_script), mode, str(self.image)],
                              env=dict(self.env, **extra), capture_output=True, text=True,
                              timeout=30)

    def assertNoWrite(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.device.read_bytes(), self.original)
        self.assertFalse(self.trace.exists())

    def test_check_does_not_write(self):
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("CHECK_ONLY", result.stdout)
        self.assertEqual(self.device.read_bytes(), self.original)
        self.assertFalse(self.trace.exists())

    def test_write_exact_prefix_and_keep_tail(self):
        result = self.invoke("--write")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("WRITE_VERIFIED", result.stdout)
        self.assertEqual(self.device.read_bytes(), self.candidate + self.original[4 * MIB:])
        self.assertEqual(self.trace.read_text(), "write\n")

    def test_wrong_candidate_hash(self):
        self.image.write_bytes(b"X" * (4 * MIB))
        self.assertNoWrite(self.invoke("--write"))

    def test_short_candidate(self):
        self.image.write_bytes(b"X")
        self.assertNoWrite(self.invoke("--write"))

    def test_original_changed(self):
        changed = bytearray(self.original)
        changed[-1] = 1
        self.device.write_bytes(changed)
        result = self.invoke("--write")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.device.read_bytes(), bytes(changed))
        self.assertFalse(self.trace.exists())

    def test_wrong_start(self):
        self.start.write_text("32768\n")
        self.assertNoWrite(self.invoke("--write"))

    def test_wrong_size(self):
        self.size.write_text("32768\n")
        self.assertNoWrite(self.invoke("--write"))

    def test_wrong_label_target(self):
        other = self.root / "wrong.raw"
        other.write_bytes(self.original)
        self.label.unlink()
        self.label.symlink_to(other)
        self.assertNoWrite(self.invoke("--write"))
        self.assertEqual(other.read_bytes(), self.original)

    def test_mount_rejected(self):
        self.assertNoWrite(self.invoke("--write", MOCK_MOUNTED="1"))

    def test_modified_boot_config(self):
        (self.boot / "uEnv/uEnv.txt").write_text("changed")
        self.assertNoWrite(self.invoke("--write"))

    def test_write_error_does_not_reboot_or_retry(self):
        result = self.invoke("--write", MOCK_DD_FAIL="1")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("do not reboot", result.stderr)
        self.assertEqual(self.trace.read_text(), "write\n")
        self.assertEqual(self.device.read_bytes(), self.original)

    def test_readback_error_never_reports_success(self):
        result = self.invoke("--write", MOCK_BAD_READBACK="1")
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("WRITE_VERIFIED", result.stdout)
        self.assertIn("do not reboot", result.stderr)
        self.assertEqual(self.trace.read_text(), "write\n")

    def test_changed_tail_never_reports_success(self):
        result = self.invoke("--write", MOCK_BAD_TAIL="1")
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("WRITE_VERIFIED", result.stdout)
        self.assertIn("last 4MiB changed", result.stderr)
        self.assertIn("do not reboot", result.stderr)

    def test_real_artifact_and_original_match_embedded_hashes(self):
        project = SCRIPT.parents[2]
        package = project / "artifacts/local/p026-loader-package-final-v6/uboot-host-unsigned.img"
        original = project / "artifacts/local/p026-approved-read-20261002T185533Z-1008600/uboot-original.raw"
        if not package.is_file() or not original.is_file():
            self.skipTest("large local P026 artifacts are not distributed through Git")
        candidate, raw = package.read_bytes(), original.read_bytes()
        source = SCRIPT.read_text()
        self.assertEqual(len(candidate), 4 * MIB)
        self.assertEqual(len(raw), 8 * MIB)
        self.assertIn(digest(candidate), source)
        self.assertIn(digest(raw), source)
        self.assertIn(digest(candidate + raw[4 * MIB:]), source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
