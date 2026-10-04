"""Prevent confusing local Thumb entry with a Linux physical load address."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    'amp_contract_entry', ROOT / 'scripts/amp/check_platform_contract.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class EntrySemantics(unittest.TestCase):
    def checks(self, entry=0x141, fit_entry=0x141):
        return checker.m0_entry_checks(entry, 0x47800000, 0, 0x80000, fit_entry)

    def test_local_thumb_entry_with_distinct_physical_load(self):
        self.assertTrue(all(self.checks().values()))

    def test_physical_load_is_not_local_pc(self):
        result = self.checks(0x47800000, 0x47800000)
        self.assertFalse(result['M0 entry is a bounded Thumb local address'])
        self.assertFalse(result['M0 entry is distinct from physical code load'])

    def test_even_or_out_of_window_pc_rejected(self):
        for value in (0, 0x140, 0x80001):
            with self.subTest(value=value):
                self.assertFalse(self.checks(value, value)[
                    'M0 entry is a bounded Thumb local address'])

    def test_fit_metadata_must_match_pc(self):
        for value in (None, 0x47800000, 0x143):
            with self.subTest(value=value):
                self.assertFalse(self.checks(fit_entry=value)[
                    'FIT entry equals M0 local entry'])

    def test_negative_or_unknown_board_capability_stays_blocked(self):
        for value in (None, False, 1, 'true', 'UNVERIFIED'):
            with self.subTest(value=value):
                result = checker.board_capability_checks({
                    'uboot_amp_enabled_on_board': value,
                    'bl31_mcu_smc_verified_on_board': value})
                self.assertFalse(any(result.values()))

    def test_explicit_positive_board_capability(self):
        self.assertTrue(all(checker.board_capability_checks({
            'uboot_amp_enabled_on_board': True,
            'bl31_mcu_smc_verified_on_board': True}).values()))


if __name__ == '__main__':
    unittest.main()
