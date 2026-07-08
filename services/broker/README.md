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

Результат:

| Путь | Содержимое |
|------|------------|
| `src/views/<path>/` | `view.hpp` / `view.cpp` — бизнес-логика (не перезаписываются) |
| `.gen/` | DTO, handler-классы, `config.openapi.yaml` |

Для `/foo/bar` → `src/views/foo/bar/`.

После добавления новых ручек перенесите блоки из `.gen/config.openapi.yaml` в `configs/static_config.yaml` и пересоберите проект (`make cmake-debug` при появлении новых файлов).
