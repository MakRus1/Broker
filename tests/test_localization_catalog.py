"""Релизный каталог проверяется и регистрируется отдельно от образа Localization."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import localization_catalog as catalog


class LocalizationCatalogTest(unittest.TestCase):
    def test_builtin_catalog_is_valid(self):
        value = catalog.load(Path(__file__).resolve().parents[1])
        self.assertGreater(len(value['ru']), 1000)

    def test_locale_keys_and_parameters_match(self):
        for value in [
            {'ru': {'a': 'A'}, 'en': {}},
            {'ru': {'a': '{count}'}, 'en': {'a': '{name}'}},
            {'ru': {'a': ''}, 'en': {'a': 'Text'}},
        ]:
            with self.assertRaises(ValueError):
                catalog.validate(value)

    def test_registration_uses_service_token_and_api(self):
        value = {'ru': {'a': "Текст ' {count}"}, 'en': {'a': 'Text {count}'}}
        with mock.patch.object(catalog, 'request_json', side_effect=[{'access_token': 'token'}, {'registered': 1}]) as request:
            self.assertEqual(catalog.register('http://source', 'http://auth/token', 'secret', value), {'registered': 1})
        self.assertEqual(request.call_args_list[0].args, ('http://auth/token', {'grant_type': 'client_credentials', 'client_id': 'localization-publisher', 'client_secret': 'secret'}))
        self.assertEqual(request.call_args_list[1].args[0], 'http://source/api/internal/localization/sources/upsert')
        self.assertEqual(request.call_args_list[1].args[2], 'token')
        self.assertEqual(request.call_args_list[1].args[1]['ru'], [{'key': 'a', 'value': "Текст ' {count}"}])

    def test_duplicate_keys_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'frontend/shared/i18n'
            target.mkdir(parents=True)
            (target / 'ru.json').write_text('{"a":"A","a":"B"}')
            (target / 'en.json').write_text('{"a":"A"}')
            with self.assertRaises(ValueError):
                catalog.load(directory)


    def test_ci_rejects_changed_parameters_but_allows_new_and_removed_keys(self):
        import subprocess
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / 'frontend/shared/i18n'
            target.mkdir(parents=True)
            def write(values):
                for locale in ('ru', 'en'):
                    (target / f'{locale}.json').write_text(json.dumps(values))
            write({'stable': 'Text {count}', 'removed': 'Gone'})
            def git(*args):
                return subprocess.check_output(['git', *args], cwd=root, text=True).strip()
            git('init', '-q')
            git('config', 'user.name', 'Test')
            git('config', 'user.email', 'test@example.invalid')
            git('add', '.')
            git('commit', '-qm', 'base')
            base = git('rev-parse', 'HEAD')
            write({'stable': 'Changed {count} {count}', 'new': '{name}'})
            catalog.check_compatibility(root, base)
            write({'stable': 'Changed {name}'})
            with self.assertRaisesRegex(ValueError, 'ru: stable'):
                catalog.check_compatibility(root, base)
            write({'stable': 'No parameters'})
            with self.assertRaisesRegex(ValueError, 'stable'):
                catalog.check_compatibility(root, base)
