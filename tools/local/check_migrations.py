"""Интеграционные проверки runner в отдельной временной БД локального PostgreSQL."""

from pathlib import Path
import subprocess
import sys
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from migrations import discover, render


def check():
    database = "migration_check_" + uuid.uuid4().hex
    command = [
        "docker",
        "compose",
        "-f",
        str(ROOT / ".local/compose.yaml"),
        "exec",
        "-T",
        "postgres-broker",
        "psql",
        "-U",
        "testsuite",
        "-v",
        "ON_ERROR_STOP=1",
    ]

    def query(sql, expected=True, target=database):
        result = subprocess.run(
            command + ["-d", target, "-At"], input=sql, text=True, capture_output=True
        )
        if (result.returncode == 0) != expected:
            raise RuntimeError(result.stderr or "Ожидался отказ применения миграции")
        return result.stdout.strip()

    query(f"CREATE DATABASE {database};", target="postgres")
    try:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            first = path / "V0001__create_probe.sql"
            first.write_text(
                "CREATE TABLE probe(id integer PRIMARY KEY); INSERT INTO probe VALUES (1);"
            )
            query(render(discover(path)))
            query(render(discover(path)))
            assert query("SELECT count(*) FROM probe;") == "1"
            (path / "V0002__add_column.sql").write_text("ALTER TABLE probe ADD COLUMN title text;")
            query(render(discover(path)))
            original = first.read_text()
            first.write_text(original + "\nSELECT 42;")
            query(render(discover(path)), expected=False)
            first.write_text(original)
            broken = path / "V0003__invalid_sql.sql"
            broken.write_text("INSERT INTO probe(id) VALUES(2); SELECT * FROM missing_table;")
            query(render(discover(path)), expected=False)
            assert query("SELECT count(*) FROM probe;") == "1"
            assert query("SELECT max(version) FROM broker_schema_migrations;") == "2"
            broken.unlink()
            (path / "V0002__add_column.sql").unlink()
            query(render(discover(path)), expected=False)
        print(
            "Миграции: повторное применение, новая версия, checksum, откат транзакции, история — OK",
            flush=True,
        )
    finally:
        query(f"DROP DATABASE {database};", target="postgres")


if __name__ == "__main__":
    check()
