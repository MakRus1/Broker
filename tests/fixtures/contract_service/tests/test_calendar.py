"""Date кодгена сохраняет JSON wire-формат и проверяет календарные дни."""

import pytest


@pytest.mark.parametrize('day', ['1900-01-01', '2000-02-29', '2026-10-25', '2099-12-31'])
async def test_calendar_body_roundtrip(service_client, day):
    response = await service_client.post(
        '/calendar/echo',
        json={
            'day': day,
            'days': [day],
            'optional_day': None,
        },
    )
    assert response.status == 200
    assert response.json() == {'day': day, 'days': [day], 'optional_day': None}
    response = await service_client.get('/calendar/check', params={'day': day})
    assert response.status == 200
    assert response.json() == {'day': day}


async def test_calendar_defaults_and_optional_values(service_client):
    response = await service_client.post('/calendar/echo', json={})
    assert response.status == 200
    assert response.json() == {'day': '2026-10-25', 'days': []}
    response = await service_client.post('/calendar/echo', json={'optional_day': '2026-10-25'})
    assert response.status == 200
    assert response.json()['optional_day'] == '2026-10-25'
    response = await service_client.get('/calendar/check')
    assert response.status == 200
    assert response.json() == {'day': '2026-10-25'}


@pytest.mark.parametrize(
    'day',
    ['2025-02-29', '2026-04-31', '2026-13-01', '0000-01-01', '2026-10-25T00:00:00Z', 42, None],
)
async def test_invalid_calendar_body(service_client, day):
    response = await service_client.post('/calendar/echo', json={'day': day})
    assert response.status == 400
    response = await service_client.post('/calendar/echo', json={'days': [day]})
    assert response.status == 400


async def test_short_calendar_input_is_normalized(service_client):
    response = await service_client.post('/calendar/echo', json={'day': '2026-1-1'})
    assert response.status == 200
    assert response.json()['day'] == '2026-01-01'
    response = await service_client.get('/calendar/check', params={'day': '2026-1-1'})
    assert response.status == 200
    assert response.json() == {'day': '2026-01-01'}
