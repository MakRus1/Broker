"""Проверки C++, которые не покрывает clang-format."""

import re

# Комментарии и литералы не являются кодом; сохраняем строки для диагностики.
IGNORED = re.compile(
    r'//[^\n]*|/\*.*?\*/|R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delimiter)"'
    r'|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
    re.DOTALL,
)
UNUSED_CAST = re.compile(
    r'\(\s*void\s*\)\s*[A-Za-z_]\w*\s*;' r'|static_cast\s*<\s*void\s*>\s*\(\s*[A-Za-z_]\w*\s*\)\s*;'
)


def unused_cast_lines(source):
    code = IGNORED.sub(lambda match: re.sub(r'[^\n]', ' ', match.group()), source)
    return [code.count('\n', 0, match.start()) + 1 for match in UNUSED_CAST.finditer(code)]
