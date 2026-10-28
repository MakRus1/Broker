import io
import sys
from pathlib import Path
import pytest
from psycopg2.errors import RaiseException

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'scripts'))
from transport_location_transfer import import_sql


def apply(cursor, locations, stations):
    script = import_sql(locations, stations)
    # psycopg COPY is used instead of interpreting the psql stream in a test.
    before, locations_copy = script.split('COPY location_import FROM STDIN WITH (FORMAT CSV);\n', 1)
    _, stations_copy = locations_copy.split(
        'COPY station_import FROM STDIN WITH (FORMAT CSV);\n', 1
    )
    after = stations_copy.split('\\.\n', 1)[1]
    cursor.execute(before)
    cursor.copy_expert('COPY location_import FROM STDIN WITH (FORMAT CSV)', io.StringIO(locations))
    cursor.copy_expert('COPY station_import FROM STDIN WITH (FORMAT CSV)', io.StringIO(stations))
    cursor.execute(after)


def test_transfer_preserves_ids_archives_and_quotes(pgsql):
    cursor = pgsql['db_1'].cursor()
    locations = '00000000-0000-0000-0000-000000000001,airport,"Аэропорт, тест",RU,AAA,t\n'
    stations = '000001,"Станция ""тест""",KZ,f\n'
    apply(cursor, locations, stations)
    cursor.execute('SELECT id::text, name, archived FROM route_locations')
    assert cursor.fetchone() == ('00000000-0000-0000-0000-000000000001', 'Аэропорт, тест', True)
    cursor.execute('SELECT esr_code, name FROM railway_stations')
    assert cursor.fetchone() == ('000001', 'Станция "тест"')


def test_transfer_rejects_psql_terminator():
    with pytest.raises(ValueError):
        import_sql('x\n\\.\nSELECT 1;\n', '')


def test_conflict_rolls_back_both_directories(pgsql):
    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        "INSERT INTO route_locations(id,kind,name,country_code) VALUES ('00000000-0000-0000-0000-000000000001','airport','Ручная запись','RU')"
    )
    locations = '00000000-0000-0000-0000-000000000001,airport,Иное название,RU,AAA,f\n'
    with pytest.raises(RaiseException, match='Целевой справочник'):
        apply(cursor, locations, '000001,Станция,RU,f\n')
    cursor.execute('ROLLBACK')
    cursor.execute('SELECT name FROM route_locations')
    assert cursor.fetchone()[0] == 'Ручная запись'
    cursor.execute('SELECT count(*) FROM railway_stations')
    assert cursor.fetchone()[0] == 0
    cursor.execute('SELECT count(*) FROM directory_transfers')
    assert cursor.fetchone()[0] == 0
