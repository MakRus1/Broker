"""Проверки подготовки миграций без изменения пользовательских баз."""

from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from migrations import discover, render


class MigrationsTest(unittest.TestCase):
    def test_validation(self):
        for filename, body in [
            ("V0000__invalid.sql", "SELECT 1;"),
            ("bad.sql", "SELECT 1;"),
            ("V0001__empty.sql", ""),
        ]:
            with self.subTest(filename=filename), tempfile.TemporaryDirectory() as directory:
                (Path(directory) / filename).write_text(body)
                with self.assertRaises(ValueError):
                    discover(directory)

    def test_duplicate_versions(self):
        with tempfile.TemporaryDirectory() as directory:
            for name in ("first", "second"):
                (Path(directory) / f"V0001__{name}.sql").write_text("SELECT 1;")
            with self.assertRaises(ValueError):
                discover(directory)

    def test_reproducible_checksums(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "V0001__create_table.sql"
            path.write_text("CREATE TABLE example(id integer);")
            first = discover(directory)
            self.assertEqual(render(first), render(discover(directory)))
            path.write_text("CREATE TABLE example(id text);")
            self.assertNotEqual(first[0].checksum, discover(directory)[0].checksum)


if __name__ == "__main__":
    unittest.main()
