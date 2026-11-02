async def test_preferences_proxy_keeps_organization_and_revision(browser, mockserver):
    await browser.sign_in()
    body = {
        'organization_id': '11111111-1111-4111-8111-111111111111',
        'key': 'shipments:columns:v1',
        'value': {'value': ['number']},
        'expected_revision': 3,
    }

    @mockserver.json_handler('/api/v1/preferences/save')
    def save(request):
        assert request.json == body
        assert request.headers['Authorization'].startswith('Bearer ')
        return {'preference': {'key': body['key'], 'value': body['value'], 'revision': 4}}

    response = await browser.post('/api/preferences/save', body)
    assert response.status == 200
    assert response.json()['preference']['revision'] == 4
    assert save.times_called == 1
