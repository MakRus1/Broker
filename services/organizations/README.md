# Organizations

Сервис организаций, членства и ролей сотрудников. Создаёт учётные записи через
Keycloak Admin API; пароли и вход остаются в Keycloak.

Контракт: `docs/api/organizations.yaml`. SQL: `postgresql/queries`.
Миграции: `postgresql/schemas/db_1/V0001__*.sql` и последующие версии.

Сценарии админки, права, локальная настройка и первоначальный администратор staging:
[Организации и сотрудники](../../docs/organizations.md).

Генерация: `make gen` из каталога сервиса. Запуск локального окружения:
`make local-up` из корня репозитория.
