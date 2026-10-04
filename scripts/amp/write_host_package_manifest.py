#!/usr/bin/env python3
"""Hash the P023 Host package. This records evidence, never authorizes deployment."""
import argparse
import hashlib
import json
from pathlib import Path


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.project.resolve()
    sources = {
        'rtos': {'base': '7c397f41751feb29b0b388dfda3d2c2225f1f87c',
                 'parent': '1d0de06c394f89be35a4b6966e766b56f035c19d',
                 'commit': '3a39b0f4f5eb25631d05b4ffd88746ce9b1b0cdb'},
        'hal': {'base': '277de3fd4b0e640654ee73bb3308be2ef01e3aad',
                'commit': 'bc99978c1a030ad79610e89a5780dcd0ee3bb1f2'},
        'uboot': {'base': '8f53f800da2c25d0c6ba414fb45902a01675703a',
                  'parent': '7daeb0fc8ad0818a833b162405a35d0511d767bb',
                  'commit': '87f467be568f1189dce4b6eb65ab138279311984'},
        'kernel': {'base': '521833e2d28decbd6473d5717f1f96cc4108e208',
                   'commit': '521833e2d28decbd6473d5717f1f96cc4108e208',
                   'modification': '0003-linux-m0-uncached-pool-fail-closed.patch'},
    }
    records = []

    def add(relative, source, command, arch, purpose):
        path = root / relative
        if not path.is_file():
            raise FileNotFoundError(path)
        records.append(dict(file=relative, sha256=sha(path), size=path.stat().st_size,
                            source_commit=sources[source]['commit'] if source in sources else source,
                            build_command=command, architecture=arch, purpose=purpose,
                            deployment_authorized=False))

    m0 = 'artifacts/local/p023-m0-clean-v5/'
    for file in ['rtthread.elf', 'rtthread.map', 'rtthread.bin']:
        add(m0+file, 'rtos', 'RTT_ROOT=p023-rtos RTT_EXEC_PATH=arm-gnu-13.2/bin; cp board/evb/defconfig .config; scons --useconfig=.config; scons -c; scons -j4',
            'ARMv6-M / EABI5 / soft-float', 'Cold bypass minimal HELLO/PING proposal; requires derived HAL')
    dt = 'artifacts/local/p023-dt-fit-clean-v3/'
    add(dt+'rttmcu.bin','rtos','cp rtthread.bin rttmcu.bin','ARMv6-M','FIT payload; identical to final rtthread.bin')
    add(dt+'amp-host.itb','rtos','mkimage -f amp.its amp-host.itb','FIT standalone ARM','Unsigned Host proposal; SHA256 image hash, not signature')
    add(dt+'amp-host.dtbo','kernel','dtc -@ -I dts -O dtb -o amp-host.dtbo rk3576-lubancat-3-v2-m0-host.dtso','FDT overlay','Host-only MCU/reserved-memory/RPMsg candidate')
    add(dt+'amp-host.dtb','kernel','fdtoverlay -i original.dtb -o amp-host.dtb amp-host.dtbo','FDT','Original v2 DTB plus Host proposal')
    add('artifacts/local/p023-linux-clean/transport/rockchip_rpmsg_mbox.o','kernel','make -C copied-board-headers M=transport ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- rockchip_rpmsg_mbox.o','AArch64','Host transport syntax/API check; cannot replace existing built-in driver')
    add('artifacts/local/p023-linux-clean/echo/rk3576_amp_echo_test.ko','9c6f025681ed7ed72d9ffc2ee5d57993505a29ed','make -C copied-board-headers M=echo ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules','AArch64','Project echo source built against current board headers; not loaded')
    u = 'artifacts/local/p023-uboot-full-clean-v2/'
    for file in ['u-boot','u-boot.bin','u-boot.dtb']:
        add(u+file,'uboot','rk3576_defconfig; merge CONFIG_AMP/CONFIG_ROCKCHIP_AMP=y; olddefconfig; make O=fresh CROSS_COMPILE=aarch64-linux-gnu- -j4 u-boot.bin u-boot.dtb','AArch64 / FDT','AMP-enabled Host-only EVB-config build; not packaged uboot.img, not current board binary')
    k = 'artifacts/local/p023-kernel-full-clean-v3/'
    add(k+'arch/arm64/boot/Image','kernel','copy original board config; make O=fresh ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig; make -j4 Image modules rockchip/rk3576-lubancat-3-v2.dtb','AArch64','Prospective kernel including required-pool and device-barrier transport patch')
    add(k+'arch/arm64/boot/dts/rockchip/rk3576-lubancat-3-v2.dtb','kernel','make O=fresh ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- rockchip/rk3576-lubancat-3-v2.dtb','FDT','Unmodified board DTS compiled from pinned source; equals original board DTB')
    add(k+'echo/rk3576_amp_echo_test.ko','9c6f025681ed7ed72d9ffc2ee5d57993505a29ed','make -C pinned-source O=fresh-kernel M=echo ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules','AArch64','Project echo module for prospective full-built kernel; not loaded')

    evidence = []
    for relative in [m0+'rtconfig.h', m0+'.config', m0+'gcc_link.ld',
                     m0+'compiler.txt', m0+'build.log', m0+'readelf.txt', m0+'disassembly.txt',
                     dt+'amp.its', dt+'amp_contract.h', dt+'layout.json', dt+'build.json',
                     dt+'overlay.log', 'artifacts/local/p023-linux-clean/compiler.txt',
                     'artifacts/local/p023-linux-clean/echo/build.log', u+'.config', u+'full-build.log',
                     k+'.config', k+'board-original.config', k+'Module.symvers', k+'build.log', k+'echo/build.log',
                     'artifacts/local/p023-host-check-final.json', 'artifacts/local/p023-uboot-cold-mock.json',
                     'artifacts/local/p023-host-ci.log', 'artifacts/local/p023-tool-versions.txt', 'artifacts/local/p023-kernel-521833e2.tar.gz',
                     'scripts/amp/generate_host_proposal.py', 'scripts/amp/validate_host_proposal.py',
                     'scripts/amp/test_uboot_mcu_startup_draft.py', 'scripts/board/amp_echo_linux/rk3576_amp_echo_test.c',
                     'artifacts/local/p023-board-backup/boot-originals-complete.tar',
                     'artifacts/local/p023-board-backup/running-device-tree.tar',
                     'artifacts/local/p023-board-backup/manifest.json']:
        path = root / relative
        evidence.append(dict(file=relative, sha256=sha(path)))
    for path in sorted((root/'patches/rk3576-amp-platform').glob('000[3-6]*.patch')):
        evidence.append(dict(file=str(path.relative_to(root)),sha256=sha(path)))
    for path in sorted((root/'artifacts/local/p023-standard-tools').glob('*.deb')):
        evidence.append(dict(file=str(path.relative_to(root)),sha256=sha(path),source='Ubuntu 22.04 official archive; APT index hash checked; local extract only'))
    contract = root/'docs/amp/AMP_PLATFORM_CONTRACT.yaml'
    headers = root/'artifacts/local/amp-kernel-headers-20261001T134544Z-137756/linux-headers-6.1.99-rk3576'
    symbols_match = sha(headers/'Module.symvers') == sha(root/k/'Module.symvers')
    dtb_match = sha(root/dt/'original.dtb') == sha(root/k/'arch/arm64/boot/dts/rockchip/rk3576-lubancat-3-v2.dtb')
    if not symbols_match or not dtb_match:
        raise ValueError('Pinned source ABI or board DTB differs from copied originals')
    check = json.loads((root/'artifacts/local/p023-host-check-final.json').read_text())
    if check['contract_sha256'] != sha(contract) or check['status'] != 'HOST_PROPOSAL_PASS_NOT_DEPLOYABLE':
        raise ValueError('contract validation is stale or failed')
    result = dict(schema_version=1, status='HOST_PROPOSAL_PASS_NOT_DEPLOYABLE', amp_grade='C. HOST_BUILD_PASS',
                  project_base='9c6f025681ed7ed72d9ffc2ee5d57993505a29ed', sources=sources,
                  contract=dict(file='docs/amp/AMP_PLATFORM_CONTRACT.yaml', sha256=sha(contract)),
                  validation_checks=check['checks'], artifacts=records, evidence=evidence,
                  toolchains=dict(m0=(root/m0/'compiler.txt').read_text(),aarch64=(root/'artifacts/local/p023-linux-clean/compiler.txt').read_text(),dt_fit=(root/'artifacts/local/p023-tool-versions.txt').read_text()),
                  build_exit_codes=dict(m0=0, dt_overlay=0, fit=0, linux_board_headers=0, uboot_bin=0, prospective_kernel_image_modules_dtb=0, prospective_echo=0, host_ci=0),
                  kernel_identity=dict(module_symvers_matches_copied_headers=symbols_match,board_dtb_byte_matches_current=dtb_match,running_image_exact_source_build='UNVERIFIED'),
                  recovery_image='UNAVAILABLE: only incomplete .downloading file observed',
                  remaining_gate='Current boot AMP/load source/signing policy; SiP dynamic result; actual mapping/cache/UART runtime and offline recovery')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print('HOST_PACKAGE_MANIFEST',len(records),'artifacts; deployment_authorized=false')


if __name__ == '__main__':
    main()
