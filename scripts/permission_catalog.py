"""Регистрация каталога релиза и сверка с работающими образами без удаления назначений."""

import json
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent / 'openapi_gen'))
from permissions import catalog
import yaml


class UniqueLoader(yaml.SafeLoader):
    pass


def unique_mapping(loader, node, deep=False):
    result = {}
    for key, value in loader.construct_pairs(node, deep=deep):
        if key in result:
            raise ValueError('Повторяющийся ключ каталога прав: ' + str(key))
        result[key] = value
    return result


UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping)


def load(root):
    return catalog(Path(root), UniqueLoader)


def literal(value):
    return "'" + value.replace("'", "''") + "'"


def register_sql(owner, version, entries):
    document = [
        {'code': code, **value} for code, value in entries.items() if value['owner'] == owner
    ]
    return (
        'BEGIN; SELECT pg_advisory_xact_lock(824193); '
        'INSERT INTO permission_releases(owner,version,entries,active) VALUES ('
        + ','.join(
            [
                literal(owner),
                literal(version),
                literal(json.dumps(document, ensure_ascii=False)),
                'true',
            ]
        )
        + ') ON CONFLICT(owner,version) DO UPDATE SET active=true; COMMIT;'
    )


def reconcile_sql(owner, versions):
    if not versions:
        raise ValueError('Нельзя сверять каталог без работающих версий: ' + owner)
    values = ','.join(literal(version) for version in sorted(set(versions)))
    # Проверка полноты выполняется до изменения активности в той же транзакции.
    return f'''BEGIN; SELECT pg_advisory_xact_lock(824193);
DO $$ BEGIN IF (SELECT count(*) FROM permission_releases
 WHERE owner={literal(owner)} AND version IN ({values})) != {len(set(versions))}
 THEN RAISE EXCEPTION 'Нет каталога одной из работающих версий'; END IF; END $$;
UPDATE permission_releases SET active=(version IN ({values})) WHERE owner={literal(owner)};
COMMIT;'''


def execute(kubectl, sql):
    subprocess.run(
        [
            *kubectl,
            'exec',
            '-i',
            'postgres-0',
            '--',
            'psql',
            '-U',
            'broker',
            '-d',
            'organizations',
            '-v',
            'ON_ERROR_STOP=1',
        ],
        input=sql,
        text=True,
        check=True,
    )


def live_images(kubectl, service):
    result = subprocess.run(
        [*kubectl, 'get', 'pods', '-o', 'json'], capture_output=True, text=True, check=True
    )
    images = set()
    for pod in json.loads(result.stdout)['items']:
        if pod['metadata'].get('labels', {}).get('app.kubernetes.io/name') != service:
            continue
        if pod.get('status', {}).get('phase') in {'Succeeded', 'Failed'}:
            continue
        images.update(
            container['image']
            for container in pod['spec']['containers']
            if container['name'] == service.replace('_', '-')
        )
    return images


def prepare(kubectl, owner, image, entries):
    execute(kubectl, register_sql(owner, image, entries))


def finish(kubectl, owner):
    # rollout status не гарантирует завершение terminating-подов: ждём их дренирования.
    for attempt in range(46):
        result = subprocess.run(
            [*kubectl, 'get', 'pods', '-o', 'json'], capture_output=True, text=True, check=True
        )
        draining = [
            pod
            for pod in json.loads(result.stdout)['items']
            if pod['metadata'].get('labels', {}).get('app.kubernetes.io/name') == owner
            and pod['metadata'].get('deletionTimestamp')
        ]
        if not draining:
            execute(kubectl, reconcile_sql(owner, live_images(kubectl, owner)))
            return
        if attempt != 45:
            time.sleep(2)
    raise RuntimeError('Каталог сохранён: старые поды ещё завершаются: ' + owner)


def restore(kubectl, owner, image):
    execute(
        kubectl,
        f'''BEGIN; SELECT pg_advisory_xact_lock(824193);
DO $$ BEGIN IF NOT EXISTS(SELECT FROM permission_releases WHERE owner={literal(owner)} AND version={literal(image)})
THEN RAISE EXCEPTION 'У целевого образа нет зарегистрированного каталога прав'; END IF; END $$;
UPDATE permission_releases SET active=true WHERE owner={literal(owner)} AND version={literal(image)}; COMMIT;''',
    )
