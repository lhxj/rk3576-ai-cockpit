#!/usr/bin/env python3
"""Read C runtime identity/address evidence. Never load a module or send RPMsg."""
import hashlib
import os
from pathlib import Path
import re
import stat
import struct
import subprocess

DT = Path('/sys/firmware/devicetree/base')
KO = Path('/boot/amp-p029/rk3576_amp_echo_test.ko')
RELEASE = '6.1.99-rk3576-m0echo-p026'
KO_SHA = 'cd82f24659ba4a5a09fc314f5d9d3372b28451c8b21c7f64a868b11d11b5fe43'


def need(ok, label):
    if not ok:
        raise RuntimeError('STOP C preflight: ' + label)


def prop(node, key):
    return (DT / node / key).read_bytes()


def cells(node, key):
    raw = prop(node, key)
    need(len(raw) % 4 == 0, 'cell encoding ' + node + '/' + key)
    return struct.unpack('>' + 'I' * (len(raw) // 4), raw)


def main():
    need(os.geteuid() == 0, 'run via existing sudo -n for iomem/dmesg reads')
    need(os.uname().release == RELEASE, 'paired release')
    args = Path('/proc/cmdline').read_text().split()
    need('amp_test_stage=C' in args and 'root=/dev/mmcblk0p3' in args, 'stage/root')
    need(prop('chosen', 'project,p029-stage') == b'C\0', 'C DT tag')
    need(prop('', 'model') == b'EmbedFire LubanCat-3-v2\0', 'model')
    need(float(Path('/proc/uptime').read_text().split()[0]) < 100, 'C window already too old')
    need(cells('reserved-memory', '#address-cells') == (2,) and
         cells('reserved-memory', '#size-cells') == (2,), 'reservation cell widths')
    regions = [('mcu@47800000', 0x47800000, 0x80000),
               ('rpmsg@47d00000', 0x47d00000, 0x10000),
               ('rpmsg-dma@47d10000', 0x47d10000, 0x10000)]
    for name, base, size in regions:
        node = 'reserved-memory/' + name
        need(cells(node, 'reg') == (0, base, 0, size), 'reserved reg ' + name)
        need(prop(node, 'no-map') == b'' and not (DT / node / 'reusable').exists(), 'no-map ' + name)
    need(prop('reserved-memory/rpmsg-dma@47d10000', 'compatible') == b'shared-dma-pool\0', 'pool type')
    for node in ['mcu-amp', 'rpmsg@47d00000', 'mailbox@2ae50000', 'mailbox@2ae54000']:
        need(prop(node, 'status') == b'okay\0', 'enabled node ' + node)
    need(not (DT / 'mcu-amp/amp-cpus').exists(), 'Linux must not launch a second M0')
    need(prop('serial@2ad80000', 'status') == b'disabled\0', 'UART5 Linux driver ownership')
    need(cells('rpmsg@47d00000', 'reg') == (0, 0x47d00000, 0, 0x10000), 'ring DT')
    need(cells('rpmsg@47d00000', 'rockchip,link-id') == (4,) and
         cells('rpmsg@47d00000', 'rockchip,vdev-nums') == (1,), 'single link4')
    need(prop('rpmsg@47d00000', 'project,require-shared-dma-pool') == b'', 'required pool')
    need(cells('rpmsg@47d00000', 'memory-region') ==
         cells('reserved-memory/rpmsg-dma@47d10000', 'phandle'), 'actual pool reference')
    need(cells('rpmsg@47d00000', 'mboxes') ==
         (cells('mailbox@2ae50000', 'phandle')[0], 0,
          cells('mailbox@2ae54000', 'phandle')[0], 0), 'RX0/TX4 mailbox references')
    need(not any(line.split()[0] == 'rk3576_amp_echo_test'
                 for line in Path('/proc/modules').read_text().splitlines()), 'echo already loaded; do not retry')
    fd = os.open(KO, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(fd)
        need(stat.S_ISREG(before.st_mode) and before.st_nlink == 1 and before.st_uid == 0 and
             before.st_gid == 0 and stat.S_IMODE(before.st_mode) == 0o644 and before.st_size == 40920, 'KO metadata')
        raw = os.read(fd, 40921)
        after = os.fstat(fd)
        need((before.st_ino, before.st_size, before.st_mtime_ns) ==
             (after.st_ino, after.st_size, after.st_mtime_ns), 'KO changed while reading')
        need(len(raw) == 40920 and hashlib.sha256(raw).hexdigest() == KO_SHA, 'exact paired KO')
    finally:
        os.close(fd)
    need(b'vermagic=' + RELEASE.encode() + b' SMP mod_unload aarch64\0' in raw, 'KO vermagic')
    ram = [(int(a, 16), int(b, 16) + 1) for a, b in
           re.findall(r'^\s*([0-9a-f]+)-([0-9a-f]+)\s*:\s*System RAM\s*$', Path('/proc/iomem').read_text(), re.M)]
    need(ram and all(end > start and end > 1 for start, end in ram), 'non-redacted System RAM')
    for name, base, size in regions:
        need(all(base + size <= start or end <= base for start, end in ram), 'reservation overlaps RAM ' + name)
    channels = [p for p in Path('/sys/bus/rpmsg/devices').iterdir()
                if (p / 'name').is_file() and (p / 'name').read_text().strip() == 'rk3576-m0-echo']
    need(len(channels) == 1, 'one announced echo service; no blind module loading')
    need(not (channels[0] / 'driver').exists(), 'echo service already bound')
    # The fixed kernel log buffer is 2^18 bytes. Read a bounded snapshot,
    # never dmesg -w, never clear the log, never access device registers.
    log = subprocess.run(['dmesg', '--color=never'], capture_output=True, timeout=5, check=True).stdout
    need(len(log) <= 2 * 1024 * 1024, 'kernel log size')
    text = log.decode(errors='replace')
    ring_lines = re.findall(r'^.*rpdev vdev0: vring0 0x47d00000, vring1 0x47d08000.*$', text, re.M)
    need(len(ring_lines) == 1, 'actual Linux rings')
    backing = re.findall(r'^.*buffers: va [^,]+, dma (?:0x)?([0-9a-fA-F]+).*$', text, re.M)
    # 64 descriptors/direction * 2 * 512 = 65536 bytes in this fixed kernel.
    need(len(backing) == 1 and int(backing[0], 16) == 0x47d10000, 'actual DMA backing for whole 64KiB pool')
    need('rpmsg host is online' in text, 'host online')
    print(ring_lines[0])
    print('C actual DMA base=0x47d10000; source-derived extent=0x10000')
    print('C service=' + channels[0].name)
    print('P030_C_PREFLIGHT_READONLY_PASS; module has NOT been loaded by this script')


if __name__ == '__main__':
    main()
