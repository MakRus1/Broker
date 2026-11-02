from urllib.parse import parse_qs, urlparse
import pytest


@pytest.mark.parametrize('theme', ['light', 'dark', 'system'])
async def test_login_preserves_selected_theme(service_client, theme):
    response = await service_client.get('/auth/login?theme=' + theme, allow_redirects=False)
    assert response.status == 302
    query = parse_qs(urlparse(response.headers['Location']).query)
    assert query['broker_theme'] == [theme]
    assert query['response_type'] == ['code']
    assert query['code_challenge_method'] == ['S256']
    assert query['state']


async def test_login_ignores_unknown_theme(service_client):
    response = await service_client.get('/auth/login?theme=unexpected', allow_redirects=False)
    assert response.status == 302
    assert 'broker_theme' not in parse_qs(urlparse(response.headers['Location']).query)
