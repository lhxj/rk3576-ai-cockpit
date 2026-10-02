import importlib.util
import json
from pathlib import Path
import unittest
import copy

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('amp_generator',ROOT/'scripts/amp/generate_host_proposal.py')
gen=importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)


class HostProposal(unittest.TestCase):
    def setUp(self):
        self.contract=json.loads((ROOT/'docs/amp/AMP_PLATFORM_CONTRACT.yaml').read_text())

    def test_existing_four_megabyte_shared_reservation_is_not_reused(self):
        x=gen.layout(self.contract)
        self.assertEqual(x['shared_size'],0x20000)
        self.assertEqual(x['shared_pa'],0x47d00000)
        self.assertEqual(x['pool_pa'],0x47d10000)
        self.assertEqual(x['pool_size'],0x10000)
        self.assertLess(x['code_pa']+x['code_size'],x['shared_pa'])

    def test_conflicting_code_region_rejected(self):
        c=copy.deepcopy(self.contract)
        c['host_proposal']['code_linux_pa']='0x47d00000'
        with self.assertRaisesRegex(AssertionError,'overlap'):
            gen.layout(c)

    def test_shared_mapping_and_vring_inconsistency_rejected(self):
        c=copy.deepcopy(self.contract)
        c['rpmsg']['vring1']['m0_address']='0x27d04000'
        with self.assertRaises(AssertionError): gen.layout(c)
        c=copy.deepcopy(self.contract)
        c['host_proposal']['shared_window_linux_pa']='0x40000001'
        with self.assertRaisesRegex(AssertionError,'aligned'): gen.layout(c)

    def test_host_proposal_does_not_open_release_gate(self):
        gen.generate(self.contract,0x141)
        self.assertEqual(self.contract['status'],'BLOCKED_PREBOARD')
        self.assertIsNone(self.contract['address_translation']['con17_final_value'])
        self.assertIsNone(self.contract['boot']['load_source'])
        self.assertFalse(self.contract['linux']['echo_module_board_ready'])

    def test_linux_name_matches_contract(self):
        driver=(ROOT/'scripts/board/amp_echo_linux/rk3576_amp_echo_test.c').read_text()
        self.assertIn('"'+self.contract['rpmsg']['service_name']+'"',driver)
        self.assertIn('.name = AMP_ECHO_SERVICE',driver)

    def test_non_thumb_entry_rejected(self):
        with self.assertRaisesRegex(AssertionError,'Thumb'): gen.generate(self.contract,0x140)

    def test_generated_snapshot_and_patch_match_contract(self):
        expected=gen.generate(self.contract,gen.number(self.contract['host_proposal']['m0_entry']))
        for name,text in expected.items():
            self.assertEqual((ROOT/'docs/amp/host-proposal'/name).read_text(),text,name)
        patch=(ROOT/'patches/rk3576-amp-platform/0004-rtos-cold-uncached-echo-host-proposal.patch').read_text()
        for line in expected['amp_contract.h'].splitlines():
            self.assertIn('+'+line+'\n',patch)

    def test_mailbox_contract_drift_rejected(self):
        c=copy.deepcopy(self.contract)
        c['mailbox']['linux_tx']['controller']=3
        with self.assertRaisesRegex(AssertionError,'mailbox'): gen.layout(c)


if __name__=='__main__': unittest.main()
