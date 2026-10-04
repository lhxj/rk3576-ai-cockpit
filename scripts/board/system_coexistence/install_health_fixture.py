#!/usr/bin/env python3
"""Approval-gated additive SI_HEALTH_V1 installer. Never starts M0 or a module."""
import hashlib
import json
import os
from pathlib import Path
import stat
import sys

DEST = Path('/boot/amp-p029/system-health-v1')
PIN_MANIFEST = '8d4d9d692c6bc271a77783c48cf4bae2970325d59348498e169fa3f25f93e11f'
MEMBERS = {'SI_HEALTH.json','SHA256SUMS','amp-signed.itb','stage-health.cmd',
           'stage-health.scr','preflight-health.py','rk3576_amp_health_test.ko'}
MAX_FILE = 256 * 1024


def need(ok, reason):
    if not ok:
        raise RuntimeError('STOP SI_HEALTH install: ' + reason)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def read_regular(path):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(fd)
        need(stat.S_ISREG(before.st_mode) and before.st_nlink == 1 and
             0 < before.st_size <= MAX_FILE, 'regular bounded file: ' + path.name)
        raw = os.read(fd, MAX_FILE + 1)
        after = os.fstat(fd)
        need((before.st_ino,before.st_size,before.st_mtime_ns) ==
             (after.st_ino,after.st_size,after.st_mtime_ns) and len(raw) == before.st_size,
             'changed/short file: ' + path.name)
        return raw
    finally:
        os.close(fd)


def validate_packet(folder):
    need(not folder.is_symlink() and folder.is_dir(), 'real staging directory')
    need({p.name for p in folder.iterdir()} == MEMBERS, 'exact seven-file packet')
    payloads = {name:read_regular(folder/name) for name in sorted(MEMBERS)}
    need(sha(payloads['SI_HEALTH.json']) == PIN_MANIFEST, 'pinned manifest')
    manifest = json.loads(payloads['SI_HEALTH.json'])
    need(manifest['id'] == 'SI_HEALTH_V1' and manifest['destination'] == str(DEST), 'manifest identity')
    need(set(manifest['files']) == MEMBERS - {'SI_HEALTH.json','SHA256SUMS'}, 'payload whitelist')
    for name,entry in manifest['files'].items():
        need(len(payloads[name]) == entry['bytes'] and sha(payloads[name]) == entry['sha256'],
             'exact payload: ' + name)
    expected_sums = ''.join(sha(payloads[name])+'  '+name+'\n'
                            for name in sorted(MEMBERS - {'SHA256SUMS'})).encode()
    need(payloads['SHA256SUMS'] == expected_sums, 'exact checksum list')
    return payloads,manifest


def install(folder):
    need(os.geteuid() == 0, 'existing sudo -n required; never change policy')
    need(folder.is_absolute() and folder.resolve(strict=True) == folder and
         folder.is_relative_to(Path('/home/cat/cockpit')), 'exact user staging path')
    need(Path('/boot').resolve(strict=True) == Path('/boot') and
         DEST.parent.resolve(strict=True) == DEST.parent, 'canonical boot directory')
    need(not os.path.lexists(DEST), 'fresh destination only; no overwrite/cleanup')
    need(os.uname().release == '6.1.99-rk3576', 'default kernel for passive install')
    args = Path('/proc/cmdline').read_text().split()
    need(not any(x.startswith(('amp_test_stage=','amp_health_test=')) for x in args), 'default boot')
    need(not list(Path('/sys/bus/rpmsg/devices').iterdir()), 'no live RPMsg peer')
    for process in Path('/proc').iterdir():
        if not process.name.isdigit():
            continue
        try:
            name = (process/'comm').read_text().strip()
        except OSError:
            continue
        need(not name.startswith(('cockpit_ui','voice_runtime','cam0_','media_rtsp',
                                  'media_record','vision_rknn')), 'project occupant')
    payloads,manifest = validate_packet(folder)
    before = {}
    for name,digest in manifest['reuse'].items():
        path = DEST.parent/name
        # Image exceeds packet limit; hash it via bounded chunks, no writes.
        need(path.is_file() and not path.is_symlink(), 'reuse file: ' + name)
        with path.open('rb') as stream:
            h = hashlib.sha256()
            for chunk in iter(lambda: stream.read(1024*1024), b''):
                h.update(chunk)
        need(h.hexdigest() == digest, 'frozen reuse hash: ' + name)
        before[name] = digest
    os.mkdir(DEST, 0o755)
    os.chmod(DEST, 0o755)
    for name,raw in payloads.items():
        path = DEST/name
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o644)
        try:
            os.fchmod(fd,0o644)
            with os.fdopen(fd,'wb',closefd=False) as stream:
                stream.write(raw)
                stream.flush()
                os.fsync(fd)
        finally:
            os.close(fd)
        need(read_regular(path) == raw, 'installed readback: ' + name)
    directory_fd = os.open(DEST, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        os.fsync(directory_fd)
    finally:
        os.close(directory_fd)
    need({p.name for p in DEST.iterdir()} == MEMBERS, 'readback members')
    for name,digest in before.items():
        with (DEST.parent/name).open('rb') as stream:
            h = hashlib.sha256()
            for chunk in iter(lambda: stream.read(1024*1024), b''):
                h.update(chunk)
        need(h.hexdigest() == digest, 'frozen reuse changed: ' + name)
    print(json.dumps({'result':'SI_HEALTH_PASSIVE_INSTALL_READBACK_PASS','destination':str(DEST),
                      'files':{name:sha(raw) for name,raw in payloads.items()},
                      'boot_chain_started':False,'module_loaded':False},sort_keys=True))


def main():
    if len(sys.argv) != 3 or sys.argv[1] != '--approved-SI_HEALTH_V1':
        raise SystemExit('STOP explicit user approval and exact staged packet required')
    install(Path(sys.argv[2]))


if __name__ == '__main__':
    main()
