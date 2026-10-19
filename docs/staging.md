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

### Одной командой

На Windows скачай или скопируй из этой ревизии репозитория
`scripts/staging-connect.ps1`. Если репозиторий уже есть на Windows, запусти из его корня:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\staging-connect.ps1
```

Bypass действует только для этого процесса. Скрипт требует Multipass и Windows OpenSSH,
сам определяет IPv4 работающей VM и запускает два kubectl port-forward внутри одного
SSH-сеанса. Kubeconfig и приватный ключ остаются на своих компьютерах.
Если Multipass возвращает несколько IPv4, скрипт выбирает первый доступный по TCP/22.
Недоступные адреса пропускаются с ограничением ожидания 1,5 секунды на адрес.
Адрес можно задать явно аргументом `-VmAddress 172.23.230.37`.
Если пробросы из предыдущей инструкции ещё работают, сначала останови их через Ctrl+C,
а также закрой прежний SSH-туннель: порты должны быть свободны.

После сообщений `Forwarding from` для обоих портов открой http://localhost:13080;
Keycloak доступен на http://localhost:18888. Терминал оставь открытым. Ctrl+C
завершает подключение и оба удалённых port-forward. После обновления подов или
обрыва связи запусти команду повторно. Ошибка занятого локального или удалённого
порта выводится в терминале; скрипт не завершает посторонние процессы.

Имя VM и путь к уже установленному ключу можно переопределить:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\staging-connect.ps1 -Instance broker-staging -IdentityFile "$env:USERPROFILE\.ssh\broker-staging"
```

### Однократная подготовка SSH-ключа

Если подключение с этим ключом уже работает, пропусти этот раздел.
В PowerShell создай ключ, если его ещё нет. Не перезаписывай существующий ключ:

```powershell
New-Item -ItemType Directory -Force "$env:USERPROFILE\.ssh" | Out-Null
if (-not (Test-Path "$env:USERPROFILE\.ssh\broker-staging")) {
    ssh-keygen -t ed25519 -f "$env:USERPROFILE\.ssh\broker-staging"
    if ($LASTEXITCODE -ne 0) { throw 'Не удалось создать SSH-ключ' }
}
multipass transfer "$env:USERPROFILE\.ssh\broker-staging.pub" broker-staging:/home/ubuntu/broker-staging.pub
```

Открой `multipass shell broker-staging`. В Ubuntu выполни следующий блок.
Он проверяет публичный ключ, сохраняет резервную копию и дописывает ключ с новой
строки, сохраняя прежние ключи Multipass. Передавать stdin через `multipass exec`
для этой операции не нужно.

```bash
python3 - <<'PYKEY'
from datetime import datetime, timezone
from pathlib import Path
import shutil
import subprocess

source = Path.home() / 'broker-staging.pub'
subprocess.run(['ssh-keygen', '-lf', str(source)], check=True)
key = source.read_text().strip()
if len(key.splitlines()) != 1 or not key.startswith('ssh-ed25519 '):
    raise SystemExit('Ожидается один публичный ключ ED25519. Ничего не изменено.')

directory = Path.home() / '.ssh'
directory.mkdir(mode=0o700, exist_ok=True)
target = directory / 'authorized_keys'
if not target.is_file():
    raise SystemExit('Нет authorized_keys Multipass. Сначала восстанови его.')
current = target.read_bytes()
stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
shutil.copy2(target, directory / f'authorized_keys.backup-{stamp}')
if key.encode() not in current.splitlines():
    with target.open('ab') as output:
        output.write(b'\n' + key.encode() + b'\n')
if not target.read_bytes().startswith(current):
    raise SystemExit('Ошибка проверки сохранности прежних ключей.')
directory.chmod(0o700)
target.chmod(0o600)
print('Ключ добавлен; прежние ключи сохранены.')
PYKEY
```

Не закрывая Ubuntu-сессию, в другом PowerShell проверь `multipass info broker-staging`,
затем запусти скрипт подключения. При первом SSH-входе сверь отпечаток ключа хоста:
в открытой Ubuntu-сессии его показывает
`sudo ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub`.
Не отключай проверку ключа хоста. Если Multipass перестал входить, сохрани открытую
Ubuntu-сессию и восстанови authorized_keys из созданной резервной копии.

Пароли `author`, `reviewer` и bootstrap admin находятся в credentials.json под
ключами `author-password`, `reviewer-password`, `admin-password`. Значения смотри
только в своём локальном файле. Автор и подтверждающий — разные пользователи.
HTTP и незащищённая cookie допустимы здесь только для localhost через SSH;
публичный доступ потребует HTTPS и изменения настроек авторизации.

## Проверки и откат

### Доступ с Mac по локальной сети

Адреса VM `172.23.*` относятся к виртуальной сети Hyper-V и могут быть недоступны
с другого компьютера. На Windows можно переслать один TCP-порт в SSH VM, разрешив
вход только с IP Mac. Перед настройкой узнай IPv4 Wi-Fi Windows через `ipconfig`
и IP Mac в настройках Wi-Fi → TCP/IP. Значения ниже — примеры, подставь фактические.

В PowerShell **от администратора**:

```powershell
$windowsIp = '192.168.1.146'
$macIp = '<MAC_IP>'
$vmIp = '172.23.230.37'
Test-NetConnection $vmIp -Port 22
```

Продолжай только при `TcpTestSucceeded: True` и после замены `<MAC_IP>`.
Проверь, что порт 2222 не занят другим приложением или правилом:

```powershell
Get-NetTCPConnection -LocalPort 2222 -State Listen -ErrorAction SilentlyContinue
netsh interface portproxy show v4tov4
```

Затем создай проброс и узкое правило брандмауэра:

