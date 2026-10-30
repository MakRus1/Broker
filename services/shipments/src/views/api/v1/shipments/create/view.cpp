#include <helpers/converters.hpp>
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
        const auto settings = GetShipmentSettings(deps, organization, request.body.customer_id);
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
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto replay = FindReplay<models::Shipment>(
            transaction, organization, "create_shipment", request.body);
        if (replay.has_value()) {
            transaction.Commit();
            return Response201{.shipment = replay.value()};
        }
        pgtypes::ShipmentDataV3 card;
        card.CustomerId = boost::uuids::string_generator{}(request.body.customer_id);
        card.Reference = request.body.reference.value_or("");
        card.ShipmentDate = Today();
        card.Status = settings.Statuses.front();
        card.Logist = logist;
        ApplyCard(request.body, card);
        const auto dictionaries =
            transaction.Execute(sql::kListShipmentDictionaries)
                .AsContainer<std::vector<pgtypes::ShipmentDictionaryValueV1>>();
        ValidateCard(card, settings, dictionaries);
        card.Number = request.body.number.has_value()
            ? userver::utils::text::Trim(request.body.number.value())
            : GenerateShipmentNumber(transaction, organization, card.ShipmentDate, settings);
        if (card.Number.empty())
            throw ApiError<400>{"invalid_number", "Укажите номер поставки"};
        const auto row = transaction.Execute(sql::kCreateShipment, organization, card)
                             .AsSingleRow<pgtypes::ShipmentV4>();
        RememberCargo(transaction, organization, row.CustomerId, row.CargoDescription);
        const auto result = Convert(row);
        SaveReplay(transaction, organization, "create_shipment", request.body, result);
        WriteAudit(transaction, organization, identity, "create_shipment", result.id, request.body,
            result);
        QueueRouteProjection(transaction, organization, result);
        transaction.Commit();
        return Response201{.shipment = result};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "number_exists", .message = "Номер поставки уже занят"};
    }
}
}  // namespace shipments::views::api::v1::shipments::create
