#!/usr/bin/env python3
"""Compile and sign a fresh bounded local-tick diagnostic; no board access."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tarfile
import zlib

from p029_prepare_signed_stage_b import script_text
from validate_host_proposal import fdt

ROOT = Path(__file__).resolve().parents[2]
BASE_HELPER_SHA = 'db21eba32dfc06a43920ba5ce0c0babdd668d71f323e8f7d85a51fd0d9c85466'
CONTROL_SHA = '43164981efd8869432e42fab2411d99941d17476700f3fc03ab76948621049ce'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def ident(data):
    return {'size': len(data), 'sha256': sha(data)}

def require(ok, message):
    if not ok:
        raise ValueError(message)

def run(argv, cwd, log):
    result = subprocess.run([str(x) for x in argv], cwd=cwd, capture_output=True,
                            timeout=120, env=dict(os.environ, SOURCE_DATE_EPOCH='1790985600'))
    log.write_bytes(result.stdout + b'\nSTDERR\n' + result.stderr)
    result.check_returncode()

def build(out, revision):
    require(not out.exists(), 'fresh output required')
    hal = ROOT.parent / 'p023-hal'
    rtos = ROOT.parent / 'p023-rtos'
    require(subprocess.check_output(['git', '-C', hal, 'rev-parse', 'HEAD'], text=True).strip() ==
            'bc99978c1a030ad79610e89a5780dcd0ee3bb1f2', 'fixed HAL commit')
    require(not subprocess.check_output(['git', '-C', hal, 'status', '--porcelain']), 'unchanged HAL')
    out.mkdir(parents=True)
    tree = out / 'source'
    tree.mkdir()
    archive = subprocess.run(['git', '-C', rtos, 'archive',
                              '3a39b0f4f5eb25631d05b4ffd88746ce9b1b0cdb'], capture_output=True, check=True)
    # This exact pinned tree contains the intentional HAL symlink outside the
    # RTOS tree. Extract the trusted git archive, then replace that link below.
    subprocess.run(['tar', '-xf', '-', '-C', tree], input=archive.stdout, check=True)
    link = tree / 'bsp/rockchip/common/hal'
    require(link.is_symlink(), 'HAL symlink')
    link.unlink()
    link.symlink_to(hal)
    patches = ['0010-m0-bounded-runtime-evidence.patch', '0011-rtthread-mbox-client-pointer.patch',
               '0012-m0-init-checkpoints.patch', '0013-m0-tick-preflight.patch']
    if revision == 4:
        patches.append('0014-m0-tick-diagnostic-short-lines.patch')
    for name in patches:
        patch = ROOT / 'patches/rk3576-amp-platform' / name
        subprocess.run(['git', '-C', tree, 'apply', '--check', patch], check=True)
        subprocess.run(['git', '-C', tree, 'apply', patch], check=True)
    toolchain = Path('/home/ywx/.local/toolchains/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi/bin')
    bsp = tree / 'bsp/rockchip/rk3576-mcu'
    (bsp / '.config').write_bytes((bsp / 'board/evb/defconfig').read_bytes())
    env = dict(os.environ, RTT_ROOT=str(tree), RTT_EXEC_PATH=str(toolchain))
    for args, name in [(['scons', '--useconfig=.config'], 'config.log'), (['scons', '-j4'], 'build.log')]:
        result = subprocess.run(args, cwd=bsp, env=env, capture_output=True, timeout=180)
        (out / name).write_bytes(result.stdout + result.stderr)
        result.check_returncode()
    for name in ['rtthread.elf', 'rtthread.bin', 'rtthread.map', 'rtconfig.h']:
        (out / name).write_bytes((bsp / name).read_bytes())

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--existing-build', action='store_true', help='Package reviewed existing clean build')
    parser.add_argument('--revision', type=int, choices=[3, 4], default=4)
    args = parser.parse_args()
    out = args.output.resolve()
    revision = args.revision
    destination = f'/boot/amp-p029/tickdiag-v{revision}'
    package_id = f'P030_M0_TICKDIAG_V{revision}'
    if not args.existing_build:
        build(out, revision)
    require(not (out / 'packet').exists(), 'no replacement of prior packet')
    raw = (out / 'rtthread.bin').read_bytes()
    stack, entry = struct.unpack_from('<II', raw)
    require(stack == 0x80000 and entry == 0x141 and len(raw) < 0x80000, 'code bounds and vectors')
    require(not re.search(rb'warning:|error:', (out / 'build.log').read_bytes()), 'warning/error-free build')
    require('#define RT_TICK_PER_SECOND 100' in (out / 'rtconfig.h').read_text(), 'fixed 100Hz tick')
    board_source = (out / 'source/bsp/rockchip/rk3576-mcu/board/evb/board.c').read_text()
    formats = re.findall(r'rt_kprintf\("(P030 (?:tick|STOP tick)[^"\n]*)"', board_source)
    def max_line_bytes(fmt):
        fmt = fmt.replace(r'\n', '\n')
        return len(re.sub(r'%s|%08x|%u|%d', lambda m: {'%s': 's'*18, '%08x': 'f'*8,
                       '%u': '9'*10, '%d': '-'+'9'*10}[m.group()], fmt).encode())
    largest_line = max(map(max_line_bytes, formats))
    if revision == 4:
        require(largest_line < 128, 'every diagnostic line fits 128-byte console buffer')
    tools = ROOT / 'artifacts/local/p028-build-final-v4/tools'
    for name, digest in [('mkimage', '988a2b46efb8dc21710b82a0b856496dbc13fa2b0fc194f3f947f1ba35a61f83'),
                         ('fit_check_sign', '3175a54383c7fc8e65b4a57a1dbcda3251f032906244fbce48cada5382b60ef8')]:
        require(sha((tools / name).read_bytes()) == digest, 'pinned vendor tool ' + name)
    control = ROOT / 'artifacts/local/p030-final-fdt-package-v1/fdt.bin'
    require(sha(control.read_bytes()) == CONTROL_SHA, 'actual proper control key DT')
    signed = out / 'signed'
    signed.mkdir()
    (signed / 'rttmcu.bin').write_bytes(raw)
    its = (ROOT / 'artifacts/local/p030-m0-initdiag-v1/vendor-fit-v2/amp-signed.its').read_text()
    total = (0x1000 + len(raw) + 511) & ~511
    its = its.replace('0x1f3e0', f'0x{total:x}')
    (signed / 'amp-signed.its').write_text(its)
    secrets = ROOT / 'artifacts/local/p029-signed-v8/private'
    require((secrets / 'p029dev.key').stat().st_mode & 0o077 == 0, 'private key mode')
    run([tools / 'mkimage', '-f', 'amp-signed.its', '-E', '-p', '0x1000', '-k', secrets,
         '-r', 'amp-signed.itb'], signed, out / 'sign.log')
    fit = (signed / 'amp-signed.itb').read_bytes()
    nodes = fdt(fit)
    cell = lambda b: struct.unpack('>I', b)[0]
    require(cell(nodes['/']['totalsize']) == len(fit), 'authenticated total container length')
    require(struct.unpack_from('>I', fit, 4)[0] < 0x1000, 'bounded FIT header')
    props = nodes['/images/mcu']
    require(cell(props['data-position']) == 0x1000 and cell(props['data-size']) == len(raw)
            and fit[0x1000:0x1000+len(raw)] == raw, 'exact external payload')
    require(nodes['/images/mcu/hash']['value'].hex() == sha(raw), 'payload hash')
    require(cell(props['load']) == 0x47800000 and cell(props['entry']) == entry, 'FIT entry/load')
    require(set(nodes['/configurations/conf/signature']['hashed-nodes'].rstrip(b'\0').decode().split('\0')) ==
            {'/', '/configurations', '/configurations/conf', '/images/mcu', '/images/mcu/hash'}, 'signature coverage')
    run([tools / 'fit_check_sign', '-f', signed / 'amp-signed.itb', '-k', control, '-s'],
        signed, out / 'verify.log')
    require(b'Signature check OK' in (out / 'verify.log').read_bytes(), 'actual control-key signature check')
    old = ROOT / 'artifacts/local/p030-m0-initdiag-v1/packet'
    command = (old / 'stage-B.cmd').read_bytes().replace(b'initdiag-v1/amp-signed.itb', f'tickdiag-v{revision}/amp-signed.itb'.encode())
    command = command.replace(b'"0x1f800"', f'"0x{len(fit):x}"'.encode())
    command = command.replace(b'P030 M0 initialization diagnostic B', f'P030 M0 tick diagnostic v{revision} B'.encode())
    require(command.count(f'amp_m0load /amp-p029/tickdiag-v{revision}/amp-signed.itb'.encode()) == 1, 'one M0 call')
    require(b'initdiag-v1/amp-signed.itb' not in command and b'"0x1f800"' not in command, 'new FIT path and length')
    data = struct.pack('>II', len(command), 0) + command
    previous = ROOT / 'artifacts/local/p030-initdiag-source-fix-v2/packet/stage-B.scr'
    header = bytearray(previous.read_bytes()[:64])
    struct.pack_into('>I', header, 12, len(data))
    struct.pack_into('>I', header, 24, zlib.crc32(data))
    header[4:8] = bytes(4)
    struct.pack_into('>I', header, 4, zlib.crc32(header))
    scr = bytes(header) + data
    require(script_text(scr) == command, 'zero-terminated single component and exact command')
    base_manifest = ROOT / 'artifacts/local/p030-initdiag-source-fix-v2/packet/SOURCE_FIX.json'
    spec = copy.deepcopy(json.loads(base_manifest.read_text()))
    for obsolete in ['correction']:
        spec.pop(obsolete, None)
    spec.update(id=package_id, destination=destination,
                source_host_tree=str(out.relative_to(ROOT)),
                tick_diagnostic={'probe_reads': 1048576, 'ISR_prints': False, 'clock_fallback': False,
                                 'failed_tick_preflight_stops_before_RPMsg_init_or_delay': True,
                                 'hardware_root_cause': 'UNVERIFIED'})
    spec['source_patches'].append({'path': 'patches/rk3576-amp-platform/0013-m0-tick-preflight.patch',
                                 'sha256': sha((ROOT / 'patches/rk3576-amp-platform/0013-m0-tick-preflight.patch').read_bytes())})
    if revision == 4:
        patch = ROOT / 'patches/rk3576-amp-platform/0014-m0-tick-diagnostic-short-lines.patch'
        spec['source_patches'].append({'path': str(patch.relative_to(ROOT)), 'sha256': sha(patch.read_bytes())})
    payloads = {'amp-signed.itb': fit, 'stage-B.cmd': command, 'stage-B.scr': scr}
    spec['files'] = {name: ident(b) for name, b in payloads.items()}
    spec['signed_fit'].update(bytes=len(fit), sha256=sha(fit), payload_bytes=len(raw), payload_sha256=sha(raw))
    pre = spec['board_precondition']
    pre['existing_amp_p029_top_level'] = sorted(pre['existing_amp_p029_top_level'] + ['initdiag-source-fix-v2'])
    v2packet = ROOT / 'artifacts/local/p030-initdiag-source-fix-v2/packet'
    names = ['SOURCE_FIX.json', 'SHA256SUMS', 'stage-B.cmd', 'stage-B.scr']
    for name in names:
        pre['existing_amp_p029_files']['initdiag-source-fix-v2/' + name] = ident((v2packet / name).read_bytes())
    receipt = json.loads((ROOT / 'artifacts/local/p030-initdiag-source-fix-v2/passive-stage-result.json').read_text())
    expected_receipt_sha = receipt.pop('receipt_sha256')
    receipt.pop('ram_source_removed')
    receipt_raw = (json.dumps(receipt, sort_keys=True, indent=2) + '\n').encode()
    require(sha(receipt_raw) == expected_receipt_sha, 'exact prior receipt reconstruction')
    pre['existing_amp_p029_files']['initdiag-source-fix-v2/INSTALL_RECEIPT.json'] = ident(receipt_raw)
    pre['existing_nested_members']['initdiag-source-fix-v2'] = sorted(names + ['INSTALL_RECEIPT.json'])
    if revision == 4:
        prior = ROOT / 'artifacts/local/p030-tickdiag-v3'
        prior_names = ['amp-signed.itb', 'stage-B.cmd', 'stage-B.scr', 'TICKDIAG.json', 'SHA256SUMS']
        for name in prior_names:
            pre['existing_amp_p029_files']['tickdiag-v3/' + name] = ident((prior / 'packet' / name).read_bytes())
        prior_output = (prior / 'install-output.txt').read_text()
        prior_receipt = json.loads(next(line for line in prior_output.splitlines() if line.startswith('{')))
        receipt_sha = prior_receipt.pop('receipt_sha256')
        prior_receipt.pop('ram_source_removed')
        raw_receipt = (json.dumps(prior_receipt, sort_keys=True, indent=2) + '\n').encode()
        require(sha(raw_receipt) == receipt_sha, 'v3 exact installed receipt')
        pre['existing_amp_p029_files']['tickdiag-v3/INSTALL_RECEIPT.json'] = ident(raw_receipt)
        pre['existing_amp_p029_top_level'] = sorted(pre['existing_amp_p029_top_level'] + ['tickdiag-v3'])
        pre['existing_nested_members']['tickdiag-v3'] = sorted(prior_names + ['INSTALL_RECEIPT.json'])
    manifest = (json.dumps(spec, sort_keys=True, indent=2) + '\n').encode()
    helper = (ROOT / 'scripts/board/p030_install_initdiag_v1.py').read_bytes()
    require(sha(helper) == BASE_HELPER_SHA, 'reviewed installer base')
    installer = helper.decode().split('def resume_receipt(', 1)[0]
    installer = installer.replace('P030_INITDIAG.json', 'TICKDIAG.json').replace('initdiag-v1', f'tickdiag-v{revision}')
    installer = installer.replace('P030_M0_INITDIAG_V1', package_id)
    installer, n = re.subn(r'PINNED_MANIFEST_SHA = "[0-9a-f]{64}"', 'PINNED_MANIFEST_SHA = "' + sha(manifest) + '"', installer)
    require(n == 1, 'manifest pin')
    installer = installer.replace('P030_INITDIAG_PASSIVE_STAGE_INSTALLED_READBACK_PASS', f'P030_TICKDIAG_V{revision}_PASSIVE_STAGE_READBACK_PASS')
    installer += f'''
if __name__ == "__main__":
    if len(sys.argv) != 5 or sys.argv[1] != "--approved-p030-tickdiag-v{revision}" or sys.argv[3] != "--installer-sha":
        raise SystemExit("STOP explicit tickdiag-v{revision} manifest/installer hashes required")
    install(sys.argv[2], sys.argv[4])
'''
    compile(installer, 'install-p030.py', 'exec')
    packet = out / 'packet'
    packet.mkdir()
    payloads['TICKDIAG.json'] = manifest
    sums = ''.join(f'{sha(b)}  {name}\n' for name, b in sorted(payloads.items())).encode()
    payloads['SHA256SUMS'] = sums
    payloads['install-p030.py'] = installer.encode()
    for name, b in payloads.items():
        (packet / name).write_bytes(b)
    with tarfile.open(out / f'tickdiag-v{revision}.tar.gz', 'w:gz') as tar:
        for path in sorted(packet.iterdir()):
            tar.add(path, arcname=path.name)
    report = {'state': 'COMPILE_SIGN_PACKAGE_PASS_BOARD_UNVERIFIED', 'destination': destination,
              'files': spec['files'], 'manifest_sha256': sha(manifest), 'installer_sha256': sha(installer.encode()),
              'archive': ident((out / f'tickdiag-v{revision}.tar.gz').read_bytes()),
              'ELF': ident((out / 'rtthread.elf').read_bytes()), 'BIN': ident(raw),
              'signature_control_sha256': CONTROL_SHA, 'tests_run': False,
              'diagnostic_max_line_bytes': largest_line,
              'scope': 'M0 local diagnostic plus pre-delay fail-stop, no new clock writes/fallback; runtime hardware unverified'}
    (out / 'build-result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
