#!/usr/bin/env python3
"""Verify Host packet identity; a BLOCKED packet never grants deployment."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REVIEW = ROOT / 'docs/reviews/rk3576-amp-platform-closure'


def check_packet(manifest, packet, root=ROOT):
    errors = []
    for item in [manifest['contract'], *manifest['artifacts'], *manifest['evidence'],
                 *manifest['baseline']['evidence_files']]:
        path = (root / item['file']).resolve()
        if not path.is_relative_to(root.resolve()):
            errors.append('path outside project: ' + item['file'])
            continue
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
            errors.append('hash mismatch or missing: ' + item['file'])
    contract = json.loads((root / manifest['contract']['file']).read_text())
    if packet['contract'] != manifest['contract'] or packet['proposal'] != contract['host_proposal']:
        errors.append('contract/proposal changed')
    by_file = {a['file']: a['sha256'] for a in manifest['artifacts']}
    for change in packet['changes']:
        if 'source' in change and by_file.get(change['source']) != change['new_sha256']:
            errors.append('changeset artifact hash mismatch: ' + change['source'])
        if change['kind'] == 'symlink':
            for state in ('old', 'new'):
                link = change[state]
                if link.get('type') != 'symlink' or hashlib.sha256(link['target'].encode()).hexdigest() != link['sha256_link_text']:
                    errors.append('symlink target identity mismatch')
    # Independently derive PA views from the canonical proposal, not copied constants.
    p = contract['host_proposal']
    code, size, base, shared = (int(p[n], 0) for n in
                              ('code_linux_pa', 'code_size', 'shared_window_linux_pa', 'shared_m0_address'))
    ring_size = int(contract['rpmsg']['vring0']['size'], 0)
    ring0 = base + shared - 0x20000000
    pool = ring0 + 2 * ring_size
    pool_size = 2 * contract['rpmsg']['buffer_count'] * (contract['rpmsg']['buffer_size'] + p['buffer_header_size'])
    if not (code + size <= ring0 or pool + pool_size <= code):
        errors.append('physical code/shared overlap')
    local_entry = int(p['m0_entry'], 0)
    if not (local_entry & 1 and 0 <= (local_entry & ~1) < size):
        errors.append('invalid M0 local Thumb entry')
    blocked = [name for name, status in packet['gates'].items() if status != 'PASS']
    if contract['status'] != 'READY_FOR_CONTROLLED_BOARD_TEST':
        blocked.append('canonical_contract_not_ready')
    for group, field in [('rtos', 'load_address'), ('rtos', 'entry_address'),
                         ('shared_memory', 'linux_pa'), ('coherency', 'selected_scheme'),
                         ('boot', 'load_source'), ('boot', 'fit_verification_policy_on_board')]:
        if contract[group].get(field) is None:
            blocked.append('final_contract_missing_' + group + '.' + field)
    for field in ('uboot_amp_enabled_on_board', 'bl31_mcu_smc_verified_on_board'):
        if contract['boot'].get(field) is not True:
            blocked.append('board_capability_unproved_' + field)
    if packet.get('status') != 'READY_FOR_CONTROLLED_BOARD_TEST':
        blocked.append('changeset_not_final')
    if packet.get('deployment_authorized') is not True:
        blocked.append('explicit_board_deployment_approval_missing')
    if any(c['kind'].endswith('partition') and c.get('destination') is None for c in packet['changes']):
        blocked.append('partition_destination_unresolved')
    # D means preparation is complete and board-test approval can be requested.
    # It does not itself authorize writing the board. Keep both gates explicit.
    readiness_blockers = [b for b in blocked if b != 'explicit_board_deployment_approval_missing']
    return {'host_packet_integrity': 'FAIL' if errors else 'PASS', 'errors': errors,
            'board_test_readiness': 'BLOCKED' if errors or readiness_blockers else 'READY',
            'readiness_blockers': readiness_blockers,
            'deployment_gate': 'BLOCKED' if errors or blocked else 'OPEN', 'blockers': blocked,
            'proposal_only': {'code_pa': hex(code), 'entry_m0': hex(local_entry),
                              'vring0_pa': hex(ring0), 'vring1_pa': hex(ring0 + ring_size),
                              'pool_pa': hex(pool), 'pool_size': hex(pool_size)}}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--allow-blocked-for-host', action='store_true',
                    help='return 0 for artifact integrity only; never a deployment approval')
    ap.add_argument('--report', type=Path)
    ap.add_argument('--manifest', type=Path, default=REVIEW / 'AMP_ARTIFACT_MANIFEST.json')
    ap.add_argument('--packet', type=Path, default=REVIEW / 'P026_DEPLOYMENT_CHANGESET.json')
    args = ap.parse_args()
    report = check_packet(json.loads(args.manifest.read_text()), json.loads(args.packet.read_text()))
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 1 if report['errors'] else (0 if args.allow_blocked_for_host or report['deployment_gate'] == 'OPEN' else 2)


if __name__ == '__main__':
    raise SystemExit(main())
