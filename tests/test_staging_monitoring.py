"""Проверка трассы выдерживает краткую недоступность после rollout."""

import importlib.util
from pathlib import Path
import unittest
from unittest.mock import MagicMock, patch
from urllib.error import HTTPError, URLError

spec = importlib.util.spec_from_file_location(
    'staging_monitoring', Path(__file__).resolve().parents[1] / 'tools/staging/check.py'
)
monitoring = importlib.util.module_from_spec(spec)
spec.loader.exec_module(monitoring)


class MonitoringTest(unittest.TestCase):
    def response(self):
        response = MagicMock()
        response.headers = {'X-Trace-Id': 'a' * 32}
        response.__enter__.return_value = response
        return response

    def test_transient_errors_are_retried(self):
        errors = [HTTPError('url', code, 'temporary', {}, None) for code in (502, 503, 504)]
        errors += [URLError('connection refused'), TimeoutError(), ConnectionResetError()]
        for error in errors:
            with self.subTest(error=error), patch.object(
                monitoring, 'urlopen', side_effect=[error, self.response()]
            ) as request, patch.object(monitoring.time, 'sleep'):
                self.assertEqual(monitoring.probe_trace('http://broker/hello'), 'a' * 32)
                self.assertEqual(request.call_count, 2)

    def test_persistent_unavailability_has_a_deadline(self):
        with patch.object(
            monitoring, 'urlopen', side_effect=HTTPError('url', 503, 'temporary', {}, None)
        ), patch.object(monitoring.time, 'monotonic', side_effect=[0, 61]), patch.object(
            monitoring.time, 'sleep'
        ) as sleep:
            with self.assertRaisesRegex(ValueError, '60 секунд'):
                monitoring.probe_trace('http://broker/hello')
            sleep.assert_not_called()

    def test_contract_errors_are_not_hidden_by_retries(self):
        for code in (400, 401, 403, 404, 500):
            with self.subTest(code=code), patch.object(
                monitoring, 'urlopen', side_effect=HTTPError('url', code, 'permanent', {}, None)
            ), patch.object(monitoring.time, 'sleep') as sleep:
                with self.assertRaises(HTTPError):
                    monitoring.probe_trace('http://broker/hello')
                sleep.assert_not_called()
        response = self.response()
        response.headers = {}
        with patch.object(monitoring, 'urlopen', return_value=response), self.assertRaisesRegex(
            ValueError, 'X-Trace-Id'
        ):
            monitoring.probe_trace('http://broker/hello')
