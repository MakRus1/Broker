# broker

Сервис HTTP + PostgreSQL + gRPC на [userver](https://github.com/userver-framework/userver).

Сборка, тесты и запуск — из **корня монорепозитория**. См. [README.md](../../README.md).

## OpenAPI кодогенерация

Спеки лежат в `docs/api/`. После изменений:

```bash
make gen          # из папки сервиса
# или из корня:
make gen SERVICE=broker
```

`make gen` также обновляет блок OpenAPI-ручек в `configs/static_config.yaml` (между маркерами `# OPENAPI_HANDLERS_BEGIN` / `# OPENAPI_HANDLERS_END`).

Результат:

| Путь | Содержимое |
|------|------------|
| `src/views/<path>/` | `view.hpp` / `view.cpp` — бизнес-логика (не перезаписываются) |
| `.gen/` | DTO, handler-классы, `config.openapi.yaml` |

Для `/foo/bar` → `src/views/foo/bar/`.

Пересоберите проект (`make cmake-debug`) при появлении новых файлов в `.gen/`.
