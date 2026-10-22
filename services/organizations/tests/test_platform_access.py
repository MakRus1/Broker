"""Только корневой администратор выдаёт независимые платформенные права."""

import pytest


@pytest.mark.parametrize('user', ['org-admin', 'viewer'])
async def test_organization_admin_cannot_manage_platform_access(api, user):
    for operation in ['get', 'update']:
        body = {'subject': 'viewer'}
        if operation == 'update':
            body.update(permissions=['configs.read'], expected_revision=0)
        await api.call('/api/admin/platform-access/' + operation, body, user=user, expected=403)


async def test_platform_grants_are_central_and_root_has_no_implicit_config_access(api):
    await api.call('/api/platform/access', {'permission': 'configs.read'}, expected=403)
    await api.call(
        '/api/platform/access', {'permission': 'configs.read'}, user='viewer', expected=403
    )
    await api.call(
        '/api/admin/platform-access/update',
        {
            'subject': 'viewer',
            'permissions': ['configs.read'],
            'expected_revision': 0,
        },
    )
    result = await api.call('/api/platform/access', {'permission': 'configs.read'}, user='viewer')
    assert result['permissions'] == ['configs.read']
    await api.call(
        '/api/platform/access', {'permission': 'configs.write'}, user='viewer', expected=403
    )
    await api.call(
        '/api/admin/platform-access/update',
        {
            'subject': 'viewer',
            'permissions': [],
            'expected_revision': 1,
        },
    )
    await api.call(
        '/api/platform/access', {'permission': 'configs.read'}, user='viewer', expected=403
    )


@pytest.mark.parametrize(
    'permissions',
    [['configs.write'], ['configs.approve'], ['organizations.manage'], ['platform.admin']],
)
async def test_platform_permission_scope_and_dependencies(api, permissions):
    await api.call(
        '/api/admin/platform-access/update',
        {
            'subject': 'viewer',
            'permissions': permissions,
            'expected_revision': 0,
        },
        expected=400,
    )


async def test_platform_grant_revision_and_audit(api, pgsql):
    await api.call(
        '/api/admin/platform-access/update',
        {
            'subject': 'viewer',
            'permissions': ['configs.read', 'configs.approve'],
            'expected_revision': 0,
        },
    )
    await api.call(
        '/api/admin/platform-access/update',
        {
            'subject': 'viewer',
            'permissions': [],
            'expected_revision': 0,
        },
        expected=409,
    )
    result = await api.call('/api/admin/platform-access/get', {'subject': 'viewer'})
    assert result['access']['revision'] == 1
    assert all(permission['id'].startswith('configs.') for permission in result['catalog'])
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT subject,revision FROM platform_access_audit')
        assert cursor.fetchall() == [('viewer', 1)]
