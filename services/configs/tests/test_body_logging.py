"""Логи административного API сохраняют логин автора черновика."""

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
    expected = response.json()
    if valid:
        assert expected['author_login'] == 'author'
        assert 'author_name' not in expected
        assert expected['value'] == request_body['value']
    assert json.loads(outgoing['body']) == expected


async def test_legacy_draft_name_is_omitted_from_response_and_logs(admin_client, service_client, pgsql):
    response = await admin_client.post('/admin/configs/drafts/create', json={
        'config_name': 'SHIPMENT_TERMS_CONFIG',
        'description': 'Старый черновик',
        'expected_config_version': 0,
        'value': terms(True, 60),
    })
    assert response.status == 201
    draft_id = response.json()['id']
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            """UPDATE config_drafts SET document = (document - 'author_login')
               || jsonb_build_object('author_name', %s::TEXT) WHERE id = %s""",
            ('Имя из старого черновика', draft_id),
        )

    path = '/admin/configs/drafts/get'
    async with service_client.capture_logs() as capture:
        response = await admin_client.post(path, json={'id': draft_id})
        assert response.status == 200
    assert 'author_name' not in response.json()
    assert 'author_login' not in response.json()
    records = capture.select(trace_id=response.headers['X-Trace-Id'], meta_type=path)
    outgoing = next(record for record in records if record.get('_type') == 'response')
    assert json.loads(outgoing['body']) == response.json()
