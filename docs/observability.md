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

Тела HTTP, заголовки и protobuf-сообщения в стандартных логах отключены. Не передавайте
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
