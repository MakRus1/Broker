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
from permission_catalog import load, release_entries, register, live_images, finish
from staging_manifest import workload
from openapi_gen.permissions import catalog


class PermissionCatalogTest(unittest.TestCase):
    def test_registration_uses_service_token_and_owned_entries(self):
        values = load(ROOT)
        entries = release_entries('shipments', values)
        self.assertTrue(all(entry['code'].startswith('shipments.') for entry in entries))
        with patch('release_catalog_api.request_json', side_effect=[{'access_token': 'token'}, {'success': True}]) as request:
            register('http://organizations', 'http://auth/token', 'secret', 'upsert',
                     dict(owner='shipments', version='image', entries=entries))
        self.assertEqual(request.call_args_list[0].args[1]['client_id'], 'permission-publisher')
        self.assertEqual(request.call_args.args[0], 'http://organizations/api/internal/permissions/releases/upsert')
        self.assertEqual(request.call_args.args[2], 'token')

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
            'permission_catalog.call_cluster'
        ) as execute:
            finish(['kubectl'], 'admin_bff', 'secret')
        sleep.assert_called_once_with(2)
        execute.assert_called_once_with(['kubectl'], 'secret', 'reconcile', dict(owner='admin_bff', versions=['current']))

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


class ServicePermissionsTest(unittest.TestCase):
    def test_only_owned_and_explicit_permissions_with_transitive_dependencies(self):
        from openapi_gen.permissions import selected

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = root / 'schemas/permissions'
            directory.mkdir(parents=True)
            def right(requires=()):
                return dict(name='Право', group='Группа', scope='organization', requires=list(requires))
            (directory / 'provider.yaml').write_text(yaml.safe_dump({
                'provider.read': right(),
                'provider.write': right(['provider.read']),
                'provider.manage': right(['provider.write']),
                'provider.unused': right(),
            }))
            service = root / 'services/consumer'
            service.mkdir(parents=True)
            (service / 'service.yaml').write_text(yaml.safe_dump({
                'auth': {'permissions': {'/read': 'authenticated', '/manage': 'provider.manage'}}
            }))
            before = selected(service, yaml.SafeLoader, root)
            self.assertEqual(set(before), {'provider.read', 'provider.write', 'provider.manage'})
            (directory / 'unrelated.yaml').write_text(yaml.safe_dump({'unrelated.read': right()}))
            self.assertEqual(before, selected(service, yaml.SafeLoader, root))
            (directory / 'consumer.yaml').write_text(yaml.safe_dump({'consumer.read': right()}))
            self.assertIn('consumer.read', selected(service, yaml.SafeLoader, root))

    def test_service_account_permissions_and_unknown_right(self):
        from openapi_gen.permissions import selected

        with tempfile.TemporaryDirectory() as temporary:
            service = Path(temporary) / 'consumer'
            service.mkdir()
            manifest = service / 'service.yaml'
            manifest.write_text(yaml.safe_dump({'service_accounts': {'worker': {
                'permissions': ['transportations.create']
            }}}))
            self.assertIn('transportations.create', selected(service, yaml.SafeLoader))
            manifest.write_text(yaml.safe_dump({'auth': {'permissions': {'/read': 'missing.read'}}}))
            with self.assertRaisesRegex(ValueError, 'missing.read'):
                selected(service, yaml.SafeLoader)
