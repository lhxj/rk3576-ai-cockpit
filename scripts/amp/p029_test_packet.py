#!/usr/bin/env python3
"""Host fault tests of actual generated scripts and M0 task control flow.

Bash exercises the shared shell subset with fault commands; no U-Boot execution.
Actual M0 C is compiled for Host with HAL/RTOS/RPMsg stubs, never real MMIO.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def test_snapshot(packet):
    """Exercise the actual snapshot block in Host temp dirs, never root install."""
    installer = (packet/"stage-assets.sh").read_text()
    snapshot = installer.split("python3 - \"$source_packet\" \"$packet\" <<'PY'\n", 1)[1].split("\nPY\n", 1)[0]
    names = [line.split("  ", 1)[1] for line in (packet/"SHA256SUMS").read_text().splitlines()] + ["SHA256SUMS"]
    cases = 0
    for fault in ["none", "symlink", "fifo", "oversize", "destination_exists", "later_source_change"]:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); source = root/"source"; dest = root/"snapshot"
            source.mkdir(); dest.mkdir()
            for name in names:
                path = source/name; path.parent.mkdir(exist_ok=True)
                path.write_bytes(("original:"+name).encode())
            leaf = source/"MANIFEST.json"
            if fault in ["symlink", "fifo"]:
                leaf.unlink()
                if fault == "symlink":
                    leaf.symlink_to(source/"README.md")
                else:
                    os.mkfifo(leaf)
            elif fault == "oversize":
                with leaf.open("wb") as file:
                    file.truncate(128*1024*1024+1)
            elif fault == "destination_exists":
                (dest/"MANIFEST.json").write_bytes(b"do not overwrite")
            r = subprocess.run(["python3", "-c", snapshot, str(source), str(dest)],
                               capture_output=True, text=True, timeout=3)
            if fault in ["none", "later_source_change"]:
                assert r.returncode == 0, r.stderr
                if fault == "later_source_change":
                    leaf.write_bytes(b"changed after snapshot")
                for name in names:
                    assert (dest/name).read_bytes() == ("original:"+name).encode()
            else:
                assert r.returncode != 0, fault
                if fault == "destination_exists":
                    assert (dest/"MANIFEST.json").read_bytes() == b"do not overwrite"
            cases += 1
    return cases

SHELL = r'''
part() { test "$fault" != part || return 1; p029_part=2; test "$fault" != wrong_part || p029_part=3; }
setenv() {
    test "$fault" != set_$1 || return 1
    if test "$1" = filesize; then
        clears=$((clears+1)); test "$fault" != clear_$clears || return 1
    fi
    export "$1=${2-}"
}
size() {
    name=${3##*/}; index=$((index+1));
    test "$fault" != size_$index || return 1
    case "$name" in
      Image) filesize=0xIMAGE_SIZE;; initrd) filesize=0xINITRD_SIZE;; stage-?.dtb) filesize=0xDT_SIZE;; *) return 1;;
    esac
    test "$fault" != length_$index || filesize=1
    test "$fault" != env_size_$index || filesize=
    test "$fault" != malformed_$index || filesize=${filesize}z
}
load() {
    loads=$((loads+1)); test "$fault" != load_$loads || return 1
    # Match vendor env_set_hex rather than echoing the load count argument.
    printf -v filesize '0x%x' "$((16#${5#0x}))"
    test "$fault" != short_$loads || filesize=0x1
    test "$fault" != env_load_$loads || filesize=
}
amp_m0load() {
    test "$loads" = 3 || { echo BAD_ORDER; exit 99; }
    test "$1" = /amp-p029/amp-host.itb && test "$2" = 0x48300000 || exit 99
    m0=$((m0+1)); echo M0_CALLED
    test "$fault" != m0
}
booti() {
    test "$loads" = 3 && test "$1" = 0x40400000 && test "$3" = 0x48300000 || exit 99
    test "$bootargs_ext" = 'root=/dev/mmcblk0p3' || exit 99
    echo BOOTI_CALLED
    exit 0
}
index=0; loads=0; m0=0; clears=0
'''

C_STUBS = r'''
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef unsigned int rt_tick_t;
typedef int rt_bool_t;
#define RT_TRUE 1
#define RT_FALSE 0
#define RT_NULL NULL
#define RL_NULL NULL
#define RL_TRUE 1
#define RL_NO_FLAGS 0
#define RL_NS_CREATE 0
#define RL_SUCCESS 0
#define RL_BUFFER_PAYLOAD_SIZE 496U
#define AMP_LINK_ID 4U
#define AMP_SERVICE_NAME "rk3576-m0-echo"
#define RPMSG_LINUX_MEM_BASE 0x27d00000U
#define AMP_POOL_LINUX_PA 0x47d10000U
#define AMP_POOL_SIZE 0x10000U
#define DCACHE_CACHE_CTRL_CACHE_BYPASS_MASK 64U
struct cache { volatile uint32_t CACHE_CTRL; } cache;
#define DCACHE (&cache)
struct rpmsg_lite_instance { int v; } instance;
struct rpmsg_lite_endpoint { int v; } endpoint;
typedef void *rpmsg_queue_handle;
static unsigned int tick, init_calls, free_calls, sends, rx_calls, cleanup_calls;
static int fault, completed;
static char hello[]="HELLO", ping[]="PING", unknown[]="WHAT";
static rt_tick_t rt_tick_get(void) { return tick; }
static rt_tick_t rt_tick_from_millisecond(unsigned int ms) { return ms; }
static void rt_thread_mdelay(unsigned int ms) { tick+=ms; }
#define rt_kprintf(...) do { if (0) printf(__VA_ARGS__); } while (0)
static uint32_t amp_pool_m0_to_pa(uintptr_t ptr) {
    (void)ptr; return fault==6 ? 0 : (fault==7 ? AMP_POOL_LINUX_PA+8 : AMP_POOL_LINUX_PA+16);
}
static struct rpmsg_lite_instance *rpmsg_lite_remote_init(void *p,unsigned int l,unsigned int f) {
    (void)p;(void)l;(void)f; init_calls++; return fault==1 ? NULL : &instance;
}
static int rpmsg_lite_is_link_up(struct rpmsg_lite_instance *p) { (void)p; return fault==2 ? 0 : 1; }
static rpmsg_queue_handle rpmsg_queue_create(struct rpmsg_lite_instance *p) { (void)p;return fault==3 ? NULL : &instance; }
static int32_t rpmsg_queue_rx_cb(void *p,uint32_t l,uint32_t s,void *q) { (void)p;(void)l;(void)s;(void)q; return 0; }
static struct rpmsg_lite_endpoint *rpmsg_lite_create_ept(struct rpmsg_lite_instance *p,unsigned int n,
      int32_t (*cb)(void *,uint32_t,uint32_t,void *),void *q) {
    (void)p;(void)n;(void)cb;(void)q; return fault==4 ? NULL : &endpoint;
}
static int rpmsg_ns_announce(struct rpmsg_lite_instance *p,struct rpmsg_lite_endpoint *e,const char *s,int c) {
    (void)p;(void)e;(void)s;(void)c;return fault==5 ? 1 : 0;
}
static int rpmsg_queue_recv_nocopy(struct rpmsg_lite_instance *p,void *q,uint32_t *s,char **m,uint32_t *len,unsigned int ms) {
    (void)p;(void)q;tick+=ms;rx_calls++;if (fault==11) return 1;
    *s=0x400;*m=rx_calls==1 ? hello:ping;*len=rx_calls==1 ? 5:4;
    if (fault==8) {*m=unknown;*len=4;} if (fault==12) *len=497;
    return 0;
}
static int rpmsg_queue_nocopy_free(struct rpmsg_lite_instance *p,void *m) {
    (void)p;(void)m;free_calls++;return fault==9 ? 1:0;
}
static int rpmsg_lite_send(struct rpmsg_lite_instance *p,struct rpmsg_lite_endpoint *e,uint32_t s,char *m,uint32_t n,unsigned int ms) {
    (void)p;(void)e;(void)s;(void)ms;sends++;
    if (fault==10) return 1;
    if(n==4 && !memcmp(m,"PONG",4)) completed=1;
    return 0;
}
static void rpmsg_lite_destroy_ept(struct rpmsg_lite_instance *p,struct rpmsg_lite_endpoint *e) {(void)p;(void)e;}
static void rpmsg_queue_destroy(struct rpmsg_lite_instance *p,void *q) {(void)p;(void)q;}
static void rpmsg_lite_deinit(struct rpmsg_lite_instance *p) {(void)p;cleanup_calls++;}
'''

C_TESTS = r'''
static int cases;
static void check(int value,const char *what) { cases++;if(!value) {fprintf(stderr,"FAIL %s\n",what);exit(1);} }
static void run(int f) {
    tick=init_calls=free_calls=sends=rx_calls=cleanup_calls=completed=0;
    fault=f;cache.CACHE_CTRL=f==-1 ? 0:64;amp_echo_run();
}
int main(void) {
    run(-1);check(!init_calls && !free_calls && !sends,"cache absent never shared init");
    run(1);check(init_calls==1 && !sends,"init failure");
    run(2);check(tick==15000 && cleanup_calls==1 && !rx_calls,"link timeout bounded");
    for(int f=3;f<=5;f++) {run(f);check(!rx_calls && cleanup_calls==1,"endpoint setup failure");}
    for(int f=6;f<=7;f++) {run(f);check(rx_calls==1 && !free_calls && !sends,"bad pointer/header never free/dereference");}
    run(12);check(!free_calls && !sends,"bad length retained until cold recovery");
    run(8);check(free_calls==1 && !sends,"unknown request free once");
    run(9);check(free_calls==1 && !sends,"free failure no send");
    run(10);check(free_calls==1 && sends==1 && !completed,"send failure no second request");
    run(11);check(tick==180000 && !free_calls && !sends,"receive window bounded");
    run(0);check(free_calls==2 && sends==2 && completed && cleanup_calls==1,"HELLO PING ownership and teardown");
    printf("M0_CONTROL_FLOW_CASES=%d\n",cases);
}
'''


def test(packet, source, report):
    cases = 0
    for line in (packet/"SHA256SUMS").read_text().splitlines():
        digest, name = line.split("  ", 1)
        assert hashlib.sha256((packet/name).read_bytes()).hexdigest() == digest, name
        cases += 1
    subprocess.run(["bash", "-n", str(packet/"stage-assets.sh")], check=True)
    denied = subprocess.run(["bash", str(packet/"stage-assets.sh")], capture_output=True, text=True)
    assert denied.returncode == 2 and "pending L3 approval" in denied.stderr
    snapshot_cases = test_snapshot(packet)
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        for stage in "ABC":
            script = (packet/f"stage-{stage}.cmd").read_text()
            assert script.count("amp_m0load ") == (stage != "A")
            assert all(token not in script for token in ["saveenv", "mw ", "mm ", "reset", "run bootcmd"])
            shell = SHELL.replace("IMAGE_SIZE", f'{(packet/"boot/Image").stat().st_size:x}').replace("INITRD_SIZE", f'{(packet/"boot/initrd").stat().st_size:x}').replace("DT_SIZE", f'{(packet/f"boot/stage-{stage}.dtb").stat().st_size:x}')
            commands = tmp/f"{stage}.sh";commands.write_text(shell+"\n"+script)
            faults = ["part", "wrong_part", "set_bootargs", "set_bootargs_ext"] + [f"{kind}_{n}" for kind in ["size", "length", "load", "short", "env_size", "env_load", "malformed"] for n in range(1,4)] + [f"clear_{n}" for n in range(1,7)]
            for fault in faults:
                r = subprocess.run(["bash", str(commands)], env={"PATH":"/usr/bin:/bin", "fault":fault}, capture_output=True, text=True)
                assert r.returncode == 1 and "M0_CALLED" not in r.stdout and "BOOTI_CALLED" not in r.stdout, (stage,fault,r.stdout)
                cases += 1
            if stage != "A":
                r = subprocess.run(["bash", str(commands)], env={"PATH":"/usr/bin:/bin", "fault":"m0"}, capture_output=True, text=True)
                assert r.returncode==1 and r.stdout.count("M0_CALLED")==1 and "BOOTI_CALLED" not in r.stdout
                cases += 1
            r = subprocess.run(["bash", str(commands)], capture_output=True, text=True)
            assert r.returncode==0 and r.stdout.count("BOOTI_CALLED")==1 and r.stdout.count("M0_CALLED")==int(stage!="A"), r.stdout
            cases += 1
        body = "\n".join(line for line in source.read_text().splitlines() if not line.startswith("#include"))
        cfile=tmp/"m0.c";exe=tmp/"m0";cfile.write_text(C_STUBS+body+C_TESTS)
        subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",str(cfile),"-o",str(exe)],check=True,capture_output=True,text=True)
        m0=subprocess.check_output([str(exe)],text=True).strip()
    result={"evidence":"HOST_TESTED","board_access":False,"firmware_executed":False,
            "script_model":"Bash shared subset; U-Boot commands fault-stubbed", "packet_and_script_cases":cases,
            "m0_model":"actual C task compiled for Host; HAL/RTOS/RPMsg fault stubs", "m0_result":m0}
    result["installer_snapshot_host_cases"] = snapshot_cases
    result["installer_persistent_writes_executed"] = False
    report.write_text(json.dumps(result,indent=2)+"\n");print(json.dumps(result,indent=2))


if __name__=="__main__":
    ap=argparse.ArgumentParser();ap.add_argument("--packet",type=Path,required=True);ap.add_argument("--source",type=Path,required=True);ap.add_argument("--report",type=Path,required=True)
    args=ap.parse_args();test(args.packet,args.source,args.report)
