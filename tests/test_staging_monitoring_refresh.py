"""Изменение каталога обновляет мониторинг без повторного деплоя инфраструктуры."""

import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace
import yaml

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import staging_monitoring as monitoring


def config(name, namespace='stage'):
    return {
        'apiVersion': 'v1',
        'kind': 'ConfigMap',
        'metadata': {'name': name, 'namespace': namespace},
        'data': {'config.yaml': 'new settings'},
    }


def deployment(name, config_name):
    return {
        'kind': 'Deployment',
        'metadata': {'name': name},
        'spec': {
            'template': {
                'spec': {
                    'containers': [{'name': name, 'image': 'unrelated:new'}],
                    'volumes': [
                        {'name': 'settings', 'configMap': {'name': config_name}},
                        {'name': 'data', 'persistentVolumeClaim': {'claimName': name}},
                    ],
                }
            }
        },
    }


class MonitoringTest(unittest.TestCase):
    def execute(self, current='old', objects=None):
        calls = []

        def run(command, **kwargs):
            calls.append((command, kwargs))
            if 'get' in command:
                name = command[command.index('deployment') + 1]
                return SimpleNamespace(stdout=json.dumps(deployment(name, current + '-' + name)))
            return SimpleNamespace(stdout='')

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            objects = objects or [
                obj
                for name in monitoring.NAMES
                for obj in [config('new-' + name), deployment(name, 'new-' + name)]
            ]
            (root / 'monitoring.yaml').write_text(yaml.safe_dump_all(objects))
            with patch.object(monitoring, 'run', side_effect=run):
                monitoring.refresh(root, 'ctx', 'stage')
        return calls

    def test_changed_catalog_updates_only_configmaps_and_volume_references(self):
        calls = self.execute()
        applies = [
            yaml.safe_load_all(kwargs['input']) for command, kwargs in calls if 'apply' in command
        ]
        self.assertEqual({obj['kind'] for group in applies for obj in group}, {'ConfigMap'})
        patches = [json.loads(command[-1]) for command, _ in calls if 'patch' in command]
        self.assertEqual(len(patches), 2)
        for patch in patches:
            self.assertEqual(set(patch['spec']['template']['spec']), {'volumes'})
            self.assertEqual(patch['spec']['template']['spec']['volumes'][0]['name'], 'settings')
            self.assertNotIn('persistentVolumeClaim', str(patch))
        self.assertEqual(sum('rollout' in command for command, _ in calls), 2)

    def test_unchanged_configuration_does_not_restart_monitoring(self):
        calls = self.execute(current='new')
        self.assertTrue(all('get' in command for command, _ in calls))

    def test_other_namespace_stops_before_mutation(self):
        objects = [
            obj
            for name in monitoring.NAMES
            for obj in [config('new-' + name, 'other'), deployment(name, 'new-' + name)]
        ]
        with self.assertRaises(ValueError):
            self.execute(objects=objects)
