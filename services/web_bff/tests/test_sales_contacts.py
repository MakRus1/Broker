async def test_sales_contacts_proxy_preserves_outcome_and_card(browser, mockserver):
    await browser.sign_in()
    card = {'id': '44444444-4444-4444-8444-444444444444',
        'organization_id': '11111111-1111-4111-8111-111111111111',
        'customer_id': '33333333-3333-4333-8333-333333333333',
        'manager_subject': 'author', 'stage_id': '55555555-5555-4555-8555-555555555555',
        'revision': 2, 'next_action': 'Отправить предложение', 'next_action_date': '2026-11-10'}
    contact = {'revision': 1, 'id': '66666666-6666-4666-8666-666666666666', 'card_id': card['id'],
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

async def test_contact_update_and_history_proxy(browser, mockserver):
    await browser.sign_in()
    contact = {'id': '66666666-6666-4666-8666-666666666666',
        'card_id': '44444444-4444-4444-8444-444444444444', 'revision': 2,
        'kind': 'email', 'contact_date': '2026-11-08', 'result': 'Исправлено',
        'author_subject': 'author', 'author_issuer': 'https://issuer.example',
        'created_at': '2026-11-08T12:00:00+00:00'}
    @mockserver.json_handler('/api/v1/sales/contacts/update')
    def update(request):
        assert request.headers['Authorization'].startswith('Bearer ')
        assert request.json['expected_revision'] == 1
        return {'item': contact}
    @mockserver.json_handler('/api/v1/sales/contacts/history')
    def history(request):
        assert request.json['id'] == contact['id']
        return {'items': [{'revision': 2, 'actor': 'https://issuer.example#author',
            'created_at': contact['created_at'], 'snapshot': contact}]}
    response = await browser.post('/api/v1/sales/contacts/update', {
        'id': contact['id'], 'card_id': contact['card_id'], 'expected_revision': 1,
        'kind': 'email', 'contact_date': '2026-11-08', 'result': 'Исправлено'})
    assert response.status == 200
    assert response.json()['item'] == contact
    response = await browser.post('/api/v1/sales/contacts/history', {'id': contact['id']})
    assert response.status == 200
    assert response.json()['items'][0]['snapshot'] == contact
