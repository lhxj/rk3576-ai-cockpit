#!/usr/bin/env python3
"""Record already completed P026 Host tests and identities, without board access."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
REVIEW = ROOT / 'docs/reviews/rk3576-amp-platform-closure'


def main():
    preload = json.loads((ROOT / 'artifacts/local/p026-primary-reviewed-preload/result.json').read_text())
    policy = json.loads((ROOT / 'artifacts/local/p026-primary-reviewed-policy/result.json').read_text())
    paired = json.loads((ROOT / 'artifacts/local/p026-kernel-isolated-clean/result.json').read_text())
    initrd = json.loads((ROOT / 'artifacts/local/p026-kernel-isolated-clean/initrd-result.json').read_text())
    packet = json.loads((ROOT / 'artifacts/local/p026-packet-check.json').read_text())
    build = ROOT / 'artifacts/local/p026-preload-clean-final-v6'
    source = ROOT / 'artifacts/local/p026-uboot-preload'
    source_sha = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    assert source_sha == '2314a3f9f5795b88c7c53a805e9d59d82c6715b3'
    assert not subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True).strip()
    for result in preload['tests'] + policy['variants']:
        assert result['compiler_exit'] == result['test_exit'] == 0
    assert paired['status'] == 'HOST_PAIRED_BUILD_PASS'
    assert paired['build_warning_count'] == paired['echo_warning_count'] == 0
    assert initrd['status'] == 'HOST_PAIRED_INITRD_PASS'
    assert packet['host_packet_integrity'] == 'PASS'
    assert packet['board_test_readiness'] == packet['deployment_gate'] == 'BLOCKED'
    strict = subprocess.run([sys.executable, str(ROOT / 'scripts/amp/check_deployment_packet.py')],
                            cwd=ROOT, capture_output=True, text=True)
    assert strict.returncode == 2 and not strict.stderr
    assert json.loads(strict.stdout) == packet
    log = (build / 'build.log').read_text()
    warning_lines = [line.strip() for line in log.splitlines() if 'warning:' in line]
    assert len(warning_lines) == 23
    assert sum('deprecated' in line for line in warning_lines) == 22
    assert all('deprecated' in line or 'bmp2gray16.c' in line for line in warning_lines)
    result = {'milestone': 'P026', 'date': '2026-10-03', 'evidence_level': 'HOST_TESTED',
              'amp_grade': 'C. HOST_BUILD_PASS', 'deployment_authorized': False,
              'host_validation_board_access': False, 'uboot_source_commit': source_sha,
              'uboot_fresh_build': 'PASS', 'uboot_target_warnings': 0,
              'uboot_host_tool_warnings': {'openssl3_deprecated': 22, 'bmp2gray16_format': 1},
              'primary_reviewed_actual_c_tests': preload,
              'primary_reviewed_fit_policy_tests': policy,
              'paired_kernel': {k: v for k, v in paired.items() if k != 'modules'},
              'paired_initrd': {k: v for k, v in initrd.items() if k != 'changes'},
              'packet': packet,
              'strict_gate_exit_code': strict.returncode,
              'manifest_sha256': hashlib.sha256((REVIEW / 'P026_ARTIFACT_MANIFEST.json').read_bytes()).hexdigest(),
              'scope': 'Actual Host C functions/libfdt; firmware calls fault-stubbed. No M0/reset/SMC/cache runtime claim.'}
    (REVIEW / 'P026_HOST_VALIDATION.json').write_text(json.dumps(result, indent=2) + '\n')
    print('P026 actual Host tests/build identities recorded; board runtime gates remain BLOCKED')


if __name__ == '__main__':
    main()
