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
            # Explicit static parser fixtures: these negative tests must not
            # depend on a vendor SDK at ROOT.parent/rtos or execute firmware.
            # Fixture values stay fixed when the contract is mutated.
            rtos = Path(tmp) / 'rtos'
            bsp = 'bsp/rockchip/rk3576-mcu/'
            platform = 'bsp/rockchip/common/hal/middleware/rpmsg-lite/lib/include/platform/RK3576/'
            files = {
                bsp + 'Image/amp.its': 'load = <0x47800000>;',
                bsp + 'gcc_link.ld.S': 'DDR (rxw) : ORIGIN = 0x00000000\n#else\nLINUX_RPMSG (rxw) : ORIGIN = 0x27d00000',
                bsp + 'applications/amp_echo.c': '#define AMP_ECHO_LINK_ID RL_PLATFORM_SET_LINK_ID(0U, 4U)\n#define AMP_ECHO_SERVICE "rk3576-m0-echo"\nrpmsg_lite_remote_init rpmsg_ns_announce',
                platform + 'rpmsg_platform.h': '#define VRING_ALIGN 0x1000\n#define VRING_SIZE 0x8000',
                platform + 'rpmsg_config.h': '#define RL_BUFFER_COUNT 64\n#define RL_BUFFER_PAYLOAD_SIZE 496',
                'bsp/rockchip/common/drivers/rpmsg-lite/lib/rpmsg_lite/porting/platform/RK3576/rpmsg_platform.c': '"mbox-clr4"',
            }
            for name, body in files.items():
                fixture = rtos / name
                fixture.parent.mkdir(parents=True, exist_ok=True)
                fixture.write_text(body)
            return subprocess.run(['python3', str(CHECKER), '--contract', str(path),
                                   '--rtos', str(rtos),
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
