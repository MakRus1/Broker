"""Море и авиа наследуются из поставки и не копируются в локальные поля перевозки."""

from uuid import uuid4
import pytest
from test_transportations import route_segment


@pytest.mark.parametrize('mode', ['sea', 'air'])
async def test_live_shared_details(api, shipment_routes, pgsql, mode):
    shipment = str(uuid4())
    stage = route_segment(mode)
    date_field = 'sea' if mode == 'sea' else 'air'
    location = {
        'id': str(uuid4()),
        'kind': 'sea_port' if mode == 'sea' else 'airport',
        'name': 'Общий пункт',
        'country_code': 'RU',
        'code': 'TEST',
    }
    stage[date_field] = {
        'eta': '2030-01-01',
        ('pol' if mode == 'sea' else 'origin_airport'): location,
    }
    stage[date_field]['documents'] = [
        {'number': 'DOC-42'},
    ]
    if mode == 'sea':
        stage['sea']['booking'] = {'number': 'BKG-42', 'status': 'confirmed'}
    shipment_routes.set(shipment, [stage])
    ids = []
    for _ in range(2):
        result = (
            await api.post(
                '/api/v1/transportations/create',
                {
                    'id': str(uuid4()),
                    'shipment_id': shipment,
                    'type': 'container',
                    'hazardous': False,
                },
            )
        )['transportation']
        ids.append(result['id'])
        assert result['route_segments'][0][date_field] == stage[date_field]
    stage[date_field]['eta'] = '2030-01-02'
    stage[date_field]['documents'][0]['number'] = 'DOC-43'
    shipment_routes.set(shipment, [stage], revision=2)
    for id in ids:
        value = (await api.post('/api/v1/transportations/get', {'transportation_id': id}))[
            'transportation'
        ]
        assert value['route_segments'][0][date_field] == stage[date_field]
        assert value['revision'] == 1
    await api.post(
        '/api/v1/transportation-actuals/record',
        {
            'transportation_id': ids[0],
            'expected_revision': 1,
            'route_segment_id': stage['id'],
            'event_kind': 'departure',
            'occurred_at': '2020-01-01T00:00:00+00:00',
        },
        expected=400,
    )
    result = (
        await api.post(
            '/api/v1/transportation-routes/set',
            {
                'transportation_id': ids[0],
                'expected_revision': 1,
                'route_segments': [{'id': stage['id'], 'mode': mode}],
            },
        )
    )['transportation']
    assert result['route_segments'][0][date_field] == stage[date_field]
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT jsonb_array_elements(route_segments) FROM transportations WHERE id=%s', (ids[0],))
    assert date_field not in cursor.fetchone()[0]


@pytest.mark.parametrize('mode', ['international_road', 'road', 'rail', 'pickup'])
async def test_inherited_plan_is_live_and_not_overwritten_by_execution(api, shipment_routes, pgsql, mode):
    shipment = str(uuid4())
    stage = route_segment(mode)
    stage['plan'] = {'origin_name': 'Роттердам', 'destination_name': 'Москва',
                     'departure_date': '2030-01-10', 'arrival_date': '2030-01-15'}
    shipment_routes.set(shipment, [stage])
    created = (await api.post('/api/v1/transportations/create', {
        'id': str(uuid4()), 'shipment_id': shipment, 'type': 'container', 'hazardous': False,
    }))['transportation']
    assert created['route_segments'][0]['plan'] == stage['plan']
    changed = (await api.post('/api/v1/transportation-routes/set', {
        'transportation_id': created['id'], 'expected_revision': 1,
        'route_segments': [{'id': stage['id'], 'mode': mode}],
    }))['transportation']
    assert changed['route_segments'][0]['plan'] == stage['plan']
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT jsonb_array_elements(route_segments) FROM transportations WHERE id=%s', (created['id'],))
        assert 'plan' not in cursor.fetchone()[0]
    stage['plan']['destination_name'] = 'Казань'
    shipment_routes.set(shipment, [stage], revision=2)
    fetched = (await api.post('/api/v1/transportations/get', {'transportation_id': created['id']}))['transportation']
    assert fetched['route_segments'][0]['plan'] == stage['plan']


async def test_sea_unloading_is_a_fact_of_each_transportation(api, shipment_routes):
    shipment = str(uuid4())
    stage = route_segment('sea')
    stage['sea'] = {'vessel': 'Shared vessel', 'voyage': '42', 'eta': '2020-01-01'}
    shipment_routes.set(shipment, [stage])
    request_stage = {'id': stage['id'], 'mode': 'sea'}
    dates = {}
    for date in ('2020-01-02', '2020-01-03'):
        created = (await api.post('/api/v1/transportations/create', {
            'id': str(uuid4()), 'shipment_id': shipment, 'type': 'container', 'hazardous': False,
        }))['transportation']
        body = {
            'transportation_id': created['id'], 'expected_revision': created['revision'],
            'type': 'container', 'hazardous': False,
        }
        await api.post('/api/v1/transportations/update', {
            **body, 'route_segments': [{**request_stage, 'sea_unloading_date': '9999-01-01'}],
        }, expected=400)
        saved = (await api.post('/api/v1/transportations/update', {
            **body, 'route_segments': [{**request_stage, 'sea_unloading_date': date}],
        }))['transportation']
        assert saved['route_segments'][0]['sea_unloading_date'] == date
        assert saved['route_segments'][0]['sea'] == stage['sea']
        dates[created['id']] = date

    stage['sea']['eta'] = '2020-01-02'
    shipment_routes.set(shipment, [stage], revision=2)
    listed = (await api.post('/api/v1/transportations/list', {
        'filters': [{'field': 'shipment_id', 'values': [shipment]}],
    }))['items']
    assert {item['id']: item['route_segments'][0]['sea_unloading_date'] for item in listed} == dates
    assert all(item['route_segments'][0]['sea'] == stage['sea'] for item in listed)

    first, second = listed
    retained = (await api.post('/api/v1/transportation-routes/set', {
        'transportation_id': first['id'], 'expected_revision': first['revision'],
        'route_segments': [request_stage],
    }))['transportation']
    assert retained['route_segments'][0]['sea_unloading_date'] == dates[first['id']]
    cleared = (await api.post('/api/v1/transportations/update', {
        'transportation_id': first['id'], 'expected_revision': retained['revision'],
        'type': 'container', 'hazardous': False,
        'route_segments': [{**request_stage, 'sea_unloading_date': None}],
    }))['transportation']
    assert 'sea_unloading_date' not in cleared['route_segments'][0]
    other = (await api.post('/api/v1/transportations/get', {
        'transportation_id': second['id'],
    }))['transportation']
    assert other['route_segments'][0]['sea_unloading_date'] == dates[second['id']]
