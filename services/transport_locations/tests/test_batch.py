from uuid import UUID
import pytest


@pytest.fixture
def records(pgsql):
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            """
            INSERT INTO railway_stations(id, esr_code, name, country_code, archived)
            VALUES ('00000000-0000-0000-0000-000000000001', '000001', 'Открытая станция', 'RU', FALSE),
                   ('00000000-0000-0000-0000-000000000002', '000002', 'Архивная станция', 'KZ', TRUE);
            INSERT INTO route_locations(id, kind, name, country_code, code, archived)
            VALUES ('00000000-0000-0000-0000-000000000001', 'airport', 'Аэропорт', 'RU', 'AAA', FALSE),
                   ('00000000-0000-0000-0000-000000000002', 'sea_port', 'Архивный порт', 'CN', 'CNXXX', TRUE);
        """
        )


async def test_active_batch_for_route_validation(api, records):
    stations = await api.post(
        '/api/v1/railway-stations/get-batch', {'ids': [str(UUID(int=i)) for i in (1, 1, 2, 3)]}
    )
    assert stations['items'] == [
        {'id': str(UUID(int=1)), 'esr_code': '000001', 'name': 'Открытая станция', 'country_code': 'RU'}
    ]
    points = await api.post(
        '/api/v1/route-locations/get-batch', {'ids': [str(UUID(int=i)) for i in (1, 1, 2, 3)]}
    )
    assert points['items'] == [
        {
            'id': str(UUID(int=1)),
            'kind': 'airport',
            'name': 'Аэропорт',
            'country_code': 'RU',
            'code': 'AAA',
        }
    ]


@pytest.mark.parametrize(
    'resource,body', [('railway-stations', {'ids': []}), ('route-locations', {'ids': []})]
)
async def test_batch_empty_and_permission_boundary(api, resource, body):
    assert await api.post(f'/api/v1/{resource}/get-batch', body) == {'items': []}
    await api.post(f'/api/v1/{resource}/get-batch', body, roles=[], expected=403)
