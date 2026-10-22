"""Запуск временного K3s должен подготовить cgroups и дождаться kubelet."""

import importlib.util
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch
import subprocess

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('deploy_check', ROOT / 'tools/deploy/check.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ClusterCheckTest(unittest.TestCase):
    def test_runtime_waits_for_ready_node_before_applying_manifests(self):
        calls = []

        def run(args, **kwargs):
            calls.append(args)
            output = (
                json.dumps({'serverVersion': {'gitVersion': 'v-test'}}) if 'version' in args else ''
            )
            return subprocess.CompletedProcess(args, 0, stdout=output)

        with TemporaryDirectory() as temporary:
            directory = Path(temporary)
            (directory / 'plan.json').write_text(
                json.dumps(
                    {
                        'kubernetes_version': 'v-test',
                        'namespace': 'test',
                        'jobs': [],
                    }
                )
            )
            (directory / 'release.json').write_text(json.dumps({'images': {}, 'revision': 'test'}))
            for phase in ['namespace', 'infrastructure', 'migrations', 'services']:
                (directory / (phase + '.yaml')).write_text('')
            with patch.object(module.subprocess, 'run', side_effect=run):
                module.check(directory, runtime=True)
        docker_run = calls[0]
        self.assertIn('--cgroupns=host', docker_run)
        self.assertIn('/sys/fs/cgroup:/sys/fs/cgroup:rw', docker_run)
        ready = next(i for i, args in enumerate(calls) if '--for=condition=Ready' in args)
        apply = next(i for i, args in enumerate(calls) if 'apply' in args)
        self.assertLess(ready, apply)
        self.assertEqual(calls[-1][:4], ['docker', 'rm', '-f', '-v'])
