"""Контрактные проверки генерации и неизменности ручного кода."""
from copy import deepcopy
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
import yaml

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from generate import load, run
from model import parse_spec
from render import outputs


def spec():
    data = {'openapi': '3.0.3', 'info': {'title': 'Тест', 'version': '1'}, 'paths': {
        '/items/{id}': {'parameters': [{'name': 'id', 'in': 'path', 'required': True, 'schema': {'type': 'integer'}}],
            'get': {'parameters': [{'name': 'active', 'in': 'query', 'schema': {'type': 'boolean'}}], 'responses': {'204': {'description': 'Нет тела'}}},
            'post': {'requestBody': {'required': True, 'content': {'application/json': {'schema': {'type': 'object', 'additionalProperties': False, 'required': ['name'], 'properties': {'name': {'type': 'string'}, 'count': {'type': 'integer'}, 'tags': {'type': 'array', 'items': {'type': 'string'}}}}}}},
                'responses': {'201': {'description': 'Создан', 'content': {'application/json': {'schema': {'type': 'object', 'properties': {'id': {'type': 'integer'}}, 'required': ['id']}}}},
                              '409': {'description': 'Конфликт', 'content': {'application/json': {'schema': {'type': 'object', 'properties': {'id': {'type': 'integer'}}, 'required': ['id']}}}}}}}}}

    item = data['paths']['/items/{id}']
    data['paths']['/items/{id}/create'] = {'parameters': deepcopy(item['parameters']), 'post': item.pop('post')}
    return data


