"""Создание платформенного конфига пользователем с правом записи."""
import pytest

from test_configs import current
from test_drafts import action

pytestmark = pytest.mark.empty_configs
NAME = 'CUSTOM_SETTINGS'


async def create(client, **patch):
    return await client.post('/admin/configs/drafts/create', json={
        'config_name': NAME, 'expected_config_version': 0,
        'value': {}, 'description': 'Первоначальное значение', **patch})


async def test_create_then_publish(service_client, admin_client):
    before = await current(service_client)
    assert before['configs'] == {}
    assert (await admin_client.get('/admin/configs/list')).json()['items'] == []
    missing = await admin_client.post('/admin/configs/current', json={'config_name': NAME})
    assert missing.status == 200
    assert missing.json()['value'] is None
    assert missing.json()['version'] == 0
    response = await create(admin_client)
    assert response.status == 201
    draft = response.json()
    assert draft['base_value'] is None
    assert draft['base_version'] == 0
    assert await current(service_client) == before
    opened = await admin_client.post('/admin/configs/current', json={'config_name': NAME})
    assert opened.json()['value'] is None
    assert opened.json()['active_draft_id'] == draft['id']
    await action(admin_client, 'approve', draft, status=403)
    await action(admin_client, 'approve', draft, actor='reviewer')
    after = await current(service_client)
    assert after['configs'] == {NAME: {}}
    assert after['revision'] > before['revision']
    assert after['updated_at'] > before['updated_at']


async def test_cancel_initial_draft(admin_client, service_client):
    draft = (await create(admin_client)).json()
    assert (await create(admin_client)).status == 409
    await action(admin_client, 'cancel', draft)
    assert (await admin_client.get('/admin/configs/list')).json()['items'] == []
    assert (await current(service_client))['configs'] == {}
    assert (await create(admin_client)).status == 201


async def test_developer_needs_write_not_admin(monitor_client, access_token):
    token = access_token('developer', roles=['configs.read', 'configs.write'])
    response = await monitor_client.post('/admin/configs/drafts/create', headers={
        'Authorization': 'Bearer ' + token}, json={
        'config_name': NAME, 'expected_config_version': 0,
        'value': {'enabled': True}, 'description': 'Создан разработчиком'})
    assert response.status == 201
    assert response.json()['author'].endswith('#developer')


@pytest.mark.parametrize('name', ['invalid-name', '_CONFIG', 'CONFIG__KEY', 'BROKER_CONFIG_DELIVERY_META'])
async def test_invalid_or_reserved_name(admin_client, name):
    assert (await create(admin_client, config_name=name)).status == 400


async def test_restore_cannot_invent_unpublished_value(admin_client):
    response = await admin_client.post('/admin/configs/drafts/create', json={
        'config_name': NAME, 'expected_config_version': 0,
        'source_revision': 0, 'description': 'Несуществующее значение'})
    assert response.status == 404


async def test_delete_requires_approval_and_preserves_history(admin_client, service_client):
    created = (await create(admin_client, value={'enabled': True})).json()
    published = await action(admin_client, 'approve', created, actor='reviewer')
    version = published['published_revision']
    response = await create(admin_client, value=None, remove=True,
                            expected_config_version=version)
    # null не заменяет отсутствие value: запрос должен задавать ровно одно действие.
    assert response.status == 400
    response = await admin_client.post('/admin/configs/drafts/create', json={
        'config_name': NAME, 'expected_config_version': version,
        'remove': True, 'description': 'Больше не используется'})
    assert response.status == 201
    deletion = response.json()
    assert deletion['value'] is None
    assert (await current(service_client))['configs'][NAME] == {'enabled': True}
    await action(admin_client, 'approve', deletion, status=403)
    await action(admin_client, 'edit', deletion, value={}, status=400)
    deleted = await action(admin_client, 'approve', deletion, actor='reviewer')
    assert (await current(service_client))['configs'] == {}
    assert (await admin_client.get('/admin/configs/list')).json()['items'] == []
    opened = await admin_client.post('/admin/configs/current', json={'config_name': NAME})
    assert opened.json()['version'] == deleted['published_revision']
    restored = await admin_client.post('/admin/configs/drafts/create', json={
        'config_name': NAME, 'expected_config_version': deleted['published_revision'],
        'source_revision': version, 'description': 'Восстановление'})
    assert restored.status == 201
    await action(admin_client, 'approve', restored.json(), actor='reviewer')
    assert (await current(service_client))['configs'][NAME] == {'enabled': True}
    history = await admin_client.post('/admin/configs/drafts/list', json={'config_name': NAME})
    assert len(history.json()['items']) == 3


async def test_cannot_delete_missing_config(admin_client):
    response = await admin_client.post('/admin/configs/drafts/create', json={
        'config_name': NAME, 'expected_config_version': 0,
        'remove': True, 'description': 'Нет такого конфига'})
    assert response.status == 404
