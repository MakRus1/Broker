"""Контракты экспорта: glob URL, фильтрация смешанных файлов и проверка без записи."""
from pathlib import Path
import sys
import tempfile
import unittest
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from export_clients import export, matches


class ExportClientsTest(unittest.TestCase):
    def test_glob_segments(self):
        for path, pattern, expected in [
            ('/admin/configs/list', '/admin/**', True),
            ('/admin/configs/list', '/admin/*', False),
            ('/admin/list', '/admin/*', True),
            ('/foo/bar', '/foo/**/bar', True),
            ('/foo/a/b/bar', '/foo/**/bar', True),
            ('/foo', '/foo/**', True),
            ('/configs/status', '/configs/status', True),
        ]:
            with self.subTest(path=path, pattern=pattern):
                self.assertEqual(matches(path, pattern), expected)
        for pattern in ['configs/*', '/foo/**bar', '/[ab]/*', '/foo?']:
            with self.subTest(pattern=pattern), self.assertRaises(ValueError):
                matches('/foo', pattern)

    def test_filters_and_nonmutating_check(self):
        with tempfile.TemporaryDirectory() as temporary:
            service = Path(temporary) / 'services/example'
            docs = service / 'docs/api'
            docs.mkdir(parents=True)
            operation = {'get': {'responses': {'204': {'description': 'Готово'}}}}
            source = docs / 'mixed.yaml'
            source.write_text(yaml.safe_dump({'openapi': '3.0.3', 'info': {'title': 'API', 'version': '1'}, 'paths': {'/configs/status': operation, '/admin/configs/list': operation}}))
            policy = service / 'client-api.yaml'
            policy.write_text('version: 1\ninclude: [/configs/**]\n')
            export(service)
            target = Path(temporary) / 'schemas/services/example/mixed.yaml'
            self.assertEqual(set(yaml.safe_load(target.read_text())['paths']), {'/configs/status'})
            before = target.read_bytes()
            export(service, check=True)
            policy.write_text('version: 1\ninclude: [/**]\n')
            with self.assertRaises(ValueError):
                export(service, check=True)
            self.assertEqual(target.read_bytes(), before)
            export(service)
            self.assertEqual(set(yaml.safe_load(target.read_text())['paths']), {'/configs/status', '/admin/configs/list'})
            self.assertNotRegex(target.read_text(), r'[&*]id[0-9]+')
            policy.write_text('version: 1\nexclude: [/admin/**]\n')
            export(service)
            self.assertEqual(target.read_bytes(), before)
            self.assertIn('/admin/configs/list', source.read_text())

    def test_ambiguous_policy_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            service = Path(temporary) / 'services/example'
            service.mkdir(parents=True)
            for policy in ['version: 1\ninclude: []\nexclude: []', 'version: 1\ninclude: nope', 'version: true\ninclude: [/**]']:
                (service / 'client-api.yaml').write_text(policy)
                with self.subTest(policy=policy), self.assertRaises(ValueError):
                    export(service)
