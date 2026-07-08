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

Существующие `view.hpp` / `view.cpp` не перезаписываются.

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
