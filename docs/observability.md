# Логирование, метрики и трассировка

## Автоматические служебные ручки

Каждый новый сервис получает внутренний listener на loopback:

- `/ping`: процесс может обработать запрос, без обращения к зависимостям.
- `/ping/ready`: компоненты работают; для сервиса с PostgreSQL выполняется `SELECT 1`.
  При отказе БД или завершении работы возвращается 503.
- `/metrics`: Prometheus-формат статистики userver, включая HTTP, gRPC, PostgreSQL,
  состояние процесса и пул соединений подключенных компонентов.

Обычный сервис использует порт 8082; testsuite подбирает свободный служебный порт.
Публичный HTTP listener эти пути не обслуживает. Служебный порт нельзя открывать
в Интернет. В Kubernetes или Docker-сети привязку можно будет изменить явно в IaC
вместе с сетевыми ограничениями.

## Логи

Основной logger пишет JSON в stderr. Запросы и gRPC используют общий trace context.
На все HTTP- и gRPC-запросы middleware автоматически добавляет `service.name`,
`service.version` и `deployment.environment.name`. Имя и окружение берутся из
`platform-http-telemetry`, версия — из проекта CMake. Ручные вызовы в обработчиках
не нужны; новый сервис получает middleware и конфигурацию из шаблона.
HTTP-middleware выполняется после создания trace context, до аутентификации и обработчика;
gRPC-middleware — до стандартного логирования вызова.
OTLP resource-атрибуты задаются в конфигурации. При установке Collector нужно также
обогащать ими ранние и системные stdout/stderr-записи без контекста запроса.

Сгенерированные HTTP-ручки по умолчанию логируют тела запросов и ответов до 32 КиБ.
Исключения и лимиты задаются через `http_logging` в service.yaml;
[настройка](service-manifest.md#тела-http-в-логах). В BFF исключены ручки сессии и выхода,
ручные обработчики входа сохраняют отключенное логирование тел. Заголовки и
protobuf-сообщения по умолчанию не логируются. Не передавайте
пароли и документы в query string: URL может попасть в диагностический контекст userver.
Идентификаторы клиента, поставки, контейнера и документа нельзя добавлять в labels метрик.
Аудит действий пользователей будет отдельным механизмом с собственным хранением.

## Трассировка и локальный экспорт

HTTP принимает и передает W3C traceparent; gRPC использует встроенный механизм userver.
Тестовая `/internal/trace-probe` из `tests/support` вызывает локальный gRPC HelloService, чтобы функциональный
тест мог проверить общий trace_id. Testsuite сам добавляет конфигурацию ручки и client factory и включает их через `BROKER_TRACE_TESTS=1`.
В обычной конфигурации этих компонентов нет, при обычном запуске они не регистрируются.
При `BUILD_TESTING=OFF` ее исходники не компилируются. Она не включается в шаблон сервиса.

Для локальной проверки экспорта:

```sh
docker compose -f tools/compose.telemetry.yaml up -d
docker compose up -d postgres-broker
cp services/broker/configs/config_vars.yaml work-local.yaml
# В work-local.yaml задайте otlp-enabled: true.
# Для внешнего PostgreSQL задайте DB_CONNECTION или pg-connection.
./build-debug/services/broker/broker -c services/broker/configs/static_config.yaml --config_vars work-local.yaml
```

После запуска сервиса в другом терминале:

```sh
curl -H 'traceparent: 00-1234567890abcdef1234567890abcdef-1234567890abcdef-01' http://localhost:8080/hello
curl http://localhost:8082/metrics
docker compose -f tools/compose.telemetry.yaml logs collector
```

Сохранение JSON в stderr продолжается и при включенном OTLP (`sinks: both`). Ошибка
Collector не должна блокировать бизнес-запросы: используется ограниченная асинхронная очередь.
Endpoint OTLP задается через `OTEL_EXPORTER_OTLP_ENDPOINT`; переменная среды развертывания
должна совпадать с config_vars и resource-атрибутами.

Для проверки готовности остановите `docker compose stop postgres-broker`: служебный
`/ping` должен оставаться 200, `/ping/ready` — возвращать 503. После
`docker compose start postgres-broker` и восстановления соединений `/ping/ready`
снова возвращает 200. Проверка выполнена на работающем Broker.

Целевая центральная связка: OpenTelemetry Collector, Prometheus, Grafana, Loki, Tempo.
Полный локальный стенд с хранением телеметрии и готовым дашбордом запускается через
`make local-up`: [инструкция](local-environment.md). Центральное production-развертывание
и alerting остаются отдельным этапом.

Сервисные Grafana-дашборды генерируются из `service.yaml` вместе с остальной
обвязкой: [состав, фильтры и кастомизация](service-dashboards.md).


Для отладки конфигов возьмите trace_id из response header `X-Trace-Id` в браузере
и найдите связанные записи в Loki:

```logql
{service_name=~"admin_bff|configs"} | trace_id="<trace_id>"
```

Поле `body` содержит JSON запроса либо ответа. `_type=request` — входящий запрос,
`_type=response` — завершение обработки с ответом; `uri`/`meta_type` указывают путь,
`meta_code` — статус ответа. В Grafana поля Loki с `_` в начале
могут нормализоваться: проще искать по trace_id и смотреть JSON исходной строки.

Все HTTP-ответы содержат `X-Trace-Id` — чистый идентификатор текущей трассы. Его можно скопировать из Network в браузере прямо в Tempo или фильтр `trace_id` в Loki. Стандартный `traceparent` также сохраняется. Заголовок добавляется общей middleware, включая ответы с ошибками.

В локальном окружении `otlp-logger.sinks.tracing: both` отправляет спаны в Tempo и записывает их в JSON-логи. Именно завершающая запись HTTP-спана содержит тело ответа; при `tracing: otlp` она отсутствует в Loki. Обычные логи поступают в Loki через Collector из файлов, без повторного OTLP-экспорта.

Панели логов на общем и сервисных дашбордах выводят тип записи, сообщение/имя спана, HTTP-статус и `body` прямо в строке. `trace_id` остается видимым и содержит ссылку в Tempo. В Explore с произвольным запросом отображение определяется самим запросом и выбранными полями.
