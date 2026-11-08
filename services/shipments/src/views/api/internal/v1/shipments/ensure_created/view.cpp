#include <helpers/create_shipment.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/route_notifications.hpp>
#include <helpers/route_projection.hpp>
#include <tuple>
#include <views/api/internal/v1/shipments/ensure_created/view.hpp>

namespace shipments::views::api::internal::v1::shipments::ensure_created {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    const auto& command = request.body;
    const auto& sender = platform::auth::GetIdentity(context);
    if (sender.Issuer != command.actor_issuer)
        throw ApiError<400>{"invalid_actor", "Некорректный провайдер пользователя"};
    auto actor = sender;
    actor.Subject = command.actor_subject;
    actor.Issuer = command.actor_issuer;
    const auto result = CreateShipment(deps, command.organization_id, actor, command.shipment,
        {.Producer = sender.ClientId, .Type = command.source.type, .Id = command.source.id},
        Document(command));
    return Response200{.shipment = result};
}
}  // namespace shipments::views::api::internal::v1::shipments::ensure_created
