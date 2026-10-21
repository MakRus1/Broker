"""Проверка YAML-описаний по каталогу реальной PostgreSQL после миграций."""

import yaml


def assert_postgres_types(database, service_source_dir, specification=None):
    if specification is None:
        specification = yaml.safe_load((service_source_dir / 'postgresql/types.yaml').read_text())
    with database.cursor() as cursor:
        for name, definition in specification['types'].items():
            cursor.execute(
                '''
                SELECT attribute.attname, attribute.atttypid
                FROM pg_attribute AS attribute
                JOIN pg_type AS composite ON composite.typrelid = attribute.attrelid
                WHERE composite.oid = to_regtype(%s)
                  AND attribute.attnum > 0 AND NOT attribute.attisdropped
                ORDER BY attribute.attnum
            ''',
                (name,),
            )
            actual = cursor.fetchall()
            fields = definition['fields']
            cursor.execute(
                '''
                SELECT to_regtype(type_name)::oid
                FROM unnest(%s::TEXT[]) AS expected(type_name)
            ''',
                ([field['type'] for field in fields],),
            )
            expected = [(field['name'], oid[0]) for field, oid in zip(fields, cursor.fetchall())]
            assert all(oid is not None for _, oid in expected), f'{name}: тип поля не создан в БД'
            assert (
                actual == expected
            ), f'{name}: SQL-тип и YAML расходятся: {actual!r} != {expected!r}'
