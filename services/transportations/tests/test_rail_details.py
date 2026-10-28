"""ЖД-детали принадлежат перевозке, справочник канонизирует станции."""

from uuid import uuid4
import pytest
from test_transportations import route_segment


@pytest.fixture
def stations(pgsql):
    pgsql['db_1'].cursor().execute("""
        INSERT INTO railway_stations (esr_code, name, country_code) VALUES
        ('000001', 'Тестовая станция отправления', 'RU'),
        ('000002', 'Тестовая станция назначения', 'RU')
    """)


async def create_rail(api, shipment_routes, mode='rail'):
    shipment = str(uuid4())
    stage = route_segment(mode)
    shipment_routes.set(shipment, [stage])
    created = await api.post(
        '/api/v1/transportations/create',
        {
            'id': str(uuid4()),
            'shipment_id': shipment,
            'type': 'rail',
            'hazardous': False,
        },
    )
    return created['transportation'], stage


def details():
    return {
        'origin_station': {'id': '34996a7f-8d2d-1d6c-d1ec-75950190cc6d', 'esr_code': '000001', 'name': 'Чужое название', 'country_code': 'CN'},
        'destination_station': {
            'id': '3e13ac6a-1d4d-9602-deaa-922533ff8fe2', 'esr_code': '000002',
            'name': 'Чужое название',
            'country_code': 'CN',
        },
        'etd': '2020-01-01',
        'eta': '2020-01-04',
        'departure_date': '2020-01-02',
        'arrival_date': '2020-01-03',
        'unloading_date': '2020-01-04',
    }


@pytest.mark.parametrize('mode', ['rail', 'international_rail'])
async def test_rail_roundtrip_and_directory(api, shipment_routes, stations, pgsql, mode):
    transportation, stage = await create_rail(api, shipment_routes, mode)
    saved = (
        await api.post(
            '/api/v1/transportation-routes/set',
            {
                'transportation_id': transportation['id'],
                'expected_revision': 1,
                'route_segments': [{**stage, 'rail': details()}],
            },
        )
    )['transportation']
    rail = saved['route_segments'][0]['rail']
    assert rail['origin_station'] == {
        'id': '34996a7f-8d2d-1d6c-d1ec-75950190cc6d', 'esr_code': '000001',
        'name': 'Тестовая станция отправления',
        'country_code': 'RU',
    }
    assert rail['unloading_date'] == '2020-01-04'
    assert 'origin' not in saved['route_segments'][0]
    assert 'destination' not in saved['route_segments'][0]
    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        'SELECT document FROM route_segments WHERE transportation_id = %s', (transportation['id'],)
    )
    stored = cursor.fetchone()[0]
    assert 'origin' not in stored and 'destination' not in stored
    fetched = (
        await api.post('/api/v1/transportations/get', {'transportation_id': transportation['id']})
    )['transportation']
    assert fetched == saved
    assert shipment_routes.registrations
    history = await api.post(
        '/api/v1/transportations/history', {'transportation_id': transportation['id']}
    )
    assert history['items'][0]['route_segments'][0]['rail'] == rail




@pytest.mark.parametrize(
    'change',
    [
        {'eta': '2019-12-31'},
        {'departure_date': '9999-01-01'},
        {'arrival_date': '2020-01-01'},
        {'unloading_date': '2020-01-02'},
        {'etd': '1111-01-01'},
        {'eta': '2020-02-30'},
        {'origin_station': {'id': 'fc7c3eef-99bb-23e0-fbd8-462cd2389687', 'esr_code': '999999', 'name': 'Неизвестная', 'country_code': 'RU'}},
    ],
)
async def test_rejected_details_do_not_change_revision(api, shipment_routes, stations, change):
    transportation, stage = await create_rail(api, shipment_routes)
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': transportation['id'],
            'expected_revision': 1,
            'route_segments': [{**stage, 'rail': {**details(), **change}}],
        },
        expected=400,
    )
    fetched = (
        await api.post('/api/v1/transportations/get', {'transportation_id': transportation['id']})
    )['transportation']
    assert fetched['revision'] == 1
    assert 'rail' not in fetched['route_segments'][0]


async def test_rail_details_reject_non_rail_stage(api, shipment_routes, stations):
    transportation, stage = await create_rail(api, shipment_routes, 'sea')
    await api.post(
        '/api/v1/transportation-routes/set',
        {
            'transportation_id': transportation['id'],
            'expected_revision': 1,
            'route_segments': [{**stage, 'rail': details()}],
        },
        expected=400,
    )


async def test_archiving_keeps_existing_rail_details(api, pgsql, shipment_routes, stations):
    transportation, stage = await create_rail(api, shipment_routes)
    saved = (
        await api.post(
            '/api/v1/transportation-routes/set',
            {
                'transportation_id': transportation['id'],
                'expected_revision': 1,
                'route_segments': [{**stage, 'rail': details()}],
            },
        )
    )['transportation']
    pgsql['db_1'].cursor().execute("UPDATE railway_stations SET archived = TRUE WHERE esr_code = '000001'")
    unchanged = (
        await api.post(
            '/api/v1/transportation-routes/set',
            {
                'transportation_id': transportation['id'],
                'expected_revision': saved['revision'],
                'route_segments': [{**stage, 'rail': saved['route_segments'][0]['rail']}],
            },
        )
    )['transportation']
    assert unchanged['route_segments'][0]['rail'] == saved['route_segments'][0]['rail']


async def test_station_dependency_failure_does_not_change_route(api, shipment_routes, mockserver):
    transportation, stage = await create_rail(api, shipment_routes)
    @mockserver.handler('/api/v1/railway-stations/get-batch')
    def unavailable(request):
        return mockserver.make_response(status=503)
    result = await api.post('/api/v1/transportation-routes/set', {
        'transportation_id': transportation['id'], 'expected_revision': 1,
        'route_segments': [{**stage, 'rail': details()}],
    }, expected=503)
    assert result['error'] == 'transport_locations_unavailable'
    current = await api.post('/api/v1/transportations/get', {'transportation_id': transportation['id']})
    assert current['transportation']['revision'] == 1
