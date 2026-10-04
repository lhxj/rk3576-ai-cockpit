#!/usr/bin/env python3
"""Host-only U-Boot FIT using verified recovery firmware; no signing or flashing."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess

from validate_host_proposal import fdt

ORIGINAL_SHA = '06f48ed3716ccdde6adcb3187ca3eff766927e27bf6b7f432046539b782c4a56'
SLOT = 0x200000
START = 0x1200
NAMES = ('uboot', 'atf-1', 'atf-2', 'atf-3', 'optee', 'fdt')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def cell(value):
    if len(value) != 4:
        raise ValueError('expected one 32-bit cell')
    return struct.unpack('>I', value)[0]


def property_line(name, value):
    if not re.fullmatch(r'[A-Za-z0-9_#,+.-]+', name):
        raise ValueError('invalid property name')
    return f'{name} = [' + ' '.join(f'{byte:02x}' for byte in value) + '];'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--original', type=Path, required=True)
    ap.add_argument('--build', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    original = args.original.read_bytes()
    if sha(original) != ORIGINAL_SHA or len(original) != 2*SLOT:
        raise ValueError('recovery package identity mismatch')
    if original[:SLOT] != original[SLOT:]:
        raise ValueError('original FIT copies differ')
    nodes = fdt(original)
    payloads, original_payloads = {}, {}
    for name in NAMES:
        node = nodes['/images/' + name]
        start, size = cell(node['data-position']), cell(node['data-size'])
        if not (START <= start < start + size <= SLOT):
            raise ValueError('original payload outside first FIT slot')
        data = original[start:start+size]
        if bytes.fromhex(sha(data)) != nodes['/images/' + name + '/hash']['value']:
            raise ValueError('original payload hash mismatch')
        original_payloads[name] = data
    payloads.update(original_payloads)
    # FIT image is nodtb code; the FDT is a separate retained metadata node.
    payloads['uboot'] = (args.build / 'u-boot-nodtb.bin').read_bytes()
    payloads['fdt'] = (args.build / 'u-boot.dtb').read_bytes()
    if not payloads['uboot']:
        raise ValueError('empty U-Boot code')
    if payloads['fdt'] != original_payloads['fdt']:
        raise ValueError('board U-Boot DT changed; review before packaging')
    config = (args.build / '.config').read_text()
    for line in ('CONFIG_AMP=y', 'CONFIG_ROCKCHIP_AMP=y', 'CONFIG_FIT_SIGNATURE=y'):
        if line not in config.splitlines():
            raise ValueError('missing candidate configuration: ' + line)
    total = START + sum((len(data) + 511) & ~511 for data in payloads.values())
    if total > SLOT:
        raise ValueError('candidate FIT cannot fit the original 2 MiB slot')
    args.output.mkdir(parents=True, exist_ok=False)
    for name, data in payloads.items():
        (args.output / (name + '.bin')).write_bytes(data)
    lines = ['/dts-v1/;', '/ {']
    for name, value in nodes['/'].items():
        if name == 'timestamp':
            continue
        if name == 'totalsize':
            value = struct.pack('>I', total)
        lines.append('    ' + property_line(name, value))
    lines.append('    images {')
    for name in NAMES:
        lines.append(f'        {name} {{')
        lines.append(f'            data = /incbin/("{name}.bin");')
        for key, value in nodes['/images/' + name].items():
            if key not in ('data', 'data-size', 'data-position', 'data-offset'):
                lines.append('            ' + property_line(key, value))
        lines.append('            hash { algo = "sha256"; };')
        lines.append('        };')
    lines.append('    };')
    lines.append('    configurations {')
    for key, value in nodes['/configurations'].items():
        lines.append('        ' + property_line(key, value))
    lines.append('        conf {')
    for key, value in nodes['/configurations/conf'].items():
        lines.append('            ' + property_line(key, value))
    lines.append('            signature {')
    for key, value in nodes['/configurations/conf/signature'].items():
        if key == 'value':
            raise ValueError('original signed FIT requires legitimate signing workflow')
        lines.append('                ' + property_line(key, value))
    lines.extend(['            };', '        };', '    };', '};', ''])
    (args.output / 'loader-host.its').write_text('\n'.join(lines))
    cmd = ['mkimage', '-E', '-p', hex(START), '-B', '0x200',
           '-f', 'loader-host.its', 'loader-host.itb']
    env = dict(os.environ, SOURCE_DATE_EPOCH=str(cell(nodes['/']['timestamp'])))
    result = subprocess.run(cmd, cwd=args.output, env=env,
                            capture_output=True, text=True, timeout=90)
    (args.output / 'build.log').write_text(result.stdout + '\nSTDERR\n' + result.stderr)
    result.check_returncode()
    built = (args.output / 'loader-host.itb').read_bytes()
    new_nodes = fdt(built)
    if len(built) != total:
        raise ValueError(f'FIT length mismatch: {len(built)} != {total}')
    checks = []
    for name in NAMES:
        node = new_nodes['/images/' + name]
        start, size = cell(node['data-position']), cell(node['data-size'])
        if built[start:start+size] != payloads[name]:
            raise ValueError('packaged payload differs: ' + name)
        if bytes.fromhex(sha(payloads[name])) != new_nodes['/images/' + name + '/hash']['value']:
            raise ValueError('packaged hash differs: ' + name)
        for key, value in nodes['/images/' + name].items():
            if key not in ('data', 'data-size', 'data-position', 'data-offset') and node[key] != value:
                raise ValueError('firmware metadata changed: ' + name + '/' + key)
        checks.append({'name': name, 'sha256': sha(payloads[name]),
                       'unchanged_from_recovery': payloads[name] == original_payloads[name]})
    for path in ('/configurations', '/configurations/conf', '/configurations/conf/signature'):
        if new_nodes[path] != nodes[path]:
            raise ValueError('configuration metadata changed: ' + path)
    padded = built + bytes(SLOT-len(built))
    package = padded + padded
    (args.output / 'uboot-host-unsigned.img').write_bytes(package)
    report = {'status': 'HOST_PACKAGED_NOT_DEPLOYABLE', 'signed': False,
              'board_access': False, 'original_sha256': ORIGINAL_SHA,
              'command': cmd, 'exit_code': result.returncode,
              'fit_size': len(built), 'slot_size': SLOT, 'copies': 2,
              'package_sha256': sha(package), 'package_size': len(package),
              'preserved_payloads': checks,
              'missing_gate': 'actual boot policy, authorized key/provisioning if required, AMP GPT source'}
    (args.output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
