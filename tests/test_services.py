"""Проверка создания сервисов и атомарности отказов в отдельной копии репозитория."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import yaml

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'scripts'))
import create_service
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from generate import run


class ServicesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / 'repo'
        shutil.copytree(ROOT, self.root, ignore=shutil.ignore_patterns('.git', '.venv*', 'build-*', '__pycache__', '.cache', '.ccache', '.new-service-*'))
        self.patch = mock.patch.object(create_service, 'ROOT', self.root)
        self.patch.start()

    def tearDown(self):
        self.patch.stop(); self.temp.cleanup()

    def test_four_feature_combinations_and_automatic_ping(self):
        cache = self.root / 'scripts/templates/service/tests/__pycache__'
        cache.mkdir(parents=True)
        (cache / 'cached.pyc').write_bytes(b'\xcb\x00')
        for pg, grpc in [(False, False), (True, False), (False, True), (True, True)]:
            name = f'sample_{int(pg)}{int(grpc)}'
            directory = create_service.create(name, pg, grpc)
            run(directory, check=True)
            self.assertFalse(list(directory.rglob('*.pyc')))
            config = yaml.safe_load((directory / 'configs/static_config.yaml').read_text())['components_manager']['components']
            self.assertEqual(config['handler-ping']['path'], '/ping')
            self.assertEqual(config['handler-ping-ready']['path'], '/ping/ready')
            self.assertTrue(config['handler-ping']['monitor-handler'])
            self.assertEqual('postgres-db-1' in config, pg)
            self.assertEqual('grpc-server' in config, grpc)
            main = (directory / 'src/main.cpp').read_text()
            self.assertIn(f'.Append<{name}::Ping>()', main)
            self.assertEqual(f'.Append<{name}::HelloGrpc>()' in main, grpc)
            cmake = (directory / 'CMakeLists.txt').read_text()
            self.assertEqual('src/hello_grpc.cpp' in cmake, grpc)
            self.assertIn('broker_common', cmake)
            self.assertIn('broker_openapi_generate', cmake)
            self.assertEqual('pg_->Execute' in (directory / 'src/ping.cpp').read_text(), pg)
            self.assertEqual('userver/storages/postgres/' in (directory / 'src/ping.cpp').read_text(), pg)
            self.assertEqual('userver/storages/postgres/' in (directory / 'src/ping.hpp').read_text(), pg)
            with self.assertRaises(ValueError): create_service.create(name, pg, grpc)

    def test_invalid_names_leave_no_files(self):
        for name in ['', '../escape', '/tmp/escape', 'two words', 'a-b', 'class', '_reserved', 'x;echo bad']:
            with self.assertRaises(ValueError): create_service.create(name)
        self.assertFalse(list((self.root / 'services').glob('.new-service-*')))

    def test_registration_rolls_back(self):
        paths = [self.root / p for p in ['CMakeLists.txt', '.github/service-deps.yaml', 'docker-compose.yml']]
        before = {p: p.read_bytes() for p in paths}
        original = Path.write_text
        def fail(path, *args, **kwargs):
            if path == self.root / '.github/service-deps.yaml': raise OSError('Тестовый отказ записи')
            return original(path, *args, **kwargs)
        with mock.patch.object(Path, 'write_text', fail):
            with self.assertRaises(OSError): create_service.create('rollback_test')
        self.assertFalse((self.root / 'services/rollback_test').exists())
        self.assertEqual(before, {p: p.read_bytes() for p in paths})
