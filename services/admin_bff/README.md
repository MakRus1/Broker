# Административный BFF

userver-сервис для React-админки Configs. Принимает сессионную cookie браузера,
вызывает Configs через типизированный клиент с пользовательским access token.
Доступ к бизнес-операциям проверяет Configs, данные конфигов в BFF не сохраняются.

- Публичный listener: 8180; локальный стенд публикует его как http://localhost:13080.
- Внутренний listener: 8182; `/ping`, `/ping/ready`, метрики.
- Сессии хранятся в памяти процесса, максимум 2000, время жизни — 8 часов.
  Перезапуск завершает локальные сессии. Пока запускается одна реплика.
- Вход: Authorization Code + S256 PKCE, state и nonce; проверяются оба JWT.
- В браузере нет access/refresh token. При HTTPS cookie имеют Secure и префикс `__Host-`.
- POST требует точный Origin и `X-CSRF-Token`; токен CSRF выдается через `/api/session`.

`src/components/sessions.*` хранит сессии и обменивает токены.
`src/components/browser_session.*` проверяет сессию и CSRF, создает снимок личности
и преобразует ошибки клиента. Цепочка BFF добавляет эту middleware последней,
в том числе после служебной middleware testsuite: ошибки upstream обрабатываются одинаково
в тестах и обычном режиме. Mutex сессии освобождается до вызова Configs и выхода.

`docs/api/api` описывает JSON API; `src/views/api` содержит отдельную реализацию
каждой операции. `src/models/config_conversions.hpp` явно преобразует DTO Configs
в модели браузерного контракта. `src/transport/login.cpp` обслуживает вход и callback;
`assets.cpp` — только `/`, `/index.html` и `/assets/*`.

Контракт маршрутов: [docs/api/README.md](docs/api/README.md).
Полный запуск, интерфейс и ограничения: [docs/admin-ui.md](../../docs/admin-ui.md).
