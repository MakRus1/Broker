# Браузерный API BFF

Все маршруты находятся на одном origin с React-приложением. JSON API описан в
`api/**/*.yaml`, общие модели — в `../components/schemas`. Кодген создает обработчики,
DTO и проверку запросов; ручные реализации находятся в `src/views/api`.
Спецификации целиком экспортируются в `schemas/services/admin_bff`.

Защищенные операции объявляют `BrowserSession` (`apiKey`, `in: cookie`).
Общая middleware `browser-session` проверяет сессию и CSRF до разбора запроса,
кладет проверенную личность в request context и преобразует ошибки upstream.
Без нее сгенерированный защищенный обработчик отклоняет запрос.
C++-клиенты поддерживают bearer JWT, но явно отклоняют браузерную cookie-схему.
Redirect, cookie входа и статические файлы обслуживаются отдельными ручными обработчиками.

| Метод и путь | Назначение |
| --- | --- |
| GET `/auth/login` | Создать одноразовую транзакцию входа и перейти в Keycloak |
| GET `/auth/callback` | Проверить cookie/state/PKCE/nonce/JWT и создать новую сессию |
| GET `/api/session` | Проверенная личность, роли и CSRF-токен; без bearer/refresh token |
| POST `/api/logout` | Уничтожить серверную сессию, отозвать refresh token, вернуть URL выхода Keycloak |
| GET `/api/configs/list` | Каталог конфигов |
| GET `/api/configs/history` | История опубликованных ревизий |
| POST `/api/configs/current` | Значение и версия |
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
Каждая view вызывает конкретный метод типизированного клиента Configs; произвольного прокси нет.
POST принимает `Content-Type: application/json`, `Origin` из конфигурации и
`X-CSRF-Token` из `/api/session`. Cookie HttpOnly, SameSite=Lax, Path=/, без Domain.
Временная cookie входа живет пять минут и удаляется после callback.

Ошибки Configs 400/401/403/404/409/503 сохраняют статус и типизированное JSON-тело.
Ошибки транспорта дают 503, нарушение контракта upstream — 502.
URL callback логируется только как путь: code и state в технические логи не попадают.
Тела сгенерированных HTTP-ручек по умолчанию логируются до 32 КиБ в поле `body`.
В `http_logging.exclude_paths` исключены сессии и выход; их тела
не логируются; настройки заголовков Authorization/Cookie не меняются.

В Network браузера скопируйте `X-Trace-Id` из заголовков ответа для поиска трассы в Tempo и связанных логов в Loki. Заголовок содержит только ID, без разбора `traceparent`.
