"""Реальный HTTP-контракт вложенных общих моделей."""
from copy import deepcopy

import pytest


@pytest.fixture(autouse=True)
async def fresh_required_configs(service_client):
    # Testsuite отключает периодические обновления: каждый тест начинает со свежего конфига.
    # Проверки истечения срока сами продвигают время после этой подготовки.
    await service_client.invalidate_caches(cache_names=['dynamic-config-client-updater'])


def payload():
    profile = {'id': 1, 'address': {'city': 'Москва'}}
    return {'profile': profile, 'profiles': [deepcopy(profile)]}


@pytest.mark.parametrize('note', ['absent', None, 'Описание'])
async def test_nested_models_defaults_and_null(service_client, note):
    body = payload()
    if note != 'absent':
        body['note'] = note
    response = await service_client.post('/models/echo', json=body)
    assert response.status == 200, response.text
    value = response.json()
    assert value['profile']['state'] == 'active'
    assert value['profile']['address']['kind'] == 'city'
    assert value['profiles'][0] == value['profile']
    assert value['limit'] == 5
    assert value['payload_provided'] is False
    if note == 'absent':
        assert 'note' not in value
    else:
        assert value['note'] == note


async def test_nested_explicit_default_and_presence(service_client):
    body = payload()
    body['profile']['state'] = 'active'
    response = await service_client.post('/models/echo', json=body)
    assert response.status == 200
    assert response.json()['payload_provided'] is True


@pytest.mark.parametrize('patch', [
    {'profile': {}},
    {'profile': {'id': 1, 'address': {'city': ''}}},
    {'profile': {'id': 1, 'address': {'city': 'Москва', 'unknown': 1}}},
    {'profile': {'id': 1, 'address': {'city': 'Москва'}, 'state': 'unknown'}},
    {'profiles': [42]}, {'profiles': []}, {'profile': None},
    {'limit': 0}, {'limit': 21}, {'limit': True}, {'note': 1},
])
async def test_nested_contract_rejects_invalid_input(service_client, patch):
    body = {**payload(), **patch}
    response = await service_client.post('/models/echo', json=body)
    assert response.status == 400
    assert response.json()['error'] == 'invalid_request'


@pytest.mark.parametrize('query,expected', [({}, 'active'), ({'state': 'disabled'}, 'disabled')])
async def test_shared_enum_query(service_client, query, expected):
    response = await service_client.get('/models/state', params=query)
    assert response.status == 200
    assert response.json()['state'] == expected


@pytest.mark.parametrize('scoped,expected', [
    ({}, {'state': 'ready', 'kind': 'primary'}),
    ({'state': 'done', 'kind': 'secondary'}, {'state': 'done', 'kind': 'secondary'}),
])
async def test_namespaced_models_roundtrip(service_client, scoped, expected):
    response = await service_client.post('/models/echo', json={**payload(), 'scoped': scoped})
    assert response.status == 200, response.text
    assert response.json()['scoped'] == expected


@pytest.mark.parametrize('scoped', [{'state': 'active'}, {'kind': 'unknown'}])
async def test_namespaced_models_validate_independently(service_client, scoped):
    response = await service_client.post('/models/echo', json={**payload(), 'scoped': scoped})
    assert response.status == 400
