"""Миграция терминологии сохраняет данные предыдущей схемы."""

from naming_migrations import check_naming_migration


def test_naming_migration(pgsql, service_source_dir):
    check_naming_migration(pgsql['db_1'], service_source_dir)