```powershell
netsh interface portproxy add v4tov4 listenaddress=$windowsIp listenport=2222 connectaddress=$vmIp connectport=22
if ($LASTEXITCODE -ne 0) { throw 'Не удалось создать проброс SSH' }
New-NetFirewallRule -Name 'BrokerStagingSsh' -DisplayName 'Broker staging SSH from Mac' -Direction Inbound -Action Allow -Protocol TCP -LocalAddress $windowsIp -LocalPort 2222 -RemoteAddress $macIp -Profile Any
```

IP Helper (`iphlpsvc`) должен работать: проверь `Get-Service iphlpsvc`, при необходимости
запусти `Start-Service iphlpsvc`. Проброс не открывает Kubernetes API, PostgreSQL или
админку в локальной сети. С Mac проверь `nc -vz <WINDOWS_IP> 2222`.
Для SSH нужен собственный ключ Mac: его публичную часть добавь в authorized_keys VM
с сохранением существующих ключей по инструкции выше. Приватный ключ Windows не переносится.

В `~/.ssh/config` на Mac:

```sshconfig
Host broker-staging
    HostName <WINDOWS_IP>
    Port 2222
    User ubuntu
    IdentityFile ~/.ssh/broker-staging
    IdentitiesOnly yes
    ServerAliveInterval 15
    ServerAliveCountMax 3
```

Сверь отпечаток хоста VM при первом `ssh broker-staging`. При смене DHCP-адреса VM
обнови connectaddress проброса; при смене адресов Windows/Mac обнови также listenaddress,
правило брандмауэра и SSH-алиас. Старый проброс можно удалить:

```powershell
netsh interface portproxy delete v4tov4 listenaddress=$windowsIp listenport=2222
Remove-NetFirewallRule -Name 'BrokerStagingSsh'
```

Windows и VM должны оставаться включёнными без сна. После настройки SSH можно
выполнять render/apply и забирать копии прямо с Mac; kubeconfig остаётся в VM.

### Позже: доступ через интернет

Для доступа независимо от домашней сети установи Tailscale на Mac и прямо в Ubuntu VM.
Windows не должен быть промежуточным SSH-сервером. Инструкции поставщика:
[установка на Linux](https://tailscale.com/docs/install/linux),
[SSH к Linux VM](https://tailscale.com/docs/how-to/connect-ssh-linux-vm).
Оба устройства должны быть в одной tailnet. Достаточно обычного OpenSSH поверх
Tailscale; включать Tailscale SSH через `--ssh` не требуется.

В Ubuntu после установки:

```bash
sudo tailscale up
tailscale ip -4
```

Открой показанную ссылку и войди в тот же аккаунт, что на Mac. Ограничь доступ
к VM своими устройствами через политику tailnet. Публичные порты роутера открывать
не нужно. Для работы стенда Windows и VM должны оставаться включёнными без сна.

На Mac нужен собственный SSH-ключ: приватный ключ Windows переносить не нужно.
Его публичную часть добавь в Ubuntu способом из раздела подготовки ключа, сохраняя
ключ Multipass. Затем настрой алиас в `~/.ssh/config`, подставив Tailscale IP и путь
к своему ключу:

```sshconfig
Host broker-staging
    HostName <TAILSCALE_IP_VM>
    User ubuntu
    IdentityFile ~/.ssh/broker-staging
    IdentitiesOnly yes
    ServerAliveInterval 15
    ServerAliveCountMax 3
```

При первом `ssh broker-staging` сверь отпечаток хоста с Ubuntu. После этого SSH-команды
с Mac доступны и для диагностики, и для запуска render/apply в VM. Это ручной удалённый
деплой; автоматический CD из GitHub пока не настроен. Kubeconfig остаётся внутри VM.

### Резервная копия вне VM

Из репозитория на Mac, после настройки SSH:

```bash
python3 scripts/staging_backup.py create --host broker-staging --context default \
  --output "$HOME/Documents/BrokerBackups/$(date -u +%Y%m%dT%H%M%SZ)"
```

Для запуска внутри VM опусти `--host` и задай `KUBECONFIG`; такую копию затем нужно
отдельно перенести за пределы VM. Команда создаёт custom-format дампы broker, configs
и keycloak, manifest с SHA-256 и временем создания. Готовый каталог появляется только
после успешной выгрузки всех трёх БД; существующая копия не перезаписывается.
Файлы передаются бинарным потоком без SSH TTY и без PowerShell-перенаправления.

Дампы содержат данные пользователей и Keycloak: храни их на защищённом диске,
не добавляй в Git. Пароли ролей PostgreSQL и credentials.json не входят в копию:
отдельная резервная копия credentials.json по-прежнему нужна. Каждая БД выгружается
из своего согласованного снимка; общего снимка сразу трёх БД нет. Расписание,
ротация, шифрование архива и отправка в объектное хранилище пока не автоматизированы.

Проверь восстановление на Mac с запущенным Docker, указав фактический каталог:

```bash
python3 scripts/staging_backup.py verify "$HOME/Documents/BrokerBackups/<TIMESTAMP>"
```

Команда проверяет контрольные суммы и восстанавливает все три БД через pg_restore
с `--exit-on-error` в отдельный временный PostgreSQL из закреплённого образа.
У него нет внешних портов и сети; данные находятся в tmpfs и удаляются вместе
с контейнером при завершении. Владельцы и ACL при этой проверке не восстанавливаются.
Проверка подтверждает читаемость и восстановимость дампов, но не заменяет проверку
запуска приложений после полного аварийного восстановления. Рабочий кластер команда
verify не использует. При росте БД для проверки потребуется достаточно RAM в Docker.

Сначала сделай копию и проверь восстановление, затем обновляй сервисы.

### Диагностика и откат сервисов

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
