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
