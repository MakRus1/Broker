"""Самоподтверждение разрешается настройкой окружения, но не заменяет права."""
import pytest

from test_drafts import create, action
from test_configs import current


@pytest.fixture(scope='session')
def allow_self_approval():
    def patch(config, config_vars):
        config_vars['configs-allow-self-approval'] = True
    return patch


@pytest.mark.uservice_oneshot(config_hooks=['allow_self_approval'])
async def test_self_approval_when_enabled(admin_client, service_client):
    draft = await create(admin_client)
    opened = await admin_client.post('/admin/configs/current', json={'config_name': draft['config_name']})
    assert opened.json()['allow_self_approval'] is True
    published = await action(admin_client, 'approve', draft)
    assert published['approved_by'] == draft['author']
    assert (await current(service_client))['revision'] == published['published_revision']
    events = await admin_client.post('/admin/configs/drafts/events', json={'id': draft['id']})
    assert events.json()['items'][-1]['actor'] == draft['author']


@pytest.mark.uservice_oneshot(config_hooks=['allow_self_approval'])
async def test_self_approval_still_requires_permission(admin_client, monitor_client, access_token):
    draft = await create(admin_client)
    response = await monitor_client.post('/admin/configs/drafts/approve', json={
        'id': draft['id'], 'expected_version': draft['version']}, headers={
        'Authorization': 'Bearer ' + access_token('author', roles=['configs.read', 'configs.write'])})
    assert response.status == 403


async def test_self_approval_disabled_by_default(admin_client):
    draft = await create(admin_client)
    opened = await admin_client.post('/admin/configs/current', json={'config_name': draft['config_name']})
    assert opened.json()['allow_self_approval'] is False
    await action(admin_client, 'approve', draft, status=403)
