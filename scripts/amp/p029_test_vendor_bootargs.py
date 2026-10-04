#!/usr/bin/env python3
"""Host regression of the pinned Rockchip DT/environment bootargs merger."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

from p026_test_preload import extract

PIN = "f8b4554584dd475ce783c605850c5e883b0a0fd4"

PRELUDE = r'''
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <libfdt.h>
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define debug(...) ((void)0)
#define GD_FLG_ENV_READY 1
#define GD_FLG_DISABLE_CONSOLE 2
#define HK_CMDLINE 0
#define HK_INITCALL 0
static struct {int flags;} global_data={GD_FLG_ENV_READY};
static __typeof__(global_data) *gd=&global_data;
static char *args, *extension;
static char *env_get(const char *name) {
    if(!strcmp(name,"bootargs")) return args;
    if(!strcmp(name,"bootargs_ext")) return extension;
    return NULL;
}
static int env_set(const char *name,const char *value) {
    char **target=!strcmp(name,"bootargs") ? &args : &extension;
    char *copy=value ? strdup(value):NULL;
    if(value && !copy) abort();
    free(*target);*target=copy;return 0;
}
static int env_delete(const char *name,const char *value,int complete) {
    (void)name;(void)complete;
    /* No initrd= or disabled-console earlycon deletion in these fixtures. */
    if(args && strstr(args,value)) abort();return 0;
}
static int is_hotkey(int key) {(void)key;return 0;}
static void hotkey_run(int key) {(void)key;}
static int fdt_find_or_add_subnode(void *fdt,int parent,const char *name) {
    int node=fdt_subnode_offset(fdt,parent,name);
    return node>=0 ? node:fdt_add_subnode(fdt,parent,name);
}
/* Tail services are stubbed; separate source/config audit establishes no root
 * rewrite for raw booti, mmc, ANDROID_AB=n, ENVF=n, ENV_PARTITION=n. */
static void bootargs_add_partition(bool v) {(void)v;}
static void bootargs_add_fwver(bool v) {(void)v;}
static void bootargs_add_android(bool v) {(void)v;}
char *env_exist(const char *,const char *);
'''

MAIN = r'''
static int checks;
static void check(int ok,const char *what) {
    checks++;if(!ok){fprintf(stderr,"FAIL %s\n",what);exit(1);}
}
static int count_root(const char *line) {
    char *copy=strdup(line),*save=NULL;int count=0;
    for(char *p=strtok_r(copy," ",&save);p;p=strtok_r(NULL," ",&save))
        if(!strncmp(p,"root=",5)) count++;
    free(copy);return count;
}
int main(int argc,char **argv) {
    check(argc==5,"DT plus three generated command lines");
    for(int stage=0;stage<3;stage++) {
        FILE *file=fopen(argv[1],"rb");check(file!=NULL,"open actual DT");
        unsigned char input[1048576],tree[1048576];
        size_t n=fread(input,1,sizeof(input),file);fclose(file);
        check(n>0 && !fdt_check_header(input),"actual DT header");
        check(!fdt_open_into(input,tree,sizeof(tree)),"actual libfdt copy");
        int chosen=fdt_path_offset(tree,"/chosen");check(chosen>=0,"chosen");
        const char *original=fdt_getprop(tree,chosen,"bootargs",NULL);
        check(original && strstr(original,"root=PARTUUID=614e0000-0000"),"actual inherited short UUID");
        env_set("bootargs",argv[stage+2]);env_set("bootargs_ext",NULL);
        char *old=board_fdt_chosen_bootargs(tree);
        check(strstr(old,"root=PARTUUID=614e0000-0000") && !strstr(old,"root=/dev/mmcblk0p3"),"reproduce DT root override");
        for(int hostile=0;hostile<2;hostile++) {
            check(!fdt_setprop_string(tree,chosen,"bootargs_ext",hostile ? "root=/dev/unexpected":""),"DT extension fixture");
            env_set("bootargs",argv[stage+2]);env_set("bootargs_ext","root=/dev/mmcblk0p3");
            char *merged=board_fdt_chosen_bootargs(tree);
            check(count_root(merged)==1,"exactly one root");
            check(strstr(merged,"root=/dev/mmcblk0p3")!=NULL,"environment extension wins");
            check(!strstr(merged,"PARTUUID=") && !strstr(merged,"/dev/unexpected"),"all DT root overrides removed");
            char tag[32];snprintf(tag,sizeof(tag),"amp_test_stage=%c",'A'+stage);
            check(strstr(merged,tag)!=NULL,"stage tag retained");
            check(strstr(merged,"rootfstype=ext4") && strstr(merged,"rootwait"),"root options retained");
            if(stage==2) check(strstr(merged,"dyndbg=\"file virtio_rpmsg_bus.c +p\"")!=NULL,"quoted C diagnostic preserved");
        }
    }
    free(args);free(extension);
    printf("VENDOR_BOOTARGS_CHECKS=%d\n",checks);
}
'''


def test(source, packet, report):
    head = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    assert head == PIN
    names = ["cmd/nvedit.c", "arch/arm/mach-rockchip/board.c"]
    assert not subprocess.check_output(["git", "-C", str(source), "diff", "HEAD", "--", *names], text=True)
    text = {name: (source/name).read_text() for name in names}
    blocks = [extract(text[names[0]], name) for name in ["env_exist", "env_append", "env_replace", "env_update_filter", "env_update"]]
    blocks.insert(3, "#define ARGS_ITEM_NUM 50")
    blocks += [extract(text[names[1]], name) for name in ["bootargs_add_dtb_dtbo", "board_fdt_chosen_bootargs"]]
    cmdlines = []
    for stage in "ABC":
        cmd = (packet/f"stage-{stage}.cmd").read_text()
        match = re.search(r"if setenv bootargs '([^\n]+)'; then", cmd)
        assert match
        assert "if setenv bootargs_ext 'root=/dev/mmcblk0p3'; then" in cmd
        assert cmd.index("if setenv bootargs_ext") < cmd.index("amp_m0load" if stage != "A" else "booti ")
        cmdlines.append(match[1])
    with tempfile.TemporaryDirectory() as temporary:
        tmp = Path(temporary); c = tmp/"test.c"; c.write_text(PRELUDE+"\n"+"\n".join(blocks)+"\n"+MAIN)
        library = source/"scripts/dtc/libfdt"
        objects = []
        for name in ["fdt", "fdt_ro", "fdt_wip", "fdt_sw", "fdt_rw", "fdt_strerror", "fdt_empty_tree", "fdt_addresses"]:
            obj = tmp/(name+".o")
            subprocess.run(["cc", "-I", str(library), "-c", str(library/(name+".c")), "-o", str(obj)], check=True, capture_output=True, text=True)
            objects.append(obj)
        build = subprocess.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare", "-Wno-unused-parameter", "-Wno-misleading-indentation", "-I", str(library), str(c), *map(str, objects), "-o", str(tmp/"test")], capture_output=True, text=True)
        assert build.returncode == 0, build.stderr
        output = subprocess.check_output([str(tmp/"test"), str(packet/"boot/stage-A.dtb"), *cmdlines], text=True).strip()
    result = {"evidence": "HOST_TESTED_ACTUAL_VENDOR_C", "source_commit": head,
              "source_sha256": {name: hashlib.sha256((source/name).read_bytes()).hexdigest() for name in names},
              "result": output, "merger": "actual board DT/env merger and env_update, actual vendored libfdt; environment and tail services stubbed",
              "scope": "root precedence, hostile DT extension, stage tags and quoted C parameter; no firmware/HUSH or initrd execution",
              "board_access": False}
    report.write_text(json.dumps(result, indent=2)+"\n"); print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ["source", "packet", "report"]: parser.add_argument("--"+name, type=Path, required=True)
    args = parser.parse_args(); test(args.source, args.packet, args.report)
