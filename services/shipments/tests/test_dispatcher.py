"""Потеря ответа не освобождает резерв; повтор использует ту же операцию."""

import asyncio
from uuid import uuid4
import pytest
from test_shipments import setup_item


@pytest.mark.parametrize(
    'first_response', ['unavailable', 'duplicates', 'token_unavailable', 'token_redirect']
)
@pytest.mark.uservice_oneshot(config_hooks=['configure_dispatcher'])
async def test_delivery_retries_and_confirms(api, mockserver, service_client, first_response, container_facts):
    state = {'calls': [], 'result': None, 'token_calls': 0, 'redirect_calls': 0}

    @mockserver.json_handler('/service-token')
    def token(request):
        assert request.method == 'POST'
        assert request.form['grant_type'] == 'client_credentials'
        assert request.form['client_id'] == 'testsuite-dispatcher'
        assert request.form['client_secret'] == 'testsuite-only-secret'
        state['token_calls'] += 1
        if state['token_calls'] == 1:
            if first_response == 'token_unavailable':
                return mockserver.make_response(status=503, json={'error': 'unavailable'})
            if first_response == 'token_redirect':
                return mockserver.make_response(
                    status=307, headers={'Location': mockserver.url('/redirected-token')}, json={}
                )
        return {'access_token': 'test-service-token'}

    @mockserver.json_handler('/redirected-token')
    def redirected_token(request):
        state['redirect_calls'] += 1
        return {'access_token': 'unexpected-redirect-token'}

    @mockserver.json_handler('/api/internal/v1/transportations/ensure-created')
    def create(request):
        body = request.json
        state['calls'].append(body)
        assert request.headers['Authorization'] == 'Bearer test-service-token'
        if state['result'] is None:
            state['result'] = {
                'operation': {
                    'id': body['operation_id'],
                    'item_id': body['item_id'],
                    'quantity': body['quantity'],
                    'status': 'succeeded',
                    'transportation_ids': [str(uuid4()) for _ in range(body['quantity'])],
                }
            }
            # Получатель сохранил результат, но отправитель не получил успешный ответ.
            if first_response == 'unavailable':
                return mockserver.make_response(
                    status=503, json={'error': 'unavailable', 'message': 'Retry'}
                )
            if first_response == 'duplicates':
                return {
                    'operation': {
                        **state['result']['operation'],
                        'transportation_ids': [
                            state['result']['operation']['transportation_ids'][0]
                        ]
                        * body['quantity'],
                    }
                }
        return state['result']

    shipment, item = await setup_item(api)
    result = await api.post(
        '/api/v1/transportation-operations/create',
        {
            'item_id': item['id'],
            'expected_revision': 1,
            'quantity': 3,
            'idempotency_key': str(uuid4()),
        },
        expected=202,
    )
    oid = result['operation']['id']
    for _ in range(80):
        selected = await api.post('/api/v1/transportation-operations/get', {'operation_id': oid})
        if selected['operation']['status'] == 'succeeded':
            break
        await asyncio.sleep(0.1)
    else:
        pytest.fail('Операция не доставлена')
    if first_response.startswith('token_'):
        assert state['token_calls'] >= 2
        assert len(state['calls']) == 1
    else:
        assert len(state['calls']) >= 2
    assert state['redirect_calls'] == 0
    assert all(body == state['calls'][0] for body in state['calls'])
    container_facts['groups'] = [{'container_type_code': '40HC', 'container_ownership': 'COC', 'quantity': 3}]
    listed = await api.post('/api/v1/shipment-items/list', {'shipment_id': shipment['id']})
    assert listed['items'][0]['created_quantity'] == 3
    assert listed['items'][0]['pending_quantity'] == 0
    assert listed['items'][0]['available_quantity'] == 2
