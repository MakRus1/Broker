from urllib.parse import parse_qs, urlparse
import pytest


@pytest.mark.parametrize('language', ['ru', 'en'])
async def test_login_forwards_supported_language(service_client, language):
    response = await service_client.get('/auth/login?language=' + language, allow_redirects=False)
    assert response.status == 302
    assert parse_qs(urlparse(response.headers['Location']).query)['ui_locales'] == [language]


async def test_login_ignores_unknown_language(service_client):
    response = await service_client.get('/auth/login?language=unexpected', allow_redirects=False)
    assert response.status == 302
    assert 'ui_locales' not in parse_qs(urlparse(response.headers['Location']).query)
