"""Серверные таблицы админки: полный порядок, сочетания фильтров и область доступа."""

from uuid import UUID
import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION, ISSUER

ORGS = '/api/admin/organizations/list'
MEMBERS = '/api/admin/organizations/members/list'


@pytest.fixture
def registry_records(pgsql):
    with pgsql['db_1'].cursor() as cursor:
        cursor.executemany(
            'INSERT INTO organizations(id,name,enabled) VALUES(%s,%s,%s)',
            [(str(UUID(int=100 + i)), f'Alpha-{i%3}', i % 2 == 0) for i in range(18)],
        )
        for organization in [ORGANIZATION, OTHER_ORGANIZATION]:
            cursor.executemany(
                '''INSERT INTO memberships
                (organization_id,issuer,subject,username,display_name,email,role,enabled)
                VALUES(%s,%s,%s,%s,%s,%s,%s,%s)''',
                [
                    (
                        organization,
                        ISSUER,
                        f'member-{i:02}',
                        f'user-{i:02}',
                        f'Employee-{i%3}',
                        f'user-{i:02}@example.test',
                        'operator' if i % 2 else 'viewer',
                        i % 3 != 0,
                    )
                    for i in range(18)
                ],
            )


async def pages(api, path, query, **kwargs):
    result = []
    for _ in range(30):
        page = await api.call(path, {**query, 'limit': 4}, **kwargs)
        result += page['items']
        if not page.get('next_cursor'):
            return result
        query = {**query, 'cursor': page['next_cursor']}
    pytest.fail('Курсор не завершил обход')


@pytest.mark.parametrize('sort', ['id', 'name', 'enabled'])
@pytest.mark.parametrize('descending', [False, True])
async def test_organization_pages(api, registry_records, sort, descending):
    items = await pages(api, ORGS, {'query': 'Alpha', 'sort': sort, 'descending': descending})
    key = lambda item: (str(item[sort]).lower(), item['id']) if sort != 'id' else (item['id'],)
    assert len(items) == 18
    assert items == sorted(items, key=key, reverse=descending)
    assert len({row['id'] for row in items}) == 18


@pytest.mark.parametrize('sort', ['subject', 'display_name', 'username', 'email', 'enabled'])
@pytest.mark.parametrize('descending', [False, True])
async def test_member_pages_and_combined_filters(api, registry_records, sort, descending):
    items = await pages(
        api,
        MEMBERS,
        {
            'organization_id': ORGANIZATION,
            'query': 'Employee',
            'sort': sort,
            'descending': descending,
            'role': 'viewer',
            'enabled': True,
        },
        user='org-admin',
    )
    assert len(items) == 6
    assert all(item['role'] == 'viewer' and item['enabled'] for item in items)
    key = lambda item: (
        (str(item[sort]).lower(), item['subject']) if sort != 'subject' else (item['subject'],)
    )
    assert items == sorted(items, key=key, reverse=descending)
    assert len({row['subject'] for row in items}) == 6


async def test_cursor_is_bound_to_filters_and_organization(api, registry_records):
    body = {'organization_id': ORGANIZATION, 'sort': 'display_name', 'limit': 2}
    cursor = (await api.call(MEMBERS, body, user='org-admin'))['next_cursor']
    await api.call(
        MEMBERS, {**body, 'cursor': cursor, 'query': 'other'}, user='org-admin', expected=400
    )
    await api.call(
        MEMBERS, {**body, 'cursor': cursor, 'organization_id': OTHER_ORGANIZATION}, expected=400
    )
    await api.call(
        MEMBERS, {**body, 'organization_id': OTHER_ORGANIZATION}, user='org-admin', expected=403
    )
    await api.call(ORGS, {}, user='org-admin', expected=403)


@pytest.mark.parametrize('query', ['%', '_', "' OR 1=1 --"])
async def test_literal_search(api, registry_records, query):
    assert (await api.call(ORGS, {'query': query}))['items'] == []
    assert (await api.call(MEMBERS, {'organization_id': ORGANIZATION, 'query': query}))[
        'items'
    ] == []


async def test_roles_are_sorted_by_display_name(api, registry_records, pgsql):
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            "UPDATE organization_roles SET name='AAA' WHERE organization_id=%s AND id='viewer'",
            (ORGANIZATION,),
        )
        cursor.execute(
            "UPDATE organization_roles SET name='ZZZ' WHERE organization_id=%s AND id='operator'",
            (ORGANIZATION,),
        )
    items = await pages(
        api, MEMBERS, {'organization_id': ORGANIZATION, 'query': 'Employee', 'sort': 'role'}
    )
    assert [row['role'] for row in items] == ['viewer'] * 9 + ['operator'] * 9

async def test_exact_last_page_and_disabled_organizations(api, registry_records):
    page = await api.call(ORGS, {'query': 'Alpha', 'enabled': False, 'sort': 'name', 'limit': 9})
    assert len(page['items']) == 9
    assert all(not row['enabled'] for row in page['items'])
    assert 'next_cursor' not in page
