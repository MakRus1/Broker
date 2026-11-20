"""Фильтры маршрута работают по участкам всей поставки, до пагинации."""

import json
from uuid import uuid4

import pytest
from conftest import ORGANIZATION, OTHER_ORGANIZATION

LIST = '/api/v1/shipments/list'
VALUES = '/api/v1/shipments/filter-values'


@pytest.fixture
def routes(pgsql):
    ids = [str(uuid4()) for _ in range(3)]
    port, airport = str(uuid4()), str(uuid4())
    structures = [
        [('sea', 'Альфа', 'Бета'), ('rail', 'Бета', 'Гамма')],
        [('rail', 'Альфа', 'Бета')],
        [('sea', 'Чужой порт', 'Чужой город')],
    ]
    with pgsql['db_1'].cursor() as cursor:
        for index, structure in enumerate(structures):
            route = [
                {
                    'id': str(uuid4()), 'type': mode,
                    'plan': {'origin_name': origin, 'destination_name': destination},
                }
                for mode, origin, destination in structure
            ]
            if index == 0:
                route[0]['sea'] = {'pol': {
                    'id': port, 'kind': 'sea_port',
                    'name': 'Порт', 'country_code': 'RU', 'code': 'PORT',
                }}
                route.append({
                    'id': str(uuid4()), 'type': 'air',
                    'air': {'destination_airport': {
                        'id': airport, 'kind': 'airport',
                        'name': 'Аэропорт', 'country_code': 'RU', 'code': 'AIR',
                    }},
                })
            cursor.execute(
                '''INSERT INTO shipments (organization_id,id,number,customer_id,route_segments)
                VALUES (%s,%s,%s,%s,ARRAY(SELECT json_populate_record(
                    NULL::public.shipment_route_segment_v4, value)
                    FROM json_array_elements(%s::json)))''',
                (OTHER_ORGANIZATION if index == 2 else ORGANIZATION,
                 ids[index], f'Route-{index}', str(uuid4()), json.dumps(route)),
            )
    return ids, port, airport


def group(*filters):
    return {'filters': list(filters)}


def value(field, selected):
    return {'field': field, 'values': [selected]}


async def test_same_segment_and_multiple_groups(api, routes):
    routes, _, _ = routes
    body = {'route_filters': [group(value('mode', 'sea'), value('destination_name', 'Гамма'))]}
    assert (await api.post(LIST, body))['items'] == []
    body['route_filters'] = [
        group(value('mode', 'sea'), value('destination_name', 'Бета')),
        group(value('mode', 'rail'), value('destination_name', 'Гамма')),
    ]
    body['limit'] = 1
    result = await api.post(LIST, body)
    assert [item['id'] for item in result['items']] == [routes[0]]
    assert not result.get('next_cursor')
    body['filters'] = [value('id', routes[1])]
    assert (await api.post(LIST, body))['items'] == []


async def test_facets_respect_segment_main_filters_and_organization(api, routes):
    routes, _, _ = routes
    body = {
        'scope': 'route', 'field': 'destination_name',
        'registry': {'route_filters': [group(value('mode', 'sea'))]},
    }
    result = await api.post(VALUES, body)
    assert [item['value'] for item in result['values']] == ['Бета']
    body['registry']['filters'] = [value('id', routes[1])]
    assert (await api.post(VALUES, body))['values'] == []


async def test_main_facets_respect_route_filters(api, routes):
    result = await api.post(VALUES, {
        'field': 'number',
        'registry': {'route_filters': [group(value('mode', 'sea'))]},
    })
    assert [item['value'] for item in result['values']] == ['route-0']


async def test_route_metadata_and_invalid_groups(api):
    metadata = await api.post(VALUES, {'scope': 'route', 'registry': {}})
    assert {'mode', 'origin_name', 'destination_name', 'pol', 'pod'} <= {
        field['id'] for field in metadata['fields']
    }
    await api.post(VALUES, {
        'scope': 'route', 'field': 'mode', 'group': 1, 'registry': {},
    }, expected=400)
    await api.post(LIST, {'route_filters': [group(value('unknown', 'sea'))]}, expected=400)


async def test_typed_ports_and_airports(api, routes):
    ids, port, airport = routes
    for field, selected in [('pol', port), ('destination_airport', airport)]:
        body = {'route_filters': [group(value(field, selected))]}
        result = await api.post(LIST, body)
        assert [item['id'] for item in result['items']] == [ids[0]]
        options = await api.post(VALUES, {'scope': 'route', 'field': field, 'registry': {}})
        assert [item['value'] for item in options['values']] == [selected]


async def test_current_field_is_excluded_from_facets(api, routes):
    result = await api.post(VALUES, {
        'scope': 'route', 'field': 'destination_name',
        'registry': {'route_filters': [group(
            value('mode', 'sea'), value('destination_name', 'Нет такого города'),
        )]},
    })
    assert [item['value'] for item in result['values']] == ['Бета']


async def test_cursor_cannot_be_reused_after_route_change(api, routes):
    first = await api.post(LIST, {'limit': 1})
    assert first.get('next_cursor')
    await api.post(LIST, {
        'limit': 1, 'cursor': first['next_cursor'],
        'route_filters': [group(value('mode', 'sea'))],
    }, expected=400)
