"""Выборочная политика логирования тел HTTP без изменения других ручек."""

from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from http_logging import body_limits, policy
from generate import UniqueLoader, configuration, load
import yaml


class HttpLoggingTest(unittest.TestCase):
    def test_paths_and_independent_limits(self):
        selected = policy(
            {
                'http_logging': {
                    'paths': ['/api/configs/**'],
                    'request_body_limit': 0,
                    'response_body_limit': 12345,
                }
            }
        )
        self.assertEqual(body_limits('/api/configs/drafts/create', selected), (0, 12345))
        self.assertEqual(body_limits('/api/session', selected), (0, 0))
        self.assertEqual(body_limits('/configs/values', None), (0, 0))
        self.assertEqual(
            body_limits('/one', policy({'http_logging': {'paths': ['/one']}})), (32768, 32768)
        )

    def test_invalid_policy_is_rejected(self):
        for value in [
            None,
            True,
            {},
            {'paths': []},
            {'paths': ['relative']},
            {'paths': ['/x', '/x']},
            {'paths': ['/x'], 'unknown': True},
            {'paths': ['/x'], 'request_body_limit': -1},
            {'paths': ['/x'], 'response_body_limit': True},
        ]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                policy({'http_logging': value})

    def test_only_config_admin_routes_opt_in(self):
        for service, prefix in [('admin_bff', '/api/configs/'), ('configs', '/admin/configs/')]:
            path = ROOT / 'services' / service
            generated = configuration(path, load(path))
            components = yaml.load(generated, Loader=UniqueLoader)['components_manager'][
                'components'
            ]
            for operation in load(path):
                handler = components[operation.handler]
                expected = 32768 if operation.path.startswith(prefix) else 0
                self.assertEqual(handler['request_body_size_log_limit'], expected)
                self.assertEqual(handler['response_data_size_log_limit'], expected)
