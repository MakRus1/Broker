# Доставка уведомлений

`platform::notifications::Dispatcher` регистрирует обработчик `notifications.publish`
в общем outbox. Получение сервисного токена и вызов Notifications находятся здесь;
событие и его получателей формирует сервис-отправитель. Событие сохраняется в outbox
в транзакции бизнесового изменения.

В `service.yaml` подключите библиотеку `notifications` и HTTP-клиент Notifications.
В списке компонентов сервиса зарегистрируйте клиент, `platform::outbox::Component`,
`platform::auth::ServiceTokenProvider` и диспетчер. Например, для Shipments:

```cpp
using NotificationDispatcher = platform::notifications::Dispatcher<
    shipments::clients::notifications::Component,
    shipments::clients::notifications::ApiInternalV1EventsPublish::Request>;

component_list.Append<NotificationDispatcher>();
```

В static config компонент называется `notification-dispatcher`. Типы клиента и
запроса передаются явно: библиотека не включает сгенерированные заголовки сервиса.

Outbox запускает отдельный периодик для каждого типа задач, управляет параллелизмом,
повторами и подтверждением доставки. Диспетчер не создаёт собственную очередь или
дополнительный цикл. При остановке outbox завершает обработку до разрушения зависимостей.
