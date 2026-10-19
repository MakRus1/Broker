"""Упаковка Release-бинарников и замыкания динамических библиотек без компилятора."""

from pathlib import Path
import re
import shutil
import subprocess
import sys


def package(build, output, services):
    output.mkdir(parents=True, exist_ok=True)
    (output / 'bin').mkdir(exist_ok=True)
    for service in services:
        binary = build / 'services' / service / service
        shutil.copy2(binary, output / 'bin' / service)
        result = subprocess.run(['ldd', str(binary)], text=True, capture_output=True, check=True)
        if 'not found' in result.stdout:
            raise RuntimeError(result.stdout)
        for path in re.findall(r'(?:=>\s+|^\s*)(/[^\s]+)', result.stdout, re.M):
            source = Path(path)
            # В Ubuntu /lib — ссылка на /usr/lib: копируем в реальный каталог, сохраняя soname.
            target = output / 'libraries' / source.parent.resolve().relative_to('/') / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source.resolve(), target)


if __name__ == '__main__':
    package(Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3:])
