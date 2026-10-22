"""Права платформы и организации, выдача доступа и безопасные повторы создания."""

from uuid import uuid4
import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION

ADMIN = '/api/admin/organizations'


async def test_platform_creates_and_updates_organization(api):
    body = {'id': str(uuid4()), 'name': '  Новая компания  '}
    organization = (await api.call(ADMIN + '/create', body, expected=201))['organization']
    assert organization == {**body, 'name': 'Новая компания', 'enabled': True, 'revision': 1}
    changed = await api.call(
        ADMIN + '/update',
        {
            'id': body['id'],
            'name': 'Переименованная компания',
            'enabled': False,
            'expected_revision': 1,
        },
    )
    assert changed['organization']['revision'] == 2
    await api.call(
        ADMIN + '/update',
        {
            'id': body['id'],
            'name': 'Устаревшая редакция',
            'enabled': True,
            'expected_revision': 1,
        },
        expected=409,
    )


@pytest.mark.parametrize('user', ['org-admin', 'viewer', 'other-admin'])
async def test_only_platform_can_create_or_list_all_organizations(api, user):
    await api.call(
        ADMIN + '/create', {'id': str(uuid4()), 'name': 'Запрещено'}, user=user, expected=403
    )
    await api.call(ADMIN + '/list', {}, user=user, expected=403)


@pytest.mark.parametrize(
    'user,organization,status',
    [
        ('org-admin', ORGANIZATION, 200),
        ('org-admin', OTHER_ORGANIZATION, 403),
        ('viewer', ORGANIZATION, 403),
        ('platform', OTHER_ORGANIZATION, 200),
    ],
)
async def test_members_are_visible_only_to_allowed_administrator(api, user, organization, status):
    await api.call(
        ADMIN + '/members/list', {'organization_id': organization}, user=user, expected=status
    )


async def test_me_returns_only_own_organizations(api):
    result = await api.call('/api/organizations/me', user='org-admin')
    assert result['platform_admin'] is False
    assert [item['id'] for item in result['organizations']] == [ORGANIZATION]
    assert result['managed_organization_ids'] == [ORGANIZATION]


@pytest.mark.parametrize(
    'user,permission,status',
    [
        ('viewer', 'shipments.read', 200),
        ('viewer', 'shipments.write', 403),
        ('org-admin', 'organizations.manage', 200),
        ('other-admin', 'shipments.read', 403),
    ],
)
async def test_access_is_scoped_to_membership_role(api, user, permission, status):
    await api.call(
        '/api/organizations/access',
        {
            'organization_id': ORGANIZATION,
            'permission': permission,
        },
        user=user,
        expected=status,
    )


async def test_last_administrator_cannot_be_disabled_or_demoted(api):
    for role, enabled in [('viewer', True), ('administrator', False)]:
        result = await api.call(
            ADMIN + '/members/update',
            {
                'organization_id': ORGANIZATION,
                'subject': 'org-admin',
                'expected_revision': 1,
                'role': role,
                'enabled': enabled,
            },
            user='org-admin',
            expected=409,
        )
        assert result['error'] == 'last_administrator'


async def test_disabling_membership_revokes_access_without_disabling_other_organizations(api):
    await api.call(
        ADMIN + '/members/update',
        {
            'organization_id': ORGANIZATION,
            'subject': 'viewer',
            'expected_revision': 1,
            'role': 'viewer',
            'enabled': False,
        },
        user='org-admin',
    )
    await api.call(
        '/api/organizations/access', {'permission': 'shipments.read'}, user='viewer', expected=403
    )
    await api.call(
        '/api/organizations/access', {'permission': 'shipments.read'}, user='other-admin'
    )


def new_member():
    return {
        'organization_id': ORGANIZATION,
        'request_id': str(uuid4()),
        'username': 'new-employee',
        'display_name': 'Новый сотрудник',
        'email': 'employee@example.test',
        'temporary_password': 'temporary-test-password',
        'role': 'operator',
    }


async def test_create_user_and_membership_replay_keeps_password_out_of_database(
    api, identity_provider, pgsql
):
    body = new_member()
    first = await api.call(ADMIN + '/members/create', body, user='org-admin', expected=201)
    replay = await api.call(ADMIN + '/members/create', body, user='org-admin', expected=201)
    assert replay == first
    assert identity_provider['calls'] == ['POST', 'GET']
    assert identity_provider['users']['new-employee']['credentials'][0]['temporary'] is True
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT payload::TEXT FROM user_provisioning')
        assert body['temporary_password'] not in str(cursor.fetchall())
        cursor.execute('SELECT document::TEXT FROM access_audit')
        assert body['temporary_password'] not in str(cursor.fetchall())
    await api.call(
        ADMIN + '/members/create', {**body, 'role': 'administrator'}, user='org-admin', expected=409
    )


