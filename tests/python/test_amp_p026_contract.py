"""P026 metadata/header must move together when the sole contract changes."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/amp"))
from p026_generate_preload_contract import files


class PreloadContract(unittest.TestCase):
    def contract(self):
        return json.loads((ROOT / "docs/amp/AMP_PLATFORM_CONTRACT.yaml").read_text())

    def test_generated_metadata_includes_full_code_not_bin_length(self):
        c = self.contract()
        generated = files(json.dumps(c).encode())
        size = int(c["host_proposal"]["code_size"], 0)
        self.assertIn(f"rockchip,mcu-code-size = <0x{size:x}>", generated["amp.its"])
        self.assertIn(f"#define AMP_CODE_SIZE 0x{size:08x}", generated["amp_project_contract.h"])

    def test_shared_base_changes_all_generated_views(self):
        c = self.contract()
        before = files(json.dumps(c).encode())
        c["host_proposal"]["shared_window_linux_pa"] = "0x50000000"
        after = files(json.dumps(c).encode())
        for name in ("amp.its", "amp_project_contract.h", "layout.json", "rk3576-lubancat-3-v2-m0-host.dtso"):
            self.assertNotEqual(before[name], after[name], name)
        self.assertIn("0x57d00000", after["amp.its"])
        self.assertIn("#define AMP_SHARED_LINUX_PA 0x57d00000", after["amp_project_contract.h"])

    def test_bad_geometry_rejected_before_generating_loader_inputs(self):
        c = self.contract()
        c["host_proposal"]["code_linux_pa"] = "0x47d00000"
        with self.assertRaises(AssertionError):
            files(json.dumps(c).encode())


if __name__ == "__main__":
    unittest.main()
