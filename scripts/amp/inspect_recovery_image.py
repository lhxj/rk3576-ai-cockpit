#!/usr/bin/env python3
"""Bounded Host-only RKFW/RKAF inventory; never mount or execute payloads.

RKFW: rockchip-linux/rkdeveloptool@304f073752fd25c854e1bcf05d8e7f925b1f4e14.
RKAF fixed table: neo-technologies/rockchip-mkbootimg@2348690523faee6ce3cea9eb9ff47e8b8d5e1df6.
The latter is an independent implementation, not a vendor format guarantee.
Extended RKAF sizes are deliberately NOT decoded: selected extended items fail.
Whole-file checksums and RKFW's documented HI extension cover the full container.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

RKFW = struct.Struct("<4sHIIH5BI4I61s")
RKAF_ITEM = struct.Struct("<32s60sIIIII")
DEFAULT_ITEMS = ("package-file", "parameter", "bootloader", "uboot", "boot")


def require(ok, message):
    if not ok:
        raise ValueError(message)


def read_at(stream, offset, size, limit):
    require(offset >= 0 and size >= 0 and offset + size <= limit,
            "Read outside input boundary")
    stream.seek(offset)
    data = stream.read(size)
    require(len(data) == size, "Truncated input")
    return data


def ctext(raw):
    return raw.split(b"\0", 1)[0].decode("ascii", errors="strict")


def hash_range(stream, offset, length, target=None):
    sha = hashlib.sha256()
    stream.seek(offset)
    remaining = length
    while remaining:
        data = stream.read(min(4 * 1024 * 1024, remaining))
        require(bool(data), "Truncated payload")
        sha.update(data)
        if target is not None:
            target.write(data)
        remaining -= len(data)
    return sha.hexdigest()


def inventory(stream, size):
    h = RKFW.unpack(read_at(stream, 0, RKFW.size, size))
    require(h[0] == b"RKFW" and h[1] == RKFW.size, "Unsupported RKFW header")
    boot_offset, boot_size, fw_offset, fw_low, reserved = h[-5:]
    high = struct.unpack_from("<I", reserved, 16)[0] if reserved[14:16] == b"HI" else 0
    fw_size = (high << 32) + fw_low
    end = fw_offset + fw_size
    require(boot_offset >= RKFW.size and boot_size > 0
            and boot_offset + boot_size <= fw_offset and fw_size >= 2052,
            "Overlapping/invalid RKFW regions")
    require(end <= size and (size - end == 32 or 160 <= size - end <= 288),
            "Invalid/unsupported RKFW trailer/bounds")
    trailer = read_at(stream, end, size - end, size)
    require(re.fullmatch(rb"[0-9a-fA-F]{32}", trailer[:32]), "Invalid MD5 trailer")
    ah = read_at(stream, fw_offset, 2048, end)
    require(ah[:4] == b"RKAF", "Missing RKAF")
    length_low = struct.unpack_from("<I", ah, 4)[0]
    require(length_low == (fw_size - 4) & 0xFFFFFFFF, "RKAF length differs from RKFW")
    count = struct.unpack_from("<I", ah, 136)[0]
    require(0 < count <= 16, "Invalid RKAF item count")
    items, ranges = [], []
    for i in range(count):
        name_raw, filename_raw, nand_size, pos, nand_addr, padded, item_size = RKAF_ITEM.unpack_from(ah, 140 + i * 112)
        name, filename = ctext(name_raw), ctext(filename_raw)
        require(re.fullmatch(r"[A-Za-z0-9_.-]+", name) is not None
                and name not in (".", ".."), "Unsafe/unsupported item name")
        require(name not in {x["name"] for x in items}, "Duplicate RKAF item name")
        # Nonzero bytes after a terminator may encode a vendor >4GiB extension.
        tail = filename_raw[len(filename.encode("ascii")) + 1:]
        extended = bool(any(tail))
        require(pos >= 2048 and item_size > 0 and pos + item_size <= fw_size - 4,
                "RKAF item outside firmware boundary")
        ranges.append((pos, pos + item_size))
        items.append({"name": name, "filename": filename, "offset": fw_offset + pos,
                      "rkaf_offset": pos, "size_low32": item_size,
                      "size": None if extended else item_size,
                      "extended_size_undecoded": extended, "filename_tail_hex": tail.hex(),
                      "nand_address_sectors": nand_addr, "nand_size_sectors": nand_size,
                      "padded_field_raw": padded})
    ranges.sort()
    require(all(a[1] <= b[0] for a, b in zip(ranges, ranges[1:])), "RKAF items overlap")
    require(all(not x["extended_size_undecoded"] or x["rkaf_offset"] == ranges[-1][0]
                for x in items), "Undecoded extended item must be last")
    return {"format": "RKFW/RKAF", "release": list(h[4:10]),
            "boot": {"offset": boot_offset, "size": boot_size},
            "firmware": {"offset": fw_offset, "size": fw_size, "size_high32": high},
            "rkfw_trailer": {"size": len(trailer), "content_md5": trailer[:32].decode("ascii").lower(),
                             "signature_bytes": max(0, len(trailer) - 32)},
            "rkaf_crc_verified": False, "items": items}


def extract(stream, info, out, selected, limit=256 * 1024 * 1024):
    require(not out.is_symlink(), "Output directory is a symlink")
    out.mkdir(parents=True, exist_ok=True)
    known = {item["name"] for item in info["items"]}
    require(set(selected) <= known, "Selected item not found")
    # Validate everything before creating any output. Never use embedded filenames.
    planned = []
    for item in info["items"]:
        if item["name"] not in selected:
            continue
        require(item["size"] is not None, "Extended RKAF item extraction unsupported")
        require(item["size"] <= limit, "Selected item exceeds extraction limit")
        dest = out / (item["name"] + ".payload")
        require(not dest.exists() and not dest.is_symlink(), "Refusing to overwrite payload")
        planned.append((item, dest))
    for item, dest in planned:
        with dest.open("xb") as target:
            item["sha256"] = hash_range(stream, item["offset"], item["size"], target)
        item["extracted_file"] = dest.name


def full_hash(stream, size, content_end):
    sha, md5, content_md5 = hashlib.sha256(), hashlib.md5(), hashlib.md5()
    stream.seek(0)
    pos = 0
    while pos < size:
        data = stream.read(min(4 * 1024 * 1024, size - pos))
        require(bool(data), "Truncated input while hashing")
        sha.update(data)
        md5.update(data)
        if pos < content_end:
            content_md5.update(data[:min(len(data), content_end - pos)])
        pos += len(data)
    return sha.hexdigest(), md5.hexdigest(), content_md5.hexdigest()


def loader_inventory(data):
    """Decode only the packed tables from Rockchip RKBoot.h; no RC4/code execution."""
    require(len(data) >= 106 and data[:4] in (b"LDR ", b"BOOT"), "Unsupported loader")
    require(struct.unpack_from("<H", data, 4)[0] == 102, "Unsupported loader header")
    records = []
    table_end = 102
    for idx in range(3):
        count, offset, stride = struct.unpack_from("<BIB", data, 25 + idx * 6)
        require(stride == 57 and count <= 32 and offset >= 102
                and offset + count * stride <= len(data) - 4, "Invalid loader table")
        table_end = max(table_end, offset + count * stride)
        for entry in range(count):
            entry_size, kind, name, payload, length, delay = struct.unpack_from("<BI40sIII", data, offset + entry * stride)
            require(entry_size == 57 and kind in (1, 2, 4) and length > 0
                    and payload + length <= len(data) - 4, "Invalid loader entry")
            text = name.decode("utf-16le").split("\0", 1)[0]
            records.append({"name": text, "type": kind, "offset": payload, "size": length,
                            "delay": delay, "sha256": hashlib.sha256(data[payload:payload + length]).hexdigest()})
    ranges = sorted((x["offset"], x["offset"] + x["size"]) for x in records)
    require(all(x[0] >= table_end for x in ranges)
            and all(a[1] <= b[0] for a, b in zip(ranges, ranges[1:])), "Loader payload/table overlap")
    return {"version_raw": hex(struct.unpack_from("<I", data, 6)[0]),
            "sign_flag_raw": data[43], "rc4_disable_flag_raw": data[44],
            "crc_verified": False, "entries": records}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("image", type=Path)
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--expected-md5", required=True)
    ap.add_argument("--expected-sha256", required=True)
    args = ap.parse_args()
    require(args.image.is_file(), "Input must be a regular Host file")
    report = args.output / "container-manifest.json"
    require(not report.exists() and not report.is_symlink(), "Refusing to overwrite manifest")
    size = args.image.stat().st_size
    with args.image.open("rb") as stream:
        info = inventory(stream, size)
        sha, md5, content = full_hash(stream, size,
                                    info["firmware"]["offset"] + info["firmware"]["size"])
        require(md5 == args.expected_md5.lower() and sha == args.expected_sha256.lower(),
                "Input identity mismatch; no payload extracted")
        require(content == info["rkfw_trailer"]["content_md5"], "RKFW internal MD5 mismatch")
        info.update({"file": args.image.name, "size": size, "sha256": sha, "md5": md5,
                     "rkfw_content_md5_verified": True})
        extract(stream, info, args.output, DEFAULT_ITEMS)
        stream.seek(info["boot"]["offset"])
        info["boot"]["sha256"] = hash_range(stream, info["boot"]["offset"], info["boot"]["size"])
        loader = next(x for x in info["items"] if x["name"] == "bootloader")
        require(loader["sha256"] == info["boot"]["sha256"], "Outer/inner loader mismatch")
        info["loader_inventory"] = loader_inventory((args.output / "bootloader.payload").read_bytes())
    with report.open("x") as dest:
        dest.write(json.dumps(info, indent=2) + "\n")
    print(json.dumps(info, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, struct.error) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
