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


@pytest.mark.parametrize('remove', [False, True])
async def test_create_and_delete_draft_keep_nullable_values(browser, mockserver, remove):
    await browser.sign_in()
    body = {'config_name': 'CUSTOM_SETTINGS', 'description': 'Проверка драфта',
            'expected_config_version': 2 if remove else 0}
    body.update({'remove': True} if remove else {'value': {}})

    @mockserver.json_handler('/admin/configs/drafts/create')
    def create(request):
        assert request.json == body
        assert request.headers['Authorization'].startswith('Bearer ')
        return mockserver.make_response(status=201, json={
            'id': 1, 'config_name': body['config_name'], 'author': 'issuer#author',
            'description': body['description'], 'version': 1, 'status': 'draft',
            'base_version': body['expected_config_version'],
            'base_value': {'enabled': True} if remove else None,
            'value': None if remove else {},
        })

    response = await browser.post('/api/configs/drafts/create', body)
    assert response.status == 201
    assert response.json()['value'] == (None if remove else {})
    assert response.json()['base_value'] == ({'enabled': True} if remove else None)


async def test_unpublished_current_value_is_null(browser, mockserver):
    await browser.sign_in()

    @mockserver.json_handler('/admin/configs/current')
    def current(request):
        return {'config_name': 'CUSTOM_SETTINGS', 'version': 0, 'value': None,
                'active_draft_id': 1}

    response = await browser.post('/api/configs/current', {'config_name': 'CUSTOM_SETTINGS'})
    assert response.status == 200
    assert response.json()['value'] is None
