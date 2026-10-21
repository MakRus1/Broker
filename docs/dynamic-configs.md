# Динамические конфиги

`libs/configs` использует штатные Source, Snapshot, updater и файловый кеш userver.
Сервис `configs` хранит проверенные JSON-документы и историю публикаций в PostgreSQL.
C++-типы и ключи создаются из YAML при сборке, исходники генерации не коммитятся.

## Каталоги и подключение

```text
schemas/
├── configs/
│   ├── shipment/SHIPMENT_TERMS_CONFIG.yaml
│   └── broker/BROKER_HTTP_CLIENT_CONFIG.yaml  # Создается автоматически
└── config_contexts/
    └── shipment/SHIPMENT_CONTEXT.yaml
```

Папка обозначает владельца схемы, а не потребителя. Имя конфига глобально уникально,
имя файла совпадает с `name` в UPPER_CASE. Контексты тоже имеют уникальные имена.
В `service.yaml` достаточно имени, без пути:

```yaml
version: 1
libraries: [common, configs]
configs: [SHIPMENT_TERMS_CONFIG]
```

После изменения схем выполните `make gen` у потребителей и сервиса `configs`,
либо `make gen SERVICE=<name>` из корня. Поставщик HTTP API также обновляет свой
автоматический конфиг через `make gen`. `make check-gen-all` проверяет актуальность
без записи. Неизвестное имя и повторные объявления отклоняются генератором.
Сервис `configs` автоматически включает весь каталог для проверки публикаций.

## Схема значения и контекст

`schemas/configs/shipment/SHIPMENT_TERMS_CONFIG.yaml`:

```yaml
name: SHIPMENT_TERMS_CONFIG
mode: conditional
context: shipment/SHIPMENT_CONTEXT
schema:
  type: object
  required: [is_enabled, max_days]
  properties:
    is_enabled: {type: boolean}
    max_days: {type: integer, minimum: 1, maximum: 365}
    description: {type: string, default: ''}
default:
  is_enabled: false
  max_days: 30
```

`schemas/config_contexts/shipment/SHIPMENT_CONTEXT.yaml`:

```yaml
name: SHIPMENT_CONTEXT
schema:
  type: object
  properties:
    organization_id: {type: string, minLength: 1}
    transport_mode: {type: string, enum: [sea, road, rail]}
```

Поддерживаются скалярные типы, массивы скаляров, enum, format, default и ограничения
текущего OpenAPI-генератора. Произвольные вложенные объекты пока не поддерживаются.
Неизвестные поля запрещены. `required` требует явного поля в документе, даже если
у поля есть default. Необязательное поле с default становится обычным C++ значением;
без default — `std::optional`. Корневой `default` схемы обязателен: это встроенное
значение для запуска, а не значение опубликованной ревизии.

`mode: plain` описывает значение без клоз и не допускает `context`.
`mode: conditional` публикуется в следующей форме:

```json
{
  "clauses": [
    {
      "title": "Морские поставки клиента",
      "value": {"is_enabled": true, "max_days": 60},
      "predicates": [
        {"field": "organization_id", "type": "eq", "value": "example"},
        {"field": "transport_mode", "type": "in", "value": ["sea"]}
      ]
    }
  ],
  "default": {"is_enabled": false, "max_days": 30}
}
```

Выбирается первая подходящая клоза. Предикаты внутри нее соединяются AND;
пустой список соответствует любому контексту. Если ничего не подошло, возвращается
`default`. Значение клозы проверяется целиком и не наследует поля из fallback.
`title` обязателен; необязательный `id`, если задан, должен быть непустым и уникальным.
Лимиты: 1000 клоз на конфиг, 100 предикатов на клозу.

Для всех полей доступны `eq`, `in` (непустой массив) и `exists` (boolean).
Для чисел дополнительно доступны `lt`, `lte`, `gt`, `gte`. Тип и ограничения
предиката проверяются по схеме контекста. Отсутствующее поле не соответствует
предикату, кроме `exists: false`. Неизвестное поле или оператор запрещены.

## C++ API

