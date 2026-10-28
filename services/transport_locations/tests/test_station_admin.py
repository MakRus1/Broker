"""Общий справочник: платформенный доступ и сохранность маршрутов при архивировании."""

import pytest

ROOT = '/api/admin/railway-stations/'
STATION = {'id': '00000000-0000-0000-0000-000000000001', 'esr_code': '000001', 'name': 'Общая станция', 'country_code': 'KZ'}


@pytest.fixture
def admin(api):
    class Admin:
        async def post(self, path, body, **kwargs):
            return await api.post(path, body, roles=['platform.admin'], **kwargs)

    return Admin()


async def test_shared_station_management_and_search(api, admin):
    assert await admin.post(ROOT + 'save', STATION) == {**STATION, 'esr_source': 'manual'}
    assert (await admin.post(ROOT + 'list', {}))['items'] == [{**STATION, 'esr_source': 'manual'}]
    # Пользователь другой организации видит ту же станцию.
    assert (await api.post('/api/v1/railway-stations/search', {'query': '000001'}, user='other'))[
        'items'
    ] == [{**STATION, 'esr_source': 'manual'}]
    renamed = {**STATION, 'name': 'Новое название'}
    await admin.post(ROOT + 'save', renamed, expected=409)
    assert await admin.post(ROOT + 'save', {**renamed, 'existing': True}) == {**renamed, 'esr_source': 'manual'}
    await admin.post(ROOT + 'archive', {'id': STATION['id']})
    assert (await api.post('/api/v1/railway-stations/search', {'query': '000001'}))['items'] == []
    assert (await admin.post(ROOT + 'list', {}))['items'] == []
    assert await admin.post(ROOT + 'save', {**STATION, 'existing': True}) == {**STATION, 'esr_source': 'manual'}


@pytest.mark.parametrize(
    'operation,body', [('list', {}), ('save', STATION), ('archive', {'id': STATION['id']})]
)
async def test_organization_user_cannot_change_global_directory(api, operation, body):
    await api.post(ROOT + operation, body, user='other', expected=403)


async def test_station_does_not_require_external_codes(admin, api):
    body = {'id': '00000000-0000-0000-0000-000000000002',
            'name': 'Станция без кода', 'country_code': 'CN'}
    assert await admin.post(ROOT + 'save', body) == body
    rows = await api.post('/api/v1/railway-stations/get-batch', {'ids': [body['id']]})
    assert rows['items'] == [body]
    changed = {**body, 'esr_code': '123456', 'uic_code': '1234567', 'existing': True}
    saved = await admin.post(ROOT + 'save', changed)
    assert saved == {**body, 'esr_code': '123456', 'uic_code': '1234567', 'esr_source': 'manual'}
