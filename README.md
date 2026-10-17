# Broker

Основа модульной логистической платформы: C++20, userver **3.2**, PostgreSQL и gRPC.
Версии userver, CMake, Ninja и clang-format зафиксированы в `tools/versions.env`.
Бизнес-модули и миграция старого Broker пока не реализованы.

## Быстрый старт через Docker

Установите Docker Desktop и запустите Docker Engine. На Linux нужен Docker Engine.
На Windows храните checkout в файловой системе WSL2. На macOS используется Linux-контейнер
для родной архитектуры, без обязательной эмуляции amd64.

```sh
make docker-image
make docker-test-debug
make docker-test-release
```

Первая сборка образа компилирует userver и занимает заметное время. CI и Dev Container
используют тот же `tools/Dockerfile`. Открытие в VS Code: **Dev Containers: Reopen in Container**.
Внутри контейнера команды выполняются без префикса `docker-`.

## Нативная сборка macOS

Command Line Tools предоставляют Clang и SDK; IDE Xcode устанавливать не требуется.
Редактором может быть VS Code.

```sh
xcode-select --install
# Установите Homebrew по инструкции https://brew.sh, если он отсутствует.
./scripts/install-deps-macos.sh
export PATH="$(brew --prefix)/opt/python@3.12/libexec/bin:$(brew --prefix)/opt/postgresql@16/bin:$PATH"
make doctor
make gen
make test-debug
```

Подробности, ограничения проверки и разделение окружений: [docs/development.md](docs/development.md).

## Кодогенерация и сервисы

```sh
make gen SERVICE=broker
make check-gen-all check-format
make test-tools
make new-service NAME=orders POSTGRES=1 GRPC=1
make test-debug SERVICE=orders
```

Каждая пара HTTP-метод + путь имеет собственный каталог `src/views/<path>/<method>/`.
Ручной код не перезаписывается. Генерируемые DTO и обработчики находятся в каталоге сборки.
Проверка `check-gen-all` не исправляет файлы: устаревший контракт требует явного `make gen`.
`make gen` также работает из каталога сервиса без SERVICE.
SQL-запросы генерируются из `postgresql/queries`; `src/deps.hpp` общий на сервис.
Метаданные логов и трасс автоматически добавляет middleware.
Новый сервис автоматически получает `/ping`, `/ping/ready` и `/metrics` на служебном listener.

Структура сервиса: [docs/service-layout.md](docs/service-layout.md).
Подключение БД, gRPC и библиотек: [docs/service-manifest.md](docs/service-manifest.md).

Поддерживаемый OpenAPI-контракт: [docs/codegen.md](docs/codegen.md).
Логирование, метрики и трассировка: [docs/observability.md](docs/observability.md).

## Проверки

CI запускает Python-проверки, выборочные тесты сервисов, чистые Debug/Release-сборки,
ASan/UBSan и Debug на arm64. Отдельный тестовый сервис проверяет скомпилированный кодоген.
Четыре варианта создания сервиса также собираются и тестируются в изолированной копии.

Наличие workflow не означает, что проверки уже прошли: фактические результаты текущего
рефакторинга записаны в [docs/validation.md](docs/validation.md).
