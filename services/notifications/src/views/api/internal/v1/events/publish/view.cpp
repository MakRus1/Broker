#include <helpers/notifications.hpp>
#include <userver/formats/json.hpp>
#include <views/api/internal/v1/events/publish/view.hpp>

namespace notifications::views::api::internal::v1::events::publish {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::notifications::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto& envelope = request.body;
    // Тип разрешён только доверенному producer, указанному в auth.service-clients.
    if (envelope.type != "shipment.route_changed" || envelope.version != 1)
        throw ApiError<400>{"unsupported_event", "Неизвестный тип или версия события"};
    models::ShipmentRouteChanged event;
    try {
        event = models::ParseModelJson(
            envelope.payload, std::type_identity<models::ShipmentRouteChanged>{});
    } catch (const std::exception&) {
        throw ApiError<400>{"invalid_event_payload", "Некорректные данные события"};
    }
    if (event.organization_id != envelope.organization_id)
        throw ApiError<400>{
            "event_organization_mismatch", "Организация события не совпадает с конвертом"};
    if (event.recipient_issuer != identity.Issuer)
        throw ApiError<400>{"invalid_recipient_issuer",
            "Получатель должен принадлежать провайдеру пользователей отправителя"};
    const auto payload = userver::formats::json::ValueBuilder(envelope).ExtractValue();
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inserted = transaction.Execute(sql::kAcceptEvent, identity.ClientId,
        envelope.organization_id, envelope.id, envelope.type, envelope.version, payload);
    if (inserted.IsEmpty()) {
        if (!transaction
                 .Execute(sql::kGetEvent, identity.ClientId, envelope.organization_id, envelope.id,
                     envelope.type, envelope.version, payload)
                 .AsSingleRow<bool>())
            throw ApiError<409>{
                "event_conflict", "Событие с этим ключом уже принято с другими данными"};
    } else if (event.recipient_subject.has_value() &&
        event.recipient_subject.value() != event.actor_subject) {
        transaction.Execute(sql::kCreateNotification, event.organization_id, event.recipient_issuer,
            event.recipient_subject.value(), event.shipment_id, event.shipment_number,
            event.revision);
    }
    transaction.Commit();
    return Response200{.accepted = true};
}
}  // namespace notifications::views::api::internal::v1::events::publish
