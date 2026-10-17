import pytest


@pytest.mark.parametrize('path', ['/ping', '/ping/ready'])
async def test_ping(service_client, monitor_client, path):
    response = await monitor_client.get(path)
    assert response.status == 200
    response = await service_client.get(path)
    assert response.status == 404


async def test_metrics(monitor_client):
    response = await monitor_client.get('/metrics')
    assert response.status == 200
    assert 'broker' in response.text
    assert 'postgres' in response.text


async def test_http_trace_context(service_client):
    trace_id = '1234567890abcdef1234567890abcdef'
    response = await service_client.get('/hello', headers={'traceparent': f'00-{trace_id}-1234567890abcdef-01'})
    assert response.status == 200
    assert response.headers['traceparent'].split('-')[1] == trace_id


async def test_http_to_grpc_trace(service_client):
    trace_id = '1234567890abcdef1234567890abcdef'
    async with service_client.capture_logs() as capture:
        response = await service_client.get('/internal/trace-probe', headers={'traceparent': f'00-{trace_id}-1234567890abcdef-01'})
        assert response.status == 200
        assert response.text == trace_id
    records = capture.select(trace_id=trace_id, text='SayHello')
    assert records
    assert records[0]['service.name'] == 'broker'
    assert records[0]['service.version'] == '0.1.0'
