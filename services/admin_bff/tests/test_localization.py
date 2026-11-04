import pytest

CATALOG = {'ru': [{'key': 'nav.shipments', 'value': 'Букинги'}], 'en': []}
STATE = {'version': 1, 'revision': 0, 'published': {'ru': [], 'en': []}, 'inherited': {'ru': [], 'en': []}, 'draft': CATALOG, 'source': CATALOG}


@pytest.mark.parametrize('action,body', [('get', {}), ('save', {'version': 0, 'catalog': CATALOG}), ('publish', {'version': 1})])
async def test_localization_proxy(browser, mockserver, action, body):
    await browser.sign_in()
    path = '/api/admin/localization/' + action

    @mockserver.json_handler(path)
    def upstream(request):
        assert request.json == body
        assert request.headers['Authorization'].startswith('Bearer ')
        return STATE

    response = await browser.post(path, body)
    assert response.status == 200, response.text
    assert response.json() == STATE
