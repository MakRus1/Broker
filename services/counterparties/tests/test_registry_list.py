"""Фильтры справочника применяются до LIMIT, курсор связан с запросом."""

from uuid import UUID
import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION

PATH = '/api/v1/counterparties/list'


@pytest.fixture
def records(pgsql):
    rows = [
        {
            'id': str(UUID(int=i + 1)),
            'name': ['Alpha', 'Beta', 'alpha'][i % 3],
            'legal_name': f'Компания {i}',
            'tax_id': str(1000000000 + i) if i % 4 else None,
            'country_code': ['RU', 'CN', None][i % 3],
            'archived': i % 4 == 0,
            'roles': ['customer'] if i % 2 else ['carrier'],
        }
        for i in range(21)
    ]
    with pgsql['db_1'].cursor() as cursor:
        for organization in [ORGANIZATION, OTHER_ORGANIZATION]:
            cursor.executemany(
                """INSERT INTO counterparties (organization_id,id,name,legal_name,
                tax_id,country_code,archived,roles) VALUES (%s,%s,%s,%s,%s,%s,%s,%s)""",
                [
                    (
                        organization,
                        r['id'],
                        r['name'],
                        r['legal_name'],
                        r['tax_id'],
                        r['country_code'],
                        r['archived'],
                        r['roles'],
                    )
                    for r in rows
                ],
            )
    return rows


@pytest.mark.parametrize('sort', ['id', 'name', 'country_code', 'tax_id', 'archived'])
@pytest.mark.parametrize('descending', [False, True])
async def test_cursor_paging_with_empty_requisites(api, records, sort, descending):
    body = {'sort': sort, 'descending': descending, 'status': 'all', 'limit': 4}
    found = []
    for _ in range(10):
        page = await api.post(PATH, body)
        found.extend(r['id'] for r in page['items'])
        if not page.get('next_cursor'):
            break
        body['cursor'] = page['next_cursor']
    expected = sorted(
        records,
        key=lambda r: (
            (
                ''
                if sort == 'id'
                else str(int(r[sort])) if sort == 'archived' else (r[sort] or '').lower()
            ),
            r['id'],
        ),
        reverse=descending,
    )
    assert found == [r['id'] for r in expected]
    assert len(set(found)) == len(records)


async def test_combined_filters(api, records):
    page = await api.post(
        PATH,
        {
            'query': 'LPh',
            'role': 'customer',
            'status': 'active',
            'country_code': 'RU',
            'sort': 'name',
        },
    )
    expected = [
        r
        for r in records
        if 'lph' in r['name'].lower()
        and 'customer' in r['roles']
        and not r['archived']
        and r['country_code'] == 'RU'
    ]
    assert {r['id'] for r in page['items']} == {r['id'] for r in expected}


@pytest.mark.parametrize(
    'patch',
    [
        {'query': 'alpha'},
        {'role': 'carrier'},
        {'status': 'archived'},
        {'country_code': 'RU'},
        {'sort': 'name'},
        {'descending': True},
    ],
)
async def test_cursor_scope(api, records, patch):
    first = await api.post(PATH, {'limit': 2})
    await api.post(PATH, {'cursor': first['next_cursor'], **patch}, expected=400)


async def test_scope_is_bound_to_organization(api, records):
    first = await api.post(PATH, {'limit': 2})
    await api.post(PATH, {'cursor': first['next_cursor']}, user='other', expected=400)


@pytest.mark.parametrize('query', ['%', '_', "' OR 1=1 --"])
async def test_literal_search(api, records, query):
    assert (await api.post(PATH, {'query': query, 'status': 'all'}))['items'] == []


async def test_last_page_and_legacy_after(api, records):
    first = await api.post(PATH, {'limit': 5, 'status': 'all'})
    second = await api.post(PATH, {'limit': 5, 'status': 'all', 'after': first['items'][-1]['id']})
    assert [r['id'] for r in second['items']] == [r['id'] for r in records[5:10]]
    assert not (await api.post(PATH, {'limit': len(records), 'status': 'all'})).get('next_cursor')


async def test_batch_ids_stays_in_organization(api, records):
    ids = [row['id'] for row in records[:3]]
    page = await api.post(PATH, {'ids': ids, 'status': 'all'})
    assert {row['id'] for row in page['items']} == set(ids)
    assert len(page['items']) == 3
