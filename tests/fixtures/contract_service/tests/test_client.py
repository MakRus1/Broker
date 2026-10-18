"""Настоящий HTTP-транспорт, объявленные ошибки API и корреляция трассы."""
import asyncio
import pytest


@pytest.mark.parametrize('status,body,expected', [
    (200, {'greeting': 'Привет'}, {'status': 200, 'value': 'Привет'}),
    (404, {'message': 'Нет'}, {'status': 404, 'value': 'Нет'}),
    (200, {'greeting': 123}, {'error': 'contract'}),
    (200, {}, {'error': 'contract'}),
    (503, {'message': 'Недоступен'}, {'error': 'contract'}),
])
async def test_generated_client(service_client, mockserver, status, body, expected):
    @mockserver.handler('/foo/bar')
    def foo(request):
        assert request.query['name'] == 'A + Б & /'
        assert request.headers.get('X-YaTraceId') == '0123456789abcdef0123456789abcdef'
        return mockserver.make_response(status=status, json=body)
    response = await service_client.get('/client-probe', params={'name': 'A + Б & /'}, headers={'X-YaTraceId': '0123456789abcdef0123456789abcdef'})
    assert response.status == 200
    assert response.json() == expected
    assert foo.times_called == 1


async def test_client_timeout_does_not_retry(service_client, mockserver):
    @mockserver.handler('/foo/bar')
    async def slow(request):
        await asyncio.sleep(0.3)
        return mockserver.make_response(status=200, json={'greeting': 'Поздно'})
    response = await service_client.get('/client-probe', params={'name': 'timeout'})
    assert response.json() == {'error': 'timeout'}
    await asyncio.sleep(0.35)
    assert slow.times_called == 1


async def test_client_post(service_client, mockserver):
    @mockserver.handler('/hello/create')
    def hello(request):
        assert request.method == 'POST'
        assert request.query['name'] == 'Клиент'
        return mockserver.make_response('Привет', headers={'Content-Type': 'text/plain'})
    response = await service_client.get('/client-probe', params={'method': 'post', 'name': 'Клиент'})
    assert response.json() == {'value': 'Привет'}
    assert hello.times_called == 1


async def test_timeout_changes_without_restart(service_client, mockserver, dynamic_config):
    @mockserver.handler('/foo/bar')
    async def slow(request):
        await asyncio.sleep(0.25)
        return mockserver.make_response(status=200, json={'greeting': 'Готово'})
    response = await service_client.get('/client-probe', params={'name': 'config'})
    assert response.json() == {'error': 'timeout'}
    await asyncio.sleep(0.3)
    dynamic_config.set_values({'BROKER_HTTP_CLIENT_CONFIG': {
        '**default**': {'timeout': 100, 'max_retries': 0}, '/foo/bar': {'timeout': 1000}}})
    response = await service_client.get('/client-probe', params={'name': 'config'})
    assert response.json() == {'status': 200, 'value': 'Готово'}


async def test_get_retries_but_post_does_not(service_client, mockserver, dynamic_config):
    dynamic_config.set_values({'BROKER_HTTP_CLIENT_CONFIG': {
        '**default**': {'timeout': 1000, 'max_retries': 2}}})
    @mockserver.handler('/foo/bar')
    def transient(request):
        if transient.times_called < 2:
            return mockserver.make_response(status=503)
        return mockserver.make_response(status=200, json={'greeting': 'Повтор'})
    response = await service_client.get('/client-probe', params={'name': 'retry'})
    assert response.json() == {'status': 200, 'value': 'Повтор'}
    assert transient.times_called == 3
    @mockserver.handler('/hello/create')
    def unavailable(request):
        return mockserver.make_response(status=503)
    response = await service_client.get('/client-probe', params={'method': 'post', 'name': 'retry'})
    assert response.json() == {'error': 'contract'}
    assert unavailable.times_called == 1


@pytest.mark.parametrize('expected', [
    pytest.param({'error': 'timeout'}, marks=pytest.mark.config(
        BROKER_HTTP_CLIENT_CONFIG={'**default**': {'timeout': 30, 'max_retries': 0}}), id='short-timeout'),
    pytest.param({'status': 200, 'value': 'Готово'}, marks=pytest.mark.config(
        BROKER_HTTP_CLIENT_CONFIG={'**default**': {'timeout': 1000, 'max_retries': 0}}), id='long-timeout'),
])
async def test_config_marker_parameters(service_client, mockserver, expected):
    @mockserver.handler('/foo/bar')
    async def slow(request):
        await asyncio.sleep(0.15)
        return mockserver.make_response(status=200, json={'greeting': 'Готово'})
    response = await service_client.get('/client-probe', params={'name': 'config-marker'})
    assert response.json() == expected
    await asyncio.sleep(0.2)


async def test_config_marker_does_not_leak(dynamic_config):
    assert dynamic_config.get('BROKER_HTTP_CLIENT_CONFIG') == {
        '**default**': {'timeout': 100, 'max_retries': 0}}
