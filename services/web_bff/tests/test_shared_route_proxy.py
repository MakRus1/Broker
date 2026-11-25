"""Морские и авиационные поля проходят BFF без потери данных."""

from uuid import uuid4
from test_proxy import SHIPMENT


async def test_shipment_shared_details(browser, mockserver):
    await browser.sign_in()
    location = {
        'id': str(uuid4()),
        'kind': 'airport',
        'name': 'Аэропорт',
        'country_code': 'RU',
        'code': 'TEST',
    }
    air = {
        'id': str(uuid4()),
        'type': 'air',
        'air': {
            'origin_airport': location,
            'flight_number': 'SU123',
            'eta': '2030-01-01',
            'documents': [{'number': '555-12345678', 'date': '2024-10-01'}],
        },
    }
    sea = {
        'id': str(uuid4()),
        'type': 'sea',
        'sea': {
            'booking': {'status': 'confirmed', 'number': 'BK-42', 'placed_date': '2024-10-01'},
            'documents': [{'number': 'BL-42', 'leg': 'feeder'}],
            'original_received': {},
            'telex_release': {'date': '2024-10-02'},
        },
    }
    route = [sea, air]
    body = {
        'shipment_id': SHIPMENT['id'],
        'expected_revision': 1,
        'route_segments': route,
    }

    @mockserver.json_handler('/api/v1/shipments/update')
    def save(request):
        assert request.json == body
        return {'shipment': {**SHIPMENT, 'route_segments': route}}

    response = await browser.post('/api/v1/shipments/update', body)
    assert response.status == 200, response.text
    assert response.json()['shipment']['route_segments'] == route


async def test_location_search(browser, mockserver):
    await browser.sign_in()
    location = {
        'id': str(uuid4()),
        'kind': 'sea_port',
        'name': 'Порт',
        'country_code': 'RU',
        'code': 'TEST',
    }

    @mockserver.json_handler('/api/v1/route-locations/search')
    def search(request):
        assert request.json == {'kind': 'sea_port', 'query': 'Порт'}
        return {'items': [location]}

    response = await browser.post(
        '/api/geography/route-locations', {'kind': 'sea_port', 'query': 'Порт'}
    )
    assert response.status == 200, response.text
    assert response.json() == {'items': [location]}


async def test_transportation_receives_shared_fields(browser, mockserver):
    from test_proxy import TRANSPORTATION

    await browser.sign_in()
    stage = {
        'id': str(uuid4()),
        'mode': 'sea',
        'sea': {
            'eta': '2030-01-01',
            'booking': {'status': 'confirmed'},
            'documents': [{'leg': 'ocean', 'number': 'DOC-42'}],
            'original_received': {},
        },
    }

    @mockserver.json_handler('/api/v1/transportations/get')
    def load(request):
        return {'transportation': {**TRANSPORTATION, 'route_segments': [stage]}}

    response = await browser.post(
        '/api/v1/transportations/get', {'transportation_id': TRANSPORTATION['id']}
    )
    assert response.status == 200, response.text
    assert response.json()['transportation']['route_segments'] == [stage]


async def test_shipment_plan_survives_save_and_response(browser, mockserver):
    await browser.sign_in()
    plan = {'origin_name': 'Роттердам', 'origin_country': 'NL', 'origin_address': 'Терминал',
            'destination_name': 'Москва', 'destination_country': 'RU', 'destination_address': 'Склад',
            'departure_date': '2030-01-10', 'arrival_date': '2030-01-15'}
    stage = {'id': str(uuid4()), 'type': 'international_road', 'plan': plan}
    body = {'shipment_id': SHIPMENT['id'], 'expected_revision': 1, 'route_segments': [stage]}

    @mockserver.json_handler('/api/v1/shipments/update')
    def save(request):
        assert request.json == body
        return {'shipment': {**SHIPMENT, 'route_segments': [stage]}}

    response = await browser.post('/api/v1/shipments/update', body)
    assert response.status == 200, response.text
    assert response.json()['shipment']['route_segments'] == [stage]


async def test_transportation_receives_source_plan(browser, mockserver):
    from test_proxy import TRANSPORTATION
    await browser.sign_in()
    stage = {'id': str(uuid4()), 'mode': 'pickup', 'plan': {
        'origin_name': 'Порт', 'destination_name': 'Склад', 'arrival_date': '2030-01-15'},
        'pickup': {'driver': 'Тестовый водитель'}}

    @mockserver.json_handler('/api/v1/transportations/get')
    def load(request):
        return {'transportation': {**TRANSPORTATION, 'route_segments': [stage]}}

    response = await browser.post('/api/v1/transportations/get', {'transportation_id': TRANSPORTATION['id']})
    assert response.status == 200, response.text
    assert response.json()['transportation']['route_segments'] == [stage]


async def test_transportation_details_and_route_saved_together(browser, mockserver):
    from test_proxy import TRANSPORTATION

    await browser.sign_in()
    stage = {
        'id': str(uuid4()), 'mode': 'international_road',
        'international_road': {
            'cmr_number': 'CMR-42',
            'cmr_received_date': '2020-01-04',
            'loading_address': 'Склад отправителя',
            'unloading_terminal_id': str(uuid4()),
            'unloading_address': 'Склад получателя',
            'customs_terminal_id': str(uuid4()),
            'customs_arrival_date': '2020-01-02',
            'customs_departure_date': '2020-01-03',
            'planned_border_crossings': ['Кукурыки — Козловичи', 'Красная Горка'],
        },
    }
    sea = {'id': str(uuid4()), 'mode': 'sea', 'sea_unloading_date': '2020-01-02'}
    cleared = {'id': str(uuid4()), 'mode': 'sea'}
    body = {
        'transportation_id': TRANSPORTATION['id'], 'expected_revision': 1,
        'type': 'container', 'hazardous': True, 'container_number': 'CSQU3054383',
        'route_segments': [stage, sea, {**cleared, 'sea_unloading_date': None}],
    }

    @mockserver.json_handler('/api/v1/transportations/update')
    def save(request):
        assert request.json == body
        return {'transportation': {
            **TRANSPORTATION, 'revision': 2, 'container_number': 'CSQU3054383',
            'hazardous': True, 'route_segments': [stage, sea, cleared],
        }}

    response = await browser.post('/api/v1/transportations/update', body)
    assert response.status == 200, response.text
    assert save.times_called == 1
    assert response.json()['transportation']['route_segments'] == [stage, sea, cleared]
    assert response.json()['transportation']['container_number'] == 'CSQU3054383'


async def test_create_transportation_forwards_route(browser, mockserver):
    from test_proxy import TRANSPORTATION

    await browser.sign_in()
    stage = {
        'id': str(uuid4()),
        'mode': 'international_road',
        'international_road': {'cmr_number': 'CMR-42', 'cmr_received_date': '2020-01-04'},
    }
    body = {
        'id': TRANSPORTATION['id'],
        'shipment_id': TRANSPORTATION['shipment_id'],
        'type': 'container',
        'hazardous': False,
        'route_segments': [stage],
    }

    @mockserver.json_handler('/api/v1/transportations/create')
    def create(request):
        assert request.json == body
        return {'transportation': {**TRANSPORTATION, 'route_segments': [stage]}}

    response = await browser.post('/api/v1/transportations/create', body)
    assert response.status == 200, response.text
    assert create.times_called == 1
    assert response.json()['transportation']['route_segments'] == [stage]
