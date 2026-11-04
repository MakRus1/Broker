async def test_catalog_proxy_keeps_organization_and_locale(browser, mockserver):
    await browser.sign_in()
    body = {
        'organization_id': '11111111-1111-4111-8111-111111111111',
        'locale': 'en',
    }

    @mockserver.json_handler('/api/v1/localization/catalog')
    def catalog(request):
        assert request.json == body
        assert request.headers['Authorization'].startswith('Bearer ')
        return {'messages': [{'key': 'nav.shipments', 'value': 'Bookings'}]}

    response = await browser.post('/api/localization/catalog', body)
    assert response.status == 200
    assert response.json() == {'messages': [{'key': 'nav.shipments', 'value': 'Bookings'}]}
    assert catalog.times_called == 1
