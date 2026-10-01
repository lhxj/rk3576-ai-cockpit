#!/usr/bin/env python3
"""Host regression: source mismatches remain detectable while release is blocked."""
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / 'scripts/amp/check_platform_contract.py'
DTS = ROOT / 'scripts/fixtures/rk3576-amp-reference.dtsi'
CONTRACT = ROOT / 'docs/amp/AMP_PLATFORM_CONTRACT.yaml'


class ContractFailures(unittest.TestCase):
    def run_contract(self, mutation):
        data = json.loads(CONTRACT.read_text())
        mutation(data)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'contract.yaml'
            path.write_text(json.dumps(data))
            return subprocess.run(['python3', str(CHECKER), '--contract', str(path),
                                   '--linux-dts', str(DTS)], text=True,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def test_baseline_is_blocked(self):
        result = self.run_contract(lambda data: None)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('FAIL Linux DTS link-id', result.stdout)
        self.assertIn('FAIL final RTOS load', result.stdout)

    def test_service_change_is_rejected(self):
        result = self.run_contract(lambda data: data['rpmsg'].update(service_name='wrong-service'))
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('FAIL RTOS NS and Linux ID service', result.stdout)

    def test_link_change_is_rejected(self):
        result = self.run_contract(lambda data: data['rpmsg'].update(link_id='0x03'))
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('FAIL M0 link-id', result.stdout)


if __name__ == '__main__':
    unittest.main()
