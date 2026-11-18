from uuid import uuid4


async def test_calculation_reader_cannot_read_service_charges(api):
    roles = ['finance.read_calculation_settings']
    settings = await api.post('/api/v1/finance/settings/get', {}, roles=roles)
    assert settings['reporting_currency'] == 'RUB'
    rate = await api.post('/api/v1/exchange-rates/get', {
        'date': '2026-11-18', 'from_currency': 'RUB', 'to_currency': 'RUB',
    }, roles=roles)
    assert rate['rate_millionths'] == 1000000
    await api.post('/api/v1/service-charges/list', {
        'scope': {'shipment_id': str(uuid4()), 'transportation_id': None},
        'after': None, 'limit': 50, 'include_archived': False,
    }, roles=roles, expected=403)


async def test_calculation_settings_require_explicit_permission(api):
    for path in ('/api/v1/finance/settings/get', '/api/v1/exchange-rates/get'):
        body = {} if 'settings' in path else {
            'date': '2026-11-18', 'from_currency': 'RUB', 'to_currency': 'RUB',
        }
        await api.post(path, body, roles=['sales.read'], expected=403)
