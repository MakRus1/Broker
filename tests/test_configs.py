"""Контракты конфигов, уникальные имена и воспроизводимая генерация."""
from copy import deepcopy
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from support.generator_repository import copy_generator_repository
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
import configs
from generate import UniqueLoader, run


class ConfigSchemasTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        copy_generator_repository(self.root)

    def tearDown(self):
        self.tmp.cleanup()

    def test_duplicates_and_invalid_defaults_rejected(self):
        path = self.root / 'schemas/configs/shipment/SHIPMENT_TERMS_CONFIG.yaml'
        original = yaml.safe_load(path.read_text())
        for mutation in [lambda value: value['default'].pop('is_enabled'),
                         lambda value: value['default'].update(max_days=0),
                         lambda value: value.update(context='unknown/CONTEXT'),
                         lambda value: value.update(mode='unknown'),
                         lambda value: value['schema'].update(additionalProperties=True)]:
            value = deepcopy(original);mutation(value)
            path.write_text(yaml.safe_dump(value))
            with self.subTest(value=value), self.assertRaises(ValueError):
                configs.catalog(self.root, UniqueLoader)
        path.write_text(yaml.safe_dump(original))
        duplicate = path.parent.parent / 'other' / path.name
        duplicate.parent.mkdir()
        duplicate.write_text(path.read_text())
        with self.assertRaisesRegex(ValueError, 'Duplicate global'):
            configs.catalog(self.root, UniqueLoader)

    def test_automatic_http_schema_is_provider_owned_and_plain(self):
        files = configs.auto_files(self.root / 'services/broker', UniqueLoader)
        target = self.root / 'schemas/configs/broker/BROKER_HTTP_CLIENT_CONFIG.yaml'
        self.assertIn(target, files)
        value = yaml.safe_load(files[target])
        self.assertEqual(value['mode'], 'http-client')
        self.assertNotIn('context', value)
        self.assertEqual(value['default']['**default**'], {'timeout': 1000, 'max_retries': 0})
        self.assertIn('/hello/create', value['paths'])

    def test_plain_config_has_typed_key(self):
        path = self.root / 'schemas/configs/shipment/PLAIN_CONFIG.yaml'
        path.write_text(yaml.safe_dump({'name': 'PLAIN_CONFIG', 'mode': 'plain',
            'schema': {'type': 'object', 'required': ['is_enabled'], 'properties': {'is_enabled': {'type': 'boolean'}}},
            'default': {'is_enabled': True}}))
        manifest = self.root / 'services/broker/service.yaml'
        spec = yaml.safe_load(manifest.read_text());spec['configs'] = ['PLAIN_CONFIG']
        manifest.write_text(yaml.safe_dump(spec))
        generated = configs.outputs(self.root / 'services/broker', UniqueLoader)['include/configs/keys.hpp']
        self.assertIn('PlainKey<configs::detail_PLAIN_CONFIG::Value>', generated)
        self.assertIn('bool IsEnabled', generated)
        self.assertNotIn('ConditionalKey', generated)

    def test_manifest_resolves_global_names_without_paths(self):
        service = self.root / 'services/broker'
        manifest = service / 'service.yaml'
        spec = yaml.safe_load(manifest.read_text())
        _, catalog, entries = configs.selected(service, UniqueLoader)
        self.assertEqual([catalog[entry]['name'] for entry in entries], ['SHIPMENT_TERMS_CONFIG'])
        for references in [['shipment/SHIPMENT_TERMS_CONFIG'], ['UNKNOWN_CONFIG'],
                           ['SHIPMENT_TERMS_CONFIG', 'SHIPMENT_TERMS_CONFIG']]:
            spec['configs'] = references
            manifest.write_text(yaml.safe_dump(spec))
            with self.subTest(references=references), self.assertRaises(ValueError):
                configs.selected(service, UniqueLoader)

    def test_critical_policies_are_explicit_and_validated(self):
        service = self.root / 'services/broker'
        manifest = yaml.safe_load((service / 'service.yaml').read_text())
        valid = {'max_age_seconds': 60, 'allow_cache': True, 'allow_builtin': False}
        manifest['critical_dependencies'] = {'configs': {'SHIPMENT_TERMS_CONFIG': valid}}
        rules = configs.readiness_rules(service, manifest, UniqueLoader)
        self.assertEqual(rules[0]['max-age-seconds'], 60)
        self.assertFalse(rules[0]['allow-builtin'])
        self.assertEqual(len(rules[0]['schema-hash']), 64)
        for name, policy in [('UNKNOWN', valid), ('SHIPMENT_TERMS_CONFIG', {}),
                             ('SHIPMENT_TERMS_CONFIG', {'max_age_seconds': True}),
                             ('SHIPMENT_TERMS_CONFIG', {'max_age_seconds': 0}),
                             ('SHIPMENT_TERMS_CONFIG', {'max_age_seconds': 60, 'allow_cache': 'yes'}),
                             ('SHIPMENT_TERMS_CONFIG', {'max_age_seconds': 60, 'typo': False})]:
            manifest['critical_dependencies'] = {'configs': {name: policy}}
            with self.subTest(name=name, policy=policy), self.assertRaises(ValueError):
                configs.readiness_rules(service, manifest, UniqueLoader)

    def test_migration_names_and_duplicate_versions(self):
        spec = importlib.util.spec_from_file_location('migration_check', ROOT / 'scripts/check-migrations.py')
        module = importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        module.check(self.root)
        database = self.root / 'services/broker/postgresql/schemas/db_1'
        for name in ['001_old.sql', 'V0000__zero.sql', 'V0001__duplicate.sql']:
            target = database / name;target.write_text('SELECT 1;')
            with self.subTest(name=name), self.assertRaises(ValueError):
                module.check(self.root)
            target.unlink()


class JsonSchemaConfigsTest(unittest.TestCase):
    def test_native_query_map_and_default_validation(self):
        schema = {
            'type': 'object',
            'additionalProperties': {'$ref': '#/definitions/Timeout'},
            'definitions': {'Timeout': {
                'type': 'object', 'additionalProperties': False,
                'properties': {'network_timeout_ms': {'type': 'integer', 'minimum': 1}},
            }},
        }
        configs.validate_json_schema(schema)
        spec = {'name': 'SQL_SETTINGS', 'mode': 'json-schema', 'schema': schema,
                'default': {'some-query': {'network_timeout_ms': 1000}}}
        configs.validate_default(spec)
        for invalid in [{'some-query': {'network_timeout_ms': 0}},
                        {'some-query': {'network_timeout_ms': '1000'}},
                        {'some-query': {'unexpected': True}}]:
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                configs.validate_default({**spec, 'default': invalid})

    def test_external_and_missing_references_are_rejected(self):
        for ref in ['https://example.org/schema.json', '#/definitions/Missing']:
            with self.subTest(ref=ref), self.assertRaises(ValueError):
                configs.validate_json_schema({'type': 'object', 'additionalProperties': {'$ref': ref}})

    def test_invalid_schema_is_rejected(self):
        with self.assertRaises(ValueError):
            configs.validate_json_schema({'type': 'object', 'properties': {'a': {'type': 'invalid'}}})
