"""Один активный драфт на конфиг; подтверждение сразу атомарно публикует значение."""
import asyncio
import pytest

from test_configs import current, publish_one, terms

PREFIX = '/admin/configs/drafts/'
NAME = 'SHIPMENT_TERMS_CONFIG'


async def create(client, **patch):
    body = {'config_name': NAME, 'description': 'Проверка условий', 'value': terms(True, 60)}
    actor = patch.pop('actor', 'author')
    body.update(patch)
    selected = await client.post('/admin/configs/current', json={'config_name': body['config_name']})
    assert selected.status == 200
    body.setdefault('expected_config_version', selected.json()['version'])
    response = await client.post(PREFIX + 'create', user=actor, json=body)
    assert response.status == 201, response.text
    return response.json()


async def action(client, operation, draft, actor='author', status=200, **patch):
    body = {'id': draft['id'], 'expected_version': draft['version'], **patch}
    response = await client.post(PREFIX + operation, user=actor, json=body)
    assert response.status == status, response.text
    return response.json()


async def test_lifecycle(service_client, admin_client):
    before = await current(service_client)
    draft = await create(admin_client)
    assert draft['base_version'] == 0
    assert draft['base_value'] == before['configs'][NAME]
    await action(admin_client, 'edit', draft, value={}, status=400)
    draft = await action(admin_client, 'edit', draft, value=terms(True, 60))
    await action(admin_client, 'approve', draft, status=403)
    assert await current(service_client) == before
    draft = await action(admin_client, 'approve', draft, actor='reviewer')
    assert draft['status'] == 'published'
    after = await current(service_client)
    assert after['revision'] == draft['published_revision']
    assert after['configs'][NAME] == terms(True, 60)
    events = await admin_client.post(PREFIX + 'events', json={'id': draft['id']})
    assert events.status == 200
    assert [event['action'] for event in events.json()['items']] == ['create', 'edit', 'approve']
    assert events.json()['items'][-1]['actor'] == 'http://broker-test-issuer#reviewer'
    assert events.json()['items'][-1]['snapshot']['value'] == terms(True, 60)
    for operation in ['edit', 'approve', 'cancel']:
        await action(admin_client, operation, draft, status=409)


async def test_review_of_stale_draft_version(admin_client):
    before = await create(admin_client)
    draft = await action(admin_client, 'edit', before, value=terms(True, 90))
    await action(admin_client, 'approve', before, actor='reviewer', status=409)
    draft = await action(admin_client, 'approve', draft, actor='reviewer')
    assert draft['value'] == terms(True, 90)


async def test_other_config_does_not_conflict(service_client, admin_client):
    draft = await create(admin_client)
    client_value = {'**default**': {'timeout': 2000, 'max_retries': 1}}
    await publish_one(admin_client, 'BROKER_HTTP_CLIENT_CONFIG', client_value)
    await action(admin_client, 'approve', draft, actor='reviewer')
    assert (await current(service_client))['configs']['BROKER_HTTP_CLIENT_CONFIG'] == client_value


async def test_concurrent_edit_and_approval(admin_client):
    draft = await create(admin_client)
    responses = await asyncio.gather(*[
        admin_client.post(PREFIX + 'edit', json={'id': draft['id'], 'expected_version': draft['version'], 'value': terms(True, days)})
        for days in [60, 90]])
    assert sorted(response.status for response in responses) == [200, 409]
    edited = next(response.json() for response in responses if response.status == 200)
    responses = await asyncio.gather(*[
        admin_client.post(PREFIX + 'approve', user=reviewer, json={'id': edited['id'], 'expected_version': edited['version']})
        for reviewer in ['reviewer1', 'reviewer2']])
    assert sorted(response.status for response in responses) == [200, 409]


