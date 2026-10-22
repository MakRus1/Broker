"""DaData мокируется на HTTP-границе; авторизация и PostgreSQL настоящие."""

import asyncio
from uuid import uuid4
import pytest

INN = '7707083893'
PARTY = {
    'value': 'ПАО Сбербанк',
    'data': {
        'inn': INN,
        'kpp': '773601001',
        'ogrn': '1027700132195',
        'name': {
            'short_with_opf': 'ПАО Сбербанк',
            'full_with_opf': 'Публичное акционерное общество Сбербанк',
        },
        'address': {'unrestricted_value': 'г Москва'},
        'state': {'status': 'ACTIVE'},
    },
}


@pytest.fixture
def registry(mockserver):
    state = {'status': 200, 'body': {'suggestions': [PARTY]}}

    @mockserver.json_handler('/dadata/party')
    def handler(request):
        assert request.headers['Authorization'] == 'Token testsuite-dadata-token'
        assert request.headers['Content-Type'] == 'application/json'
        assert request.json == {'query': INN, 'branch_type': 'MAIN', 'count': 1}
        return mockserver.make_response(status=state['status'], json=state['body'])

    return state, handler


async def test_lookup_without_saving(api, registry, pgsql):
    result = await api.post('/api/v1/counterparties/lookup', {'tax_id': INN})
    assert result['items'] == [
        {
            'name': 'ПАО Сбербанк',
            'legal_name': 'Публичное акционерное общество Сбербанк',
            'tax_id': INN,
            'kpp': '773601001',
            'registration_id': '1027700132195',
            'legal_address': 'г Москва',
            'status': 'ACTIVE',
        }
    ]
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT count(*) FROM counterparties')
        assert cursor.fetchone()[0] == 0
        cursor.execute('SELECT count(*) FROM business_audit')
        assert cursor.fetchone()[0] == 0
    assert registry[1].times_called == 1


@pytest.mark.parametrize(
    'tax_id', ['7707083894', '0000000000', '123', '770708389a', '123456789012']
)
async def test_invalid_inn_does_not_call_registry(api, registry, tax_id):
    await api.post('/api/v1/counterparties/lookup', {'tax_id': tax_id}, expected=400)
    assert registry[1].times_called == 0


async def test_lookup_requires_write_permission(api, registry):
    await api.post(
        '/api/v1/counterparties/lookup',
        {'tax_id': INN},
        roles=['counterparties.read'],
        expected=403,
    )
    assert registry[1].times_called == 0


async def test_lookup_not_found(api, registry):
    registry[0]['body'] = {'suggestions': []}
    assert await api.post('/api/v1/counterparties/lookup', {'tax_id': INN}) == {'items': []}


@pytest.mark.parametrize('status', [401, 403, 429, 500, 503, 302])
async def test_registry_failure_is_safe_and_not_retried(api, registry, status):
    registry[0].update(status=status, body={'error': 'testsuite-dadata-token'})
    result = await api.post('/api/v1/counterparties/lookup', {'tax_id': INN}, expected=503)
    assert result['error'] == 'registry_unavailable'
    assert 'testsuite-dadata-token' not in str(result)
    assert registry[1].times_called == 1


@pytest.mark.parametrize(
    'body',
    [
        {},
        {'suggestions': None},
        {'suggestions': [{}]},
        {'suggestions': [{**PARTY, 'data': {**PARTY['data'], 'inn': '7800000000'}}]},
    ],
)
async def test_malformed_registry_response(api, registry, body):
    registry[0]['body'] = body
    result = await api.post('/api/v1/counterparties/lookup', {'tax_id': INN}, expected=503)
    assert result['error'] == 'registry_unavailable'


async def test_kpp_round_trip_and_clear(api):
    body = {'id': str(uuid4()), 'name': 'Компания', 'kpp': '773601001'}
    created = (await api.post('/api/v1/counterparties/create', body, expected=201))['counterparty']
    assert created['kpp'] == body['kpp']
    cleared = (
        await api.post(
            '/api/v1/counterparties/update',
            {'id': body['id'], 'name': 'Компания', 'expected_revision': 1, 'kpp': None},
        )
    )['counterparty']
    assert cleared['kpp'] is None
    assert (await api.post('/api/v1/counterparties/get', {'id': body['id']}))[
        'counterparty'
    ] == cleared


@pytest.mark.uservice_oneshot(registry={'api_key': ''})
async def test_registry_not_configured(api, registry):
    result = await api.post('/api/v1/counterparties/lookup', {'tax_id': INN}, expected=503)
    assert result['error'] == 'registry_not_configured'
    assert registry[1].times_called == 0


@pytest.mark.uservice_oneshot(
    registry={'timeout_ms': 50}, config_hooks=['configure_registry_timeout']
)
async def test_registry_timeout(api, mockserver):
    calls = []

    @mockserver.json_handler('/dadata/party')
    async def delayed(request):
        calls.append(request.json)
        await asyncio.sleep(0.2)
        return {'suggestions': [PARTY]}

    result = await api.post('/api/v1/counterparties/lookup', {'tax_id': INN}, expected=503)
    assert result['error'] == 'registry_unavailable'
    assert calls == [{'query': INN, 'branch_type': 'MAIN', 'count': 1}]


async def test_individual_without_kpp_or_address(api, mockserver):
    @mockserver.json_handler('/dadata/party')
    def individual(request):
        assert request.json['query'] == '784806113663'
        return {
            'suggestions': [
                {
                    'value': 'ИП Тест',
                    'data': {
                        'inn': '784806113663',
                        'ogrn': '310784731900018',
                        'name': {
                            'short_with_opf': None,
                            'full_with_opf': 'Индивидуальный предприниматель Тест',
                        },
                        'kpp': None,
                        'address': None,
                        'state': {'status': 'LIQUIDATED'},
                    },
                }
            ]
        }

    item = (await api.post('/api/v1/counterparties/lookup', {'tax_id': '784806113663'}))['items'][0]
    assert item['name'] == 'ИП Тест'
    assert item['kpp'] is None
    assert item['legal_address'] is None
    assert item['status'] == 'LIQUIDATED'


@pytest.fixture(scope='session')
def configure_registry_timeout(daemon_scoped_mark):
    def patch(config, config_vars):
        # Последний hook выполняется после стандартной подмены таймаута testsuite.
        config['components_manager']['components']['http-client-core']['testsuite-timeout'] = '50ms'

    return patch
