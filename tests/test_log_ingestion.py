"""Проверка контракта сквозного теста уровней и времени событий."""

import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('check_logs', ROOT / 'tools/local/check_logs.py')
logs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(logs)


class LogIngestionTest(unittest.TestCase):
    def streams(self):
        return [
            {
                'stream': {'level': level, 'severity_text': severity, 'severity_number': number},
                'values': [['123000', json.dumps({'level': level, 'check_id': 'test'})]],
            }
            for level, (severity, number) in logs.LEVELS.items()
        ]

    def test_all_levels_and_original_timestamp_are_required(self):
        self.assertTrue(logs.verify(self.streams(), 'test', 123000))
        self.assertFalse(logs.verify(self.streams()[:-1], 'test', 123000))
        self.assertFalse(logs.verify(self.streams(), 'another-run', 123000))
        with self.assertRaisesRegex(AssertionError, 'времени'):
            logs.verify(self.streams(), 'test', 456000)
        mixed = self.streams()
        mixed[0]['stream']['level'] = 'ERROR'
        with self.assertRaisesRegex(AssertionError, 'другого уровня'):
            logs.verify(mixed, 'test', 123000)
