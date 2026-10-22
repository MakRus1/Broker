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

### Обновление одной командой

После настройки `ssh broker-staging` скачай `release.json` из успешного запуска
**Service images**, опубликованного из main. Переносить его вручную в VM больше
не требуется. Из репозитория на Mac:

```bash
python3 scripts/staging_deploy.py --release ~/Downloads/release.json --context default
```

Mac только запускает процесс по SSH и показывает результат. Дампы не попадают
ни на Mac, ни на GitHub runner. Docker на вызывающей машине не нужен.
На VM нужны git, kubectl, существующие `.venv-gen`, `.staging/credentials.json`
и чистый checkout `~/projects/Broker`. Команда обновляет установленный стенд.

Порядок: блокировка другого удалённого деплоя → проверка чистого checkout и
принадлежности коммита origin/main → копия трёх БД на диске VM → восстановление
в отдельном временном PostgreSQL pod → переключение checkout на SHA релиза →
установка закреплённых Python-зависимостей → render/apply → готовность сервисов.
Ошибка бэкапа или восстановления останавливает процесс до checkout и apply.

Копии хранятся по умолчанию в `~/.local/share/broker-staging/backups/` **на VM**.
После успешного деплоя остаются последние семь проверенных копий; повреждённые,
непроверенные и посторонние каталоги автоматически не удаляются.
Путь и количество задаются через `--backup-root` и `--keep-backups`.
Дампы содержат данные пользователей и Keycloak; credentials.json сохраняется
отдельно и по-прежнему нужен для аварийного восстановления.

Проверочный pod использует закреплённый образ PostgreSQL, собственный emptyDir,
лимит памяти 512Mi, не монтирует PVC рабочей БД и не открывает TCP-listener.
Дампы передаются в него через kubectl exec; проверка выполняется с --exit-on-error,
без восстановления владельцев и ACL. Pod и временные данные удаляются при завершении
проверки. При аварийном обрыве процесса проверь pod с префиксом backup-check- и
удали оставшийся временный pod после диагностики. Места на диске VM должно хватать
на сохраняемые копии и ещё одну распакованную копию трёх БД для проверки.

Блокировка действует между staging_deploy.py; ручной apply параллельно запускать
нельзя. Во время копирования приложения продолжают принимать запросы: это
не остановка записей и не общий снимок нескольких БД.

История попыток хранится в VM в `.staging/releases/`: целевой release.json,
предыдущие образы и checkout в before.json, путь к бэкапу и статус success/failed.
Те же сведения о предыдущем состоянии есть рядом с копией.
Исходный credentials.json сохраняется. Digest ограничены тремя репозиториями
сервисов Broker; принадлежность SHA main не заменяет проверку CI.

При ошибке миграции или готовности автоматического отката нет: часть ресурсов
может быть уже обновлена. Команда завершается ошибкой, копия сохраняется.
При обрыве SSH обновление может продолжаться в VM: сначала проверь историю и поды,
затем решай вопрос с откатом. Для возвращения сервисов используй rollout undo ниже;
миграции и данные БД не откатываются.

Для другого SSH-алиаса или checkout предусмотрены `--host` и `--repository`.
В будущем CD запускает тот же сценарий на сервере: runner передаёт релиз и получает
статус, а резервные копии остаются в серверном хранилище. Автоматический workflow
доступа в кластер пока не настроен. Сейчас копии лежат на том же SSD, что и VM;
отдельное внешнее хранилище и периодическое расписание будут следующим этапом.
Для ручного копирования вне VM остаётся staging_backup.py create из предыдущего раздела.

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

## Постоянный HTTPS-доступ из локальной сети

В `platform.yaml` секция `lan` задаёт имена admin/auth/grafana и объём томов телеметрии.
Режим включается явно один раз на сервере, после установки релиза с поддержкой LAN:

```bash
export KUBECONFIG="$HOME/.kube/config"
cd ~/projects/Broker
.venv-gen/bin/python scripts/staging_lan.py init --context default
```

Команда создаёт локальный CA и сертификат сервера, Secret TLS, доверенный CA bundle
для приложений и отдельный пароль Grafana. Существующие ключи и пароль сохраняются.
Приватные файлы находятся в `.staging/lan/`, не попадают в Git и образы.
CA действует 10 лет, сертификат сервера — 397 дней; автоматического продления пока нет.
Повторный init проверяет SAN и подпись, но не перевыпускает сертификаты.
Файл `.staging/lan/enabled` включает профиль в последующих render/apply.
Сохрани защищённую копию `.staging/lan` отдельно: дампы БД этих файлов не содержат.

