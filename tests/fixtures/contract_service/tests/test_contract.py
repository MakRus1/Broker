import pytest


async def test_no_content(service_client):
    response = await service_client.get('/items/42', params={'active': 'false'})
    assert response.status == 204
    assert response.content == b''


async def test_independent_post_and_conflict(service_client):
    response = await service_client.post('/items/42', json={'name': '', 'count': 0, 'tags': []})
    assert response.status == 201
    assert response.json() == {'id': 42}
    response = await service_client.post('/items/42', json={'name': 'Тест', 'count': -1})
    assert response.status == 409
    assert response.json() == {'id': 42}
    response = await service_client.post('/items/42', json={'name': 'Тест'})
    assert response.status == 201


@pytest.mark.parametrize('payload', [{}, {'name': None}, {'name': 3}, {'name': 'x', 'count': False}, {'name': 'x', 'tags': [1]}, {'name': 'x', 'extra': 'x'}])
async def test_body_validation(service_client, payload):
    response = await service_client.post('/items/42', json=payload)
    assert response.status == 400


@pytest.mark.parametrize('value', ['nope', '1tail', '9223372036854775808'])
async def test_path_validation(service_client, value):
    response = await service_client.get('/items/' + value)
    assert response.status == 400


@pytest.mark.parametrize('value', ['', '0', 'False', 'garbage'])
async def test_query_validation(service_client, value):
    response = await service_client.get('/items/42', params={'active': value})
    assert response.status == 400


@pytest.mark.parametrize('body', ['', '{', '[]', 'null'],
                         ids=['missing', 'malformed', 'array', 'null'])
async def test_missing_and_malformed_body(service_client, body):
    response = await service_client.post('/items/42', data=body,
                                         headers={'Content-Type': 'application/json'})
    assert response.status == 400


async def test_repeated_query_parameter(service_client):
    response = await service_client.get('/items/42?active=true&active=false')
    assert response.status == 400


@pytest.mark.parametrize('method,payload,status', [('get', None, 204), ('post', {'name': 'x'}, 201), ('post', {}, 400)])
async def test_automatic_metadata(service_client, method, payload, status):
    trace_id = '1234567890abcdef1234567890abcdef'
    async with service_client.capture_logs() as capture:
        response = await service_client.request(method, '/items/42', json=payload,
            headers={'traceparent': f'00-{trace_id}-1234567890abcdef-01'})
        assert response.status == status
    records = capture.select(trace_id=trace_id)
    assert any(record.get('service.name') == 'contract_service'
               and record.get('service.version') == '0.1.0'
               and record.get('deployment.environment.name') == 'testing' for record in records)


async def test_default_preserves_presence(service_client):
    response = await service_client.get('/validated')
    assert response.status == 200
    assert response.json() == {'limit': 10, 'provided': False, 'status': 'new'}
    response = await service_client.get('/validated', params={'limit': '10', 'status': 'in_transit'})
    assert response.status == 200
    assert response.json() == {'limit': 10, 'provided': True, 'status': 'in_transit'}


@pytest.mark.parametrize('params', [
    {'status': 'unknown'}, {'limit': '0'}, {'limit': '101'}, {'limit': '2147483648'},
    {'day': '2025-02-29'}, {'day': '2026-13-01'}, {'day': '2026-1-01'},
    {'token': 'not-a-uuid'}, {'ratio': '-0.25'}, {'ratio': '1.25'}, {'ratio': '0.1'}, {'ratio': '1e100'},
])
async def test_format_and_query_constraints(service_client, params):
    response = await service_client.get('/validated', params=params)
    assert response.status == 400


async def test_valid_formats(service_client):
    response = await service_client.get('/validated', params={
        'day': '2024-02-29', 'token': '12345678-1234-1234-1234-123456789abc', 'ratio': '0.5'})
    assert response.status == 200
    response = await service_client.post('/validated', json={
        'status': 'closed', 'labels': ['abc', 'def'], 'created': '2026-10-17T12:30:20.123+03:00',
        'amount': 3.5, 'count': 0, 'description': '🚢🚂', 'encoded': 'dGVzdA=='})
    assert response.status == 201


@pytest.mark.parametrize('extra', [
    {'status': 'unknown'}, {'labels': []}, {'labels': ['same', 'same']}, {'labels': ['a', 'b', 'c', 'd']},
    {'labels': ['']}, {'labels': ['abcdef']}, {'labels': ['A']}, {'labels': ['я']},
    {'created': '2026-02-29T12:00:00Z'}, {'created': '2026-10-17T24:00:00Z'},
    {'created': '2026-10-17T12:00:00+27:00'}, {'created': '2026-10-17'},
    {'amount': -0.5}, {'amount': 100.5}, {'amount': 0.1}, {'count': -1},
    {'description': '🚢'}, {'description': 'пятья'}, {'encoded': 'invalid=='},
])
async def test_body_constraints(service_client, extra):
    response = await service_client.post('/validated', json={'status': 'new', **extra})
    assert response.status == 400


async def test_body_defaults_are_values(service_client):
    response = await service_client.post('/validated', json={'status': 'new'})
    assert response.status == 201
    assert response.json() == {'status': 'success', 'count': 3, 'count_provided': False, 'labels': ['a']}
    response = await service_client.post('/validated', json={'status': 'new', 'count': 0, 'labels': ['b']})
    assert response.status == 201
    assert response.json() == {'status': 'success', 'count': 0, 'count_provided': True, 'labels': ['b']}


async def test_response_enum_value_is_not_allowed_in_request(service_client):
    response = await service_client.post('/validated', json={'status': 'success'})
    assert response.status == 400
