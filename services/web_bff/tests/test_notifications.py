async def test_notifications_proxy_uses_current_browser_identity(browser, mockserver):
    await browser.sign_in()
    organization = '11111111-1111-4111-8111-111111111111'
    notification = {
        'id': 'aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',
        'shipment_id': 'bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb',
        'shipment_number': 'S-1042', 'shipment_revision': 3,
        'created_at': '2026-11-05T12:00:00Z', 'read': False,
    }

    @mockserver.json_handler('/api/v1/notifications/list')
    def listing(request):
        assert request.json['organization_id'] == organization
        assert request.headers['Authorization'].startswith('Bearer ')
        assert 'recipient_subject' not in request.json
        return {'items': [notification]}

    @mockserver.json_handler('/api/v1/notifications/read')
    def read(request):
        assert request.json == {'organization_id': organization,
            'ids': [notification['id']], 'dismiss': True}
        assert request.headers['Authorization'].startswith('Bearer ')
        return {'updated': 1}

    result = await browser.post('/api/notifications/list', {'organization_id': organization})
    assert result.status == 200
    assert result.json()['items'][0]['id'] == notification['id']
    result = await browser.post('/api/notifications/read', {'organization_id': organization,
        'ids': [notification['id']], 'dismiss': True})
    assert result.status == 200 and result.json() == {'updated': 1}
