import pytest


@pytest.fixture
def stations(pgsql):
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute(
            "INSERT INTO railway_stations(id,esr_code,name,country_code) VALUES ('00000000-0000-0000-0000-000000000001','000001','Тестовая отправления','RU'), ('00000000-0000-0000-0000-000000000002','000002','Тестовая назначения','RU')"
        )


@pytest.mark.parametrize('query', ['', 'Т', '000'])
async def test_station_directory_opens_without_minimum_query(api, stations, query):
    result = await api.post('/api/v1/railway-stations/search', {'query': query})
    assert len(result['items']) == 2
