"""Постраничный общий справочник: одинаковые значения, фильтры и запрет чужого доступа."""

from uuid import UUID
import pytest

ROOT = '/api/admin/route-locations/list'
BASE = {'kind': 'sea_port'}


@pytest.fixture
def directory_records(pgsql):
    with pgsql['db_1'].cursor() as cursor:
        cursor.executemany(
            "INSERT INTO route_locations(id,kind,name,country_code,code,archived) VALUES(%s,%s,%s,%s,%s,%s)",
            [
                (
                    str(UUID(int=i + 100)),
                    'sea_port' if i < 20 else 'airport',
                    f'Alpha-{i%3}',
                    'RU' if i % 2 else 'KZ',
                    None if i % 3 else f'CODE{i}',
                    i == 19,
                )
                for i in range(22)
            ],
        )


@pytest.mark.parametrize('sort', ['id', 'name', 'country_code', 'code'])
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
        item['id'],
    )
    assert len(items) == 19
    assert len({item['id'] for item in items}) == 19
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
