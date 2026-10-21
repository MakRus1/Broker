# PostgreSQL-типы и C++-кодген

Миграция и описание для C++ поддерживаются явно: сначала пишем SQL с `CREATE TYPE`, затем добавляем соответствующее определение в `services/<service>/postgresql/types.yaml`. Генератор не разбирает SQL, не создает и не переписывает миграции, не подключается к БД.

```sql
CREATE TYPE public.example_v1 AS (
    id UUID,
    reference TEXT
);
```

```yaml
version: 1
types:
  public.example_v1:
    fields:
      - {name: id, type: uuid}
      - {name: reference, type: text}
```

`make gen SERVICE=<service>` обновляет проверяемую генерацию. CMake создает `build-<preset>/services/<service>/generated/include/postgresql/types.hpp` со структурой `<service>::pgtypes::ExampleV1` и `CppToUserPg`, связывающим ее с `public.example_v1`. Имена C++-полей образуются из SQL-имен: `customer_id` → `CustomerId`. Нужен `postgresql.enabled: true` в service.yaml.

Запрос остается обычным SQL с явной проекцией:

```sql
SELECT ROW(id, reference)::public.example_v1
FROM examples
WHERE organization_id = $1::UUID AND id = $2::UUID;
```

```cpp
const auto row = result.AsSingleRow<pgtypes::ExampleV1>();
```

Это один composite-столбец: `kRowTag` не нужен. Для списка используются `AsSetOf<pgtypes::ExampleV1>()` / `AsContainer<std::vector<pgtypes::ExampleV1>>()`. Составные типы можно передавать и параметрами запроса; завершение операции доставки использует этот путь, а testsuite проверяет реальный двусторонний binary I/O.

## Описания и проверка

Поддерживаются text, boolean, smallint, integer, bigint, real, double precision, uuid, jsonb, timestamptz, одномерные массивы этих типов и ссылки на другие описанные composite-типы. UUID представлен `boost::uuids::uuid`, timestamptz — `userver::storages::postgres::TimePointTz`, jsonb — JSON Value. `nullable: true` дает `std::optional<T>` для целого поля; nullable-элементы массива пока не описываются. SQL-enum/domain/numeric пока не поддерживаются и отклоняются явно.

Порядок полей в YAML значим и должен совпадать с `CREATE TYPE`. Генератор отклоняет повторения, неизвестные типы и свойства, рекурсию, некорректные имена и конфликты имен C++. Поддерживается одна схема описания формата `version: 1`; это версия YAML-формата, а не конкретного PostgreSQL-типа.

После применения миграций testsuite сверяет имена, порядок и OID типов атрибутов через `tests/support/postgres_types.py`. Проверка ловит расхождение SQL и YAML, включая перестановку двух UUID-полей: одного совпадения C++-типов недостаточно. Подключение к PostgreSQL требуется для тестов, а не для кодогенерации.

`nullable` не создает ограничение NOT NULL в составном типе. Ограничения таблиц и бизнес-проверки остаются в соответствующих миграциях и C++-сценариях. Прямое отображение типа хранения в API не предполагается: преобразование UUID/enum и вычисляемые значения выполняются явно на границе моделей.

## Канарейка и откат приложения

Опубликованный `example_v1` не меняем через ALTER TYPE. Добавляем `example_v2` отдельной ручной миграцией и новым элементом YAML; v1 сохраняем на время канарейки и окна отката. Новый код явно выбирает `ROW(...)::public.example_v2` и `pgtypes::ExampleV2`, старый продолжает использовать v1. Данные могут оставаться в тех же таблицах.

Изменения таблиц также должны быть совместимыми: добавление поля с подходящим default/nullable, согласование поведения старых и новых писателей, удаление старого контракта только отдельным поздним этапом. Новый тип сам по себе не обеспечивает совместимость изменения бизнес-смысла и не разрешает сразу удалять старые колонки.

Тест shipments создает v2 и дополнительную колонку, затем проверяет, что работающий код с v1 продолжает создавать, читать и обновлять поставки, сохраняя новое поле. Остальные testsuite-сценарии проверяют чтение UUID/nullable JSON и запись composite-параметра через userver; межсервисный smoke проверяет доставку между репликами.

Основы реализации: [PostgreSQL composite types](https://www.postgresql.org/docs/16/rowtypes.html), [userver v3.2 composite I/O](https://userver.tech/docs/v3.2/d4/dda/composite__types_8hpp.html).
