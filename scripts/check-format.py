#!/usr/bin/env python3
"""Проверка C++ единым clang-format из окружения инструментов проекта."""
import argparse
from pathlib import Path
import subprocess
import sys
from cpp_style import unused_cast_lines

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--fix', action='store_true')
parser.add_argument('--generated', type=Path, action='append', default=[])
args = parser.parse_args()
files = []
for base in [root/'libs', root/'services', root/'scripts/templates/service', root/'tests/fixtures', *args.generated]:
    files.extend(str(p) for p in base.rglob('*') if p.suffix in {'.cpp', '.hpp'} and p.is_file())
command = [str(Path(sys.executable).parent/'clang-format'), '--style=file:'+str(root/'.clang-format')]
command += ['-i'] if args.fix else ['--dry-run', '--Werror']
subprocess.run([*command, *sorted(set(files))], check=True)

violations = []
for filename in sorted(set(files)):
    for line in unused_cast_lines(Path(filename).read_text()):
        violations.append(f'{filename}:{line}: не подавляйте неиспользуемый параметр через void-cast; '
                          'уберите имя или используйте [[maybe_unused]]')
if violations:
    sys.exit('\n'.join(violations))
