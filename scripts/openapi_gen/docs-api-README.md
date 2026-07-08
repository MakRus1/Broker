# OpenAPI спецификации

Сюда кладутся YAML/JSON файлы с описанием HTTP API.

После изменения спеки запустите кодогенерацию из папки сервиса:

```bash
make gen
```

Генерация:

- `src/views/<path>/` — заготовки ручек (для `/foo/bar` → `src/views/foo/bar/`);
- `.gen/` — сгенерированные DTO, handler-классы и фрагмент `config.openapi.yaml`.

Существующие `view.hpp` / `view.cpp` не перезаписываются.

После добавления новых ручек скопируйте блоки из `.gen/config.openapi.yaml` в `configs/static_config.yaml`.
