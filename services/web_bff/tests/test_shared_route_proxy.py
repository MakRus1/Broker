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
    stage = {
        'id': str(uuid4()),
        'type': 'air',
        'air': {'origin_airport': location, 'flight_number': 'SU123', 'eta': '2030-01-01'},
    }
    body = {
        'shipment_id': SHIPMENT['id'],
        'expected_revision': 1,
        'route_segments': [stage],
    }

    @mockserver.json_handler('/api/v1/shipments/update')
    def save(request):
        assert request.json == body
        return {'shipment': {**SHIPMENT, 'route_segments': [stage]}}

    response = await browser.post('/api/v1/shipments/update', body)
    assert response.status == 200, response.text
    assert response.json()['shipment']['route_segments'] == [stage]


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
        'sea': {'eta': '2030-01-01', 'vessel': 'Судно', 'voyage': 'Рейс 12'},
    }

    @mockserver.json_handler('/api/v1/transportations/get')
    def load(request):
        return {'transportation': {**TRANSPORTATION, 'route_segments': [stage]}}

    response = await browser.post(
        '/api/v1/transportations/get', {'transportation_id': TRANSPORTATION['id']}
    )
    assert response.status == 200, response.text
    assert response.json()['transportation']['route_segments'] == [stage]
