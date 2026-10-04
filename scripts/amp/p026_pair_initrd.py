#!/usr/bin/env python3
"""Pair the copied Debian initrd's one module with the new Host kernel release."""
import gzip
import hashlib
import json
from pathlib import Path
import stat
import tarfile

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'artifacts/local/p026-kernel-isolated-clean'
OLD = '6.1.99-rk3576'
NEW = OLD + '-m0echo-p026'


def entries(data):
    offset = 0
    while offset < len(data):
        while offset < len(data) and not data[offset]:
            offset += 1
        if offset == len(data):
            return
        if data[offset:offset + 2] == b'\x1f\x8b':
            yield from entries(gzip.decompress(data[offset:]))
            return
        assert data[offset:offset + 6] == b'070701', 'only inspected newc format accepted'
        fields = [int(data[offset + 6 + 8*i:offset + 14 + 8*i], 16) for i in range(13)]
        size, name_size = fields[6], fields[11]
        begin = offset + 110
        assert name_size and begin + name_size <= len(data)
        name = data[begin:begin + name_size - 1].decode()
        begin = (begin + name_size + 3) & ~3
        assert begin + size <= len(data)
        content = data[begin:begin + size]
        offset = (begin + size + 3) & ~3
        if name != 'TRAILER!!!':
            yield fields, name, content


def encode(fields, name, content):
    fields = list(fields)
    name = name.encode() + b'\0'
    fields[6], fields[11], fields[12] = len(content), len(name), 0
    result = b'070701' + b''.join(f'{v:08x}'.encode() for v in fields) + name
    result += bytes((-len(result)) % 4)
    result += content
    result += bytes((-len(result)) % 4)
    return result


def main():
    with tarfile.open(ROOT / 'artifacts/local/p025-post-recovery-20261002T173913Z-975579/boot-originals.tar') as tar:
        old = tar.extractfile('initrd.img-' + OLD).read()
    assert hashlib.sha256(old).hexdigest() == '425d2a68a4a67e807518c067cfefc1111d0a33af02236761ddaa3794c3205397'
    encoded, changes, seen = [], [], set()
    for fields, name, content in entries(old):
        previous = name
        changed = False
        for prefix in ('usr/lib/modules/', 'lib/modules/'):
            if name == prefix + OLD or name.startswith(prefix + OLD + '/'):
                name = prefix + NEW + name[len(prefix + OLD):]
                changed = True
                if stat.S_ISREG(fields[1]):
                    relative = name[len(prefix + NEW + '/'):]
                    replacement = BUILD / 'module-root/lib/modules' / NEW / relative
                    assert replacement.is_file(), 'missing paired module/metadata: ' + relative
                    content = replacement.read_bytes()
        if stat.S_ISLNK(fields[1]) and OLD.encode() in content:
            content = content.replace(OLD.encode(), NEW.encode())
            changed = True
        assert name not in seen, 'duplicate transformed cpio path'
        seen.add(name)
        if changed:
            changes.append({'old': previous, 'new': name, 'sha256': hashlib.sha256(content).hexdigest()})
        # Preserve all non-module file bytes/permissions/owners/inodes/timestamps.
        encoded.append(encode(fields, name, content))
    assert sum(c['new'].endswith('.ko') for c in changes) == 1
    trailer = [0] * 13
    trailer[4] = 1
    encoded.append(encode(trailer, 'TRAILER!!!', b''))
    raw = b''.join(encoded)
    raw += bytes((-len(raw)) % 512)
    result = gzip.compress(raw, compresslevel=6, mtime=0)
    # Parse output independently again and reject any old-release module paths.
    names = [name for _, name, _ in entries(result)]
    assert all('/modules/' + OLD + '/' not in n for n in names)
    output = BUILD / ('initrd.img-' + NEW)
    output.write_bytes(result)
    report = {'status': 'HOST_PAIRED_INITRD_PASS', 'original_sha256': hashlib.sha256(old).hexdigest(),
              'file': str(output.relative_to(ROOT)), 'sha256': hashlib.sha256(result).hexdigest(),
              'size': len(result), 'release': NEW, 'module_replacements': 1,
              'changes': changes, 'non_module_file_bytes_preserved': True,
              'board_access': False, 'scope': 'Debian original scripts retained; no runtime boot claim'}
    (BUILD / 'initrd-result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'changes'}, indent=2))


if __name__ == '__main__':
    main()
