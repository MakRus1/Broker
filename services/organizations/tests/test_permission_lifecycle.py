"""Каталог переживает канарейку и откат без потери назначений ролям."""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'scripts'))
from permission_catalog import load, release_entries
from test_permission_registration import call
from conftest import ORGANIZATION


async def test_removed_permission_denied_and_rollback_restores_grant(api, pgsql):
    body = {'organization_id': ORGANIZATION, 'permission': 'shipments.read'}
    await api.call('/api/organizations/access', body, user='org-admin')
    await call(api, 'upsert', dict(owner='shipments', version='without-permissions', entries=[]))
    await call(api, 'reconcile', dict(owner='shipments', versions=['without-permissions']))
    await api.call('/api/organizations/access', body, user='org-admin', expected=403)
    await call(api, 'reconcile', dict(owner='shipments', versions=['test']))
    await api.call('/api/organizations/access', body, user='org-admin')


async def test_canary_keeps_union_and_other_modules(api, pgsql):
    catalog = load(Path(__file__).resolve().parents[3])
    await call(api, 'upsert', dict(owner='shipments', version='next', entries=[]))
    await call(api, 'reconcile', dict(owner='shipments', versions=['test', 'next']))
    await api.call(
        '/api/organizations/access',
        {'organization_id': ORGANIZATION, 'permission': 'shipments.write'},
        user='org-admin',
    )
    await call(api, 'reconcile', dict(owner='shipments', versions=['next']))
    await api.call(
        '/api/organizations/access',
        {'organization_id': ORGANIZATION, 'permission': 'counterparties.write'},
        user='org-admin',
    )
    await call(api, 'upsert', dict(owner='shipments', version='test', entries=release_entries('shipments', catalog)))
    await call(api, 'reconcile', dict(owner='shipments', versions=['test']))
    await api.call(
        '/api/organizations/access',
        {'organization_id': ORGANIZATION, 'permission': 'shipments.write'},
        user='org-admin',
    )


async def test_inactive_assignments_survive_role_edit(api, pgsql):
    roles = await api.call('/api/admin/organizations/roles/list', {'organization_id': ORGANIZATION})
    role = next(role for role in roles['items'] if role['id'] == 'administrator')
    await call(api, 'upsert', dict(owner='shipments', version='without-permissions', entries=[]))
    await call(api, 'reconcile', dict(owner='shipments', versions=['without-permissions']))
    await api.call(
        '/api/admin/organizations/roles/update',
        {
            'organization_id': ORGANIZATION,
            'id': role['id'],
            'name': 'Администратор команды',
            'description': role['description'],
            'permissions': role['permissions'],
            'expected_revision': role['revision'],
        },
    )
    await call(api, 'reconcile', dict(owner='shipments', versions=['test']))
    await api.call(
        '/api/organizations/access',
        {'organization_id': ORGANIZATION, 'permission': 'shipments.write'},
        user='org-admin',
    )
