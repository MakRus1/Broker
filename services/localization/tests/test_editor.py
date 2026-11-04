import asyncio
from conftest import ORGANIZATION

ROOT = '/api/admin/localization/'
EMPTY = {'ru': [], 'en': []}
CATALOG = {'ru': [{'key': 'nav.shipments', 'value': 'Букинги'}], 'en': [{'key': 'nav.shipments', 'value': 'Bookings'}]}


async def test_editor_permissions(api, service_client):
    for action, body in [('get', {}), ('save', {'version': 0, 'catalog': EMPTY}), ('publish', {'version': 0})]:
        await api.post(ROOT + action, body, roles=['localization.write', 'localization.read'], expected=403)
        assert (await service_client.post(ROOT + action, json=body)).status == 401
    await api.post(ROOT + 'get', {}, user='reader')
    await api.post(ROOT + 'save', {'version': 0, 'catalog': EMPTY}, user='reader', expected=403)


async def test_draft_publish_and_reset(api):
    async def call(action, body):
        return await api.post(ROOT + action, body, user='translator')
    state = await call('get', {})
    assert state['version'] == 0
    state = await call('save', {'version': 0, 'catalog': CATALOG})
    assert state['draft'] == CATALOG
    assert state['published'] == EMPTY
    assert await api.post('/api/v1/localization/catalog', {'organization_id': ORGANIZATION, 'locale': 'ru'}) == {'messages': []}
    await api.post(ROOT + 'publish', {'version': 0}, user='translator', expected=409)
    state = await call('publish', {'version': state['version']})
    assert state['revision'] == 1 and 'draft' not in state
    assert state['published'] == CATALOG
    for locale in ['ru', 'en']:
        result = await api.post('/api/v1/localization/catalog', {'organization_id': ORGANIZATION, 'locale': locale})
        assert result['messages'] == CATALOG[locale]
    scope = {'organization_id': ORGANIZATION}
    state = await call('get', scope)
    assert state['inherited'] == CATALOG
    state = await call('save', scope | {'version': 0, 'catalog': {'ru': [{'key': 'nav.shipments', 'value': 'Заказы'}], 'en': []}})
    await call('publish', scope | {'version': state['version']})
    state = await call('get', scope)
    state = await call('save', scope | {'version': state['version'], 'catalog': EMPTY})
    await call('publish', scope | {'version': state['version']})
    assert (await api.post('/api/v1/localization/catalog', {'organization_id': ORGANIZATION, 'locale': 'ru'}))['messages'] == CATALOG['ru']


async def test_validation_and_concurrent_save(api):
    for entries in [
        [{'key': 'unknown.key', 'value': 'Text'}],
        [{'key': 'nav.shipments', 'value': ''}],
        [{'key': 'nav.shipments', 'value': '{count}'}],
        CATALOG['ru'] * 2,
    ]:
        await api.post(ROOT + 'save', {'version': 0, 'catalog': {'ru': entries, 'en': []}}, user='translator', expected=400)
    await api.post(ROOT + 'save', {'organization_id': ORGANIZATION, 'version': 0, 'catalog': {'ru': [{'key': 'login.title', 'value': 'Вход'}], 'en': []}}, user='translator', expected=400)
    results = await asyncio.gather(*[api.post(ROOT + 'save', {'version': 0, 'catalog': CATALOG}, user='translator', expected=(200,409)) for _ in range(2)])
    assert sum('version' in result for result in results) == 1


async def test_public_catalog_changes_only_on_publish(api, service_client):
    catalog = {'ru': [{'key': 'keycloak.doLogIn', 'value': 'Войти в Broker'}], 'en': []}
    state = await api.post(ROOT + 'save', {'version': 0, 'catalog': catalog}, user='translator')
    assert (await service_client.get('/localization/catalog?locale=ru')).json()['messages'] == []
    await api.post(ROOT + 'publish', {'version': state['version']}, user='translator')
    assert (await service_client.get('/localization/catalog?locale=ru')).json()['messages'] == catalog['ru']


async def test_register_new_key_without_service_rebuild(api, pgsql, source_catalog):
    source = {'ru': {'new.release.key': 'Новый {count}'}, 'en': {'new.release.key': 'New {count}'}}
    await source_catalog(source)
    state = await api.post(ROOT + 'get', {}, user='translator')
    assert {'key': 'new.release.key', 'value': 'Новый {count}'} in state['source']['ru']
    changes = {'ru': [{'key': 'new.release.key', 'value': 'Переопределённый {count}'}], 'en': []}
    state = await api.post(ROOT + 'save', {'version': 0, 'catalog': changes}, user='translator')
    await source_catalog({'ru': {'another.key': 'Другой'}, 'en': {'another.key': 'Another'}})
    await source_catalog(source)
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute("SELECT updated_at FROM localization_source WHERE key = 'new.release.key'")
        original_time = cursor.fetchone()[0]
    await source_catalog(source)
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute("SELECT updated_at FROM localization_source WHERE key = 'new.release.key'")
        assert cursor.fetchone()[0] == original_time
    current = await api.post(ROOT + 'get', {}, user='translator')
    assert current['version'] == state['version'] and current['draft'] == changes
    await api.post(ROOT + 'publish', {'version': state['version']}, user='translator')
    await source_catalog({'ru': {'new.release.key': 'Обновлённый {count}'}, 'en': {'new.release.key': 'Updated {count}'}})
    current = await api.post(ROOT + 'get', {}, user='translator')
    assert current['published'] == changes
    assert {'key': 'another.key', 'value': 'Другой'} in current['source']['ru']


async def test_registration_access_and_validation(api, service_client, source_catalog):
    path = '/api/internal/localization/sources/upsert'
    body = {'ru': [], 'en': []}
    await api.post(path, body, user='translator', expected=403)
    await api.post(path, body, roles=['localization.register'], expected=403)
    assert (await service_client.post(path, json=body)).status == 401
    await source_catalog({'ru': {'stable.key': '{count}'}, 'en': {'stable.key': '{count}'}})
    await source_catalog({'ru': {'stable.key': '{name}'}, 'en': {'stable.key': '{name}'}}, expected=409)
    await source_catalog({'ru': {'a': 'A'}, 'en': {}}, expected=400)
    await source_catalog({'ru': {'a': '{count}'}, 'en': {'a': '{name}'}}, expected=400)
