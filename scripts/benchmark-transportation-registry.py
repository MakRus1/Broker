#!/usr/bin/env python3
"""Изолированное измерение реестра: реальные миграции, синтетические данные, EXPLAIN."""
import argparse
import json
from pathlib import Path
import re
import shlex
import shutil
import statistics
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
ORG = '00000000-0000-0000-0000-000000000001'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rows', type=int, default=500000)
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument(
        '--query-renderer',
        required=True,
        help='Команда утилиты transportations_registry_query_dump; JSON запроса через stdin',
    )
    parser.add_argument('--baseline', type=Path, help='Прежний статический SQL для сравнения')
    args = parser.parse_args()
    if not 1000 <= args.rows <= 5000000 or not 1 <= args.repetitions <= 10:
        parser.error('Ожидается 1000–5000000 строк и 1–10 повторений')
    docker = shutil.which('docker')
    if docker is None:
        parser.error('Нужен docker в PATH')
    name = 'broker-registry-bench-' + uuid.uuid4().hex[:12]

    def run(*command, **kwargs):
        result = subprocess.run([docker, *command], text=True, capture_output=True, **kwargs)
        if result.returncode:
            raise RuntimeError(result.stderr)
        return result.stdout

    def sql(text):
        return run(
            'exec',
            '-i',
            name,
            'psql',
            '-h',
            '127.0.0.1',
            '-XAt',
            '-U',
            'postgres',
            '-v',
            'ON_ERROR_STOP=1',
            input=text,
        )

    started = False
    try:
        run(
            'run',
            '--rm',
            '-d',
            '--name',
            name,
            '-e',
            'POSTGRES_HOST_AUTH_METHOD=trust',
            'postgres:16.14-alpine',
        )
        started = True
        for _ in range(100):
            ready = subprocess.run(
                [docker, 'exec', name, 'pg_isready', '-h', '127.0.0.1', '-U', 'postgres'],
                capture_output=True,
            )
            if ready.returncode == 0:
                break
            time.sleep(0.1)
        else:
            raise RuntimeError('PostgreSQL не запустился')
        migrations = sorted(
            (ROOT / 'services/transportations/postgresql/schemas/db_1').glob('V*.sql')
        )
        sql('\n'.join(path.read_text() for path in migrations))
        sql(
            f"""
            INSERT INTO transportations (organization_id, id, shipment_id, ordinal,
                container_type_code, container_ownership, container_number, type, hazardous)
            SELECT CASE WHEN i <= {args.rows * 4 // 5} THEN '{ORG}'::uuid
                ELSE '00000000-0000-0000-0000-000000000002'::uuid END,
                md5(i::text)::uuid, md5((i % 10000)::text)::uuid, 1,
                CASE WHEN i % 3 = 0 THEN '20DC' ELSE '40HC' END, 'COC',
                CASE WHEN i % 10 = 0 THEN NULL
                    ELSE 'MSCU' || lpad((i % 250000)::text, 7, '0') END,
                CASE WHEN i % 10 = 0 THEN 'rail' ELSE 'container' END, i % 100 = 1
            FROM generate_series(1, {args.rows}) i;
            ANALYZE transportations;
        """
        )
        baseline = args.baseline.read_text() if args.baseline else None

        def render(request, parameters):
            query = subprocess.run(
                shlex.split(args.query_renderer),
                input=json.dumps(request),
                text=True,
                capture_output=True,
                check=True,
            ).stdout
            bindings = {
                int(index): kind for index, kind in re.findall(r'\$(\d+)::([A-Z]+(?:\[\])?)', query)
            }
            types = ','.join(bindings[index] for index in range(1, len(bindings) + 1))
            return (
                'PREPARE registry(' + types + ') AS ' + query.rstrip().rstrip(';') + ';',
                parameters,
            )

        cases = {
            'first_page': "NULL,NULL,NULL,NULL,'container_number',false,NULL,NULL,51",
            'hazardous': "NULL,NULL,NULL,true,'container_number',false,NULL,NULL,51",
            'combined': "'%010%',ARRAY['container'],ARRAY['40HC'],true,'container_number',false,NULL,NULL,51",
            'short_search': "'%1%',NULL,NULL,NULL,'container_number',false,NULL,NULL,51",
            'absent_search': "'%not-found%',NULL,NULL,NULL,'container_number',false,NULL,NULL,51",
            'deep_page': "NULL,NULL,NULL,NULL,'container_number',false,'mscu0200000','00000000-0000-0000-0000-000000000000',51",
            'container_type_order': "NULL,NULL,NULL,NULL,'container_type',false,NULL,NULL,51",
            'type_order': "NULL,NULL,NULL,NULL,'type',true,NULL,NULL,51",
        }
        requests = {
            'first_page': ({'sort': 'container_number'}, []),
            'hazardous': ({'sort': 'container_number', 'hazardous': True}, ['true']),
            'combined': (
                {
                    'sort': 'container_number',
                    'query': '010',
                    'types': ['container'],
                    'container_types': ['40HC'],
                    'hazardous': True,
                },
                ["'%010%'", "ARRAY['container']", "ARRAY['40HC']", 'true'],
            ),
            'short_search': ({'sort': 'container_number', 'query': '1'}, ["'%1%'"]),
            'absent_search': (
                {'sort': 'container_number', 'query': 'not-found'},
                ["'%not-found%'"],
            ),
            'deep_page': (
                {
                    'sort': 'container_number',
                    'cursor': {
                        'id': '00000000-0000-0000-0000-000000000000',
                        'scope': '0' * 64,
                        'value': 'mscu0200000',
                    },
                },
                ["'mscu0200000'", "'00000000-0000-0000-0000-000000000000'"],
            ),
            'container_type_order': ({'sort': 'container_type'}, []),
            'type_order': ({'sort': 'type', 'descending': True}, []),
        }
        built = {
            case: render({**request, 'limit': 50}, ','.join([f"'{ORG}'", *values, '51']))
            for case, (request, values) in requests.items()
        }
        report = {
            'rows': args.rows,
            'rows_largest_organization': args.rows * 4 // 5,
            'repetitions': args.repetitions,
            'postgres': sql('SELECT version();').strip(),
            'scope': 'SQL only; warm cache; no HTTP, inherited routes or concurrent writers',
            'indexes_bytes': {
                index: int(sql(f"SELECT pg_relation_size('{index}');"))
                for index in [
                    'transportations_registry_number_idx',
                    'transportations_registry_search_idx',
                ]
            },
            'cases': {},
        }
        results = {}
        variants = ['builder'] + (['baseline'] if baseline else [])
        for variant in variants:
            for mode in ['force_custom_plan', 'force_generic_plan']:
                label = variant + ':' + mode
                report['cases'][label] = {}
                for case, parameters in cases.items():
                    if variant == 'builder':
                        prepare, arguments = built[case]
                    else:
                        prepare = (
                            'PREPARE registry(uuid,text,text[],text[],boolean,text,'
                            'boolean,text,uuid,integer,uuid,uuid) AS ' + baseline
                        )
                        arguments = f"'{ORG}',{parameters},NULL,NULL"

                    actual = sql(
                        f'SET plan_cache_mode={mode};\n'
                        + prepare
                        + f'\nEXECUTE registry({arguments});'
                    )
                    if case in results and actual != results[case]:
                        raise RuntimeError(f'Различаются строки или порядок: {label}, {case}')
                    results[case] = actual
                    timings = []
                    for _ in range(args.repetitions):
                        result = sql(
                            f'SET plan_cache_mode={mode};\n'
                            + prepare
                            + f"\nEXPLAIN (ANALYZE, BUFFERS, FORMAT JSON) EXECUTE registry({arguments});"
                        )
                        plan = json.loads(result[result.index('[') :])[0]
                        timings.append(plan['Execution Time'])
                    report['cases'][label][case] = {
                        'median_ms': statistics.median(timings),
                        'min_ms': min(timings),
                        'max_ms': max(timings),
                        'explain': plan,
                        'query': prepare,
                        'arguments': arguments,
                    }
                    print(f'{label}: {case}: {statistics.median(timings):.3f} ms', flush=True)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    finally:
        if started:
            subprocess.run([docker, 'rm', '-f', name], capture_output=True, check=False)


if __name__ == '__main__':
    main()
