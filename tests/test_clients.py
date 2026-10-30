"""Проверки контрактов клиентов и независимости от серверной реализации."""
from copy import deepcopy
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from clients import client_specs, client_outputs, operation_code
from generate import UniqueLoader, load, digest
from model import parse_spec


class ClientTest(unittest.TestCase):
    def test_manifest_validation_and_confinement(self):
        with tempfile.TemporaryDirectory() as tmp:
            service = Path(tmp)
            cases = [[], {'bad-name': {'service': 'broker', 'transport': 'http'}},
                     {'pg_cluster': {'service': 'broker', 'transport': 'http'}},
                     {'configs': {'service': 'broker', 'transport': 'http'}},
                     {'readiness': {'service': 'broker', 'transport': 'http'}},
                     {'api': {'service': '../broker', 'transport': 'http'}},
                     {'api': {'service': 'unknown', 'transport': 'http'}},
                     {'api': {'service': 'broker', 'transport': 'grpc'}},
                     {'api': {'service': 'broker', 'transport': 'http', 'operations': ['Hello']}},
                     {'api': {'service': 'broker', 'transport': 'http', 'url': 'http://localhost'}}]
            for clients in cases:
                (service / 'service.yaml').write_text(yaml.safe_dump({'version': 1, 'clients': clients}))
                with self.subTest(clients=clients), self.assertRaises((ValueError, TypeError)):
                    client_specs(service, UniqueLoader)

    def test_client_dto_and_response_decoding(self):
        provider = ROOT / 'tests/fixtures/contract_service'
        ops = load(provider)
        for op in ops:
            code = operation_code('::example::clients::api::' + op.name, op)
            self.assertNotIn('InputPresence', code)
            self.assertNotIn('MarkProvided', code)
            self.assertNotIn('RequestContext', code)
            self.assertIn('Undeclared HTTP response status', code)
        get = next(op for op in ops if op.path == '/items/{id}' and op.method == 'GET')
        code = operation_code('::example::clients::api::' + get.name, get)
        self.assertIn('ReplacePath', code)
        self.assertIn('using Response = void;', code)
        self.assertIn('unexpected body', code)
        post = next(op for op in ops if op.path == '/validated/create' and op.method == 'POST')
        code = operation_code('::example::clients::api::' + post.name, post)
        self.assertIn('enum class status', code)
        self.assertIn('status::New', code)
        self.assertIn('Validate_RequestBody_', code)

    def test_operation_id_is_rejected(self):
        data = yaml.safe_load((ROOT / 'schemas/services/broker/foo.yaml').read_text())
        data['paths']['/foo/bar']['get']['operationId'] = 'arbitraryName'
        with self.assertRaisesRegex(ValueError, 'operationId'):
            parse_spec(data)

    def test_public_contract_excludes_internal_operations(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            service = repo / 'services/example'
            public = repo / 'schemas/services/example'
            (service / 'docs/api').mkdir(parents=True)
            public.mkdir(parents=True)
            def spec(path, operation):
                return {'openapi': '3.0.3', 'info': {'title': 'Тест', 'version': '1'},
                        'paths': {path: {'get': {'responses': {'204': {'description': 'Нет тела'}}}}}}
            (public / 'openapi.yaml').write_text(yaml.safe_dump(spec('/external', 'Public')))
            (service / 'docs/api/openapi.yaml').write_text(yaml.safe_dump(spec('/external', 'Public')))
            (service / 'docs/api/private.yaml').write_text(yaml.safe_dump(spec('/internal', 'Private')))
            self.assertEqual({op.name for op in load(service)}, {'External', 'Internal'})
            self.assertEqual({op.name for op in load(service, public_only=True)}, {'External'})

    def test_success_return_and_api_error_types(self):
        from dataclasses import replace
        ops = load(ROOT / 'tests/fixtures/contract_service')
        post = next(op for op in ops if op.name == 'ItemsByIdCreate')
        code = operation_code('::example::clients::api::ItemsByIdCreate', post)
        self.assertIn('using Response = Response201;', code)
        self.assertIn('class Error409 final : public platform::http::ErrorByStatus<409>', code)
        self.assertIn('platform::http::ErrorByStatus<409>(ToJson(value))', code)
        self.assertIn('throw Error409{std::move(value)};', code)
        self.assertIn('catch (const platform::http::ApiError&)', code)
        mixed = replace(post, responses=(post.responses[0], replace(post.responses[0], status=202), post.responses[1]))
        code = operation_code('::example::clients::api::ItemsByIdCreate', mixed)
        self.assertIn('using Response = std::variant<Response201, Response202>;', code)
        self.assertNotIn('Response202, Response409', code)
        errors_only = replace(post, responses=(post.responses[1],))
        self.assertIn('using Response = void;', operation_code('::example::clients::api::ItemsByIdCreate', errors_only))

    def test_shared_status_base_supports_custom_and_empty_errors(self):
        from dataclasses import replace
        ops = load(ROOT / 'tests/fixtures/contract_service')
        post = next(op for op in ops if op.name == 'ItemsByIdCreate')
        empty = replace(post, responses=(replace(post.responses[1], status=404, body=None, media=None),))
        code = operation_code('::example::clients::api::EmptyError', empty)
        self.assertIn('platform::http::ErrorByStatus<404>()', code)
        custom = replace(empty, responses=tuple(replace(r, status=499) for r in empty.responses))
        code = operation_code('::example::clients::api::CustomError', custom)
        self.assertIn('class Error499 final : public platform::http::ErrorByStatus<499>', code)
