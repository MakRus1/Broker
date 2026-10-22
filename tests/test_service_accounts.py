"""Проверка деклараций и одинаковых привязок служебной авторизации в окружениях."""

from copy import deepcopy
import tempfile
import shutil
import yaml
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts/openapi_gen'))
from service_accounts import load, policy, provision, secret_key, variables


class ServiceAccountsTest(unittest.TestCase):
    def test_environments_use_same_accounts_and_secret_keys(self):
        accounts = load(ROOT)
        credentials = {secret_key(account): 'private' for account in accounts.values()}
        for base in ['http://keycloak:8080', 'https://keycloak.example:8443']:
            value = variables(accounts, 'shipments', credentials, base + '/token')
            self.assertTrue(value['dispatcher-enabled'])
            self.assertEqual(value['dispatcher-client-id'], 'shipments-dispatcher')
            self.assertEqual(value['dispatcher-client-secret'], 'private')
            self.assertEqual(value['dispatcher-token-url'], base + '/token')
            manager = variables(accounts, 'organizations', credentials, base + '/token')
            self.assertEqual(manager['users-client-id'], 'organizations-manager')
            self.assertEqual(manager['users-client-secret'], 'private')
        self.assertEqual(variables(accounts, 'configs', credentials, 'unused'), {})

    def test_invalid_account_is_rejected(self):
        original = load(ROOT)['shipments-dispatcher']
        for patch in [
            {'client_id': '../escape'},
            {'permissions': 'transportations.create'},
            {'permissions': ['same', 'same']},
            {'audience': ''},
            {'config': {}},
            {'keycloak_roles': {'realm-management': ['view-users', 'view-users']}},
            {'unexpected': True},
        ]:
            value = {
                key: value for key, value in original.items() if key not in {'service', 'alias'}
            }
            with self.subTest(patch=patch), self.assertRaises(ValueError):
                policy({'service_accounts': {'worker': {**deepcopy(value), **patch}}})

    def test_invalid_repository_binding_fails_before_provisioning(self):
        for patch in [
            {'permissions': ['missing.create']},
            {'permissions': ['shipments.write']},
            {'client_id': 'untrusted-worker'},
            {
                'config': {
                    'client_id': 'missing',
                    'client_secret': 'dispatcher-client-secret',
                    'token_url': 'dispatcher-token-url',
                }
            },
        ]:
            with self.subTest(patch=patch), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                shutil.copytree(ROOT / 'schemas/permissions', root / 'schemas/permissions')
                for source in (ROOT / 'services').glob('*/service.yaml'):
                    target = root / 'services' / source.parent.name
                    (target / 'configs').mkdir(parents=True)
                    shutil.copy(source, target / 'service.yaml')
                    shutil.copy(
                        source.parent / 'configs/config_vars.yaml',
                        target / 'configs/config_vars.yaml',
                    )
                path = root / 'services/shipments/service.yaml'
                spec = yaml.safe_load(path.read_text())
                spec['service_accounts']['dispatcher'].update(patch)
                path.write_text(yaml.safe_dump(spec))
                with self.assertRaises(ValueError):
                    load(root)

    def test_provision_creates_missing_role_and_grants_only_declared_access(self):
        account = load(ROOT)['shipments-dispatcher']
        for existing in [False, True]:
            with self.subTest(existing=existing):
                lookups = iter([([{'id': 'worker'}] if existing else []), [{'id': 'worker'}]])

                def call(path, value=None, method=None):
                    if value is not None:
                        return None
                    if path == 'clients?clientId=shipments-dispatcher':
                        return next(lookups)
                    return {
                        'clients/worker/service-account-user': {'id': 'subject'},
                        'clients?clientId=broker-logistics': [{'id': 'resource'}],
                        'clients/resource/roles': [],
                        'clients/resource/roles/transportations.create': {
                            'id': 'role',
                            'name': 'transportations.create',
                        },
                    }[path]

                api = Mock(side_effect=call)
                subjects = provision(
                    api, {'worker': account}, {'shipments-dispatcher-secret': 'private'}
                )
                self.assertEqual(subjects, {'shipments-dispatcher': 'subject'})
                api.assert_any_call('clients/resource/roles', {'name': 'transportations.create'})
                api.assert_any_call(
                    'users/subject/role-mappings/clients/resource',
                    [{'id': 'role', 'name': 'transportations.create'}],
                )
                desired = next(
                    item.args[1]
                    for item in api.call_args_list
                    if item.args[0] in {'clients', 'clients/worker'} and len(item.args) > 1
                )
                self.assertFalse(desired['fullScopeAllowed'])
                self.assertFalse(desired['directAccessGrantsEnabled'])
                self.assertEqual(
                    desired['protocolMappers'][0]['config']['included.client.audience'],
                    'broker-logistics',
                )
