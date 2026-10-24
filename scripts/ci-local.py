#!/usr/bin/env python3
"""Ручной CI полного стенда: статусы и прогресс сборки каждые 30 секунд."""

from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def diagnostics(root=ROOT):
    compose = root / '.local/compose.yaml'
    if not compose.exists():
        print('Подготовка конфигурации стенда…', flush=True)
        return
    command = ['docker', 'compose', '-f', str(compose)]
    for args in [['ps', '--all'], ['logs', '--no-color', '--tail', '15', 'prepare']]:
        try:
            subprocess.run(command + args, timeout=10, check=False)
        except subprocess.TimeoutExpired:
            print('Docker не ответил за 10 секунд', flush=True)


def main():
    started = time.monotonic()
    with subprocess.Popen(['make', 'local-up'], cwd=ROOT) as process:
        while True:
            try:
                result = process.wait(timeout=30)
                diagnostics()
                raise SystemExit(result)
            except subprocess.TimeoutExpired:
                print(f'\nСтенд запускается: {int(time.monotonic() - started)} с', flush=True)
                diagnostics()


if __name__ == '__main__':
    main()
