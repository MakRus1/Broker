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
    shipment_routes.set(shipment, [stage], revision=2)
    for id in ids:
        value = (await api.post('/api/v1/transportations/get', {'transportation_id': id}))[
            'transportation'
        ]
        assert value['route_segments'][0][date_field]['eta'] == '2030-01-02'
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
    cursor.execute('SELECT document FROM route_segments WHERE transportation_id=%s', (ids[0],))
    assert date_field not in cursor.fetchone()[0]
