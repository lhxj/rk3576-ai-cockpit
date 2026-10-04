#!/usr/bin/env python3
"""Compile the pinned vendor size/load/env/test C on Host; no HUSH or MMIO."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def function(text, name):
    match = re.search(r"^(?:static )?(?:int|ulong) " + name + r"\([^;]*?\n\{", text, re.M)
    assert match, name
    pos = text.index("{", match.start())
    depth = 1
    end = pos + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[match.start():end]


STUBS = r'''
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define loff_t unsigned long long
typedef unsigned long ulong;
typedef int cmd_tbl_t;
#define CMD_RET_USAGE -1
#define CMD_RET_FAILURE 1
#define CONFIG_SYS_LOAD_ADDR 0x40400000UL
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define debug(...) ((void)0)
static char filesize[32], fileaddr[32];
static int fail_env, fail_size, fail_read;
static loff_t actual_size, short_by, last_count;
static int env_set(const char *name, const char *value) {
    char *dest = !strcmp(name,"filesize") ? filesize : fileaddr;
    if (fail_env) return 1;
    snprintf(dest,32,"%s",value ? value : ""); return 0;
}
static const char *env_get(const char *name) {
    return !strcmp(name,"filesize") ? filesize : NULL;
}
static ulong simple_strtoul(const char *s,char **end,unsigned int base) { return strtoul(s,end,base); }
static long simple_strtol(const char *s,char **end,unsigned int base) { return strtol(s,end,base); }
static int fs_set_blk_dev(const char *a,const char *b,int type) {(void)a;(void)b;(void)type;return 0;}
static int fs_size(const char *name,loff_t *size) {(void)name;*size=actual_size;return fail_size ? -1:0;}
static int fs_read(const char *name,ulong addr,loff_t pos,loff_t count,loff_t *read) {
    (void)name;(void)addr;(void)pos;last_count=count;
    *read=(count && count<actual_size ? count:actual_size)-short_by;
    return fail_read ? -1:0;
}
static ulong get_timer(ulong base) {(void)base;return 0;}
static void print_size(uint64_t size,const char *unit) {(void)size;(void)unit;}
static uint64_t div_u64(uint64_t value,uint64_t divisor) { return value/divisor; }
static int file_exists(const char *a,const char *b,const char *c,int type) {(void)a;(void)b;(void)c;(void)type;return 0;}
#define FS_TYPE_ANY 0
'''

MAIN = r'''
static int checks;
static void check(int value,const char *what) {checks++;if(!value){fprintf(stderr,"FAIL %s\n",what);exit(1);}}
static int compare(const char *op,const char *a,const char *b) {
    char *args[]={"test",(char *)a,(char *)op,(char *)b,NULL};
    return do_test(NULL,0,4,args);
}
static int size_cmd(void) {
    char *args[]={"size","mmc","0:2","/amp-p029/file",NULL};
    return do_size(NULL,0,4,args,0);
}
static int load_cmd(const char *count) {
    char *args[]={"load","mmc","0:2","0x40400000","/amp-p029/file",(char *)count,NULL};
    return do_load(NULL,0,6,args,0);
}
int main(void) {
    for(unsigned int i=0;i<ARRAY_SIZE(cases);i++) {
        const struct input *p=&cases[i]; actual_size=p->size;
        fail_env=fail_size=fail_read=0;short_by=0;
        check(!env_set("filesize",NULL),"clear before size");
        check(!size_cmd(),"actual size function success");
        check(!strcmp(filesize,p->expected),"vendor 0x filesize matches generated precheck");
        check(!compare("=",filesize,p->expected),"actual string test accepts generated length");
        check(compare("=",filesize,p->count),"old bare-hex precheck rejected");
        check(!env_set("filesize",NULL),"clear before load");
        check(!load_cmd(p->count),"actual bounded load success");
        check(last_count==p->size,"bare count parsed with base16");
        check(!compare("=",filesize,p->expected),"actual readback length matches generated guard");
        check(!strcmp(fileaddr,"0x40400000"),"actual fileaddr formatter");
        actual_size=p->size+1;check(!size_cmd() && compare("=",filesize,p->expected),"oversize rejected before load");
        actual_size=p->size-1;check(!size_cmd() && compare("=",filesize,p->expected),"undersize rejected before load");
        actual_size=p->size;short_by=1;
        check(!load_cmd(p->count) && compare("=",filesize,p->expected),"successful short read rejected");
        short_by=0;check(!env_set("filesize",p->expected),"seed stale length");
        check(!env_set("filesize",NULL),"clear stale length");fail_env=1;
        check(!size_cmd() && compare("=",filesize,p->expected),"ignored env_set_hex failure fails closed after clear");
        check(!load_cmd(p->count) && compare("=",filesize,p->expected),"load env failure fails closed after clear");
        fail_env=0;fail_size=1;check(size_cmd()!=0,"size failure propagated");
        fail_size=0;fail_read=1;check(load_cmd(p->count)!=0,"read failure propagated");
        fail_read=0;
    }
    check(!compare("-eq","0x1","0x2930200"),"decimal test unsafe: different hex values compare equal");
    check(compare("=","0x1","0x2930200"),"string comparison rejects different length");
    check(compare("=","","0x2930200"),"empty value rejected");
    check(compare("=","0x2930200z","0x2930200"),"trailing junk rejected");
    printf("VENDOR_FILESIZE_CHECKS=%d\n",checks);
}
'''


def test(source, packet, report):
    source = source.resolve(); packet = packet.resolve()
    head = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    assert head == "f8b4554584dd475ce783c605850c5e883b0a0fd4", head
    files = {name: (source/name).read_text() for name in ["cmd/nvedit.c", "fs/fs.c", "cmd/test.c"]}
    assert not subprocess.check_output(["git", "-C", str(source), "diff", "HEAD", "--", *files], text=True)
    operators = files["cmd/test.c"].split("#define OP_INVALID",1)[1].split("static int do_test",1)[0]
    blocks = [function(files["cmd/nvedit.c"], "env_set_hex"),
              function(files["fs/fs.c"], "do_size"), function(files["fs/fs.c"], "do_load"),
              "#define OP_INVALID"+operators, function(files["cmd/test.c"], "do_test")]
    table = []
    for stage in "ABC":
        text = (packet/f"stage-{stage}.cmd").read_text()
        expected = re.findall(r'if test "\$\{filesize\}" = "([^"]+)";', text)
        assert len(expected) == 6, stage
        assert text.count("if setenv filesize; then") == 6, stage
        for index, name in enumerate(["Image", "initrd", f"stage-{stage}.dtb"]):
            size = (packet/"boot"/name).stat().st_size
            assert expected[2*index:2*index+2] == [hex(size)]*2
            table.append(f'{{{size}ULL,"{expected[2*index]}","{size:x}"}}')
    c = STUBS+"\n"+"\n".join(blocks)+"\nstruct input {loff_t size;const char *expected,*count;};\nstatic const struct input cases[]={"+",".join(table)+"};\n"+MAIN
    with tempfile.TemporaryDirectory() as tmp:
        p = Path(tmp); (p/"test.c").write_text(c)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-Wno-sign-compare", str(p/"test.c"), "-o", str(p/"test")], check=True, capture_output=True, text=True)
        output = subprocess.check_output([str(p/"test")], text=True)
    result = {"evidence": "HOST_TESTED_ACTUAL_VENDOR_C", "source_commit": head,
              "source_sha256": {name: hashlib.sha256((source/name).read_bytes()).hexdigest() for name in files},
              "result": output.splitlines()[-1], "generated_guard_inputs": len(table),
              "board_access": False, "HUSH_executed": False, "MMIO_or_firmware_execution": False}
    report.write_text(json.dumps(result, indent=2)+"\n"); print(json.dumps(result, indent=2))


if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("--source", type=Path, required=True)
    ap.add_argument("--packet", type=Path, required=True); ap.add_argument("--report", type=Path, required=True)
    args = ap.parse_args(); test(args.source, args.packet, args.report)
