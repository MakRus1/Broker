#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/employees.hpp>
#include <helpers/revision.hpp>
#include <helpers/route_counterparties.hpp>
#include <helpers/route_locations.hpp>
#include <helpers/route_usage.hpp>
#include <helpers/shipment_card.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <views/api/v1/shipments/update/view.hpp>

namespace shipments::views::api::v1::shipments::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
        if (request.body.logist.has_value())
            ValidateLogist(deps, context, CleanText(request.body.logist.value()));
        const auto organization = platform::auth::GetOrganization(identity);
        if (request.body.route_segments.has_value()) {
            const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
                sql::kGetShipment, organization, request.body.shipment_id);
            if (rows.IsEmpty())
                throw ApiError{404, "not_found", "Поставка не найдена"};
            const auto previous = rows.AsSingleRow<pgtypes::ShipmentV3>();
            CheckRevision(previous.Revision, request.body.expected_revision);
            const auto locations =
                LoadRouteLocations(*deps.pg_cluster, request.body.route_segments.value());
            domain::ResolveRouteDetails(
                request.body.route_segments.value(), Convert(previous).route_segments, locations);
            ValidateRouteCounterparties(deps, context, request.body.route_segments.value(),
                Convert(previous).route_segments);
            CheckExistingRouteUsage(deps, context, request.body.shipment_id,
                ChangedRouteSegments(previous, request.body.route_segments.value()));
        }
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto current =
            transaction.Execute(sql::kLockShipment, organization, request.body.shipment_id);
        if (current.IsEmpty())
            throw ApiError{404, "not_found", "Поставка не найдена"};
        const auto previous = current.AsSingleRow<pgtypes::ShipmentV3>();
        CheckRevision(previous.Revision, request.body.expected_revision);
        auto card = ConvertData(previous);
        if (request.body.reference.has_value())
            card.Reference = request.body.reference.value();
        if (request.body.customer_id.has_value())
            card.CustomerId = boost::uuids::string_generator{}(request.body.customer_id.value());
        if (request.body.number.has_value()) {
            card.Number = userver::utils::text::Trim(request.body.number.value());
            if (card.Number.empty())
                throw ApiError{400, "invalid_number", "Укажите номер поставки"};
        }
        ApplyCard(request.body, card);
        const auto dictionaries =
            transaction.Execute(sql::kListShipmentDictionaries)
                .AsContainer<std::vector<pgtypes::ShipmentDictionaryValueV1>>();
        const auto protected_ids =
            transaction.Execute(sql::kGetRouteUsage, organization, request.body.shipment_id)
                .AsContainer<std::vector<std::string>>();
        for (const auto& id : protected_ids) {
            const auto before =
                std::find_if(previous.RouteSegments.begin(), previous.RouteSegments.end(),
                    [&](const auto& segment) { return boost::uuids::to_string(segment.Id) == id; });
            const auto after = std::find_if(card.RouteSegments.begin(), card.RouteSegments.end(),
                [&](const auto& segment) { return boost::uuids::to_string(segment.Id) == id; });
            if (before != previous.RouteSegments.end() &&
                (after == card.RouteSegments.end() || after->Type != before->Type))
                throw ApiError{409, "route_segment_in_use",
                    "Нельзя удалить или изменить тип участка с данными перевозок"};
        }
        ValidateCard(card,
            GetShipmentSettings(deps, organization, boost::uuids::to_string(card.CustomerId)),
            dictionaries, previous);
        const auto result =
            Convert(transaction
                        .Execute(sql::kUpdateShipment, organization, request.body.shipment_id,
                            request.body.expected_revision, card)
                        .AsSingleRow<pgtypes::ShipmentV3>());
        if (card.CargoDescription != previous.CargoDescription ||
            card.CustomerId != previous.CustomerId)
            RememberCargo(transaction, organization, card.CustomerId, card.CargoDescription);
        WriteAudit(transaction, organization, identity, "update_shipment", result.id, request.body,
            result);
        transaction.Commit();
        return Response200{.shipment = result};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "number_exists", .message = "Номер поставки уже занят"};
    } catch (const ::shipments::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 503:
            return Response503{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace shipments::views::api::v1::shipments::update
