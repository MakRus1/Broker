"""Общий поиск городов: международные подсказки и ручной ввод при сбоях."""

import pytest


async def test_city_lookup(browser, mockserver):
    await browser.sign_in()

    @mockserver.json_handler('/dadata/cities')
    def cities(request):
        assert request.headers['Authorization'] == 'Token testsuite-key'
        assert request.json['from_bound'] == {'value': 'city'}
        assert request.json['locations'] == [{'country': '*'}]
        return {
            'suggestions': [
                {
                    'value': 'Китай, Шанхай',
                    'data': {
                        'city': 'Шанхай',
                        'country': 'Китай',
                        'country_iso_code': 'CN',
                    },
                }
            ]
        }

    response = await browser.post('/api/geography/cities', {'query': 'Шанх'})
    assert response.status == 200, response.text
    assert response.json()['items'] == [
        {'name': 'Шанхай', 'country': 'Китай', 'country_code': 'CN', 'label': 'Китай, Шанхай'}
    ]


async def test_quota_exceeded_returns_safe_fallback_message(browser, mockserver):
    await browser.sign_in()

    @mockserver.handler('/dadata/cities')
    def cities(request):
        return mockserver.make_response(
            status=403, json={'error': 'Quota exceeded with secret provider details'}
        )

    response = await browser.post('/api/geography/cities', {'query': 'Москва'})
    assert response.status == 503
    assert response.json()['error'] == 'cities_unavailable'
    assert 'вручную' in response.json()['message']
    assert 'secret' not in response.text


async def test_city_lookup_requires_session(service_client):
    response = await service_client.post('/api/geography/cities', json={'query': 'Москва'})
    assert response.status == 401


@pytest.mark.parametrize('document', [
    'provider-secret: not json',
    '{}',
    '{"suggestions": null}',
    '{"suggestions": "provider-secret"}',
    '{"suggestions": [' + ','.join(['{}'] * 11) + ']}',
    '{"suggestions": [{"data": {"city": 123}}]}',
])
async def test_malformed_city_response_has_safe_fallback(browser, mockserver, document):
    await browser.sign_in()

    @mockserver.handler('/dadata/cities')
    def cities(request):
        return mockserver.make_response(response=document, status=200)

    response = await browser.post('/api/geography/cities', {'query': 'Москва'})
    assert response.status == 503
    assert response.json()['error'] == 'cities_unavailable'
    assert 'вручную' in response.json()['message']
    assert 'provider-secret' not in response.text
    assert cities.times_called == 1


async def test_city_lookup_skips_incomplete_suggestions(browser, mockserver):
    await browser.sign_in()

    @mockserver.json_handler('/dadata/cities')
    def cities(request):
        return {'suggestions': [
            {'data': {'city': None, 'country_iso_code': 'CN'}},
            {'data': {'city': 'City', 'country_iso_code': None}},
            {'data': {'city': 'City', 'country_iso_code': 'CHN'}},
            {'value': 'Китай, Шанхай', 'data': {
                'city': 'Шанхай', 'country': 'Китай', 'country_iso_code': 'CN',
            }},
        ]}

    response = await browser.post('/api/geography/cities', {'query': 'Шанх'})
    assert response.status == 200
    assert response.json()['items'] == [
        {'name': 'Шанхай', 'country': 'Китай', 'country_code': 'CN', 'label': 'Китай, Шанхай'},
    ]
