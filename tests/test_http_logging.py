"""Логирование HTTP по умолчанию, исключения и независимые лимиты."""

from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from http_logging import body_limits, policy
from generate import UniqueLoader, configuration, load
import yaml


class HttpLoggingTest(unittest.TestCase):
    def test_exclusions_and_independent_limits(self):
        selected = policy(
            {
                'http_logging': {
                    'exclude_paths': ['/auth/**'],
                    'request_body_limit': 0,
                    'response_body_limit': 12345,
                }
            }
        )
        self.assertEqual(body_limits('/api/configs/drafts/create', selected), (0, 12345))
        self.assertEqual(body_limits('/auth/callback', selected), (0, 0))
        self.assertEqual(body_limits('/configs/values', None), (32768, 32768))
        self.assertEqual(body_limits('/one', policy({})), (32768, 32768))

    def test_invalid_policy_is_rejected(self):
        for value in [
            None,
            True,
            {'exclude_paths': None},
            {'exclude_paths': ['relative']},
            {'exclude_paths': ['/x', '/x']},
            {'unknown': True},
            {'request_body_limit': -1},
            {'response_body_limit': True},
        ]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                policy({'http_logging': value})

    def test_defaults_and_empty_section(self):
        for spec in ({}, {'http_logging': {}}, {'http_logging': {'exclude_paths': []}}):
            with self.subTest(spec=spec):
                self.assertEqual(body_limits('/any/new/route', policy(spec)), (32768, 32768))

    def test_generated_routes_use_defaults_except_auth(self):
        for service in ('admin_bff', 'configs', 'broker'):
            path = ROOT / 'services' / service
            generated = configuration(path, load(path))
            components = yaml.load(generated, Loader=UniqueLoader)['components_manager'][
                'components'
            ]
            for operation in load(path):
                with self.subTest(service=service, path=operation.path):
                    handler = components[operation.handler]
                    expected = (
                        0
                        if service == 'admin_bff'
                        and operation.path
                        in (
                            '/api/session',
                            '/api/logout',
                            '/api/admin/organizations/members/create',
                        )
                        else 32768
                    )
                    self.assertEqual(handler['request_body_size_log_limit'], expected)
                    self.assertEqual(handler['response_data_size_log_limit'], expected)
            if service == 'admin_bff':
                for component in components.values():
                    if isinstance(component, dict) and str(component.get('path', '')).startswith(
                        '/auth/'
                    ):
                        defaults = components['server']['listener']['handler-defaults']
                        self.assertEqual(
                            component.get(
                                'request_body_size_log_limit',
                                defaults['request_body_size_log_limit'],
                            ),
                            0,
                        )
                        self.assertEqual(
                            component.get(
                                'response_data_size_log_limit',
                                defaults['response_data_size_log_limit'],
                            ),
                            0,
                        )
