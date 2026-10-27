"""Постраничный общий справочник: одинаковые значения, фильтры и запрет чужого доступа."""

from uuid import UUID
import pytest

ROOT = '/api/admin/railway-stations/list'
BASE = {}


@pytest.fixture
def directory_records(pgsql):
    with pgsql['db_1'].cursor() as cursor:
        cursor.executemany(
            "INSERT INTO railway_stations(esr_code,name,country_code,archived) VALUES(%s,%s,%s,%s)",
            [(f'{i+100:06}', f'Alpha-{i%3}', 'RU' if i % 2 else 'KZ', i == 19) for i in range(20)],
        )


@pytest.mark.parametrize('sort', ['esr_code', 'name', 'country_code'])
@pytest.mark.parametrize('descending', [False, True])
async def test_directory_cursor_order(api, directory_records, sort, descending):
    query = {**BASE, 'query': 'Alpha', 'sort': sort, 'descending': descending, 'limit': 4}
    items = []
    for _ in range(10):
        page = await api.post(ROOT, query, roles=['platform.admin'])
        items += page['items']
        if not page.get('next_cursor'):
            break
        query = {**query, 'cursor': page['next_cursor']}
    key = lambda item: (
        (item.get(sort) or '').lower() if sort == 'name' else item.get(sort) or '',
        item['esr_code'],
    )
    assert len(items) == 19
    assert len({item['esr_code'] for item in items}) == 19
    assert items == sorted(items, key=key, reverse=descending)


async def test_directory_filters_and_scope(api, directory_records):
    body = {**BASE, 'query': 'Alpha', 'country_code': 'RU', 'sort': 'name', 'limit': 2}
    first = await api.post(ROOT, body, roles=['platform.admin'])
    assert all(row['country_code'] == 'RU' for row in first['items'])
    await api.post(
        ROOT,
        {**body, 'country_code': 'KZ', 'cursor': first['next_cursor']},
        roles=['platform.admin'],
        expected=400,
    )
    await api.post(ROOT, BASE, user='other', expected=403)


@pytest.mark.parametrize('query', ['%', '_', "' OR 1=1 --"])
async def test_directory_search_is_literal(api, directory_records, query):
    assert (await api.post(ROOT, {**BASE, 'query': query}, roles=['platform.admin']))['items'] == []
