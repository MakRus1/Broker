"""Разрешение схем, конфликтов и ограничений до записи сгенерированных файлов."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from model import collect_models, parse_spec, validate_operations
from render import outputs
from generate import load


def specification(schemas):
    return {'openapi': '3.0.3', 'components': {'schemas': schemas}, 'paths': {
        '/example': {'get': {'responses': {'204': {'description': 'Нет тела'}}}}}}


class ModelsTest(unittest.TestCase):
    def test_optional_access_is_explicit(self):
        schema = specification({'Settings': {
            'type': 'object',
            'additionalProperties': False,
            'properties': {
                'enabled': {'type': 'boolean'},
                'count': {'type': 'integer', 'nullable': True},
            },
        }})
        generated = outputs('example', parse_spec(schema), set())
        header = generated['include/openapi/models.hpp']
        # optional<bool>{false} имеет значение: проверка не должна путать его с отсутствием.
        self.assertIn('value.enabled.has_value()', header)
        self.assertIn('value.enabled.value()', header)
        self.assertIn('value.has_value()', header)
        self.assertIn('value.value()', header)
        self.assertNotIn('*value.', header)

    def test_shared_types_and_dependency_order(self):
        path = ROOT / 'tests/fixtures/contract_service/docs/api/models.yaml'
        operations = load(ROOT / 'tests/fixtures/contract_service')
        models = collect_models(operations)
        names = [model.name for model in models]
        self.assertLess(names.index('Address'), names.index('Profile'))
        self.assertLess(names.index('Profile'), names.index('ProfileAlias'))
        generated = outputs('example', operations, set())
        header = generated['include/openapi/models.hpp']
        self.assertEqual(header.count('struct Profile :'), 1)
        self.assertIn('using ProfileAlias = ::example::models::Profile;', header)
        self.assertIn('enum class State', header)
        request = generated['include/views/models/echo/requests.hpp']
        self.assertIn('using RequestBody = ::example::models::Payload;', request)
        self.assertNotIn('struct RequestBody', request)
        self.assertIn('::example::models::ProfileAlias', header)
        self.assertIn('std::vector<::example::models::Profile>', header)
        self.assertIn('std::optional<std::optional<std::string>>', header)
        responses = generated['include/views/models/echo/responses.hpp']
        # Даже при одной модели тела статусы остаются разными типами variant.
        self.assertIn('struct Response400', responses)
        self.assertIn('struct Response409', responses)

    def test_rejects_recursive_and_unresolved_references(self):
        for schemas in [
            {'A': {'$ref': '#/components/schemas/A'}},
            {'A': {'$ref': '#/components/schemas/B'}, 'B': {'$ref': '#/components/schemas/A'}},
            {'A': {'type': 'object', 'properties': {'child': {'$ref': '#/components/schemas/A'}}}},
            {'A': {'type': 'array', 'items': {'$ref': '#/components/schemas/A'}}},
            {'A': {'$ref': '#/components/schemas/Missing'}},
            {'A': {'$ref': '../outside.yaml#/components/schemas/B'}},
            {'A': {'$ref': 'https://example.org/spec.yaml#/components/schemas/B'}},
        ]:
            with self.subTest(schemas=schemas), self.assertRaises(ValueError):
                parse_spec(specification(schemas))

    def test_unused_models_and_reference_siblings_are_validated(self):
        for schemas in [
            {'A': {'type': 'string', 'minLength': -1}},
            {'A': {'type': 'object', 'properties': {'value': {'type': 'unknown'}}}},
            {'A': {'$ref': '#/components/schemas/B', 'description': 'Нельзя'}, 'B': {'type': 'string'}},
            {'A': {'$ref': '#/components/schemas/B', 'nullable': True}, 'B': {'type': 'string'}},
            {'class': {'type': 'string'}}, {'models': {'type': 'string'}},
            {'std': {'type': 'integer'}},
            {'A': {'type': 'object', 'properties': {'id': {'type': 'integer'}}},
             'Validate_A_id': {'type': 'integer'}},
            {'A': {'type': 'object', 'properties': {'state': {'type': 'string', 'enum': ['ok']}}},
             'A_state': {'type': 'string'}},
        ]:
            with self.subTest(schemas=schemas), self.assertRaises(ValueError):
                parse_spec(specification(schemas))

    def test_model_conflicts_between_files(self):
        first = parse_spec(specification({'Shared': {'type': 'string'}}))
        second_spec = specification({'Shared': {'type': 'integer'}})
        second_spec['paths']['/other'] = second_spec['paths'].pop('/example')
        with self.assertRaisesRegex(ValueError, 'Conflicting components.schemas'):
            validate_operations(first + parse_spec(second_spec))
        second_spec['components']['schemas']['Shared'] = {'type': 'string'}
        validate_operations(first + parse_spec(second_spec))
        self.assertEqual(len(collect_models(first + parse_spec(second_spec))), 1)

    def test_object_array_default_and_uniqueness_are_explicitly_rejected(self):
        for extra in [{'default': [{}]}, {'uniqueItems': True}]:
            data = specification({'Item': {'type': 'object', 'properties': {'id': {'type': 'integer'}}},
                                  'Items': {'type': 'array', 'items': {'$ref': '#/components/schemas/Item'}, **extra}})
            with self.subTest(extra=extra), self.assertRaisesRegex(ValueError, 'default and uniqueItems'):
                parse_spec(data)

    def test_scalar_reference_default_is_validated(self):
        data = specification({'Number': {'type': 'integer', 'minimum': 1},
                              'Numbers': {'type': 'array', 'items': {'$ref': '#/components/schemas/Number'},
                                          'default': [0]}})
        with self.assertRaises(ValueError):
            parse_spec(data)