После init выполни обычный render/apply **релиза с поддержкой LAN**. Он включает:

- HTTPS Ingress в Traefik для трёх имён, без публикации API Kubernetes и PostgreSQL;
- secure-cookie BFF, HTTPS-обмен токенами с Keycloak и доверие к локальному CA;
- обновление redirect URI существующего клиента Keycloak (импорт realm этого не делает);
- Grafana, Prometheus, Loki, Tempo, Collector с постоянными томами;
- автоматические сервисные дашборды, сбор JSON stdout, метрик и OTLP-трасс.

Служебные API телеметрии доступны только внутри кластера. Grafana требует входа:
пользователь `admin`, пароль в `.staging/lan/grafana-password`. Анонимный доступ и
регистрация отключены. Логи и трейсы хранятся 24 часа, метрики — до 3 дней/4GB.
local-path не обеспечивает жёсткие дисковые квоты: контролируй свободное место.
Сборщик логов читает `/var/log/pods` read-only от root без Kubernetes API token.

На Keenetic/AdGuard Home настрой DNS-перезаписи трёх имён на LAN-IP Windows.
При использовании AdGuard Home на порту 53 записей `ip host` Keenetic может быть
недостаточно — ответы должен формировать фактический DNS-сервер.
Зафиксируй IP Windows через DHCP reservation. Устройства должны использовать этот DNS.

Windows, PowerShell от администратора (подставь текущий доступный IP VM):

```powershell
$windowsIp = '192.168.1.146'
$vmIp = '172.23.230.37'
Test-NetConnection $vmIp -Port 443
netsh interface portproxy add v4tov4 listenaddress=$windowsIp listenport=443 connectaddress=$vmIp connectport=443
New-NetFirewallRule -Name 'BrokerStagingHttps' -DisplayName 'Broker staging HTTPS from LAN' -Direction Inbound -Action Allow -Protocol TCP -LocalAddress $windowsIp -LocalPort 443 -RemoteAddress 192.168.1.0/24 -Profile Any
```

Порт 443 на Windows должен быть свободен; правило не открывает доступ из интернета.
При смене IP VM обнови connectaddress, как для SSH. На устройства за пределами
основной подсети это правило не распространяется; изоляция гостевого Wi-Fi сохраняется.

Перед первым входом установи **только публичный** `.staging/lan/ca.crt` как доверенный
корневой сертификат на каждом устройстве. Сверь SHA-256 через `openssl x509 -in ca.crt
-noout -fingerprint -sha256`. Приватный `ca.key` передавать на клиентские устройства нельзя.
Установка CA — отдельное явное действие пользователя; не обходи предупреждения браузера.

Адреса:

- `https://admin.broker.home.arpa` — админка конфигов
- `https://web.broker.home.arpa` — пользовательский интерфейс
- `https://auth.broker.home.arpa`
- `https://grafana.broker.home.arpa`

После перехода старые localhost-сеансы требуют повторного входа. Обычный деплой
продолжает учитывать профиль LAN; возвращение к релизу до поддержки LAN запрещено
командой удалённого обновления. Для изменения доменов сначала подготовь новый сертификат,
DNS и настройки Keycloak; простое изменение DNS не меняет origin приложений.

## Выборочное обновление и CD

