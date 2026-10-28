"""Общие поля моря/авиа принадлежат поставке и используют канонические справочники."""

from uuid import uuid4
import pytest
from test_shipment_card import creation


@pytest.fixture
def locations(pgsql):
    values = [
        {
            'id': str(uuid4()),
            'kind': 'sea_port',
            'name': 'Порт отправления',
            'country_code': 'CN',
            'code': 'CNAAA',
        },
        {
            'id': str(uuid4()),
            'kind': 'sea_port',
            'name': 'Порт назначения',
            'country_code': 'RU',
            'code': 'RUBBB',
        },
        {
            'id': str(uuid4()),
            'kind': 'airport',
            'name': 'Аэропорт отправления',
            'country_code': 'CN',
            'code': 'AAA',
        },
        {
            'id': str(uuid4()),
            'kind': 'airport',
            'name': 'Аэропорт назначения',
            'country_code': 'RU',
            'code': 'BBB',
        },
    ]
    cursor = pgsql['db_1'].cursor()
    for item in values:
        cursor.execute(
            'INSERT INTO route_locations (id,kind,name,country_code,code) VALUES (%s,%s,%s,%s,%s)',
            tuple(item[key] for key in ('id', 'kind', 'name', 'country_code', 'code')),
        )
    return values


def stages(locations):
    return [
        {
            'id': str(uuid4()),
            'type': 'sea',
            'sea': {
                'pol': {**locations[0], 'name': 'Подменённое имя'},
                'pod': locations[1],
                'eta': '2030-01-01',
                'departure_date': '2020-01-01',
                'vessel': 'Судно',
                'voyage': 'Рейс 12',
                'departure_terminal_id': str(uuid4()),
                'arrival_terminal_id': str(uuid4()),
            },
        },
        {
            'id': str(uuid4()),
            'type': 'air',
            'air': {
                'origin_airport': locations[2],
                'destination_airport': locations[3],
                'carrier_id': str(uuid4()),
                'etd': '2030-01-01',
                'eta': '2030-01-03',
                'departure_date': '2020-01-01',
                'arrival_date': '2020-01-02',
                'flight_number': 'SU123',
            },
        },
    ]


async def test_shared_fields_roundtrip_and_patch(api, locations):
    route = stages(locations)
    created = (
        await api.post('/api/v1/shipments/create', creation(route_segments=route), expected=201)
    )['shipment']
    route[0]['sea']['pol'] = locations[0]
    assert created['route_segments'] == route
    selected = (await api.post('/api/v1/shipments/get', {'shipment_id': created['id']}))['shipment']
    assert selected == created
    contexts = await api.post(
        '/api/v1/shipments/transportation-contexts',
        {'shipment_ids': [created['id']]},
        roles=['transportations.read'],
    )
    assert contexts['items'][0]['route_segments'] == route
    patched = (
        await api.post(
            '/api/v1/shipments/update',
            {
                'shipment_id': created['id'],
                'expected_revision': 1,
                'reference': 'Изменение примечания',
            },
        )
    )['shipment']
    assert patched['route_segments'] == route
    route[0]['sea']['voyage'] = 'Рейс 13'
    route[1]['air']['eta'] = '2030-01-04'
    saved = (
        await api.post(
            '/api/v1/shipments/update',
            {
                'shipment_id': created['id'],
                'expected_revision': 2,
                'reference': '',
                'route_segments': route,
            },
        )
    )['shipment']
    assert saved['route_segments'] == route


@pytest.mark.parametrize(
    'change',
    [
        {'departure_date': '9999-01-01'},
        {'departure_date': '2020-02-30'},
        {'eta': '1111-01-01'},
    ],
)
async def test_invalid_sea_dates(api, locations, change):
    route = stages(locations)
    route[0]['sea'].update(change)
    await api.post('/api/v1/shipments/create', creation(route_segments=route), expected=400)


async def test_invalid_air_order_and_location_kind(api, locations):
    route = stages(locations)
    route[1]['air']['arrival_date'] = '2019-01-01'
    await api.post('/api/v1/shipments/create', creation(route_segments=route), expected=400)
    route = stages(locations)
    route[0]['sea']['pol'] = locations[2]
    await api.post('/api/v1/shipments/create', creation(route_segments=route), expected=400)
    route = stages(locations)
    route[0]['type'] = 'rail'
    await api.post('/api/v1/shipments/create', creation(route_segments=route), expected=400)


async def test_archived_location_retained_but_cannot_be_selected(api, locations, pgsql):
    route = stages(locations)
    created = (
        await api.post('/api/v1/shipments/create', creation(route_segments=route), expected=201)
    )['shipment']
    pgsql['db_1'].cursor().execute(
        'UPDATE route_locations SET archived=TRUE WHERE id=%s', (locations[0]['id'],)
    )
    saved = (
        await api.post(
            '/api/v1/shipments/update',
            {
                'shipment_id': created['id'],
                'expected_revision': 1,
                'reference': '',
                'route_segments': created['route_segments'],
            },
        )
    )['shipment']
    assert saved['route_segments'] == created['route_segments']
    await api.post('/api/v1/shipments/create', creation(route_segments=route), expected=400)




async def test_route_counterparty_must_belong_to_organization(api, locations, mockserver):
    @mockserver.json_handler('/api/v1/counterparties/get')
    def missing(request):
        return mockserver.make_response(
            status=404, json={'error': 'not_found', 'message': 'Не найден'}
        )

    await api.post(
        '/api/v1/shipments/create',
        creation(logist='', route_segments=stages(locations)),
        expected=400,
    )


async def test_route_only_update_preserves_main_fields(api, locations):
    created = (
        await api.post(
            '/api/v1/shipments/create',
            creation(
                reference='Основные данные',
                cargo_description='Груз',
                route_segments=stages(locations),
            ),
            expected=201,
        )
    )['shipment']
    route = created['route_segments']
    route[0]['sea']['voyage'] = 'Новый рейс'
    changed = (
        await api.post(
            '/api/v1/shipments/update',
            {'shipment_id': created['id'], 'expected_revision': 1, 'route_segments': route},
        )
    )['shipment']
    assert changed['reference'] == 'Основные данные'
    assert changed['cargo_description'] == 'Груз'
    assert changed['customer_id'] == created['customer_id']
    assert changed['route_segments'] == route
    details = (
        await api.post(
            '/api/v1/shipments/update',
            {'shipment_id': created['id'], 'expected_revision': 2, 'reference': 'Обновлено'},
        )
    )['shipment']
    assert details['route_segments'] == route


async def test_location_dependency_failure_does_not_create_shipment(api, locations, mockserver, pgsql):
    @mockserver.handler('/api/v1/route-locations/get-batch')
    def unavailable(request):
        return mockserver.make_response(status=503)
    result = await api.post('/api/v1/shipments/create', creation(route_segments=stages(locations)), expected=503)
    assert result['error'] == 'transport_locations_unavailable'
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM shipments')
        assert cursor.fetchone()[0] == 0