async def test_single_active_draft_and_release(admin_client):
    draft = await create(admin_client)
    opened = await admin_client.post('/admin/configs/current', json={'config_name': NAME})
    assert opened.json()['active_draft_id'] == draft['id']
    duplicate = {'config_name': NAME, 'description': 'Второй драфт', 'value': terms(True, 90), 'expected_config_version': draft['base_version']}
    response = await admin_client.post(PREFIX + 'create', json=duplicate)
    assert response.status == 409
    await action(admin_client, 'cancel', draft)
    draft = await create(admin_client)
    await action(admin_client, 'approve', draft, actor='reviewer')
    await create(admin_client)


async def test_concurrent_create(admin_client):
    body = {'config_name': NAME, 'description': 'Создание', 'value': terms(), 'expected_config_version': 0}
    responses = await asyncio.gather(*[admin_client.post(PREFIX + 'create', json=body) for _ in range(2)])
    assert sorted(response.status for response in responses) == [201, 409]


async def test_owner_and_cancel(admin_client):
    draft = await create(admin_client)
    await action(admin_client, 'edit', draft, actor='stranger', status=403, value={})
    for operation in ['cancel']:
        await action(admin_client, operation, draft, actor='stranger', status=403)
    cancelled = await action(admin_client, 'cancel', draft)
    assert cancelled['status'] == 'cancelled'
    await action(admin_client, 'approve', cancelled, actor='reviewer', status=409)


async def test_rollback_one_config(service_client, admin_client):
    changed = await publish_one(admin_client, NAME, terms(True, 90))
    await publish_one(admin_client, 'BROKER_HTTP_CLIENT_CONFIG', {'**default**': {'timeout': 2000, 'max_retries': 0}})
    response = await admin_client.post(PREFIX + 'create', json={'config_name': NAME, 'description': 'Возврат дефолта', 'source_revision': 0, 'expected_config_version': changed['published_revision']})
    assert response.status == 201
    await action(admin_client, 'approve', response.json(), actor='reviewer')
    values = (await current(service_client))['configs']
    assert values[NAME] == terms()
    assert values['BROKER_HTTP_CLIENT_CONFIG']['**default**']['timeout'] == 2000


async def test_list_and_get(admin_client):
    first = await create(admin_client)
    await action(admin_client, 'cancel', first)
    second = await create(admin_client)
    response = await admin_client.post(PREFIX + 'list', json={'config_name': NAME, 'before_id': second['id']})
    assert response.status == 200
    assert [item['id'] for item in response.json()['items']] == [first['id']]
    response = await admin_client.post(PREFIX + 'get', json={'id': second['id']})
    assert response.status == 200
    assert response.json() == second


@pytest.mark.parametrize('patch', [
    {'actor': ''}, {'actor': ' '}, {'actor': 5}, {'config_name': 'UNKNOWN'},
    {'description': ''}, {'source_revision': 0}, {'extra': True},
])
async def test_invalid_create(admin_client, patch):
    response = await admin_client.post(PREFIX + 'create', json={'config_name': NAME, 'description': 'Проверка', 'value': terms(), 'expected_config_version': 0, **patch})
    assert response.status == 400


@pytest.mark.parametrize('operation', ['get', 'events', 'edit', 'approve', 'cancel'])
async def test_missing_draft(admin_client, operation):
    body = {'id': 987654321}
    if operation not in ['get', 'events']:
        body.update(expected_version=1)
    response = await admin_client.post(PREFIX + operation, json=body)
    assert response.status == 404


@pytest.mark.parametrize('operation', ['create', 'get', 'list', 'edit', 'approve', 'cancel', 'events'])
async def test_admin_only(service_client, operation):
    response = await service_client.post(PREFIX + operation, json={})
    assert response.status == 404


@pytest.mark.parametrize('operation', ['submit', 'reject', 'publish', 'rebase'])
async def test_no_extra_workflow_steps(admin_client, operation):
    response = await admin_client.post(PREFIX + operation, json={})
    assert response.status == 404


async def test_invalid_draft_is_not_created(admin_client):
    response = await admin_client.post(PREFIX + 'create', json={
        'config_name': NAME, 'description': 'Невалидное значение',
        'expected_config_version': 0, 'value': {}})
    assert response.status == 400
    response = await admin_client.post(PREFIX + 'list', json={})
    assert response.json()['items'] == []
    await create(admin_client)


