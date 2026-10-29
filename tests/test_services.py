"""Проверка создания сервисов и атомарности отказов в отдельной копии репозитория."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import yaml

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tests'))
from support.generator_repository import copy_generator_repository
sys.path.insert(0, str(ROOT / 'scripts'))
import create_service
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from generate import run
import features


class ServicesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / 'repo'
        copy_generator_repository(self.root)
        self.patch = mock.patch.object(create_service, 'ROOT', self.root)
        self.patch.start()
        self.feature_patch = mock.patch.object(features, 'ROOT', self.root)
        self.feature_patch.start()

    def tearDown(self):
        self.feature_patch.stop()
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
            self.assertFalse(list((directory / 'docs/api').glob('*.yaml')))
            self.assertFalse(list((directory / 'src/views').rglob('view.cpp')))
            self.assertFalse(list((directory / 'proto').rglob('*.proto')))
            self.assertFalse(list((directory / 'postgresql').rglob('*.sql')))
            config = yaml.safe_load((directory / 'configs/static_config.yaml').read_text())['components_manager']['components']
            self.assertEqual(config['handler-ping']['path'], '/ping')
            self.assertEqual(config['handler-ping-ready']['path'], '/ping/ready')
            self.assertTrue(config['handler-ping']['monitor-handler'])
            self.assertEqual('postgres-main' in config, pg)
            self.assertEqual('grpc-server' in config, grpc)
            self.assertEqual('platform-grpc-readiness' in config, grpc)
            if grpc:
                self.assertTrue(config['grpc-server-middlewares-pipeline']['middlewares']['platform-grpc-readiness']['enabled'])
            main = (directory / 'src/main.cpp').read_text()
            self.assertIn(f'.Append<{name}::Ping>()', main)
            self.assertNotIn('HelloGrpc', main)
            cmake = (directory / 'CMakeLists.txt').read_text()
            self.assertNotIn('hello_grpc', cmake)
            self.assertIn('broker_common', cmake)
            self.assertIn('broker_openapi_generate', cmake)
            self.assertEqual('deps_.pg_cluster->Execute' in (directory / 'src/bootstrap/ping.cpp').read_text(), pg)
            self.assertEqual('userver/storages/postgres/' in (directory / 'src/bootstrap/ping.cpp').read_text(), pg)
            self.assertEqual('pg_cluster' in (directory / 'src/deps.hpp').read_text(), pg)
            self.assertFalse((directory / 'src/domain/greeting.hpp').exists())
            self.assertFalse((directory / 'tests/unit/greeting_test.cpp').exists())
            with self.assertRaises(ValueError): create_service.create(name, pg, grpc)

    def test_manifest_generates_auth_and_rejects_incomplete_policy(self):
        directory = create_service.create('secured')
        contract = directory / 'docs/api/hello.yaml'
        document = yaml.safe_load((ROOT / 'services/broker/docs/api/hello.yaml').read_text())
        document['paths'] = {
            path.replace('/hello', '/api/hello'): value for path, value in document['paths'].items()
        }
        document.setdefault('components', {})['securitySchemes'] = {
            'Bearer': {'type': 'http', 'scheme': 'bearer', 'bearerFormat': 'JWT'}
        }
        document['security'] = [{'Bearer': []}]
        contract.write_text(yaml.safe_dump(document, sort_keys=False))
        manifest = directory / 'service.yaml'
        spec = yaml.safe_load(manifest.read_text())
        spec['auth'] = {
            'audience': 'secured-api',
            'roles-client': 'secured-api',
            'protected-prefix': '/api/',
            'permissions': {path: 'configs.read' for path in document['paths']},
        }
        manifest.write_text(yaml.safe_dump(spec, sort_keys=False))
        run(directory)
        run(directory, check=True)
        config = yaml.safe_load((directory / 'configs/static_config.yaml').read_text())[
            'components_manager'
        ]['components']
        self.assertEqual(config['platform-auth']['permissions'], spec['auth']['permissions'])
        self.assertEqual(config['platform-http-pipeline']['extra-middlewares'], ['platform-auth'])
        self.assertEqual(config['platform-auth']['issuer'], '$auth-issuer')
        main = (directory / 'src/main.cpp').read_text()
        self.assertEqual(main.count('Append<platform::auth::Component>()'), 1)
        self.assertEqual(main.count('Append<userver::components::HttpClient>("auth-http-client")'), 1)
        self.assertIn('broker_auth', (directory / 'CMakeLists.txt').read_text())
        # Ручная реализация сохраняется, повторная генерация ничего не меняет.
        before = {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()}
        run(directory)
        self.assertEqual(before, {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()})
        for mutation in ('missing', 'unknown', 'no-auth', 'public'):
            broken = yaml.safe_load(yaml.safe_dump(spec))
            if mutation == 'missing':
                broken['auth']['permissions'].pop(next(iter(document['paths'])))
            elif mutation == 'unknown':
                broken['auth']['permissions']['/api/unknown'] = 'configs.read'
            elif mutation == 'no-auth':
                del broken['auth']
            else:
                document['security'] = []
                contract.write_text(yaml.safe_dump(document, sort_keys=False))
            manifest.write_text(yaml.safe_dump(broken, sort_keys=False))
            before = {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()}
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                run(directory)
            self.assertEqual(before, {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()})

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
        self.assertFalse((self.root / 'schemas/services/rollback_test').exists())
        self.assertFalse((self.root / 'schemas/configs/rollback_test').exists())
        self.assertEqual(before, {p: p.read_bytes() for p in paths})

    def test_manifest_enables_features_and_preserves_manual_code(self):
        directory = create_service.create('upgrade')
        specification = directory / 'docs/api/hello.yaml'
        specification.write_text((ROOT / 'services/broker/docs/api/hello.yaml').read_text())
        run(directory)
        main = directory / 'src/main.cpp'
        main.write_text(main.read_text() + '\n// Ручной код сохраняется.\n')
        view = directory / 'src/views/hello/view.cpp'
        view.write_text(view.read_text() + '\n// Ручная реализация.\n')
        view_before = view.read_bytes()
        manifest = directory / 'service.yaml'
        manifest.write_text('version: 1\nlibraries: [common]\npostgresql: {enabled: true}\ngrpc: {enabled: false}\n')
        before = {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()}
        with self.assertRaisesRegex(ValueError, 'make gen'):
            run(directory, check=True)
        self.assertEqual(before, {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()})
        run(directory)
        self.assertTrue((directory / 'postgresql/schemas/db_1/.gitkeep').exists())
        manifest.write_text(manifest.read_text().replace('grpc: {enabled: false}', 'grpc: {enabled: true}'))
        run(directory)
        run(directory, check=True)
        self.assertIn('Ручной код сохраняется', main.read_text())
        self.assertEqual(view_before, view.read_bytes())
        self.assertTrue((directory / 'proto/.gitkeep').exists())
        demo = directory / 'src/transport/grpc/custom.cpp'
        demo.parent.mkdir(parents=True)
        demo.write_text('// Ручная реализация gRPC сохраняется.\n')
        before = {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()}
        run(directory)
        self.assertEqual(before, {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()})

    def test_outbox_generates_component_and_one_immutable_migration(self):
        directory = create_service.create('queued', postgres=True)
        manifest = directory / 'service.yaml'
        manifest.write_text('version: 1\nlibraries: [common, outbox]\npostgresql: {enabled: true, cluster: queued}\n')
        run(directory)
        migrations = list((directory / 'postgresql/schemas/db_1').glob('V*__create_outbox.sql'))
        self.assertEqual(len(migrations), 1)
        migration = migrations[0]
        original = migration.read_bytes()
        self.assertEqual(original, (self.root / 'libs/outbox/postgresql/schema.sql').read_bytes())
        self.assertIn('platform::outbox::Component', (directory / 'src/main.cpp').read_text())
        self.assertIn('postgres-component: postgres-queued', (directory / 'configs/static_config.yaml').read_text())
        self.assertIn('broker_outbox', (directory / 'CMakeLists.txt').read_text())
        static = directory / 'configs/static_config.yaml'
        static.write_text(static.read_text().replace(
            'postgres-component: postgres-queued',
            'postgres-component: postgres-queued\n            max-concurrency: 6\n            topic-concurrency:\n                test.event: 3'))
        # Обновление шаблона библиотеки не переписывает уже выпущенную миграцию.
        template = self.root / 'libs/outbox/postgresql/schema.sql'
        template.write_text(template.read_text() + '\n-- Следующая версия схемы.\n')
        run(directory)
        run(directory, check=True)
        self.assertEqual(migration.read_bytes(), original)
        self.assertIn('max-concurrency: 6', static.read_text())
        self.assertIn('test.event: 3', static.read_text())
        self.assertEqual(len(list(migration.parent.glob('V*__create_outbox.sql'))), 1)

    def test_outbox_requires_postgresql(self):
        directory = create_service.create('no_database')
        (directory / 'service.yaml').write_text('version: 1\nlibraries: [common, outbox]\n')
        with self.assertRaisesRegex(ValueError, 'outbox требует postgresql'):
            run(directory)

    def test_manifest_validation_and_additional_library(self):
        directory = create_service.create('manifest')
        manifest = directory / 'service.yaml'
        for content in ['version: 2', 'version: 1\npostgresql: {enabled: yes_}', 'version: 1\nunknown: true', 'version: 1\nlibraries: [../escape]', 'version: 1\nlibraries: [common, unknown]', 'version: 1\nlibraries: [common, common]', 'version: 1\nlibraries: [common]\nlibraries: [common]']:
            manifest.write_text(content)
            before = {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()}
            with self.assertRaises(ValueError):
                run(directory)
            self.assertEqual(before, {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()})
        library = self.root / 'libs/example'
        library.mkdir()
        (library / 'CMakeLists.txt').write_text('add_library(broker_example INTERFACE)\n')
        manifest.write_text('version: 1\nlibraries: [common, example]\n')
        run(directory)
        self.assertIn('broker_common broker_example', (directory / 'CMakeLists.txt').read_text())
        manifest.write_text('version: 1\nlibraries: [common]\npostgresql: {enabled: true}\n')
        run(directory)
        manifest.write_text('version: 1\nlibraries: [common]\n')
        with self.assertRaisesRegex(ValueError, 'Отключение postgresql'):
            run(directory)

    def test_postgresql_cluster_name_and_rename(self):
        directory = create_service.create('named_cluster', True)
        manifest = directory / 'service.yaml'
        for cluster in ['primary', 'accounting-read']:
            manifest.write_text(f'version: 1\npostgresql: {{enabled: true, cluster: {cluster}}}\n')
            run(directory)
            run(directory, check=True)
            config = yaml.safe_load((directory / 'configs/static_config.yaml').read_text())['components_manager']['components']
            self.assertIn('postgres-' + cluster, config)
            self.assertEqual([key for key in config if key.startswith('postgres-')], ['postgres-' + cluster])
            for path in ['src/main.cpp', 'src/deps.hpp']:
                self.assertIn('"postgres-' + cluster + '"', (directory / path).read_text())
        for invalid in ['../escape', 'two words', '', 1, 'UPPER', 'bad__name']:
            manifest.write_text(yaml.safe_dump({'version': 1, 'postgresql': {'enabled': True, 'cluster': invalid}}))
            before = {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()}
            with self.subTest(cluster=invalid), self.assertRaisesRegex(ValueError, 'postgresql.cluster'):
                run(directory)
            self.assertEqual(before, {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()})

    def test_manifest_write_failure_rolls_back(self):
        directory = create_service.create('transaction')
        manifest = directory / 'service.yaml'
        manifest.write_text('version: 1\nlibraries: [common]\npostgresql: {enabled: true}\n')
        before = {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()}
        original = Path.write_text
        def fail(path, *args, **kwargs):
            if path == directory / 'tests/conftest.py':
                raise OSError('Тестовый отказ генерации')
            return original(path, *args, **kwargs)
        with mock.patch.object(Path, 'write_text', fail):
            with self.assertRaises(OSError):
                run(directory)
        self.assertEqual(before, {p: p.read_bytes() for p in directory.rglob('*') if p.is_file()})
