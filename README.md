# Broker — монорепозиторий на userver

C++ монорепозиторий с [userver](https://github.com/userver-framework/userver) v2.15, PostgreSQL и gRPC.

**Рекомендуемый способ разработки — Dev Container или Docker.** Одинаковое окружение на macOS, Windows (WSL2) и Linux, без установки десятков пакетов.

## Структура

```
.
├── .devcontainer/        # Dev Container (основной способ разработки)
├── cmake/                # DownloadUserver + CPM (fallback без образа)
├── libs/common/          # общие библиотеки
├── services/broker/      # сервис: HTTP + PostgreSQL + gRPC
├── docker-compose.yml    # PostgreSQL для локального запуска
└── .github/workflows/    # CI через Docker
```

## Быстрый старт (Dev Container)

1. Установите [Docker Desktop](https://www.docker.com/products/docker-desktop/) (Windows/macOS) или Docker + [Colima](https://github.com/abiosoft/colima) (macOS)
2. На Apple Silicon с Colima выделите достаточно памяти: `colima start --memory 8`
3. Откройте репозиторий в **Cursor / VS Code**
3. Command Palette → **Dev Containers: Reopen in Container**
4. Дождитесь сборки образа и `postCreateCommand`

Внутри контейнера:

```bash
make build-debug
make test-debug
make start-debug    # запуск сервиса
```

Образ `ghcr.io/userver-framework/ubuntu-22.04-userver-pg-dev:v2.15` содержит userver и все зависимости.

## Быстрый старт (Windows + WSL2)

1. Установите WSL2 с Ubuntu 24.04
2. Клонируйте репозиторий **внутри WSL** (`~/projects/broker`, не на `C:\`)
3. Откройте папку в Cursor → **Reopen in Container**

Либо без IDE — из WSL:

```bash
make docker-cmake-debug
make docker-build-debug
make docker-test-debug
```

## Сборка через Docker (без Dev Container)

Подходит для CI и одноразовых сборок с хоста:

```bash
make docker-cmake-debug
make docker-build-debug
make docker-test-debug
```

## Тесты

По умолчанию `SERVICE=broker`. Локально запускаются тесты **только выбранного сервиса**:

```bash
make test-debug                  # broker (по умолчанию)
make test-debug SERVICE=orders   # другой сервис
make test-all-debug              # все сервисы
```

В Docker то же самое:

```bash
make docker-test-debug SERVICE=broker
```

## Selective CI

CI запускает тесты **только для изменённых сервисов** (см. `.github/service-deps.yaml`):

| Изменения | Что тестируется |
|-----------|-----------------|
| `services/broker/...` | только `broker` |
| `libs/common/...` | все сервисы, зависящие от `common` |
| `Makefile`, `cmake/`, `CMakeLists.txt`, `.github/`, ... | все сервисы |
| только docs / `.md` | тесты пропускаются |

Локально — какие сервисы попадут в CI:

```bash
./scripts/affected-services.sh origin/main
```

При добавлении сервиса регистрация в CI — через `make new-service`.

## PostgreSQL

**Тесты** (`make test-debug`) сами поднимают PostgreSQL через testsuite на порту **15433**. Перед запуском убедитесь, что порт свободен:

```bash
make testsuite-clean   # остановить зависший postgres и docker compose
make test-debug
```

**Ручной запуск** (два режима, порт **15433** один — не смешивать):

| Команда | PostgreSQL |
|---------|------------|
| `make docker-run-debug` | compose (`make db-up`) |
| `make docker-start-debug` | testsuite сам |

```bash
# Из корня репозитория или из services/broker/ (SERVICE подставится сам)
cd services/broker
make docker-run-debug          # вариант A — compose postgres + бинарник
make docker-start-debug        # вариант B — testsuite postgres + service-runner

make db-down                   # остановить compose-postgres
```

На Apple Silicon образ userver — **amd64**. Colima с `aarch64` эмулирует его через qemu — бинарник `broker` падает с `SIGKILL` / `Subprocess killed`, все functional-тесты валятся на setup.

**Удалять Colima не нужно** — у него есть **профили** (`-p`), каждый со своей VM и архитектурой. Обычные проекты остаются на `default` (aarch64).

Отдельный профиль для userver (x86_64). На Apple Silicon нужны зависимости (один раз):

```bash
brew install qemu lima-additional-guestagents
colima start userver --arch x86_64 --memory 8 --cpu 4
docker context use colima-userver
make dist-clean
make docker-test-debug
```

Вернуться к другим проектам:

```bash
colima stop -p userver
colima start -p default             # или: colima start
docker context use colima
```

Список профилей: `colima list`.

Либо нативная сборка на macOS: `make deps-macos` и `make cmake-debug && make test-debug`.

## Эндпоинты сервиса broker

| Протокол | Адрес | Описание |
|----------|-------|----------|
| HTTP | `GET/POST /hello` | простой hello |
| HTTP | `GET/POST /hello-postgres?name=...` | hello с PostgreSQL |
| gRPC | `:8081` `HelloService.SayHello` | gRPC hello |

## Новый сервис

```bash
make new-service NAME=orders POSTGRES=1 GRPC=1
```

Добавьте `add_subdirectory(services/orders)` в корневой `CMakeLists.txt`.

## Нативная сборка (опционально)

Только если не используете Docker. На macOS:

```bash
make deps-macos
export PATH="/opt/homebrew/opt/python@3.13/bin:/opt/homebrew/bin:$PATH"
pip3 install -r requirements.txt
make cmake-debug && make build-debug
```

## Полезные команды

| Команда | Описание |
|---------|----------|
| `make build-debug` | Сборка (внутри Dev Container) |
| `make test-debug` | Unit + functional тесты сервиса `SERVICE` (по умолчанию broker) |
| `make test-all-debug` | Тесты всех сервисов подряд |
| `make docker-test-debug` | То же через Docker с хоста |
| `make testsuite-clean` | Очистить зависший PostgreSQL testsuite перед тестами |
| `make dist-clean` | Очистка артефактов |
| `make gen SERVICE=broker` | Кодогенерация из OpenAPI (`docs/api/` → `src/views/`, `.gen/`, `codegen.lock`) |
| `make check-gen-all` | Проверка, что кодоген актуален (как в CI) |

Документация userver: https://userver.tech
