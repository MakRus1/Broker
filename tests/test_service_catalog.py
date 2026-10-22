"""Добавление service.yaml подхватывается сборкой и staging без реестра имён."""

import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from service_catalog import discover, validate_names
import staging_manifest


class CatalogTest(unittest.TestCase):
    def test_names_and_symlink_escape_are_rejected(self):
        for names in ([], ['all'], ['postgres'], ['../escape'], ['foo', 'foo']):
            with self.subTest(names=names), self.assertRaises(ValueError):
                validate_names(names)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'outside').mkdir()
            (root / 'outside/service.yaml').write_text('version: 1')
            (root / 'services').mkdir()
            (root / 'services/escape').symlink_to(root / 'outside', target_is_directory=True)
            with self.assertRaises(ValueError):
                discover(root)

    def test_new_service_build_and_render_with_default_resources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copytree(
                ROOT / 'services', root / 'services', ignore=shutil.ignore_patterns('__pycache__')
            )
            shutil.copytree(ROOT / 'tools/local', root / 'tools/local')
            shutil.copytree(ROOT / 'schemas/permissions', root / 'schemas/permissions')
            shutil.copy(ROOT / 'tools/container-images.json', root / 'tools/container-images.json')
            shutil.copy(ROOT / 'platform.yaml', root / 'platform.yaml')
            shutil.copytree(root / 'services/broker', root / 'services/new_service')
            services = discover(root)
            self.assertIn('new_service', services)
            spec = importlib.util.spec_from_file_location(
                'build_images_test', ROOT / 'tools/deploy/build.py'
            )
            build = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(build)

            def docker(command, **kwargs):
                metadata = Path(command[command.index('--metadata-file') + 1])
                metadata.write_text(json.dumps({'containerimage.digest': 'sha256:' + 'b' * 64}))

            output = root / 'state/release.json'
            with patch.object(build, 'ROOT', root), patch.object(
                build.subprocess, 'run', side_effect=docker
            ) as run:
                build.build('a' * 40, output=output)
                self.assertEqual(run.call_count, len(services))
            release = json.loads(output.read_text())
            self.assertEqual(set(release['images']), set(services))
            (root / 'state/lan').mkdir()
            (root / 'state/lan/enabled').write_text('true\n')
            phases = staging_manifest.render(release, root / 'state', root=root)
            import yaml

            collector = next(
                obj
                for obj in phases['monitoring']
                if obj['kind'] == 'ConfigMap'
                and obj['metadata']['name'].startswith('collector-settings')
            )
            telemetry = yaml.safe_load(collector['data']['config.yaml'])
            self.assertIn(
                'filelog/new_service', telemetry['service']['pipelines']['logs']['receivers']
            )
            self.assertEqual(
                telemetry['receivers']['filelog/new_service']['operators'][-1]['value'],
                'new_service',
            )
            deployments = {
                obj['metadata']['name']: obj
                for obj in phases['services']
                if obj['kind'] == 'Deployment'
            }
            self.assertIn('new-service', deployments)
            self.assertEqual(deployments['new-service']['spec']['replicas'], 1)
            migration = next(
                obj
                for obj in phases['migrations']
                if obj['kind'] == 'ConfigMap'
                and obj['metadata']['labels']['broker/service'] == 'new_service'
            )
            self.assertIn('CREATE DATABASE new_service', migration['data']['migrate.sql'])
            self.assertIn('\\connect new_service\n', migration['data']['migrate.sql'])
            self.assertNotIn('\\\\connect', migration['data']['migrate.sql'])