CD находится в приватном [Broker-deploy](https://github.com/MakRus1/Broker-deploy).
Runner работает службой внутри Ubuntu VM и зарегистрирован только в этом репозитории.
Публичные PR Broker выполняются на GitHub runners и не получают доступ к staging.

После слияния изменений запусти `Service images` из `main` и дождись успеха.
В `Broker-deploy → Actions` выбери workflow нужного сервиса, `all` или
`infrastructure`. Пустое поле релиза выбирает последнюю успешную публикацию
Service images из main; ссылка или ID позволяют указать другую версию.
Общая инфраструктура обновляется отдельным workflow.
`check_only` проверяет происхождение релиза и образы без изменения кластера.
Первый релиз для CD должен включать поддержку выборочного обновления.

Образы берутся по digest из публичного GHCR; на ноутбуке ничего не компилируется.
CD проверяет успешную публикацию из main и OCI revision каждого образа.
Бэкапы трёх БД, проверка восстановления и хранение семи проверенных копий выполняются
на сервере. Бэкапы, credentials.json и ключи TLS не передаются GitHub.
Ноутбук и VM должны быть включены, runner требует исходящий HTTPS к GitHub/GHCR.
Дополнительных входящих портов для CD нет.

Ручное обновление с Mac также поддерживает выбор сервисов:

```bash
.venv-gen/bin/python scripts/staging_deploy.py --release ~/release.json --context default --service configs
```

`--service` можно повторить. Без него удалённое обновление выбирает все три сервиса,
но не инфраструктуру. `--infrastructure` включает её явно, `--infra-only` обновляет
только инфраструктуру. Команда первоначальной установки `staging.py apply` без
флагов по-прежнему применяет весь staging.

У каждого объекта сервисов и миграций есть метка `broker/service`: выборочное
обновление применяет только выбранные объекты и ждёт только их миграции/rollout.
Невыбранные Deployments, конфиги и миграции не применяются. Проверка телеметрии общая.
Следи за совместимостью API между разными версиями сервисов.

`.staging/deployed.json` содержит фактические image digest, ревизию, поколения
Deployment и readiness каждого сервиса. После начала применения состояние читается
из Kubernetes и сохраняется даже при ошибке: частичный деплой не изображается единым
успешным релизом. История `.staging/releases/` хранит выбор сервисов, состояние
до/после, релиз и результат. В Actions фактические версии доступны в summary.
Автоматического отката БД нет; при ошибке используй историю и сохранённый бэкап.

### Новый сервис без ручного реестра деплоя

`services/<name>/service.yaml` определяет список сервисов для CMake, сборки образов,
упаковки runtime, staging и телеметрии. `platform.yaml.services` содержит только
необязательные переопределения ресурсов; для нового сервиса действуют defaults
и его `service.yaml.deployment`. Неизвестные имена переопределений отклоняются.

В Broker-deploy синхронизация каталога main каждые полчаса генерирует отдельные
workflows; для немедленного обновления запусти `Sync service workflows` вручную.
Синхронизация не запускает деплой. Сначала опубликуй образы из main, затем обнови
инфраструктуру для регистрации сервиса в мониторинге и разверни сам сервис.
Новый PostgreSQL-сервис создаёт свою БД перед миграциями, в том числе на существующем
томе. Бэкап и проверка восстановления включают все пользовательские БД staging,
обнаруженные в PostgreSQL; старые копии из трёх БД также поддерживаются.

При CD каталог сервисов читается из выбранной ревизии публикации. Явно выбранный
старый релиз не получает список сервисов из нового main. Автоматическое удаление
workflows не удаляет рабочие Deployment, PVC или БД: снятие сервиса с эксплуатации
выполняется отдельно.

## Откат одного сервиса без миграций

Обычный деплой сохраняет `before-services.json` и `after-services.json` в своей
`.staging/releases/<record>/`: полные Deployment/Service, ссылки на immutable
конфигурацию и признак завершённого здорового rollout. Значения секретов в summary
и историю Actions не выводятся. Сами снимки находятся только на сервере.

После слияния поддержки отката опубликуй Service images из main и выполни один
обычный деплой. Он сохранит и состояние до обновления. Старые записи, в которых
есть лишь SHA/образ/часть ссылок, не считаются полными снимками и не используются
для восстановления наугад.

В Broker-deploy генерируются отдельные `Rollback <service>`:

1. `operation=history` показывает текущую версию и доступные ID в summary.
2. `operation=rollback`, `check_only=true` показывает план восстановления.
3. Пустой `target` выбирает предыдущее отличающееся рабочее состояние. Для явного
   выбора вставь ID `<record>:before` или `<record>:after` из истории.
4. Убери `check_only`, чтобы применить выбранное состояние.

Откат меняет только Deployment и Service одного сервиса, включая образ, ресурсы,
pod template и ссылки на сохранённые конфиги. Общая инфраструктура и соседние
сервисы не применяются. Миграции, восстановление бэкапа и SQL не запускаются.
Checkout репозитория не переводится на старый SHA: инструменты управления остаются
актуальными. Изменения схемы БД должны быть совместимы со старым кодом; исправление
миграций выполняется отдельно новой миграцией.

До применения проверяются существование конфигураций и immutable для сервисных
config/vars. Общие настройки (например CA) остаются текущими. Не удаляй старые
immutable ConfigMap/Secret, пока нужен откат к ссылающимся на них снимкам.
Снимок после неуспешной операции не предлагается как рабочий; здоровое состояние
до частично неудачного деплоя доступно для восстановления.

Откат использует ту же блокировку, что и деплой. После применения выполняются
rollout и проверка телеметрии; результат и фактическое состояние сохраняются
также при частичном сбое. Автоматического каскадного отката нет. BFF с in-memory
сессиями при смене pod потребует повторного входа.

Для ручного запуска **на Ubuntu VM**:

```bash
cd ~/projects/Broker
.venv-gen/bin/python scripts/staging_rollback.py history --context default --service configs
.venv-gen/bin/python scripts/staging_rollback.py rollback --context default --service configs
# Применить показанный план, при необходимости закрепив --target из истории.
.venv-gen/bin/python scripts/staging_rollback.py rollback --context default --service configs --apply
```


### Добавление пользовательского Web в существующий staging

Добавь в AdGuard Home запись `web.broker.home.arpa → 192.168.1.146`, аналогично
admin/auth/grafana. LAN-профиль задаёт имя через `platform.yaml → environments.staging.lan.web_host`,
а localhost-режим — через `web_origin`.

При следующем деплое с инфраструктурой сертификат дополняется новым именем и
подписывается существующим CA. Корневой ключ не меняется: повторно устанавливать
CA на устройства не нужно. Настройки Web используют HTTPS origin и Secure cookie.
Синхронизация Keycloak создаёт отсутствующие broker-web/broker-logistics, добавляет
роли из шаблона для author/reviewer и обновляет точные callback/logout адреса обоих
браузерных клиентов; пароли и пользовательские записи не пересоздаются.
Привязки сотрудников к организации в бизнес-сервисах — отдельная доверенная операция.

`Deploy all` выбирает последнюю **опубликованную** версию. Зелёная проверка Service
images на PR собирает образы, но не публикует их. После мерджа запусти Service images
вручную из main, дождись успеха и затем запусти Deploy all с включённой инфраструктурой.
В журнале CD проверь выбранную ревизию и ссылку на публикацию.

Если после перезагрузки Windows оба адреса дают тайм-аут, проверь актуальный IP
Multipass и `netsh interface portproxy show all`. После смены connectaddress
убедись, что IP Helper слушает 2222/443: `Get-NetTCPConnection -State Listen`.
При отсутствии listener перезапусти `iphlpsvc` из PowerShell администратора.

После смены IP VM обнови не только portproxy. Если K3s продолжает показывать старый
INTERNAL-IP узла или адрес Kubernetes API, перезапусти `sudo systemctl restart k3s`.
Дождись нового IP в `sudo k3s kubectl get nodes -o wide` и единственного актуального
адреса в `sudo k3s kubectl -n default get endpointslices -l kubernetes.io/service-name=kubernetes -o wide`.
Иначе Traefik может сохранить устаревшие endpoints и отдавать 502; во время потери
доступа к API после перезапуска Traefik может временно показывать стандартный
сертификат. Это не повод менять доверенный CA или обходить предупреждение браузера.

После смены подсети Hyper-V также перезапустите CoreDNS:

```bash
export KUBECONFIG="$HOME/.kube/config"
kubectl -n kube-system rollout restart deployment/coredns
kubectl -n kube-system rollout status deployment/coredns --timeout=90s
```

Его pod мог сохранить прежний DNS-шлюз в `/etc/resolv.conf`. Внутренние имена
с точкой на конце разрешаются без внешних search-суффиксов; OTLP endpoint теперь
использует эту форму. Проверка мониторинга отдельно сообщает, отсутствует ли
трасса в Tempo или запись в Loki.

Появление нового LAN-домена требует Deploy infrastructure (или Deploy all с
включённой инфраструктурой): обновление только сервисов не создаёт ingress и
не дополняет сертификат. Корневой CA при этом сохраняется.


### Фоновое создание перевозок

На staging обработчик Shipments включается автоматически. Секрет служебного клиента
`shipments-dispatcher` хранится в `.staging/shipments-dispatcher-secret` и сохраняется
между рендерами. Deploy infrastructure настраивает клиент в существующем Keycloak
с правом `transportations.create`; пароли пользователей не изменяются.
В LAN-профиле получение токена использует внутренний HTTPS Keycloak.

При первом включении выполните Deploy all с инфраструктурой. Уже принятые операции
`pending` будут обработаны автоматически; повторять создание через UI не нужно.
