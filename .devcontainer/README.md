# Dev Container

Откройте репозиторий в Cursor/VS Code и выберите **Reopen in Container**.

Образ `ubuntu-22.04-userver-pg-dev:v2.15` содержит userver, PostgreSQL-клиент и все зависимости для сборки с gRPC.

После первого запуска контейнера:

```bash
make build-debug
make test-debug
```
