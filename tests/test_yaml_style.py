"""Проверка запрещает синтаксические якоря, сохраняя обычный текст с & и *."""
import runpy
from pathlib import Path
import tempfile
import unittest

CHECK = runpy.run_path(str(Path(__file__).resolve().parents[1] / 'scripts/check-yaml.py'))


class YamlStyleTest(unittest.TestCase):
    def test_anchors_and_aliases_are_rejected(self):
        for text in ('value: &id001 {type: string}\n', 'value: *shared\n'):
            with self.subTest(text=text), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / 'schema.yaml'
                path.write_text(text)
                with self.assertRaisesRegex(ValueError, r':1: YAML-якоря'):
                    CHECK['check_file'](path)

    def test_quoted_and_block_text_are_allowed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'schema.yaml'
            path.write_text('value: "&id001"\nexample: |\n  *shared\n')
            CHECK['check_file'](path)
