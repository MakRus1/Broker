"""Типизированные ЖД-детали сохраняются на границе браузерного BFF."""

from uuid import uuid4
from test_proxy import TRANSPORTATION


async def test_railway_station_search(browser, mockserver):
    await browser.sign_in()
    station = {'esr_code': '000001', 'name': 'Тестовая станция', 'country_code': 'RU'}

    @mockserver.json_handler('/api/v1/railway-stations/search')
    def search(request):
        assert request.json == {'query': '000001'}
        assert request.headers['Authorization'].startswith('Bearer ')
        return {'items': [station]}

    response = await browser.post('/api/geography/railway-stations', {'query': '000001'})
    assert response.status == 200, response.text
    assert response.json() == {'items': [station]}


async def test_rail_route_roundtrip(browser, mockserver):
    await browser.sign_in()
    stage = {
        'id': str(uuid4()),
        'mode': 'international_rail',
        'rail': {
            'origin_station': {'esr_code': '000001', 'name': 'Тестовая', 'country_code': 'RU'},
            'etd': '2020-01-01',
            'eta': '2020-01-05',
            'departure_date': '2020-01-02',
            'arrival_date': '2020-01-03',
            'unloading_date': '2020-01-04',
        },
    }
    body = {
        'transportation_id': TRANSPORTATION['id'],
        'expected_revision': 1,
        'route_segments': [stage],
    }

    @mockserver.json_handler('/api/v1/transportation-routes/set')
    def save(request):
        assert request.json == body
        return {'transportation': {**TRANSPORTATION, 'route_segments': [stage]}}

    response = await browser.post('/api/v1/transportation-routes/set', body)
    assert response.status == 200, response.text
    assert response.json()['transportation']['route_segments'][0]['rail'] == stage['rail']
