# Типизированные HTTP-клиенты

Публичные контракты лежат в `schemas/services/<service>/*.yaml` (также поддерживаются
JSON и YML). Это клиентская спецификация сервиса: весь API, доступный его потребителям.
Внутренние операции остаются в `services/<service>/docs/api`. Сервер генерируется
по обоим каталогам; клиент — только по публичному. Одну операцию нельзя объявить
в обоих местах: коллизия завершает генерацию ошибкой.

```text
Broker/
├── schemas/services/broker/
│   ├── foo.yaml
│   ├── hello.yaml
│   └── hello_postgres.yaml
└── services/orders/
    ├── service.yaml
    ├── docs/api/                 # Внутренние API и codegen.lock
    └── src/deps.hpp
```

`make new-service` создает публичные схемы сразу в `schemas/services/<name>`.
Если для существующего сервиса добавляется демонстрационный API вместе с PostgreSQL,
его схема первоначально появляется в docs/api. Для публикации переместите ее в
schemas/services/<service> и выполните make gen.

## Подключение

В `services/orders/service.yaml`:

```yaml
version: 1
libraries: [common]
clients:
  broker:
    service: broker
    transport: http
```

Подключается публичная спецификация целиком, без списка отдельных ручек.
После `make gen SERVICE=orders` генератор добавляет компонент, регистрацию в main,
клиент в общий Deps и переменные конфигурации. Ручной код вне SERVICE-блоков сохраняется.

Адрес задается в `configs/config_vars*.yaml` потребителя:

```yaml
client-broker-base-url: http://broker:8080
```

Значения для разных окружений передаются обычным механизмом config_vars userver.
Пул соединений, DNS, TLS и общие ограничения настраиваются в http-client/http-client-core.
В manifest нет адресов и секретов. Авторизация и OpenAPI security пока не поддерживаются:
такая схема явно отклоняется, а не генерируется частично.

## C++ API

Путь — единственный идентификатор операции; каждый путь имеет один HTTP-метод.
Поле operationId запрещено. Имя метода генерируется из пути в PascalCase:
`/foo/bar` → FooBar, `/api/v1/foo/bar` → ApiV1FooBar, `/items/{id}` → ItemsById.
Коллизии имен и зарезервированные имена Client/Component отклоняются генератором.

```cpp
const auto response = deps.broker.FooBar({.name = "Груз"});
UseGreeting(response.greeting);
```

Типы находятся в `<consumer>::clients::<alias>::<PathMethod>`, например
`orders::clients::broker::FooBar::Request` и `Response200`.
При единственном успешном ответе с телом метод возвращает его DTO напрямую:

```cpp
try {
    const auto response = deps.broker.FooBar({.name = "Груз"});
    UseGreeting(response.greeting);
} catch (const orders::clients::broker::FooBar::Error404& error) {
    HandleNotFound(error.response.message);
}
```

Единственный успех без тела (например, 204) дает void: просто вызовите метод.
Несколько успешных статусов дают std::variant только успешных ResponseNNN;
ответ без тела в таком варианте представлен пустым ResponseNNN.
Объявленные 3xx/4xx/5xx выбрасываются как ErrorNNN с типизированным DTO в поле response.
У всех ошибок API есть общий базовый тип platform::http::ApiError с GetStatus().
Сообщение what() содержит только HTTP-статус, без бизнес-данных из тела.
У операции, объявляющей только ошибки, возвращаемый тип void; любой объявленный
ответ выбрасывает исключение, а необъявленный статус дает ContractError.
DTO клиента не содержат WasProvided и не зависят от серверных handler-классов.
Необязательное значение без default — std::optional, с default — обычное значение T.
Запрос отправляет default, если вызывающий код не заменил его.

Поддерживаются те же типы enum/format/default/ограничений, что у серверного генератора.
Path/query кодируются, тело сериализуется в JSON, ответ проверяется по статусу,
Content-Type, обязательности, типам и ограничениям полей. Ответ без тела не принимает тело.

