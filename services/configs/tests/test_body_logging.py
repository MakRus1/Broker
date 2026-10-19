"""Логи административного API содержат тело запроса и полученный драфт."""

import json

import pytest

from test_configs import terms


@pytest.mark.parametrize("valid", [True, False])
async def test_draft_bodies_are_logged(admin_client, service_client, valid):
    request_body = {
        'config_name': 'SHIPMENT_TERMS_CONFIG',
        'description': 'Отладка конфигов',
        'expected_config_version': 0,
        'value': terms(True, 60) if valid else {},
    }
    trace_id = 'cd1234567890abcdef1234567890abcd'
    async with service_client.capture_logs() as capture:
        response = await admin_client.post(
            '/admin/configs/drafts/create',
            json=request_body,
            headers={
                'traceparent': f'00-{trace_id}-1234567890abcdef-01',
            },
        )
        assert response.status == (201 if valid else 400)
    assert response.headers['X-Trace-Id'] == trace_id
    records = capture.select(trace_id=trace_id, meta_type='/admin/configs/drafts/create')
    incoming = next(record for record in records if record.get('_type') == 'request')
    outgoing = next(record for record in records if record.get('_type') == 'response')
    assert json.loads(incoming['body']) == request_body
    assert json.loads(outgoing['body']) == response.json()
    if valid:
        assert json.loads(outgoing['body'])['value'] == request_body['value']
