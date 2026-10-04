"""Compile and exercise the exact test KO protocol state machine on Host."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class AmpHealthStateTest(unittest.TestCase):
    def test_bounded_round_trips_fail_closed(self):
        source = ROOT / 'scripts/board/system_coexistence/amp_health_linux/state_test.c'
        with tempfile.TemporaryDirectory(prefix='rk3576-amp-health-host-') as folder:
            binary = Path(folder) / 'state-test'
            result = subprocess.run(
                ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
                 str(source), '-o', str(binary)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('AMP_HEALTH_STATE_HOST_PASS', result.stdout)


if __name__ == '__main__':
    unittest.main()
