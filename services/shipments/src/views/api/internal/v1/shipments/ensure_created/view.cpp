#include <components/shipments.hpp>
#include <helpers/json.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/internal/v1/shipments/ensure_created/view.hpp>

namespace shipments::views::api::internal::v1::shipments::ensure_created {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& command = request.body;
    const auto& sender = platform::auth::GetIdentity(context);
    if (sender.Issuer != command.actor_issuer)
        throw ApiError<400>{"invalid_actor", "Некорректный провайдер пользователя"};
    auto actor = sender;
    actor.Subject = command.actor_subject;
    actor.Issuer = command.actor_issuer;
    const ShipmentSource source{
        .Producer = sender.ClientId, .Type = command.source.type, .Id = command.source.id};
    const auto document = helpers::Document(command);
    if (auto replay =
            deps.shipments->FindCreatedShipment(command.organization_id, source, document);
        replay.has_value())
        return Response200{.shipment = std::move(replay.value())};
    return Response200{.shipment = deps.shipments->CreateShipmentFromSource(
                           command.organization_id, actor, command.shipment, source, document)};
}
}  // namespace shipments::views::api::internal::v1::shipments::ensure_created
