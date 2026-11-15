async def test_ping(service_client, monitor_client):
    path = '/ping'
    response = await monitor_client.get(path)
    assert response.status == 200
    response = await service_client.get(path)
    assert response.status == 404
