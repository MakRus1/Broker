import grpc
import pytest

import handlers.hello_pb2 as hello_protos  # noqa: E402, E501


async def test_grpc_service(grpc_service, service_client):
    request = hello_protos.HelloRequest(name='Tester')
    async with service_client.capture_logs() as capture:
        response = await grpc_service.SayHello(request)
    assert response.text == 'Hello, Tester!\n'
    records = capture.select(text='SayHello')
    assert records
    assert records[0]['service.name'] == 'broker'
    assert records[0]['service.version'] == '0.1.0'


async def test_grpc_readiness_blocks_handler_and_recovers(grpc_service, service_client, monitor_client):
    request = hello_protos.HelloRequest(name='Readiness')
    assert (await grpc_service.SayHello(request)).text == 'Hello, Readiness!\n'
    try:
        assert (await monitor_client.get('/internal/readiness-control', params={'ready': 'false'})).status == 200
        assert (await monitor_client.get('/ping')).status == 200
        assert (await monitor_client.get('/ping/ready')).status == 503
        assert (await service_client.get('/hello')).status == 503
        async with service_client.capture_logs() as capture:
            for _ in range(2):
                with pytest.raises(grpc.aio.AioRpcError) as error:
                    await grpc_service.SayHello(request)
                assert error.value.code() == grpc.StatusCode.UNAVAILABLE
                assert error.value.details() == 'dependency unavailable'
        # Прямой RPC не обходит gate и не вызывает бизнес-обработчик.
        assert not capture.select(text='SayHello')
    finally:
        await monitor_client.get('/internal/readiness-control', params={'ready': 'true'})
    assert (await monitor_client.get('/ping/ready')).status == 200
    assert (await service_client.get('/hello')).status == 200
    assert (await grpc_service.SayHello(request)).text == 'Hello, Readiness!\n'
