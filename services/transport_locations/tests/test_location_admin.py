from uuid import uuid4


async def test_global_directory_permissions(api):
    item = {
        'id': str(uuid4()),
        'kind': 'airport',
        'name': 'Общий аэропорт',
        'country_code': 'KZ',
        'code': 'TEST',
    }
    saved = await api.post('/api/admin/route-locations/save', item, roles=['platform.admin'])
    assert saved == item
    await api.post('/api/admin/route-locations/save', item, user='other', expected=403)
    found = await api.post(
        '/api/v1/route-locations/search', {'kind': 'airport', 'query': 'TEST'}, user='other'
    )
    assert found['items'] == [item]
    await api.post('/api/admin/route-locations/save', item, roles=['platform.admin'], expected=409)
    await api.post(
        '/api/admin/route-locations/archive', {'id': item['id']}, roles=['platform.admin']
    )
    assert (await api.post('/api/v1/route-locations/search', {'kind': 'airport', 'query': 'TEST'}))[
        'items'
    ] == []
