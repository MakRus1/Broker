"""Расширение карточки сохраняет данные и старый composite-тип."""


def test_details_migration(pgsql, service_source_dir):
    directory = service_source_dir / 'postgresql/schemas/db_1'
    with pgsql['db_1'].cursor() as cursor:
        cursor.execute('BEGIN')
        try:
            cursor.execute('CREATE SCHEMA details_migration')
            cursor.execute('SET LOCAL search_path = details_migration, public')
            paths = sorted(directory.glob('V*.sql'))
            for path in paths[:-1]:
                cursor.execute(path.read_text().replace('public.', 'details_migration.'))
            cursor.execute(
                "INSERT INTO counterparties VALUES ('11111111-1111-4111-8111-111111111111', '22222222-2222-4222-8222-222222222222', 'Существующий клиент')"
            )
            cursor.execute(paths[-1].read_text().replace('public.', 'details_migration.'))
            cursor.execute('SELECT name,revision,archived,roles,contacts FROM counterparties')
            assert cursor.fetchone() == ('Существующий клиент', 1, False, [], [])
            cursor.execute(
                'SELECT (ROW(id,name)::details_migration.counterparty_v1).name FROM counterparties'
            )
            assert cursor.fetchone() == ('Существующий клиент',)
        finally:
            cursor.execute('ROLLBACK')
