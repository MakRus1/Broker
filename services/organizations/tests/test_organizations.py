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
        'last_name': 'Иванов',
        'first_name': 'Иван',
        'middle_name': 'Иванович',
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
    assert first['member']['display_name'] == 'Иванов Иван Иванович'
    assert first['member']['last_name'] == 'Иванов'
    assert first['member']['first_name'] == 'Иван'
    assert first['member']['middle_name'] == 'Иванович'
    stored = identity_provider['users']['new-employee']
    assert stored['lastName'] == 'Иванов'
    assert stored['firstName'] == 'Иван'
    assert stored['attributes']['middle_name'] == ['Иванович']
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
    await api.call(
        ADMIN + '/members/create', {**new_member(), 'email': email}, user='org-admin', expected=400
    )


@pytest.mark.parametrize('middle', [None, '   '])
async def test_middle_name_is_optional(api, identity_provider, middle):
    body = {**new_member(), 'middle_name': middle}
    member = (await api.call(ADMIN + '/members/create', body, user='org-admin', expected=201))[
        'member'
    ]
    assert member['middle_name'] is None
    assert member['display_name'] == 'Иванов Иван'


@pytest.mark.parametrize('field', ['last_name', 'first_name'])
async def test_names_cannot_be_blank(api, identity_provider, field):
    await api.call(
        ADMIN + '/members/create', {**new_member(), field: '  '}, user='org-admin', expected=400
    )
    assert identity_provider['calls'] == []


@pytest.mark.parametrize('attribute', [['Иванович', 'Петрович'], 'Иванович', [123]])
async def test_invalid_middle_name_from_provider_is_not_silently_discarded(
    api, identity_provider, attribute
):
    identity_provider['users']['employee'] = {
        'id': 'employee',
        'username': 'employee',
        'enabled': True,
        'firstName': 'Иван',
        'lastName': 'Иванов',
        'email': 'employee@example.test',
        'attributes': {'middle_name': attribute},
    }
    await api.call(
        ADMIN + '/members/add',
        {
            'organization_id': ORGANIZATION,
            'username': 'employee',
            'role': 'operator',
        },
        user='org-admin',
        expected=503,
    )


async def test_manual_profile_update_changes_all_memberships(api, mockserver, pgsql):
    from conftest import ISSUER

    calls = []
    profile = {
        'id': 'viewer',
        'username': 'viewer',
        'enabled': True,
        'firstName': 'Старое цельное ФИО',
        'email': 'viewer@example.test',
        'attributes': {'keep': ['existing'], 'middle_name': ['Старое']},
    }

    @mockserver.handler('/admin/realms/broker/users/viewer')
    def user(request):
        calls.append(request.method)
        if request.method == 'GET':
            return mockserver.make_response(json=profile)
        assert request.method == 'PUT'
        assert 'credentials' not in request.json
        assert request.json['attributes']['keep'] == ['existing']
        profile.update(request.json)
        return mockserver.make_response(status=204)

    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            '''INSERT INTO memberships
            (organization_id,issuer,subject,username,display_name,email,role)
            VALUES (%s,%s,'viewer','viewer','Старое ФИО','viewer@example.test','viewer')''',
            (OTHER_ORGANIZATION, ISSUER),
        )
    body = {
        'organization_id': ORGANIZATION,
        'subject': 'viewer',
        'last_name': ' Иванов ',
        'first_name': ' Иван ',
        'middle_name': 'Иванович',
    }
    await api.call(
        ADMIN + '/members/update-profile',
        {**body, 'organization_id': OTHER_ORGANIZATION},
        user='org-admin',
        expected=403,
    )
    assert calls == []
    member = (await api.call(ADMIN + '/members/update-profile', body, user='org-admin'))['member']
    assert member['display_name'] == 'Иванов Иван Иванович'
    assert profile['lastName'] == 'Иванов'
    assert profile['firstName'] == 'Иван'
    assert profile['attributes']['middle_name'] == ['Иванович']
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            "SELECT display_name,last_name,first_name,middle_name FROM memberships WHERE subject='viewer'"
        )
        assert cursor.fetchall() == [('Иванов Иван Иванович', 'Иванов', 'Иван', 'Иванович')] * 2
    cleared = (await api.call(ADMIN + '/members/update-profile', {**body, 'middle_name': None}))[
        'member'
    ]
    assert cleared['display_name'] == 'Иванов Иван'
    assert cleared['middle_name'] is None
    assert 'middle_name' not in profile['attributes']
