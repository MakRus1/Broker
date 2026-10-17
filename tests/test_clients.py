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
                     {'api': {'service': '../broker', 'transport': 'http'}},
                     {'api': {'service': 'unknown', 'transport': 'http'}},
                     {'api': {'service': 'broker', 'transport': 'grpc'}},
                     {'api': {'service': 'broker', 'transport': 'http', 'operations': ['helloGet']}},
                     {'api': {'service': 'broker', 'transport': 'http', 'url': 'http://localhost'}}]
            for clients in cases:
                (service / 'service.yaml').write_text(yaml.safe_dump({'version': 1, 'clients': clients}))
                with self.subTest(clients=clients), self.assertRaises((ValueError, TypeError)):
                    client_specs(service, UniqueLoader)

    def test_client_dto_and_response_decoding(self):
        provider = ROOT / 'tests/fixtures/contract_service'
        ops = load(provider)
        for op in ops:
            code = operation_code('::example::clients::api::' + op.operation_id, op)
            self.assertNotIn('InputPresence', code)
            self.assertNotIn('MarkProvided', code)
            self.assertNotIn('RequestContext', code)
            self.assertIn('Undeclared HTTP response status', code)
        get = next(op for op in ops if op.path == '/items/{id}' and op.method == 'GET')
        code = operation_code('::example::clients::api::' + get.operation_id, get)
        self.assertIn('ReplacePath', code)
        self.assertIn('Response204', code)
        self.assertIn('unexpected body', code)
        post = next(op for op in ops if op.path == '/validated' and op.method == 'POST')
        code = operation_code('::example::clients::api::' + post.operation_id, post)
        self.assertIn('enum class status', code)
        self.assertIn('status::New', code)
        self.assertIn('Validate_RequestBody_', code)

    def test_missing_or_duplicate_operation_id_fails(self):
        fixture = ROOT / 'tests/fixtures/contract_service'
        operations = load(fixture)
        from dataclasses import replace
        with patch('clients.client_specs', return_value=[('api', fixture)]):
            with self.assertRaisesRegex(ValueError, 'operationId'):
                client_outputs(fixture, UniqueLoader, lambda _, **kwargs: [replace(operations[0], operation_id=None)])
        data = yaml.safe_load((fixture / 'docs/api/contract.yaml').read_text())
        for item in data['paths'].values():
            for method, operation in item.items():
                if method in {'get', 'post'}:
                    operation['operationId'] = 'same'
        with tempfile.TemporaryDirectory() as tmp:
            service = Path(tmp) / 'example'
            (service / 'docs/api').mkdir(parents=True)
            (service / 'docs/api/a.yaml').write_text(yaml.safe_dump(data))
            with self.assertRaisesRegex(ValueError, 'operationId'):
                load(service)

    def test_public_contract_excludes_internal_operations(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            service = repo / 'services/example'
            public = repo / 'schemas/services/example'
            (service / 'docs/api').mkdir(parents=True)
            public.mkdir(parents=True)
            def spec(path, operation):
                return {'openapi': '3.0.3', 'info': {'title': 'Тест', 'version': '1'},
                        'paths': {path: {'get': {'operationId': operation, 'responses': {'204': {'description': 'Нет тела'}}}}}}
            (public / 'openapi.yaml').write_text(yaml.safe_dump(spec('/external', 'Public')))
            (service / 'docs/api/private.yaml').write_text(yaml.safe_dump(spec('/internal', 'Private')))
            self.assertEqual({op.operation_id for op in load(service)}, {'Public', 'Private'})
            self.assertEqual({op.operation_id for op in load(service, public_only=True)}, {'Public'})
