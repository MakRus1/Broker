#!/usr/bin/env python3
"""Измерить SQL поставок или контрагентов в отдельном PostgreSQL без рабочих данных."""
import argparse
import json
from pathlib import Path
import re
import shlex
import shutil
import hashlib
import statistics
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
ORG = '00000000-0000-0000-0000-000000000001'


def workload(service, rows):
    organization = f"CASE WHEN i <= {rows * 4 // 5} THEN '{ORG}'::uuid ELSE '00000000-0000-0000-0000-000000000002'::uuid END"
    if service == 'shipments':
        seed = f"""INSERT INTO shipments (organization_id,id,number,customer_id,shipment_date,
            status,cargo_description,route_segments)
            SELECT {organization},md5(i::text)::uuid,'S-'||i,
                md5((i%1000)::text)::uuid,DATE '2025-01-01'+(i%730),
                CASE WHEN i%4=0 THEN 'В работе' ELSE 'Новая' END,
                CASE WHEN i%1000=1 THEN 'Редкий груз' WHEN i%3=0 THEN 'Сталь' ELSE 'Трубы' END,'{{}}'
            FROM generate_series(1,{rows}) i;"""
        types = 'uuid,text,text,uuid,text[],date,date,text,boolean,text,date,uuid,integer'
        cases = {
            'date_first': "NULL,NULL,NULL,NULL,NULL,NULL,'shipment_date',true,NULL,NULL,NULL,51",
            'number_first': "NULL,NULL,NULL,NULL,NULL,NULL,'number',false,NULL,NULL,NULL,51",
            'deep_date': "NULL,NULL,NULL,NULL,NULL,NULL,'shipment_date',true,NULL,'2025-10-01','00000000-0000-0000-0000-000000000000',51",
            'rare_cargo': "NULL,'%редкий%',NULL,NULL,NULL,NULL,'shipment_date',true,NULL,NULL,NULL,51",
            'combined': "NULL,NULL,md5('42')::uuid,ARRAY['Новая'],'2026-01-01','2026-01-31','shipment_date',true,NULL,NULL,NULL,51",
            'cargo_sort': "NULL,NULL,NULL,NULL,NULL,NULL,'cargo_description',false,NULL,NULL,NULL,51",
        }
        indexes = [
            'shipments_registry_date_idx',
            'shipments_registry_number_idx',
            'shipments_registry_search_idx',
        ]
    else:
        seed = f"""INSERT INTO counterparties (organization_id,id,name,legal_name,tax_id,country_code,archived,roles)
            SELECT {organization},md5(i::text)::uuid,
                CASE WHEN i%2=1 THEN 'Alpha-' ELSE 'Beta-' END||(i%50000),'Компания '||i,
                (1000000000::bigint+i)::text,CASE WHEN i%3=0 THEN 'RU' ELSE 'CN' END,
                i%10=0,CASE WHEN i%2=1 THEN ARRAY['customer'] ELSE ARRAY['carrier'] END
            FROM generate_series(1,{rows}) i;"""
        types = 'uuid,text,text,text,text,integer,text,boolean,text,uuid,uuid[]'
        cases = {
            'name_first': "NULL,NULL,'active',NULL,51,'name',false,NULL,NULL",
            'deep_name': "NULL,NULL,'active',NULL,51,'name',false,'alpha-30000','00000000-0000-0000-0000-000000000000'",
            'rare_search': "'%12345%',NULL,'active',NULL,51,'name',false,NULL,NULL",
            'combined': "'%alpha%','customer','active','RU',51,'name',false,NULL,NULL",
            'country_sort': "NULL,NULL,'all',NULL,51,'country_code',false,NULL,NULL",
        }
        cases = {name: parameters + ',NULL' for name, parameters in cases.items()}
        indexes = ['counterparties_registry_name_idx', 'counterparties_registry_search_idx']
    return seed, types, cases, indexes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--service', choices=['shipments', 'counterparties'], required=True)
    parser.add_argument('--rows', type=int, default=50000)
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument(
        '--query-renderer',
        required=True,
        help='Команда утилиты SERVICE_registry_query_dump; JSON через stdin',
    )
    parser.add_argument('--baseline', type=Path, help='Прежний статический SQL для сравнения')
    args = parser.parse_args()
    if not 1000 <= args.rows <= 5000000 or not 1 <= args.repetitions <= 10:
        parser.error('Ожидается 1000–5000000 строк и 1–10 повторений')
    docker = shutil.which('docker')
    if docker is None:
        parser.error('Нужен docker в PATH')
    container = 'broker-registry-bench-' + uuid.uuid4().hex[:12]
    started = False

    def run(*command, **kwargs):
        result = subprocess.run([docker, *command], text=True, capture_output=True, **kwargs)
        if result.returncode:
            raise RuntimeError(result.stderr or result.stdout)
        return result.stdout

    def sql(text):
        return run(
            'exec',
            '-i',
            container,
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

    try:
        run(
            'run',
            '--rm',
            '-d',
            '--name',
            container,
            '-e',
            'POSTGRES_HOST_AUTH_METHOD=trust',
            'postgres:16.14-alpine',
        )
        started = True
        for _ in range(100):
            ready = subprocess.run(
                [docker, 'exec', container, 'pg_isready', '-h', '127.0.0.1', '-U', 'postgres'],
                capture_output=True,
            )
            if ready.returncode == 0:
                break
            time.sleep(0.1)
        else:
            raise RuntimeError('PostgreSQL не запустился')
        migrations = sorted(
            (ROOT / f'services/{args.service}/postgresql/schemas/db_1').glob('V*.sql')
        )
        sql('\n'.join(path.read_text() for path in migrations))
        seed, types, cases, indexes = workload(args.service, args.rows)
        sql(seed + f'\nANALYZE {args.service};')
        if args.service == 'shipments':
            requests = {
                'date_first': ({'sort': 'shipment_date', 'descending': True}, []),
                'number_first': ({'sort': 'number'}, []),
                'deep_date': (
                    {
                        'sort': 'shipment_date',
                        'descending': True,
                        'cursor': {
                            'id': '00000000-0000-0000-0000-000000000000',
                            'scope': '0' * 64,
                            'date_value': '2025-10-01',
                        },
                    },
                    ["'2025-10-01'", "'00000000-0000-0000-0000-000000000000'"],
                ),
                'rare_cargo': (
                    {'sort': 'shipment_date', 'descending': True, 'cargo_query': 'редкий'},
                    ["'%редкий%'"],
                ),
                'combined': (
                    {
                        'sort': 'shipment_date',
                        'descending': True,
                        'customer_id': hashlib.md5(b'42').hexdigest(),
                        'statuses': ['Новая'],
                        'date_from': '2026-01-01',
                        'date_to': '2026-01-31',
                    },
                    ["md5('42')::uuid", "ARRAY['Новая']", "'2026-01-01'", "'2026-01-31'"],
                ),
                'cargo_sort': ({'sort': 'cargo_description'}, []),
            }
            # UUID в JSON использует канонический формат.
            requests['combined'][0]['customer_id'] = str(
                uuid.UUID(requests['combined'][0]['customer_id'])
            )
        else:
            requests = {
                'name_first': ({'sort': 'name'}, ['false']),
                'deep_name': (
                    {
                        'sort': 'name',
                        'cursor': {
                            'id': '00000000-0000-0000-0000-000000000000',
                            'scope': '0' * 64,
                            'value': 'alpha-30000',
                        },
                    },
                    ['false', "'alpha-30000'", "'00000000-0000-0000-0000-000000000000'"],
                ),
                'rare_search': ({'sort': 'name', 'query': '12345'}, ["'%12345%'", 'false']),
                'combined': (
                    {'sort': 'name', 'query': 'alpha', 'role': 'customer', 'country_code': 'RU'},
                    ["'%alpha%'", "'customer'", 'false', "'RU'"],
                ),
                'country_sort': ({'sort': 'country_code', 'status': 'all'}, []),
            }
        built = {}
        for name, (request, values) in requests.items():
            query = subprocess.run(
                shlex.split(args.query_renderer),
                input=json.dumps({**request, 'limit': 50}),
                text=True,
                capture_output=True,
                check=True,
            ).stdout
            bindings = {
                int(index): kind for index, kind in re.findall(r'\$(\d+)::([A-Z]+(?:\[\])?)', query)
            }
            parameter_types = ','.join(bindings[index] for index in range(1, len(bindings) + 1))
            built[name] = (
                f'PREPARE registry({parameter_types}) AS ' + query.rstrip().rstrip(';') + ';',
                ','.join([f"'{ORG}'", *values, '51']),
            )
        baseline = args.baseline.read_text() if args.baseline else None
        report = {
            'service': args.service,
            'rows': args.rows,
            'rows_largest_organization': args.rows * 4 // 5,
            'repetitions': args.repetitions,
            'postgres': sql('SELECT version();').strip(),
            'scope': 'SQL only; warm cache; no HTTP or concurrent writers',
            'indexes_bytes': {
                index: int(sql(f"SELECT pg_relation_size('{index}');")) for index in indexes
            },
            'cases': {},
        }
        rows_by_case = {}
        for variant in ['builder'] + (['baseline'] if baseline else []):
            for mode in ['force_custom_plan', 'force_generic_plan']:
                label = variant + ':' + mode
                report['cases'][label] = {}
                for name, parameters in cases.items():
                    if variant == 'builder':
                        prepared, arguments = built[name]
                    else:
                        prepared = f'PREPARE registry({types}) AS ' + baseline
                        arguments = f"'{ORG}',{parameters}"
                    actual = sql(
                        f'SET plan_cache_mode={mode};\n'
                        + prepared
                        + f'\nEXECUTE registry({arguments});'
                    )
                    if name in rows_by_case and actual != rows_by_case[name]:
                        raise RuntimeError(f'Различаются строки или порядок: {label}, {name}')
                    rows_by_case[name] = actual
                    timings = []
                    for _ in range(args.repetitions):
                        result = sql(
                            f'SET plan_cache_mode={mode};\n'
                            + prepared
                            + f"\nEXPLAIN (ANALYZE, BUFFERS, FORMAT JSON) EXECUTE registry({arguments});"
                        )
                        plan = json.loads(result[result.index('[') :])[0]
                        timings.append(plan['Execution Time'])
                    median = statistics.median(timings)
                    report['cases'][label][name] = {
                        'median_ms': median,
                        'min_ms': min(timings),
                        'max_ms': max(timings),
                        'explain': plan,
                        'query': prepared,
                        'arguments': arguments,
                    }
                    print(f'{args.service}: {label}: {name}: {median:.3f} ms', flush=True)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    finally:
        if started:
            subprocess.run([docker, 'rm', '-f', container], capture_output=True, check=False)


if __name__ == '__main__':
    main()