class CodegenTest(unittest.TestCase):
    def test_distinct_paths_and_response_statuses(self):
        ops = parse_spec(spec())
        self.assertEqual([o.method for o in ops], ['GET', 'POST'])
        result = outputs('example', ops, set())
        self.assertIn('struct Response201', result['include/views/items/by_id/create/responses.hpp'])
        self.assertIn('struct Response409', result['include/views/items/by_id/create/responses.hpp'])
        cpp = result['src/views/items/by_id/create/handler.cpp']
        self.assertIn('RequestBody()', cpp)
        self.assertIn('ParseJson<std::string>', cpp)
        self.assertIn('HttpStatus>(201)', cpp)
        self.assertIn('HttpStatus>(204)', result['src/views/items/by_id/handler.cpp'])
        self.assertIn('std::optional<bool>', result['include/views/items/by_id/requests.hpp'])
        self.assertNotIn('RequestBody', result['include/views/items/by_id/requests.hpp'])

    def test_explicit_rejection_of_unsupported_contracts(self):
        for key, value in [('nullable', 'yes'), ('enum', []), ('format', 'unknown'), ('minimum', 0), ('default', 3)]:
            with self.subTest(key=key):
                data = spec()
                data['paths']['/items/{id}/create']['post']['requestBody']['content']['application/json']['schema']['properties']['name'][key] = value
                with self.assertRaises(ValueError):
                    parse_spec(data)
        for mutation in [lambda d: d.update(openapi='3.1.0'),
                         lambda d: d.update(security=[]),
                         lambda d: d['paths']['/items/{id}']['get']['responses'].update(default={'description': 'Любой'}),
                         lambda d: d['paths']['/items/{id}']['parameters'].clear()]:
            data = spec(); mutation(data)
            with self.assertRaises(ValueError):
                parse_spec(data)

    def test_defaults_and_constraints_rejected_before_generation(self):
        from model import parse_type
        cases = [
            {'type': 'integer', 'format': 'int32', 'default': 2147483648},
            {'type': 'number', 'multipleOf': 0},
            {'type': 'integer', 'minimum': 10, 'maximum': 0},
            {'type': 'number', 'exclusiveMinimum': True},
            {'type': 'number', 'exclusiveMinimum': 1},
            {'type': 'string', 'enum': ['x', 'x']},
            {'type': 'string', 'enum': ['x'], 'default': 'other'},
            {'type': 'string', 'format': 'date', 'default': '2025-02-29'},
            {'type': 'string', 'pattern': '[', 'default': 'x'},
            {'type': 'string', 'pattern': '(?=a)'},
            {'type': 'string', 'pattern': 'a{1001}'},
            {'type': 'array', 'items': {'type': 'integer'}, 'uniqueItems': True, 'default': [1, 1]},
        ]
        for schema in cases:
            with self.subTest(schema=schema), self.assertRaises(ValueError):
                parse_type(schema)
        typ = parse_type({'type': 'string', 'enum': ['new', 'in_transit'], 'default': 'new'}, 'Status')
        self.assertEqual(typ.cpp, 'Status')

    def test_named_enum_has_direct_values(self):
        from cpp_types import enums
        from model import parse_type, enum_types
        typ = parse_type({'type': 'string', 'enum': ['success', 'new']}, 'status')
        code = enums(typ)
        self.assertIn('enum class status', code)
        self.assertIn('status::Success', code)
        self.assertIn('status::New', code)
        self.assertNotIn('Make_Enum', code)
        from cpp_types import enum_labels
        self.assertEqual(enum_labels(parse_type({'type': 'string', 'enum': ['in_transit']}, 'status')), ['InTransit'])
        with self.assertRaisesRegex(ValueError, 'конфликт'):
            enum_labels(parse_type({'type': 'string', 'enum': ['new', 'New']}, 'status'))
        with self.assertRaisesRegex(ValueError, 'допустимое'):
            enum_labels(parse_type({'type': 'string', 'enum': ['123']}, 'status'))
        operation = parse_spec(yaml.safe_load((ROOT / 'tests/fixtures/contract_service/docs/api/contract.yaml').read_text()))
        post = next(op for op in operation if op.path == '/validated/create' and op.method == 'POST')
        status = next(typ for typ in enum_types(post) if typ.enum_name == 'status')
        self.assertIn('success', status.schema['enum'])
        self.assertIn('new', status.schema['enum'])

    def test_scaffold_check_and_build_are_separate(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp); service = base / 'example'
            (service / 'docs/api').mkdir(parents=True)
            (service / 'configs').mkdir()
            (service / 'docs/api/api.yaml').write_text(yaml.safe_dump(spec()))
            (service / 'configs/static_config.yaml').write_text('components_manager:\n    components:\n        # OPENAPI_HANDLERS_BEGIN\n        # OPENAPI_HANDLERS_END\n')
            before = self.snapshot(service)
            with self.assertRaises(ValueError):
                run(service, check=True)
            self.assertEqual(before, self.snapshot(service))
            run(service)
            hand = service / 'src/views/items/by_id/view.cpp'
            hand.write_text('// Ручная реализация\n')
            run(service)
            self.assertEqual(hand.read_text(), '// Ручная реализация\n')
            before = self.snapshot(service)
            run(service, check=True)
            run(service, output=base / 'generated')
            self.assertEqual(before, self.snapshot(service))
            first = self.snapshot(base / 'generated')
            run(service, output=base / 'generated')
            self.assertEqual(first, self.snapshot(base / 'generated'))
            spec_path = service / 'docs/api/api.yaml'
            spec_path.write_text(spec_path.read_text() + '\n# Изменение контракта\n')
            with self.assertRaises(ValueError):
                run(service, check=True)

    def test_duplicate_yaml_keys_rejected(self):
        import generate
        with tempfile.TemporaryDirectory() as tmp:
            service = Path(tmp) / 'example'
            (service / 'docs/api').mkdir(parents=True)
            (service / 'docs/api/api.yaml').write_text('openapi: 3.0.3\nopenapi: 3.1.0\n')
            with self.assertRaises(ValueError):
                generate.load(service)

    def test_delete_uses_valid_cpp_namespace(self):
        data = spec()
        item = data['paths']['/items/{id}']
        item['delete'] = item.pop('get')
        ops = parse_spec(data)
        self.assertEqual(ops[0].rel, 'items/by_id')
        self.assertEqual(ops[0].name, 'ItemsById')

    def test_path_collisions_and_traversal(self):
        for path in ['/../../outside', '/a//b', '/class', '/{id}/{id}']:
            data = spec(); data['paths'] = {path: data['paths']['/items/{id}']}
            with self.assertRaises(ValueError): parse_spec(data)

    def test_paths_are_the_only_operation_identity(self):
        data = {'openapi': '3.0.3', 'paths': {
            '/api/v1/foo/bar': {'get': {'responses': {'204': {'description': 'Нет тела'}}}}}}
        op, = parse_spec(data)
        self.assertEqual(op.rel, 'api/v1/foo/bar')
        self.assertEqual(op.ns, 'api::v1::foo::bar')
        self.assertEqual(op.name, 'ApiV1FooBar')
        data['paths']['/api/v1/foo/bar']['get']['operationId'] = 'OtherName'
        with self.assertRaisesRegex(ValueError, 'operationId'):
            parse_spec(data)

    def test_multiple_methods_and_ambiguous_names_are_rejected(self):
        operation = {'get': {'responses': {'204': {'description': 'Нет тела'}}}}
        for paths in [
            {'/foo': {**operation, 'post': operation['get']}},
            {'/foo/': operation},
            {'/foo-bar': operation, '/foo_bar': operation},
            {'/foo_bar': operation, '/foo/bar': operation},
            {'/foo': operation, '/foo/Request': operation},
            {'/component': operation},
            {'/': operation, '/root': operation},
        ]:
            with self.subTest(paths=paths), self.assertRaises(ValueError):
                parse_spec({'openapi': '3.0.3', 'paths': paths})

    def test_duplicate_path_across_server_specs_does_not_write(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            service = root / 'services/example'
            public = root / 'schemas/services/example'
            (service / 'docs/api').mkdir(parents=True)
            (service / 'configs').mkdir()
            public.mkdir(parents=True)
            for path, method in [(service / 'docs/api/external.yaml', 'get'), (service / 'docs/api/internal.yaml', 'post')]:
                path.write_text(yaml.safe_dump({'openapi': '3.0.3', 'paths': {
                    '/foo': {method: {'responses': {'204': {'description': 'Нет тела'}}}}}}))
            (service / 'configs/static_config.yaml').write_text('# OPENAPI_HANDLERS_BEGIN\n# OPENAPI_HANDLERS_END\n')
            before = self.snapshot(root)
            with self.assertRaisesRegex(ValueError, 'Colliding path'):
                run(service)
            self.assertEqual(before, self.snapshot(root))

    @staticmethod
    def snapshot(directory):
        return {p.relative_to(directory).as_posix(): p.read_bytes() for p in directory.rglob('*') if p.is_file()}


if __name__ == '__main__':
    unittest.main()


class JsonObjectContractTest(unittest.TestCase):
    def test_object_fields_require_explicit_free_form_schema(self):
        from model import parse_type
        self.assertEqual(parse_type({'type': 'object', 'additionalProperties': True}).cpp,
                         'userver::formats::json::Value')
        for schema in [{'type': 'object'}, {'type': 'object', 'properties': {}},
                       {'type': 'object', 'additionalProperties': {'type': 'string'}}]:
            with self.subTest(schema=schema), self.assertRaises(ValueError):
                parse_type(schema)

    def test_admin_listener_is_generated_from_path(self):
        from render import config_block
        data = {'openapi': '3.0.3', 'paths': {'/admin/configs/list': {'get': {'responses': {'204': {'description': 'Готово'}}}}}}
        self.assertIn('monitor-handler: true', config_block(parse_spec(data)))
        data['paths']['/configs/status'] = data['paths'].pop('/admin/configs/list')
        self.assertNotIn('monitor-handler: true', config_block(parse_spec(data)))
