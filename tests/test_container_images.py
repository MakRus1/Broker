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
