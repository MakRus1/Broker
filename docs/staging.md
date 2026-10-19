# Личный staging на K3s

Цель первого этапа: воспроизводимая установка Broker на одну VM с 4 vCPU/8 ГБ RAM,
PostgreSQL на постоянном диске, Keycloak и откатом сервисов. Версия существующего
кластера закреплена в `platform.yaml`: `v1.36.5+k3s1`. Канарейка и центральная
телеметрия подключаются следующим этапом; JSON-логи доступны через kubectl.

## Настройки

`platform.yaml` задаёт окружение, namespace, storage class и значения по умолчанию.
Любой сервис может задать потребности в `service.yaml`:

```yaml
deployment:
  replicas: 2
  resources:
    requests:
      cpu: 250m
      memory: 256Mi
    limits:
      memory: 512Mi
```

Итог: defaults окружения → deployment сервиса → services.<имя> окружения.
Словари resources объединяются по отдельным полям. CPU в целых ядрах задаётся
строкой (`'1'`), память — в Ki/Mi/Gi. request не может превышать limit.
Секции deployment не требуется для обычного сервиса.

BFF поддерживает только одну реплику, пока его сессии хранятся в памяти процесса.
Для него используется Recreate: при обновлении есть короткая пауза.
Его перезапуск, обновление и откат требуют повторного входа. Перед несколькими
репликами и канарейкой нужен отдельный этап работы с сессиями.

## Образы

Workflow **Service images** проверяет сборку, запуск бинарников и серверную валидацию
манифестов на отдельном временном K3s на PR. Там же запускается весь набор:
PostgreSQL, Keycloak, миграции и три сервиса с проверками готовности. Публикация выполняется вручную
из main через workflow_dispatch. Образы amd64 размещаются в GHCR:
`broker-configs`, `broker-broker`, `broker-admin-bff`. Artifact
`staging-release-<SHA>` содержит только `release.json`: полный git SHA и digest
трёх образов. Секреты и сгенерированные манифесты не публикуются.

При первой публикации проверь видимость пакетов. Для этого личного стенда можно
сделать их Public, если допустима публикация бинарников приложения. При приватных
пакетах нужно заранее создать в namespace imagePullSecret и добавить ссылку в
pod templates; текущий вариант рассчитан на доступные без авторизации образы.
Сам workflow не меняет видимость пакетов.

Локальная сборка для проверки без публикации:

```bash
python3 tools/deploy/build.py --revision "$(git rev-parse HEAD)"
```

C++ компилируется в закреплённом dev-образе. Runtime содержит бинарник, необходимые
динамические библиотеки, CA, timezone data и собранную админку; компилятор и исходники
туда не копируются. Сервис запускается от UID 10001 с read-only root filesystem.

## Подготовка в Ubuntu VM

```bash
sudo apt-get update
sudo apt-get install -y git python3-venv unzip
mkdir -p ~/projects
cd ~/projects
git clone https://github.com/MakRus1/Broker.git
cd Broker
python3 -m venv .venv-gen
.venv-gen/bin/pip install -r requirements.txt
```

Скачай artifact **Service images** нужного main-коммита в интерфейсе GitHub и перенеси
`release.json` в VM. В PowerShell, если файл находится в Downloads:

```powershell
multipass transfer "$env:USERPROFILE\Downloads\release.json" broker-staging:/home/ubuntu/release.json
```

В VM переключись на тот же коммит, что указан в release.json:

```bash
cd ~/projects/Broker
revision=$(.venv-gen/bin/python -c 'import json; print(json.load(open("/home/ubuntu/release.json"))["revision"])')
git fetch origin
git checkout "$revision"
.venv-gen/bin/python scripts/staging.py render --release ~/release.json
```

Команда проверяет совпадение checkout с revision релиза и входные файлы и сохраняет четыре фазы манифестов в `.staging/`.
Она не подключается к Kubernetes. Пароли генерируются один раз и сохраняются в
`.staging/credentials.json`; повторный render их не меняет. Каталог игнорируется
Git и Docker, файлы имеют режим 0600. Сохрани зашифрованную резервную копию этого
файла отдельно от VM: удаление credentials.json не меняет пароль существующей БД.

Не запускай render на другом компьютере с новыми паролями для уже существующей БД.
Повторный импорт realm не обновляет существующих пользователей Keycloak: смена
паролей и клиентских настроек выполняется отдельно через Keycloak.

## Установка

Для работы без root kubeconfig доступен только своему пользователю:

