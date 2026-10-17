# Результаты проверки стабилизации Broker

17 октября 2026 года. userver v3.2, C++20, CMake 3.31.6, Python 3.12.
Репозиторий: `/Users/maksim/projects/Broker`. Изменения опубликованы в draft PR #1.
Исходная стабилизация bc2f355 прошла [GitHub CI](https://github.com/MakRus1/Broker/actions/runs/39020555990):
tools, Linux amd64 Debug/Release/ASan+UBSan и Linux arm64 Debug.

## Исходная стабилизация bc2f355

| Окружение | Debug | Release | ASan + UBSan |
| --- | --- | --- | --- |
| macOS arm64, Apple Clang 21 | Пройдены | Пройдены | Пройдены |
| Linux arm64, Docker, Clang 18 | Пройдены | Пройдены | Пройдены, detect_leaks=1 |
| Linux amd64, Docker под эмуляцией на Mac | Пройдены | Пройдены | Предусмотрены в удаленном CI |

Каждый проверенный режим включает unit-тест, Broker testsuite с HTTP/PostgreSQL/gRPC и 54 проверки контрактного сервиса. Нативные macOS-проверки на финальном коде использовали отдельный PostgreSQL в Docker через штатный параметр testsuite `--postgresql`; локальный PostgreSQL не запускался из песочницы Codex. Контрактный testsuite запускается без этого параметра, поскольку не использует PostgreSQL.

В Linux arm64 проверены все четыре сочетания PostgreSQL/gRPC для нового сервиса: создание, регистрация, сборка и testsuite. Прошли 16 проверок инструментов, check-gen-all, check-format и git diff --check. Нативное окружение macOS и Linux-окружения хранятся раздельно.

## Проверки работающего приложения

- Настоящий OpenTelemetry Collector Contrib 0.120.0 в Docker принял OTLP-логи и связанную HTTP/gRPC-трассу.
- Подтверждены JSON-логи с метаданными сервиса и trace_id, а также экспорт Prometheus.
- При остановке и запуске PostgreSQL готовность прошла 200 → 503 → 200, /ping продолжал отвечать 200.
- Production-сборка BUILD_TESTING=OFF собирается при монтировании исходников только для чтения, запускается со штатной конфигурацией и не предоставляет тестовые endpoints. Исходники trace-probe не компилируются.
- Тестовая конфигурация trace-probe добавляется только testsuite; регистрация включается явно через BROKER_TRACE_TESTS=1 в сборке с тестами.

## Исправления по результатам Docker-проверок

Образ содержит отдельные Debug- и Release-библиотеки userver, zstd, локаль en_US.UTF-8 и runtime Clang для санитайзеров. CMake не сканирует неиспользуемые C++-модули и учитывает выбранный компилятор. Python-зависимости Protobuf/gRPC закреплены отдельно для macOS и Ubuntu маркерами платформы. Docker и Dev Container используют отдельные volumes для Python-окружения и результатов сборки.

В dev-образе штатной переменной USERVER_ENABLE_STACK_USAGE_MONITOR=0 отключен необязательный учет стека корутин: обычный Docker запрещает необходимый ему userfaultfd. Дополнительные привилегии контейнеру не выдаются; логи, трассы, остальные метрики и санитайзеры работают.

## Дополнительные изменения: service.yaml и структура сервиса

После перехода на service.yaml локально повторно пройдены:

- Linux arm64: Debug, Release и ASan+UBSan, включая HTTP/PostgreSQL/gRPC;
- контрактный testsuite: 58 случаев, malformed/missing body параметризованы;
- macOS arm64: Debug-сборка, unit и контрактный testsuite;
- 19 проверок инструментов, check-gen-all, check-format и git diff --check;
- четыре сочетания PostgreSQL/gRPC: создание, настоящая сборка и testsuite;
- существующий сервис без зависимостей: добавление PostgreSQL, затем gRPC,
  с повторной сборкой/testsuites и дополнительной библиотекой из libraries;
- сохранение ручного кода, немутирующая проверка, откат при отказе записи,
  отклонение неверного manifest и выборочный CI по библиотекам manifest.

GitHub Actions обновлены на версии с Node.js 24. Для этих дополнительных изменений
удаленный CI запускается следующим коммитом в PR; результаты исходного запуска
выше не подменяют проверку нового коммита.

## Осталось

Полный стек Prometheus/Grafana/Loki/Tempo, IaC, CD, бизнес-модули и миграция Firebird остаются отдельными этапами.
