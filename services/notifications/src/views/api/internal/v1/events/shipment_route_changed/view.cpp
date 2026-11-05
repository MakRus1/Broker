#include <helpers/notifications.hpp>
#include <userver/formats/json.hpp>
#include <views/api/internal/v1/events/shipment_route_changed/view.hpp>

namespace notifications::views::api::internal::v1::events::shipment_route_changed {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::notifications::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto& event = request.body;
    if (event.recipient_issuer != identity.Issuer)
        throw ApiError<400>{"invalid_recipient_issuer",
            "Получатель должен принадлежать провайдеру пользователей отправителя"};
    const auto payload = userver::formats::json::ValueBuilder(event).ExtractValue();
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inserted = transaction.Execute(sql::kAcceptEvent, identity.ClientId,
        event.organization_id, event.shipment_id, event.revision, payload);
    if (inserted.IsEmpty()) {
        if (!transaction
                 .Execute(sql::kGetEvent, identity.ClientId, event.organization_id,
                     event.shipment_id, event.revision, payload)
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
}  // namespace notifications::views::api::internal::v1::events::shipment_route_changed
