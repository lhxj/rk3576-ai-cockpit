#!/usr/bin/env python3
"""Compile actual draft C functions with fault-injected Host stubs; no MMIO.

The numeric fixtures are synthetic and are not proposed board addresses.
Run with --source pointing to the derived, patched U-Boot checkout.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def function(text, name):
    match = re.search(r"(?:^|\n)(?:__weak\s+)?(?:static\s+)?int\s+" +
                      re.escape(name) + r"\s*\([^;{}]*\)\s*\{", text)
    if not match:
        raise ValueError(f"function not found: {name}")
    start = match.start()
    # These selected vendor functions have no braces inside strings/comments.
    pos = text.index("{", match.start())
    depth = 1
    end = pos + 1
    while depth:
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
        end += 1
    return text[start:end].lstrip()


PREFIX = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint8_t u8;
typedef uint32_t fdt32_t;
typedef uintptr_t phys_addr_t;
typedef struct { unsigned long arg0, arg1, arg2, arg3; } boot_args_t;
typedef struct { u32 arch, state, entry, linux_os, boot_on; } boot_cpu_t;
#define AMP_E(...) printf(__VA_ARGS__)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define PE_STATE(a,b,c,d) ((a) | ((b)<<1) | ((c)<<2) | ((d)<<3))
#define IH_TYPE_STANDALONE 1
#define IH_ARCH_ARM 2
#define ROCKCHIP_SIP_CONFIG_BUSMCU_0_ID 0
#define ROCKCHIP_SIP_CONFIG_MCU_CODE_START_ADDR 1
#define ROCKCHIP_SIP_CONFIG_MCU_SRAM_START_ADDR 3
/* Fake Host device identifiers, not RK3576 register addresses. */
#define TOP_CRU_BASE 0x1000
#define TOP_CRU_GATE_CON19 0x10
#define TOP_CRU_SOFTRST_CON19 0x20
static u32 os_amp_dispatcher_cpu[8];
static struct {
    bool present, desc, load_present, allocation;
    int len, code_ret, shared_ret, size, size_ret;
    u32 base, load;
    int trace[8], count, alloc_calls, delays;
    u32 code_arg, shared_arg;
} f;
static void event(int n) { if (f.count >= 8) abort(); f.trace[f.count++] = n; }
static u32 fdt32_to_cpu(fdt32_t v) { return __builtin_bswap32(v); }
static const void *fdt_getprop(const void *fit, int off, const char *key, int *len)
{
    static fdt32_t cell;
    (void)fit; (void)off;
    if (!strcmp(key, "description")) return f.desc ? "bus_mcu" : NULL;
    if (strcmp(key, "rockchip,mcu-shared-window-base")) abort();
    if (len) *len = f.len;
    cell = __builtin_bswap32(f.base);
    return f.present ? &cell : NULL;
}
static int sip_smc_mcu_config(int id, int selector, uintptr_t value)
{
    if (id != 0) abort();
    event(selector);
    if (selector == 1) { f.code_arg = value; return f.code_ret; }
    if (selector == 3) { f.shared_arg = value; return f.shared_ret; }
    abort();
}
static void writel(u32 value, uintptr_t addr)
{
    if (addr == TOP_CRU_BASE + TOP_CRU_GATE_CON19 && value == 0x5c000000)
        event(10);
    else if (addr == TOP_CRU_BASE + TOP_CRU_SOFTRST_CON19 && value == 0x38000000)
        event(20);
    else abort();
}
static void *sysmem_alloc_base_by_name(const char *id, phys_addr_t p, int size)
{
    if (strcmp(id, "bus_mcu") || p != f.load || size != f.size) abort();
    f.alloc_calls++;
    return f.allocation ? &f : NULL;
}
static u32 fit_get_u32_default(const void *fit, int off, const char *key, u32 def)
{
    (void)fit; (void)off;
    if (!strcmp(key, "load")) return f.load_present ? f.load : def;
    return def;
}
static int fit_image_get_arch(const void *fit, int off, u8 *v)
{ (void)fit; (void)off; *v = IH_ARCH_ARM; return 0; }
static int fit_image_get_type(const void *fit, int off, u8 *v)
{ (void)fit; (void)off; *v = IH_TYPE_STANDALONE; return 0; }
static int fit_image_get_data_size(const void *fit, int off, int *v)
{ (void)fit; (void)off; if (!f.size_ret) *v = f.size; return f.size_ret; }
/* Non-standalone paths must never run in this integration fixture. */
static int env_set_hex(const char *a, u32 b) { (void)a; (void)b; abort(); }
static u32 read_mpidr(void) { abort(); }
static int load_linux_for_nonboot_cpu(u32 a,u32 b,u32 c,u32 *d,boot_args_t *e)
{ (void)a; (void)b; (void)c; (void)d; (void)e; abort(); }
static void setup_sync_bits_for_linux(void) { abort(); }
static int smc_cpu_on(u32 a,u32 b,u32 c,boot_args_t *d,bool e)
{ (void)a; (void)b; (void)c; (void)d; (void)e; abort(); }
static void udelay(u32 v) { (void)v; f.delays++; }
static void reset_fixture(void)
{
    memset(&f, 0, sizeof(f));
    f.present = f.desc = f.load_present = f.allocation = true;
    f.len = 4; f.base = 0x1000; f.load = 0x01000000; f.size = 1024;
}
static int tests;
static void check(bool ok, const char *name)
{ tests++; if (!ok) { fprintf(stderr, "FAIL: %s\n", name); exit(1); } }
static int parent(void);
'''

SUFFIX = r'''
static int parent(void) { boot_cpu_t cpu = {0}; return brought_up_amp(&f, 1, &cpu, 0); }
static int release(uintptr_t entry)
{ return fit_standalone_release_with_config(&f, 1, "bus_mcu", entry); }
int main(void)
{
    reset_fixture(); f.present = false;
    check(parent() == -EINVAL && !f.count, "missing property blocks release through parent");
    const int bad_lengths[] = {-1, 0, 1, 3, 8};
    for (unsigned int i = 0; i < ARRAY_SIZE(bad_lengths); i++) {
        reset_fixture(); f.len = bad_lengths[i];
        check(parent() == -EINVAL && !f.count, "one-cell property required");
    }
    reset_fixture(); f.base = 0;
    check(parent() == -EINVAL && !f.count, "zero base blocked");
    reset_fixture(); f.base = 0x1001;
    check(parent() == -EINVAL && !f.count, "unaligned base blocked");
    const uintptr_t bad_entries[] = {0, 0x00800000, 0x01000001, UINT64_C(0x100000000)};
    for (unsigned int i = 0; i < ARRAY_SIZE(bad_entries); i++) {
        reset_fixture();
        check(release(bad_entries[i]) == -EINVAL && !f.count, "bad code mapping blocked");
    }
    reset_fixture(); f.code_ret = -4;
    check(parent() == -4 && f.count == 1 && f.trace[0] == 1, "CODE failure propagated; no shared/gate/reset");
    reset_fixture(); f.shared_ret = -3;
    check(parent() == -3 && f.count == 2 && f.trace[1] == 3, "shared failure propagated; no gate/reset");
    reset_fixture(); f.shared_ret = 1;
    check(parent() == 1 && f.count == 2, "positive unexpected SMC result also blocks release");
    reset_fixture(); f.desc = false;
    check(parent() == -EINVAL && !f.count && !f.alloc_calls, "missing description rejected");
    reset_fixture(); f.load_present = false;
    check(parent() == -EINVAL && !f.count && !f.alloc_calls, "missing load rejected");
    reset_fixture(); f.size = 0;
    check(parent() == -EINVAL && !f.count && !f.alloc_calls, "zero image size rejected");
    reset_fixture(); f.size = -1;
    check(parent() == -EINVAL && !f.count, "negative image size rejected");
    reset_fixture(); f.size_ret = -1;
    check(parent() == -EINVAL && !f.count, "failed data-size lookup cannot use uninitialized size");
    reset_fixture(); f.allocation = false;
    check(parent() == -ENXIO && !f.count, "allocation failure propagated");
    reset_fixture();
    check(fit_standalone_release("bus_mcu", f.load) == -EINVAL && !f.count,
          "legacy BUS M0 hook cannot bypass required FIT configuration");
    reset_fixture();
    check(parent() == 0 && f.count == 4 && f.trace[0] == 1 && f.trace[1] == 3 &&
          f.trace[2] == 10 && f.trace[3] == 20 && f.code_arg == f.load && f.shared_arg == f.base,
          "success orders CODE/shared/gate/reset; exact big-endian property decoded");
    printf("HOST_MOCK_PASS: %d checks; no firmware or hardware executed\n", tests);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--cold-proposal", action="store_true",
                        help="Check P023 reset/clock/barrier order and preserve Linux boot")
    args = parser.parse_args()
    soc_path = args.source / "arch/arm/mach-rockchip/rk3576/rk3576.c"
    amp_path = args.source / "drivers/cpu/rockchip_amp.c"
    soc, amp = soc_path.read_text(), amp_path.read_text()
    names = ["fit_standalone_release", "fit_standalone_release_with_config",
             "standalone_handler", "brought_up_amp"]
    extracted = [function(soc, n) for n in names[:2]] + [function(amp, n) for n in names[2:]]
    prefix, suffix = PREFIX, SUFFIX
    if args.cold_proposal:
        prefix = prefix.replace('trace[8]', 'trace[64]').replace('f.count >= 8', 'f.count >= 64')
        prefix = prefix.replace('static u32 os_amp_dispatcher_cpu[8];', '''
typedef struct { struct { u32 arch; } os; } bootm_headers_t;
static boot_cpu_t g_bootcpu;
static void dsb(void) { event(15); }
static u32 os_amp_dispatcher_cpu[8];'''.replace('static void dsb(void) { event(15); }', ''))
        prefix = prefix.replace('static u32 fdt32_to_cpu', 'static void dsb(void) { event(15); }\nstatic u32 fdt32_to_cpu')
        prefix = prefix.replace('else abort();\n}\nstatic void *sysmem', '''
    else if (addr == TOP_CRU_BASE + TOP_CRU_SOFTRST_CON19 && value == 0x38003800) event(11);
    else if (addr == TOP_CRU_BASE + 0x0834 && value == 0x40000000) event(40);
    else if (addr == TOP_CRU_BASE + 0x0838 && value == 0x80000000) event(41);
    else if (addr == TOP_CRU_BASE + 0x0844 && value == 0x20000000) event(42);
    else abort();
}
static void *sysmem''')
        prefix = prefix.replace('(void)v; f.delays++;', 'if (v != 10) abort(); event(16); f.delays++;')
        suffix = suffix.replace('f.count == 1 && f.trace[0] == 1', 'f.count == 8 && f.trace[4] == 11 && f.trace[7] == 1')
        suffix = suffix.replace('f.count == 2 && f.trace[1] == 3', 'f.count == 9 && f.trace[4] == 11 && f.trace[8] == 3')
        suffix = suffix.replace('f.count == 2,', 'f.count == 9,')
        suffix = suffix.replace('CODE failure propagated; no shared/gate/reset', 'CODE failure propagated; reset held, no shared/release')
        suffix = suffix.replace('shared failure propagated; no gate/reset', 'shared failure propagated; reset held, no release')
        suffix = suffix.replace('f.count == 4 && f.trace[0] == 1 && f.trace[1] == 3 &&\n          f.trace[2] == 10 && f.trace[3] == 20', '''f.count == 11 &&
          !memcmp(f.trace, (int[]){40,41,42,10,11,15,16,1,3,15,20}, 11*sizeof(int))''')
        suffix = suffix.replace('success orders CODE/shared/gate/reset', 'success orders clock/assert/barrier/CODE/shared/barrier/release')
        suffix = suffix.replace('    printf("HOST_MOCK_PASS:', '''    bootm_headers_t images = {.os.arch = 99};
    g_bootcpu = (boot_cpu_t){0};
    check(arm64_switch_amp_pe(&images) == 0 && images.os.arch == 99, "M0-only/missing AMP preserves Linux arch");
    g_bootcpu.entry = 1;
    check(arm64_switch_amp_pe(&images) == 0 && images.os.arch == 99, "non-Linux AMP preserves Linux arch");
    g_bootcpu.linux_os = 1; g_bootcpu.arch = 3; g_bootcpu.state = 2;
    check(arm64_switch_amp_pe(&images) == 2 && images.os.arch == 3, "existing Linux AMP path preserved");
    printf("HOST_MOCK_PASS:''')
        extracted.append(function(amp, 'arm64_switch_amp_pe'))
        names.append('arm64_switch_amp_pe')
    report = {"scope": "Host mock only; synthetic addresses; not deployable",
              "source_functions": names,
              "source_sha256": {str(p.relative_to(args.source)): hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in (soc_path, amp_path)}}
    with tempfile.TemporaryDirectory(prefix="amp-startup-host-") as temp:
        directory = Path(temp)
        harness, binary = directory / "test.c", directory / "test"
        harness.write_text(prefix + "\n".join(extracted) + suffix)
        # These exclusions apply to existing generic vendor code's signed
        # sentinels/loop, u8 non-standalone sentinels and unused legacy
        # parameters, not callback mismatches. The u8 sentinel comparisons
        # are an upstream warning outside this standalone-only fixture.
        command = ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                   "-Wno-unused-parameter", "-Wno-sign-compare", "-Wno-type-limits",
                   str(harness), "-o", str(binary)]
        build = subprocess.run(command, capture_output=True, text=True, timeout=30)
        report["compile"] = {"command": command, "exit_code": build.returncode,
                             "stdout": build.stdout, "stderr": build.stderr}
        if build.returncode:
            print(build.stderr)
            if args.report:
                args.report.parent.mkdir(parents=True, exist_ok=True)
                args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
            raise SystemExit(build.returncode)
        run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
        report["run"] = {"exit_code": run.returncode, "stdout": run.stdout, "stderr": run.stderr}
        print(run.stdout, end="")
        print(run.stderr, end="")
        if not run.returncode:
            weak = function(amp, "fit_standalone_release_with_config")
            harness.write_text(r'''
#include <stdint.h>
#include <string.h>
#define __weak __attribute__((weak))
static int calls;
int fit_standalone_release(char *id, uintptr_t entry)
{ calls++; return !strcmp(id, "other_soc") && entry == 0x1000 ? -7 : -99; }
''' + weak + r'''
int main(void)
{ return fit_standalone_release_with_config(0, 1, "other_soc", 0x1000) == -7 && calls == 1 ? 0 : 1; }
''')
            compat_build = subprocess.run(command, capture_output=True, text=True, timeout=30)
            report["weak_compat_compile"] = {"exit_code": compat_build.returncode,
                                             "stderr": compat_build.stderr}
            if compat_build.returncode:
                print(compat_build.stderr)
                raise SystemExit(compat_build.returncode)
            compat = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
            report["weak_compat_run"] = {"exit_code": compat.returncode,
                                         "purpose": "existing other-SoC hook retains argument/result forwarding"}
            if compat.returncode:
                raise SystemExit(compat.returncode)
            print("HOST_WEAK_HOOK_COMPAT_PASS: 1 check")
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
        raise SystemExit(run.returncode)


if __name__ == "__main__":
    main()
