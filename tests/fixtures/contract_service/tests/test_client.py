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
    @mockserver.handler('/hello')
    def hello(request):
        assert request.method == 'POST'
        assert request.query['name'] == 'Клиент'
        return mockserver.make_response('Привет', headers={'Content-Type': 'text/plain'})
    response = await service_client.get('/client-probe', params={'method': 'post', 'name': 'Клиент'})
    assert response.json() == {'value': 'Привет'}
    assert hello.times_called == 1
