"""Сгенерированные маршруты сохраняют контракты, а проверки выполняются до вызова Configs."""

import json

import pytest

DRAFT = {
    'id': 7,
    'config_name': 'EXAMPLE',
    'author': 'issuer#author',
    'author_name': 'Автор',
    'description': 'Изменение',
    'version': 1,
    'status': 'draft',
    'base_version': 0,
    'base_value': {'enabled': False},
    'value': {'enabled': True},
    'source_revision': 0,
}
ROUTES = [
    pytest.param(
        'GET', 'list', None,
        {'items': [{'config_name': 'EXAMPLE', 'version': 0, 'active_draft_id': None}]},
        200, id='catalog',
    ),
    pytest.param(
        'GET', 'history', None,
        {'items': [{'revision': 3, 'created_at': '2026-10-19T00:00:00+00:00'}]},
        200, id='history',
    ),
    pytest.param(
        'POST', 'current', {'config_name': 'EXAMPLE'},
        {'config_name': 'EXAMPLE', 'value': {}, 'version': 0},
        200, id='current',
    ),
    pytest.param(
        'POST', 'drafts/create',
        {
            'config_name': 'EXAMPLE',
            'description': 'Изменение',
            'expected_config_version': 0,
            'value': {},
        },
        DRAFT, 201, id='create',
    ),
    pytest.param(
        'POST', 'drafts/edit', {'id': 7, 'expected_version': 1, 'value': {}},
        DRAFT, 200, id='edit',
    ),
    pytest.param(
        'POST', 'drafts/approve', {'id': 7, 'expected_version': 1},
        {
            **DRAFT,
            'status': 'published',
            'approved_by': 'issuer#reviewer',
            'approved_by_name': 'Ревьюер',
            'published_revision': 3,
        },
        200, id='approve',
    ),
    pytest.param(
        'POST', 'drafts/cancel', {'id': 7, 'expected_version': 1},
        {**DRAFT, 'status': 'cancelled'}, 200, id='cancel',
    ),
    pytest.param('POST', 'drafts/get', {'id': 7}, DRAFT, 200, id='get'),
    pytest.param(
        'POST', 'drafts/list', {'config_name': 'EXAMPLE'},
        {'items': [DRAFT]}, 200, id='drafts',
    ),
    pytest.param(
        'POST', 'drafts/events', {'id': 7},
        {
            'items': [{
                'action': 'create',
                'actor': 'issuer#author',
                'created_at': '2026-10-19T00:00:00+00:00',
                'snapshot': {key: value for key, value in DRAFT.items() if key != 'id'},
            }],
        },
        200, id='events',
    ),
]
POST_PATHS = [case.values[1] for case in ROUTES if case.values[0] == 'POST']


@pytest.mark.parametrize('method,path,body,payload,status', ROUTES)
async def test_generated_forwarding(browser, mockserver, method, path, body, payload, status):
    await browser.sign_in()

    @mockserver.json_handler('/admin/configs/' + path)
    def upstream(request):
        assert request.method == method
        assert request.headers['Authorization'].startswith('Bearer ')
        assert 'Cookie' not in request.headers
        assert 'X-CSRF-Token' not in request.headers
        if body is not None:
            assert json.loads(request.get_data()) == body
        return mockserver.make_response(status=status, json=payload)

    url = '/api/configs/' + path
    response = await browser.get(url) if method == 'GET' else await browser.post(url, body)
    assert response.status == status, response.text
    assert response.json() == payload
    assert upstream.times_called == 1
    assert response.headers['Cache-Control'] == 'no-store'


@pytest.mark.parametrize('method,path,body,payload,status', ROUTES)
async def test_authentication_precedes_validation(service_client, method, path, body, payload, status):
    url = '/api/configs/' + path
    if method == 'GET':
        response = await service_client.get(url)
    else:
        response = await service_client.post(url, json={'forged': True})
    assert response.status == 401
    assert response.json()['error'] == 'unauthorized'


@pytest.mark.parametrize('path', POST_PATHS)
async def test_csrf_precedes_validation(browser, path):
    await browser.sign_in()
    response = await browser.post('/api/configs/' + path, {'forged': True}, {'X-CSRF-Token': 'wrong'})
    assert response.status == 403
    assert response.json()['error'] == 'forbidden'


@pytest.mark.parametrize('status,body', [(418, {'error': 'unknown'}), (200, {'invalid': True})])
async def test_upstream_contract_errors(browser, mockserver, status, body):
    await browser.sign_in()

    @mockserver.json_handler('/admin/configs/current')
    def upstream(request):
        return mockserver.make_response(status=status, json=body)

    response = await browser.post('/api/configs/current', {'config_name': 'EXAMPLE'})
    assert response.status == 502
    assert response.json()['error'] == 'invalid_upstream'


@pytest.mark.parametrize('method,path', [
    ('POST', '/api/configs/list'),
    ('GET', '/api/configs/current'),
    ('GET', '/api/logout'),
])
async def test_wrong_method_is_not_a_static_asset(browser, method, path):
    await browser.sign_in()
    response = await browser.get(path) if method == 'GET' else await browser.post(path, {})
    assert response.status == 405
