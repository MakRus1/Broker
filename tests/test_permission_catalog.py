"""Контракт каталога и безопасной сверки релизов."""

import sys
from pathlib import Path
import tempfile
import unittest
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from permission_catalog import load, register_sql, reconcile_sql
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
