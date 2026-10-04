import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('flag_build', ROOT / 'scripts/amp/build_read_verified_boot_flag.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)

# Synthetic source fixtures; test ABI rejection, not a successful board read.
VENDOR = '''
#define STORAGE_CMD_READ_ENABLE_FLAG 5
static uint32_t trusty_base_efuse_or_otp_operation(void) {
 TEEC_UUID tempuuid = {0x2d26d8a8, 0x5134, 0x4dd8,
 {0xb3, 0x2f, 0xb3, 0x4b, 0xce, 0xeb, 0xc4, 0x71}};
}
uint32_t trusty_read_attribute_hash(void) {}
uint32_t trusty_read_vbootkey_enable_flag(void) {
 trusty_base_efuse_or_otp_operation(STORAGE_CMD_READ_ENABLE_FLAG, false, &bootflag, 1);
 if (bootflag == 0x000000FF) {}
}
uint32_t trusty_write_ta_encryption_key(void) {}
'''


class ReadFlagABI(unittest.TestCase):
    def setUp(self):
        self.client = (ROOT / 'tools/amp/read_verified_boot_flag.c').read_text()

    def test_fixed_read_abi(self):
        result = MODULE.check_vendor_abi(VENDOR, self.client)
        self.assertEqual(result['uuid'], '2d26d8a851344dd8b32fb34bceebc471')
        self.assertFalse(result['invoked_on_board'])

    def test_wrong_uuid_rejected(self):
        with self.assertRaises(ValueError):
            MODULE.check_vendor_abi(VENDOR, self.client.replace('0xb3, 0x4b', '0xbd, 0xc4'))

    def test_wrong_command_rejected(self):
        with self.assertRaises(ValueError):
            MODULE.check_vendor_abi(VENDOR, self.client.replace('READ_ENABLE_FLAG = 5', 'READ_ENABLE_FLAG = 6'))

    def test_vendor_write_rejected(self):
        with self.assertRaises(ValueError):
            MODULE.check_vendor_abi(VENDOR.replace('false, &bootflag', 'true, &bootflag'), self.client)


if __name__ == '__main__':
    unittest.main()