```cpp
#include <configs/keys.hpp>

const auto snapshot = deps.configs.GetSnapshot();
const auto terms = snapshot.Get(
    configs::SHIPMENT_TERMS_CONFIG,
    config_contexts::ShipmentContext{.OrganizationId = "example"});
if (terms.IsEnabled) {
    UseMaxDays(terms.MaxDays);
}
```

Поля C++ используют PascalCase. Контекст типизирован, enum генерируются в C++.
Для plain-конфига второй аргумент не нужен. Snapshot неизменяемый: последующее
обновление не меняет уже полученный снимок. Берите один snapshot на логически
связанную операцию, чтобы читать согласованные значения нескольких конфигов.
Source можно передавать бизнес-компоненту через конструктор вместо целого Deps.

## Доставка и кеш

В `configs/config_vars.yaml` потребителя:

```yaml
config-updates-enabled: true
config-server-url: http://configs:8090
config-cache-path: /var/cache/broker/dynamic-config.json
```

По умолчанию обновления выключены, путь кеша пустой; работают встроенные значения.
При включении native updater выполняет полное обновление через `/configs/values`
раз в секунду. Неизменившиеся значения тоже подтверждаются для проверки свежести. Каталог файлового кеша должен существовать
и быть доступен пользователю процесса для записи; для контейнера используйте volume.
При проблемах обновления сохраняется последний принятый snapshot. Схемы и значения
проверяются и при публикации, и в потребителе. Невалидные значения не заменяют
последнее принятое состояние. Разные сервисы получают обновления независимо;
атомарности между процессами нет. Ревизия относится ко всей публикации сервера.

Первый неудачный запрос не останавливает процесс. Для обязательных конфигов можно
запретить builtin, ограничить использование кеша и возраст последнего подтверждения.
Метаданные snapshot и HTTP gate описаны в [critical-dependencies.md](critical-dependencies.md).

Не размещайте секреты и бизнес-документы в конфигах. Значения не пишутся в штатный
лог публикации. Изменения схем требуют согласованного развертывания сервера и клиентов:
автоматическая проверка совместимости схем пока отсутствует. Удаленный из каталога
ключ, оставшийся в опубликованной ревизии, нужно убрать отдельной миграцией данных
до публикации по новой схеме. API удаления ключа пока нет.

Настройки HTTP-клиентов: [client-codegen.md](client-codegen.md).
API публикации, откат и запуск: [services/configs/README.md](../services/configs/README.md).


## Конфиги в тестах

Используется штатный механизм testsuite, отдельной фикстуры Broker нет:

```python
import pytest

@pytest.mark.config(
    SHIPMENT_TERMS_CONFIG={
        'clauses': [],
        'default': {'is_enabled': True, 'max_days': 60},
    },
)
async def test_shipment(service_client):
    ...
```

Для короткого написания `@config(...)` можно объявить в модуле
`config = pytest.mark.config`. Это обычный alias существующего маркера.
Значение передается целиком и должно соответствовать схеме; несколько конфигов
можно передать разными именованными аргументами одного маркера.

Параметризация тоже штатная:

```python
@pytest.mark.parametrize('expected_days', [
    pytest.param(30, marks=pytest.mark.config(
        SHIPMENT_TERMS_CONFIG={
            'clauses': [], 'default': {'is_enabled': True, 'max_days': 30},
        })),
    pytest.param(60, marks=pytest.mark.config(
        SHIPMENT_TERMS_CONFIG={
            'clauses': [], 'default': {'is_enabled': True, 'max_days': 60},
        })),
])
async def test_terms(service_client, expected_days):
    ...
```

Testsuite подает значения через mockserver и изолирует изменения между тестами.
Если конфиг нужно поменять во время одного теста, используйте существующую
фикстуру `dynamic_config.set_values({...})`. Перед запросом через service_client
обновление доставляется автоматически; при обращении только через monitor_client
сначала вызовите `await service_client.update_server_state()`.
Общие тестовые значения задавайте в config_service_defaults, чтобы autouse-фикстура
не перезаписывала per-test маркеры после их применения.
