# PostgreSQL-модели в C++

Модели составных типов PostgreSQL описываем вручную в `services/<service>/src/models/pg/`.
Для каждого типа используем отдельный заголовок. В нём находятся структура и её
специализация `CppToUserPg`; `postgresql/types.yaml` сервисам больше не нужен.

## Добавление модели

Сначала добавляем SQL-тип новой миграцией:

```sql
CREATE TYPE public.example_v1 AS (
    id UUID,
    reference TEXT
);
```

Затем объявляем модель хранения в `src/models/pg/example_raw.hpp`:

```cpp
#pragma once
#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace example::models::pg {
struct ExampleRaw {
    boost::uuids::uuid Id{};
    std::optional<std::string> Reference{};
};
}

namespace userver::storages::postgres::io {
template <> struct CppToUserPg<::example::models::pg::ExampleRaw> {
    static constexpr DBTypeName postgres_name = "public.example_v1";
};
}
```

Поля структуры следуют порядку атрибутов в `CREATE TYPE`. Поля, для которых запрос
может вернуть NULL, объявляем как `std::optional<T>`. Составной тип PostgreSQL сам
по себе не задаёт NOT NULL: ограничения таблиц и бизнес-проверки остаются в
миграциях и C++-сценариях.

UUID представлен `boost::uuids::uuid`, дата — `userver::utils::datetime::Date`,
timestamptz — `userver::storages::postgres::TimePointTz`, jsonb —
`userver::formats::json::Value`. В заголовке модели подключаем соответствующие
маппинги из `userver/storages/postgres/io/`, например `date.hpp`, `json_types.hpp`
или `array_types.hpp`.

## Чтение и запись

Запрос остаётся обычным SQL с явной проекцией:

```sql
SELECT ROW(id, reference)::public.example_v1
FROM examples
WHERE organization_id = $1::UUID AND id = $2::UUID;
```

```cpp
const auto row = result.AsSingleRow<models::pg::ExampleRaw>();
```

Это один composite-столбец: `kRowTag` не нужен. Для списка используем
`AsSetOf<models::pg::ExampleRaw>()` или
`AsContainer<std::vector<models::pg::ExampleRaw>>()`. Составные типы также можно
передавать параметрами запроса. Сериализацию и разбор выполняет userver через
`CppToUserPg`; отдельный сгенерированный заголовок для модели не требуется.

Модель хранения не заменяет модель API. Преобразование UUID, enum и вычисляемых
значений выполняется явно в конвертерах. Интеграционные тесты сервиса проверяют
чтение и запись моделей реальными запросами к PostgreSQL.

## Изменение типа и откат приложения

Опубликованный `example_v1` не меняем через ALTER TYPE. Добавляем `example_v2`
новой миграцией и обновляем проекции SQL-запросов, поля C++-структуры и
`postgres_name`. Имя `ExampleRaw` остаётся прежним, поэтому места использования
модели не нужно переименовывать из-за новой версии SQL-типа.

Тип v1 сохраняем на время канарейки и окна отката: старое приложение продолжает
использовать v1, новое — v2. Данные могут оставаться в тех же таблицах.

Изменения таблиц также должны быть совместимыми: добавление поля с подходящим
default/nullable, согласование поведения старых и новых писателей, удаление
старого контракта только отдельным поздним этапом. Новый тип сам по себе не
обеспечивает совместимость изменения бизнес-смысла и не разрешает сразу удалять
старые колонки.

Основы реализации: [PostgreSQL composite types](https://www.postgresql.org/docs/16/rowtypes.html),
[userver v3.2 composite I/O](https://userver.tech/docs/v3.2/d4/dda/composite__types_8hpp.html).
