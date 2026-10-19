"""Сквозная проверка filelog → Collector → Loki на диагностических записях."""

import datetime
import json
from pathlib import Path
import subprocess
import time
import urllib.parse
import urllib.request
import uuid

ROOT = Path(__file__).resolve().parents[2]
LEVELS = {
    'TRACE': ('TRACE', '1'),
    'DEBUG': ('DEBUG', '5'),
    'INFO': ('INFO', '9'),
    'WARNING': ('WARN', '13'),
    'ERROR': ('ERROR', '17'),
    'CRITICAL': ('FATAL', '21'),
}


def read(path, **parameters):
    query = urllib.parse.urlencode(parameters)
    with urllib.request.urlopen(
        'http://127.0.0.1:13100/loki/api/v1/' + path + ('?' + query if query else ''),
        timeout=5,
    ) as response:
        return json.load(response)['data']


def verify(streams, marker, timestamp_ns):
    found = set()
    for stream in streams:
        labels = stream['stream']
        for entry in stream['values']:
            body = json.loads(entry[1])
            if body.get('check_id') != marker:
                continue
            level = body['level']
            severity_text, severity_number = LEVELS[level]
            assert labels['level'] == level, 'Запись попала в stream другого уровня'
            assert labels['severity_text'] == severity_text, 'Некорректный SeverityText'
            assert labels['severity_number'] == severity_number, 'Некорректный SeverityNumber'
            assert int(entry[0]) == timestamp_ns, 'Вместо времени события используется время чтения'
            found.add(level)
    return found == set(LEVELS)


def check():
    marker = 'loki-smoke-' + uuid.uuid4().hex
    # Время заметно отличается от момента чтения, чтобы проверить timestamp parser.
    timestamp_ns = (time.time_ns() // 1000 - 30_000_000) * 1000
    timestamp = (
        datetime.datetime.fromtimestamp(
            timestamp_ns // 1_000_000_000,
            datetime.timezone.utc,
        )
        .replace(microsecond=(timestamp_ns % 1_000_000_000) // 1000)
        .isoformat()
        .replace('+00:00', 'Z')
    )
    records = [
        json.dumps(
            {
                'timestamp': timestamp,
                'level': level,
                'text': 'Проверка уровня Loki',
                'check_id': marker,
                'trace_id': uuid.uuid4().hex,
                'span_id': uuid.uuid4().hex[:16],
            },
            ensure_ascii=False,
        )
        for level in LEVELS
    ]
    writer = (
        'import sys; '
        'f=open("/logs/diagnostics.jsonl", "a"); '
        'f.write(sys.stdin.read()); f.close()'
    )
    subprocess.run(
        [
            'docker',
            'compose',
            '-f',
            str(ROOT / '.local/compose.yaml'),
            'run',
            '--rm',
            '--no-deps',
            '-T',
            '--entrypoint',
            'python3',
            'diagnostics',
            '-c',
            writer,
        ],
        input='\n'.join(records) + '\n' + marker + ': unstructured startup message\n',
        text=True,
        check=True,
    )
    query = '{service_name="diagnostics",level=~"' + '|'.join(LEVELS) + '"} |= "' + marker + '"'
    deadline = time.monotonic() + 90
    last = None
    while time.monotonic() < deadline:
        try:
            streams = read('query_range', query=query, limit=100)['result']
            if verify(streams, marker, timestamp_ns):
                break
        except (OSError, AssertionError, KeyError, ValueError) as error:
            last = error
        time.sleep(1)
    else:
        raise RuntimeError(f'Loki не подтвердил уровни и время события: {last}')
    labels = read('labels')
    assert 'level' in labels, 'level недоступен в селекторе labels'
    assert not {'trace_id', 'span_id', 'check_id'} & set(labels), 'Динамические ID попали в индекс'
    # Фильтр после selector работает и с индексом новых записей, и со старой metadata.
    selected = read(
        'query_range',
        query='{service_name="diagnostics"} | level="ERROR" |= "' + marker + '"',
        limit=100,
    )['result']
    assert sum(len(item['values']) for item in selected) == 1, 'Фильтр ERROR не изолировал запись'
    plain = read(
        'query_range',
        query='{service_name="diagnostics"} |= "' + marker + ': unstructured startup message"',
        limit=100,
    )['result']
    assert sum(len(item['values']) for item in plain) == 1, 'Потеряна неструктурированная строка'
    print(
        'Loki: 6 уровней, severity, время события, фильтр ERROR и обычная строка проверены',
        flush=True,
    )


if __name__ == '__main__':
    check()
