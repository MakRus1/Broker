async def test_sales_contacts_proxy_preserves_outcome_and_card(browser, mockserver):
    await browser.sign_in()
    card = {'id': '44444444-4444-4444-8444-444444444444',
        'organization_id': '11111111-1111-4111-8111-111111111111',
        'customer_id': '33333333-3333-4333-8333-333333333333',
        'manager_subject': 'author', 'stage_id': '55555555-5555-4555-8555-555555555555',
        'revision': 2, 'next_action': 'Отправить предложение', 'next_action_date': '2026-11-10'}
    contact = {'id': '66666666-6666-4666-8666-666666666666', 'card_id': card['id'],
        'kind': 'call', 'contact_date': '2026-11-08', 'result': 'Обсудили сроки',
        'author_subject': 'author', 'author_issuer': 'https://issuer.example',
        'created_at': '2026-11-08T12:00:00+00:00'}
    @mockserver.json_handler('/api/v1/sales/contacts/add')
    def add(request):
        assert request.headers['Authorization'].startswith('Bearer ')
        assert request.json['expected_revision'] == 1
        assert request.json['card_id'] == card['id']
        return {'card': card, 'item': contact}
    @mockserver.json_handler('/api/v1/sales/contacts/list')
    def listing(request):
        assert request.headers['Authorization'].startswith('Bearer ')
        assert request.json['card_id'] == card['id']
        return {'items': [contact]}
    response = await browser.post('/api/v1/sales/contacts/add', {
        'id': contact['id'], 'card_id': card['id'], 'expected_revision': 1,
        'kind': 'call', 'contact_date': contact['contact_date'], 'result': contact['result']})
    assert response.status == 200
    assert response.json()['card']['next_action'] == card['next_action']
    response = await browser.post('/api/v1/sales/contacts/list', {'card_id': card['id']})
    assert response.status == 200
    assert response.json()['items'] == [contact]
