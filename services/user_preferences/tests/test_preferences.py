import asyncio

import pytest

from conftest import ORGANIZATION, OTHER_ORGANIZATION

LIST = '/api/v1/preferences/list'
SAVE = '/api/v1/preferences/save'


def setting(**changes):
    return (
        dict(
            organization_id=ORGANIZATION,
            key='shipments:columns:v1',
            value={'value': [{'id': 'number', 'visible': True, 'width': 200}]},
            expected_revision=0,
        )
        | changes
    )


async def test_roundtrip_and_revision_conflict(api):
    body = setting()
    saved = (await api.post(SAVE, body))['preference']
    assert saved == {'key': body['key'], 'value': body['value'], 'revision': 1}
    assert (await api.post(LIST, {'organization_id': ORGANIZATION}))['preferences'] == [saved]
    await api.post(SAVE, body, expected=409)
    changed = (await api.post(SAVE, setting(expected_revision=1, value={'value': []})))[
        'preference'
    ]
    assert changed['revision'] == 2
    assert changed['value'] == {'value': []}
    await api.post(SAVE, setting(expected_revision=1), expected=409)


async def test_user_and_organization_isolation(api):
    await api.post(SAVE, setting())
    assert (await api.post(LIST, {'organization_id': ORGANIZATION}, user='colleague'))[
        'preferences'
    ] == []
    assert (await api.post(LIST, {'organization_id': OTHER_ORGANIZATION}, user='other'))[
        'preferences'
    ] == []
    await api.post(LIST, {'organization_id': ORGANIZATION}, user='other', expected=403)
    await api.post(SAVE, setting(), user='other', expected=403)
    await api.post(LIST, {'organization_id': ORGANIZATION}, user='outsider', expected=403)


async def test_invalid_and_oversized_preferences(api, service_client):
    await api.post(SAVE, setting() | {'subject': 'colleague'}, expected=400)
    await api.post(SAVE, setting(key=''), expected=400)
    await api.post(SAVE, setting(value={'value': 'x' * 65536}), expected=400)
    await api.post(SAVE, setting(expected_revision=-1), expected=400)
    response = await service_client.post(LIST, json={'organization_id': ORGANIZATION})
    assert response.status == 401


@pytest.mark.parametrize('revision', [0, 1])
async def test_concurrent_save_has_one_winner(api, revision):
    if revision:
        await api.post(SAVE, setting())
    results = await asyncio.gather(*[
        api.post(SAVE, setting(expected_revision=revision, value={'value': index}), expected=(200, 409))
        for index in range(2)
    ])
    winners = [result['preference'] for result in results if 'preference' in result]
    assert len(winners) == 1
    assert winners[0]['revision'] == revision + 1
    assert [result['error'] for result in results if 'error' in result] == ['preference_changed']
    assert (await api.post(LIST, {'organization_id': ORGANIZATION}))['preferences'] == winners


@pytest.mark.parametrize('failure,expected', [(403, 403), (404, 403), (500, 502), ('invalid_json', 502)])
@pytest.mark.parametrize('operation', ['list', 'save'])
async def test_membership_failure_preserves_preferences(api, mockserver, pgsql, failure, expected, operation):
    await api.post(SAVE, setting())

    @mockserver.handler('/api/organizations/membership/get')
    def unavailable(request):
        assert request.json == {'organization_id': ORGANIZATION}
        if failure == 'invalid_json':
            return mockserver.make_response(response='invalid membership response', status=200)
        return mockserver.make_response(status=failure, json={'error': 'test', 'message': 'test'})

    body = {'organization_id': ORGANIZATION} if operation == 'list' else setting(expected_revision=1, value={'value': 'changed'})
    error = await api.post(LIST if operation == 'list' else SAVE, body, expected=expected)
    assert error['error'] == ('organization_access_denied' if expected == 403 else 'organizations_unavailable')
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('SELECT value, revision FROM user_preferences WHERE organization_id=%s AND key=%s',
                       (ORGANIZATION, setting()['key']))
        assert cursor.fetchone() == (setting()['value'], 1)
