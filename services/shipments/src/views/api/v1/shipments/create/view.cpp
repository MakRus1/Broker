#include <helpers/converters.hpp>
#include <helpers/create_shipment.hpp>
#include <helpers/database.hpp>
#include <helpers/employees.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/route_counterparties.hpp>
#include <helpers/route_locations.hpp>
#include <helpers/route_projection.hpp>
#include <helpers/shipment_card.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <views/api/v1/shipments/create/view.hpp>

namespace shipments::views::api::v1::shipments::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto saved =
            FindSavedReplay<models::Shipment>(deps, organization, "create_shipment", request.body);
        if (saved.has_value())
            return Response201{.shipment = saved.value()};
        const ShipmentSource source{
            .Producer = "web", .Type = "manual", .Id = request.body.idempotency_key};
        if (const auto replay =
                FindCreatedShipment(deps, organization, source, Document(request.body)))
            return Response201{.shipment = *replay};
        const auto logist = request.body.logist.has_value()
            ? CleanText(request.body.logist.value())
            : CustomerLogist(deps, context, request.body.customer_id);
        ValidateLogist(deps, context, logist);
        if (request.body.route_segments.has_value()) {
            const auto locations =
                LoadRouteLocations(deps, context, request.body.route_segments.value());
            domain::ResolveRouteDetails(request.body.route_segments.value(), {}, locations);
            ValidateRouteCounterparties(deps, context, request.body.route_segments.value(), {});
        }
        auto body = request.body;
        body.logist = logist;
        const auto result = CreateShipment(deps, organization, identity, body,
            {.Producer = "web", .Type = "manual", .Id = request.body.idempotency_key},
            Document(request.body));
        return Response201{.shipment = result};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "number_exists", .message = "Номер поставки уже занят"};
    }
}
}  // namespace shipments::views::api::v1::shipments::create
