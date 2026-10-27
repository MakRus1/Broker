"""Браузерные административные операции сохраняют тип справочника и коды."""

from uuid import uuid4
import pytest

ID = str(uuid4())
LOCATION = {'id': ID, 'kind': 'airport', 'name': 'Аэропорт', 'country_code': 'RU', 'code': 'TEST'}


@pytest.mark.parametrize(
    'operation,body,result',
    [
        ('list', {'kind': 'airport', 'query': 'Тест'}, {'items': [LOCATION]}),
        ('save', {**LOCATION, 'existing': True}, LOCATION),
        ('archive', {'id': ID}, {'id': ID}),
    ],
)
async def test_location_proxy(browser, mockserver, operation, body, result):
    await browser.sign_in()
    path = '/api/admin/route-locations/' + operation

    @mockserver.json_handler(path)
    def upstream(request):
        expected = {**body, 'sort': 'id', 'descending': False, 'limit': 50} if operation == 'list' else body
        assert request.json == expected
        assert request.headers['Authorization'].startswith('Bearer ')
        return result

    response = await browser.post(path, body)
    assert response.status == 200, response.text
    assert response.json() == result
