"""Настройка доступа служебного клиента не должна давать ему realm-admin."""

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from organization_identity import configure


class IdentitySetupTest(unittest.TestCase):
    def test_minimal_roles_and_private_provisioning_attribute(self):
        calls = []
        profile = {'attributes': [{'name': 'email', 'permissions': {'edit': ['user', 'admin']}}]}

        def api(path, value=None, method=None):
            calls.append((path, copy.deepcopy(value), method))
            if value is not None:
                return None
            if path == 'clients?clientId=organizations-manager':
                return [{'id': 'manager'}]
            if path == 'clients?clientId=realm-management':
                return [{'id': 'management'}]
            if path == 'clients?clientId=broker-admin':
                return [{'id': 'browser'}]
            if path == 'clients?clientId=broker-logistics':
                return [{'id': 'logistics'}]
            if path == 'clients/manager/service-account-user':
                return {'id': 'service-user'}
            if path == 'clients/management/roles':
                return [
                    {'name': name}
                    for name in ['manage-users', 'query-users', 'view-users', 'realm-admin']
                ]
            if path == 'clients/logistics/roles':
                return [{'name': 'platform.admin'}]
            if path == 'users/profile':
                return copy.deepcopy(profile)
            raise AssertionError(path)

        configure(api)
        updated = next(value for path, value, _ in calls if path == 'users/profile' and value)
        self.assertEqual(updated['attributes'][0], profile['attributes'][0])
        marker = updated['attributes'][-1]
        self.assertEqual(marker['permissions'], {'view': ['admin'], 'edit': ['admin']})
        self.assertFalse(any(path.startswith('users?username=') for path, _, _ in calls))
