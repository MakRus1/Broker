"""Административный BFF сохраняет типизированный контракт общего справочника."""

import pytest

STATION = {'id': '34996a7f-8d2d-1d6c-d1ec-75950190cc6d', 'esr_code': '000001', 'name': 'Общая станция', 'country_code': 'KZ'}


@pytest.mark.parametrize(
    'operation,body,result',
    [
        ('list', {'query': 'Общая'}, {'items': [STATION], 'next': '34996a7f-8d2d-1d6c-d1ec-75950190cc6d'}),
        ('save', {**STATION, 'existing': True}, STATION),
        ('archive', {'id': '34996a7f-8d2d-1d6c-d1ec-75950190cc6d'}, {'id': '34996a7f-8d2d-1d6c-d1ec-75950190cc6d'}),
    ],
)
async def test_station_proxy(browser, mockserver, operation, body, result):
    await browser.sign_in()
    path = '/api/admin/railway-stations/' + operation

    @mockserver.json_handler(path)
    def upstream(request):
        expected = {**body, 'sort': 'name', 'descending': False, 'limit': 50} if operation == 'list' else body
        assert request.json == expected
        assert request.headers['Authorization'].startswith('Bearer ')
        return result

    response = await browser.post(path, body)
    assert response.status == 200, response.text
    assert response.json() == result
