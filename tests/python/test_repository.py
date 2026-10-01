"""Offline metadata checks; they make no claim about actual physical hardware."""
import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]

class RepositoryTests(unittest.TestCase):
    def test_required_documents(self):
        for name in ['AGENTS.md', 'START_HERE.md', '.agent/PLANS.md',
                     'docs/STATUS.md', 'docs/prompts/PHASE0.md',
                     'docs/architecture/SYSTEM.md', '.codex/config.toml']:
            with self.subTest(name=name):
                self.assertTrue((ROOT / name).read_text(encoding='utf-8').strip())

    def test_json_examples(self):
        for path in (ROOT / 'config').glob('*.json'):
            self.assertEqual(json.loads(path.read_text(encoding='utf-8'))['schema_version'], 1)
        cameras = json.loads((ROOT / 'config/cameras.example.json').read_text(encoding='utf-8'))
        self.assertIsNone(cameras['cameras'][0]['video_node'])
        self.assertFalse(cameras['cameras'][1]['enabled'])
        self.assertIsNone(cameras['cameras'][1]['video_node'])

    def test_task_graph(self):
        data = json.loads((ROOT / 'docs/tasks/manifest.json').read_text(encoding='utf-8'))
        tasks = data['tasks']
        ids = {t['id'] for t in tasks}
        self.assertEqual(len(ids), len(tasks))
        graph = {t['id']: t['depends_on'] for t in tasks}
        seen, active = set(), set()
        def visit(node):
            self.assertNotIn(node, active, 'cycle in task dependencies')
            if node in seen:
                return
            active.add(node)
            for dep in graph[node]:
                self.assertIn(dep, ids)
                visit(dep)
            active.remove(node)
            seen.add(node)
        for task in tasks:
            self.assertTrue((ROOT / task['body_file']).is_file())
            visit(task['id'])

    def test_issue_seed_is_dry_run_by_default(self):
        result = subprocess.run([sys.executable, str(ROOT / 'scripts/dev/seed_issues.py')],
                                text=True, capture_output=True, timeout=5, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('DRY RUN', result.stdout)
        self.assertIn('[P009]', result.stdout)

    def test_ssh_options_preserve_authentication(self):
        text = (ROOT / 'scripts/board/_common.sh').read_text(encoding='utf-8')
        self.assertIn('BatchMode=yes', text)
        self.assertIn('StrictHostKeyChecking=yes', text)
        self.assertNotIn('StrictHostKeyChecking=no', text)
        self.assertNotIn('sshpass', text)

    def test_presets_version(self):
        data = json.loads((ROOT / 'CMakePresets.json').read_text(encoding='utf-8'))
        self.assertEqual(data['version'], 3)
        self.assertEqual(data['configurePresets'][0]['name'], 'host-debug')

if __name__ == '__main__':
    unittest.main()
