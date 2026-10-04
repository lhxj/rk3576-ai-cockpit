#!/usr/bin/env python3
"""Sanitize already copied P026 evidence; no board access or firmware execution."""
import hashlib
import json
from pathlib import Path
import re
import struct

from validate_host_proposal import fdt

ROOT = Path(__file__).resolve().parents[2]
REVIEW = ROOT / 'docs/reviews/rk3576-amp-platform-closure'
RAW = ROOT / 'artifacts/local/p026-approved-read-20261002T185533Z-1008600'
TEE = ROOT / 'artifacts/local/p026-tee-run-20261002T190348Z-1019054'


def record(path):
    data = path.read_bytes()
    return {'file': str(path.relative_to(ROOT)), 'size': len(data),
            'sha256': hashlib.sha256(data).hexdigest()}


def main():
    raw = (RAW / 'uboot-original.raw').read_bytes()
    assert len(raw) == 0x800000
    nodes = fdt(raw)
    firmware = {'raw': record(RAW / 'uboot-original.raw'),
                'first4MiB_sha256': hashlib.sha256(raw[:0x400000]).hexdigest(),
                'tail4MiB_sha256': hashlib.sha256(raw[0x400000:]).hexdigest(),
                'tail_distinct_bytes': sorted(set(raw[0x400000:])), 'payloads': []}
    assert firmware['first4MiB_sha256'] == '06f48ed3716ccdde6adcb3187ca3eff766927e27bf6b7f432046539b782c4a56'
    for name in ('uboot', 'atf-1', 'atf-2', 'atf-3', 'optee', 'fdt'):
        value = nodes['/images/' + name]
        start = struct.unpack('>I', value['data-position'])[0]
        size = struct.unpack('>I', value['data-size'])[0]
        assert 0 <= start < start + size <= 0x200000
        data = raw[start:start + size]
        digest = hashlib.sha256(data).hexdigest()
        assert bytes.fromhex(digest) == nodes['/images/' + name + '/hash']['value']
        firmware['payloads'].append({'name': name, 'offset': start, 'size': size,
                                     'sha256': digest, 'FIT_hash_verified': True})
        if name == 'uboot':
            firmware['amp_strings_offsets'] = {s: data.find(s.encode()) for s in
                ('bus_mcu', 'Handle standalone:', 'Brought up amps', 'amp_m0load')}
    text = (TEE / 'tee-read.stdout').read_text()
    assert 'tee_read_exit_code=0' in (TEE / 'result.txt').read_text()
    assert not (TEE / 'tee-read.stderr').read_bytes() and not (TEE / 'cleanup.stderr').read_bytes()
    assert 'open_session_ret=0x00000000 origin=4' in text
    assert 'invoke_ret=0x00000000 origin=4 output_size=4' in text
    flag = int(re.search(r'^read_enable_flag_raw=(0x[0-9a-f]{8})$', text, re.M)[1], 16)
    required = int(re.search(r'^vendor_rk3576_required_flag=([01])$', text, re.M)[1])
    assert required == (flag == 0xff)
    pins = (RAW / 'pinmux-owners.txt').read_text()
    for number in (124, 125):
        assert re.search(rf'^pin {number} \(gpio3-{number - 96}\): \(MUX UNCLAIMED\) \(GPIO UNCLAIMED\)', pins, re.M)
    result = {
        'milestone': 'P026', 'date': '2026-10-03', 'evidence_level': 'BOARD_OBSERVED_READONLY',
        'authorization': {'source': 'Direct user reply', 'expires': '2026-10-03T12:00:00+08:00',
                          'scope': 'preboard diagnostic reads; no M0 start/boot or GPT writes'},
        'verified_boot_read': {'execution': 'Linux normal world via /dev/tee0 OP-TEE ioctl',
            'TA_UUID': '2d26d8a8-5134-4dd8-b32f-b34bceebc471', 'command': 5,
            'direction': 'MEMREF_OUTPUT', 'bytes': 4, 'open_result': 0, 'invoke_result': 0,
            'raw_word': hex(flag), 'vendor_required_flag': required,
            'binary_sha256': '2b8929f02dbc86ff4f6f34eaecc0d878b9de1706956f8a8a33b84b8ca2e6beca',
            'temporary_location': '/run (existing executable tmpfs)', 'cleanup': 'PASS',
            'earlier_attempt': '/dev/shm noexec: process never started; not a TA refusal',
            'proper_uboot_expected_interpretation': 'SOURCE_INFERRED: same vendor TA/read ABI -> required=0',
            'proper_uboot_actual_call_observed': False,
            'scope': 'No OTP writes; not proof of global OTP/security policy or MCU mapping'},
        'original_firmware': firmware,
        'uart5_ownership': {'GPIO3_D4_pin124': 'MUX/GPIO UNCLAIMED',
                           'GPIO3_D5_pin125': 'MUX/GPIO UNCLAIMED',
                           'scope': 'Linux runtime owner snapshot; no pinmux changed',
                           'user_confirmation': 'TTL supports 3.3V; 40Pin16/18 have no peripherals'},
        'remaining_runtime_unknowns': ['CON16/CON17 actual/effective values', 'cold MCU reset/setter success',
                                      'MCU cache bypass actual state', 'new U-Boot actual execution'],
        'evidence_files': [record(p) for p in (TEE / 'tee-read.stdout', TEE / 'result.txt',
                            TEE / 'mounts.txt', TEE / 'cleanup.stderr', RAW / 'pinmux-owners.txt')]
    }
    (REVIEW / 'P026_BOARD_READ_EVIDENCE.json').write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps({'flag': hex(flag), 'vendor_required_flag': required,
                      'raw_uboot_sha256': firmware['raw']['sha256'],
                      'firmware_payloads_verified': 6, 'UART5_runtime': 'UNCLAIMED',
                      'board_writes': 'temporary RAM diagnostic only; cleaned'}, ensure_ascii=False))


if __name__ == '__main__':
    main()
