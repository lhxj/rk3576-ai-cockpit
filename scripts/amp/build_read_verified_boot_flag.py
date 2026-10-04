#!/usr/bin/env python3
"""Check the fixed vendor read ABI and Host-build one reviewed diagnostic."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_vendor_abi(vendor, client):
    start = vendor.index('static uint32_t trusty_base_efuse_or_otp_operation(')
    end = vendor.index('\nuint32_t trusty_read_attribute_hash(', start)
    backend = vendor[start:end]
    match = re.search(r'TEEC_UUID tempuuid\s*=\s*\{(.*?)\};', backend, re.S)
    if not match:
        raise ValueError('vendor TA UUID missing')
    values = [int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', match.group(1))]
    if len(values) != 11:
        raise ValueError('vendor TA UUID has unexpected layout')
    expected = struct.pack('>IHH', *values[:3]) + bytes(values[3:])
    match = re.search(r'flag_ta\[16\]\s*=\s*\{(.*?)\};', client, re.S)
    actual = bytes(int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', match.group(1)))
    if expected != actual:
        raise ValueError('client TA UUID differs from the vendor read path')
    match = re.search(r'#define\s+STORAGE_CMD_READ_ENABLE_FLAG\s+(\d+)', vendor)
    command = int(match.group(1))
    if not re.search(r'READ_ENABLE_FLAG\s*=\s*' + str(command) + r'\b', client):
        raise ValueError('client uses the wrong TA command')
    start = vendor.index('uint32_t trusty_read_vbootkey_enable_flag(')
    end = vendor.index('\nuint32_t trusty_write_ta_encryption_key(', start)
    read_func = vendor[start:end]
    if not re.search(r'STORAGE_CMD_READ_ENABLE_FLAG,\s*false,\s*&bootflag,\s*1', read_func):
        raise ValueError('vendor operation is not the expected one-word read')
    if 'bootflag == 0x000000FF' not in read_func:
        raise ValueError('vendor required-flag interpretation changed')
    forbidden = ('TEE_IOC_SUPPL_', 'TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT',
                 'TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT', 'STORAGE_CMD_WRITE', '/dev/mem')
    if any(token in client for token in forbidden):
        raise ValueError('diagnostic contains an unexpected input/write interface')
    return {'uuid': expected.hex(), 'command': command, 'output_bytes': 4,
            'vendor_abi_match': 'PASS', 'invoked_on_board': False}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--uboot-source', type=Path, required=True)
    ap.add_argument('--kernel-source', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    source = ROOT / 'tools/amp/read_verified_boot_flag.c'
    vendor_path = args.uboot_source / 'lib/optee_clientApi/OpteeClientInterface.c'
    header = args.kernel_source / 'include/uapi/linux/tee.h'
    abi = check_vendor_abi(vendor_path.read_text(), source.read_text())
    args.output.mkdir(exist_ok=False, parents=True)
    binary = args.output / 'read_verified_boot_flag'
    command = ['aarch64-linux-gnu-gcc', '-std=c11', '-D_GNU_SOURCE', '-Wall', '-Wextra',
               '-Werror', '-O2', '-static', '-include', str(header), str(source), '-o', str(binary)]
    result = subprocess.run(command, text=True, capture_output=True)
    (args.output / 'build.log').write_text(result.stdout + result.stderr)
    result.check_returncode()
    record = {'source': str(source.relative_to(ROOT)), 'source_sha256': digest(source),
              'vendor_read_source_sha256': digest(vendor_path),
              'kernel_tee_uapi_sha256': digest(header),
              'compiler': subprocess.check_output(['aarch64-linux-gnu-gcc', '--version'], text=True),
              'command': command, 'exit_code': result.returncode,
              'binary': str(binary.relative_to(ROOT)), 'binary_sha256': digest(binary),
              'abi': abi, 'board_execution_authorized': False,
              'scope': 'Host build only; no OTP write command, SMC/MMIO retry, or M0 control'}
    (args.output / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps(record, indent=2))


if __name__ == '__main__':
    main()