async def test_existing_account_password_is_never_overwritten(api, identity_provider):
    identity_provider['users']['new-employee'] = {
        'id': str(uuid4()),
        'username': 'new-employee',
        'enabled': True,
        'firstName': 'Уже существующий сотрудник',
        'email': 'existing@example.test',
    }
    await api.call(ADMIN + '/members/create', new_member(), user='org-admin', expected=409)
    assert 'credentials' not in identity_provider['users']['new-employee']
    added = await api.call(
        ADMIN + '/members/add',
        {
            'organization_id': ORGANIZATION,
            'username': 'new-employee',
            'role': 'viewer',
        },
        user='org-admin',
        expected=201,
    )
    assert added['member']['display_name'] == 'Уже существующий сотрудник'


async def test_cross_organization_create_does_not_call_identity_provider(api, identity_provider):
    await api.call(
        ADMIN + '/members/create',
        {
            **new_member(),
            'organization_id': OTHER_ORGANIZATION,
        },
        user='org-admin',
        expected=403,
    )
    assert identity_provider['calls'] == []


async def test_provider_outage_can_be_retried_with_same_request(api, identity_provider):
    body = new_member()
    identity_provider['unavailable'] = True
    await api.call(ADMIN + '/members/create', body, user='org-admin', expected=503)
    identity_provider['unavailable'] = False
    await api.call(ADMIN + '/members/create', body, user='org-admin', expected=201)


async def test_custom_role_updates_permissions_for_all_assigned_members(api, identity_provider):
    body = {
        'organization_id': ORGANIZATION,
        'id': 'custom-logistician',
        'name': 'Логист',
        'description': 'Без изменения контрагентов',
        'permissions': ['shipments.read', 'shipments.write', 'counterparties.read'],
    }
    role = (await api.call(ADMIN + '/roles/create', body, user='org-admin', expected=201))['role']
    assert role['permissions'] == body['permissions']
    identity_provider['users']['employee'] = {
        'id': 'employee',
        'username': 'employee',
        'firstName': 'Сотрудник',
        'email': 'employee@example.test',
        'enabled': True,
    }
    await api.call(
        ADMIN + '/members/add',
        {'organization_id': ORGANIZATION, 'username': 'employee', 'role': 'custom-logistician'},
        user='org-admin',
        expected=201,
    )
    await api.call('/api/organizations/access', {'permission': 'shipments.write'}, user='employee')
    await api.call(
        '/api/organizations/access',
        {'permission': 'counterparties.write'},
        user='employee',
        expected=403,
    )
    await api.call(
        ADMIN + '/roles/update',
        {**body, 'permissions': ['shipments.read'], 'expected_revision': role['revision']},
        user='org-admin',
    )
    await api.call(
        '/api/organizations/access',
        {'permission': 'shipments.write'},
        user='employee',
        expected=403,
    )
    await api.call('/api/organizations/access', {'permission': 'shipments.read'}, user='employee')


@pytest.mark.parametrize(
    'permission',
    [
        'configs.read',
        'configs.write',
        'configs.approve',
        'platform.admin',
        'transportations.create',
    ],
)
async def test_organization_roles_cannot_grant_platform_permissions(api, permission):
    await api.call(
        ADMIN + '/roles/create',
        {
            'organization_id': ORGANIZATION,
            'id': 'escalation',
            'name': 'Запрещено',
            'description': '',
            'permissions': [permission],
        },
        user='org-admin',
        expected=400,
    )


async def test_roles_are_isolated_and_cannot_remove_last_administrator(api):
    await api.call(
        ADMIN + '/roles/list',
        {'organization_id': OTHER_ORGANIZATION},
        user='org-admin',
        expected=403,
    )
    await api.call(
        ADMIN + '/roles/create',
        {
            'organization_id': OTHER_ORGANIZATION,
            'id': 'foreign',
            'name': 'Чужая роль',
            'description': '',
            'permissions': ['shipments.read'],
        },
        user='org-admin',
        expected=403,
    )
    await api.call(
        ADMIN + '/roles/update',
        {
            'organization_id': ORGANIZATION,
            'id': 'administrator',
            'name': 'Без управления',
            'description': '',
            'permissions': ['shipments.read'],
            'expected_revision': 1,
        },
        user='org-admin',
        expected=409,
    )


async def test_write_permission_requires_read(api):
    await api.call(
        ADMIN + '/roles/create',
        {
            'organization_id': ORGANIZATION,
            'id': 'missing-read',
            'name': 'Без чтения',
            'description': '',
            'permissions': ['shipments.write'],
        },
        user='org-admin',
        expected=400,
    )


@pytest.mark.parametrize('email', ['a@@example.test', 'a@example', 'a@.test', 'a b@example.test'])
async def test_create_employee_rejects_invalid_email(api, email):
    await api.call(ADMIN + '/members/create', {**new_member(), 'email': email}, user='org-admin', expected=400)
