"""Проверка описаний PostgreSQL и чистой генерации C++, без создания миграций."""

from pathlib import Path
import sys
import tempfile
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from generate import UniqueLoader
import pg_types


class PgTypesTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.service = Path(self.temporary.name) / 'sample'
        (self.service / 'postgresql/schemas/db_1').mkdir(parents=True)
        self.migration = self.service / 'postgresql/schemas/db_1/V0001__create_type.sql'
        self.migration.write_text('-- Миграция написана вручную\n')
        (self.service / 'service.yaml').write_text('postgresql: {enabled: true}\n')

    def write(self, types):
        path = self.service / 'postgresql/types.yaml'
        path.write_text(yaml.safe_dump({'version': 1, 'types': types}, sort_keys=False))

    def test_versions_nested_types_arrays_and_nullable(self):
        self.write(
            {
                'public.sample_v2': {
                    'fields': [
                        {'name': 'previous', 'type': 'public.sample_v1'},
                        {'name': 'identifiers', 'type': 'uuid[]'},
                        {'name': 'value', 'type': 'jsonb', 'nullable': True},
                    ]
                },
                'public.sample_v1': {
                    'fields': [
                        {'name': 'id', 'type': 'uuid'},
                        {'name': 'name', 'type': 'text'},
                    ]
                },
            }
        )
        before = {path: path.read_bytes() for path in self.service.rglob('*') if path.is_file()}
        first = pg_types.outputs(self.service, UniqueLoader)
        self.assertEqual(first, pg_types.outputs(self.service, UniqueLoader))
        self.assertEqual(
            before, {path: path.read_bytes() for path in self.service.rglob('*') if path.is_file()}
        )
        header = first['include/postgresql/types.hpp']
        self.assertLess(header.index('struct SampleV1'), header.index('struct SampleV2'))
        self.assertIn('SampleV1 Previous', header)
        self.assertIn('std::vector<boost::uuids::uuid> Identifiers', header)
        self.assertIn('std::optional<userver::formats::json::Value> Value', header)
        self.assertIn('CppToUserPg<::sample::pgtypes::SampleV1>', header)
        self.assertIn('"public.sample_v2"', header)

    def test_invalid_descriptions(self):
        cases = [
            {'public.sample': {'fields': [{'name': 'id', 'type': 'uuid'}]}},
            {'public.sample_v1': {'fields': [{'name': 'id', 'type': 'numeric'}]}},
            {'public.sample_v1': {'fields': [{'name': 'id', 'type': 'uuid', 'nullable': 'yes'}]}},
            {
                'public.sample_v1': {
                    'fields': [{'name': 'id', 'type': 'uuid'}, {'name': 'id', 'type': 'text'}]
                }
            },
            {'public.sample_v1': {'fields': [{'name': 'nested', 'type': 'public.sample_v1'}]}},
            {'public.sample_v1': {'fields': [{'name': 'ids', 'type': 'uuid[][]'}]}},
            {'public.sample_v1': {'fields': []}},
            {'public.sample_v1': {'fields': [{'name': 'id', 'type': 'uuid', 'default': 'bad'}]}},
        ]
        for types in cases:
            with self.subTest(types=types):
                self.write(types)
                with self.assertRaises(ValueError):
                    pg_types.outputs(self.service, UniqueLoader)

    def test_requires_postgresql_and_rejects_duplicate_yaml_keys(self):
        self.write({'public.sample_v1': {'fields': [{'name': 'id', 'type': 'uuid'}]}})
        (self.service / 'service.yaml').write_text('postgresql: {enabled: false}\n')
        with self.assertRaisesRegex(ValueError, 'postgresql.enabled'):
            pg_types.outputs(self.service, UniqueLoader)
        (self.service / 'postgresql/types.yaml').write_text('version: 1\nversion: 1\ntypes: {}\n')
        with self.assertRaisesRegex(ValueError, 'Повторяющийся ключ'):
            pg_types.outputs(self.service, UniqueLoader)
