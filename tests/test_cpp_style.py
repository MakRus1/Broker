"""Проверяем запрет подавления неиспользуемых параметров."""

import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from cpp_style import unused_cast_lines


class CppStyleTest(unittest.TestCase):
    def test_unused_casts(self):
        for source in ['(void)request;', '( void ) request ;', 'static_cast<void>(request);']:
            with self.subTest(source=source):
                self.assertEqual(unused_cast_lines(source), [1])

    def test_allowed_code_and_text(self):
        for source in [
            'void Handle(Request&&);',
            'void Handle([[maybe_unused]] Request&& request);',
            'void Handle(Request&& /*request*/);',
            '(void)request.perform();',
            '(void)Validate(value);',
            '// (void)request;',
            '/* (void)request; */',
            'const auto text = "(void)request;";',
            'const auto text = R"tag((void)request;)tag";',
        ]:
            with self.subTest(source=source):
                self.assertEqual(unused_cast_lines(source), [])

    def test_line_numbers(self):
        self.assertEqual(unused_cast_lines('/* comment\n */\n(void)request;'), [3])
