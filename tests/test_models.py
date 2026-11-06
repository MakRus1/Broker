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


class CalendarDateModelTest(unittest.TestCase):
    def test_calendar_date_has_native_type_and_typed_validation(self):
        from model import parse_type
        from cpp_types import validator, literal
        value = parse_type({'type': 'string', 'format': 'date'}, 'day')
        self.assertEqual(value.cpp, 'userver::utils::datetime::Date')
        code = validator(value, 'Day')
        self.assertIn('Date{1, 1, 1}', code)
        self.assertNotIn('ToString', code)
        self.assertNotIn('CheckFormat', code)
        self.assertIn('ParseCalendarDate', literal(value, '2026-10-25'))


class InstantModelTest(unittest.TestCase):
    def test_date_time_has_native_type_without_string_validation(self):
        from model import parse_type
        from cpp_types import validator
        value = parse_type({'type': 'string', 'format': 'date-time'}, 'instant')
        self.assertEqual(value.cpp, 'std::chrono::system_clock::time_point')
        code = validator(value, 'Instant')
        self.assertNotIn('Utf8Length', code)
        self.assertNotIn('CheckFormat', code)


class CompositionTest(unittest.TestCase):
    def schemas(self):
        return {
            'Base': {'type': 'object', 'required': ['id'], 'properties': {'id': {'type': 'integer', 'minimum': 1}}},
            'Extended': {'allOf': [
                {'$ref': '#/components/schemas/Base'},
                {'type': 'object', 'required': ['name'], 'properties': {'name': {'type': 'string'}}},
            ]},
            'Choice': {'oneOf': [{'$ref': '#/components/schemas/Base'}, {'$ref': '#/components/schemas/Extended'}]},
        }

    def test_all_of_fields_constraints_and_required(self):
        models = {m.name: m for m in collect_models(parse_spec(specification(self.schemas())))}
        fields = {f.name: f for f in models['Extended'].body.fields}
        self.assertEqual(set(fields), {'id', 'name'})
        self.assertTrue(all(f.required for f in fields.values()))
        self.assertEqual(fields['id'].type.schema['minimum'], 1)

    def test_dependency_order_and_exactly_one_validation(self):
        operations = parse_spec(specification(self.schemas()))
        names = [m.name for m in collect_models(operations)]
        self.assertLess(names.index('Extended'), names.index('Choice'))
        header = outputs('example', operations, set())['include/openapi/models.hpp']
        self.assertIn('std::variant<::example::models::Base, ::example::models::Extended>', header)
        self.assertIn('matches != 1', header)
        self.assertIn('catch (const std::bad_alloc&)', header)

    def test_all_of_rejects_silent_semantic_changes(self):
        cases = [
            [], [{'type': 'integer'}],
            [{'type': 'object', 'required': ['missing']}],
            [{'type': 'object', 'properties': {'id': {'type': 'integer'}}},
             {'type': 'object', 'properties': {'id': {'type': 'string'}}}],
            [{'type': 'object', 'additionalProperties': False, 'properties': {'a': {'type': 'string'}}},
             {'type': 'object', 'properties': {'b': {'type': 'string'}}}],
            [{'$ref': '#/components/schemas/Bad'}],
        ]
        for parts in cases:
            with self.subTest(parts=parts), self.assertRaises(ValueError):
                parse_spec(specification({'Bad': {'allOf': parts}}))

    def test_one_of_requires_nonempty_valid_branches(self):
        for branches in [[], 'bad', [{}], [{'$ref': '#/components/schemas/Missing'}], [{'$ref': '#/components/schemas/Bad'}]]:
            with self.subTest(branches=branches), self.assertRaises(ValueError):
                parse_spec(specification({'Bad': {'oneOf': branches}}))

    def test_discriminator_mapping_validated(self):
        schemas = {
            'A': {'type': 'object', 'required': ['kind'], 'properties': {'kind': {'type': 'string', 'enum': ['a']}}},
            'Union': {'oneOf': [{'$ref': '#/components/schemas/A'}],
                      'discriminator': {'propertyName': 'kind', 'mapping': {'a': '#/components/schemas/A'}}},
        }
        parse_spec(specification(schemas))
        for discriminator in [{}, {'propertyName': 'missing'}, {'propertyName': 'kind', 'mapping': {'wrong': 'A'}},
                              {'propertyName': 'kind', 'mapping': {'a': 'Missing'}}]:
            invalid = deepcopy(schemas)
            invalid['Union']['discriminator'] = discriminator
            with self.subTest(discriminator=discriminator), self.assertRaises(ValueError):
                parse_spec(specification(invalid))

    def test_union_query_parameter_is_rejected(self):
        data = specification(self.schemas())
        data['paths']['/example']['get']['parameters'] = [
            {'in': 'query', 'name': 'choice', 'schema': {'$ref': '#/components/schemas/Choice'}}]
        with self.assertRaises(ValueError):
            parse_spec(data)

    def test_inline_union_names_are_local_to_operation(self):
        data = specification(self.schemas())
        union = {'oneOf': [{'$ref': '#/components/schemas/Base'}]}
        operation = {'post': {'requestBody': {'content': {'application/json': {'schema': union}}},
                              'responses': {'200': {'description': 'OK', 'content': {'application/json': {'schema': union}}}}}}
        data['paths'] = {'/first': deepcopy(operation), '/second': deepcopy(operation)}
        names = [m.name for m in collect_models(parse_spec(data))]
        self.assertIn('first_RequestBody', names)
        self.assertIn('second_RequestBody', names)
        generated = outputs('example', parse_spec(data), set())
        self.assertIn('ParseJson<RequestBody>(json)', generated['src/views/first/handler.cpp'])

    def test_generated_union_validator_name_conflicts_are_rejected(self):
        schemas = self.schemas()
        schemas['Validate_Choice_alternative_0'] = {'type': 'string'}
        with self.assertRaisesRegex(ValueError, 'validation name'):
            parse_spec(specification(schemas))

    def test_all_of_inline_unions_keep_owner_names(self):
        schemas = self.schemas()
        for owner, variant in [('First', 'Base'), ('Second', 'Extended')]:
            schemas[owner] = {'allOf': [{'type': 'object', 'properties': {
                'choice': {'oneOf': [{'$ref': '#/components/schemas/' + variant}]},
            }}]}
        models = {m.name: m for m in collect_models(parse_spec(specification(schemas)))}
        self.assertEqual(models['First'].body.fields[0].type.model_name, 'First_choice')
        self.assertEqual(models['Second'].body.fields[0].type.model_name, 'Second_choice')
        self.assertNotIn('Schema_choice', models)
