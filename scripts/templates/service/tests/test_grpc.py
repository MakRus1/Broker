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
