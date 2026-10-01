#!/usr/bin/env python3
"""Print planned issues by default. --apply explicitly creates missing private-repo issues."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]

def run(args: list[str], timeout: int = 45) -> str:
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=timeout, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed ({result.returncode}): {' '.join(args[:3])}\n{result.stderr.strip()}")
    return result.stdout.strip()

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true', help='Create issues after explicit user approval')
    args = parser.parse_args()
    data = json.loads((ROOT / 'docs/tasks/manifest.json').read_text(encoding='utf-8'))
    repo = data['repository']
    tasks = data['tasks']
    if not args.apply:
        print(f'DRY RUN: target {repo}; no GitHub request will be made.')
        for task in tasks:
            print(f"{task['title']} | {task['status']} | {task['body_file']}")
        print('After repo setup and approval: python3 scripts/dev/seed_issues.py --apply')
        return 0
    if not shutil.which('gh'):
        raise RuntimeError('gh CLI is not installed')
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repo):
        raise RuntimeError('Invalid repository name in manifest')
    user = run(['gh', 'api', 'user', '--jq', '.login'])
    if user.lower() != repo.split('/')[0].lower():
        raise RuntimeError('Authenticated account does not match expected personal repository owner; stop for review')
    info = json.loads(run(['gh', 'repo', 'view', repo, '--json', 'nameWithOwner,isPrivate']))
    if info['nameWithOwner'].lower() != repo.lower() or not info['isPrivate']:
        raise RuntimeError('Expected private repository was not confirmed; refusing to publish')
    existing = json.loads(run(['gh', 'issue', 'list', '--repo', repo, '--state', 'all', '--limit', '1000', '--json', 'number,title,body']))
    if len(existing) >= 1000:
        raise RuntimeError('Issue listing may be incomplete; stop instead of creating duplicates')
    for task in tasks:
        marker = f"<!-- cockpit-task:{task['id']} -->"
        if any(marker in (item.get('body') or '') or item['title'].startswith(f"[{task['id']}]") for item in existing):
            print(f"SKIP existing {task['id']}")
            continue
        path = (ROOT / task['body_file']).resolve()
        if ROOT not in path.parents:
            raise RuntimeError('Task file escapes repository')
        body = marker + '\n\n' + path.read_text(encoding='utf-8')
        result = subprocess.run(
            ['gh', 'issue', 'create', '--repo', repo, '--title', task['title'], '--body-file', '-'],
            input=body, text=True, capture_output=True, cwd=ROOT, timeout=45, check=False)
        if result.returncode != 0:
            raise RuntimeError(f"Issue creation failed for {task['id']}: {result.stderr.strip()}")
        print(result.stdout.strip())
        existing.append({'title':task['title'], 'body':body})
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, subprocess.TimeoutExpired, RuntimeError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        sys.exit(1)
