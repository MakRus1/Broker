# service.yaml

Файл в корне сервиса описывает его инфраструктуру и библиотеки:

```yaml
version: 1
libraries: [common]
postgresql:
  enabled: true
  cluster: main
grpc:
  enabled: true
```

Применение из каталога сервиса:

```sh
make gen
make test-debug
```

Из корня: `make gen SERVICE=orders`, `make test-debug SERVICE=orders`.
`make new-service NAME=orders POSTGRES=1 GRPC=1` создает такой manifest сразу.
Для существующего сервиса меняйте manifest, повторять new-service не нужно.
Отсутствующая секция PostgreSQL/gRPC означает enabled: false;
при отсутствии libraries используется [common]. Неизвестные ключи, повторения
и неверные типы отклоняются до изменения файлов.

## Что обновляется

Генератор меняет только блоки между парными маркерами `SERVICE_*_BEGIN/END`:

- CMake: библиотеки, SQL и protobuf-кодогенерация, исходник gRPC-примера;
- main.cpp: include и регистрация компонентов;
- deps.hpp: PostgreSQL-зависимость и получение кластера;
- bootstrap/ping.cpp: проверка БД в готовности;
- static_config.yaml: PostgreSQL, gRPC-listener и middleware трассировки;
- tests/conftest.py: плагины и fixtures PostgreSQL/gRPC.

Ручные изменения внутри этих блоков заменяются. Вне блоков код сохраняется.
При первом включении добавляются отсутствующие демонстрационные файлы:
SQL/схема/данные и hello_postgres либо proto, HelloGrpc и gRPC-тест.
Уже существующие файлы примеров не перезаписываются.
Собственные gRPC-компоненты регистрируйте вне управляемого блока;
все `.proto` из proto/ автоматически участвуют в сборке.

Секреты и адреса не хранятся в manifest: dbconnection поддерживает DB_CONNECTION,
переменные находятся в configs/config_vars*.yaml. Отсутствующие переменные добавляются,
существующие значения сохраняются. После подключения БД проверьте pg-connection.
Testsuite запускает свою БД самостоятельно. Для ручного запуска нужен доступный PostgreSQL;
manifest не поднимает БД и не меняет docker-compose существующего сервиса.

`.service-features.json` фиксирует примененные инфраструктурные возможности,
его нужно коммитить. Автоматическое отключение уже включенных PostgreSQL/gRPC
отклоняется: сначала нужно отдельно убрать использующий их ручной код, настройки
и тесты, затем выполнить осознанную миграцию состояния генератора.
Сейчас автоматизировано безопасное добавление, а не удаление инфраструктуры.

`make check-gen-all` проверяет manifest и обвязку без записи.
Сборка требует актуального `make gen` и не меняет исходники.
Старые сервисы без service.yaml продолжают работать; для миграции понадобятся
управляемые блоки из актуального шаблона. Текущий broker уже мигрирован.

## Библиотеки

`libraries` содержит имена каталогов `libs/<name>`, соответствующие CMake-target
`broker_<name>`. Каталог должен содержать CMakeLists.txt. Например:

```yaml
libraries: [common, routing]
```

```cmake
# libs/routing/CMakeLists.txt
add_library(broker_routing INTERFACE)
target_include_directories(broker_routing INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

Корневая сборка автоматически находит библиотеки libs/*/CMakeLists.txt.
Порядок межбиблиотечных зависимостей задавайте через CMake-targets, не через include-пути сервиса.
`common` пока обязательна: в ней находятся middleware телеметрии.
Скрипт выборочного CI читает libraries прямо из manifest. Для старых сервисов
без manifest остается карта .github/service-deps.yaml. Дублировать зависимости
нового сервиса в этой карте вручную не нужно.

Подключение HTTP-клиентов в секции `clients`: [типизированные клиенты](client-codegen.md).

## Динамические конфиги

```yaml
libraries: [common, configs]
configs: [SHIPMENT_TERMS_CONFIG]
```

Ссылки используют глобально уникальные имена, без путей к YAML. Генератор находит
схему в `schemas/configs`, создает ключи и DTO в каталоге сборки, добавляет
`deps.configs` и штатные компоненты обновления userver. Неизвестное имя запрещено.
HTTP-клиенты автоматически подключают библиотеку и конфиг своего поставщика;
добавлять их ключи в `configs` вручную не нужно.
Обновления по умолчанию выключены; настройка доставки и схемы: [dynamic-configs.md](dynamic-configs.md).

## Имя PostgreSQL-кластера

`postgresql.cluster` задает логическое имя кластера; по умолчанию `main`.
Например, `cluster: accounting` создает компонент `postgres-accounting`.
Генератор согласованно обновляет регистрацию, static_config и получение `deps.pg_cluster`.
Имя допускает строчные латинские буквы, цифры и одиночные разделители `-`/`_`.
После изменения выполните `make gen`; ручные FindComponent с прежним именем нужно
обновить самостоятельно. Обычным компонентам лучше передавать `deps.pg_cluster`.

Имя кластера не переименовывает саму БД, каталог SQL, PostgreSQL-сервер или compose-сервис.
Имя базы и адрес задаются в `pg-connection`/`DB_CONNECTION`; каталог `schemas/db_1`
соответствует имени тестовой базы `<service>_db_1` и остается отдельным понятием.
Пока manifest поддерживает один PostgreSQL-кластер на сервис.
