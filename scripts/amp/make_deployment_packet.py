#!/usr/bin/env python3
"""Generate a blocked, reviewable Host deployment packet; never access a board."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
REVIEW = ROOT / 'docs/reviews/rk3576-amp-platform-closure'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def record(path, **extra):
    path = ROOT / path
    data = path.read_bytes()
    return {'file': str(path.relative_to(ROOT)), 'size': len(data),
            'sha256': sha(data), **extra}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline', type=Path, required=True)
    ap.add_argument('--uboot-build', type=Path, required=True)
    ap.add_argument('--uboot-source', type=Path, required=True)
    ap.add_argument('--uboot-package', type=Path, required=True)
    args = ap.parse_args()
    contract_path = ROOT / 'docs/amp/AMP_PLATFORM_CONTRACT.yaml'
    contract = json.loads(contract_path.read_text())
    old_manifest = json.loads((REVIEW / 'HOST_PACKAGE_MANIFEST.json').read_text())
    recovery = json.loads((REVIEW / 'RECOVERY_IMAGE_IDENTITY.json').read_text())
    serial_path = ROOT / 'docs/reviews/rk3576-amp-board-evidence/SPL_VERIFIED_BOOT_SERIAL_EVIDENCE.json'
    serial = json.loads(serial_path.read_text())
    result = (args.baseline / 'result.txt').read_text()
    if 'inventory_ssh_exit_code=0' not in result or 'ordinary_boot_and_running_dt_copy=PASS' not in result:
        raise ValueError('read-only backup did not complete')
    with tarfile.open(args.baseline / 'boot-originals.tar') as archive:
        original = {}
        for member in archive.getmembers():
            if member.isfile():
                original[member.name] = {'type': 'file', 'sha256': sha(archive.extractfile(member).read()),
                                         'size': member.size, 'mode': oct(member.mode)}
            elif member.issym():
                original[member.name] = {'type': 'symlink', 'target': member.linkname,
                                         'sha256_link_text': sha(member.linkname.encode())}
    assert original['Image']['target'] == 'Image-6.1.99-rk3576'
    assert original['rk-kernel.dtb']['target'] == 'dtb/rk3576-lubancat-3-v2.dtb'
    inventory = (args.baseline / 'inventory.txt').read_text()
    for identity in ('6.1.99-rk3576 #8', 'uboot-8f53f800da-04/24/2026', 'bl31-v1.14', 'EmbedFire LubanCat-3-v2'):
        if identity not in inventory:
            raise ValueError('post-recovery baseline identity changed: ' + identity)
    table = inventory.split('COMMAND: lsblk ', 1)[1].split('EXIT_CODE=', 1)[0]
    labels = [line.split()[3] for line in table.splitlines()
              if len(line.split()) >= 4 and line.split()[2] == 'part']
    if labels != ['uboot', 'boot', 'rootfs']:
        raise ValueError('partition baseline changed: ' + repr(labels))
    for name in ('Image-6.1.99-rk3576', 'config-6.1.99-rk3576', 'boot.cmd', 'boot.scr',
                 'uEnv/uEnv.txt', 'dtb/rk3576-lubancat-3-v2.dtb'):
        if original[name]['sha256'] not in inventory:
            raise ValueError('copied boot file disagrees with board inventory: ' + name)
    baseline = {'evidence': 'BOARD_OBSERVED_READONLY', 'ssh_exit_code': 0,
                'kernel': '6.1.99-rk3576 #8 2026-04-24', 'uboot_version': '8f53f800da-04/24/2026',
                'bl31_version': 'v1.14', 'amp_partition_present': False,
                'files': original,
                'evidence_files': [record(args.baseline / name) for name in
                                   ('inventory.txt', 'running-device-tree.tar', 'boot-originals.tar')]}
    # Preserve the P023 source/build history; exclude the now superseded U-Boot.
    selected = [a for a in old_manifest['artifacts'] if
                'uboot' not in a['file'] and not a['file'].endswith('rockchip_rpmsg_mbox.o')]
    artifacts = []
    for artifact in selected:
        actual = record(artifact['file'])
        if actual['sha256'] != artifact['sha256'] or actual['size'] != artifact['size']:
            raise ValueError('P023 artifact identity mismatch: ' + artifact['file'])
        artifacts.append({**artifact, 'built_milestone': 'P023', 'rechecked_milestone': 'P025'})
    source_sha = subprocess.check_output(['git', '-C', str(args.uboot_source),
                                           'rev-parse', 'HEAD'], text=True).strip()
    if subprocess.check_output(['git', '-C', str(args.uboot_source), 'status', '--porcelain'], text=True).strip():
        raise ValueError('derived U-Boot source is not committed and clean')
    for name in ('u-boot', 'u-boot.bin', 'u-boot-nodtb.bin', 'u-boot.dtb'):
        artifacts.append(record(args.uboot_build / name, source_commit=source_sha,
            build_command='bash scripts/amp/build_uboot_fit_policy_host.sh SOURCE NEW_OUTPUT enabled',
            architecture='FDT' if name.endswith('.dtb') else 'AArch64',
            purpose='AMP/FIT policy Host build; no board execution', deployment_authorized=False,
            built_milestone='P025'))
    for name in ('loader-host.itb', 'uboot-host-unsigned.img'):
        artifacts.append(record(args.uboot_package / name, source_commit=source_sha,
            build_command='python3 scripts/amp/package_uboot_host_fit.py --original recovery/uboot.payload --build NEW_BUILD --output NEW_PACKAGE',
            architecture='FIT / AArch64 + retained firmware', signed=False,
            purpose='Unsigned Host package; retains exact original BL31/OP-TEE/FDT',
            deployment_authorized=False, built_milestone='P025'))
    sources = dict(old_manifest['sources'])
    sources['uboot'] = {'base': sources['uboot']['base'], 'parent': sources['uboot']['commit'],
                         'commit': source_sha, 'patch': '0007-uboot-fit-policy-and-partition-bounds.patch'}
    manifest = {'schema_version': 1, 'date': '2026-10-03', 'amp_grade': 'C. HOST_BUILD_PASS',
                'status': 'HOST_PACKET_NOT_DEPLOYABLE', 'contract': record(contract_path),
                'sources': sources, 'artifacts': artifacts, 'baseline': baseline,
                'recovery_image': recovery['image']['file'],
                'recovery_image_sha256': recovery['image']['sha256'],
                'whole_board_recovery': 'PASS / BOARD_OBSERVED_USER_REPORT',
                'boot_policy_evidence': {
                    'source': record(serial_path),
                    'evidence_level': serial['evidence_level'],
                    'spl_signature_required_for_recorded_fit': serial['spl']['signature_required_for_recorded_fit'],
                    'proper_uboot_amp_policy': serial['uboot_proper']['amp_signature_policy'],
                    'scope': 'SPL result is not proper U-Boot AMP policy or global OTP state'},
                'evidence': [record(args.uboot_build / n) for n in
                             ('.config', 'compiler.txt', 'build.log', 'hashes.sha256')] +
                            [record(args.uboot_package / 'result.json'),
                             record('artifacts/local/p025-fit-policy-test-final/result.json'),
                             record('artifacts/local/p025-host-layout-check.json'),
                             record('artifacts/local/p025-cold-reset-test.json'),
                             record('patches/rk3576-amp-platform/0007-uboot-fit-policy-and-partition-bounds.patch')]}
    by_file = {a['file']: a for a in artifacts}

    def add_file(source, destination, purpose):
        a = by_file[source]
        return {'kind': 'new_file', 'source': source, 'destination': destination,
                'old': 'ABSENT_RECHECK_BEFORE_WRITE', 'new_sha256': a['sha256'],
                'purpose': purpose, 'rollback': 'retain original files; restore the original symlink first',
                'deployment_authorized': False}

    def link(name, target):
        return {'kind': 'symlink', 'destination': '/boot/' + name, 'old': original[name],
                'new': {'type': 'symlink', 'target': target,
                        'sha256_link_text': sha(target.encode())},
                'rollback': 'restore old.target after original target file SHA256 check',
                'deployment_authorized': False}

    changes = [
        add_file('artifacts/local/p023-kernel-full-clean-v3/arch/arm64/boot/Image',
                 '/boot/Image-6.1.99-rk3576-m0echo-host', 'built-in uncached RPMsg transport; Host candidate'),
        add_file('artifacts/local/p023-dt-fit-clean-v3/amp-host.dtb',
                 '/boot/dtb/rk3576-lubancat-3-v2-m0echo-host.dtb', 'MCU DDR reservation and MBOX0/4 DT; Host candidate'),
        link('Image', 'Image-6.1.99-rk3576-m0echo-host'),
        link('rk-kernel.dtb', 'dtb/rk3576-lubancat-3-v2-m0echo-host.dtb'),
        {'kind': 'firmware_partition', 'source': str((args.uboot_package/'uboot-host-unsigned.img').relative_to(ROOT)),
         'destination': '/dev/disk/by-partlabel/uboot (observed mmcblk0p1; identity must be rechecked)',
         'old_sha256': None, 'old_prefix_payload_sha256': '06f48ed3716ccdde6adcb3187ca3eff766927e27bf6b7f432046539b782c4a56',
         'old_prefix_evidence_scope': 'previous read-only board prefix and recovery image; not a current raw re-read',
         'new_sha256': manifest['artifacts'][-1]['sha256'], 'risk': 'early M0 loader; verified-boot policy unknown',
         'rollback': 'official complete update.img through user-tested recovery path', 'deployment_authorized': False},
        {'kind': 'amp_partition', 'source': 'artifacts/local/p023-dt-fit-clean-v3/amp-host.itb',
         'destination': None, 'old': 'NO_AMP_PARTITION',
         'new_sha256': by_file['artifacts/local/p023-dt-fit-clean-v3/amp-host.itb']['sha256'],
         'required_label': 'amp', 'risk': 'GPT/rootfs change requires separate exact plan and user-data backup',
         'rollback': 'official complete update.img; restores original three-partition design', 'deployment_authorized': False},
        add_file('artifacts/local/p023-kernel-full-clean-v3/echo/rk3576_amp_echo_test.ko',
                 '/opt/rk3576-amp-test/rk3576_amp_echo_test.ko', 'manual HELLO/PING only after future boot acceptance; no autoload'),
    ]
    packet = {'schema_version': 1, 'status': 'BLOCKED', 'deployment_authorized': False,
              'manifest': 'P025_ARTIFACT_MANIFEST.json', 'contract': record(contract_path),
              'proposal': contract['host_proposal'], 'changes': changes,
              'untouched_originals': ['uEnv/uEnv.txt', 'boot.cmd', 'boot.scr', 'initrd.img-6.1.99-rk3576',
                                      'Image-6.1.99-rk3576', 'dtb/rk3576-lubancat-3-v2.dtb'],
              'gates': {
                  'runtime_sip_mapping_and_cache': 'BLOCKED',
                  'actual_verified_boot_and_authorized_key': 'BLOCKED',
                  'amp_gpt_destination_and_userdata_backup': 'BLOCKED',
                  'full_code_shared_preload_reservation': 'BLOCKED',
                  'candidate_kernel_module_upgrade_identity': 'BLOCKED',
                  'uart5_runtime_ownership_and_ttl_wiring': 'BLOCKED',
                  'current_raw_uboot_prewrite_identity': 'BLOCKED',
                  'whole_board_recovery_user_test': 'PASS'},
              'first_test_scope': ['Linux boot', 'M0 banner', 'HELLO/HELLO_ACK', 'PING/PONG'],
              'reboot_required_for_future_boot_chain_change': True,
              'approval_gate_board_test': 'CLOSED'}
    for name, value in [('P025_ARTIFACT_MANIFEST.json', manifest),
                        ('P025_DEPLOYMENT_CHANGESET.json', packet)]:
        (REVIEW / name).write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')
    print('HOST_PACKET_WRITTEN_NOT_DEPLOYABLE')


if __name__ == '__main__':
    main()
