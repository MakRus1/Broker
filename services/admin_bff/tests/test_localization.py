import pytest

CATALOG = {
    'ru': [{'key': 'nav.shipments', 'value': 'Букинги'}, {'key': 'count', 'value': 'Найдено: {count}'}],
    'en': [{'key': 'nav.shipments', 'value': 'Bookings'}],
}
STATE = {
    'version': 3, 'revision': 2,
    'published': {'ru': [], 'en': [{'key': 'old', 'value': 'Previous'}]},
    'inherited': {'ru': [{'key': 'inherited', 'value': 'Общий текст'}], 'en': []},
    'draft': CATALOG, 'source': CATALOG,
}
ORG = '11111111-1111-4111-8111-111111111111'


@pytest.mark.parametrize('organization', [None, ORG])
@pytest.mark.parametrize('draft', [False, True])
@pytest.mark.parametrize('action,body', [
    ('get', {}),
    ('save', {'version': 2, 'catalog': CATALOG}),
    ('publish', {'version': 3}),
])
async def test_localization_proxy(browser, mockserver, action, body, organization, draft):
    await browser.sign_in()
    path = '/api/admin/localization/' + action
    body = {**body, **({'organization_id': organization} if organization else {})}
    state = {key: value for key, value in STATE.items() if draft or key != 'draft'}

    @mockserver.json_handler(path)
    def upstream(request):
        assert request.json == body
        assert request.headers['Authorization'].startswith('Bearer ')
        assert 'Cookie' not in request.headers
        assert 'X-CSRF-Token' not in request.headers
        return state

    response = await browser.post(path, body)
    assert response.status == 200, response.text
    assert response.json() == state
    assert upstream.times_called == 1
