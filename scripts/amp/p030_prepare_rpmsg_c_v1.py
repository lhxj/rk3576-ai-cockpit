#!/usr/bin/env python3
"""Prepare passive C entry using exact board-proven v5 FIT. No board access."""
import argparse
import copy
import json
from pathlib import Path
import re
import struct
import tarfile
import zlib
from p030_prepare_tickdiag_v3 import ROOT, BASE_HELPER_SHA, CONTROL_SHA, sha, ident, require, run
from p029_prepare_signed_stage_b import script_text
from validate_host_proposal import fdt, cells

FIT_SHA = '348109ebecda0f00314d4dbcaaae0f8a51e2d88ac714c03ab74c1966a3b19bd6'
BIN_SHA = '28691e30790f1585782f3528dbd611c1e8005f8790947462574d73b8a75b6124'
DT_SHA = 'dd68818b7fd27e9abc56522d7c4782adb0b1dd49d274fe1daa75a32ad3366fbf'
ID = 'P030_RPMSG_C_V1'
DEST = '/boot/amp-p029/rpmsg-c-v1'


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    out = p.parse_args().output.resolve()
    require(not out.exists(), 'fresh output only')
    out.mkdir(parents=True)
    v5 = ROOT / 'artifacts/local/p030-tickdiag-v5'
    baseline = json.loads((ROOT / 'docs/reviews/rk3576-amp-platform-closure/P030_TICKDIAG_V5_B_EXECUTION.json').read_text())
    require(baseline['acceptance']['B_tick_and_first_delay_and_bounded_exit'] == 'PASS' and
            baseline['acceptance']['post_M0_cold_default_recovery'].startswith('PASS'), 'B and cold recovery gates')
    spec = copy.deepcopy(json.loads((v5 / 'packet/TICKDIAG.json').read_text()))
    fit = (v5 / 'packet/amp-signed.itb').read_bytes()
    require(len(fit) == 131072 and sha(fit) == FIT_SHA, 'exact board-proven v5 FIT')
    nodes = fdt(fit)
    require(nodes['/images/mcu/hash']['value'].hex() == BIN_SHA and
            len(fit[4096:4096+125704]) == 125704 and sha(fit[4096:4096+125704]) == BIN_SHA,
            'exact board-proven payload')
    require(cells(nodes['/']['totalsize']) == (len(fit),) and
            cells(nodes['/images/mcu']['load']) == (0x47800000,) and
            cells(nodes['/images/mcu']['entry']) == (0x141,), 'container/load/entry')
    tool = ROOT / 'artifacts/local/p028-build-final-v4/tools/fit_check_sign'
    require(sha(tool.read_bytes()) == '3175a54383c7fc8e65b4a57a1dbcda3251f032906244fbce48cada5382b60ef8', 'vendor verifier identity')
    control = ROOT / 'artifacts/local/p030-final-fdt-package-v1/fdt.bin'
    require(sha(control.read_bytes()) == CONTROL_SHA, 'existing control key')
    run([tool, '-f', v5 / 'packet/amp-signed.itb', '-k', control, '-s'], out, out / 'verify.log')
    require(b'Signature check OK' in (out / 'verify.log').read_bytes(), 'FIT signature')
    old = ROOT / 'artifacts/local/p029-packet-v6'
    dt = (old / 'boot/stage-C.dtb').read_bytes()
    require(sha(dt) == DT_SHA and len(dt) == 0x4b455, 'paired C DT')
    n = fdt(dt)
    require(n['/chosen']['project,p029-stage'] == b'C\0', 'C DT tag')
    for name, base, size in [('mcu@47800000', 0x47800000, 0x80000),
                             ('rpmsg@47d00000', 0x47d00000, 0x10000),
                             ('rpmsg-dma@47d10000', 0x47d10000, 0x10000)]:
        props = n['/reserved-memory/' + name]
        require(cells(props['reg']) == (0, base, 0, size) and props.get('no-map') == b'' and
                'reusable' not in props, 'reserved no-map ' + name)
    for name in ['/mcu-amp', '/rpmsg@47d00000', '/mailbox@2ae50000', '/mailbox@2ae54000']:
        require(n[name]['status'] == b'okay\0', 'C enabled ' + name)
    require(not any(k.startswith('/mcu-amp/') for k in n) and
            n['/serial@2ad80000']['status'] == b'disabled\0', 'no Linux second boot or UART driver')
    props = n['/rpmsg@47d00000']
    require(cells(props['reg']) == (0, 0x47d00000, 0, 0x10000) and
            cells(props['rockchip,link-id']) == (4,) and cells(props['rockchip,vdev-nums']) == (1,), 'C ring/link')
    require(props.get('project,require-shared-dma-pool') == b'' and
            cells(props['memory-region']) == cells(n['/reserved-memory/rpmsg-dma@47d10000']['phandle']) and
            n['/reserved-memory/rpmsg-dma@47d10000']['compatible'] == b'shared-dma-pool\0', 'required fixed pool')
    require(cells(props['mboxes']) == (cells(n['/mailbox@2ae50000']['phandle'])[0], 0,
                                     cells(n['/mailbox@2ae54000']['phandle'])[0], 0), 'RX0/TX4 ch0')
    ko = (old / 'boot/rk3576_amp_echo_test.ko').read_bytes()
    require(len(ko) == 40920 and sha(ko) == 'cd82f24659ba4a5a09fc314f5d9d3372b28451c8b21c7f64a868b11d11b5fe43' and
            b'vermagic=6.1.99-rk3576-m0echo-p026 SMP mod_unload aarch64\0' in ko, 'exact matching KO')
    for name in ['Image', 'initrd']:
        raw = (old / 'boot' / name).read_bytes()
        require(ident(raw) == spec['board_precondition']['paired_assets'][name], 'paired ' + name)
    command = (v5 / 'packet/stage-B.cmd').read_bytes()
    require(sha(command) == 'c8d4479174ea5c60724a14af05c61b4c2bc2a6d4537f62fcfa2257fb9cd44235', 'proven B command')
    command = command.replace(b'P030 M0 tick diagnostic v5 B', b'P030 RPMsg C v1 with proven tickdiag-v5')
    command = command.replace(b'stage-B.dtb', b'stage-C.dtb').replace(b'0x4b461', b'0x4b455').replace(b' 4b461;', b' 4b455;')
    command = command.replace(b'amp_test_stage=B\'', b'amp_test_stage=C dyndbg="file virtio_rpmsg_bus.c +p"\'')
    command = command.replace(b'tickdiag-v5/amp-signed.itb', b'rpmsg-c-v1/amp-signed.itb')
    require(command.count(b'amp_m0load /amp-p029/rpmsg-c-v1/amp-signed.itb 0x48300000') == 1 and
            b'amp_test_stage=C dyndbg="file virtio_rpmsg_bus.c +p"' in command and
            b'stage-B.dtb' not in command and b'4b461' not in command and b'bootargs_ext' in command and
            b'"0x20000"' in command and b'insmod' not in command, 'single-loader C command and exact lengths')
    component = struct.pack('>II', len(command), 0) + command
    header = bytearray((v5 / 'packet/stage-B.scr').read_bytes()[:64])
    struct.pack_into('>I', header, 12, len(component))
    struct.pack_into('>I', header, 24, zlib.crc32(component))
    header[4:8] = bytes(4)
    header[32:64] = b'P030 RPMsg C v1'.ljust(32, b'\0')
    struct.pack_into('>I', header, 4, zlib.crc32(header))
    scr = bytes(header) + component
    require(script_text(scr) == command, 'correct single-component SCRIPT CRC and zero terminator')
    preflight = (ROOT / 'scripts/board/p030_rpmsg_c_preflight.py').read_bytes()
    compile(preflight, 'preflight-c.py', 'exec')
    payloads = {'amp-signed.itb': fit, 'stage-C.cmd': command, 'stage-C.scr': scr, 'preflight-c.py': preflight}
    spec.pop('stage_b')
    spec.update(id=ID, destination=DEST, source_host_tree=str(out.relative_to(ROOT)),
                files={name: ident(raw) for name, raw in payloads.items()},
                stage_c={'m0_load_invocations': 1, 'loads_ko': False, 'includes_stage_c': True,
                         'boots_paired_linux_only_after_m0_loader_success': True,
                         'manual_cold_source_and_KO_required': True, 'agent_started_m0': False,
                         'rebooted_by_agent': False},
                prior_v5_B_and_cold_recovery='PASS_USER_LOGS_AND_PHYSICAL_REPORT',
                source_patch_policy='exact proven v5 FIT; no firmware, kernel, DT or KO changes')
    spec['tick_diagnostic']['hardware_root_cause'] = 'B_RUNTIME_RATE_DELAY_PASS; C_UNVERIFIED'
    spec['board_precondition']['paired_assets'].update({'stage-C.dtb': ident(dt), 'rk3576_amp_echo_test.ko': ident(ko)})
    pre = spec['board_precondition']
    names = ['amp-signed.itb', 'stage-B.cmd', 'stage-B.scr', 'TICKDIAG.json', 'SHA256SUMS']
    for name in names:
        pre['existing_amp_p029_files']['tickdiag-v5/' + name] = ident((v5 / 'packet' / name).read_bytes())
    receipt = json.loads(next(x for x in (v5 / 'install-output.txt').read_text().splitlines() if x.startswith('{')))
    digest = receipt.pop('receipt_sha256')
    receipt.pop('ram_source_removed')
    raw_receipt = (json.dumps(receipt, sort_keys=True, indent=2) + '\n').encode()
    require(sha(raw_receipt) == digest, 'exact installed v5 receipt')
    pre['existing_amp_p029_files']['tickdiag-v5/INSTALL_RECEIPT.json'] = ident(raw_receipt)
    pre['existing_amp_p029_top_level'] = sorted(pre['existing_amp_p029_top_level'] + ['tickdiag-v5'])
    pre['existing_nested_members']['tickdiag-v5'] = sorted(names + ['INSTALL_RECEIPT.json'])
    manifest = (json.dumps(spec, sort_keys=True, indent=2) + '\n').encode()
    base = (ROOT / 'scripts/board/p030_install_initdiag_v1.py').read_bytes()
    require(sha(base) == BASE_HELPER_SHA, 'reviewed unchanged installer base')
    installer = base.decode().split('def resume_receipt(', 1)[0]
    installer = installer.replace('P030_INITDIAG.json', 'RPMSG_C.json').replace('initdiag-v1', 'rpmsg-c-v1')
    installer = installer.replace('P030_M0_INITDIAG_V1', ID).replace('stage-B', 'stage-C').replace('stage_b', 'stage_c')
    installer = installer.replace('PAYLOADS = {"amp-signed.itb", "stage-C.cmd", "stage-C.scr"}',
                                  'PAYLOADS = {"amp-signed.itb", "stage-C.cmd", "stage-C.scr", "preflight-c.py"}')
    installer = installer.replace('get("includes_stage_c") is not False', 'get("includes_stage_c") is not True')
    installer, count = re.subn(r'PINNED_MANIFEST_SHA = "[0-9a-f]{64}"', 'PINNED_MANIFEST_SHA = "' + sha(manifest) + '"', installer)
    require(count == 1 and '"preflight-c.py"}' in installer, 'C helper pin/whitelist')
    installer = installer.replace('P030_INITDIAG_PASSIVE_STAGE_INSTALLED_READBACK_PASS', 'P030_RPMSG_C_V1_PASSIVE_STAGE_READBACK_PASS')
    installer += '''
if __name__ == "__main__":
    if len(sys.argv) != 5 or sys.argv[1] != "--approved-p030-rpmsg-c-v1" or sys.argv[3] != "--installer-sha":
        raise SystemExit("STOP explicit C manifest/installer hashes required")
    install(sys.argv[2], sys.argv[4])
'''
    compile(installer, 'install-p030.py', 'exec')
    packet = out / 'packet'
    packet.mkdir()
    payloads['RPMSG_C.json'] = manifest
    payloads['SHA256SUMS'] = ''.join(f'{sha(raw)}  {name}\n' for name, raw in sorted(payloads.items())).encode()
    payloads['install-p030.py'] = installer.encode()
    for name, raw in payloads.items():
        (packet / name).write_bytes(raw)
    with tarfile.open(out / 'rpmsg-c-v1.tar.gz', 'w:gz') as archive:
        for path in sorted(packet.iterdir()):
            archive.add(path, arcname=path.name)
    report = {'state': 'HOST_C_PACKAGE_VALIDATED_BOARD_RUNTIME_UNVERIFIED', 'files': spec['files'],
              'manifest_sha256': sha(manifest), 'installer_sha256': sha(installer.encode()),
              'archive': ident((out / 'rpmsg-c-v1.tar.gz').read_bytes()),
              'C_DT': ident(dt), 'KO': ident(ko), 'source': 'proven v5 binary unchanged; paired C assets unchanged',
              'destination': DEST, 'signature_control_sha256': CONTROL_SHA, 'C_ready': False, 'D_ready': False,
              'scope': 'new passive C entry and read-only runtime preflight, no agent source/M0/KO/MMIO/reboot'}
    (out / 'build-result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
