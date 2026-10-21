#include <helpers/rows.hpp>
#include <map>
#include <platform/auth/principal.hpp>
#include <views/api/v1/transportations/list/view.hpp>

namespace transportations::views::api::v1::transportations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        Session session{deps.pg_cluster, principal, std::nullopt, kReadSnapshot};
        const auto rows = session.Tx.Execute(sql::kListTransportations, session.Tenant,
            request.body.after, request.body.shipment_id, request.body.item_id, request.body.limit);
        std::vector<models::Transportation> result;
        std::map<std::string, std::size_t> indexes;
        std::vector<std::string> ids;
        for (const auto& row : rows.AsSetOf<pgtypes::TransportationV1>()) {
            indexes.emplace(boost::uuids::to_string(row.Id), result.size());
            ids.push_back(boost::uuids::to_string(row.Id));
            result.push_back(ToModel(row));
        }
        // Все плечи страницы читаются одним запросом, без N+1.
        if (!ids.empty()) {
            const auto legs = session.Tx.Execute(sql::kListLegs, session.Tenant, ids);
            for (const auto& leg : legs.AsSetOf<pgtypes::TransportationLegV1>())
                result.at(indexes.at(boost::uuids::to_string(leg.TransportationId)))
                    .legs.push_back(models::ParseModelJson(
                        leg.Document, std::type_identity<models::TransportationLeg>{}));
        }
        session.Tx.Commit();
        return Response200{.items = result};
    } catch (const ::transportations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace transportations::views::api::v1::transportations::list
