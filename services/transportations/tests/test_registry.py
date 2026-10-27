"""Курсорный реестр: фильтры, порядок и граница организаций."""
from uuid import UUID
import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION

PATH = '/api/v1/transportations/list'


@pytest.fixture
def records(pgsql):
    values = []
    for index in range(24):
        values.append({
            'id': str(UUID(int=index + 1)),
            'container_number': [None, 'MSCU1234567', 'abcd0000001', 'MSCU7654321'][index % 4],
            'type': ['container', 'air', 'rail'][index % 3],
            'container_type_code': ['20DC', '40HC'][index % 2],
            'hazardous': index % 5 == 0,
        })
    with pgsql['db_1'].cursor() as cursor:
        for organization in [ORGANIZATION, OTHER_ORGANIZATION]:
            cursor.executemany("""
                INSERT INTO transportations (organization_id, id, shipment_id, ordinal,
                    container_number, type, container_type_code, hazardous)
                VALUES (%s, %s, %s, 1, %s, %s, %s, %s)
            """, [(organization, row['id'], str(UUID(int=1000)), row['container_number'],
                   row['type'], row['container_type_code'], row['hazardous']) for row in values])
    return values


@pytest.mark.parametrize('sort', ['id', 'container_number', 'container_type', 'type'])
@pytest.mark.parametrize('descending', [False, True])
async def test_cursor_covers_ties_and_empty_values(api, records, sort, descending):
    body = { 'limit': 5, 'sort': sort, 'descending': descending}
    found = []
    for _ in range(10):
        page = await api.post(PATH, body)
        found.extend(row['id'] for row in page['items'])
        if not page.get('next_cursor'):
            break
        body['cursor'] = page['next_cursor']
    field = {'container_type': 'container_type_code'}.get(sort, sort)
    expected = sorted(records, key=lambda row: (
        '' if sort == 'id' else (row[field] or '').lower() if sort == 'container_number' else row[field],
        row['id']), reverse=descending)
    assert found == [row['id'] for row in expected]
    assert len(found) == len(set(found)) == len(records)


async def test_combined_filters_and_case_insensitive_search(api, records):
    page = await api.post(PATH, {'query': '  mscu  ', 'types': ['container', 'rail'],
        'container_types': ['20DC', '40HC'], 'hazardous': True, 'sort': 'container_number'})
    expected = [row for row in records if (row['container_number'] or '').startswith('MSCU')
        and row['type'] in ['container', 'rail'] and row['hazardous']]
    assert {row['id'] for row in page['items']} == {row['id'] for row in expected}
    assert 'next_cursor' not in page


@pytest.mark.parametrize('patch', [{'types': ['air']}, {'sort': 'type'}, {'descending': True},
                                  {'query': 'MSCU'}, {'hazardous': False}])
async def test_cursor_cannot_be_reused_for_another_query(api, records, patch):
    page = await api.post(PATH, {'limit': 2})
    await api.post(PATH, {'limit': 2, 'cursor': page['next_cursor'], **patch}, expected=400)


async def test_cursor_is_scoped_to_organization(api, records):
    page = await api.post(PATH, {'limit': 2})
    await api.post(PATH, {'limit': 2, 'cursor': page['next_cursor']}, user='other', expected=400)


@pytest.mark.parametrize('query', ['%', '_', "' OR 1=1 --", "\\"])
async def test_search_treats_special_characters_literally(api, records, query):
    assert (await api.post(PATH, {'query': query}))['items'] == []


async def test_legacy_after_remains_uuid_ordered(api, records):
    page = await api.post(PATH, {'limit': 5})
    more = await api.post(PATH, {'limit': 5, 'after': page['items'][-1]['id']})
    assert [row['id'] for row in more['items']] == [row['id'] for row in records[5:10]]
    await api.post(PATH, {'after': page['items'][-1]['id'], 'sort': 'container_number'}, expected=400)


async def test_exact_page_has_no_false_next_cursor(api, records):
    page = await api.post(PATH, {'limit': len(records)})
    assert len(page['items']) == len(records)
    assert not page.get('next_cursor')


async def test_search_matches_inside_number(api, records):
    page = await api.post(PATH, {'query': '123'})
    assert page['items']
    assert all('123' in row['container_number'] for row in page['items'])


async def test_next_cursor_is_returned_without_opt_in(api, records):
    page = await api.post(PATH, {'limit': 2})
    assert page['next_cursor']['id'] == page['items'][-1]['id']


@pytest.mark.parametrize('field', ['types', 'container_types'])
async def test_empty_filter_set_matches_nothing(api, records, field):
    assert (await api.post(PATH, {field: []}))['items'] == []


async def test_false_hazardous_filter_is_not_omitted(api, records):
    page = await api.post(PATH, {'hazardous': False})
    assert {row['id'] for row in page['items']} == {
        row['id'] for row in records if not row['hazardous']
    }
