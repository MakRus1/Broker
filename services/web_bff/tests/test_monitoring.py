import pytest


@pytest.mark.parametrize('path', ['/ping', '/ping/ready'])
async def test_ping(service_client, monitor_client, path):
    response = await monitor_client.get(path)
    assert response.status == 200
    response = await service_client.get(path)
    assert response.status == 404
