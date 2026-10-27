#!/usr/bin/env python3
"""Изолированное измерение реестра: реальные миграции, синтетические данные, EXPLAIN."""
import argparse
import json
from pathlib import Path
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
    args = parser.parse_args()
    if not 1000 <= args.rows <= 5000000 or not 1 <= args.repetitions <= 10:
        parser.error('Ожидается 1000–5000000 строк и 1–10 повторений')
    docker = shutil.which('docker')
    if docker is None:
        parser.error('Нужен docker в PATH')
    name = 'broker-registry-bench-' + uuid.uuid4().hex[:12]

    def run(*command, **kwargs):
        return subprocess.run([docker, *command], check=True, text=True,
                              capture_output=True, **kwargs).stdout

    def sql(text):
        return run('exec', '-i', name, 'psql', '-h', '127.0.0.1', '-XAt', '-U', 'postgres',
                   '-v', 'ON_ERROR_STOP=1', input=text)

    started = False
    try:
        run('run', '--rm', '-d', '--name', name,
            '-e', 'POSTGRES_HOST_AUTH_METHOD=trust', 'postgres:16.14-alpine')
        started = True
        for _ in range(100):
            ready = subprocess.run([docker, 'exec', name, 'pg_isready', '-h', '127.0.0.1', '-U', 'postgres'],
                                   capture_output=True)
            if ready.returncode == 0:
                break
            time.sleep(.1)
        else:
            raise RuntimeError('PostgreSQL не запустился')
        migrations = sorted((ROOT / 'services/transportations/postgresql/schemas/db_1').glob('V*.sql'))
        sql('\n'.join(path.read_text() for path in migrations))
        sql(f"""
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
        """)
        query = (ROOT / 'services/transportations/postgresql/queries/list_transportations.sql').read_text()
        prepare = 'PREPARE registry(uuid,text,text[],text[],boolean,text,boolean,text,uuid,integer,uuid,uuid) AS ' + query
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
        report = {'rows': args.rows, 'rows_largest_organization': args.rows * 4 // 5,
                  'repetitions': args.repetitions, 'postgres': sql('SELECT version();').strip(),
                  'scope': 'SQL only; warm cache; no HTTP, inherited routes or concurrent writers',
                  'indexes_bytes': {index: int(sql(f"SELECT pg_relation_size('{index}');")) for index in ['transportations_registry_number_idx', 'transportations_registry_search_idx']},
                  'cases': {}}
        for mode in ['force_custom_plan', 'force_generic_plan']:
            report['cases'][mode] = {}
            for case, parameters in cases.items():
                timings = []
                for _ in range(args.repetitions):
                    result = sql(f'SET plan_cache_mode={mode};\n' + prepare +
                                 f"\nEXPLAIN (ANALYZE, BUFFERS, FORMAT JSON) EXECUTE registry('{ORG}',{parameters},NULL,NULL);")
                    plan = json.loads(result[result.index('['):])[0]
                    timings.append(plan['Execution Time'])
                report['cases'][mode][case] = {
                    'median_ms': statistics.median(timings), 'min_ms': min(timings),
                    'max_ms': max(timings), 'explain': plan}
                print(f'{mode}: {case}: {statistics.median(timings):.3f} ms', flush=True)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    finally:
        if started:
            subprocess.run([docker, 'rm', '-f', name], capture_output=True, check=False)


if __name__ == '__main__':
    main()
