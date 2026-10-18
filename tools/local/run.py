"""Запуск сервиса с ограниченным размером локальных логов и передачей сигналов."""

import logging.handlers
import signal
import subprocess
import sys

name, binary = sys.argv[1:]
handler = logging.handlers.RotatingFileHandler(
    f"/logs/{name}.jsonl", maxBytes=10 * 1024 * 1024, backupCount=2
)
handler.setFormatter(logging.Formatter("%(message)s"))
process = subprocess.Popen([binary, "-c", f"/local/{name}.yaml"], stderr=subprocess.PIPE)
for sig in (signal.SIGTERM, signal.SIGINT):
    signal.signal(sig, lambda received, frame: process.send_signal(received))
for line in process.stderr:
    sys.stderr.buffer.write(line)
    sys.stderr.buffer.flush()
    handler.emit(
        logging.LogRecord(
            name, logging.INFO, "", 0, line.decode(errors="replace").rstrip("\n"), (), None
        )
    )
handler.close()
sys.exit(process.wait())
