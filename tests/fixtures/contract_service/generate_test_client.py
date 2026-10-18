"""Дополнительный контракт проверяет C++ enum/default/path/body/status без сервера."""
from dataclasses import replace
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'scripts/openapi_gen'))
from generate import load
from clients import render_client

out = Path(sys.argv[1])
out.parent.mkdir(parents=True, exist_ok=True)
operations = load(Path(__file__).parent)
get = next(op for op in operations if op.name == 'ItemsById')
post = next(op for op in operations if op.name == 'ItemsByIdCreate')
created, conflict = post.responses
operations.extend([
    replace(get, path='/multiple-successes', rel='multiple_successes', responses=(replace(created, status=200), *get.responses, conflict)),
    replace(get, path='/errors-only', rel='errors_only', responses=(conflict,)),
    replace(get, path='/empty-error', rel='empty_error', responses=(replace(get.responses[0], status=404),)),
])
out.write_text(render_client('client_test', 'contract', operations) + '\n' + render_client('client_test', 'configs_api', load(root / 'services/configs', public_only=True)))
