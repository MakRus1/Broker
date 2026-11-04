async def test_public_catalog_is_anonymous_and_has_cors(service_client, mockserver):
    @mockserver.json_handler('/localization/catalog')
    def catalog(request):
        assert 'Authorization' not in request.headers
        return {'messages': [{'key': 'keycloak.doLogIn', 'value': 'Sign in'}]}

    response = await service_client.get('/localization/catalog?locale=en')
    assert response.status == 200
    assert response.headers['Access-Control-Allow-Origin'] == '*'
    assert response.json() == {'messages': [{'key': 'keycloak.doLogIn', 'value': 'Sign in'}]}
    assert catalog.times_called == 1
