#!/usr/bin/env python3
"""Compile actual P026 C reservation/preflight/order functions with Host stubs.

Does not execute firmware, use MMIO, contact the board, or authorize deployment.
The LMB and reservation logic is actual source; FDT/firmware calls are fault stubs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def extract(text, name):
    m = re.search(r"(?:^|\n)(?:static\s+)?(?:inline\s+)?(?:[\w]+(?:\s+\*)?\s*|void\s+\*)" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", text)
    if not m:
        raise ValueError(f"function not found: {name}")
    pos = text.index("{", m.start())
    depth, end = 1, pos + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[m.start():end].lstrip()


PRELUDE = r'''
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint8_t u8;
typedef unsigned long ulong;
typedef uint64_t phys_addr_t;
typedef uint64_t phys_size_t;
typedef uint32_t fdt32_t;
static int cases;
static void check(int ok, const char *name)
{ cases++; if (!ok) { fprintf(stderr,"FAIL: %s\n",name); exit(1); } }
'''

LMB_PREFIX = r'''
#define MAX_LMB_REGIONS 32
#define LMB_ALLOC_ANYWHERE 0
#define min(a,b) ((a)<(b)?(a):(b))
struct lmb_property { phys_addr_t base; phys_size_t size; };
struct lmb_region { ulong cnt; phys_size_t size; struct lmb_property region[MAX_LMB_REGIONS+1]; };
struct lmb { struct lmb_region memory, reserved; };
struct list_head { struct list_head *next,*prev; };
struct memblk_attr { const char *name; u32 flags; };
struct memblock { phys_addr_t base, orig_base; phys_size_t size; struct memblk_attr attr; struct list_head node; };
struct sysmem { struct lmb lmb; struct list_head allocated_head; ulong allocated_cnt; };
static struct sysmem plat_sysmem;
static int initialized=1, fail_calloc;
static bool sysmem_has_init(void) { return initialized; }
static void list_add_tail(struct list_head *node, struct list_head *head)
{ node->next=head; node->prev=head->prev; head->prev->next=node; head->prev=node; }
#define F_KMEM_CAN_OVERLAP 32
static void *test_calloc(size_t n, size_t size) { return fail_calloc ? NULL : calloc(n,size); }
#define calloc test_calloc
'''

LMB_SUFFIX = r'''
int main(void)
{
    struct lmb *l=&plat_sysmem.lmb;
    lmb_init(l); plat_sysmem.allocated_head.next=plat_sysmem.allocated_head.prev=&plat_sysmem.allocated_head;
    check(lmb_add(l,0x40000000,0x40000000)>=0,"DRAM populated");
    check(sysmem_reserve_amp_region("code",0x47800000,0x80000)==(void *)0x47800000,"full code reserve");
    check(sysmem_reserve_amp_region("shared",0x47d00000,0x20000)==(void *)0x47d00000,"full shared reserve");
    check(lmb_overlaps_region(&l->reserved,0x47870000,4)>=0,"BSS/heap/stack tail protected beyond BIN");
    check(lmb_overlaps_region(&l->reserved,0x47d1fffc,4)>=0,"last pool bytes protected");
    check(sysmem_reserve_amp_region("duplicate",0x47800000,4)==NULL,"duplicate rejected");
    check(sysmem_reserve_amp_region("code",0x47f00000,8)==(void *)0x47f00000,"names do not grant incorrect existing reservation");
    check(sysmem_reserve_amp_region("partial",0x477ffffc,8)==NULL,"edge conflict rejected");
    check(sysmem_reserve_amp_region("MMIO",0x26004060,8)==NULL,"MMIO outside DRAM rejected");
    check(sysmem_reserve_amp_region("wrap",UINT64_MAX-7,16)==NULL,"64-bit overflow rejected");
    check(sysmem_reserve_amp_region("gap",0x80000000,8)==NULL,"out-of-bank rejected");
    check(sysmem_reserve_amp_region("zero",0x47000000,0)==NULL,"zero size rejected");
    check(sysmem_reserve_amp_region(NULL,0x47000000,8)==NULL,"null name rejected");
    check(sysmem_reserve_amp_region("unaligned",0x47000001,8)==NULL,"unaligned base rejected");
    check(sysmem_reserve_amp_region("size",0x47000000,1)==NULL,"unaligned size rejected");
    initialized=0;
    check(sysmem_reserve_amp_region("not-init",0x47000000,8)==NULL,"not initialized fails closed");
    initialized=1; fail_calloc=1;
    check(sysmem_reserve_amp_region("noheap",0x47000000,8)==NULL,"metadata allocation fails closed");
    check(lmb_overlaps_region(&l->reserved,0x47000000,8)<0,"metadata failure did not leak LMB entry");
    fail_calloc=0;
    check(__lmb_alloc_base(l,0x80000,1,0x47880000)!=0x47800000,"later LMB allocation cannot claim code");
    check(__lmb_alloc_base(l,0x20000,1,0x47d20000)!=0x47d00000,"later LMB allocation cannot claim shared");
    printf("RESERVATION_CASES=%d\n",cases);
    return 0;
}
'''

ORDER_PREFIX = r'''
#include "amp_project_contract.h"
#define FIT_HEADER_SIZE 4096
#define SZ_64K 65536
#define CONFIG_SYSMEM 1
#define IS_ENABLED(x) (x)
#define IH_TYPE_STANDALONE 1
#define IH_ARCH_ARM 2
#define IH_COMP_NONE 0
#define FIT_LOADABLE_PROP "loadables"
#define IH_ARCH_DEFAULT 2
#define IF_TYPE_MMC 1
#define FS_TYPE_ANY 0
#define ARCH_DMA_MINALIGN 64
#define CONFIG_NR_DRAM_BANKS 2
#define LINUX_FDT_FAKE ((void *)0x48300000)
#define ALIGN(n,a) (((n)+(a)-1)&~((a)-1))
typedef int64_t loff_t;
struct fdt_header { uint32_t words[10]; };
struct board_test { struct { u64 start, size; } bi_dram[2]; };
static struct board_test bd = { .bi_dram={{0x40000000,0x40000000},{0,0}} };
static struct { struct board_test *bd; } gd_store = { &bd }, *gd = &gd_store;
typedef struct { int placeholder; } disk_partition_t;
struct blk_desc { int if_type, devnum; };
static struct blk_desc test_bootdev={IF_TYPE_MMC,0};
typedef struct { const char *fit_uname_cfg; void *fit_hdr_os; int verify; } bootm_headers_t;
static bool amp_load_attempted, amp_preload_reserved;
static bool amp_remote_copy_active;
static struct {
    int init, reserve_fail, reserve_calls, copies, releases, verify_calls;
    int header, conf, linux_node, loadable_count, node, desc_len, shared_len;
    int type, arch, comp, data_result, hash_node, algo_len, value_len, ignore;
    int policy, hash_ok, copy_result, dispatcher_result, release_result;
    size_t total, tree, data_size, data_offset;
    u32 code_size, window, shared_base, shared_size;
    ulong load, entry;
    const char *name, *desc, *algo;
    int partition, fs_selects, fail_select, file_missing, read_error, alloc_failed;
    loff_t file_size, read_size;
    int linux_fdt_bad;
    void *heap_alias;
} f;
static int amp_linux_fdt_check(const void *blob, bool require_memory) { (void)blob;(void)require_memory;return f.linux_fdt_bad; }
static struct blk_desc *rockchip_get_bootdev(void) { return &test_bootdev; }
static int part_get_info_by_name(struct blk_desc *desc,const char *name,disk_partition_t *part)
{ (void)desc;(void)part;check(!strcmp(name,"boot"),"file loader uses existing named boot partition");return f.partition; }
static int fs_set_blk_dev(const char *interface,const char *device,int type)
{ (void)device;(void)type;check(!strcmp(interface,"mmc"),"file loader matches boot device class");return ++f.fs_selects==f.fail_select ? -EIO:0; }
static int fs_size(const char *filename,loff_t *size)
{ (void)filename;*size=f.file_size;return f.file_missing ? -ENOENT:0; }
static int fs_read(const char *filename,ulong target,loff_t offset,loff_t bytes,loff_t *read_size)
{ (void)filename;(void)target;check(offset==0 && bytes==f.file_size,"filesystem read bounded to measured size");*read_size=f.read_size;return f.read_error; }
static void *memalign(size_t alignment,size_t size)
{ (void)alignment;return f.alloc_failed ? NULL:(f.heap_alias ? f.heap_alias:malloc(size)); }
static bool sysmem_has_init(void) { return f.init; }
static void *sysmem_reserve_amp_region(const char *name, phys_addr_t base, phys_size_t size)
{
    (void)name; (void)size; f.reserve_calls++;
    return f.reserve_fail==f.reserve_calls ? NULL : (void *)(uintptr_t)base;
}
static u32 fdt32_to_cpu(fdt32_t v) { return __builtin_bswap32(v); }
static const void *fdt_getprop(const void *fit,int node,const char *key,int *len)
{
    static fdt32_t cells[2]; static uint8_t value[32];
    (void)fit; (void)node;
    if (!strcmp(key,"description")) { *len=f.desc_len; return f.desc; }
    if (!strcmp(key,"rockchip,mcu-shared-region")) {
        cells[0]=__builtin_bswap32(f.shared_base); cells[1]=__builtin_bswap32(f.shared_size);
        *len=f.shared_len; return cells;
    }
    if (!strcmp(key,"algo")) { *len=f.algo_len; return f.algo; }
    if (!strcmp(key,"value")) { *len=f.value_len; return value; }
    if (!strcmp(key,"ignore")) return f.ignore ? value : NULL;
    if (!strcmp(key,"rockchip,mcu-code-size")) cells[0]=__builtin_bswap32(f.code_size);
    else if (!strcmp(key,"rockchip,mcu-shared-window-base")) cells[0]=__builtin_bswap32(f.window);
    else return NULL;
    *len=4; return cells;
}
static int fdt_check_header(const void *fit) { (void)fit; return f.header; }
static size_t fdt_totalsize(const void *fit) { (void)fit; return f.tree; }
static int fit_get_totalsize(const void *fit,int *size) { (void)fit; *size=f.total; return 0; }
static int fit_conf_get_node(const void *fit,const char *name) { (void)fit;(void)name;return f.conf; }
static int fdt_subnode_offset(const void *fit,int node,const char *name)
{ (void)fit;(void)node;return !strcmp(name,"linux") ? f.linux_node : f.hash_node; }
static int fdt_stringlist_count(const void *fit,int node,const char *name)
{ (void)fit;(void)node;(void)name;return f.loadable_count; }
static const char *fdt_stringlist_get(const void *fit,int node,const char *key,int index,int *len)
{ (void)fit;(void)node;(void)key;(void)index;*len=f.name ? strlen(f.name):0;return f.name; }
static int fit_image_get_node(const void *fit,const char *name) { (void)fit;(void)name;return f.node; }
static int fit_image_get_type(const void *fit,int node,u8 *out) { (void)fit;(void)node;*out=f.type;return 0; }
static int fit_image_get_arch(const void *fit,int node,u8 *out) { (void)fit;(void)node;*out=f.arch;return 0; }
static int fit_image_get_comp(const void *fit,int node,u8 *out) { (void)fit;(void)node;*out=f.comp;return 0; }
static int fit_image_get_load(const void *fit,int node,ulong *out) { (void)fit;(void)node;*out=f.load;return 0; }
static int fit_image_get_entry(const void *fit,int node,ulong *out) { (void)fit;(void)node;*out=f.entry;return 0; }
static int fit_image_get_data(const void *fit,int node,const void **out,size_t *size)
{ (void)node;*out=(const char *)fit+f.data_offset;*size=f.data_size;return f.data_result; }
static int amp_verify_config_policy(const void *fit,const char *config) { (void)fit;(void)config;f.verify_calls++;return f.policy; }
static int fit_image_verify(const void *fit,int node) { (void)fit;(void)node;return f.hash_ok; }
static int boot_get_loadable(int argc,void *argv,bootm_headers_t *images,int arch,void *start,void *size)
{
    (void)argc;(void)argv;(void)images;(void)arch;(void)start;(void)size;
    check(amp_preload_reserved && f.reserve_calls==2,"copy after complete reservations");
    check(amp_remote_copy_active,"copy exception active only in controlled loader");
    f.copies++;return f.copy_result;
}
static void flush_dcache_all(void) { }
static int parse_os_amp_dispatcher(void) { return f.dispatcher_result; }
static int brought_up_all_amp(void *fit,const char *config)
{ (void)fit;(void)config;check(f.copies==1,"release only after successful copy");f.releases++;return f.release_result; }
static void reset(void)
{
    memset(&f,0,sizeof(f));amp_load_attempted=amp_preload_reserved=amp_remote_copy_active=false;
    f.init=1;f.conf=1;f.linux_node=-1;f.loadable_count=1;f.node=2;
    f.desc_len=sizeof("bus_mcu");f.shared_len=8;f.type=IH_TYPE_STANDALONE;
    f.arch=IH_ARCH_ARM;f.comp=IH_COMP_NONE;f.hash_node=3;f.algo_len=sizeof("sha256");
    f.value_len=32;f.hash_ok=1;f.total=8192;f.tree=4096;f.data_size=4096;f.data_offset=4096;
    f.code_size=AMP_CODE_SIZE;f.window=AMP_SHARED_WINDOW_LINUX_PA;
    f.shared_base=AMP_SHARED_LINUX_PA;f.shared_size=AMP_SHARED_SIZE;
    f.load=AMP_CODE_LINUX_PA;f.entry=AMP_M0_LOCAL_ENTRY;f.name="mcu";f.desc="bus_mcu";f.algo="sha256";
    f.partition=2;f.file_size=f.read_size=8192;
}
'''

ORDER_SUFFIX = r'''
int main(void)
{
    void *fit=malloc(AMP_CODE_SIZE+SZ_64K); check(fit!=NULL,"Host fake FIT storage");
    reset();check(amp_boot_fit(fit,8192)==0,"valid exact contract accepted");
    check(f.copies==1 && f.releases==1,"one copy and release");
    check(!amp_remote_copy_active,"copy exception closed before release");
    check(amp_fit_copy_range_check(AMP_CODE_LINUX_PA,4096)<0,"subsequent FIT copy cannot overwrite remote");
    check(amp_boot_fit(fit,8192)<0 && f.copies==1,"second start blocked");
    check(amp_reserved_range_check(AMP_CODE_LINUX_PA+AMP_CODE_SIZE-8,8)<0,"late code write rejected");
    check(amp_reserved_range_check(AMP_SHARED_LINUX_PA+AMP_SHARED_SIZE-8,8)<0,"late payload write rejected");
    check(amp_reserved_range_check(AMP_CODE_LINUX_PA-8,8)==0,"adjacent write allowed");
    check(amp_reserved_range_check(AMP_CODE_LINUX_PA+AMP_CODE_SIZE,8)==0,"end adjacent allowed");
    check(amp_reserved_range_check(UINT64_MAX-7,16)<0,"late range overflow rejected");
    reset();f.reserve_fail=1;check(amp_boot_fit(fit,8192)<0 && !f.copies && !f.releases,"first reserve failure no copy/release");
    reset();f.reserve_fail=2;check(amp_boot_fit(fit,8192)<0 && !f.copies && !f.releases,"second reserve failure no copy/release");
    check(amp_load_attempted && !amp_preload_reserved,"partial reserve remains protected and cannot retry");
    reset();f.policy=-EACCES;check(amp_boot_fit(fit,8192)==-EACCES && !f.reserve_calls && !f.copies,"policy failure no reservation/copy");
    reset();f.hash_ok=0;check(amp_boot_fit(fit,8192)==-EACCES && !f.copies,"hash failure no copy");
    reset();f.copy_result=-EIO;check(amp_boot_fit(fit,8192)==-EIO && !f.releases,"copy error no release");
    reset();f.dispatcher_result=-EINVAL;check(amp_boot_fit(fit,8192)<0 && !f.releases,"dispatcher failure no release");
    reset();f.release_result=-EACCES;check(amp_boot_fit(fit,8192)==-EACCES,"secure release failure propagated");
#define BAD(field,value,label) do { reset();f.field=(value);check(amp_boot_fit(fit,8192)<0 && !f.copies && !f.reserve_calls && !f.verify_calls,label); } while(0)
    BAD(header,1,"bad header rejected before verification");
    BAD(tree,8193,"truncated metadata rejected");
    BAD(total,8191,"FIT total mismatch rejected");
    BAD(linux_node,5,"non-M0 Linux dispatcher rejected");
    BAD(loadable_count,2,"extra loadable collision cannot enter loop");
    BAD(name,"cpu3","CPU3 loadable rejected");
    BAD(desc,"pmu_mcu","PMU MCU rejected");
    BAD(desc_len,0,"malformed description rejected");
    BAD(type,9,"wrong image type rejected");
    BAD(arch,3,"wrong architecture rejected");
    BAD(comp,1,"compressed loadable rejected");
    BAD(load,AMP_SHARED_LINUX_PA,"shared-pool destination rejected");
    BAD(entry,AMP_M0_LOCAL_ENTRY+2,"entry mismatch rejected");
    BAD(code_size,AMP_CODE_SIZE-8,"short code reserve rejected");
    BAD(window,AMP_SHARED_WINDOW_LINUX_PA+0x400,"window mismatch rejected");
    BAD(shared_base,AMP_CODE_LINUX_PA,"shared-code collision rejected");
    BAD(shared_size,AMP_SHARED_SIZE-8,"partial shared region rejected");
    BAD(shared_len,4,"missing size cell rejected");
    BAD(data_size,0,"empty payload rejected");
    BAD(data_size,AMP_CODE_SIZE+1,"payload exceeds code allocation");
    BAD(data_offset,8193,"payload source beyond FIT buffer rejected");
    BAD(data_size,4097,"external payload truncated rejected");
    BAD(data_result,-ENOENT,"missing payload rejected");
    BAD(hash_node,-ENOENT,"missing integrity hash rejected");
    BAD(algo,"sha1","unexpected weak hash rejected");
    BAD(value_len,31,"truncated SHA rejected");
    BAD(ignore,1,"ignored hash rejected");
    reset();check(amp_preflight_m0((void *)(uintptr_t)AMP_CODE_LINUX_PA,8192)<0,"staging FIT overlapping code rejected");
    reset();check(amp_preflight_m0((void *)(uintptr_t)AMP_SHARED_LINUX_PA,8192)<0,"staging FIT overlapping shared rejected");
    reset();f.init=0;check(amp_boot_fit(fit,8192)<0 && !f.copies,"missing sysmem never copies");
    reset();check(amp_m0_file("/amp/amp.itb",LINUX_FDT_FAKE)==0 && f.fs_selects==2,"file path reselects after size then uses checked loader");
    reset();f.file_missing=1;check(amp_m0_file("/absent",LINUX_FDT_FAKE)==-ENOENT && !f.releases && !f.reserve_calls,"missing file has no MCU effects");
    reset();f.partition=-ENOENT;check(amp_m0_file("/amp/amp.itb",LINUX_FDT_FAKE)==-ENODEV && !f.copies,"missing boot partition rejected");
    reset();f.fail_select=1;check(amp_m0_file("/amp/amp.itb",LINUX_FDT_FAKE)==-ENOENT && !f.copies,"initial filesystem select failure");
    reset();f.fail_select=2;check(amp_m0_file("/amp/amp.itb",LINUX_FDT_FAKE)==-EIO && !f.copies,"filesystem reselect failure");
    reset();f.file_size=AMP_CODE_SIZE+SZ_64K+1;check(amp_m0_file("/amp/amp.itb",LINUX_FDT_FAKE)==-EFBIG && !f.copies,"oversized file rejected before allocation");
    reset();f.file_size=4095;check(amp_m0_file("/amp/amp.itb",LINUX_FDT_FAKE)==-EFBIG && !f.copies,"short file rejected");
    reset();f.alloc_failed=1;check(amp_m0_file("/amp/amp.itb",LINUX_FDT_FAKE)==-ENOMEM && !f.releases,"heap failure no MCU effects");
    reset();f.read_size=8191;check(amp_m0_file("/amp/amp.itb",LINUX_FDT_FAKE)==-EIO && !f.reserve_calls,"short file read rejected");
    reset();f.read_error=-EIO;check(amp_m0_file("/amp/amp.itb",LINUX_FDT_FAKE)==-EIO && !f.verify_calls,"filesystem read error before verify");
    reset();f.policy=-EACCES;check(amp_m0_file("/amp/amp.itb",LINUX_FDT_FAKE)==-EACCES && !f.releases,"file loader does not bypass signature policy");
    reset();check(amp_m0_file("/amp/amp.itb",(void *)AMP_CODE_LINUX_PA)==-EINVAL && !f.fs_selects,"inactive AMP code cannot hold prepared DT");
    reset();check(amp_m0_file("/amp/amp.itb",(void *)AMP_SHARED_LINUX_PA)==-EINVAL && !f.fs_selects,"inactive AMP pool cannot hold prepared DT");
    reset();check(amp_m0_file("/amp/amp.itb",(void *)0x26004060)==-EINVAL && !f.fs_selects,"FDT MMIO pointer rejected before read");
    reset();f.linux_fdt_bad=-EINVAL;check(amp_m0_file("/amp/amp.itb",LINUX_FDT_FAKE)==-EINVAL && !f.reserve_calls && !f.releases,"stock Linux FDT rejected before file or MCU effects");
    reset();check(amp_fdt_ram_range(0x7ffffff0,0x100)<0,"FDT entire buffer cannot cross DRAM bank");
    check(amp_fdt_ram_range(UINT64_MAX-7,16)<0,"FDT address overflow rejected");
    reset();f.heap_alias=malloc(65536);check(f.heap_alias!=NULL,"alias test buffer");
    bd.bi_dram[1].start=(ulong)f.heap_alias;bd.bi_dram[1].size=65536;
    check(amp_m0_file("/amp/amp.itb",f.heap_alias)==-EINVAL && f.fs_selects==1 && !f.reserve_calls,"FIT heap cannot overwrite prepared Linux DT before read");
    bd.bi_dram[1].size=0;
    printf("PREFLIGHT_ORDER_CASES=%d\n",cases);free(fit);return 0;
}
'''

REAL_FDT_PREFIX = r'''
#include <libfdt.h>
#include "amp_project_contract.h"
#define FIT_HEADER_SIZE 4096
#define SZ_64K 65536
#define FIT_LOADABLE_PROP "loadables"
#define IH_TYPE_STANDALONE 1
#define IH_ARCH_ARM 2
#define IH_COMP_NONE 0
static bool amp_load_attempted;
static int fit_get_totalsize(const void *fit,int *value)
{ int len;const fdt32_t *p=fdt_getprop(fit,0,"totalsize",&len);if (!p || len!=4)return -EINVAL;*value=fdt32_to_cpu(*p);return 0; }
static int fit_conf_get_node(const void *fit,const char *name)
{ int configs=fdt_path_offset(fit,"/configurations");return configs<0 ? configs:fdt_subnode_offset(fit,configs,name); }
static int fit_image_get_node(const void *fit,const char *name)
{ int images=fdt_path_offset(fit,"/images");return images<0 ? images:fdt_subnode_offset(fit,images,name); }
static int string_kind(const void *fit,int node,const char *property,const char *expected,u8 *value,u8 kind)
{ int len;const char *p=fdt_getprop(fit,node,property,&len);if (!p || (size_t)len!=strlen(expected)+1 || memcmp(p,expected,len))return -EINVAL;*value=kind;return 0; }
static int fit_image_get_type(const void *fit,int n,u8 *v) { return string_kind(fit,n,"type","standalone",v,IH_TYPE_STANDALONE); }
static int fit_image_get_arch(const void *fit,int n,u8 *v) { return string_kind(fit,n,"arch","arm",v,IH_ARCH_ARM); }
static int fit_image_get_comp(const void *fit,int n,u8 *v) { return string_kind(fit,n,"compression","none",v,IH_COMP_NONE); }
static int cell(const void *fit,int n,const char *key,ulong *value)
{ int len;const fdt32_t *p=fdt_getprop(fit,n,key,&len);if (!p || len!=4)return -EINVAL;*value=fdt32_to_cpu(*p);return 0; }
static int fit_image_get_load(const void *fit,int n,ulong *v) { return cell(fit,n,"load",v); }
static int fit_image_get_entry(const void *fit,int n,ulong *v) { return cell(fit,n,"entry",v); }
static int fit_image_get_data(const void *fit,int n,const void **data,size_t *size)
{ ulong position,len;if (cell(fit,n,"data-position",&position)||cell(fit,n,"data-size",&len))return -EINVAL;*data=(const char *)fit+position;*size=len;return 0; }
'''

REAL_FDT_SUFFIX = r'''
int main(int argc,char **argv)
{
    FILE *in;long size;void *fit,*linux_original,*linux_tree;int node,parent,memory,code,pool,extra;
    fdt32_t reg[4];
    check(argc==3,"FIT and Linux fixture arguments");in=fopen(argv[1],"rb");check(in!=NULL,"FIT opened");
    check(fseek(in,0,SEEK_END)==0,"FIT seek");size=ftell(in);check(size>0,"FIT size");
    rewind(in);fit=malloc(size);check(fit!=NULL,"FIT buffer");check(fread(fit,1,size,in)==(size_t)size,"FIT read");fclose(in);
    check(amp_preflight_m0(fit,size)==0,"actual generated external-data FIT passes actual preflight");
    node=fit_image_get_node(fit,"mcu");check(node>=0,"real MCU node");
    check(fdt_setprop_inplace_u32(fit,node,"rockchip,mcu-code-size",AMP_CODE_SIZE-8)==0,"mutate real geometry");
    check(amp_preflight_m0(fit,size)<0,"real malformed geometry fails closed");
    check(fdt_setprop_inplace_u32(fit,node,"rockchip,mcu-code-size",AMP_CODE_SIZE)==0,"restore geometry");
    check(fdt_setprop_inplace_u32(fit,node,"data-size",size)==0,"mutate real payload length");
    check(amp_preflight_m0(fit,size)<0,"real out-of-bounds payload fails closed");
    in=fopen(argv[2],"rb");check(in!=NULL,"Linux DT fixture opened");
    check(fseek(in,0,SEEK_END)==0,"Linux DT seek");size=ftell(in);check(size>0,"Linux DT size");rewind(in);
    linux_original=malloc(size);linux_tree=malloc(size);check(linux_original && linux_tree,"Linux DT buffers");
    check(fread(linux_original,1,size,in)==(size_t)size,"Linux DT read");fclose(in);memcpy(linux_tree,linux_original,size);
    check(amp_linux_fdt_check(linux_tree,true)==0,"actual libfdt accepts full contract no-map Linux DT");
    parent=fdt_path_offset(linux_tree,"/reserved-memory");code=fdt_subnode_offset(linux_tree,parent,"code");pool=fdt_subnode_offset(linux_tree,parent,"pool");memory=fdt_path_offset(linux_tree,"/memory");
    check(parent>=0 && code>=0 && pool>=0 && memory>=0,"fixture nodes exist");
#define BAD_DT(mutation,label) do { memcpy(linux_tree,linux_original,size);check((mutation)==0,"DT mutation succeeds");check(amp_linux_fdt_check(linux_tree,true)<0,label); } while (0)
    BAD_DT(fdt_nop_node(linux_tree,parent),"stock DT without reserved-memory rejected");
    BAD_DT(fdt_setprop_inplace(linux_tree,parent,"status","fail",5),"disabled reserved-memory parent cannot satisfy reservation");
    BAD_DT(fdt_setprop_inplace(linux_tree,code,"status","fail",5),"disabled code child cannot satisfy reservation");
    BAD_DT(fdt_setprop_inplace(linux_tree,pool,"status","fail",5),"disabled buffer child cannot satisfy reservation");
    BAD_DT(fdt_nop_property(linux_tree,code,"no-map"),"missing code no-map rejected");
    BAD_DT(fdt_nop_property(linux_tree,pool,"no-map"),"missing payload no-map rejected");
    BAD_DT(fdt_setprop(linux_tree,code,"reusable",NULL,0),"reusable code rejected");
    BAD_DT(fdt_setprop_inplace_u32(linux_tree,0,"#address-cells",1),"wrong root address cells rejected");
    BAD_DT(fdt_setprop_inplace_u32(linux_tree,parent,"#address-cells",1),"wrong reserved address cells rejected");
    BAD_DT(fdt_setprop_inplace_u32(linux_tree,parent,"#size-cells",1),"wrong reserved size cells rejected");
    BAD_DT(fdt_nop_property(linux_tree,parent,"ranges"),"missing identity ranges rejected");
    BAD_DT(fdt_nop_node(linux_tree,memory),"reserved areas outside Linux memory rejected");
    check(amp_linux_fdt_check(linux_tree,false)==0,"preflight permits kernel DT memory to be added by normal arch_fixup");
    BAD_DT(fdt_setprop_inplace(linux_tree,memory,"status","fail",5),"disabled Linux memory rejected");
    reg[0]=0;reg[1]=cpu_to_fdt32(AMP_CODE_LINUX_PA);reg[2]=0;reg[3]=cpu_to_fdt32(AMP_CODE_SIZE-4);
    BAD_DT(fdt_setprop_inplace(linux_tree,code,"reg",reg,sizeof(reg)),"short code reservation rejected");
    reg[1]=cpu_to_fdt32(AMP_CODE_LINUX_PA+4);reg[3]=cpu_to_fdt32(AMP_CODE_SIZE);
    BAD_DT(fdt_setprop_inplace(linux_tree,code,"reg",reg,sizeof(reg)),"shifted code address rejected");
    memcpy(linux_tree,linux_original,size);extra=fdt_add_subnode(linux_tree,parent,"foreign");check(extra>=0,"add foreign owner");
    reg[1]=cpu_to_fdt32(AMP_CODE_LINUX_PA+4);reg[3]=cpu_to_fdt32(4);
    check(fdt_setprop(linux_tree,extra,"reg",reg,sizeof(reg))==0,"foreign reg added");
    check(amp_linux_fdt_check(linux_tree,true)<0,"foreign overlapping reservation rejected");
    free(linux_original);free(linux_tree);
    printf("REAL_LIBFDT_CASES=%d\n",cases);free(fit);return 0;
}
'''


BOOT_BUFFERS_PREFIX = r'''
#include "amp_project_contract.h"
#define CONFIG_AMP 1
#define CONFIG_SYS_FDT_PAD 0x3000
#define CHUNKSZ 65536
#define BOOTSTAGE_ID_COPY_RAMDISK 1
#define debug(...) do {} while (0)
static bool amp_load_attempted = true;
struct lmb { int unused; };
static const char *test_high = "ffffffffffffffff";
static ulong test_alloc = 0x4a300000;
static int copies, fdt_writes;
static char *env_get(const char *name) { (void)name; return (char *)test_high; }
static ulong simple_strtoul(const char *s, char **end, unsigned int base)
{ return strtoul(s,end,base); }
static ulong env_get_bootm_mapsize(void) { return 0x80000000; }
static ulong env_get_bootm_low(void) { return 0x40000000; }
static int env_get_yesno(const char *name) { (void)name; return 0; }
static long lmb_reserve(struct lmb *l, ulong base, ulong size)
{ (void)l;(void)base;(void)size;return 0; }
static ulong lmb_alloc_base(struct lmb *l, ulong size, ulong align, ulong limit)
{ (void)l;(void)size;(void)align;(void)limit;return test_alloc; }
static ulong lmb_alloc(struct lmb *l, ulong size, ulong align)
{ return lmb_alloc_base(l,size,align,~0UL); }
static void bootstage_mark(int stage) { (void)stage; }
static void memmove_wd(void *dst, const void *src, ulong len, ulong chunk)
{ (void)dst;(void)src;(void)len;(void)chunk;copies++; }
static int fdt_check_header(const void *tree) { (void)tree;return 0; }
static void fdt_error(const char *message) { (void)message; }
static void fdt_set_totalsize(void *tree, ulong size)
{ (void)tree;(void)size;fdt_writes++; }
static int fdt_open_into(const void *src, void *dst, ulong size)
{ (void)src;(void)dst;(void)size;fdt_writes++;return 0; }
'''

BOOT_BUFFERS_SUFFIX = r'''
int main(void)
{
    struct lmb l; ulong start=0, end=0, size; char *tree;
    check(boot_ramdisk_high(&l,0x4a200000,0x100000,&start,&end)==0,"valid in-place initrd");
    check(start==0x4a200000 && end==0x4a300000 && !copies,"in-place initrd performs no relocation");
    check(boot_ramdisk_high(&l,AMP_CODE_LINUX_PA,0x100,&start,&end)<0 && !copies,"in-place initrd cannot use code");
    check(boot_ramdisk_high(&l,AMP_SHARED_LINUX_PA,0x100,&start,&end)<0 && !copies,"in-place initrd cannot use rings/payload");
    check(boot_ramdisk_high(&l,~0UL-7,16,&start,&end)<0,"initrd source overflow rejected");
    test_high="80000000";test_alloc=AMP_CODE_LINUX_PA;
    check(boot_ramdisk_high(&l,0x4a200000,0x100000,&start,&end)<0 && !copies,"bad LMB initrd destination rejected before copy");
    test_alloc=AMP_SHARED_LINUX_PA;
    check(boot_ramdisk_high(&l,0x4a200000,0x100000,&start,&end)<0 && !copies,"initrd destination pool rejected before copy");
    test_alloc=0x4a300000;
    check(boot_ramdisk_high(&l,0x4a200000,0x100000,&start,&end)==0 && copies==1,"valid initrd relocation retained");
    test_high="ffffffffffffffff";tree=(char *)0x48300000;size=0x7c000;
    check(boot_relocate_fdt(&l,&tree,&size)==0 && fdt_writes==1,"valid padded in-place FDT");
    tree=(char *)AMP_SHARED_LINUX_PA;size=0x100;
    check(boot_relocate_fdt(&l,&tree,&size)>0 && fdt_writes==1,"in-place FDT pool overlap rejected before write");
    tree=(char *)0x477ff000;size=0x800;
    check(boot_relocate_fdt(&l,&tree,&size)>0 && fdt_writes==1,"FDT original bytes safe but pad collision rejected");
    tree=(char *)0x48300000;size=~0UL-0x100;
    check(boot_relocate_fdt(&l,&tree,&size)>0 && fdt_writes==1,"FDT padding size overflow rejected");
    tree=(char *)(~0UL-0x100);size=0x100;
    check(boot_relocate_fdt(&l,&tree,&size)>0 && fdt_writes==1,"FDT source address overflow rejected");
    test_high="80000000";test_alloc=AMP_CODE_LINUX_PA;tree=(char *)0x48300000;size=0x7c000;
    check(boot_relocate_fdt(&l,&tree,&size)>0 && fdt_writes==1,"bad LMB FDT destination rejected before write");
    test_alloc=AMP_SHARED_LINUX_PA;tree=(char *)0x48300000;size=0x7c000;
    check(boot_relocate_fdt(&l,&tree,&size)>0 && fdt_writes==1,"FDT destination payload rejected");
    test_alloc=0x48300000;tree=(char *)0x48200000;size=0x7c000;
    check(boot_relocate_fdt(&l,&tree,&size)==0 && fdt_writes==2,"valid relocated FDT retained");
    printf("BOOT_BUFFER_CASES=%d\n",cases);return 0;
}
'''

ACTUAL_LINUX_SUFFIX = r'''
int main(int argc, char **argv)
{
    FILE *in; long size; void *blob; int pre, final;
    check(argc==2,"actual Host kernel DT argument");
    in=fopen(argv[1],"rb");check(in!=NULL,"actual Host kernel DT opened");
    check(fseek(in,0,SEEK_END)==0,"actual Host kernel DT seek");size=ftell(in);check(size>0,"actual Host kernel DT size");rewind(in);
    blob=malloc(size);check(blob!=NULL,"actual Host kernel DT buffer");check(fread(blob,1,size,in)==(size_t)size,"actual Host kernel DT read");fclose(in);
    pre=amp_linux_fdt_check(blob,false);final=amp_linux_fdt_check(blob,true);
    printf("ACTUAL_LINUX_PREFLIGHT=%d\nFINAL_MEMORY_CHECK_BEFORE_RUNTIME_FIXUP=%d\n",pre,final);
    check(pre==0,"actual Host merged AMP DT is accepted before runtime memory fixup");
    free(blob);return 0;
}
'''


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--fit", type=Path, help="Optional generated FIT, parsed by actual vendored libfdt")
    p.add_argument("--linux-fdt", type=Path, help="Optional actual Host merged AMP kernel DT; no automatic mutation")
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=False)
    body = (a.source / "drivers/cpu/rockchip_amp.c").read_text()
    sysmem = (a.source / "lib/sysmem.c").read_text()
    lmb = (a.source / "lib/lmb.c").read_text()
    image = (a.source / "common/image.c").read_text()
    image_fdt = (a.source / "common/image-fdt.c").read_text()
    preflight = extract(body, "amp_preflight_m0")
    lmb_functions = ["lmb_addrs_overlap", "lmb_addrs_adjacent", "lmb_regions_adjacent", "lmb_remove_region", "lmb_coalesce_regions", "lmb_init", "lmb_add_region", "lmb_add", "lmb_reserve", "lmb_overlaps_region", "lmb_align_down", "lmb_align_up", "__lmb_alloc_base"]
    sources = {
        "strict-reservation": PRELUDE + LMB_PREFIX + '#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored "-Wsign-compare"\n' + "\n".join(extract(lmb, n) for n in lmb_functions) + '\n#pragma GCC diagnostic pop\n' + extract(sysmem, "sysmem_is_overlap") + extract(sysmem, "sysmem_reserve_amp_region") + LMB_SUFFIX,
        "preflight-order": PRELUDE + ORDER_PREFIX + "\n".join(extract(body, n) for n in ["amp_ranges_overlap", "amp_fdt_ram_range", "amp_reserved_range_check", "amp_fit_copy_range_check", "amp_preload_reserve", "amp_require_u32", "amp_preflight_m0", "amp_boot_fit", "amp_m0_file"]) + ORDER_SUFFIX,
        "boot-buffer-guards": PRELUDE + BOOT_BUFFERS_PREFIX + "\n".join(extract(body, n) for n in ["amp_ranges_overlap", "amp_reserved_range_check"]) + extract(image, "boot_ramdisk_high") + extract(image_fdt, "boot_relocate_fdt") + BOOT_BUFFERS_SUFFIX,
    }
    if a.fit:
        sources["real-libfdt"] = PRELUDE.replace("typedef uint32_t fdt32_t;", "") + REAL_FDT_PREFIX + "\n".join(extract(body, n) for n in ["amp_ranges_overlap", "amp_fdt_node_enabled", "amp_linux_fdt_check", "amp_require_u32", "amp_preflight_m0"]) + REAL_FDT_SUFFIX
    if a.linux_fdt:
        sources["actual-linux-fdt"] = PRELUDE.replace("typedef uint32_t fdt32_t;", "") + '#include <libfdt.h>\n#include "amp_project_contract.h"\n' + "\n".join(extract(body, n) for n in ["amp_ranges_overlap", "amp_fdt_node_enabled", "amp_linux_fdt_check"]) + ACTUAL_LINUX_SUFFIX
    results = []
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        (tmp / "amp_project_contract.h").write_bytes((a.source / "include/amp_project_contract.h").read_bytes())
        if a.fit:
            macros = {name: int(value, 16) for name, value in re.findall(r"#define (AMP_\w+) (0x[0-9a-fA-F]+)", (tmp / "amp_project_contract.h").read_text())}
            linux_fixture = '/dts-v1/; / { #address-cells=<2>; #size-cells=<2>; memory { device_type="memory"; status="okay"; reg=<0 0x%x 0 0x10000000>; }; reserved-memory { #address-cells=<2>; #size-cells=<2>; ranges; status="okay";' % macros["AMP_SHARED_WINDOW_LINUX_PA"]
            for name, base, size in (("code", macros["AMP_CODE_LINUX_PA"], macros["AMP_CODE_SIZE"]), ("rings", macros["AMP_SHARED_LINUX_PA"], macros["AMP_POOL_LINUX_PA"]-macros["AMP_SHARED_LINUX_PA"]), ("pool", macros["AMP_POOL_LINUX_PA"], macros["AMP_POOL_SIZE"])):
                linux_fixture += '%s { reg=<0 0x%x 0 0x%x>; no-map; status="okay"; };' % (name, base, size)
            linux_fixture += '}; };'
            (tmp / "linux-fixture.dts").write_text(linux_fixture)
            subprocess.run(["dtc", "-I", "dts", "-O", "dtb", "-p", "4096", "-o", str(tmp / "linux-fixture.dtb"), str(tmp / "linux-fixture.dts")], check=True, capture_output=True, text=True)
        for name, source in sources.items():
            src, exe = tmp / f"{name}.c", tmp / name
            src.write_text(source)
            cmd = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(tmp), str(src), "-o", str(exe)]
            if name in ("real-libfdt", "actual-linux-fdt"):
                dtc = a.source / "scripts/dtc/libfdt"
                cmd += ["-I", str(dtc)]
                # These are the pin's standard Host libfdt sources, not firmware.
                # Unmodified upstream libfdt has unsigned/signed and unused-arg
                # diagnostics under Wextra; compile it separately in Host tools.
                # The actual new preload functions still use Wextra/Werror.
                native_objects = []
                for file in ("fdt.c", "fdt_ro.c", "fdt_wip.c", "fdt_rw.c", "fdt_sw.c", "fdt_empty_tree.c"):
                    obj = tmp / (file + ".o")
                    subprocess.run(["cc", "-std=c11", "-I", str(dtc), "-c", str(dtc / file), "-o", str(obj)], check=True, capture_output=True, text=True)
                    native_objects.append(str(obj))
                cmd += native_objects
            build = subprocess.run(cmd, capture_output=True, text=True)
            if build.returncode:
                raise RuntimeError(build.stderr)
            arguments = [str(a.fit), str(tmp / "linux-fixture.dtb")] if name == "real-libfdt" else ([str(a.linux_fdt)] if name == "actual-linux-fdt" else [])
            run = subprocess.run([str(exe)] + arguments, capture_output=True, text=True, check=True)
            results.append({"test": name, "compiler_exit": build.returncode, "test_exit": run.returncode, "stdout": run.stdout, "stderr": run.stderr})
    loader = extract(body, "amp_boot_fit")
    assert loader.index("amp_verify_config_policy") < loader.index("amp_preload_reserve") < loader.index("boot_get_loadable") < loader.index("brought_up_all_amp")
    assert "amp_boot_fit(fit, file_size)" in extract(body, "amp_m0_file")
    assert "sysmem_alloc_base_by_name" not in extract(body, "standalone_handler")
    fs = (a.source / "fs/fs.c").read_text()
    assert extract(fs, "fs_read").index("amp_reserved_range_check") < extract(fs, "fs_read").index("info->read(")
    booti = (a.source / "cmd/booti.c").read_text()
    assert extract(booti, "booti_setup").index("amp_reserved_range_check") < extract(booti, "booti_setup").index("memmove(")
    assert extract(image, "boot_ramdisk_high").index("amp_reserved_range_check") < extract(image, "boot_ramdisk_high").index("memmove_wd(")
    assert extract(image_fdt, "boot_relocate_fdt").index("amp_reserved_range_check") < extract(image_fdt, "boot_relocate_fdt").index("fdt_set_totalsize(")
    assert extract(body, "amp_m0_file").index("amp_linux_fdt_check") < extract(body, "amp_m0_file").index("fs_read(") < extract(body, "amp_m0_file").index("amp_boot_fit(")
    assert extract(image_fdt, "image_setup_libfdt").index("amp_linux_fdt_check") < extract(image_fdt, "image_setup_libfdt").index("ft_verify_fdt(")
    report = {"evidence": "HOST_TESTED", "board_access": False, "firmware_executed": False, "fdt_and_firmware_calls": "FAULT_STUBS; optional real libfdt uses thin FIT-property adapters plus actual Linux DT validation", "source_sha256": hashlib.sha256(body.encode()).hexdigest(), "FIT_sha256": hashlib.sha256(a.fit.read_bytes()).hexdigest() if a.fit else None, "Linux_DTB_sha256": hashlib.sha256(a.linux_fdt.read_bytes()).hexdigest() if a.linux_fdt else None, "integration_order_checks": 9, "tests": results}
    (a.output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
