"""Композиция схем через настоящий скомпилированный HTTP-сервер."""
import pytest


@pytest.mark.parametrize('body', [
    {'id': 1, 'kind': 'sea', 'voyage': 'V-1'},
    {'kind': 'rail', 'station': 'Москва'},
])
async def test_one_of_round_trip(service_client, body):
    response = await service_client.post('/composition/echo', json=body)
    assert response.status == 200, response.text
    assert response.json() == body


@pytest.mark.parametrize('body', [
    {}, None, [], 1, {'kind': 'unknown'},
    {'id': 0, 'kind': 'sea', 'voyage': 'V-1'},
    {'id': 1, 'kind': 'sea', 'voyage': ''},
    {'id': 1, 'kind': 'sea'},
    {'kind': 'rail', 'station': 'Москва', 'extra': True},
    {'kind': 'rail', 'station': 5},
])
async def test_unknown_or_invalid_variant(service_client, body):
    response = await service_client.post('/composition/echo', json=body)
    assert response.status == 400, response.text


async def test_ambiguous_is_not_first_matching_variant(service_client):
    response = await service_client.post('/composition/ambiguous', json={})
    assert response.status == 400, response.text


@pytest.mark.parametrize('body', [1, 'hello'])
async def test_scalar_union(service_client, body):
    response = await service_client.post('/composition/scalar', json=body)
    assert response.status == 200, response.text
    assert response.json() == body


@pytest.mark.parametrize('body', [0, '', False, {}, None])
async def test_scalar_constraints(service_client, body):
    response = await service_client.post('/composition/scalar', json=body)
    assert response.status == 400, response.text


@pytest.mark.parametrize('optional', ['absent', None, {'kind': 'rail', 'station': 'A'}])
async def test_nested_array_nullable_and_alias(service_client, optional):
    body = {'event': {'kind': 'rail', 'station': 'A'}, 'events': [{'id': 2, 'kind': 'sea', 'voyage': 'V'}]}
    if optional != 'absent':
        body['optional'] = optional
    response = await service_client.post('/composition/wrapped', json=body)
    assert response.status == 200, response.text
    assert response.json() == body
