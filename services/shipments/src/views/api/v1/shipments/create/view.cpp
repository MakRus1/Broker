#include <components/shipments.hpp>
#include <helpers/json.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/text.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <utility>
#include <views/api/v1/shipments/create/view.hpp>

namespace shipments::views::api::v1::shipments::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) try {
    const auto& actor = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
    const auto organization = platform::auth::GetOrganization(actor);
    auto& body = request.body;
    const auto source = ShipmentSource::Manual(body.idempotency_key);
    const auto document = helpers::Document(body);
    if (auto replay =
            deps.shipments->FindManualShipmentReplay(organization, body, source, document);
        replay.has_value())
        return Response201{.shipment = std::move(replay.value())};
    const auto logist = body.logist.has_value()
        ? platform::TrimToOptional(body.logist.value())
        : deps.shipment_card_integrations->CustomerLogist(context, body.customer_id);
    deps.shipment_card_integrations->ValidateLogist(context, logist);
    if (body.route_segments.has_value())
        deps.shipment_card_integrations->ResolveRoute(context, body.route_segments.value(), {});
    body.logist = logist;
    return Response201{.shipment = deps.shipments->CreateShipmentFromSource(
                           organization, actor, body, source, document)};
} catch (const userver::storages::postgres::UniqueViolation&) {

    throw ApiError<409>{"number_exists", "Номер поставки уже занят"};
}
}  // namespace shipments::views::api::v1::shipments::create
