#!/usr/bin/env python3
"""Actual U-Boot C source/parser/gates + actual compiled ELF default env.

No firmware execution, board access, deployment, or emulated MMIO. Host stubs
replace policy/command/hardware boundaries; the source parser and CRC routines,
main, autoboot, CLI, booti, image-format recognition are extracted unchanged.
libfdt is the pin's actual vendored Host implementation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def extract(text, name, occurrence=0):
    matches = list(re.finditer(r"(?:^|\n)(?:static\s+)?(?:inline\s+)?(?:const\s+)?(?:void|bool|int|ulong|uint32_t|char)[ \t\r\n]+(?:\*[ \t\r\n]*)?" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", text))
    m = matches[occurrence]
    start, pos = m.start(), text.index("{", m.start())
    depth, i, state = 1, pos + 1, "code"
    while depth:
        ch, nxt = text[i], text[i:i+2]
        if state == "code":
            if nxt == "/*": state = "comment"; i += 1
            elif nxt == "//": state = "line"; i += 1
            elif ch == '"': state = "string"
            elif ch == "'": state = "char"
            elif ch == "{": depth += 1
            elif ch == "}": depth -= 1
        elif state == "comment" and nxt == "*/": state = "code"; i += 1
        elif state == "line" and ch == "\n": state = "code"
        elif state in ("string", "char"):
            if ch == "\\": i += 1
            elif ch == ('"' if state == "string" else "'"): state = "code"
        i += 1
    return text[start:i].strip()


def elf_symbol(path, symbol):
    raw = path.read_bytes()
    assert raw[:6] == b"\x7fELF\x02\x01"
    hdr = struct.unpack_from("<16sHHIQQQIHHHHHH", raw)
    sections = [struct.unpack_from("<IIQQQQIIQQ", raw, hdr[6] + i * hdr[11]) for i in range(hdr[12])]
    for table in sections:
        if table[1] != 2: continue
        strings = sections[table[6]]
        names = raw[strings[4]:strings[4]+strings[5]]
        for off in range(table[4], table[4] + table[5], table[9]):
            st_name, info, other, index, value, size = struct.unpack_from("<IBBHQQ", raw, off)
            if names[st_name:names.find(b"\0", st_name)].decode() == symbol:
                sec = sections[index]
                fileoff = sec[4] + value - sec[3]
                return raw[fileoff:fileoff+size]
    raise ValueError("missing ELF symbol " + symbol)


PRELUDE = r'''
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <zlib.h>
#include <libfdt.h>
typedef unsigned long ulong;
typedef uint32_t u32;
typedef uint32_t __be32;
typedef uint64_t u64;
typedef unsigned char uchar;
typedef struct {} cmd_tbl_t;
#define CONFIG_AMP_PROJECT_FACTORY_BOOT 1
#define CONFIG_CONSOLE_DISABLE_CLI 1
#define CONFIG_HUSH_PARSER 1
#define CONFIG_FIT 1
#define CONFIG_FIT_SIGNATURE 1
#define CONFIG_ARCH_ROCKCHIP 1
#define CONFIG_PREBOOT ""
#define CONFIG_OF_CONTROL 1
#define CONFIG_SYS_TEXT_BASE 0x40200000UL
#define CONFIG_BOOTDELAY 3
#define CONFIG_IS_ENABLED(x) 1
#define IMAGE_ENABLE_FIT 1
#define IMAGE_ENABLE_OF_LIBFDT 1
#define IMAGE_FORMAT_INVALID 0
#define IMAGE_FORMAT_LEGACY 1
#define IMAGE_FORMAT_FIT 2
#define CMD_RET_USAGE (-1)
#define CMD_RET_FAILURE 1
#define NR_DRAM_BANKS 2
#define CONFIG_NR_DRAM_BANKS 2
#define CHUNKSZ_CRC32 65536
#define BOOTSTAGE_ID_MAIN_LOOP 1
#define BOOTM_STATE_OS_PREP 1
#define BOOTM_STATE_OS_FAKE_GO 2
#define BOOTM_STATE_OS_GO 4
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define debug(...) do {} while (0)
#define uimage_to_cpu(x) __builtin_bswap32(x)
#define cpu_to_uimage(x) __builtin_bswap32(x)
#define putc(x) fputc((x), stdout)
#define getc() fake_getc()
struct bank {u64 start, size;};
struct bd {struct bank bi_dram[NR_DRAM_BANKS];};
struct gd_data {void *fdt_blob; struct bd *bd;};
static struct bd bd;
static struct gd_data global_data;
static struct gd_data *gd = &global_data;
static bool amp_project_policy_initialized, amp_project_factory_allowed;
static int checks, policy_result, policy_reads, reserved_result;
static int console_calls, preboot_calls, raw_boot_calls, fit_boot_calls, command_calls;
static int secure_cli_calls, ctrlc_calls, ctrlc_result, executed_script;
static int fit_conf_result, fit_verify_result = 1, fit_signature_calls;
static char last_command[256], fileaddr[64], filesize[64];
static char *bootcmd_override;
static uchar control[65536], original_control[65536];
static size_t original_control_size;
static uchar *script_buf;
static size_t script_allocated;
static jmp_buf stop;
static ulong ticks;
static int stored_bootdelay;
static void check(bool ok, const char *label) { checks++; if(!ok) {fprintf(stderr,"FAIL %d: %s\n",checks,label);exit(1);} }
static int fit_board_verify_required_sigs(void) { policy_reads++; return policy_result; }
static int amp_reserved_range_check(ulong a, ulong s) {(void)a;(void)s;return reserved_result;}
static ulong simple_strtoul(const char *s,char **end,unsigned int base) {return strtoul(s,end,base);}
static long simple_strtol(const char *s,char **end,unsigned int base) {return strtol(s,end,base);}
static const char *lookup_env(const char *key);
static char *env_get(const char *key) {return (char *)lookup_env(key);}
static int env_set_addr(const char *key,void *value) {(void)key;(void)value;return 0;}
static int run_command(const char *s,int flag) {(void)flag;command_calls++;snprintf(last_command,sizeof(last_command),"%s",s);if(!strcmp(s,"boot_fit"))fit_boot_calls++;return 0;}
static int run_command_list(const char *s,int len,int flag);
static void *map_sysmem(ulong addr,ulong len) {(void)len;return (void *)addr;}
static u32 crc32_wd(u32 seed,const uchar *data,ulong len,ulong chunk) {(void)chunk;return crc32(seed,data,len);}
static int fit_check_format(const void *f) {return fdt_check_header(f)==0;}
static int fit_image_get_node(const void *f,const char *name) {(void)f;(void)name;return 1;}
static int fit_image_check_type(const void *f,int off,int type) {(void)f;(void)off;(void)type;return 1;}
static int fit_conf_get_node(const void *f,const char *n) {(void)f;(void)n;return 1;}
static int fit_config_verify(const void *f,int off) {(void)f;(void)off;fit_signature_calls++;return fit_conf_result;}
static int fit_image_verify(const void *f,int off) {(void)f;(void)off;return fit_verify_result;}
static int fit_image_get_data(const void *f,int off,const void **data,size_t *len) {(void)off;*data=(const char *)f+256;*len=4;return 0;}
static void bootstage_mark_name(int n,const char *s) {(void)n;(void)s;}
static void cli_init(void) {}
static void parse_file_outer(void) {console_calls++;longjmp(stop,1);}
static void panic(const char *s) {(void)s;longjmp(stop,2);}
static void cli_secure_boot_cmd(const char *s) {(void)s;secure_cli_calls++;abort();}
static int fdtdec_get_config_int(const void *f,const char *n,int d) {int node=fdt_path_offset(f,"/config"),len;const fdt32_t *v;if(node<0)return d;v=fdt_getprop(f,node,n,&len);return v&&len==4?(int)fdt32_to_cpu(*v):d;}
static char *fdtdec_get_config_string(const void *f,const char *n) {int node=fdt_path_offset(f,"/config"),len;return node<0?NULL:(char *)fdt_getprop(f,node,n,&len);}
static void bootretry_init_cmd_timeout(void) {}
static int ctrlc(void) {ctrlc_calls++;return ctrlc_result;}
static int fake_getc(void) {return 3;}
static ulong get_timer(ulong base) {return ticks-base;}
static void udelay(ulong micros) {ticks+=micros/1000;}
static void autoboot_command_fail_handle(void) {}
static struct {struct {int os,arch;} os;} images;
static int booti_start(cmd_tbl_t *c,int f,int n,char *const a[],void *i) {(void)c;(void)f;(void)n;(void)a;(void)i;raw_boot_calls++;return 1;}
static void bootm_disable_interrupts(void) {}
static int do_bootm_states(cmd_tbl_t *c,int f,int n,char *const a[],int state,void *i,int boot) {(void)c;(void)f;(void)n;(void)a;(void)state;(void)i;(void)boot;return 1;}
'''

SUFFIX = r'''
static const char *lookup_env(const char *key)
{
    if(!strcmp(key,"fileaddr"))return fileaddr;
    if(!strcmp(key,"filesize"))return filesize;
    if(!strcmp(key,"preboot"))return "HOST_PREBOOT";
    if(!strcmp(key,"bootcmd")&&bootcmd_override)return bootcmd_override;
    const char *s=(const char *)compiled_default_environment;
    while(*s) {size_t k=strlen(key);if(!strncmp(s,key,k)&&s[k]=='=')return s+k+1;s+=strlen(s)+1;}
    return NULL;
}
static int run_command_list(const char *s,int len,int flag)
{
    (void)flag;
    if(len==-1&&!strcmp(s,"HOST_PREBOOT")) {preboot_calls++;return 0;}
    if(len==-1&&!strcmp(s,"amp_project_boot")) {char *args[]={"amp_project_boot"};return do_amp_project_boot(NULL,0,1,args);}
    if(len==-1&&!strcmp(s,"boot_fit"))return run_command(s,0);
    executed_script++;
    if((size_t)len==sizeof(factory_script_text)-1)check(!memcmp(s,factory_script_text,len),"factory script payload byte-for-byte equals boot.cmd");
    return 0;
}
static void reset_state(int result)
{
    check(fdt_open_into(original_control,control,sizeof(control))==0,"reset actual original control DT");
    gd->fdt_blob=control;gd->bd=&bd;
    amp_project_policy_initialized=amp_project_factory_allowed=false;
    policy_result=result;policy_reads=0;reserved_result=0;
    console_calls=preboot_calls=raw_boot_calls=fit_boot_calls=command_calls=0;
    secure_cli_calls=ctrlc_calls=ctrlc_result=executed_script=0;
    fit_conf_result=0;fit_verify_result=1;fit_signature_calls=0;
    bootcmd_override=NULL;last_command[0]=0;ticks=0;
    memset(&bd,0,sizeof(bd));bd.bi_dram[0].start=(ulong)script_buf;bd.bi_dram[0].size=script_allocated;
    memcpy(script_buf,factory_script_image,sizeof(factory_script_image));
    snprintf(fileaddr,sizeof(fileaddr),"%lx",(ulong)script_buf);
    snprintf(filesize,sizeof(filesize),"%zx",sizeof(factory_script_image));
}
static void init_allowed(void) {reset_state(0);amp_project_boot_policy_init();check(amp_project_factory_boot_allowed(),"exact policy zero opens gate");}
static void refresh_crc(image_header_t *h) {image_set_dcrc(h,crc32(0,(uchar *)h+sizeof(*h),image_get_size(h)));image_set_hcrc(h,0);image_set_hcrc(h,crc32(0,(uchar *)h,sizeof(*h)));}
static void rejects(const char *label) {int before=executed_script;check(source((ulong)script_buf,NULL)!=0,label);check(executed_script==before,"rejected script never dispatched");}
static void rejects_fit(const char *label) {int before=executed_script;check(source((ulong)script_buf,"script")!=0,label);check(executed_script==before,"rejected FIT script never dispatched");}
int main(void)
{
    script_allocated=131072;script_buf=calloc(1,script_allocated);check(script_buf!=NULL,"Host allocated synthetic DRAM");
    check(fdt_check_header(original_control)==0,"actual original control DT valid");
    int cfg=fdt_path_offset(original_control,"/config"),len;
    check(cfg>=0||cfg==-FDT_ERR_NOTFOUND,"actual original control DT /config lookup valid or absent");
    check(cfg==-FDT_ERR_NOTFOUND||(fdt_getprop(original_control,cfg,"bootcmd",&len)==NULL&&len==-FDT_ERR_NOTFOUND),"original control DT has no bootcmd override");
    check(cfg==-FDT_ERR_NOTFOUND||(fdt_getprop(original_control,cfg,"bootsecure",&len)==NULL&&len==-FDT_ERR_NOTFOUND),"original control DT has no bootsecure override");
    reset_state(0);
    check(!amp_project_factory_boot_allowed()&&!amp_project_console_allowed(),"default denial before init");
    check(!amp_project_source_range_valid((ulong)script_buf,4108),"source range denied before init");
    amp_project_boot_policy_init();check(policy_reads==1&&amp_project_factory_boot_allowed(),"one policy read grants exact zero");
    policy_result=1;amp_project_boot_policy_init();check(policy_reads==1&&amp_project_factory_boot_allowed(),"cached success cannot be recomputed during this boot");
    check(!strcmp(lookup_env("bootcmd"),"amp_project_boot"),"actual compiled default bootcmd is policy router");
    check(!strcmp(lookup_env("bootdelay"),"3"),"actual compiled default bootdelay is three seconds");
    check(strstr(lookup_env("distro_bootcmd"),"boot_targets")!=NULL,"compiled distro command dispatches boot targets");
    check(strstr(lookup_env("boot_a_script"),"load")&&strstr(lookup_env("boot_a_script"),"source"),"compiled distro script uses filesystem load and source");
    check(!strcmp(lookup_env("boot_scripts"),"boot.scr.uimg boot.scr"),"actual compiled boot scripts include original boot.scr");
    check(strstr(lookup_env("boot_targets"),"mmc0")!=NULL,"actual compiled boot targets include original eMMC");
    char *args[]={"amp_project_boot"};check(do_amp_project_boot(NULL,0,1,args)==0,"policy router allowed");
    check(!strcmp(last_command,"run distro_bootcmd"),"allowed router enters actual distro path");
    check(do_amp_project_boot(NULL,0,2,args)==CMD_RET_USAGE,"policy router arity enforced");
    init_allowed();check(genimg_get_format(script_buf)==IMAGE_FORMAT_INVALID,"global legacy format remains disabled");
    check(source((ulong)script_buf,NULL)==0&&executed_script==1,"actual source accepts original factory boot.scr");
    check(image_check_hcrc((image_header_t *)script_buf)&&image_check_dcrc((image_header_t *)script_buf),"actual factory header and payload CRC routines pass");
    init_allowed();script_buf[4]^=1;rejects("bad header CRC rejected");
    init_allowed();script_buf[100]^=1;rejects("bad payload CRC rejected even verify=no");
    init_allowed();((image_header_t *)script_buf)->ih_type=IH_TYPE_KERNEL;refresh_crc((image_header_t *)script_buf);rejects("legacy kernel not accepted as script");
    check(genimg_get_format(script_buf)==IMAGE_FORMAT_INVALID,"CRC-valid legacy kernel remains globally invalid");
    init_allowed();((image_header_t *)script_buf)->ih_comp=IH_COMP_GZIP;refresh_crc((image_header_t *)script_buf);rejects("compressed script rejected");
    init_allowed();((image_header_t *)script_buf)->ih_arch=IH_ARCH_ARM64;refresh_crc((image_header_t *)script_buf);rejects("non-factory architecture rejected");
    init_allowed();snprintf(filesize,sizeof(filesize),"40");rejects("truncated payload bounded before CRC");
    init_allowed();snprintf(filesize,sizeof(filesize),"%zx",sizeof(factory_script_image)+1);rejects("trailing bytes rejected");
    init_allowed();image_set_size((image_header_t *)script_buf,0xffffffff);image_set_hcrc((image_header_t *)script_buf,0);image_set_hcrc((image_header_t *)script_buf,crc32(0,script_buf,64));rejects("overflow payload length rejected before CRC");
    init_allowed();((u32 *)(script_buf+64))[1]=1;refresh_crc((image_header_t *)script_buf);rejects("multi-component/nonzero terminator rejected");
    init_allowed();((u32 *)(script_buf+64))[0]=0;refresh_crc((image_header_t *)script_buf);rejects("zero-length script rejected");
    init_allowed();((u32 *)(script_buf+64))[0]=cpu_to_uimage(4);refresh_crc((image_header_t *)script_buf);rejects("component length mismatch rejected");
    init_allowed();snprintf(fileaddr,sizeof(fileaddr),"%lx",(ulong)script_buf+4);rejects("fileaddr mismatch rejected");
    init_allowed();snprintf(filesize,sizeof(filesize),"-10");rejects("negative metadata rejected");
    init_allowed();snprintf(filesize,sizeof(filesize),"100g");rejects("nonhex metadata rejected");
    init_allowed();snprintf(filesize,sizeof(filesize),"10000000000000000");rejects("overflow metadata rejected");
    init_allowed();bd.bi_dram[0].size=63;rejects("image outside DRAM rejected");
    init_allowed();reserved_result=-1;rejects("image touching AMP reserved area rejected");
    init_allowed();check(!amp_project_source_range_valid(~0UL-10,64),"range wrap rejected");
    init_allowed();snprintf(fileaddr,sizeof(fileaddr),"%lx",(ulong)script_buf+1);check(source((ulong)script_buf+1,NULL)!=0&&executed_script==0,"unaligned source rejected before header read");
    init_allowed();check(fdt_create_empty_tree(script_buf,4096)==0,"Host synthetic FIT-shaped blob");snprintf(filesize,sizeof(filesize),"1000");
    fit_conf_result=-1;rejects_fit("FIT source required signature rejection preserved");check(fit_signature_calls==1,"FIT signature verifier actually invoked");
    init_allowed();check(fdt_create_empty_tree(script_buf,4096)==0,"reset synthetic FIT-shaped blob");snprintf(filesize,sizeof(filesize),"1000");fit_verify_result=0;rejects_fit("FIT source payload verifier rejection preserved");
    for(int p=0;p<4;p++) {
        int results[]={1,2,-1,-EIO};reset_state(results[p]);bootcmd_override="HOST_RAW_COMMAND";amp_project_boot_policy_init();
        check(!amp_project_factory_boot_allowed()&&!amp_project_console_allowed(),"nonzero/error policy never authorizes console");
        check(policy_reads==1,"denied policy read exactly once");policy_result=0;amp_project_boot_policy_init();check(!amp_project_factory_boot_allowed()&&policy_reads==1,"cached refusal cannot reopen by retry");
        rejects("denied policy blocks source");check(do_booti(NULL,0,1,args)==CMD_RET_FAILURE&&raw_boot_calls==0,"denied policy blocks raw booti before first access");
        check(do_amp_project_boot(NULL,0,1,args)==0&&!strcmp(last_command,"boot_fit"),"denied router retains FIT branch");
        ctrlc_result=1;check(__abortboot(3)==0&&ctrlc_calls==0,"closed autoboot ignores CTRL+C");
        autoboot_command("HOST_RAW_COMMAND");check(!strcmp(last_command,"boot_fit"),"closed autoboot cannot dispatch env command");
        check(setjmp(stop)==0? (cli_loop(),console_calls==0): false,"closed CLI returns without hush loop");
    }
    for(int kind=0;kind<2;kind++) {
        reset_state(0);cfg=fdt_path_offset(control,"/config");if(cfg==-FDT_ERR_NOTFOUND)cfg=fdt_add_subnode(control,0,"config");check(cfg>=0,"Host creates config node for override fault");
        check(fdt_setprop_string(control,cfg,kind?"bootsecure":"bootcmd",kind?"":"booti")==0,"inject DT override");amp_project_boot_policy_init();
        check(!amp_project_factory_boot_allowed()&&policy_reads==0,"DT command/secure override rejected before policy request");
    }
    reset_state(0);control[0]=0;amp_project_boot_policy_init();check(!amp_project_factory_boot_allowed()&&policy_reads==0,"invalid control DT denied");
    init_allowed();ctrlc_result=1;check(__abortboot(3)==1&&ctrlc_calls==1,"allowed console CTRL+C stops countdown");
    check(do_booti(NULL,0,1,args)==1&&raw_boot_calls==1,"allowed booti reaches actual booti_start boundary");
    for(int p=0;p<3;p++) {
        int results[]={0,1,-EIO};reset_state(results[p]);
        int stop_code=setjmp(stop);if(!stop_code)main_loop();
        if(!p) {check(stop_code==1&&console_calls==1,"allowed main enters real CLI function");check(preboot_calls==1,"allowed main may run preboot");check(!strcmp(last_command,"run distro_bootcmd"),"allowed main dispatches distro router");}
        else {check(stop_code==2&&console_calls==0,"closed main terminates without console");check(preboot_calls==0&&secure_cli_calls==0,"closed main skips preboot and DT secure override");check(fit_boot_calls==1&&!strcmp(last_command,"boot_fit"),"closed main invokes only FIT boot");}
    }
    free(script_buf);printf("P028_ACTUAL_C_CHECKS=%d\n",checks);return 0;
}
'''


def array(name, data):
    return "static const unsigned char " + name + "[]={" + ",".join(str(i) for i in data) + "};\n"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--source",type=Path,required=True)
    p.add_argument("--build",type=Path,required=True)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--factory-control",type=Path,required=True)
    p.add_argument("--boot-scr",type=Path,required=True)
    p.add_argument("--boot-cmd",type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    read=lambda s:(a.source/s).read_text()
    image_header=read("include/image.h")
    enums="\n".join(re.findall(r"enum \{[^{}]+\};",image_header[:image_header.index("#define IH_MAGIC")]))
    hdr=re.search(r"typedef struct image_header \{.*?\} image_header_t;",image_header,re.S).group()
    getters=image_header[image_header.index("static inline uint32_t image_get_header_size"):image_header.index("static inline int image_check_os")]+extract(image_header,"image_check_os")
    image_prefix=enums+'\n#define IH_MAGIC 0x27051956\n#define IH_NMLEN 32\n#define USE_HOSTCC 1\n'+hdr+'\n'+getters
    policy=read("common/amp_project_boot.c")
    main_c=read("common/main.c");cli=read("common/cli.c");autoboot=read("common/autoboot.c");source=read("cmd/source.c");image=read("common/image.c")
    c=PRELUDE+array("compiled_default_environment",elf_symbol(a.build/"u-boot","default_environment"))+array("factory_script_image",a.boot_scr.read_bytes())+array("factory_script_text",a.boot_cmd.read_bytes()+b"\0")
    c+=image_prefix+'\n'+extract(image,"image_check_hcrc")+'\n'+extract(image,"image_check_dcrc")+'\n'+extract(image,"genimg_get_format")+'\n'
    c+='\n'.join(extract(policy,n) for n in ["amp_project_boot_policy_init","amp_project_factory_boot_allowed","amp_project_console_allowed","amp_project_source_range_valid","do_amp_project_boot"])
    c+='\n'+'\n'.join(extract(source,n) for n in ["amp_project_loaded_script_bounds","amp_project_source_legacy_script","source"])
    # Preprocess actual conditional branches before extracting __abortboot:
    # its #if alternatives intentionally contain unmatched braces as raw text.
    defines='\n'.join(re.findall(r'^#define.*$',PRELUDE,re.M))
    selected=subprocess.run(["cc","-E","-P","-x","c","-"],input=defines+'\n'+re.sub(r'^#include[^\n]*','',autoboot,flags=re.M),capture_output=True,text=True,check=True).stdout
    c+='\n'+extract(selected,"__abortboot")+'\n'+extract(selected,"abortboot")
    c+='\n#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored "-Wunused-parameter"\n'+extract(selected,"process_fdt_options")+'\n#pragma GCC diagnostic pop\n'
    c+='\n'+'\n'.join(extract(selected,n) for n in ["bootdelay_process","autoboot_command"])
    c+='\n'+extract(cli,"cli_process_fdt")+'\n'+extract(cli,"cli_loop")+'\n'+extract(main_c,"run_preboot_environment_command")+'\n'+extract(main_c,"main_loop")+'\n'+extract(read("cmd/booti.c"),"do_booti")+'\n'+SUFFIX
    original=a.factory_control.read_bytes();c=c.replace("static uchar control[65536], original_control[65536];",'static uchar control[65536];\n'+array("original_control",original))
    c=c.replace("static size_t original_control_size;", "")
    src=a.output/"actual-c-regression.c";src.write_text(c)
    dtc=a.source/"scripts/dtc/libfdt";objects=[]
    for name in ["fdt.c","fdt_ro.c","fdt_wip.c","fdt_rw.c","fdt_sw.c","fdt_empty_tree.c"]:
        obj=a.output/(name+".o");r=subprocess.run(["cc","-std=c11","-I",str(dtc),"-c",str(dtc/name),"-o",str(obj)],capture_output=True,text=True);assert r.returncode==0,r.stderr;objects.append(str(obj))
    command=["cc","-std=c11","-Wall","-Wextra","-Werror","-I",str(dtc),str(src),*objects,"-lz","-o",str(a.output/"actual-c-regression")]
    build=subprocess.run(command,capture_output=True,text=True);(a.output/"compile.log").write_text(build.stdout+build.stderr);assert build.returncode==0,build.stderr
    run=subprocess.run([str(a.output/"actual-c-regression")],capture_output=True,text=True);(a.output/"test.log").write_text(run.stdout+run.stderr);assert run.returncode==0,run.stderr
    report={"evidence":"HOST_TESTED","board_access":False,"firmware_executed":False,"policy_and_hardware_boundaries":"FAULT_STUBS; actual factory control DT uses real vendored libfdt","source_parser":"Actual source(), private bounded SCRIPT parser, actual header/payload CRC; command execution captured, hardware script commands not executed","default_environment":"Extracted byte-for-byte from actual target ELF symbol and traversed in compiled Host C","actual_functions":["amp_project_boot_policy_init","source","image_check_hcrc","image_check_dcrc","genimg_get_format","main_loop","cli_loop","__abortboot","bootdelay_process","autoboot_command","do_booti"],"compiler_exit":build.returncode,"test_exit":run.returncode,"checks":int(re.search(r"P028_ACTUAL_C_CHECKS=(\d+)",run.stdout).group(1)),"files":{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [a.build/"u-boot",a.factory_control,a.boot_scr,a.boot_cmd,src]},"compile_command":command,"stdout":run.stdout}
    (a.output/"result.json").write_text(json.dumps(report,indent=2)+"\n");print(json.dumps(report,indent=2))


if __name__=="__main__":main()
