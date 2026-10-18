# Браузерный API BFF

Все маршруты находятся на одном origin с React-приложением. BFF использует cookie,
поэтому его браузерные маршруты не помечены как HTTP bearer операции кодгена.
Redirect, cookie и статические файлы обслуживаются ручными HTTP-обработчиками;
контракт Configs читается из `schemas/services/configs`, DTO и HTTP-клиент генерируются.

| Метод и путь | Назначение |
| --- | --- |
| GET `/auth/login` | Создать одноразовую транзакцию входа и перейти в Keycloak |
| GET `/auth/callback` | Проверить cookie/state/PKCE/nonce/JWT и создать новую сессию |
| GET `/api/session` | Проверенная личность, роли и CSRF-токен; без bearer/refresh token |
| POST `/api/logout` | Уничтожить серверную сессию, отозвать refresh token, вернуть URL выхода Keycloak |
| GET `/api/configs/list` | Каталог конфигов |
| GET `/api/configs/history` | История рабочих ревизий |
| POST `/api/configs/current` | Рабочее значение и версия |
| POST `/api/configs/drafts/create` | Создать валидный драфт |
| POST `/api/configs/drafts/edit` | Изменить собственный драфт |
| POST `/api/configs/drafts/approve` | Подтвердить чужой драфт и опубликовать |
| POST `/api/configs/drafts/cancel` | Отменить собственный драфт |
| POST `/api/configs/drafts/get` | Прочитать драфт |
| POST `/api/configs/drafts/list` | До 100 драфтов одного конфига |
| POST `/api/configs/drafts/events` | События драфта |
| GET `/` и `/assets/...` | Production-сборка React |
| GET `/version` | Версия BFF, OpenAPI `version.yaml` |

Тела и ответы `/api/configs/...` совпадают с `/admin/configs/...` Configs.
Пути перечислены явно в `src/transport/configs.cpp`; произвольного прокси нет.
POST принимает `Content-Type: application/json`, `Origin` из конфигурации и
`X-CSRF-Token` из `/api/session`. Cookie HttpOnly, SameSite=Lax, Path=/, без Domain.
Временная cookie входа живет пять минут и удаляется после callback.

Ошибки Configs 400/401/403/404/409/503 сохраняют статус и типизированное JSON-тело.
Ошибки транспорта дают 503, нарушение контракта upstream — 502.
URL callback логируется только как путь: code и state в технические логи не попадают.
Тела запросов, Authorization и Cookie не логируются.
