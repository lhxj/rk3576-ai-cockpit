"""Host safety gates for additive test-fixture installer; no board access."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT/'scripts/board/system_coexistence/install_health_fixture.py'
spec = importlib.util.spec_from_file_location('si_health_installer',SOURCE)
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class InstallerSafetyTest(unittest.TestCase):
    def test_no_approval_rejects_before_target_access(self):
        result = subprocess.run(['python3',str(SOURCE)],capture_output=True,text=True,timeout=10)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('STOP explicit user approval',result.stdout+result.stderr)

    def test_extra_argument_never_reaches_install(self):
        result = subprocess.run(['python3',str(SOURCE),'--approved-SI_HEALTH_V1',
                                 '/dev/nonexistent-test-path','extra'],
                                capture_output=True,text=True,timeout=10)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('STOP explicit user approval',result.stdout+result.stderr)

    def test_packet_missing_members_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaisesRegex(RuntimeError,'exact seven-file packet'):
                installer.validate_packet(Path(folder))

    def test_pinned_manifest_not_replaced_by_another_valid_json(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for name in installer.MEMBERS:
                (root/name).write_bytes(b'{}\n')
            with self.assertRaisesRegex(RuntimeError,'pinned manifest'):
                installer.validate_packet(root)

    def test_symlink_payload_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root/'target').write_text('not a key or device')
            (root/'link').symlink_to(root/'target')
            with self.assertRaises(OSError):
                installer.read_regular(root/'link')


if __name__ == '__main__':
    unittest.main()
