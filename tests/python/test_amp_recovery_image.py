"""Malformed containers must fail before any extraction; no vendor binaries."""
import hashlib
import importlib.util
import io
from pathlib import Path
import struct
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / "scripts/amp/inspect_recovery_image.py"
spec = importlib.util.spec_from_file_location("recovery", SCRIPT)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def fixture(name=b"boot", filename=b"boot.img", offset=2048, extra=False):
    af = bytearray(2064)
    af[:4] = b"RKAF"
    struct.pack_into("<I", af, 4, len(af))
    struct.pack_into("<I", af, 136, 1)
    if extra:
        filename = filename.ljust(54, b"\0") + b"H\x01\0\0\0\0"
    af[140:252] = mod.RKAF_ITEM.pack(name, filename, 0, offset, 0, 1, 16)
    fwsize = len(af) + 4
    hdr = mod.RKFW.pack(b"RKFW", 102, 0, 0, 2026, 4, 24, 1, 2, 3,
                        0, 102, 8, 110, fwsize, b"\0" * 61)
    body = hdr + b"LDR test" + bytes(af) + b"CRC!"
    return body + hashlib.md5(body).hexdigest().encode("ascii")


class RecoveryImageTests(unittest.TestCase):
    def parse(self, data):
        return mod.inventory(io.BytesIO(data), len(data))

    def test_internal_and_whole_hash_and_exact_extraction(self):
        data = fixture()
        info = self.parse(data)
        sha, md5, inner = mod.full_hash(io.BytesIO(data), len(data), len(data) - 32)
        self.assertEqual(sha, hashlib.sha256(data).hexdigest())
        self.assertEqual(md5, hashlib.md5(data).hexdigest())
        self.assertEqual(inner, info["rkfw_trailer"]["content_md5"])
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "out"
            mod.extract(io.BytesIO(data), info, out, ["boot"])
            self.assertEqual((out / "boot.payload").read_bytes(), data[2158:2174])
            with self.assertRaisesRegex(ValueError, "overwrite"):
                mod.extract(io.BytesIO(data), info, out, ["boot"])

    def test_unsupported_extended_item_not_truncated(self):
        data = fixture(extra=True)
        info = self.parse(data)
        self.assertIsNone(info["items"][0]["size"])
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, "Extended"):
                mod.extract(io.BytesIO(data), info, Path(tmp) / "out", ["boot"])
            self.assertFalse(any((Path(tmp) / "out").iterdir()))

    def test_truncation_and_out_of_range_payload(self):
        with self.assertRaises(ValueError):
            self.parse(fixture()[:-8])
        with self.assertRaisesRegex(ValueError, "boundary"):
            self.parse(fixture(offset=999999))

    def test_path_traversal_name_and_duplicate_items_rejected(self):
        with self.assertRaisesRegex(ValueError, "Unsafe"):
            self.parse(fixture(name=b"../boot"))
        data = bytearray(fixture())
        struct.pack_into("<I", data, 110 + 136, 2)
        data[110 + 252:110 + 364] = data[110 + 140:110 + 252]
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            self.parse(data)

    def test_overlap_and_header_length_rejected(self):
        data = bytearray(fixture())
        struct.pack_into("<I", data, 110 + 136, 2)
        data[110 + 252:110 + 364] = mod.RKAF_ITEM.pack(b"other", b"other.img", 0, 2048, 0, 1, 8)
        with self.assertRaisesRegex(ValueError, "overlap"):
            self.parse(data)
        data = bytearray(fixture())
        struct.pack_into("<I", data, 114, 1)
        with self.assertRaisesRegex(ValueError, "length"):
            self.parse(data)

    def test_output_symlink_and_limit_rejected(self):
        data = fixture()
        info = self.parse(data)
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "link"
            out.symlink_to(Path(tmp), target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "symlink"):
                mod.extract(io.BytesIO(data), info, out, ["boot"])
            with self.assertRaisesRegex(ValueError, "limit"):
                mod.extract(io.BytesIO(data), info, Path(tmp) / "out", ["boot"], limit=1)

    def test_high_rkfw_size_is_not_wrapped_to_32bits(self):
        data = bytearray(fixture())
        data[41 + 14:41 + 16] = b"HI"
        struct.pack_into("<I", data, 41 + 16, 1)
        with self.assertRaisesRegex(ValueError, "bounds"):
            self.parse(data)

    def test_loader_entry_bounds(self):
        data = bytearray(170)
        data[:4] = b"LDR "
        struct.pack_into("<H", data, 4, 102)
        for i in range(3):
            struct.pack_into("<BIB", data, 25 + i * 6, 1 if i == 0 else 0, 102, 57)
        struct.pack_into("<BI40sIII", data, 102, 57, 1, "DDR".encode("utf-16le"), 159, 7, 1)
        self.assertEqual(mod.loader_inventory(data)["entries"][0]["size"], 7)
        struct.pack_into("<I", data, 102 + 49, 99999)
        with self.assertRaisesRegex(ValueError, "entry"):
            mod.loader_inventory(data)


if __name__ == "__main__":
    unittest.main()
