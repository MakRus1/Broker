"""Автомобильные этапы принадлежат перевозке; даты календарные, контрагенты проверяются."""

from uuid import uuid4
import pytest
from test_transportations import route_segment


async def create(api, shipment_routes, mode):
    shipment = str(uuid4())
    stage = route_segment(mode)
    shipment_routes.set(shipment, [stage])
    result = (
        await api.post(
            '/api/v1/transportations/create',
            {'id': str(uuid4()), 'shipment_id': shipment, 'type': 'container', 'hazardous': False},
        )
    )['transportation']
    return result, stage


def road():
    return {
        'loading_terminal_id': str(uuid4()),
        'planned_loading_date': '2030-01-01',
        'planned_unloading_date': '2030-01-03',
        'departure_date': '2020-01-01',
        'unloading_date': '2020-01-03',
        'vehicle_number': 'А123ВС',
        'trailer_number': 'АА1234',
        'cmr_number': 'CMR-12',
    }


def pickup():
    return {
        'planned_pickup_date': '2030-01-01',
        'planned_delivery_date': '2030-01-03',
        'pickup_date': '2020-01-01',
        'delivery_date': '2020-01-03',
        'delivery_city': 'Москва',
        'delivery_country': 'RU',
        'delivery_address': 'Склад 1',
        'driver': 'Иванов Иван Иванович',
        'vehicle_number': 'А123ВС',
        'ttn_date': '2020-01-01',
    }


@pytest.mark.parametrize(
    'mode,field,details',
    [
        ('international_road', 'international_road', road),
        ('pickup', 'pickup', pickup),
        ('road', 'pickup', pickup),
    ],
)
async def test_roundtrip(api, shipment_routes, mode, field, details):
    created, stage = await create(api, shipment_routes, mode)
    data = details()
    saved = (
        await api.post(
            '/api/v1/transportation-routes/set',
            {
                'transportation_id': created['id'],
                'expected_revision': 1,
                'route_segments': [{**stage, field: data, 'contractor_id': str(uuid4())}],
            },
        )
    )['transportation']
    assert saved['route_segments'][0][field] == data
    assert (await api.post('/api/v1/transportations/get', {'transportation_id': created['id']}))[
        'transportation'
    ] == saved
    history = await api.post(
        '/api/v1/transportations/history', {'transportation_id': created['id']}
    )
    assert history['items'][0]['route_segments'][0][field] == data
    assert shipment_routes.registrations
    await api.post(
        '/api/v1/transportation-actuals/record',
        {
            'transportation_id': created['id'],
            'expected_revision': 2,
            'route_segment_id': stage['id'],
            'event_kind': 'departure',
            'occurred_at': '2020-01-01T00:00:00+00:00',
        },
        expected=400,
    )


@pytest.mark.parametrize(
    'mode,field,data',
    [
        ('international_road', 'international_road', {'departure_date': '9999-01-01'}),
        (
            'international_road',
            'international_road',
            {'planned_loading_date': '2030-01-03', 'planned_unloading_date': '2030-01-01'},
        ),
        ('international_road', 'international_road', {'unloading_date': '2020-02-30'}),
        ('pickup', 'pickup', {'pickup_date': '2020-01-03', 'delivery_date': '2020-01-01'}),
        ('pickup', 'pickup', {'ttn_date': '9999-01-01'}),
        ('pickup', 'pickup', {'planned_pickup_date': '1111-01-01'}),
        ('rail', 'pickup', {'driver': 'Иванов'}),
        ('pickup', 'international_road', {'cmr_number': '12'}),
    ],
)
async def test_invalid_details_leave_revision(api, shipment_routes, mode, field, data):
    created, stage = await create(api, shipment_routes, mode)
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': created['id'],
            'expected_revision': 1,
            'route_segments': [{**stage, field: data}],
        },
        expected=400,
    )
    assert (await api.post('/api/v1/transportations/get', {'transportation_id': created['id']}))[
        'transportation'
    ]['revision'] == 1


async def test_terminal_must_belong_to_organization(api, shipment_routes, mockserver):
    created, stage = await create(api, shipment_routes, 'international_road')

    @mockserver.json_handler('/api/v1/counterparties/get')
    def missing(request):
        return mockserver.make_response(
            status=404, json={'error': 'not_found', 'message': 'Не найден'}
        )

    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': created['id'],
            'expected_revision': 1,
            'route_segments': [{**stage, 'international_road': road()}],
        },
        expected=400,
    )
