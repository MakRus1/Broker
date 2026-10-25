"""Date кодгена сохраняет JSON wire-формат и проверяет календарные дни."""

import pytest


@pytest.mark.parametrize('day', ['0001-01-01', '1111-11-11', '2263-01-01', '9999-12-31', '1900-01-01', '2000-02-29', '2026-10-25', '2099-12-31'])
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


async def test_instant_offset_is_normalized(service_client):
    response = await service_client.post('/calendar/echo', json={
        'instant': '2026-10-25T12:34:56.123456+03:00',
    })
    assert response.status == 200
    from datetime import datetime, timezone
    actual = datetime.fromisoformat(response.json()['instant'].replace('Z', '+00:00'))
    assert actual == datetime(2026, 10, 25, 9, 34, 56, 123456, tzinfo=timezone.utc)


@pytest.mark.parametrize('instant', ['not-a-date', '2026-02-30T12:00:00Z', '1111-11-11T12:00:00Z', '9999-12-31T12:00:00Z', 42, None])
async def test_invalid_instant(service_client, instant):
    response = await service_client.post('/calendar/echo', json={'instant': instant})
    assert response.status == 400
