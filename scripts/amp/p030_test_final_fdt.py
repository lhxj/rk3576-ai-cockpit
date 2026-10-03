#!/usr/bin/env python3
"""Compile real vendor final FDT prep/guard with saved DTs and Host boundaries.

No SSH, MMIO, firmware execution or board writes. Production source is read-only.
Tests real vendor bank packing, arch/final prep, libfdt and both old/new guards;
hardware-specific board/bidram/bootargs/network/LMB hooks are explicit stubs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from p026_test_preload import extract


PREFIX = r'''
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libfdt.h>
typedef unsigned long long u64;
typedef uint32_t u32;
typedef uint8_t u8;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef uint64_t phys_addr_t;
typedef uint64_t phys_size_t;
#define MEMORY_BANKS_MAX CONFIG_NR_DRAM_BANKS
#define CONFIG_OF_LIBFDT 1
#define CONFIG_BIDRAM 1
#define CONFIG_ARCH_FIXUP_FDT_MEMORY 1
#define CONFIG_AMP 1
#define IMAGE_OF_BOARD_SETUP 0
#define IMAGE_OF_SYSTEM_SETUP 0
#define FDT_RAMDISK_OVERHEAD 0x80
#define ALIGN(n,a) (((n)+(a)-1)&~((a)-1))
typedef struct { struct { u64 start, size; } bi_dram[CONFIG_NR_DRAM_BANKS]; } bd_t;
static bd_t board;
static struct { bd_t *bd; } gd_data = { &board }, *gd = &gd_data;
typedef struct { ulong initrd_start,initrd_end; } bootm_headers_t;
struct lmb { int unused; };
static bool fixed_active, amp_active = true;
static int checks, phase, last_check, ft_verify_calls, mutation;
static int inject(void *blob,int fault);
static void check(bool condition,const char *name) {
    checks++; if (!condition) { fprintf(stderr,"FAIL %s\n",name);exit(1); }
}
static int board_fdt_fixup(void *blob) { (void)blob;phase=1;return 0; }
static void boot_mem_rsv_regions(struct lmb *lmb,void *blob) { (void)lmb;(void)blob; }
static int bidram_fixup(void) { return 0; }
static int fdt_chosen(void *blob) {
    check(phase==1,"chosen follows actual arch bank fixup");phase=2;
    return mutation ? inject(blob,mutation) : 0;
}
static void fdt_fixup_ethernet(void *blob) { (void)blob;check(phase==2,"ethernet follows chosen");phase=3; }
static int ft_board_setup(void *blob,bd_t *bd) { (void)blob;(void)bd;return 0; }
static int ft_system_setup(void *blob,bd_t *bd) { (void)blob;(void)bd;return 0; }
static int lmb_free(struct lmb *lmb,phys_addr_t base,phys_size_t size) { (void)lmb;(void)base;(void)size;return 0; }
static int lmb_reserve(struct lmb *lmb,phys_addr_t base,phys_size_t size) { (void)lmb;(void)base;(void)size;return 0; }
static bool amp_memory_guard_active(void) { return amp_active; }
static int ft_verify_fdt(void *blob) { (void)blob;check(phase==3,"final verify ordered after prep");ft_verify_calls++;return 1; }
'''

WRAPPER = r'''
static int amp_linux_fdt_check(const void *blob,bool memory) {
    int chosen=fdt_path_offset(blob,"/chosen"),len;
    check(phase==3,"AMP guard follows arch/chosen/ethernet/final prep");
    check(chosen>=0 && fdt_getprop(blob,chosen,"linux,initrd-start",&len)!=NULL &&
          len==(fdt_address_cells(blob,0)==2?8:4),
          "actual initrd fixup completed before final guard");
    check(fdt_num_mem_rsv(blob)>=2,"actual shrink/initrd memreserve operations completed");
    last_check=fixed_active ? amp_linux_fdt_check_fixed(blob,memory) : amp_linux_fdt_check_original(blob,memory);
    return last_check;
}
'''

SUFFIX = r'''
static void *load_dt(const char *path,size_t *size) {
    FILE *file=fopen(path,"rb");void *buffer;check(file!=NULL,"open saved DT");
    check(fseek(file,0,SEEK_END)==0,"seek saved DT");long bytes=ftell(file);rewind(file);
    check(bytes>0 && bytes<1048576,"bounded saved DT");buffer=malloc(bytes);check(buffer!=NULL,"read buffer");
    check(fread(buffer,1,bytes,file)==(size_t)bytes,"saved DT read");fclose(file);*size=bytes;return buffer;
}
static void *fresh(const void *original,size_t size) {
    void *blob=calloc(1,size+65536);check(blob!=NULL,"working DT buffer");
    check(fdt_open_into(original,blob,size+65536)==0,"open DT with normal prep slack");return blob;
}
static void banks(void) {
    memset(&board,0,sizeof(board));
    board.bi_dram[0].start=0x40200000;board.bi_dram[0].size=0x08200000;
    board.bi_dram[1].start=0x49400000;board.bi_dram[1].size=0xb6c00000;
    board.bi_dram[2].start=0x100000000ULL;board.bi_dram[2].size=0x40000000;
}
static int snapshot(void *blob,unsigned char *bytes) {
    int parent=fdt_path_offset(blob,"/reserved-memory"),node,total=0,len;
    check(parent>=0,"reserved parent exists");
    fdt_for_each_subnode(node,blob,parent) {
        const void *value=fdt_getprop(blob,node,"reg",&len);
        if (!value) continue;
        check(len>0 && total+len+1<4096,"bounded reserved snapshot");
        memcpy(bytes+total,value,len);total+=len;
        value=fdt_getprop(blob,node,"no-map",&len);bytes[total++]=value && len==0;
    }
    return total;
}
static int code_node(void *blob) {
    int parent=fdt_path_offset(blob,"/reserved-memory"),node;
    fdt_for_each_subnode(node,blob,parent) {
        int len;const fdt32_t *reg=fdt_getprop(blob,node,"reg",&len);
        if(reg && len==16 && fdt32_to_cpu(reg[1])==AMP_CODE_LINUX_PA)return node;
    }
    return -FDT_ERR_NOTFOUND;
}
static int inject(void *blob,int fault) {
    int memory=fdt_path_offset(blob,"/memory"),parent=fdt_path_offset(blob,"/reserved-memory"),code=code_node(blob),len;
    fdt32_t values[CONFIG_NR_DRAM_BANKS*4];const fdt32_t *reg=fdt_getprop(blob,memory,"reg",&len);
    check(reg && len==(int)sizeof(values),"vendor exports full configured memory slots");
    memcpy(values,reg,sizeof(values));
    switch(fault) {
    case 1: values[13]=cpu_to_fdt32(0x47000000);break; /* nonzero base, size0 */
    case 2: values[12]=cpu_to_fdt32(0xffffffff);values[13]=cpu_to_fdt32(0xfffff000);values[15]=cpu_to_fdt32(0x2000);break;
    case 3: memset(values,0,sizeof(values));break; /* coverage absent */
    case 4: return fdt_setprop_string(blob,memory,"status","disabled");
    case 5: return fdt_setprop(blob,memory,"reg",values,sizeof(values)-4);
    case 6: return fdt_setprop_u32(blob,0,"#address-cells",1);
    case 7: return fdt_setprop_u32(blob,0,"#size-cells",1);
    case 8: return fdt_setprop_string(blob,parent,"status","disabled");
    case 9: return fdt_delprop(blob,code,"no-map");
    case 10:return fdt_setprop(blob,code,"reusable",NULL,0);
    case 11:return fdt_setprop_string(blob,code,"status","disabled");
    case 12:return fdt_setprop_u32(blob,parent,"#address-cells",1);
    case 13:return fdt_setprop_u32(blob,parent,"ranges",0);
    case 14:return fdt_setprop_u32(blob,code,"no-map",0);
    case 15:case 16:case 17:case 18:case 19: {
        fdt32_t tuples[12]={0,cpu_to_fdt32(AMP_CODE_LINUX_PA),0,cpu_to_fdt32(AMP_CODE_SIZE),
                           0,cpu_to_fdt32(AMP_CODE_LINUX_PA+4),0,cpu_to_fdt32(16)};
        if(fault==16)memcpy(tuples+4,tuples,16); /* duplicate second tuple */
        if(fault==17){tuples[5]=cpu_to_fdt32(0x47000000);tuples[7]=cpu_to_fdt32(16);} /* disjoint allowed */
        if(fault==18){tuples[5]=cpu_to_fdt32(0x47000000);tuples[7]=cpu_to_fdt32(16);memcpy(tuples+8,tuples,16);} /* duplicate third */
        if(fault==19){tuples[1]=cpu_to_fdt32(AMP_CODE_LINUX_PA+4);tuples[3]=cpu_to_fdt32(AMP_CODE_SIZE);} /* malformed first */
        return fdt_setprop(blob,code,"reg",tuples,fault==18?48:32);
    }
    default:check(false,"known fault");
    }
    return fdt_setprop(blob,memory,"reg",values,sizeof(values));
}
int main(int argc,char **argv) {
    void *original,*blob,*other;size_t size,other_size;int ret,len,before_len,after_len;
    const fdt32_t *reg;unsigned char before[4096],after[4096];
    bootm_headers_t images={0x4a200000,0x4a200000+0x64c22e};
    check(argc==4,"B/A/factory DT arguments");original=load_dt(argv[1],&size);
    check(amp_linux_fdt_check_original(original,false)==0,"B original preflight accepted");
    check(amp_linux_fdt_check_fixed(original,false)==0,"B fixed preflight accepted");
    blob=fresh(original,size);before_len=snapshot(blob,before);banks();fixed_active=false;mutation=0;
    ret=image_setup_libfdt(&images,blob,fdt_totalsize(blob),NULL);
    check(ret!=0 && last_check==-EINVAL && ft_verify_calls==0,"old complete final prep reproduces stop before final verify");
    reg=fdt_getprop(blob,fdt_path_offset(blob,"/memory"),"reg",&len);
    check(reg && len==CONFIG_NR_DRAM_BANKS*16,"old runtime reg includes configured empty slots");
    after_len=snapshot(blob,after);check(before_len==after_len && !memcmp(before,after,before_len),"actual final prep does not overwrite no-map/reg");
    printf("OLD_FINAL_GUARD=%d MEMORY_BYTES=%d EMPTY_SLOTS=%d RESERVED_UNCHANGED=1\n",last_check,len,CONFIG_NR_DRAM_BANKS-3);
    free(blob);fixed_active=true;
    for(int fault=0;fault<=19;fault++) {
        blob=fresh(original,size);banks();mutation=fault;phase=0;last_check=123;ft_verify_calls=0;
        ret=image_setup_libfdt(&images,blob,fdt_totalsize(blob),NULL);
        bool allowed=fault==0 || fault==17;
        check(allowed ? ret==0 && last_check==0 && ft_verify_calls==1 : ret!=0 && last_check<0 && ft_verify_calls==0,
              "actual fixed complete prep/guard accept/reject as required");
        printf("FINAL_FDT_CASE=%d ALLOWED=%d GUARD=%d RETURN=%d\n",fault,allowed,last_check,ret);free(blob);
    }
    mutation=0;amp_active=false;
    for(int dt=2;dt<=3;dt++) {
        other=load_dt(argv[dt],&other_size);blob=fresh(other,other_size);banks();last_check=123;ft_verify_calls=0;
        ret=image_setup_libfdt(&images,blob,fdt_totalsize(blob),NULL);
        check(ret==0 && last_check==123 && ft_verify_calls==1,"inactive AMP retains A/factory normal final prep");free(blob);free(other);
    }
    printf("P030_ACTUAL_FINAL_FDT_CHECKS=%d\n",checks);free(original);return 0;
}
'''


def run():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["source", "baseline-source", "build", "linux-fdt", "stage-a-dt", "factory-dt", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    files = {name: (args.source / name).read_text() for name in [
        "drivers/cpu/rockchip_amp.c", "common/fdt_support.c", "common/image-fdt.c",
        "arch/arm/lib/bootm-fdt.c", "include/amp_project_contract.h"]}
    cfg = (args.build / "u-boot.cfg").read_text()
    banks = int(re.search(r"^#define CONFIG_NR_DRAM_BANKS\s+(\d+)$", cfg, re.M).group(1))
    if banks != 12 or not re.search(r"^#define CONFIG_ARCH_FIXUP_FDT_MEMORY\s+1$", cfg, re.M):
        raise RuntimeError("Saved actual RK3576 build must have 12 banks and enabled memory fixup")
    old_guard = extract((args.baseline_source / "drivers/cpu/rockchip_amp.c").read_text(), "amp_linux_fdt_check")
    fixed_guard = extract(files["drivers/cpu/rockchip_amp.c"], "amp_linux_fdt_check")
    if old_guard == fixed_guard:
        raise RuntimeError("Old and repaired guards must be independently supplied actual source")
    support_names = ["fdt_find_or_add_subnode", "fdt_pack_reg", "fdt_fixup_memory_banks", "fdt_setprop_uxx", "fdt_shrink_to_minimum", "fdt_initrd"]
    body = "#define CONFIG_NR_DRAM_BANKS %d\n" % banks + PREFIX + files["include/amp_project_contract.h"]
    body += "\n".join(extract(files["common/fdt_support.c"], name) for name in support_names)
    body += "\n".join(extract(files["drivers/cpu/rockchip_amp.c"], name) for name in ["amp_ranges_overlap", "amp_fdt_node_enabled"])
    body += old_guard.replace("amp_linux_fdt_check(", "amp_linux_fdt_check_original(", 1)
    body += fixed_guard.replace("amp_linux_fdt_check(", "amp_linux_fdt_check_fixed(", 1) + WRAPPER
    body += extract(files["arch/arm/lib/bootm-fdt.c"], "arch_fixup_fdt")
    body += extract(files["common/image-fdt.c"], "image_setup_libfdt") + SUFFIX
    generated = args.output / "actual-final-fdt.c"
    generated.write_text(body)
    native, commands = [], []
    libfdt = args.source / "scripts/dtc/libfdt"
    for name in ["fdt.c", "fdt_ro.c", "fdt_wip.c", "fdt_rw.c", "fdt_sw.c", "fdt_empty_tree.c", "fdt_addresses.c", "fdt_strerror.c"]:
        obj = args.output / (name + ".o")
        command = ["cc", "-std=c11", "-I", str(libfdt), "-c", str(libfdt / name), "-o", str(obj)]
        compiled = subprocess.run(command, capture_output=True, text=True)
        if compiled.returncode:
            raise RuntimeError(compiled.stderr)
        native.append(str(obj)); commands.append(command)
    command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(libfdt), str(generated), *native, "-o", str(args.output / "actual-final-fdt")]
    compiled = subprocess.run(command, capture_output=True, text=True)
    (args.output / "compile.stdout").write_text(compiled.stdout)
    (args.output / "compile.stderr").write_text(compiled.stderr)
    if compiled.returncode:
        raise RuntimeError(compiled.stderr)
    test = subprocess.run([str(args.output / "actual-final-fdt"), str(args.linux_fdt), str(args.stage_a_dt), str(args.factory_dt)], capture_output=True, text=True)
    (args.output / "test.stdout").write_text(test.stdout)
    (args.output / "test.stderr").write_text(test.stderr)
    if test.returncode:
        raise RuntimeError(test.stdout + test.stderr)
    evidence = [generated, args.build / "u-boot.cfg", args.linux_fdt, args.stage_a_dt, args.factory_dt,
                args.baseline_source / "drivers/cpu/rockchip_amp.c", *[args.source / name for name in files]]
    report = {"result": "PASS", "evidence_level": "HOST_TESTED", "compiler_exit": compiled.returncode, "test_exit": test.returncode,
              "checks": int(re.search(r"P030_ACTUAL_FINAL_FDT_CHECKS=(\d+)", test.stdout).group(1)),
              "actual_complete_final_prep_cases": 23, "negative_mutations": 18,
              "source_guard": "Actual supplied repaired source, unmodified by harness", "baseline_guard": "Actual supplied old source, unchanged",
              "actual_functions": ["arch_fixup_fdt", "fdt_fixup_memory_banks", "fdt_pack_reg", "image_setup_libfdt", "fdt_shrink_to_minimum", "fdt_initrd", "amp_linux_fdt_check"],
              "configured_memory_slots": banks, "unused_slots": banks-3, "board_access": False, "firmware_executed": False,
              "boundaries": "Real vendored libfdt. Board/DRM/bidram/chosen bootargs/network/LMB/hardware hooks stubbed. Saved real three bank intervals supplied; malformed DT mutations injected after real arch fixup. HUSH and hardware are not executed.",
              "commands": commands + [command], "files": {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in evidence}, "stdout": test.stdout}
    (args.output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    run()
