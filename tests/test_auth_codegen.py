"""Контракт авторизации одинаков в сервисной и клиентской спецификациях."""
from pathlib import Path
import sys
import unittest
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from generate import load
from clients import operation_code
from model import parse_spec


class AuthContractTest(unittest.TestCase):
    def spec(self):
        return {'openapi': '3.0.3', 'info': {'title': 'Тест', 'version': '1'},
                'components': {'securitySchemes': {'Admin': {'type': 'http', 'scheme': 'bearer', 'bearerFormat': 'JWT'}}},
                'security': [{'Admin': []}],
                'paths': {'/admin/example': {'get': {'responses': {'204': {'description': 'Готово'}}}}}}

    def test_bearer_inheritance_and_public_override(self):
        spec = self.spec()
        self.assertTrue(parse_spec(spec)[0].authenticated)
        spec['paths']['/admin/example']['get']['security'] = []
        self.assertFalse(parse_spec(spec)[0].authenticated)

    def test_unsupported_security_fails(self):
        for requirement in [None, {}, [{}], [{'Missing': []}], [{'Admin': ['scope']}], [{'Admin': []}, {'Admin': []}]]:
            spec = self.spec()
            spec['security'] = requirement
            with self.subTest(requirement=requirement), self.assertRaises(ValueError):
                parse_spec(spec)
        for scheme in [{'type': 'apiKey', 'in': 'header', 'name': 'X-Key'},
                       {'type': 'oauth2', 'flows': {}}, {'type': 'http', 'scheme': 'basic'}]:
            spec = self.spec()
            spec['components']['securitySchemes']['Admin'] = scheme
            with self.subTest(scheme=scheme), self.assertRaises(ValueError):
                parse_spec(spec)

    def test_client_requires_explicit_token(self):
        operation = parse_spec(self.spec())[0]
        code = operation_code('::test::AdminExample', operation)
        self.assertIn('std::string_view access_token', code)
        self.assertNotIn('access_token =', code)
        self.assertIn('raw.bearer_token = std::string(access_token)', code)
        self.assertIn('Invalid bearer token', code)
        from render import handler
        _, source = handler('example', operation, False)
        self.assertIn('::platform::auth::GetPrincipal(context)', source)
        self.assertLess(source.index('GetPrincipal(context)'), source.index('View::Handle'))

    def test_every_admin_operation_has_policy_and_security(self):
        service = ROOT / 'services/configs'
        operations = load(service)
        component = yaml.safe_load((service / 'configs/static_config.yaml').read_text())['components_manager']['components']['platform-auth']
        admin = {operation.path for operation in operations if operation.path.startswith('/admin/')}
        self.assertEqual(admin, set(component['permissions']))
        for operation in operations:
            self.assertEqual(operation.authenticated, operation.path.startswith('/admin/'))
        self.assertEqual({op.path: op.authenticated for op in operations},
                         {op.path: op.authenticated for op in load(service, public_only=True)})
