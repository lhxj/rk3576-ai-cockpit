#!/usr/bin/env bash
# Read-only host inventory; no installations, Git changes, or board access.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
printf 'Project root: %s\n' "$ROOT"
printf 'This script is for WSL/host, not the physical board.\n'
uname -m
if command -v lsb_release >/dev/null 2>&1; then lsb_release -ds; fi
df -h "$ROOT"
if [[ -n "${VIRTUAL_ENV:-}" ]]; then
    printf 'WARNING: Python venv active: %s\nDeactivate unrelated project venv in your shell.\n' "$VIRTUAL_ENV"
fi
missing=0
for cmd in git cmake g++ make python3 bash; do
    if command -v "$cmd" >/dev/null 2>&1; then
        printf 'FOUND: %s -> %s\n' "$cmd" "$(command -v "$cmd")"
    else
        printf 'MISSING required: %s\n' "$cmd"
        missing=1
    fi
done
for cmd in ssh gh codex timeout flock rsync dtc aarch64-linux-gnu-g++; do
    if command -v "$cmd" >/dev/null 2>&1; then
        printf 'FOUND optional/current-task: %s\n' "$cmd"
    else
        printf 'NOT FOUND optional/current-task: %s (not needed for host smoke)\n' "$cmd"
    fi
done
if command -v cmake >/dev/null 2>&1; then cmake --version; fi
if command -v python3 >/dev/null 2>&1; then
    python3 -c 'import sys; print("Python:", sys.version.split()[0]); sys.exit(0 if sys.version_info >= (3,10) else 1)'
fi
if git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git -C "$ROOT" status --short
else
    printf 'INFO: no Git repository in this extracted copy; the user target may already have .git.\n'
fi
printf 'No GitHub login/token file has been printed. No board connection attempted.\n'
exit "$missing"
