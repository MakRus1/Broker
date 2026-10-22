"""Каталог переживает канарейку и откат без потери назначений ролям."""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'scripts'))
from permission_catalog import load, register_sql, reconcile_sql
from conftest import ORGANIZATION


def execute(db, sql):
    with db.cursor() as cursor:
        cursor.execute(sql.replace('BEGIN;', '').replace('COMMIT;', ''))


async def test_removed_permission_denied_and_rollback_restores_grant(api, pgsql):
    body = {'organization_id': ORGANIZATION, 'permission': 'shipments.read'}
    await api.call('/api/organizations/access', body, user='org-admin')
    execute(pgsql['db_1'], register_sql('shipments', 'without-permissions', {}))
    execute(pgsql['db_1'], reconcile_sql('shipments', ['without-permissions']))
    await api.call('/api/organizations/access', body, user='org-admin', expected=403)
    execute(pgsql['db_1'], reconcile_sql('shipments', ['test']))
    await api.call('/api/organizations/access', body, user='org-admin')


async def test_canary_keeps_union_and_other_modules(api, pgsql):
    catalog = load(Path(__file__).resolve().parents[3])
    execute(pgsql['db_1'], register_sql('shipments', 'next', {}))
    execute(pgsql['db_1'], reconcile_sql('shipments', ['test', 'next']))
    await api.call(
        '/api/organizations/access',
        {'organization_id': ORGANIZATION, 'permission': 'shipments.write'},
        user='org-admin',
    )
    execute(pgsql['db_1'], reconcile_sql('shipments', ['next']))
    await api.call(
        '/api/organizations/access',
        {'organization_id': ORGANIZATION, 'permission': 'counterparties.write'},
        user='org-admin',
    )
    execute(pgsql['db_1'], register_sql('shipments', 'test', catalog))
    execute(pgsql['db_1'], reconcile_sql('shipments', ['test']))
    await api.call(
        '/api/organizations/access',
        {'organization_id': ORGANIZATION, 'permission': 'shipments.write'},
        user='org-admin',
    )


async def test_inactive_assignments_survive_role_edit(api, pgsql):
    roles = await api.call('/api/admin/organizations/roles/list', {'organization_id': ORGANIZATION})
    role = next(role for role in roles['items'] if role['id'] == 'administrator')
    execute(pgsql['db_1'], register_sql('shipments', 'without-permissions', {}))
    execute(pgsql['db_1'], reconcile_sql('shipments', ['without-permissions']))
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
    execute(pgsql['db_1'], reconcile_sql('shipments', ['test']))
    await api.call(
        '/api/organizations/access',
        {'organization_id': ORGANIZATION, 'permission': 'shipments.write'},
        user='org-admin',
    )
