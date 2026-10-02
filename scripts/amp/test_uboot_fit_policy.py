#!/usr/bin/env python3
"""Exercise actual derived U-Boot policy/bounds functions on Host, never firmware."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

from test_uboot_mcu_startup_draft import function

PREFIX = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
typedef uint64_t u64;
typedef unsigned long ulong;
typedef uint64_t lbaint_t;
#define FIT_HEADER_SIZE 4096
#define DIV_ROUND_UP(n,d) (((n)+(d)-1)/(d))
#define CONFIG_FIT_SIGNATURE TEST_SIGNATURE
#define IMAGE_ENABLE_VERIFY TEST_SIGNATURE
#define IS_ENABLED(x) (x)
#define AMP_E(...) printf(__VA_ARGS__)
static int required, node, verification, calls, cases, sig_node=1, key=1;
#define FIT_SIG_NODENAME "signature"
#define gd_fdt_blob() NULL
#define fdt_for_each_subnode(n,b,p) for ((n)=0; (n)<1; (n)++)
static int fdt_subnode_offset(const void *blob, int parent, const char *name)
{ (void)blob; (void)parent; (void)name; return sig_node; }
static const char *fdt_getprop(const void *blob,int offset,const char *name,int *len)
{ (void)blob; (void)offset; (void)name; *len=5; return key ? "conf" : NULL; }
static int fit_board_verify_required_sigs(void) { return required; }
static int fit_conf_get_node(const void *fit, const char *name)
{ (void)fit; (void)name; return node; }
static int fit_config_verify(const void *fit, int offset)
{ (void)fit; (void)offset; calls++; return verification; }
static void check(int result, const char *name)
{ cases++; if (!result) { fprintf(stderr,"FAIL %s\n",name); exit(1); } }
'''
SUFFIX = r'''
int main(void)
{
    required=0; node=1; verification=0; calls=0;
    check(amp_verify_config_policy(NULL,"conf")==0,"normal policy");
    check(calls==TEST_SIGNATURE,"config verified when supported");
    required=1; calls=0;
    check(amp_verify_config_policy(NULL,"conf")==
          (TEST_SIGNATURE ? 0 : -EACCES),"required board signatures enforced");
    check(calls==TEST_SIGNATURE,"required policy cannot fall through unsupported verify");
    key=0; calls=0;
    check(amp_verify_config_policy(NULL,"conf")==-EACCES,
          "signature feature without required conf key fails closed");
    check(calls==0,"empty required key list never grants policy");
    sig_node=-ENOENT;
    check(amp_verify_config_policy(NULL,"conf")==-EACCES,
          "missing signature node cannot grant policy");
    required=0;
    check(amp_verify_config_policy(NULL,"conf")==0,
          "hash-only board without keys retains image checks");
    check(calls==0,"no optional key verification without keys");
    sig_node=1; key=1;
    required=0; node=-ENOENT; calls=0;
    check(amp_verify_config_policy(NULL,"conf")==-ENOENT,"missing config rejected");
    check(calls==0,"missing node never verified");
    node=1; verification=-EINVAL;
    check(amp_verify_config_policy(NULL,"conf")==
          (TEST_SIGNATURE ? -EACCES : 0),"bad signature failure propagated");
    verification=1;
    check(amp_verify_config_policy(NULL,"conf")==
          (TEST_SIGNATURE ? -EACCES : 0),"positive verify failure propagated");
    check(amp_fit_partition_bounds(4096,512,8)==0,"exact header fits");
    check(amp_fit_partition_bounds(4097,512,9)==0,"rounded read fits");
    check(amp_fit_partition_bounds(4097,512,8)==-EFBIG,"rounded read exceeds partition");
    check(amp_fit_partition_bounds(4096,512,7)==-EFBIG,"short partition rejected");
    check(amp_fit_partition_bounds(-1,512,100)==-EINVAL,"negative size rejected");
    check(amp_fit_partition_bounds(4095,512,100)==-EINVAL,"short FIT rejected");
    check(amp_fit_partition_bounds(4096,0,100)==-EINVAL,"zero block size rejected");
    check(amp_fit_partition_bounds(4096,8192,100)==-EINVAL,"block larger than header rejected");
    check(amp_fit_partition_bounds(4096,300,100)==-EINVAL,"header must cover whole blocks");
    check(amp_fit_partition_bounds(0x7fffffff,512,0xffffffffULL)==-EFBIG,
          "rounded signed-size overflow rejected");
    printf("HOST_POLICY_CASES=%d\n",cases);
    return 0;
}
'''


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    body = (args.source / 'drivers/cpu/rockchip_amp.c').read_text()
    policy = function(body, 'amp_verify_config_policy')
    bounds = function(body, 'amp_fit_partition_bounds')
    loader = function(body, 'amp_cpus_on')
    assert loader.index('amp_verify_config_policy(fit, "conf")') < loader.index('boot_get_loadable(')
    assert loader.index('amp_fit_partition_bounds(FIT_HEADER_SIZE') < loader.index('blk_dread(')
    assert loader.index('amp_fit_partition_bounds(totalsize') < loader.index('/* load image */')
    args.output.mkdir(parents=True, exist_ok=False)
    results = []
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'policy.c'
        src.write_text(PREFIX + bounds + '\n' + policy + SUFFIX)
        for signature in (0, 1):
            exe = Path(tmp) / f'policy-{signature}'
            cmd = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                   f'-DTEST_SIGNATURE={signature}', str(src), '-o', str(exe)]
            subprocess.run(cmd, check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
            results.append({'signature_support': bool(signature), 'compiler_exit': 0,
                            'test_exit': result.returncode, 'stdout': result.stdout})
    report = {'evidence': 'HOST_TESTED', 'board_access': False,
              'source_sha256': hashlib.sha256(body.encode()).hexdigest(),
              'integration_order_checks': 3, 'variants': results}
    (args.output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