```bash
mkdir -p ~/.kube
sudo install -m 600 -o "$USER" -g "$(id -gn)" /etc/rancher/k3s/k3s.yaml ~/.kube/config
export PATH="/usr/local/bin:$PATH"
export KUBECONFIG="$HOME/.kube/config"
kubectl config current-context
```

Для новой установки K3s это обычно `default`. Передай фактическое имя явно:

```bash
.venv-gen/bin/python scripts/staging.py apply --context default
```

Порядок: namespace → PostgreSQL и Keycloak → ожидание готовности → транзакционные
миграции под advisory lock → Deployment сервисов → проверка готовности.
Применение использует server-side apply с владельцем полей broker-staging: повторная
установка восстанавливает декларативные настройки, включая изменения после ручного
отката. При неуспешной миграции сервисы не обновляются. Приложенные миграции не откатываются
автоматически, поэтому последующие изменения БД должны быть совместимы со старым
бинарником. Для ошибочной Job сначала посмотри её logs; удаление и повторный запуск
конкретной Job допустимы после исправления причины.

Постоянное хранилище: один PostgreSQL StatefulSet, один PVC 10Gi, отдельные БД
broker/configs/keycloak. Это staging-упрощение: один пользователь БД используется
всеми приложениями. local-path привязывает данные к этой VM, а не реплицирует их.
Размер PVC — запрос объёма, а не гарантированная дисковая квота local-path.
Не удаляй namespace/PVC для обновления сервисов. Перед изменениями БД делай pg_dump
и сохраняй копию за пределами VM; снапшот работающей VM не заменяет проверенный backup.

## Открыть админку из Windows

В двух терминалах Ubuntu запусти:

```bash
kubectl -n broker-staging port-forward --address 127.0.0.1 service/admin-bff 13080:8180
```

```bash
kubectl -n broker-staging port-forward --address 127.0.0.1 service/keycloak 18888:8080
```

Чтобы localhost Windows попадал в localhost VM, подготовь SSH-ключ **в PowerShell**:

```powershell
ssh-keygen -t ed25519 -f "$env:USERPROFILE\.ssh\broker-staging"
Get-Content "$env:USERPROFILE\.ssh\broker-staging.pub" | multipass exec broker-staging -- bash -c 'umask 077; mkdir -p ~/.ssh; cat >> ~/.ssh/authorized_keys'
multipass info broker-staging
```

Возьми IPv4 VM из последней команды и открой SSH-туннель, заменив `<VM_IP>`:

```powershell
ssh -i "$env:USERPROFILE\.ssh\broker-staging" -N -L 13080:127.0.0.1:13080 -L 18888:127.0.0.1:18888 ubuntu@<VM_IP>
```

Теперь админка — http://localhost:13080, Keycloak — http://localhost:18888.
Туннель и оба port-forward должны работать всё время сеанса. После перезапуска
пода соответствующий port-forward может потребоваться запустить снова.
Пароли `author`, `reviewer` и bootstrap admin находятся в credentials.json под
ключами `author-password`, `reviewer-password`, `admin-password`. Значения смотри
только в своём локальном файле. Автор и подтверждающий — разные пользователи.
HTTP и незащищённая cookie допустимы здесь только для localhost через SSH;
публичный доступ потребует HTTPS и изменения настроек авторизации.

## Проверки и откат

```bash
kubectl -n broker-staging get pods,pvc
kubectl -n broker-staging top pods
kubectl -n broker-staging logs deployment/configs --tail=100
kubectl -n broker-staging logs deployment/admin-bff --tail=100
```

При обновлении сохрани прежний release.json, скачай новый и повтори render/apply
в соответствующей ревизии репозитория, сохранив credentials.json.
Kubernetes сохраняет три предыдущих ReplicaSet. Конфиги и переменные именуются
по содержимому, поэтому откат возвращает и настройки предыдущей версии:

```bash
kubectl -n broker-staging rollout history deployment/configs
kubectl -n broker-staging rollout undo deployment/configs
kubectl -n broker-staging rollout status deployment/configs --timeout=300s
```

Это откат только выбранного сервиса. БД и Keycloak не откатываются этой командой.
Старые версионные ConfigMap/Secret и Job пока не удаляются автоматически, чтобы
не разрушить откат; очистка появится вместе с управлением историей релизов.
CD в кластер пока запускается явной командой; GitHub не получает kubeconfig.
Канарейка, автоматическое продвижение релиза и наблюдаемость — следующие этапы.
