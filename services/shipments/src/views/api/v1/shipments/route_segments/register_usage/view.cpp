#include <algorithm>
#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/shipments/route_segments/register_usage/view.hpp>

namespace shipments::views::api::v1::shipments::route_segments::register_usage {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto rows =
            transaction.Execute(sql::kLockShipment, organization, request.body.shipment_id);
        if (rows.IsEmpty())
            throw ApiError{404, "not_found", "Поставка не найдена"};
        const auto shipment = rows.AsSingleRow<pgtypes::ShipmentV4>();
        if (shipment.Revision != request.body.expected_revision)
            throw ApiError{409, "shipment_revision_conflict",
                "Поставка изменилась. Обновите перевозку и повторите сохранение"};
        for (const auto& id : request.body.segment_ids) {
            const auto found =
                std::find_if(shipment.RouteSegments.begin(), shipment.RouteSegments.end(),
                    [&](const auto& segment) { return boost::uuids::to_string(segment.Id) == id; });
            if (found == shipment.RouteSegments.end())
                throw ApiError{
                    409, "route_changed", "Маршрут поставки изменён. Обновите перевозку"};
        }
        transaction.Execute(sql::kRegisterRouteUsage, organization, request.body.shipment_id,
            request.body.segment_ids);
        transaction.Commit();
        return Response200{.registered = true};
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
}  // namespace shipments::views::api::v1::shipments::route_segments::register_usage
