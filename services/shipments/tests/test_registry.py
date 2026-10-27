"""Даты фильтров и курсора остаются календарными датами."""

from datetime import date, timedelta
from uuid import UUID
import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION

PATH = '/api/v1/shipments/list'


@pytest.fixture
def records(pgsql):
    rows = [
        {
            'id': str(UUID(int=i + 1)),
            'number': f'S-{i:02d}',
            'customer_id': str(UUID(int=1000 + i % 2)),
            'shipment_date': date(2026, 1, 1) + timedelta(days=i % 4),
            'status': ['Новая', 'В работе'][i % 2],
            'cargo_description': ['Сталь', 'Трубы', None][i % 3],
        }
        for i in range(21)
    ]
    with pgsql['db_1'].cursor() as cursor:
        for organization in [ORGANIZATION, OTHER_ORGANIZATION]:
            cursor.executemany(
                """INSERT INTO shipments (organization_id,id,number,customer_id,
                shipment_date,status,cargo_description,route_segments) VALUES (%s,%s,%s,%s,%s,%s,%s,'{}')""",
                [
                    (
                        organization,
                        r['id'],
                        r['number'],
                        r['customer_id'],
                        r['shipment_date'],
                        r['status'],
                        r['cargo_description'],
                    )
                    for r in rows
                ],
            )
    return rows


@pytest.mark.parametrize('sort', ['id', 'number', 'shipment_date', 'status', 'cargo_description'])
@pytest.mark.parametrize('descending', [False, True])
async def test_all_pages_preserve_order_and_ties(api, records, sort, descending):
    body = {'limit': 4, 'sort': sort, 'descending': descending}
    found = []
    for _ in range(10):
        page = await api.post(PATH, body)
        found.extend(r['id'] for r in page['items'])
        if not page.get('next_cursor'):
            break
        cursor = page['next_cursor']
        if sort == 'shipment_date':
            assert len(cursor['date_value']) == 10
            assert 'text_value' not in cursor
        else:
            assert 'date_value' not in cursor
            assert 'text_value' in cursor
        body['cursor'] = cursor
    expected = sorted(
        records,
        key=lambda r: (
            '' if sort == 'id' else r[sort] if sort == 'shipment_date' else (r[sort] or '').lower(),
            r['id'],
        ),
        reverse=descending,
    )
    assert found == [r['id'] for r in expected]
    assert len(set(found)) == len(records)


async def test_combined_filters_and_inclusive_range(api, records):
    body = {
        'cargo_query': 'таль',
        'customer_id': str(UUID(int=1000)),
        'statuses': ['Новая'],
        'date_from': '2026-01-01',
        'date_to': '2026-01-03',
        'sort': 'shipment_date',
    }
    page = await api.post(PATH, body)
    expected = [
        r
        for r in records
        if r['cargo_description'] == 'Сталь'
        and r['customer_id'] == body['customer_id']
        and date(2026, 1, 1) <= r['shipment_date'] <= date(2026, 1, 3)
    ]
    assert {r['id'] for r in page['items']} == {r['id'] for r in expected}


@pytest.mark.parametrize(
    'patch',
    [
        {'date_from': '2026-01-02'},
        {'customer_id': str(UUID(int=1001))},
        {'sort': 'number'},
        {'descending': True},
    ],
)
async def test_cursor_scope(api, records, patch):
    page = await api.post(PATH, {'limit': 2, 'sort': 'shipment_date'})
    await api.post(
        PATH, {'sort': 'shipment_date', 'cursor': page['next_cursor'], **patch}, expected=400
    )


async def test_cursor_date_cannot_be_replaced_by_text(api, records):
    page = await api.post(PATH, {'limit': 2, 'sort': 'shipment_date'})
    cursor = page['next_cursor']
    cursor['text_value'] = cursor.pop('date_value')
    await api.post(PATH, {'sort': 'shipment_date', 'cursor': cursor}, expected=400)


async def test_date_range_validation(api, records):
    await api.post(PATH, {'date_from': '2026-01-04', 'date_to': '2026-01-01'}, expected=400)
    await api.post(PATH, {'date_from': '2026-02-29'}, expected=400)


async def test_organization_scope(api, records):
    page = await api.post(PATH, {'limit': 2})
    await api.post(PATH, {'cursor': page['next_cursor']}, user='other', expected=400)


async def test_legacy_after_and_last_page(api, records):
    first = await api.post(PATH, {'limit': 5})
    second = await api.post(PATH, {'limit': 5, 'after': first['items'][-1]['id']})
    assert [r['id'] for r in second['items']] == [r['id'] for r in records[5:10]]
    page = await api.post(PATH, {'limit': len(records)})
    assert not page.get('next_cursor')


@pytest.mark.parametrize('query', ['%', '_', "' OR 1=1 --"])
async def test_literal_search(api, records, query):
    assert (await api.post(PATH, {'cargo_query': query}))['items'] == []


async def test_number_and_cargo_are_independent(api, records):
    assert (await api.post(PATH, {'number_query': 'Сталь'}))['items'] == []
    assert (await api.post(PATH, {'cargo_query': 'S-'}))['items'] == []
    matched = await api.post(PATH, {'number_query': 'S-00', 'cargo_query': 'Сталь'})
    assert [row['id'] for row in matched['items']] == [records[0]['id']]
    assert (await api.post(PATH, {'number_query': 'S-00', 'cargo_query': 'Трубы'}))['items'] == []