- platform::http::ApiError: общий тип объявленных ошибок API; конкретные ErrorNNN доступны в namespace операции.
- platform::http::ContractError: неожиданный статус или нарушение контракта ответа.
- platform::http::TimeoutError: штатный TimeoutException userver.
- platform::http::CancelledError: штатный CancelException userver; не подавляйте отмену задачи.
- platform::http::TransportError: общий BaseException HTTP-клиента userver.
- std::invalid_argument: запрос не соответствует объявленным ограничениям до отправки.

По умолчанию повторы выключены. Перенаправления выключены. Трассировку, дедлайн и отмену передает
штатный клиент userver. Метрики используют шаблон пути, а логи и spans — шаблон пути:
значения query/path не используются как labels и не добавляются в URL логирования.
Для unit-тестов внедряется platform::http::Transport с подменяемым Perform.

## Сборка и проверки

C++ генерируется только в каталоге сборки потребителя:
`build-<preset>/services/<consumer>/generated/include/clients/<alias>/client.hpp`.
Исходная публичная YAML-схема остается в schemas; ручной копии в потребителе нет.
После изменения контракта выполните make gen для поставщика и потребителей,
чтобы обновить codegen.lock. make check-gen-all ничего не записывает.
CMake отслеживает схемы и генератор. Циклические HTTP-вызовы не создают зависимостей
между executable-targets. Выборочный CI учитывает транзитивных потребителей.

Проверки включают C++ unit-тесты кодирования/декодирования и testsuite с настоящим
HTTP-транспортом: ошибки API, некорректные ответы, таймаут без повторов и общий trace id.

Пока не реализованы внешние контракты, автоматическая проверка breaking changes,
экспорт объединенной OpenAPI-схемы для фронтенда и настройка gRPC-клиентов через manifest.
Для gRPC остается штатный protobuf/userver-генератор.

## Динамические настройки

Поставщик автоматически получает `schemas/configs/<service>/<SERVICE>_HTTP_CLIENT_CONFIG.yaml`.
Файл создается через make gen из публичных путей API; редактировать его вручную не нужно.
Потребитель автоматически подключает соответствующий ключ и библиотеку configs.
Настройки общие для всех потребителей поставщика, overrides по потребителю пока нет.

Пример публикуемого BROKER_HTTP_CLIENT_CONFIG:

```json
{
  "**default**": {"timeout": 1000, "max_retries": 0},
  "/hello": {"timeout": 500, "max_retries": 2}
}
```

`timeout` — миллисекунды (1..3600000), `max_retries` — дополнительные попытки (0..10).
Оба поля в `**default**` обязательны. Настройки пути могут переопределить любое из них,
остальное наследуется из `**default**`. Используется точный шаблон публичного пути,
например `/items/{id}`, а не конкретный URL. Неизвестные пути и клозы запрещены.
Один snapshot читается перед каждым исходящим запросом; уже начатый запрос не меняет
настройки при публикации. Таймаут дополнительно ограничивается входящим дедлайном.

Повторы выполняет штатный HTTP-клиент userver при транспортных ошибках и повторяемых
ответах. Они разрешены только для GET/HEAD/OPTIONS. POST/PUT/PATCH/DELETE не повторяются
даже при положительном max_retries. Для их поддержки нужна отдельная политика
идемпотентности. testsuite проверяет обновление таймаута без перезапуска, число
GET-попыток и отсутствие повторов POST.

Настройка обновлений и файлового кеша: [dynamic-configs.md](dynamic-configs.md).
Статическая переменная client-<alias>-timeout-ms больше не используется генерируемыми
клиентами; базовый URL остается в config_vars.

## Переход с прежней структуры API

POST `/hello` перенесен на `/hello/create`, POST `/hello_postgres` — на
`/hello_postgres/create`. Их GET-пути сохранены. В контрактном тестовом сервисе
POST `/items/{id}` и `/validated` перенесены на соответствующие пути с `/create`.
Это изменение URL и C++ API; совместимые старые маршруты не регистрируются.

При миграции своего сервиса уберите operationId, разделите совпадающие пути,
перенесите ручные view.hpp/view.cpp, исправьте include и namespaces, затем выполните
make gen. Генератор не переносит и не удаляет ручные реализации автоматически.
