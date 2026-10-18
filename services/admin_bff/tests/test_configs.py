"""BFF пользуется типизированным клиентом и сохраняет ошибки Configs."""
import pytest


async def test_catalog(browser, mockserver):
    await browser.sign_in()

    @mockserver.json_handler('/admin/configs/list')
    def catalog(request):
        assert request.headers['Authorization'].startswith('Bearer ')
        assert 'broker_admin' not in request.headers.get('Cookie', '')
        return {'items': [{'config_name': 'EXAMPLE', 'version': 0, 'active_draft_id': None}]}

    response = await browser.get('/api/configs/list')
    assert response.status == 200
    assert response.json()['items'][0]['config_name'] == 'EXAMPLE'


@pytest.mark.parametrize('status', [400, 401, 403, 404, 409, 503])
async def test_upstream_errors(browser, mockserver, status):
    await browser.sign_in()

    @mockserver.json_handler('/admin/configs/current')
    def current(request):
        return mockserver.make_response(status=status, json={'error': 'example', 'message': 'Конфликт'})

    response = await browser.post('/api/configs/current', {'config_name': 'EXAMPLE'})
    assert response.status == status
    assert response.json()['message'] == 'Конфликт'


async def test_request_validation(browser, mockserver):
    await browser.sign_in()
    assert (await browser.post('/api/configs/current', {'actor': 'fake'})).status == 400
    assert (await browser.post('/api/configs/current', {'config_name': 5})).status == 400


async def test_no_arbitrary_proxy(browser):
    await browser.sign_in()
    assert (await browser.get('/api/configs/unknown')).status == 404
    assert (await browser.post('/api/configs/values', {})).status == 404
