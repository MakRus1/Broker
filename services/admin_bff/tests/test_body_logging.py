"""Конфигурационные запросы и ответы доступны по тому же trace_id."""

import json
import re

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


@pytest.mark.parametrize('size', [32767, 32768, 32769, 65536])
@pytest.mark.parametrize('multibyte', [False, True])
async def test_large_bodies_are_truncated(browser, service_client, mockserver, size, multibyte):
    await browser.sign_in()
    # Подбираем размер JSON точно на границе лимита, включая многобайтный символ.
    prefix = '{"config_name":"EXAMPLE","value":{"payload":"'
    suffix = '"},"version":0}'
    payload_size = size - len(prefix) - len(suffix)
    payload = (
        ('я' * (payload_size // 2) + 'x' * (payload_size % 2)) if multibyte else 'x' * payload_size
    )
    response_body = prefix + payload + suffix

    @mockserver.handler('/admin/configs/current')
    def current(request):
        return mockserver.make_response(response_body, headers={'Content-Type': 'application/json'})

    trace_id = 'ef1234567890abcdef1234567890abcd'
    async with service_client.capture_logs() as capture:
        response = await browser.post(
            '/api/configs/current',
            {'config_name': payload},
            {
                'traceparent': f'00-{trace_id}-1234567890abcdef-01',
            },
        )
        assert response.status == 200
    assert response.json() == json.loads(response_body)
    assert len(response.content) == size
    records = capture.select(trace_id=trace_id, meta_type='/api/configs/current')
    for kind, length_field in [
        ('request', 'request_body_length'),
        ('response', 'response_body_length'),
    ]:
        record = next(record for record in records if record.get('_type') == kind)
        original_size = int(record[length_field])
        logged = record['body']
        if original_size <= 32768:
            assert 'truncated' not in logged
            assert len(logged.encode()) == original_size
        else:
            marker = re.search(r'\.\.\.\(truncated, total [0-9]+ bytes\)$', logged)
            assert marker is not None
            prefix_size = len(logged[: marker.start()].encode())
            assert prefix_size <= 32768
            assert original_size > prefix_size
