"""Дополнительный контракт проверяет C++ enum/default/path/body/status без сервера."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'scripts/openapi_gen'))
from generate import load
from clients import render_client

out = Path(sys.argv[1])
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(render_client('client_test', 'contract', load(Path(__file__).parent)))
