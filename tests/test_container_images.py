"""Проверка привязки digest образа ко всем входам сборки окружения."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('container_images', ROOT / 'scripts/container-images.py')
images = importlib.util.module_from_spec(spec)
spec.loader.exec_module(images)


class ContainerImagesTest(unittest.TestCase):
    def test_inputs_and_digest_lock(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for relative in images.INPUTS:
                p = root / relative
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes((ROOT / relative).read_bytes())
            lock = root / 'tools/container-images.json'
            with mock.patch.object(images, 'ROOT', root), mock.patch.object(images, 'LOCK', lock):
                key = images.fingerprint()
                (root / 'business.cpp').write_text('Изменение приложения')
                self.assertEqual(images.fingerprint(), key)
                for relative in images.INPUTS:
                    p = root / relative
                    original = p.read_bytes()
                    p.write_bytes(original + b'\n')
                    self.assertNotEqual(images.fingerprint(), key)
                    p.write_bytes(original)
                refs = {v: images.REGISTRY + '@sha256:' + char * 64
                        for v, char in [('dev', 'a'), ('sanitize', 'b')]}
                lock.write_text(json.dumps({'version': 1, 'inputs_sha256': key, 'images': refs}))
                with mock.patch('sys.argv', ['container-images.py', 'ref', '--variant', 'sanitize']):
                    with contextlib.redirect_stdout(io.StringIO()) as output:
                        images.main()
                    self.assertEqual(output.getvalue().strip(), refs['sanitize'])
                config = root / '.devcontainer/devcontainer.json'
                config.parent.mkdir()
                config.write_text(json.dumps({'image': refs['dev']}))
                with mock.patch('sys.argv', ['container-images.py', 'check']):
                    images.main()
                    config.write_text(json.dumps({'build': {}, 'image': refs['dev']}))
                    with self.assertRaisesRegex(ValueError, 'Dev Container'):
                        images.main()
                lock.write_text(json.dumps({'version': 1, 'inputs_sha256': 'outdated', 'images': refs}))
                with mock.patch('sys.argv', ['container-images.py', 'check']):
                    with self.assertRaisesRegex(ValueError, 'lock-images'):
                        images.main()

    def test_registry_requires_both_architectures(self):
        descriptor = {'digest': 'sha256:' + 'a' * 64, 'manifests': [
            {'platform': {'os': 'linux', 'architecture': arch}} for arch in ['amd64', 'arm64']]}
        with mock.patch.object(images.subprocess, 'check_output', return_value=json.dumps(descriptor)):
            self.assertIn('@sha256:', images.inspect('test'))
        descriptor['manifests'].pop()
        with mock.patch.object(images.subprocess, 'check_output', return_value=json.dumps(descriptor)):
            with self.assertRaisesRegex(ValueError, 'amd64 или arm64'):
                images.inspect('test')

    def test_publisher_covers_inputs_and_ci_only_pulls(self):
        import yaml
        publish = yaml.load((ROOT / '.github/workflows/environment.yml').read_text(), Loader=yaml.BaseLoader)
        ci = yaml.load((ROOT / '.github/workflows/ci.yml').read_text(), Loader=yaml.BaseLoader)
        self.assertTrue(set(images.INPUTS) <= set(publish['on']['push']['paths']))
        self.assertEqual(ci['permissions']['packages'], 'read')
        self.assertFalse(any(step.get('uses', '').startswith('docker/build-push-action')
                             for step in ci['jobs']['build']['steps']))

    def test_all_local_docker_copies_are_fingerprinted(self):
        import shlex
        for line in (ROOT / 'tools/Dockerfile').read_text().splitlines():
            if not line.startswith('COPY ') or line.startswith('COPY --from='):
                continue
            sources = shlex.split(line)[1:-1]
            for source in sources:
                self.assertIn(source, images.INPUTS, f'Добавьте {source} в fingerprint окружения')


class CiImageSelectionTest(unittest.TestCase):
    def test_invalid_environment_stops_step_without_publishing_empty_ref(self):
        import os
        import subprocess
        import yaml
        ci = yaml.load((ROOT / '.github/workflows/ci.yml').read_text(), Loader=yaml.BaseLoader)
        script = next(step['run'] for step in ci['jobs']['build']['steps'] if step.get('id') == 'image')
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            python = root / 'python3'
            python.write_text('#!/bin/sh\necho "stale environment" >&2\nexit 23\n')
            python.chmod(0o755)
            output = root / 'output'
            environment = {**os.environ, 'PATH': str(root) + os.pathsep + os.environ['PATH'],
                           'PRESET': 'debug', 'GITHUB_OUTPUT': str(output)}
            result = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', script],
                                    env=environment, text=True, capture_output=True)
            self.assertEqual(result.returncode, 23)
            self.assertIn('stale environment', result.stderr)
            self.assertFalse(output.exists())

    def test_environment_check_precedes_tool_tests(self):
        import yaml
        ci = yaml.load((ROOT / '.github/workflows/ci.yml').read_text(), Loader=yaml.BaseLoader)
        steps = ci['jobs']['tools']['steps']
        check = next(i for i, step in enumerate(steps) if 'container-images.py check' in step.get('run', ''))
        tests = next(i for i, step in enumerate(steps) if 'ci-tools.py' in step.get('run', ''))
        self.assertLess(check, tests)

    def test_cache_directory_exists_before_container_ownership_is_set(self):
        import os
        import subprocess
        import yaml
        ci = yaml.load((ROOT / '.github/workflows/ci.yml').read_text(), Loader=yaml.BaseLoader)
        script = next(step['run'] for step in ci['jobs']['build']['steps']
                      if step.get('name') == 'Права рабочего каталога для контейнера')
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sudo = root / 'sudo'
            sudo.write_text('#!/bin/sh\ntest -d "$GITHUB_WORKSPACE/.ccache"\n')
            sudo.chmod(0o755)
            environment = {**os.environ, 'PATH': str(root) + os.pathsep + os.environ['PATH'],
                           'GITHUB_WORKSPACE': str(root)}
            result = subprocess.run(['bash', '-e', '-c', script], cwd=root,
                                    env=environment, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
