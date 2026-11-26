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
        'loading_address': 'Склад отправителя',
        'unloading_terminal_id': str(uuid4()),
        'unloading_address': 'Склад получателя',
        'customs_terminal_id': str(uuid4()),
        'customs_arrival_date': '2020-01-02',
        'customs_departure_date': '2020-01-02',
        'planned_border_crossings': ['Кукурыки — Козловичи', 'Красная Горка'],
        'planned_loading_date': '2030-01-01',
        'planned_unloading_date': '2030-01-03',
        'departure_date': '2020-01-01',
        'unloading_date': '2020-01-03',
        'vehicle_number': 'А123ВС',
        'trailer_number': 'АА1234',
        'cmr_number': 'CMR-12',
        'cmr_received_date': '2020-01-04',
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


def extract_actual_dates(details):
    fields = {'departure_date': 'actual_departure_date', 'unloading_date': 'actual_arrival_date'}
    return {target: details.pop(source) for source, target in fields.items() if source in details}


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
    actual_dates = extract_actual_dates(data) if mode == 'international_road' else {}
    saved = (
        await api.post(
            '/api/v1/transportation-routes/set',
            {
                'transportation_id': created['id'],
                'expected_revision': 1,
                'route_segments': [{**stage, **actual_dates, field: data, 'contractor_id': str(uuid4())}],
            },
        )
    )['transportation']
    assert saved['route_segments'][0][field] == data
    for key, value in actual_dates.items():
        assert saved['route_segments'][0][key] == value
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
        ('international_road', 'international_road', {'planned_border_crossings': ['  ']}),
        ('international_road', 'international_road', {'planned_border_crossings': ['x'] * 33}),
        ('international_road', 'international_road', {'customs_arrival_date': '9999-01-01'}),
        ('international_road', 'international_road', {'customs_departure_date': '9999-01-01'}),
        ('international_road', 'international_road', {'cmr_received_date': '9999-01-01'}),
        (
            'international_road',
            'international_road',
            {'customs_arrival_date': '2020-01-02', 'customs_departure_date': '2020-01-01'},
        ),
        (
            'international_road',
            'international_road',
            {'customs_departure_date': '2020-01-03', 'unloading_date': '2020-01-02'},
        ),
        (
            'international_road',
            'international_road',
            {'departure_date': '2020-01-02', 'customs_arrival_date': '2020-01-01'},
        ),
        (
            'international_road',
            'international_road',
            {'customs_arrival_date': '2020-01-02', 'unloading_date': '2020-01-01'},
        ),
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
    data = dict(data)
    actual_dates = extract_actual_dates(data) if mode == 'international_road' else {}
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': created['id'],
            'expected_revision': 1,
            'route_segments': [{**stage, **actual_dates, field: data}],
        },
        expected=400,
    )
    assert (await api.post('/api/v1/transportations/get', {'transportation_id': created['id']}))[
        'transportation'
    ]['revision'] == 1


@pytest.mark.parametrize(
    'field', ['loading_terminal_id', 'unloading_terminal_id', 'customs_terminal_id']
)
async def test_terminal_must_belong_to_organization(api, shipment_routes, mockserver, field):
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
            'route_segments': [{**stage, 'international_road': {field: str(uuid4())}}],
        },
        expected=400,
    )


@pytest.mark.parametrize(
    'mode,field,date_field',
    [
        ('international_road', 'international_road', 'planned_loading_date'),
        ('pickup', 'pickup', 'planned_pickup_date'),
    ],
)
async def test_shared_plan_update_preserves_each_vehicles_details(
    api, shipment_routes, mode, field, date_field
):
    shipment_id = str(uuid4())
    stage = route_segment(mode)
    stage['plan'] = {'destination_name': 'Москва', 'departure_date': '2030-01-10'}
    shipment_routes.set(shipment_id, [stage])
    details_by_id = {}

    for vehicle, date, customs_departure in [
        ('А123ВС', '2030-01-11', '2020-01-11'),
        ('В456СА', '2030-01-12', '2020-01-12'),
    ]:
        created = (await api.post('/api/v1/transportations/create', {
            'id': str(uuid4()), 'shipment_id': shipment_id, 'type': 'container', 'hazardous': False,
        }))['transportation']
        details = {'vehicle_number': vehicle, date_field: date}
        if mode == 'international_road':
            details['loading_address'] = f'Склад отправления {vehicle}'
            details['unloading_address'] = f'Склад назначения {vehicle}'
            details['unloading_terminal_id'] = str(uuid4())
            details['customs_terminal_id'] = str(uuid4())
            details['customs_departure_date'] = customs_departure
            details['planned_border_crossings'] = [f'Переход {vehicle}', 'Следующий переход']
        saved = (await api.post('/api/v1/transportations/update', {
            'transportation_id': created['id'], 'expected_revision': created['revision'],
            'type': 'container', 'hazardous': False,
            'route_segments': [{'id': stage['id'], 'mode': mode, field: details}],
        }))['transportation']
        assert saved['route_segments'][0][field] == details
        details_by_id[created['id']] = details

    stage['plan'] = {'destination_name': 'Казань', 'departure_date': '2030-01-09'}
    shipment_routes.set(shipment_id, [stage], revision=2)
    for transportation_id, details in details_by_id.items():
        fetched = (await api.post('/api/v1/transportations/get', {
            'transportation_id': transportation_id,
        }))['transportation']['route_segments'][0]
        assert fetched['plan'] == stage['plan']
        assert fetched[field] == details


async def test_create_with_route_saves_details_and_route_atomically(api, shipment_routes):
    shipment_id = str(uuid4())
    stage = route_segment('international_road')
    shipment_routes.set(shipment_id, [stage])
    transportation_id = str(uuid4())
    create_path = '/api/v1/transportations/create'
    body = {
        'id': transportation_id,
        'shipment_id': shipment_id,
        'type': 'container',
        'hazardous': False,
        'route_segments': [
            {**stage, 'international_road': {'cmr_received_date': '9999-01-01'}}
        ],
    }
    await api.post(create_path, body, expected=400)
    await api.post(
        '/api/v1/transportations/get',
        {'transportation_id': transportation_id},
        expected=404,
    )
    road_details = {'cmr_number': 'QA-CMR', 'cmr_received_date': '2020-01-01'}
    body['route_segments'][0]['international_road'] = road_details
    created = (await api.post(create_path, body))['transportation']
    assert created['revision'] == 1
    assert created['route_segments'][0]['international_road'] == road_details
    assert (await api.post(create_path, body))['transportation'] == created
    assert (await api.post(
        '/api/v1/transportations/get', {'transportation_id': transportation_id}
    ))['transportation'] == created
