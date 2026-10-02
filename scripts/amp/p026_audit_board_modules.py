#!/usr/bin/env python3
"""Inspect copied modules without extracting or executing them; compare symbol ABI."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import tarfile


def sha(data):
    return hashlib.sha256(data).hexdigest()


def sections(data):
    if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 183:
        raise ValueError('Expected ELF64 little-endian AArch64 module')
    offset = struct.unpack_from('<Q', data, 40)[0]
    entry, count, names = struct.unpack_from('<HHH', data, 58)
    if entry != 64 or names >= count or offset + entry * count > len(data):
        raise ValueError('Invalid ELF section table')
    headers = [struct.unpack_from('<IIQQQQIIQQ', data, offset + entry * n) for n in range(count)]
    h = headers[names]
    strings = data[h[4]:h[4] + h[5]]
    result = {}
    for h in headers:
        if h[0] >= len(strings):
            raise ValueError('Invalid section name')
        end = strings.find(b'\0', h[0])
        name = strings[h[0]:end].decode('ascii')
        if h[1] != 8 and h[4] + h[5] > len(data):
            raise ValueError('Section outside file')
        result[name] = data[h[4]:h[4] + h[5]] if h[1] != 8 else b''
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--symvers', type=Path, required=True)
    parser.add_argument('--board-symvers', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    symbols = {}
    for line in args.symvers.read_text().splitlines():
        cells = line.split()
        if len(cells) >= 2:
            symbols[cells[1]] = int(cells[0], 16)
    records, failures = [], []
    with tarfile.open(args.archive) as archive:
        for member in archive:
            if not member.isfile() or not member.name.endswith('.ko'):
                continue
            data = archive.extractfile(member).read()
            parsed = sections(data)
            info = dict(v.decode('utf-8').split('=', 1) for v in parsed['.modinfo'].split(b'\0') if b'=' in v)
            version = parsed.get('__versions', b'')
            if len(version) % 64:
                raise ValueError('Invalid arm64 modversion_info')
            problems = []
            for start in range(0, len(version), 64):
                crc = struct.unpack_from('<Q', version, start)[0]
                symbol = version[start + 8:start + 64].split(b'\0', 1)[0].decode('ascii')
                if symbols.get(symbol) != crc:
                    problems.append({'symbol': symbol, 'module_crc': hex(crc),
                                     'kernel_crc': hex(symbols[symbol]) if symbol in symbols else None})
            if not info.get('vermagic', '').startswith('6.1.99-rk3576 '):
                problems.append({'reason': 'Unexpected vermagic'})
            record = {'file': member.name, 'sha256': sha(data), 'vermagic': info.get('vermagic'),
                      'versioned_imports': len(version) // 64, 'mismatches': problems}
            records.append(record)
            if problems:
                failures.append(record)
    same_symvers = args.symvers.read_bytes() == args.board_symvers.read_bytes()
    report = {'evidence': 'HOST_TESTED on BOARD_OBSERVED_READONLY module copies',
              'archive_sha256': sha(args.archive.read_bytes()), 'modules': len(records),
              'imports': sum(r['versioned_imports'] for r in records),
              'all_vermagic_values': sorted(set(r['vermagic'] for r in records)),
              'symvers_equal_to_board_headers': same_symvers,
              'candidate_symvers_sha256': sha(args.symvers.read_bytes()),
              'status': ('SYMBOL_ABI_PASS' if records and same_symvers and not failures and
                         all(r['versioned_imports'] for r in records) and any(symbols.values()) else
                         'VERMAGIC_ONLY_NOT_ABI' if records and not failures else 'BLOCKED'),
              'failed_modules': failures, 'records': records,
              'scope': 'Symbol CRC/vermagic only; no module insertion or runtime compatibility proof'}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('records', 'failed_modules')}, indent=2))
    return 0 if report['status'] == 'SYMBOL_ABI_PASS' else 2


if __name__ == '__main__':
    raise SystemExit(main())
