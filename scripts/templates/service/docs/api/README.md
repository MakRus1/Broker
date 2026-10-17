# OpenAPI спецификации

Сюда кладутся YAML/JSON файлы с описанием HTTP API.

После изменения спеки запустите кодогенерацию из папки сервиса:

```bash
make gen
```

Генерация:

- `src/views/<path>/` — заготовки ручек (для `/foo/bar` → `src/views/foo/bar/`);
- `.gen/` — сгенерированные DTO, handler-классы и фрагмент `config.openapi.yaml`.
- `codegen.lock` — хеш сгенерированного вывода (коммитить вместе со спекой).

Существующие `view.hpp`, `view.cpp` и `deps.hpp` не перезаписываются.

`make gen` автоматически обновляет блок между `# OPENAPI_HANDLERS_BEGIN` / `# OPENAPI_HANDLERS_END` в `configs/static_config.yaml`.

CI проверяет: `make check-gen-all` (актуальный `codegen.lock`, `static_config.yaml`, view-заготовки).

### Несколько статусов (200, 404, …)

`View::Handle` возвращает `std::variant<Response200, Response404, …>`. Пример:

```cpp
if (request.name == "missing") {
    return Response404{.message = "user not found"};
}
return Response200{.greeting = fmt::format("Hello, {}!", request.name)};
```

HTTP-статус выставляет сгенерированный handler — вручную `SetResponseStatus` в view не нужен.

### Зависимости (PostgreSQL и др.)

По аналогии с gRPC-компонентами: зависимости резолвятся из `ComponentContext` в отдельном файле `deps.hpp` рядом с view. Если файл есть — кодоген подключает его в handler.

```cpp
// src/views/hello_postgres/deps.hpp
#pragma once

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

namespace broker::views::hello_postgres {

struct Deps {
    userver::storages::postgres::ClusterPtr pg_cluster;
};

inline Deps ResolveDeps(const userver::components::ComponentContext& component_context) {
    return Deps{
        .pg_cluster = component_context
            .FindComponent<userver::components::Postgres>("postgres-db-1")
            .GetCluster(),
    };
}

}  // namespace broker::views::hello_postgres
```

В `view.hpp` подключите `deps.hpp` и примите `Deps` в `Handle`. Имя компонента (`postgres-db-1`) должно совпадать с тем, что зарегистрировано в `main.cpp`.

`deps.hpp` не перезаписывается кодогеном.
