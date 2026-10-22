"""Контракт каталога и безопасной сверки релизов."""

import sys
import json
from unittest.mock import patch
from types import SimpleNamespace
from pathlib import Path
import tempfile
import unittest
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from permission_catalog import load, register_sql, reconcile_sql, live_images, finish
from staging_manifest import workload
from openapi_gen.permissions import catalog


class PermissionCatalogTest(unittest.TestCase):
    def test_catalog_and_sql_escaping(self):
        values = load(ROOT)
        self.assertEqual(values['shipments.write']['requires'], ['shipments.read'])
        for code in ['configs.write', 'configs.approve']:
            self.assertEqual(values[code]['requires'], ['configs.read'])
        sql = register_sql('shipments', 'image', values)
        self.assertIn('ON CONFLICT', sql)
        self.assertNotIn('DELETE', sql)
        self.assertNotIn('counterparties.read', sql)

    def test_reconciliation_requires_live_versions(self):
        with self.assertRaises(ValueError):
            reconcile_sql('shipments', [])
        sql = reconcile_sql('shipments', ['old', 'new'])
        self.assertIn("version IN ('new','old')", sql)
        self.assertIn('RAISE EXCEPTION', sql)
        self.assertNotIn('DELETE', sql)

    def test_live_images_follow_manifest_labels_and_container_names(self):
        for owner in ['admin_bff', 'web_bff', 'configs']:
            with self.subTest(owner=owner):
                template = workload(
                    'Deployment',
                    owner,
                    'broker-staging',
                    {'name': owner.replace('_', '-'), 'image': 'current'},
                )['spec']['template']
                template['spec']['containers'].append({'name': 'sidecar', 'image': 'ignore'})
                completed = {**template, 'status': {'phase': 'Succeeded'}}
                response = SimpleNamespace(stdout=json.dumps({'items': [template, completed]}))
                with patch('permission_catalog.subprocess.run', return_value=response):
                    self.assertEqual(live_images(['kubectl'], owner), {'current'})

    def test_finish_waits_for_draining_pods_with_original_service_label(self):
        draining = {
            'metadata': {
                'labels': {'app.kubernetes.io/name': 'admin_bff'},
                'deletionTimestamp': '2026-10-22T11:00:00Z',
            }
        }
        responses = [
            SimpleNamespace(stdout=json.dumps({'items': [draining]})),
            SimpleNamespace(stdout=json.dumps({'items': []})),
        ]
        with patch('permission_catalog.subprocess.run', side_effect=responses), patch(
            'permission_catalog.time.sleep'
        ) as sleep, patch('permission_catalog.live_images', return_value={'current'}), patch(
            'permission_catalog.execute'
        ) as execute:
            finish(['kubectl'], 'admin_bff')
        sleep.assert_called_once_with(2)
        execute.assert_called_once_with(['kubectl'], reconcile_sql('admin_bff', {'current'}))

    def test_invalid_catalog(self):
        for entries in [
            {
                'shipments.write': {
                    'name': 'Запись',
                    'group': 'Поставки',
                    'scope': 'organization',
                    'requires': ['missing.read'],
                }
            },
            {
                'shipments.read': {
                    'name': 'Чтение',
                    'group': 'Поставки',
                    'scope': 'organization',
                    'requires': ['shipments.read'],
                }
            },
            {
                'foreign.read': {
                    'name': 'Чтение',
                    'group': 'Поставки',
                    'scope': 'organization',
                    'requires': [],
                }
            },
        ]:
            with self.subTest(entries=entries), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                directory = root / 'schemas/permissions'
                directory.mkdir(parents=True)
                (directory / 'shipments.yaml').write_text(yaml.safe_dump(entries))
                with self.assertRaises(ValueError):
                    catalog(root, yaml.SafeLoader)
