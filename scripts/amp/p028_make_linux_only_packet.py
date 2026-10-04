#!/usr/bin/env python3
"""Host-only P028 candidate identity; never opens a board or grants deployment."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

from validate_host_proposal import fdt

ROOT = Path(__file__).resolve().parents[2]
SOURCE_SHA = 'f8b4554584dd475ce783c605850c5e883b0a0fd4'
ORIGINAL_SHA = 'ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a'
SLOT = 2 * 1024 * 1024
NAMES = ('uboot', 'atf-1', 'atf-2', 'atf-3', 'optee', 'fdt')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def local_file(path):
    path = path.resolve()
    if not path.is_relative_to(ROOT) or not path.is_file():
        raise ValueError('expected regular Host file inside project: ' + str(path))
    return path


def payload(data, nodes, name):
    node = nodes['/images/' + name]
    offset = struct.unpack('>I', node['data-position'])[0]
    size = struct.unpack('>I', node['data-size'])[0]
    if not (0 < offset < offset + size <= SLOT):
        raise ValueError('payload outside first FIT slot: ' + name)
    value = data[offset:offset + size]
    if sha(value) != nodes['/images/' + name + '/hash']['value'].hex():
        raise ValueError('FIT payload SHA mismatch: ' + name)
    return value


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, required=True)
    ap.add_argument('--build', type=Path, required=True)
    ap.add_argument('--package', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    source = args.source.resolve()
    if not source.is_relative_to(ROOT):
        raise ValueError('source must stay inside project')
    source_commit = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if source_commit != SOURCE_SHA or subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True).strip():
        raise ValueError('unreviewed or dirty derived source')
    build = args.build.resolve()
    package = args.package.resolve()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT) or output.exists():
        raise ValueError('output must be a new Host directory inside project')
    config = local_file(build / '.config').read_text()
    for line in ('CONFIG_AMP=y', 'CONFIG_ROCKCHIP_AMP=y', 'CONFIG_FIT_SIGNATURE=y',
                 'CONFIG_AMP_PROJECT_FACTORY_BOOT=y', 'CONFIG_CONSOLE_DISABLE_CLI=y',
                 'CONFIG_OPTEE_CLIENT=y', 'CONFIG_BOOTDELAY=3'):
        if line not in config.splitlines():
            raise ValueError('required config absent: ' + line)
    if 'CONFIG_IMAGE_FORMAT_LEGACY=y' in config.splitlines():
        raise ValueError('global legacy executable formats must stay disabled')
    recovery = json.loads(local_file(ROOT / 'docs/reviews/rk3576-amp-platform-closure/P028_RECOVERY_READBACK.json').read_text())
    if recovery['readback_result'] != 'SHA256_MATCH_ORIGINAL_8MIB' or not recovery['linux_cold_boot'].startswith('PASS'):
        raise ValueError('original recovery has not passed')
    original_path = local_file(ROOT / recovery['host_snapshot'])
    original = original_path.read_bytes()
    if len(original) != 4 * SLOT or sha(original) != ORIGINAL_SHA:
        raise ValueError('original 8MiB board backup identity mismatch')
    new_path = local_file(package / 'uboot-host-unsigned.img')
    new = new_path.read_bytes()
    package_result = json.loads(local_file(package / 'result.json').read_text())
    if len(new) != 2 * SLOT or sha(new) != package_result['package_sha256'] or new[:SLOT] != new[SLOT:]:
        raise ValueError('new two-slot package identity mismatch')
    old_nodes, new_nodes = fdt(original), fdt(new)
    preserved = []
    for name in NAMES:
        old, candidate = payload(original, old_nodes, name), payload(new, new_nodes, name)
        if name != 'uboot' and old != candidate:
            raise ValueError('original board firmware/control DT changed: ' + name)
        preserved.append({'name': name, 'sha256': sha(candidate), 'unchanged_from_board_backup': old == candidate})
    candidate = payload(new, new_nodes, 'uboot')
    if candidate != local_file(build / 'u-boot-nodtb.bin').read_bytes():
        raise ValueError('FIT does not contain this clean build')
    # Preserve the complete remaining 4MiB from the proven board backup.
    # This is a regular Host file, not a device write or partition operation.
    full_extent = new + original[len(new):]
    output.mkdir(parents=True)
    image = output / 'uboot-P028-linux-only-8MiB-PENDING-APPROVAL.img'
    image.write_bytes(full_extent)
    records = []
    for path, purpose in (
        (build / 'u-boot', 'Clean AArch64 ELF; reviewed source, not board-tested'),
        (build / 'u-boot.bin', 'Clean build plus FDT; not a flash container'),
        (build / 'u-boot.dtb', 'Unchanged original control DT'),
        (build / '.config', 'Exact candidate build configuration'),
        (new_path, 'Two-slot 4MiB Host FIT package, not a complete partition'),
        (image, '8MiB Linux-only test proposal; original trailing 4MiB retained'),
    ):
        path = local_file(path)
        records.append({'file': path.relative_to(ROOT).as_posix(), 'size': path.stat().st_size,
                        'sha256': sha(path.read_bytes()), 'purpose': purpose,
                        'source_commit': source_commit, 'deployment_authorized': False,
                        'architecture': 'FDT' if path.name.endswith('.dtb') else
                                        ('Kconfig text' if path.name == '.config' else 'AArch64 / RK3576 firmware FIT' if path.suffix == '.img' else 'AArch64'),
                        'build_command': 'python3 scripts/amp/p028_make_linux_only_packet.py --source SOURCE --build BUILD --package PACKAGE --output NEW_OUTPUT' if path == image else
                                         'python3 scripts/amp/package_uboot_host_fit.py --original ORIGINAL --build BUILD --output NEW_OUTPUT' if path == new_path else
                                         'bash scripts/amp/build_uboot_factory_boot_host.sh SOURCE NEW_OUTPUT enabled'})
    manifest = {
        'schema_version': 1, 'milestone': 'P028', 'date': '2026-10-03',
        'status': 'HOST_ONLY_LINUX_COMPATIBILITY_CANDIDATE_PENDING_APPROVAL',
        'amp_grade': 'C. HOST_BUILD_PASS', 'deployment_authorized': False,
        'board_access': False, 'source_commit': source_commit,
        'parent_source_commit': '2314a3f9f5795b88c7c53a805e9d59d82c6715b3',
        'patch': 'patches/rk3576-amp-platform/0009-uboot-runtime-policy-factory-boot.patch',
        'build_command': 'bash scripts/amp/build_uboot_factory_boot_host.sh SOURCE NEW_OUTPUT enabled',
        'package_command': package_result['command'],
        'original_partition_sha256': ORIGINAL_SHA,
        'new_partition_sha256': sha(full_extent),
        'logical_sector_size': 512, 'uboot_start_sector': '0x4000', 'uboot_sector_count': '0x4000',
        'signed': False, 'required_policy': 'Only explicit runtime vendor OP-TEE result zero permits raw factory Linux/CLI; failure/nonzero remains closed',
        'automatic_amp_start': 'SOURCE_VERIFIED_DISARMED; only explicit amp_m0load starts M0',
        'preserved_payloads': preserved,
        'original_tail_bytes': len(original) - len(new), 'original_tail_sha256': sha(original[len(new):]),
        'artifacts': records,
        'candidate_linux_cold_boot': 'UNVERIFIED',
        'remaining_board_gates': ['new proper U-Boot policy getter and factory Linux cold boot',
                                 'M0 effective mapping/reset/cache', 'BL31 MCU setter success'],
        'recovery_evidence': 'docs/reviews/rk3576-amp-platform-closure/P028_RECOVERY_READBACK.json',
        'P026_candidate': 'WITHDRAWN; old writer remains disabled',
    }
    (output / 'P028_CANDIDATE_MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'status': manifest['status'], 'file': str(image), 'size': len(full_extent),
                      'sha256': sha(full_extent), 'deployment_authorized': False}, indent=2))


if __name__ == '__main__':
    main()
