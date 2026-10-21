#!/usr/bin/env python3
"""Транзакционные PostgreSQL-миграции с проверкой истории и advisory lock."""
from dataclasses import dataclass
import hashlib
from pathlib import Path
import re


@dataclass(frozen=True)
class Migration:
    version: int
    name: str
    checksum: str
    sql: str


def discover(directory):
    migrations = []
    for path in sorted(Path(directory).iterdir()):
        if path.name == '.gitkeep' and path.is_file():
            continue
        match = re.fullmatch(r"V([0-9]{4})__[a-z][a-z0-9]*(?:_[a-z0-9]+)*\.sql", path.name)
        if not path.is_file() or not match or int(match[1]) == 0:
            raise ValueError(f"{path}: ожидается V0001__migration_name.sql")
        sql = path.read_text(encoding="utf-8")
        if not sql.strip():
            raise ValueError(f"{path}: пустая миграция")
        migrations.append(
            Migration(int(match[1]), path.name, hashlib.sha256(sql.encode()).hexdigest(), sql)
        )
    if len({m.version for m in migrations}) != len(migrations):
        raise ValueError("Повтор версии миграции")
    return migrations


def render(migrations):
    versions = ",".join(str(m.version) for m in migrations) or "0"
    sql = r"""\set ON_ERROR_STOP on
BEGIN;
SELECT pg_advisory_xact_lock(741219);
CREATE TABLE IF NOT EXISTS public.broker_schema_migrations (
    version integer PRIMARY KEY,
    name text NOT NULL,
    checksum text NOT NULL,
    applied_at timestamptz NOT NULL DEFAULT now()
);
"""
    sql += f"""DO $broker_check$ BEGIN
IF EXISTS (SELECT 1 FROM public.broker_schema_migrations WHERE version NOT IN ({versions})) THEN
RAISE EXCEPTION 'Applied migration is missing from repository';
END IF; END $broker_check$;\n"""
    for m in migrations:
        # Имя получено только из проверенного имени файла; SQL миграции — доверенный код репозитория.
        sql += f"""DO $broker_check$ BEGIN
IF EXISTS (SELECT 1 FROM public.broker_schema_migrations WHERE version = {m.version}
AND (name <> '{m.name}' OR checksum <> '{m.checksum}')) THEN
RAISE EXCEPTION 'Migration {m.version} changed after application';
END IF; END $broker_check$;
SELECT NOT EXISTS (SELECT 1 FROM public.broker_schema_migrations WHERE version = {m.version}) AS broker_apply \\gset
\\if :broker_apply
{m.sql.rstrip()}
INSERT INTO public.broker_schema_migrations(version,name,checksum) VALUES ({m.version},'{m.name}','{m.checksum}');
\\endif
"""
    return sql + "COMMIT;\n"
