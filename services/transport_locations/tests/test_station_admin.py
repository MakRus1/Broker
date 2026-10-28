"""Общий справочник: платформенный доступ и сохранность маршрутов при архивировании."""

import pytest

ROOT = '/api/admin/railway-stations/'
STATION = {'esr_code': '000001', 'name': 'Общая станция', 'country_code': 'KZ'}


@pytest.fixture
def admin(api):
    class Admin:
        async def post(self, path, body, **kwargs):
            return await api.post(path, body, roles=['platform.admin'], **kwargs)

    return Admin()


async def test_shared_station_management_and_search(api, admin):
    assert await admin.post(ROOT + 'save', STATION) == STATION
    assert (await admin.post(ROOT + 'list', {}))['items'] == [STATION]
    # Пользователь другой организации видит ту же станцию.
    assert (await api.post('/api/v1/railway-stations/search', {'query': '000001'}, user='other'))[
        'items'
    ] == [STATION]
    renamed = {**STATION, 'name': 'Новое название'}
    await admin.post(ROOT + 'save', renamed, expected=409)
    assert await admin.post(ROOT + 'save', {**renamed, 'existing': True}) == renamed
    await admin.post(ROOT + 'archive', {'esr_code': '000001'})
    assert (await api.post('/api/v1/railway-stations/search', {'query': '000001'}))['items'] == []
    assert (await admin.post(ROOT + 'list', {}))['items'] == []
    assert await admin.post(ROOT + 'save', {**STATION, 'existing': True}) == STATION


@pytest.mark.parametrize(
    'operation,body', [('list', {}), ('save', STATION), ('archive', {'esr_code': '000001'})]
)
async def test_organization_user_cannot_change_global_directory(api, operation, body):
    await api.post(ROOT + operation, body, user='other', expected=403)
