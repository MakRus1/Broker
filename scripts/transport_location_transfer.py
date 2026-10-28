"""Однократный перенос справочников. Вызывается до запуска новых владельцев API."""

import subprocess

NAME = 'shipments-and-transportations-v1'


def query(command, sql):
    result = subprocess.run(command, input=sql, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError('Ошибка PostgreSQL при переносе справочников: ' + result.stderr.strip())
    return result.stdout


def completed(command):
    return (
        query(command, f"SELECT name FROM directory_transfers WHERE name = '{NAME}';").strip()
        == NAME
    )


def import_sql(locations, stations):
    # psql распознаёт эту строку как конец COPY даже внутри CSV. Не позволяем
    # содержимому исходного справочника стать командами psql/SQL.
    for document in (locations, stations):
        if any(line == r'\.' for line in document.splitlines()):
            raise ValueError('Недопустимая строка в CSV переноса справочника')
    return f"""BEGIN;
SELECT pg_advisory_xact_lock(87410721);
CREATE TEMP TABLE location_import ON COMMIT DROP AS SELECT id, kind, name, country_code, code, archived FROM route_locations WITH NO DATA;
CREATE TEMP TABLE station_import ON COMMIT DROP AS SELECT esr_code, name, country_code, archived FROM railway_stations WITH NO DATA;
COPY location_import FROM STDIN WITH (FORMAT CSV);
{locations}\\.
COPY station_import FROM STDIN WITH (FORMAT CSV);
{stations}\\.
DO $$ BEGIN
    IF EXISTS (
        SELECT FROM location_import i JOIN route_locations d USING (id)
        WHERE ROW(i.kind, i.name, i.country_code, i.code, i.archived)
            IS DISTINCT FROM ROW(d.kind, d.name, d.country_code, d.code, d.archived)
    ) OR EXISTS (
        SELECT FROM station_import i JOIN railway_stations d USING (esr_code)
        WHERE ROW(i.name, i.country_code, i.archived)
            IS DISTINCT FROM ROW(d.name, d.country_code, d.archived)
    ) THEN
        RAISE EXCEPTION 'Целевой справочник содержит несовпадающие записи';
    END IF;
END $$;
INSERT INTO route_locations (id, kind, name, country_code, code, archived, manual_override)
SELECT id, kind, name, country_code, code, archived, TRUE
FROM location_import ON CONFLICT (id) DO NOTHING;
INSERT INTO railway_stations SELECT * FROM station_import ON CONFLICT (esr_code) DO NOTHING;
INSERT INTO directory_transfers(name, completed_at) VALUES ('{NAME}', clock_timestamp());
COMMIT;
"""


def transfer(command):
    target = command('transport_locations')
    if completed(target):
        print('Справочники уже перенесены')
        return
    locations = query(
        command('shipments'),
        'COPY (SELECT id, kind, name, country_code, code, archived FROM route_locations ORDER BY id) TO STDOUT WITH (FORMAT CSV);',
    )
    stations = query(
        command('transportations'),
        'COPY (SELECT esr_code, name, country_code, archived FROM railway_stations ORDER BY esr_code) TO STDOUT WITH (FORMAT CSV);',
    )
    query(target, import_sql(locations, stations))
    print('Справочники перенесены; исходные таблицы сохранены')


def configure_cluster(kubectl, namespace, services):
    prefix = [*kubectl, '-n', namespace]

    def command(database):
        return [
            *prefix,
            'exec',
            '-i',
            'statefulset/postgres',
            '--',
            'psql',
            '-X',
            '-qAt',
            '-v',
            'ON_ERROR_STOP=1',
            '-U',
            'broker',
            '-d',
            database,
        ]

    if completed(command('transport_locations')):
        return
    consumers = {'transport_locations', 'shipments', 'transportations', 'admin_bff', 'web_bff'}
    if not consumers.issubset(services):
        raise ValueError(
            'Первый перенос справочников требует Deploy all: transport_locations и все его потребители'
        )
    import json

    previous = {}
    try:
        for name in ('shipments', 'transportations'):
            obj = json.loads(
                subprocess.check_output(
                    [*prefix, 'get', 'deployment', name, '--ignore-not-found', '-o', 'json'],
                    text=True,
                )
                or '{}'
            )
            if not obj:
                continue
            previous[name] = obj['spec'].get('replicas', 1)
            subprocess.run([*prefix, 'scale', f'deployment/{name}', '--replicas=0'], check=True)
            subprocess.run(
                [*prefix, 'rollout', 'status', f'deployment/{name}', '--timeout=180s'], check=True
            )
            pods = json.loads(
                subprocess.check_output(
                    [*prefix, 'get', 'pod', '-l', f'app.kubernetes.io/name={name}', '-o', 'json'],
                    text=True,
                )
            )
            for pod in pods.get('items', []):
                subprocess.run(
                    [
                        *prefix,
                        'wait',
                        '--for=delete',
                        f"pod/{pod['metadata']['name']}",
                        '--timeout=180s',
                    ],
                    check=True,
                )
        transfer(command)
    except Exception:
        # После разрыва соединения COMMIT мог успеть завершиться. Не запускаем
        # прежних писателей, пока не подтвердим отсутствие отметки переноса.
        if not completed(command('transport_locations')):
            for name, replicas in previous.items():
                subprocess.run(
                    [*prefix, 'scale', f'deployment/{name}', f'--replicas={replicas}'], check=True
                )
        raise
    # После commit старые писатели не запускаются: следующий шаг deploy применит
    # новые образы и штатное число реплик. При сбое deploy нужно повторить Deploy all.


if __name__ == '__main__':

    def command(database):
        return [
            'psql',
            '-X',
            '-qAt',
            '-v',
            'ON_ERROR_STOP=1',
            '-h',
            f"postgres-{database.replace('_', '-')}",
            '-U',
            'testsuite',
            '-d',
            f'{database}_db_1',
        ]

    transfer(command)
