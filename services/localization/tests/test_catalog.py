import json

from conftest import ORGANIZATION, OTHER_ORGANIZATION

PATH = '/api/v1/localization/catalog'


async def test_empty_catalog_and_access(api, service_client):
    body = {'organization_id': ORGANIZATION, 'locale': 'ru'}
    assert await api.post(PATH, body) == {'messages': []}
    await api.post(PATH, body, user='other', expected=403)
    await api.post(PATH, body, user='outsider', expected=403)
    response = await service_client.post(PATH, json=body)
    assert response.status == 401
    await api.post(PATH, body | {'locale': 'de'}, expected=400)


async def test_organization_overrides_and_locale(api, pgsql):
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            "INSERT INTO localization_catalogs (locale, messages, revision) VALUES ('ru', %s::JSONB, 1)",
            (json.dumps({'nav.shipments': 'Поставки', 'action.refresh': 'Обновить'}),),
        )
        cursor.execute(
            "INSERT INTO organization_localization_catalogs "
            "(organization_id, locale, messages, revision) VALUES (%s, 'ru', %s::JSONB, 1)",
            (ORGANIZATION, json.dumps({'nav.shipments': 'Букинги'})),
        )
    body = {'organization_id': ORGANIZATION, 'locale': 'ru'}
    result = await api.post(PATH, body)
    assert {item['key']: item['value'] for item in result['messages']} == {
        'nav.shipments': 'Букинги', 'action.refresh': 'Обновить',
    }
    result = await api.post(
        PATH, body | {'organization_id': OTHER_ORGANIZATION}, user='other'
    )
    assert {item['key']: item['value'] for item in result['messages']}['nav.shipments'] == 'Поставки'
    assert await api.post(PATH, body | {'locale': 'en'}) == {'messages': []}
