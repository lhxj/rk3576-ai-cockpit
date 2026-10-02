import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('packet_checker',
    Path(__file__).resolve().parents[2] / 'scripts/amp/check_deployment_packet.py')
CHECKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKER)


class DeploymentPacketGateTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.contract = {
            'status': 'BLOCKED_PREBOARD',
            'host_proposal': {'code_linux_pa': '0x47800000', 'code_size': '0x80000',
                'shared_window_linux_pa': '0x40000000', 'shared_m0_address': '0x27d00000',
                'm0_entry': '0x141', 'buffer_header_size': 16},
            'rpmsg': {'vring0': {'size': '0x8000'}, 'buffer_count': 64, 'buffer_size': 496},
            'rtos': {'load_address': None, 'entry_address': None},
            'shared_memory': {'linux_pa': None}, 'coherency': {'selected_scheme': None},
            'boot': {'load_source': None, 'fit_verification_policy_on_board': None,
                     'uboot_amp_enabled_on_board': False, 'bl31_mcu_smc_verified_on_board': False}}
        (self.root / 'artifact.bin').write_bytes(b'payload')
        self.manifest = {'contract': {}, 'artifacts': [{'file': 'artifact.bin',
            'sha256': hashlib.sha256(b'payload').hexdigest()}], 'evidence': [],
            'baseline': {'evidence_files': []}}
        self.packet = {'contract': {}, 'proposal': copy.deepcopy(self.contract['host_proposal']),
            'changes': [], 'status': 'BLOCKED', 'gates': {'current_boot_policy': 'BLOCKED'},
            'deployment_authorized': False}
        self.write_contract()

    def write_contract(self):
        data = json.dumps(self.contract).encode()
        (self.root / 'contract.json').write_bytes(data)
        item = {'file': 'contract.json', 'sha256': hashlib.sha256(data).hexdigest()}
        self.manifest['contract'] = item
        self.packet['contract'] = item

    def check(self):
        return CHECKER.check_packet(self.manifest, self.packet, self.root)

    def test_valid_host_files_do_not_grant_board_permission(self):
        result = self.check()
        self.assertEqual(result['host_packet_integrity'], 'PASS')
        self.assertEqual(result['deployment_gate'], 'BLOCKED')
        self.assertEqual(result['proposal_only']['vring0_pa'], '0x47d00000')

    def test_changed_artifact_is_rejected(self):
        (self.root / 'artifact.bin').write_bytes(b'other firmware')
        self.assertEqual(self.check()['host_packet_integrity'], 'FAIL')

    def test_false_capabilities_are_not_evidence_even_with_ready_labels(self):
        self.contract['status'] = 'READY_FOR_CONTROLLED_BOARD_TEST'
        self.packet['status'] = 'READY_FOR_CONTROLLED_BOARD_TEST'
        self.packet['deployment_authorized'] = True
        self.packet['gates'] = {'current_boot_policy': 'PASS'}
        self.write_contract()
        result = self.check()
        self.assertEqual(result['deployment_gate'], 'BLOCKED')
        self.assertIn('board_capability_unproved_uboot_amp_enabled_on_board', result['blockers'])

    def test_mismatched_contract_cannot_reuse_old_changeset(self):
        self.contract['host_proposal']['shared_window_linux_pa'] = '0x41000000'
        self.write_contract()
        self.assertEqual(self.check()['host_packet_integrity'], 'FAIL')


if __name__ == '__main__':
    unittest.main()
