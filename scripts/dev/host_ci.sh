#!/usr/bin/env bash
# Host-only, deterministic, no downloads or real-board operations.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
for cmd in cmake g++ make python3; do
    command -v "$cmd" >/dev/null 2>&1 || { printf 'Missing prerequisite: %s\n' "$cmd" >&2; exit 2; }
done
cmake --preset host-debug
cmake --build --preset host-debug --parallel 2
ctest --preset host-debug
python3 -m unittest discover -s tests/python -v
python3 scripts/amp/test_p027_manual_uboot.py
while IFS= read -r -d '' file; do
    bash -n "$file"
done < <(find scripts -type f -name '*.sh' -print0)
printf '\nHOST_SCAFFOLD_CHECKS_PASSED (not hardware validation)\n'
