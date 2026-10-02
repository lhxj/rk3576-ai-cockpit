#!/usr/bin/env python3
"""Inspect fresh paired Host kernel/modules; no insertion or board access."""
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile

from p026_audit_board_modules import sections

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'artifacts/local/p026-kernel-isolated-clean'
RELEASE = '6.1.99-rk3576-m0echo-p026'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cpio_names(data):
    names = []
    offset = 0
    while offset < len(data):
        while offset < len(data) and data[offset] == 0:
            offset += 1
        if offset == len(data):
            break
        if data[offset:offset + 2] == b'\x1f\x8b':
            names.extend(cpio_names(gzip.decompress(data[offset:])))
            break
        assert data[offset:offset + 6] in (b'070701', b'070702'), 'unknown initrd format'
        fields = [int(data[offset + 6 + 8*i:offset + 14 + 8*i], 16) for i in range(13)]
        size, name_size = fields[6], fields[11]
        start = offset + 110
        assert name_size and start + name_size <= len(data)
        name = data[start:start + name_size - 1].decode()
        start = (start + name_size + 3) & ~3
        assert start + size <= len(data)
        offset = (start + size + 3) & ~3
        if name != 'TRAILER!!!':
            names.append(name)
    return names


def main():
    assert (BUILD / 'include/config/kernel.release').read_text().strip() == RELEASE
    config = (BUILD / '.config').read_text()
    assert 'CONFIG_LOCALVERSION="-rk3576-m0echo-p026"' in config
    assert '# CONFIG_LOCALVERSION_AUTO is not set' in config
    for name in ('EXT4_FS', 'MMC', 'MMC_SDHCI', 'MMC_SDHCI_OF_ARASAN', 'BLK_DEV_INITRD'):
        assert f'CONFIG_{name}=y' in config, 'early rootfs dependency not built in: ' + name
    assert '# CONFIG_MODVERSIONS is not set' in config
    # Supersede the intermediate archive containing Host build/source symlinks.
    archive = BUILD / ('modules-' + RELEASE + '.tar')
    intermediate = archive.with_suffix('.with-host-links.tar')
    if not intermediate.exists():
        archive.rename(intermediate)
        cmd = ['tar', '--sort=name', '--mtime=2026-10-03 00:00:00Z', '--owner=0', '--group=0',
               '--numeric-owner', '--exclude=lib/modules/' + RELEASE + '/build',
               '--exclude=lib/modules/' + RELEASE + '/source', '-C', str(BUILD / 'module-root'),
               '-cf', str(archive), 'lib/modules/' + RELEASE]
        subprocess.run(cmd, check=True)
    modules = []
    with tarfile.open(archive) as tar:
        for member in tar:
            assert member.name.startswith('lib/modules/' + RELEASE)
            assert not member.issym() and not member.islnk()
            if member.isfile() and member.name.endswith('.ko'):
                data = tar.extractfile(member).read()
                info = dict(v.decode().split('=', 1) for v in sections(data)['.modinfo'].split(b'\0') if b'=' in v)
                assert info['vermagic'].startswith(RELEASE + ' ')
                modules.append({'file': member.name, 'sha256': hashlib.sha256(data).hexdigest()})
    echo = BUILD / 'echo/rk3576_amp_echo_test.ko'
    assert len(modules) == 255, 'paired module population differs from inspected baseline'
    assert (RELEASE + ' ').encode() in sections(echo.read_bytes())['.modinfo']
    with tarfile.open(ROOT / 'artifacts/local/p025-post-recovery-20261002T173913Z-975579/boot-originals.tar') as tar:
        initrd = tar.extractfile('initrd.img-6.1.99-rk3576').read()
    names = cpio_names(initrd)
    initrd_modules = [n for n in names if n.endswith(('.ko', '.ko.xz', '.ko.zst', '.ko.gz'))]
    result = {'status': 'HOST_PAIRED_BUILD_PASS', 'release': RELEASE, 'board_access': False,
              'source_commit': '521833e2d28decbd6473d5717f1f96cc4108e208',
              'build_command': 'bash scripts/amp/p026_build_isolated_kernel.sh',
              'compiler': (BUILD / 'compiler.txt').read_text().splitlines()[0],
              'image_sha256': sha(BUILD / 'arch/arm64/boot/Image'),
              'config_sha256': sha(BUILD / '.config'), 'echo_sha256': sha(echo),
              'modules_archive_sha256': sha(archive), 'module_count': len(modules),
              'modules': modules, 'old_module_overwrite': False,
              'build_warning_count': (BUILD / 'build.log').read_text().count('warning:'),
              'echo_warning_count': (BUILD / 'echo/build.log').read_text().count('warning:'),
              'depmod_log': (BUILD / 'depmod.log').read_text(),
              'original_initrd_module_files': initrd_modules,
              'early_root_dependencies': 'EXT4/SDHCI MMC/initrd built in; original initrd compatibility not runtime-tested',
              'scope': 'New known-source paired candidate; not exact rebuild of running vendor Image'}
    (BUILD / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    checked = [BUILD / name for name in ('arch/arm64/boot/Image', 'echo/rk3576_amp_echo_test.ko',
               '.config', 'Module.symvers', 'modules-' + RELEASE + '.tar')]
    paired_initrd = BUILD / ('initrd.img-' + RELEASE)
    if paired_initrd.is_file():
        checked.append(paired_initrd)
    (BUILD / 'hashes.sha256').write_text(''.join(sha(p) + '  ' + str(p.relative_to(BUILD)) + '\n'
                                                for p in checked))
    print(json.dumps({k: v for k, v in result.items() if k != 'modules'}, indent=2))


if __name__ == '__main__':
    main()
