"""Каталоги для изолированной БД testsuite, не для деплоя."""
import json

def literal(value):
    return "'" + value.replace("'", "''") + "'"


def register_sql(owner, version, entries):
    document = [
        {'code': code, **value} for code, value in entries.items() if value['owner'] == owner
    ]
    return (
        'BEGIN; SELECT pg_advisory_xact_lock(824193); '
        'INSERT INTO permission_releases(owner,version,entries,active) VALUES ('
        + ','.join(
            [
                literal(owner),
                literal(version),
                literal(json.dumps(document, ensure_ascii=False)),
                'true',
            ]
        )
        + ') ON CONFLICT(owner,version) DO UPDATE SET active=true; COMMIT;'
    )
