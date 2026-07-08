async def test_foo_bar(service_client):
    response = await service_client.get('/foo/bar', params={'name': 'Tester'})
    assert response.status == 200
    assert response.json() == {'greeting': 'Hello, Tester!'}


async def test_foo_bar_not_found(service_client):
    response = await service_client.get('/foo/bar', params={'name': 'missing'})
    assert response.status == 404
    assert response.json() == {'message': 'user not found'}