async def test_editor_version_conflict(admin_client):
    opened = await admin_client.post('/admin/configs/current', json={'config_name': NAME})
    assert opened.status == 200
    await publish_one(admin_client, NAME, terms(True, 90))
    response = await admin_client.post(PREFIX + 'create', json={
        'config_name': NAME, 'description': 'Устаревший редактор',
        'expected_config_version': opened.json()['version'], 'value': terms(True, 60)})
    assert response.status == 409
    items = (await admin_client.post(PREFIX + 'list', json={})).json()['items']
    assert len(items) == 1
    assert items[0]['status'] == 'published'


async def test_different_configs_allow_independent_drafts(admin_client):
    await create(admin_client)
    await create(admin_client, config_name='BROKER_HTTP_CLIENT_CONFIG', value={
        '**default**': {'timeout': 2000, 'max_retries': 0}})
    drafts = await admin_client.post(PREFIX + 'list', json={})
    assert len(drafts.json()['items']) == 2


async def test_current_is_admin_only(service_client):
    response = await service_client.post('/admin/configs/current', json={'config_name': NAME})
    assert response.status == 404


async def test_current_has_no_active_draft(admin_client):
    opened = await admin_client.post('/admin/configs/current', json={'config_name': NAME})
    assert opened.status == 200
    assert opened.json().get('active_draft_id') is None


async def test_edit_races_with_approval(service_client, admin_client):
    draft = await create(admin_client)
    edit, approval = await asyncio.gather(
        admin_client.post(PREFIX + 'edit', json={'id': draft['id'], 'expected_version': draft['version'], 'value': terms(True, 90)}),
        admin_client.post(PREFIX + 'approve', user='reviewer', json={'id': draft['id'], 'expected_version': draft['version']}))
    assert sorted([edit.status, approval.status]) == [200, 409]
    values = await current(service_client)
    if edit.status == 200:
        assert values['revision'] == 0
        assert values['configs'][NAME] == terms()
    else:
        assert values['configs'][NAME] == terms(True, 60)


async def test_catalog(admin_client):
    response = await admin_client.get('/admin/configs/list')
    assert response.status == 200
    catalog = {item['config_name']: item for item in response.json()['items']}
    assert NAME in catalog
    assert catalog[NAME]['version'] == 0
    assert catalog[NAME]['active_draft_id'] is None
    draft = await create(admin_client)
    catalog = {item['config_name']: item for item in (await admin_client.get('/admin/configs/list')).json()['items']}
    assert catalog[NAME]['active_draft_id'] == draft['id']
    await action(admin_client, 'approve', draft, actor='reviewer')
    catalog = {item['config_name']: item for item in (await admin_client.get('/admin/configs/list')).json()['items']}
    assert catalog[NAME]['version'] > 0
    assert catalog[NAME]['active_draft_id'] is None


async def test_catalog_is_internal(service_client):
    assert (await service_client.get('/admin/configs/list')).status == 404


@pytest.mark.parametrize('value', [None, [], 'text', 42])
async def test_codegen_rejects_non_object_config(admin_client, value):
    response = await admin_client.post(PREFIX + 'create', json={
        'config_name': NAME, 'description': 'Проверка типа',
        'expected_config_version': 0, 'value': value})
    assert response.status == 400
    assert response.json()['error'] == 'invalid_request'


async def test_unicode_description_limits(admin_client):
    draft = await create(admin_client, description='я' * 4096)
    assert draft['description'] == 'я' * 4096


@pytest.mark.pgsql('db_1', files=['corrupt_draft.sql'])
@pytest.mark.parametrize('operation,body', [('get', {'id': 900000}), ('list', {})])
async def test_corrupt_stored_draft_is_server_error(admin_client, operation, body):
    response = await admin_client.post(PREFIX + operation, json=body)
    assert response.status == 500
