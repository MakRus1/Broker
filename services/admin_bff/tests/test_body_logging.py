"""Конфигурационные запросы и ответы доступны по тому же trace_id."""

import json

import pytest


@pytest.mark.parametrize("status", [200, 409])
async def test_config_bodies_are_logged(browser, service_client, mockserver, status):
    await browser.sign_in()
    request_body = {'config_name': 'EXAMPLE'}
    value = {'payload': 'данные-' * 1200}

    expected = (
        {'config_name': 'EXAMPLE', 'value': value, 'version': 0}
        if status == 200
        else {'error': 'conflict', 'message': value['payload']}
    )

    @mockserver.json_handler('/admin/configs/current')
    def current(request):
        return mockserver.make_response(status=status, json=expected)

    trace_id = 'ab1234567890abcdef1234567890abcd'
    async with service_client.capture_logs() as capture:
        response = await browser.post(
            '/api/configs/current',
            request_body,
            {
                'traceparent': f'00-{trace_id}-1234567890abcdef-01',
            },
        )
        assert response.status == status
    assert response.headers['X-Trace-Id'] == trace_id
    records = capture.select(trace_id=trace_id, meta_type='/api/configs/current')
    incoming = next(record for record in records if record.get('_type') == 'request')
    outgoing = next(record for record in records if record.get('_type') == 'response')
    assert json.loads(incoming['body']) == request_body
    assert json.loads(outgoing['body']) == response.json()
    assert json.loads(outgoing['body']) == expected
